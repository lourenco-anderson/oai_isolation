# Boas Práticas: Construção de Imagens Docker e Deployment em Kubernetes

## Visão Geral
Este documento descreve as melhores práticas aplicadas ao construir múltiplas imagens Docker e fazer deploy em um cluster Kubernetes local (minikube).

---

## 1. Construção de Imagens Docker

### 1.1 Evitar Problemas com BuildKit
**Problema:** O driver `docker-container` do buildx pode gerar erros "404 page not found" e "No output specified" em ambientes locais.

**Solução:**
```bash
# Use o driver docker padrão em vez de docker-container
docker buildx use default

# Ou remova builders problemáticos e crie um novo
docker buildx rm default 2>/dev/null || true
docker buildx create --name simple --driver docker
docker buildx use simple
```

**Boas Práticas:**
- Verifique qual builder está ativo: `docker buildx ls`
- Para desenvolvimento local, prefira o driver `docker` padrão
- Reserve drivers `docker-container` para CI/CD com suporte completo

### 1.2 Build em Lote com Loop
**Implementação eficiente:**
```bash
for dockerfile in containers/ue/*/Dockerfile; do
  component=$(basename "$(dirname "$dockerfile")")
  side=$(basename "$(dirname "$(dirname "$dockerfile")")")
  echo "[BUILD] oai-${side}-${component}..."
  docker build --progress=plain -t oai-${side}-${component}:latest -f "$dockerfile" . \
    > /tmp/build.log 2>&1 && echo "✓ OK" || echo "✗ FAILED"
done
```

**Benefícios:**
- Padroniza automaticamente nomes de imagens
- Proporciona feedback visual do progresso
- Facilita identificação de builds falhados

### 1.3 Nomenclatura de Imagens
**Padrão recomendado:**
```
<organization>-<component>-<subcomponent>:<version>
```

Exemplo:
- `oai-ue-ch_est:latest`
- `oai-ue-ldpc_dec:v1.0`

**Vantagens:**
- Rastreabilidade clara
- Fácil identificação do propósito
- Compatível com labeling Kubernetes

---

## 2. Dockerfile Otimizações

### 2.1 Minimizar Tamanho de Imagem
```dockerfile
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC

# Instalar apenas dependências necessárias com --no-install-recommends
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    libforms2 \
    libx11-6 \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copiar apenas arquivos necessários
COPY build/oai_isolation /app/oai_isolation
COPY ext/openair/cmake_targets/ran_build/build/*.so /usr/lib/
```

**Melhores Práticas:**
- Use `--no-install-recommends` para reduzir dependências
- Limpe cache com `apt-get clean` e `rm -rf /var/lib/apt/lists/*`
- Combine múltiplos RUN em um único comando com `&&`
- Minimize camadas usando `COPY` ao final

### 2.2 Tratamento de Avisos
**Aviso comum:**
```
UndefinedVar: Usage of undefined variable '$LD_LIBRARY_PATH'
```

**Solução:**
```dockerfile
# Defina explicitamente variáveis necessárias
ENV LD_LIBRARY_PATH=/usr/lib:$LD_LIBRARY_PATH

# Ou use no Dockerfile
RUN echo 'export LD_LIBRARY_PATH=/usr/lib:$LD_LIBRARY_PATH' >> /etc/profile.d/ld_library_path.sh
```

---

## 3. Integração com Kubernetes (Minikube)

### 3.1 Usar Imagens Locais em Minikube
**Problema:** Kubernetes tenta fazer pull de imagens de registros remotos.

**Solução 1: imagePullPolicy: Never (Recomendado)**
```yaml
spec:
  containers:
  - name: ue-ch-est
    image: oai-ue-ch_est:latest
    imagePullPolicy: Never  # Usa imagem local apenas
```

**Solução 2: Usar registry local**
```bash
# Configurar Docker para usar registry local
eval $(minikube docker-env)

# Rebuild imagens (opcional se já feito localmente)
docker build -t oai-ue-ch_est:latest .
```

**Comparação:**

| Abordagem | Uso | Vantagem |
|-----------|-----|---------|
| `Never` | Desenvolvimento local | Rápido, sem necessidade de push |
| `IfNotPresent` | Dev/Prod | Tenta local, depois remoto |
| Registry local | CI/CD | Persistente, compartilhável |

### 3.2 Manifest Kubernetes Correto

**❌ Errado:**
```yaml
image: localhost/oai-ue-ch_est:latest
imagePullPolicy: IfNotPresent
```

**✅ Correto:**
```yaml
image: oai-ue-ch_est:latest
imagePullPolicy: Never
```

### 3.3 Lidar com ReplicaSets Obsoletos
**Problema:** Ao fazer update de Deployments, ReplicaSets antigos permanecem.

**Verificar ReplicaSets:**
```bash
kubectl get rs -l component=ue -o wide
```

**Limpar ReplicaSets com imagens antigas:**
```bash
# Deletar ReplicaSets específicos
kubectl delete rs ue-ch-est-7ccdb84b7f ue-ch-mmse-5566d9d467

# Forçar recriação de pods
kubectl delete pods -l component=ue --grace-period=0 --force
```

**Melhor prática:**
```bash
# Usar labels para identificar versões
kubectl delete rs -l version=old,component=ue
```

---

## 4. Fluxo Completo de Deployment

### 4.1 Checklist de Deployment
```bash
# 1. Construir imagens
for dockerfile in containers/ue/*/Dockerfile; do
  component=$(basename "$(dirname "$dockerfile")")
  side=$(basename "$(dirname "$(dirname "$dockerfile")")")
  docker build -t oai-${side}-${component}:latest -f "$dockerfile" .
done

# 2. Verificar imagens
docker images | grep oai-ue

# 3. Deletar deployment antigo (se existir)
kubectl delete deployment -l component=ue || true

# 4. Aplicar novo manifest
kubectl apply -f ./k8s-manifests/ue-deployments.yaml

# 5. Aguardar pods iniciarem
kubectl wait --for=condition=ready pod -l component=ue --timeout=120s

# 6. Verificar status
kubectl get pods -l component=ue
kubectl describe pod <pod-name>
```

### 4.2 Troubleshooting
```bash
# Ver logs de erro
kubectl describe pod <pod-name> | grep -A 10 "Events:"

# Ver logs da aplicação
kubectl logs <pod-name>

# Entrar no pod
kubectl exec -it <pod-name> -- /bin/bash

# Debugar configuração
kubectl get deployment <name> -o yaml
```

---

## 5. Otimizações de Performance

### 5.1 Limites de Recursos
```yaml
spec:
  containers:
  - name: ue-ch-est
    resources:
      requests:
        cpu: 1000m
        memory: 512Mi
      limits:
        cpu: 1000m
        memory: 1Gi
```

**Recomendações:**
- `requests`: Mínimo garantido ao pod
- `limits`: Máximo que o pod pode usar
- Ajuste conforme necessário para sua aplicação

### 5.2 Health Checks
```yaml
spec:
  containers:
  - name: ue-ch-est
    livenessProbe:
      httpGet:
        path: /health
        port: 8080
      initialDelaySeconds: 30
      periodSeconds: 10
    readinessProbe:
      httpGet:
        path: /ready
        port: 8080
      initialDelaySeconds: 5
      periodSeconds: 5
```

### 5.3 Escalabilidade
```yaml
spec:
  replicas: 3  # Múltiplas cópias da aplicação
  selector:
    matchLabels:
      app: ue-ch-est
```

---

## 6. Versionamento

### 6.1 Tagging de Imagens
```bash
# Build com tag de versão
docker build -t oai-ue-ch_est:v1.0 .
docker build -t oai-ue-ch_est:latest .

# Push para registry (opcional)
docker tag oai-ue-ch_est:v1.0 myregistry/oai-ue-ch_est:v1.0
docker push myregistry/oai-ue-ch_est:v1.0
```

### 6.2 Rastreamento no Git
```bash
# Registrar versão da build
echo "v1.0" > VERSION
git add VERSION
git commit -m "Build: oai-ue containers v1.0"
git tag -a v1.0 -m "Release oai-ue v1.0"
```

---

## 7. Automação (Script Helper)

Criar script `build-and-deploy.sh`:
```bash
#!/bin/bash
set -e

NAMESPACE=${1:-default}
VERSION=${2:-latest}

echo "🔨 Construindo imagens..."
for dockerfile in containers/ue/*/Dockerfile; do
  component=$(basename "$(dirname "$dockerfile")")
  side=$(basename "$(dirname "$(dirname "$dockerfile")")")
  tag="oai-${side}-${component}:${VERSION}"
  echo "  Building $tag..."
  docker build -t "$tag" -f "$dockerfile" . > /dev/null
done

echo "🚀 Deploying em Kubernetes..."
kubectl apply -f ./k8s-manifests/ue-deployments.yaml -n "$NAMESPACE"

echo "⏳ Aguardando pods..."
kubectl wait --for=condition=ready pod -l component=ue --timeout=120s -n "$NAMESPACE"

echo "✅ Deployment concluído!"
kubectl get pods -l component=ue -n "$NAMESPACE"
```

---

## 8. Problemas Comuns e Soluções

| Problema | Causa | Solução |
|----------|-------|--------|
| `ErrImagePull` | Imagem não encontrada | Verificar `imagePullPolicy`, garantir imagem construída |
| `ImagePullBackOff` | Tentando pull remoto | Usar `Never` para imagens locais |
| `Pending` | Recursos insuficientes | Aumentar memoria/cpu no minikube ou reduzir requests |
| `CrashLoopBackOff` | Aplicação falha ao iniciar | Verificar logs com `kubectl logs` |
| `404 buildkit` | Driver buildx problemático | Trocar para driver docker padrão |

---

## 9. Referências

- [Docker Best Practices](https://docs.docker.com/develop/dev-best-practices/)
- [Kubernetes Security Best Practices](https://kubernetes.io/docs/concepts/security/)
- [Minikube Documentation](https://minikube.sigs.k8s.io/)
- [Docker Buildx](https://docs.docker.com/build/buildx/)

---

## Resumo das Melhores Práticas Aplicadas

✅ Usar driver Docker padrão em desenvolvimento local  
✅ Nomenclatura consistente e descritiva para imagens  
✅ Minimizar tamanho de Dockerfile com `--no-install-recommends`  
✅ Usar `imagePullPolicy: Never` para imagens locais em Kubernetes  
✅ Remover prefixos de registry quando usar imagens locais  
✅ Limpar ReplicaSets obsoletos após updates  
✅ Usar labels apropriados para seleção de recursos  
✅ Definir limits e requests de recursos  
✅ Implementar verificações de saúde (health checks)  
✅ Automatizar builds e deployments com scripts  

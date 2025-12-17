# Guia Passo a Passo: Deploy de Containers no Kubernetes

Este guia mostra como fazer o deploy de qualquer container OAI Isolation para o cluster Kubernetes, começando do Dockerfile.

## Pré-requisitos

- Docker instalado e rodando
- Kind cluster criado e executando (`kepler-cluster`)
- Namespace `oai-isolation` criado no cluster
- Imagem base já construída (`oai_isolation` binary disponível em `build/`)

```bash
# Verificar se o cluster está rodando
kind get clusters

# Criar namespace se não existir
kubectl create namespace oai-isolation 2>/dev/null || echo "Namespace already exists"
```

---

## Passo 1: Localizar o Dockerfile do Container

Todos os containers OAI Isolation têm a seguinte estrutura:

```
containers/
├── gnb/
│   ├── ldpc/
│   │   └── Dockerfile
│   ├── modulation/
│   │   └── Dockerfile
│   └── ... (outros componentes gNB)
└── ue/
    ├── ldpc_dec/
    │   └── Dockerfile
    └── ... (outros componentes UE)
```

**Exemplo:** Para deploy do LDPC, o Dockerfile está em:
```
containers/gnb/ldpc/Dockerfile
```

---

## Passo 2: Revisar o Dockerfile

Antes de construir, examine o Dockerfile para entender:
- Imagem base (`FROM`)
- Dependências instaladas
- Arquivos copiados
- Comando de execução (`ENTRYPOINT` ou `CMD`)

**Exemplo (containers/gnb/ldpc/Dockerfile):**
```dockerfile
FROM ubuntu:24.04

# Instalações...
RUN apt-get update && apt-get install -y ...

# Copia binário e libs do host
COPY build/oai_isolation /usr/local/bin/
COPY ext/openair/cmake_targets/ran_build/build/lib /usr/local/lib/

# Comando de execução
ENTRYPOINT ["/usr/local/bin/oai_isolation"]
CMD ["ldpc"]
```

---

## Passo 3: Build da Imagem Docker

Execute o build a partir da **raiz do repositório**:

```bash
cd /home/anderson/dev/oai_isolation

# Build genérico para qualquer componente:
docker build -f containers/<TIPO>/<COMPONENTE>/Dockerfile \
             -t oai-<TIPO>-<COMPONENTE>:test \
             .

# Exemplos específicos:

# 1. Build do gNB LDPC
docker build -f containers/gnb/ldpc/Dockerfile \
             -t oai-gnb-ldpc:test \
             .

# 2. Build do UE LDPC Decoder
docker build -f containers/ue/ldpc_dec/Dockerfile \
             -t oai-ue-ldpc-dec:test \
             .

# 3. Build do gNB Modulation
docker build -f containers/gnb/modulation/Dockerfile \
             -t oai-gnb-modulation:test \
             .
```

**Flags importantes:**
- `-f` → Caminho do Dockerfile
- `-t` → Tag da imagem (`nome:versão`)
- `.` → Contexto de build (raiz do projeto, onde estão `build/` e `ext/`)

**Verificar build:**
```bash
docker images | grep oai-
```

---

## Passo 4: Verificar o Dockerfile de Deployment no Kubernetes

Cada container tem um manifesto YAML em dois locais:

**Opção A:** Manifesto específico do container (recomendado)
```
containers/<TIPO>/<COMPONENTE>/<COMPONENTE>-deployment.yaml
```

**Opção B:** Manifesto centralizado no k8s/
```
k8s/<COMPONENTE>-deployment.yaml
```

**Usar a Opção A** para evitar conflitos de versionamento.

---

## Passo 5: Verificar o Manifesto YAML

O manifesto deve ter:

1. **Nome correto da imagem** (sem tag ou com `:test`):
```yaml
containers:
- name: <componente>
  image: oai-<tipo>-<componente>:test        # Tag deve corresponder ao build
  imagePullPolicy: IfNotPresent               # Use imagem local, não do registry
```

2. **Recursos adequados:**
```yaml
resources:
  requests:
    memory: "512Mi"
    cpu: "500m"
  limits:
    memory: "1Gi"
    cpu: "1000m"
```

3. **Namespace correto:**
```yaml
metadata:
  namespace: oai-isolation
```

**Exemplo do manifesto completo** (`containers/gnb/ldpc/gnb-ldpc-deployment.yaml`):
```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: gnb-ldpc
  namespace: oai-isolation
  labels:
    app: gnb-ldpc
    component: gnb
    function: ldpc
spec:
  replicas: 1
  selector:
    matchLabels:
      app: gnb-ldpc
  template:
    metadata:
      labels:
        app: gnb-ldpc
    spec:
      containers:
      - name: gnb-ldpc
        image: oai-gnb-ldpc:test
        imagePullPolicy: IfNotPresent
        resources:
          requests:
            memory: "512Mi"
            cpu: "500m"
          limits:
            memory: "1Gi"
            cpu: "1000m"
```

---

## Passo 6: Carregar a Imagem Docker no Kind Cluster

O Kind roda containers em uma rede isolada. Você precisa carregar a imagem local:

```bash
kind load docker-image oai-<tipo>-<componente>:test --name kepler-cluster

# Exemplo:
kind load docker-image oai-gnb-ldpc:test --name kepler-cluster

# Verificar carregamento:
kind load docker-image oai-gnb-ldpc:test --name kepler-cluster --verbose
```

**Saída esperada:**
```
Image: "oai-gnb-ldpc:test" with ID "sha256:..." 
found to be already present on all nodes.
```

---

## Passo 7: Aplicar o Manifesto Kubernetes

```bash
# Aplicar deployment específico do container
kubectl apply -f containers/<tipo>/<componente>/<componente>-deployment.yaml

# Exemplo:
kubectl apply -f containers/gnb/ldpc/gnb-ldpc-deployment.yaml

# Saída esperada:
# deployment.apps/gnb-ldpc configured
# service/gnb-ldpc unchanged
```

---

## Passo 8: Monitorar o Deployment

```bash
# 1. Verificar status dos pods em tempo real
kubectl get pods -n oai-isolation -w

# 2. Descrever pod para diagnóstico detalhado
kubectl describe pod -n oai-isolation -l app=<componente>

# 3. Verificar logs da execução
kubectl logs -n oai-isolation -l app=<componente>
```

**Exemplos:**

```bash
# Monitorar LDPC
kubectl get pods -n oai-isolation -w
kubectl logs -n oai-isolation -l app=gnb-ldpc

# Monitorar UE LDPC Decoder
kubectl logs -n oai-isolation -l app=ue-ldpc-dec
```

---

## Passo 9: Troubleshooting Comuns

### Erro: `ImagePullBackOff` ou `ErrImagePull`

**Causa:** Imagem não está no cluster ou tag está errada.

**Solução:**
```bash
# 1. Verificar tag da imagem construída
docker images | grep oai-

# 2. Carregar novamente
kind load docker-image oai-gnb-ldpc:test --name kepler-cluster

# 3. Forçar recriação do pod
kubectl delete pods -n oai-isolation -l app=gnb-ldpc
kubectl apply -f containers/gnb/ldpc/gnb-ldpc-deployment.yaml
```

### Erro: `CrashLoopBackOff`

**Causa:** Container iniciou mas encerrou com erro.

**Solução:**
```bash
# Ver logs de erro
kubectl logs -n oai-isolation -l app=gnb-ldpc

# Ver eventos
kubectl describe pod -n oai-isolation -l app=gnb-ldpc
```

### Erro: `Insufficient memory` ou `Insufficient cpu`

**Causa:** Cluster sem recursos suficientes.

**Solução:**
```bash
# Reduzir requests/limits no manifesto YAML
# ou recriar Kind cluster com mais recursos
```

---

## Passo 10: Atualizar a Imagem e Fazer Redeploy

Quando você modifica o código-fonte:

```bash
# 1. Reconstruir a imagem
docker build -f containers/gnb/ldpc/Dockerfile \
             -t oai-gnb-ldpc:test \
             .

# 2. Carregar novamente no Kind
kind load docker-image oai-gnb-ldpc:test --name kepler-cluster

# 3. Forçar rollout de novo deployment
kubectl rollout restart deployment/gnb-ldpc -n oai-isolation

# 4. Monitorar novo pod
kubectl get pods -n oai-isolation -w
```

---

## Resumo do Fluxo Completo

```bash
# 1. Build
docker build -f containers/gnb/ldpc/Dockerfile -t oai-gnb-ldpc:test .

# 2. Carregar no Kind
kind load docker-image oai-gnb-ldpc:test --name kepler-cluster

# 3. Deploy
kubectl apply -f containers/gnb/ldpc/gnb-ldpc-deployment.yaml

# 4. Monitorar
kubectl get pods -n oai-isolation -w
kubectl logs -n oai-isolation -l app=gnb-ldpc
```

---

## Deploy em Massa (Todos os Componentes)

Para fazer deploy de múltiplos componentes:

```bash
# 1. Build de todos
for container in containers/gnb/*/Dockerfile containers/ue/*/Dockerfile; do
    dir=$(dirname "$container")
    name=$(basename "$dir")
    type=$(basename $(dirname "$dir"))
    docker build -f "$container" -t "oai-${type}-${name}:test" .
done

# 2. Carregar todos
for image in $(docker images | grep oai- | awk '{print $1":"$2}'); do
    kind load docker-image "$image" --name kepler-cluster
done

# 3. Deploy todos
for manifest in containers/*/*/[!.][-_]deployment.yaml k8s/*-deployment.yaml; do
    kubectl apply -f "$manifest"
done

# 4. Monitorar
kubectl get pods -n oai-isolation -w
```

---

## Estrutura Recomendada para Novo Container

Ao adicionar um novo container, mantenha a estrutura:

```
containers/
├── gnb/ (ou ue/)
│   └── novo_componente/
│       ├── Dockerfile
│       └── novo_componente-deployment.yaml
```

**Arquivo de deployment mínimo:**
```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: gnb-novo_componente
  namespace: oai-isolation
  labels:
    app: gnb-novo_componente
spec:
  replicas: 1
  selector:
    matchLabels:
      app: gnb-novo_componente
  template:
    metadata:
      labels:
        app: gnb-novo_componente
    spec:
      containers:
      - name: gnb-novo_componente
        image: oai-gnb-novo_componente:test
        imagePullPolicy: IfNotPresent
        resources:
          requests:
            memory: "512Mi"
            cpu: "500m"
```

---

## Referências Úteis

```bash
# Listar todos os containers OAI
find containers -name "Dockerfile" -type f

# Verificar imagens no Kind
docker exec kepler-cluster-control-plane crictl images

# Verificar pods no cluster
kubectl get pods -n oai-isolation -o wide

# Deletar deployment completo
kubectl delete deployment gnb-ldpc -n oai-isolation
```

---

## Checklist para Deploy Bem-Sucedido

- [ ] Dockerfile existe em `containers/<tipo>/<componente>/`
- [ ] Manifesto YAML existe em `containers/<tipo>/<componente>/`
- [ ] Tag da imagem no manifesto corresponde à tag do build (`oai-<tipo>-<componente>:test`)
- [ ] `imagePullPolicy: IfNotPresent` está no manifesto
- [ ] Build completou sem erros
- [ ] Imagem foi carregada no Kind com `kind load docker-image`
- [ ] Pod atingiu estado `Running` ou `Completed`
- [ ] Logs mostram execução bem-sucedida (sem crashes)


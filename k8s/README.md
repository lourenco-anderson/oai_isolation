# OAI Isolation - Kubernetes Deployment Guide

Este diretório contém manifestos Kubernetes (K8s) e scripts para fazer deploy dos containers OAI Isolation em um cluster Kubernetes.

## 📋 Estrutura

```
k8s/
├── README.md                           # Este arquivo
├── namespace.yaml                      # Namespace do projeto
├── kustomization.yaml                  # Arquivo Kustomize para gerenciar todos os recursos
│
├── gnb/                               # Deployments GNB (gNodeB)
│   ├── gnb-crc-deployment.yaml
│   ├── gnb-layer-map-deployment.yaml
│   ├── gnb-ldpc-deployment.yaml
│   ├── gnb-modulation-deployment.yaml
│   ├── gnb-ofdm-mod-deployment.yaml
│   ├── gnb-precoding-deployment.yaml
│   └── gnb-scramble-deployment.yaml
│
├── ue/                                # Deployments UE (User Equipment)
│   ├── ue-ch-est-deployment.yaml
│   ├── ue-ch-mmse-deployment.yaml
│   ├── ue-check-crc-deployment.yaml
│   ├── ue-descrambling-deployment.yaml
│   ├── ue-layer-demap-deployment.yaml
│   ├── ue-ldpc-dec-deployment.yaml
│   ├── ue-ofdm-demod-deployment.yaml
│   └── ue-soft-demod-deployment.yaml
│
└── Scripts
    ├── build-images.sh                # Build de todas as imagens Docker
    ├── deploy.sh                      # Deploy para Kubernetes
    ├── kind-setup.sh                  # Gerenciamento de cluster Kind
    └── monitor.sh                     # Monitor de status dos deployments
```

## 🚀 Pré-requisitos

### Obrigatório
- **Docker**: 20.10+
- **kubectl**: 1.28+
- **Kustomize**: 5.0+ (opcional, kubectl tem suporte integrado)

### Para desenvolvimento local
- **Kind**: 0.20+ (para criar cluster Kubernetes local)
- **Helm**: 3.12+ (opcional, para gerenciar charts)

### Instalação rápida (Linux/macOS)

```bash
# Kind
curl -Lo ./kind https://kind.sigs.k8s.io/dl/v0.27.0/kind-linux-amd64
chmod +x ./kind
sudo mv ./kind /usr/local/bin/kind

# kubectl
curl -LO "https://dl.k8s.io/release/$(curl -L -s https://dl.k8s.io/release/stable.txt)/bin/linux/amd64/kubectl"
chmod +x kubectl
sudo mv kubectl /usr/local/bin/

# Kustomize
curl -s "https://raw.githubusercontent.com/kubernetes-sigs/kustomize/master/hack/install_kustomize.sh" | bash
sudo mv kustomize /usr/local/bin/
```

## 📦 Componentes Containerizados

### GNB (gNodeB) - 7 funções
| Função | Porta | Descrição |
|--------|-------|-----------|
| `gnb-crc` | 8080 | CRC Encoding/Decoding |
| `gnb-layer-map` | 8081 | Layer Mapping |
| `gnb-ldpc` | 8082 | LDPC Encoding |
| `gnb-modulation` | 8083 | 256-QAM Modulation |
| `gnb-ofdm-mod` | 8084 | OFDM Modulation |
| `gnb-precoding` | 8085 | Precoding |
| `gnb-scramble` | 8086 | Scrambling |

### UE (User Equipment) - 8 funções
| Função | Porta | Descrição |
|--------|-------|-----------|
| `ue-ch-est` | 9080 | Channel Estimation |
| `ue-ch-mmse` | 9081 | MMSE Channel Estimation |
| `ue-check-crc` | 9082 | CRC Check |
| `ue-descrambling` | 9083 | Descrambling |
| `ue-layer-demap` | 9084 | Layer Demapping |
| `ue-ldpc-dec` | 9085 | LDPC Decoding |
| `ue-ofdm-demod` | 9086 | OFDM Demodulation |
| `ue-soft-demod` | 9087 | Soft Demodulation |

## 🔧 Quick Start

### 1. Criar cluster local com Kind (opcional)

```bash
./kind-setup.sh create
```

### 2. Build das imagens Docker

```bash
# Build com tag 'latest' (padrão: localhost:5000)
./build-images.sh

# Ou especificar registry e tag customizados
./build-images.sh myregistry.com v1.0.0
```

**Nota:** Se usar Kind localmente, as imagens built com `docker build` estarão disponíveis automaticamente.

### 3. Deploy no Kubernetes

```bash
# Deploy com namespace padrão (oai-isolation)
./deploy.sh

# Ou especificar namespace customizado
./deploy.sh meu-namespace
```

### 4. Verificar status

```bash
# Listar pods
kubectl get pods -n oai-isolation

# Listar services
kubectl get svc -n oai-isolation

# Monitorar em tempo real
./monitor.sh
```

### 5. Acessar logs

```bash
# Ver logs de um pod específico
kubectl logs -n oai-isolation <pod-name>

# Ou seguir logs em tempo real
kubectl logs -f -n oai-isolation <pod-name>
```

## 📝 Arquitetura dos Manifestos

### Deployment
Cada serviço é um `Deployment` com:
- **1 réplica** por padrão (ajustável via `spec.replicas`)
- **Rolling updates** habilitados
- **Resource limits** definidos (requests + limits)
- **Restart policy**: Always

### Service
Cada `Deployment` tem um `Service` associado (tipo `ClusterIP`) para:
- **Service discovery** dentro do cluster
- **Load balancing** entre pods

### Namespace
Todos os recursos são isolados no namespace `oai-isolation`:
```bash
kubectl get all -n oai-isolation
```

## 🛠️ Customizações

### Ajustar replicas

Edite o arquivo YAML do deployment:
```yaml
spec:
  replicas: 3  # De 1 para 3
```

Ou use kubectl:
```bash
kubectl scale deployment gnb-crc -n oai-isolation --replicas=3
```

### Modificar limites de recursos

Nos arquivos de deployment, ajuste `resources`:
```yaml
resources:
  requests:
    memory: "512Mi"
    cpu: "500m"
  limits:
    memory: "1Gi"
    cpu: "1000m"
```

### Usar registry externo

Modifique o `kustomization.yaml` para adicionar:
```yaml
images:
- name: oai-isolation
  newName: myregistry.com/oai-isolation
  newTag: v1.0.0
```

## 📊 Monitoramento

### Dashboard do Kubernetes

```bash
# Instalar Kubernetes Dashboard (opcional)
kubectl apply -f https://raw.githubusercontent.com/kubernetes/dashboard/v2.7.0/aio/deploy/recommended.yaml

# Acessar
kubectl proxy
# Visite: http://localhost:8001/api/v1/namespaces/kubernetes-dashboard/services/https:kubernetes-dashboard:/proxy/
```

### Prometheus + Grafana

Veja o arquivo `containers/README.md` para instruções de monitoramento de energia com Kepler.

## 🔐 Segurança

Ajustes de segurança nos manifestos:
- `securityContext` com `runAsNonRoot: false` (necessário para acesso a drivers)
- `readOnlyRootFilesystem: false` (necessário para writes)
- Volumes `emptyDir` para dados temporários

⚠️ **Para produção**, considere:
- Usar `ServiceAccount` com RBAC
- Implementar `NetworkPolicy`
- Usar `Pod Security Policies`
- Configurar `Resource Quotas`

## 🧹 Cleanup

### Remover deployments

```bash
# Remover namespace (remove todos os recursos dentro)
kubectl delete namespace oai-isolation

# Ou remover apenas alguns deployments
kubectl delete deployment gnb-crc -n oai-isolation
```

### Remover cluster Kind

```bash
./kind-setup.sh delete
```

### Remover imagens Docker

```bash
docker rmi oai-isolation:gnb-crc
docker rmi oai-isolation:ue-check-crc
# ... etc
```

## 🐛 Troubleshooting

### Pods não iniciam

```bash
# Ver descrição detalhada
kubectl describe pod <pod-name> -n oai-isolation

# Ver logs
kubectl logs <pod-name> -n oai-isolation
```

### Imagens não encontradas

```bash
# Verificar imagens disponíveis
docker images | grep oai-isolation

# Se usar Kind, carregar imagem para o cluster
kind load docker-image oai-isolation:gnb-crc --name oai-isolation-cluster
```

### Service discovery não funciona

```bash
# Verificar Service
kubectl get svc -n oai-isolation

# Testar conectividade entre pods
kubectl run -it --rm debug --image=busybox --restart=Never -- sh
# Dentro do pod
nslookup gnb-crc.oai-isolation.svc.cluster.local
```

## 📚 Referências

- [Kubernetes Official Docs](https://kubernetes.io/docs/)
- [Kustomize](https://kustomize.io/)
- [Kind](https://kind.sigs.k8s.io/)
- [kubectl Cheat Sheet](https://kubernetes.io/docs/reference/kubectl/cheatsheet/)

## 📞 Suporte

Para dúvidas ou problemas:
1. Verifique os logs: `kubectl logs -n oai-isolation <pod-name>`
2. Inspecione o pod: `kubectl describe pod -n oai-isolation <pod-name>`
3. Teste conectividade: `kubectl exec -it <pod-name> -n oai-isolation -- bash`

---

**Última atualização**: Dezembro 2025

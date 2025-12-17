# Arquitetura - OAI Isolation + Kubernetes

## 📐 Visão Geral

Este documento descreve a arquitetura do deployment dos containers OAI Isolation em Kubernetes.

```
┌─────────────────────────────────────────────────────────────────┐
│                      Kubernetes Cluster                         │
│                   (Kind ou Cloud Provider)                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │  Namespace: oai-isolation                               │  │
│  │                                                         │  │
│  │  ┌──────────────────────────────────────────────────┐  │  │
│  │  │  GNB Pod Layer (Transmitter Side)               │  │  │
│  │  │                                                 │  │  │
│  │  │  ┌────────────┐ ┌────────────┐ ┌────────────┐  │  │  │
│  │  │  │ gnb-crc    │ │gnb-layer-  │ │ gnb-ldpc   │  │  │  │
│  │  │  │            │ │  map       │ │            │  │  │  │
│  │  │  └──────┬─────┘ └──────┬─────┘ └──────┬─────┘  │  │  │
│  │  │         │              │              │        │  │  │
│  │  │  ┌────────────┐ ┌────────────┐ ┌────────────┐  │  │  │
│  │  │  │gnb-modulation│gnb-ofdm-  │ │gnb-precod- │  │  │  │
│  │  │  │            │ │   mod     │ │ ing/scramble  │  │  │
│  │  │  └──────┬─────┘ └──────┬─────┘ └──────┬─────┘  │  │  │
│  │  │         └──────────────┴───────────────┘        │  │  │
│  │  │                    Output RF                     │  │  │
│  │  └──────────────────────────────────────────────────┘  │  │
│  │                                                         │  │
│  │  ┌──────────────────────────────────────────────────┐  │  │
│  │  │  UE Pod Layer (Receiver Side)                    │  │  │
│  │  │                                                 │  │  │
│  │  │  ┌────────────┐ ┌────────────┐ ┌────────────┐  │  │  │
│  │  │  │ue-ofdm-    │ │ ue-soft-   │ │ue-layer-   │  │  │  │
│  │  │  │ demod      │ │ demod      │ │ demap      │  │  │  │
│  │  │  └──────┬─────┘ └──────┬─────┘ └──────┬─────┘  │  │  │
│  │  │         │              │              │        │  │  │
│  │  │  ┌────────────┐ ┌────────────┐ ┌────────────┐  │  │  │
│  │  │  │ue-ldpc-dec │ │ue-descr-   │ │ue-ch-est   │  │  │  │
│  │  │  │            │ │ambling     │ │            │  │  │  │
│  │  │  └──────┬─────┘ └──────┬─────┘ └──────┬─────┘  │  │  │
│  │  │         │              │              │        │  │  │
│  │  │  ┌────────────┐ ┌────────────┐        │        │  │  │
│  │  │  │ue-check-crc│ │ue-ch-mmse  │        │        │  │  │
│  │  │  │            │ │            │        │        │  │  │
│  │  │  └──────┬─────┘ └──────┬─────┘        │        │  │  │
│  │  │         └──────────────┴────────────────┘       │  │  │
│  │  │                 Decoded Data                     │  │  │
│  │  └──────────────────────────────────────────────────┘  │  │
│  │                                                         │  │
│  └─────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │  Services (Service Discovery)                           │  │
│  │                                                         │  │
│  │  ∘ gnb-crc.oai-isolation.svc.cluster.local:8080       │  │
│  │  ∘ gnb-layer-map.oai-isolation.svc.cluster.local:8081  │  │
│  │  ∘ gnb-ldpc.oai-isolation.svc.cluster.local:8082       │  │
│  │  ∘ gnb-modulation.oai-isolation.svc.cluster.local:8083 │  │
│  │  ∘ gnb-ofdm-mod.oai-isolation.svc.cluster.local:8084   │  │
│  │  ∘ gnb-precoding.oai-isolation.svc.cluster.local:8085  │  │
│  │  ∘ gnb-scramble.oai-isolation.svc.cluster.local:8086   │  │
│  │  ∘ ue-ch-est.oai-isolation.svc.cluster.local:9080      │  │
│  │  ∘ ue-ch-mmse.oai-isolation.svc.cluster.local:9081     │  │
│  │  ∘ ue-check-crc.oai-isolation.svc.cluster.local:9082   │  │
│  │  ∘ ue-descrambling.oai-isolation.svc.cluster.local:9083│  │
│  │  ∘ ue-layer-demap.oai-isolation.svc.cluster.local:9084 │  │
│  │  ∘ ue-ldpc-dec.oai-isolation.svc.cluster.local:9085    │  │
│  │  ∘ ue-ofdm-demod.oai-isolation.svc.cluster.local:9086  │  │
│  │  ∘ ue-soft-demod.oai-isolation.svc.cluster.local:9087  │  │
│  │                                                         │  │
│  └─────────────────────────────────────────────────────────┘  │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

## 🏗️ Componentes

### 1. **Namespace**
- **Nome**: `oai-isolation`
- **Propósito**: Isolar recursos da aplicação em um namespace dedicado
- **Arquivo**: `namespace.yaml`

### 2. **Deployments GNB (7 serviços)**

| Serviço | Container | Porta | Função |
|---------|-----------|-------|--------|
| gnb-crc | oai-isolation:gnb-crc | 8080 | CRC Encoding/Decoding |
| gnb-layer-map | oai-isolation:gnb-layer-map | 8081 | Layer Mapping |
| gnb-ldpc | oai-isolation:gnb-ldpc | 8082 | LDPC Encoding |
| gnb-modulation | oai-isolation:gnb-modulation | 8083 | 256-QAM Modulation |
| gnb-ofdm-mod | oai-isolation:gnb-ofdm-mod | 8084 | OFDM Modulation |
| gnb-precoding | oai-isolation:gnb-precoding | 8085 | Precoding |
| gnb-scramble | oai-isolation:gnb-scramble | 8086 | Scrambling |

**Características:**
- 1 réplica por padrão (ajustável)
- Rolling updates habilitado
- Resource limits: 256-512 MiB RAM, 250-500m CPU
- Restart policy: Always

### 3. **Deployments UE (8 serviços)**

| Serviço | Container | Porta | Função |
|---------|-----------|-------|--------|
| ue-ch-est | oai-isolation:ue-ch-est | 9080 | Channel Estimation |
| ue-ch-mmse | oai-isolation:ue-ch-mmse | 9081 | MMSE Channel Estimation |
| ue-check-crc | oai-isolation:ue-check-crc | 9082 | CRC Check |
| ue-descrambling | oai-isolation:ue-descrambling | 9083 | Descrambling |
| ue-layer-demap | oai-isolation:ue-layer-demap | 9084 | Layer Demapping |
| ue-ldpc-dec | oai-isolation:ue-ldpc-dec | 9085 | LDPC Decoding |
| ue-ofdm-demod | oai-isolation:ue-ofdm-demod | 9086 | OFDM Demodulation |
| ue-soft-demod | oai-isolation:ue-soft-demod | 9087 | Soft Demodulation |

**Características:**
- 1 réplica por padrão (ajustável)
- Rolling updates habilitado
- Resource limits: 512 MiB - 1 GiB RAM, 500m - 1000m CPU
- Restart policy: Always

### 4. **Services**

Cada deployment tem um Service associado (tipo `ClusterIP`) para:
- **Service Discovery**: Comunicação entre pods via DNS
- **Load Balancing**: Distribuição de tráfego entre réplicas
- **Isolamento**: Pods não acessíveis diretamente de fora do cluster

**DNS Naming Pattern**: `<service-name>.<namespace>.svc.cluster.local`

## 📦 Camadas de Processamento

### GNB (gNodeB - Transmitter)
Fluxo de processamento (pipeline):
```
Input Data
    ↓
[CRC Encoding] → gnb-crc:8080
    ↓
[Layer Mapping] → gnb-layer-map:8081
    ↓
[LDPC Encoding] → gnb-ldpc:8082
    ↓
[256-QAM Modulation] → gnb-modulation:8083
    ↓
[OFDM Modulation] → gnb-ofdm-mod:8084
    ↓
[Precoding] → gnb-precoding:8085
    ↓
[Scrambling] → gnb-scramble:8086
    ↓
RF Signal Output
```

### UE (User Equipment - Receiver)
Fluxo de processamento (pipeline):
```
RF Signal Input
    ↓
[OFDM Demodulation] → ue-ofdm-demod:9086
    ↓
[Soft Demodulation] → ue-soft-demod:9087
    ↓
[Layer Demapping] → ue-layer-demap:9084
    ↓
[LDPC Decoding] → ue-ldpc-dec:9085
    ↓
[Descrambling] → ue-descrambling:9083
    ↓
[Channel Estimation] → ue-ch-est:9080 (paralelo com MMSE)
    ↓
[MMSE Equalization] → ue-ch-mmse:9081
    ↓
[CRC Check] → ue-check-crc:9082
    ↓
Decoded Data Output
```

## 🔄 Ciclo de Vida dos Pods

```
┌─────────────────────────────────────────────────────────┐
│                    Pod Lifecycle                        │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  Pending ──→ Running ──→ Succeeded/Failed ──→ Terminating
│     ↓                                            ↓
│  (Waiting for         (Container             (Cleanup)
│   resources)          executing)
│                                                         │
├─────────────────────────────────────────────────────────┤
│ Restart Policy: Always                                  │
│ - Pod falha → Kubernetes reinicia automaticamente       │
│ - Backoff exponencial (1s, 2s, 4s, 8s, 16s, 32s...)    │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

## 📊 Resource Management

### Resource Requests (Reservados)
```yaml
GNB (CRC):
  Memory: 256 Mi
  CPU: 250 m

GNB (Outros):
  Memory: 512 Mi
  CPU: 500 m

UE (CRC Check):
  Memory: 256 Mi
  CPU: 250 m

UE (Outros):
  Memory: 512 Mi
  CPU: 500 m
```

### Resource Limits (Máximo)
```yaml
GNB (CRC):
  Memory: 512 Mi
  CPU: 500 m

GNB (Outros):
  Memory: 1 Gi
  CPU: 1000 m

UE (CRC Check):
  Memory: 512 Mi
  CPU: 500 m

UE (Outros):
  Memory: 1 Gi
  CPU: 1000 m
```

**Total Recursos Necessários:**
- **RAM Total**: ~12.5 GiB (15 serviços × ~800-900 MiB médio)
- **CPU Total**: ~8-9 CPUs (15 serviços × ~600m médio)

## 🔐 Segurança

### Network Policies
```yaml
# Por padrão, tráfego entre namespaces é bloqueado
# Comunicação dentro do namespace é permitida
# Necessário criar NetworkPolicies para tráfego externo
```

### Pod Security
```yaml
securityContext:
  runAsNonRoot: false
  allowPrivilegeEscalation: false
  readOnlyRootFilesystem: false
```

⚠️ **Para Produção:**
- Implementar `NetworkPolicies`
- Usar `ServiceAccount` com RBAC
- Habilitar `Pod Security Policies`
- Usar imagens de base mínimas
- Configurar `Resource Quotas` e `LimitRanges`

## 📈 Escalabilidade

### Horizontal Scaling
```bash
# Aumentar replicas
kubectl scale deployment gnb-crc -n oai-isolation --replicas=3
```

### Vertical Scaling
```bash
# Aumentar recursos (requer restart)
# Editar manifest e aplicar novamente
kubectl apply -f gnb-crc-deployment.yaml
```

### Auto Scaling (HPA)
```yaml
# Implementar Horizontal Pod Autoscaler
apiVersion: autoscaling/v2
kind: HorizontalPodAutoscaler
metadata:
  name: gnb-crc-hpa
spec:
  scaleTargetRef:
    apiVersion: apps/v1
    kind: Deployment
    name: gnb-crc
  minReplicas: 1
  maxReplicas: 5
  metrics:
  - type: Resource
    resource:
      name: cpu
      target:
        type: Utilization
        averageUtilization: 80
```

## 📝 Gerenciamento de Configuração

### ConfigMaps (Futura Implementação)
```yaml
apiVersion: v1
kind: ConfigMap
metadata:
  name: oai-isolation-config
  namespace: oai-isolation
data:
  log_level: INFO
  debug_mode: "false"
  timeout_seconds: "30"
```

### Secrets (Futura Implementação)
```yaml
apiVersion: v1
kind: Secret
metadata:
  name: oai-isolation-secrets
  namespace: oai-isolation
type: Opaque
stringData:
  api_key: "your-secret-key"
  db_password: "secure-password"
```

## 🔍 Monitoramento e Observabilidade

### Métricas (Prometheus)
- CPU usage por pod
- Memory usage por pod
- Network I/O por pod
- Restart count
- Pod creation latency

### Logs (ELK Stack ou Loki)
- STDOUT/STDERR dos containers
- Application logs
- Error tracking

### Health Checks (Futura Implementação)
```yaml
livenessProbe:
  httpGet:
    path: /health
    port: 8080
  initialDelaySeconds: 10
  periodSeconds: 10

readinessProbe:
  httpGet:
    path: /ready
    port: 8080
  initialDelaySeconds: 5
  periodSeconds: 5
```

## 🚀 Deployment Workflow

```
1. Build Images
   docker build → Local Registry
   
2. Load Images (Kind)
   kind load docker-image
   
3. Apply Manifests
   kustomize build → kubectl apply
   
4. Verify Deployment
   kubectl get pods/services
   
5. Monitor
   kubectl logs/describe
   
6. Cleanup
   kubectl delete namespace
```

## 📋 Arquivos e Estrutura

```
k8s/
├── README.md                    # Documentação
├── ARCHITECTURE.md              # Este arquivo
├── namespace.yaml               # Namespace do projeto
├── kustomization.yaml           # Arquivo Kustomize
├── docker-compose.yaml          # Alternativa local
│
├── gnb-*-deployment.yaml        # 7 deployments GNB
├── ue-*-deployment.yaml         # 8 deployments UE
│
├── build-images.sh              # Build todas imagens
├── deploy.sh                    # Deploy no K8s
├── kind-setup.sh                # Gerencia cluster Kind
├── quickstart.sh                # Automatiza tudo
├── monitor.sh                   # Monitora status
│
└── Makefile                     # Automação de comandos
```

## 🔗 Comunicação Inter-Pod

### DNS Service Discovery
```
Pod A ──→ Service DNS ──→ kube-dns ──→ Pod B
         (gnb-crc.oai-isolation.svc.cluster.local)
```

### Network Plugins Suportados
- Flannel (padrão Kind)
- Calico
- Weave
- Cilium

## ✅ Checklist de Validação

- [ ] Todos os pods em `Running` estado
- [ ] Services criados e descobertos
- [ ] Resource limits respeitados
- [ ] Logs sem erros críticos
- [ ] Comunicação inter-pod funcionando
- [ ] Pods reiniciando corretamente após falhas

---

**Documento atualizado**: Dezembro 2025
**Versão**: 1.0

# Monitoramento de Energia com Kepler - OAI Isolation

## 📊 Visão Geral

Cada função (GNB e UE) está rodando como um **pod separado** no Kubernetes, permitindo que o **Kepler** capture:
- Consumo de energia (Watts) por pod
- CPU usage por pod
- Memory usage por pod
- Disk I/O por pod
- Network I/O por pod

## 🏗️ Arquitetura de Monitoramento

```
┌─────────────────────────────────────────────────────────────┐
│                   Kubernetes Cluster                        │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌──────────────────────────────────────────────────────┐  │
│  │         Namespace: oai-isolation                     │  │
│  │                                                      │  │
│  │  ┌─────────────┐  ┌─────────────┐  ┌──────────────┐ │  │
│  │  │ gnb-crc     │  │gnb-layer-map│  │ gnb-ldpc     │ │  │
│  │  │   (Pod 1)   │  │   (Pod 2)   │  │   (Pod 3)    │ │  │
│  │  └─────────────┘  └─────────────┘  └──────────────┘ │  │
│  │                                                      │  │
│  │  ┌─────────────┐  ┌─────────────┐  ┌──────────────┐ │  │
│  │  │gnb-modulation│ │gnb-ofdm-mod │  │gnb-precoding│ │  │
│  │  │   (Pod 4)   │  │   (Pod 5)   │  │   (Pod 6)   │ │  │
│  │  └─────────────┘  └─────────────┘  └──────────────┘ │  │
│  │                                                      │  │
│  │  ┌─────────────┐  ┌─────────────┐  ┌──────────────┐ │  │
│  │  │gnb-scramble │  │ ue-ch-est   │  │ ue-ch-mmse   │ │  │
│  │  │   (Pod 7)   │  │   (Pod 8)   │  │   (Pod 9)    │ │  │
│  │  └─────────────┘  └─────────────┘  └──────────────┘ │  │
│  │                                                      │  │
│  │  ┌─────────────┐  ┌─────────────┐  ┌──────────────┐ │  │
│  │  │ue-check-crc │  │ue-descramble│  │ue-layer-dem  │ │  │
│  │  │  (Pod 10)   │  │  (Pod 11)   │  │  (Pod 12)    │ │  │
│  │  └─────────────┘  └─────────────┘  └──────────────┘ │  │
│  │                                                      │  │
│  │  ┌─────────────┐  ┌─────────────┐                   │  │
│  │  │ue-ldpc-dec  │  │ue-ofdm-demod│  ┌──────────────┐ │  │
│  │  │  (Pod 13)   │  │  (Pod 14)   │  │ue-soft-demod │ │  │
│  │  └─────────────┘  └─────────────┘  │  (Pod 15)    │ │  │
│  │                                     └──────────────┘ │  │
│  └──────────────────────────────────────────────────────┘  │
│                          ▲                                 │
│                          │                                 │
│  ┌──────────────────────────────────────────────────────┐  │
│  │    Kepler DaemonSet (1 pod por node)                │  │
│  │  - Coleta métricas de energia de cada container     │  │
│  │  - Expõe métricas em /metrics (porta 8888)          │  │
│  │  - Labels: pod_name, namespace, container_id        │  │
│  └──────────────────────────────────────────────────────┘  │
│                          │                                 │
│                          ▼                                 │
│  ┌──────────────────────────────────────────────────────┐  │
│  │      Prometheus (coleta + armazenamento)             │  │
│  │  - Scrape Kepler a cada 30s                          │  │
│  │  - Armazena séries temporais                         │  │
│  │  - Retenção: 15 dias                                 │  │
│  └──────────────────────────────────────────────────────┘  │
│                          │                                 │
│                          ▼                                 │
│  ┌──────────────────────────────────────────────────────┐  │
│  │        Grafana (visualização)                        │  │
│  │  - Dashboards customizados por pod                   │  │
│  │  - Gráficos de energia em tempo real                 │  │
│  │  - Alertas configuráveis                             │  │
│  └──────────────────────────────────────────────────────┘  │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

## 📦 Componentes por Pod

### GNB (7 Pods)
| Pod | Função | Porta | Métricas Kepler |
|-----|--------|-------|-----------------|
| gnb-crc | CRC Encoding | 8080 | cpu, memory, energy |
| gnb-layer-map | Layer Mapping | 8081 | cpu, memory, energy |
| gnb-ldpc | LDPC Encoding | 8082 | cpu, memory, energy |
| gnb-modulation | 256-QAM Modulation | 8083 | cpu, memory, energy |
| gnb-ofdm-mod | OFDM Modulation | 8084 | cpu, memory, energy |
| gnb-precoding | Precoding | 8085 | cpu, memory, energy |
| gnb-scramble | Scrambling | 8086 | cpu, memory, energy |

### UE (8 Pods)
| Pod | Função | Porta | Métricas Kepler |
|-----|--------|-------|-----------------|
| ue-ch-est | Channel Estimation | 9080 | cpu, memory, energy |
| ue-ch-mmse | MMSE Equalization | 9081 | cpu, memory, energy |
| ue-check-crc | CRC Check | 9082 | cpu, memory, energy |
| ue-descrambling | Descrambling | 9083 | cpu, memory, energy |
| ue-layer-demap | Layer Demapping | 9084 | cpu, memory, energy |
| ue-ldpc-dec | LDPC Decoding | 9085 | cpu, memory, energy |
| ue-ofdm-demod | OFDM Demodulation | 9086 | cpu, memory, energy |
| ue-soft-demod | Soft Demodulation | 9087 | cpu, memory, energy |

**Total: 15 pods com monitoramento independente**

## 🔧 Instalação do Kepler

### 1. Adicionar Helm Repository

```bash
helm repo add kepler https://sustainable-computing-io.github.io/kepler-helm-chart
helm repo update
```

### 2. Instalar Kepler

```bash
# Criar namespace para monitoramento
kubectl create namespace kepler

# Instalar Kepler
helm install kepler kepler/kepler \
  --namespace kepler \
  --set serviceAccount.create=true \
  --set rbac.create=true \
  --set daemonset.hostNetwork=true
```

### 3. Verificar Instalação

```bash
# Ver pods
kubectl get pods -n kepler

# Ver logs
kubectl logs -n kepler -l app.kubernetes.io/name=kepler

# Verificar métricas
kubectl port-forward -n kepler svc/kepler 8888:8888
# Acesse: http://localhost:8888/metrics
```

## 📊 Métricas Disponíveis do Kepler

### Métricas de Energia
```promql
# Energia total consumida por pod (Joules)
kepler_container_energy_total

# Energia consumida por CPU (Joules)
kepler_container_cpu_energy_total

# Energia consumida por DRAM (Joules)
kepler_container_dram_energy_total

# Potência instantânea (Watts)
rate(kepler_container_energy_total[1m])
```

### Métricas de Recursos
```promql
# CPU usage por pod
container_cpu_usage_seconds_total

# Memory usage por pod
container_memory_usage_bytes

# Network I/O por pod
container_network_transmit_bytes_total
container_network_receive_bytes_total

# Disk I/O por pod
container_fs_usage_bytes
```

## 🔌 Instalação do Prometheus

### 1. Adicionar Helm Repository

```bash
helm repo add prometheus-community https://prometheus-community.github.io/helm-charts
helm repo update
```

### 2. Instalar Prometheus

```bash
# Criar namespace
kubectl create namespace prometheus

# Instalar Prometheus
helm install prometheus prometheus-community/kube-prometheus-stack \
  --namespace prometheus \
  --set prometheus.prometheusSpec.additionalScrapeConfigs[0].job_name="kepler" \
  --set prometheus.prometheusSpec.additionalScrapeConfigs[0].static_configs[0].targets="kepler.kepler.svc.cluster.local:8888"
```

### 3. Verificar Instalação

```bash
# Ver pods
kubectl get pods -n prometheus

# Acessar Prometheus
kubectl port-forward -n prometheus svc/prometheus-kube-prometheus-prometheus 9090:9090
# Acesse: http://localhost:9090
```

## 📈 Instalação do Grafana

### 1. O Prometheus Stack já inclui Grafana

```bash
# Obter senha do admin
kubectl get secret -n prometheus prometheus-grafana \
  -o jsonpath="{.data.admin-password}" | base64 --decode

# Port forward
kubectl port-forward -n prometheus svc/prometheus-grafana 3000:80
# Acesse: http://localhost:3000
# User: admin
# Password: <output acima>
```

### 2. Criar Dashboard Customizado

#### Dashboard 1: Consumo de Energia por Função GNB

```json
{
  "dashboard": {
    "title": "OAI GNB - Energy Consumption",
    "panels": [
      {
        "title": "Energy Usage by GNB Function",
        "targets": [
          {
            "expr": "sum by (pod_name) (rate(kepler_container_energy_total{namespace='oai-isolation', pod_name=~'gnb-.*'}[5m]))"
          }
        ],
        "type": "graph"
      },
      {
        "title": "Power (Watts) - GNB Functions",
        "targets": [
          {
            "expr": "rate(kepler_container_cpu_energy_total{namespace='oai-isolation', pod_name=~'gnb-.*'}[1m]) * 1000"
          }
        ],
        "type": "heatmap"
      },
      {
        "title": "Energy vs CPU Usage",
        "targets": [
          {
            "expr": "kepler_container_energy_total{namespace='oai-isolation', pod_name=~'gnb-.*'}"
          }
        ],
        "type": "stat"
      }
    ]
  }
}
```

#### Dashboard 2: Consumo de Energia por Função UE

```json
{
  "dashboard": {
    "title": "OAI UE - Energy Consumption",
    "panels": [
      {
        "title": "Energy Usage by UE Function",
        "targets": [
          {
            "expr": "sum by (pod_name) (rate(kepler_container_energy_total{namespace='oai-isolation', pod_name=~'ue-.*'}[5m]))"
          }
        ],
        "type": "graph"
      },
      {
        "title": "Power (Watts) - UE Functions",
        "targets": [
          {
            "expr": "rate(kepler_container_dram_energy_total{namespace='oai-isolation', pod_name=~'ue-.*'}[1m]) * 1000"
          }
        ],
        "type": "heatmap"
      },
      {
        "title": "Total Energy Consumed",
        "targets": [
          {
            "expr": "sum(kepler_container_energy_total{namespace='oai-isolation'})"
          }
        ],
        "type": "gauge"
      }
    ]
  }
}
```

#### Dashboard 3: Comparativo GNB vs UE

```json
{
  "dashboard": {
    "title": "OAI - GNB vs UE Comparison",
    "panels": [
      {
        "title": "Total Energy: GNB vs UE",
        "targets": [
          {
            "expr": "sum(rate(kepler_container_energy_total{namespace='oai-isolation', pod_name=~'gnb-.*'}[5m]))"
          },
          {
            "expr": "sum(rate(kepler_container_energy_total{namespace='oai-isolation', pod_name=~'ue-.*'}[5m]))"
          }
        ],
        "type": "graph"
      },
      {
        "title": "Energy by Function (Stacked)",
        "targets": [
          {
            "expr": "kepler_container_energy_total{namespace='oai-isolation'}"
          }
        ],
        "type": "piechart"
      },
      {
        "title": "Top 5 Energy Consumers",
        "targets": [
          {
            "expr": "topk(5, sum by (pod_name) (kepler_container_energy_total{namespace='oai-isolation'}))"
          }
        ],
        "type": "table"
      }
    ]
  }
}
```

## 🚀 Quick Start - Stack Completo

```bash
#!/bin/bash

# 1. Setup OAI Isolation pods
cd k8s
./deploy.sh oai-isolation

# 2. Setup Kepler
helm repo add kepler https://sustainable-computing-io.github.io/kepler-helm-chart
helm repo update
kubectl create namespace kepler
helm install kepler kepler/kepler --namespace kepler

# 3. Setup Prometheus + Grafana
helm repo add prometheus-community https://prometheus-community.github.io/helm-charts
helm repo update
kubectl create namespace prometheus
helm install prometheus prometheus-community/kube-prometheus-stack \
  --namespace prometheus

# 4. Port forward Grafana
kubectl port-forward -n prometheus svc/prometheus-grafana 3000:80 &

# 5. Port forward Prometheus
kubectl port-forward -n prometheus svc/prometheus-kube-prometheus-prometheus 9090:9090 &

echo "Grafana: http://localhost:3000 (admin/grafana)"
echo "Prometheus: http://localhost:9090"
```

## 📊 Queries PromQL Úteis

### Consumo Total de Energia
```promql
# Total de energia consumida (todos os pods)
sum(kepler_container_energy_total{namespace="oai-isolation"})

# Energia consumida por pod
sum by (pod_name) (kepler_container_energy_total{namespace="oai-isolation"})

# Taxa de consumo (Watts)
rate(kepler_container_energy_total{namespace="oai-isolation"}[5m])
```

### Comparação GNB vs UE
```promql
# Consumo GNB
sum(rate(kepler_container_energy_total{namespace="oai-isolation", pod_name=~"gnb-.*"}[5m]))

# Consumo UE
sum(rate(kepler_container_energy_total{namespace="oai-isolation", pod_name=~"ue-.*"}[5m]))

# Razão GNB/UE
sum(rate(kepler_container_energy_total{namespace="oai-isolation", pod_name=~"gnb-.*"}[5m])) / 
sum(rate(kepler_container_energy_total{namespace="oai-isolation", pod_name=~"ue-.*"}[5m]))
```

### Análise por Função
```promql
# Top 5 funções mais consumidoras
topk(5, sum by (pod_name) (kepler_container_energy_total{namespace="oai-isolation"}))

# Função com maior consumo relativo
max by (pod_name) (rate(kepler_container_cpu_energy_total{namespace="oai-isolation"}[5m]))

# Distribuição de energia
kepler_container_cpu_energy_total{namespace="oai-isolation"} / 
(kepler_container_cpu_energy_total{namespace="oai-isolation"} + 
 kepler_container_dram_energy_total{namespace="oai-isolation"})
```

### Alertas
```yaml
# Exemplo de regra de alerta
groups:
- name: oai-isolation
  rules:
  - alert: HighEnergyConsumption
    expr: rate(kepler_container_energy_total{namespace="oai-isolation"}[5m]) > 10
    for: 5m
    annotations:
      summary: "High energy consumption detected in {{ $labels.pod_name }}"
      
  - alert: PodEnergySpike
    expr: rate(kepler_container_energy_total{namespace="oai-isolation"}[1m]) > 2 * avg_over_time(rate(kepler_container_energy_total{namespace="oai-isolation"}[1m])[1h:1m])
    for: 2m
    annotations:
      summary: "Energy spike in {{ $labels.pod_name }}"
```

## 📝 Arquivos de Configuração

### ConfigMap para Alertas
```yaml
apiVersion: v1
kind: ConfigMap
metadata:
  name: prometheus-oai-rules
  namespace: prometheus
data:
  oai-isolation-rules.yaml: |
    groups:
    - name: oai-isolation
      interval: 30s
      rules:
      - record: oai:gnb:energy:rate5m
        expr: rate(kepler_container_energy_total{namespace="oai-isolation", pod_name=~"gnb-.*"}[5m])
      
      - record: oai:ue:energy:rate5m
        expr: rate(kepler_container_energy_total{namespace="oai-isolation", pod_name=~"ue-.*"}[5m])
```

## ✅ Checklist de Validação

- [ ] OAI Isolation pods rodando em oai-isolation namespace
- [ ] Kepler pod rodando em kepler namespace
- [ ] Kepler metrics acessíveis em :8888/metrics
- [ ] Prometheus conectado e coletando dados de Kepler
- [ ] Grafana acessível e configurado
- [ ] Dashboards customizados criados
- [ ] Queries PromQL testadas
- [ ] Alertas configurados

## 🔗 Recursos Úteis

- [Kepler Docs](https://sustainable-computing-io.github.io/kepler/)
- [Prometheus Docs](https://prometheus.io/docs/)
- [Grafana Docs](https://grafana.com/docs/)
- [PromQL Guide](https://prometheus.io/docs/prometheus/latest/querying/basics/)

---

**Documento atualizado**: Dezembro 2025
**Versão**: 1.0

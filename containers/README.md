# Kepler + Prometheus + Grafana Stack

Stack de monitoramento de consumo de energia em containers Kubernetes usando **Kepler** (energy exporter), **Prometheus** (time-series database) e **Grafana** (visualização).

## Pré-requisitos

### Obrigatório
- **Docker**: 20.10+
- **Kind** (Kubernetes in Docker): 0.20+
- **Kubectl**: 1.28+
- **Helm**: 3.12+

### Instalação rápida de ferramentas (Linux/macOS)

```bash
# Kind
curl -Lo ./kind https://kind.sigs.k8s.io/dl/v0.27.0/kind-linux-amd64
chmod +x ./kind
sudo mv ./kind /usr/local/bin/kind

# Helm
curl https://raw.githubusercontent.com/helm/helm/main/scripts/get-helm-3 | bash

# Kubectl
curl -LO "https://dl.k8s.io/release/$(curl -L -s https://dl.k8s.io/release/stable.txt)/bin/linux/amd64/kubectl"
chmod +x kubectl
sudo mv kubectl /usr/local/bin/

# Docker
# https://docs.docker.com/engine/install/
```

### Repositórios Helm

```bash
# Adicionar repositórios Helm necessários
helm repo add prometheus-community https://prometheus-community.github.io/helm-charts
helm repo add grafana https://grafana.github.io/helm-charts
helm repo add kepler https://sustainable-computing-io.github.io/kepler-helm-chart
helm repo update
```

## Instalação Passo a Passo

### 1. Criar Cluster Kind

```bash
# Deletar clusters existentes (opcional)
kind delete cluster --name kepler-cluster 2>/dev/null || true

# Criar novo cluster
kind create cluster --name kepler-cluster

# Verificar contexto kubectl
kubectl config current-context
# Esperado: kind-kepler-cluster
```

### 2. Criar Namespaces

```bash
# Criar namespaces para organizar componentes
kubectl create namespace kepler
kubectl create namespace monitoring
```

### 3. Instalar Prometheus Operator CRDs

Necessário para o Kepler registrar ServiceMonitor:

```bash
kubectl apply -f https://raw.githubusercontent.com/prometheus-operator/prometheus-operator/main/example/prometheus-operator-crd/monitoring.coreos.com_servicemonitors.yaml
```

### 4. Configurar e Instalar Kepler

Criar arquivo de valores customizados:

```bash
cat > /tmp/kepler-values.yaml << 'EOF'
extraEnvVars:
  KEPLER_LOG_LEVEL: "5"
  ENABLE_GPU: "false"
  ENABLE_QAT: "false"
  ENABLE_EBPF_CGROUPID: "true"
  EXPOSE_HW_COUNTER_METRICS: "true"
  EXPOSE_IRQ_COUNTER_METRICS: "true"
  EXPOSE_CGROUP_METRICS: "true"
  ENABLE_PROCESS_METRICS: "true"
  EXPOSE_BPF_METRICS: "true"
  EXPOSE_COMPONENT_POWER: "true"
  RAPL_ENABLED: "true"
  EXPOSE_RAPL_METRICS: "true"

serviceMonitor:
  enabled: true
  namespace: monitoring
  interval: 30s
  scrapeTimeout: 10s

startupProbe:
  enabled: true
  failureThreshold: 15
  periodSeconds: 10

livenessProbe:
  enabled: true
  periodSeconds: 60

readinessProbe:
  enabled: true
  periodSeconds: 10

resources:
  limits:
    memory: 1Gi
    cpu: 2000m
  requests:
    memory: 512Mi
    cpu: 500m

hostNetwork: true
securityContext:
  privileged: true

EOF
```

Instalar Kepler:

```bash
helm install kepler kepler/kepler -n kepler -f /tmp/kepler-values.yaml
```

Aguardar pod estar pronto:

```bash
kubectl rollout status daemonset kepler -n kepler --timeout=2m
```

### 5. Configurar e Instalar Prometheus

Criar arquivo de valores:

```bash
cat > /tmp/prom-values.yaml << 'EOF'
server:
  persistentVolume:
    enabled: false

extraScrapeConfigs: |-
  - job_name: 'kepler'
    scrape_interval: 30s
    scrape_timeout: 10s
    static_configs:
      - targets: ['kepler.kepler:9102']

alertmanager:
  enabled: false
prometheus-pushgateway:
  enabled: false
prometheus-node-exporter:
  enabled: false
kubeStateMetrics:
  enabled: false
EOF
```

Instalar Prometheus:

```bash
helm install prometheus prometheus-community/prometheus -n monitoring -f /tmp/prom-values.yaml
```

Aguardar pod estar pronto:

```bash
kubectl rollout status deployment prometheus-server -n monitoring --timeout=2m
```

### 6. Configurar e Instalar Grafana

Copiar dashboard do Kepler (fornecido pelo usuário) para ConfigMap:

```bash
# Copiar o arquivo Kepler-Exporter.json para /tmp (se não existir)
# Assumindo que está em /tmp/Kepler-Exporter.json

kubectl create configmap kepler-dashboard \
  -n monitoring \
  --from-file=/tmp/Kepler-Exporter.json
```

Criar arquivo de valores do Grafana:

```bash
cat > /tmp/grafana-values.yaml << 'EOF'
adminPassword: admin123

service:
  type: ClusterIP

datasources:
  datasources.yaml:
    apiVersion: 1
    datasources:
      - name: Prometheus-Kepler
        type: prometheus
        url: http://prometheus-server.monitoring.svc.cluster.local
        access: proxy
        isDefault: true

dashboardProviders:
  dashboardproviders.yaml:
    apiVersion: 1
    providers:
      - name: 'kepler'
        orgId: 1
        folder: ''
        type: file
        disableDeletion: false
        editable: true
        options:
          path: /var/lib/grafana/dashboards/kepler

dashboardsConfigMaps:
  kepler: kepler-dashboard
EOF
```

Instalar Grafana:

```bash
helm install grafana grafana/grafana -n monitoring -f /tmp/grafana-values.yaml
```

Aguardar pod estar pronto:

```bash
kubectl rollout status deployment grafana -n monitoring --timeout=2m
```

## Validação e Acesso

### 1. Verificar Status dos Pods

```bash
# Kepler
kubectl get pods -n kepler
# Esperado: kepler-xxxxx (1/1 Running)

# Prometheus e Grafana
kubectl get pods -n monitoring
# Esperado: prometheus-server-xxxxx (2/2 Running)
#          grafana-xxxxx (1/1 Running)
```

### 2. Verificar Métricas do Kepler

```bash
# Testar endpoint de métricas
kubectl exec -n kepler kepler-xxxxx -- curl -s http://localhost:9102/metrics | head -20
```

### 3. Verificar Targets no Prometheus

```bash
# Prometheus deve estar scrapeando Kepler
curl -s 'http://localhost:9091/api/v1/targets' | jq '.data.activeTargets[] | select(.labels.job=="kepler") | {job: .labels.job, health: .health}'
# Esperado: health: "up"
```

### 4. Acessar Interfaces

**Port-forwards (abrir em novos terminais):**

```bash
# Terminal 1 - Grafana (http://localhost:3000)
kubectl port-forward -n monitoring svc/grafana 3000:80

# Terminal 2 - Prometheus (http://localhost:9091)
kubectl port-forward -n monitoring svc/prometheus-server 9091:80
```

**Credenciais:**
- **Grafana**: admin / admin123
- **Prometheus**: sem autenticação

**Dashboard do Kepler**: Automaticamente importado ao acessar Grafana (via ConfigMap)

## Próximos Passos: Deploy de Containers

Após validar que o stack está coletando métricas, você pode:

### 1. Criar Containers de Teste

```bash
# Exemplo: Deploy de aplicação teste
kubectl create deployment nginx-test --image=nginx -n default
kubectl scale deployment nginx-test --replicas=3 -n default

# Aplicar carga para gerar consumo de energia
kubectl run -it --rm load-generator --image=busybox -- /bin/sh -c "while sleep 0.01; do wget -q -O- http://nginx-test:80 > /dev/null; done"
```

### 2. Monitorar no Grafana

- Acessar http://localhost:3000
- Navegar para o dashboard "Kepler Exporter"
- Buscar por métricas de containers específicos:
  - `kepler_container_core_joules_total`: Energia de core consumida
  - `kepler_container_dram_joules_total`: Energia de DRAM consumida
  - `kepler_container_uncore_joules_total`: Energia de uncore consumida

### 3. Queries Úteis no Prometheus

```promql
# Consumo total de energia por namespace
sum by (container_namespace) (rate(kepler_container_core_joules_total[5m]))

# Top 10 containers por consumo
topk(10, rate(kepler_container_core_joules_total[5m]))

# Consumo de um container específico
kepler_container_core_joules_total{pod_name="nginx-test-xxxxx"}
```

## Troubleshooting

### Kepler não está pronto

```bash
# Ver logs
kubectl logs -n kepler -l app.kubernetes.io/name=kepler --tail=100

# Verificar probes
kubectl describe pod -n kepler kepler-xxxxx | grep -A 10 Events
```

### Prometheus não está scrapeando Kepler

```bash
# Verificar configuração
kubectl get configmap -n monitoring prometheus-server-serverfiles-conf -o yaml | grep -A 20 kepler

# Verificar targets
curl -s http://localhost:9091/api/v1/targets | jq '.data.activeTargets'
```

### Grafana sem acesso ao Prometheus

```bash
# Testar conectividade dentro do Grafana pod
kubectl exec -n monitoring grafana-xxxxx -- curl -s http://prometheus-server.monitoring.svc.cluster.local/api/v1/query?query=up

# Verificar datasource
kubectl logs -n monitoring grafana-xxxxx | grep -i prometheus
```

## Limpeza

```bash
# Deletar cluster Kind (isso remove tudo)
kind delete cluster --name kepler-cluster

# Ou deletar apenas namespaces
kubectl delete namespace kepler monitoring
```

## Limitações Atuais (Kind vs Bare-metal)

- ✅ Cgroup metrics: coleta de energia por container
- ✅ Component power: estimativa de poder dinâmico
- ❌ Hardware counters: requer bare-metal
- ❌ eBPF metrics: requer privilégios de kernel completos

Para acesso a métricas completas (HW counters, eBPF), considere implementar em:
- Cluster Kubernetes em bare-metal
- Cluster gerenciado (AWS EKS, GKE, AKS)
- Minikube com driver none: `minikube start --driver=none`

## Referências

- [Kepler Documentation](https://sustainable-computing-io.github.io/kepler/)
- [Prometheus Documentation](https://prometheus.io/docs/)
- [Grafana Documentation](https://grafana.com/docs/)
- [Kind Documentation](https://kind.sigs.k8s.io/)

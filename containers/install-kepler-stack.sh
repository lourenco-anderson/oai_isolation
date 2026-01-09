#!/bin/bash
# Script para instalar Kepler + Prometheus + Grafana com todas as dependências
# Uso: ./install-kepler-stack.sh

set -e

echo "=========================================="
echo "Instalando Kepler + Prometheus + Grafana"
echo "=========================================="

# Cores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
  echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
  echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
  echo -e "${RED}[ERROR]${NC} $1"
}

# 1. Criar namespaces
log_info "Criando namespaces..."
kubectl create namespace kepler --dry-run=client -o yaml | kubectl apply -f -
kubectl create namespace monitoring --dry-run=client -o yaml | kubectl apply -f -

# 2. Instalar Prometheus Operator CRDs
log_info "Instalando Prometheus Operator CRDs..."
kubectl apply -f https://raw.githubusercontent.com/prometheus-operator/prometheus-operator/main/example/prometheus-operator-crd/monitoring.coreos.com_servicemonitors.yaml
kubectl apply -f https://raw.githubusercontent.com/prometheus-operator/prometheus-operator/main/example/prometheus-operator-crd/monitoring.coreos.com_prometheusrules.yaml

# Verificar CRDs
log_info "Verificando CRDs instalados..."
if kubectl get crd servicemonitors.monitoring.coreos.com &>/dev/null; then
  log_info "✓ ServiceMonitor CRD encontrado"
else
  log_error "ServiceMonitor CRD não encontrado!"
  exit 1
fi

# 3. Criar Kepler values
log_info "Criando valores do Kepler..."
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
  interval: 5s
  scrapeTimeout: 2s

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
hostPID: true
securityContext:
  privileged: true
  capabilities:
    add:
      - SYS_ADMIN
      - SYS_PTRACE
      - SYS_RESOURCE
      - NET_ADMIN
      - PERFMON

volumes:
  - name: lib-modules
    hostPath:
      path: /lib/modules
  - name: usr-src
    hostPath:
      path: /usr/src
  - name: sys
    hostPath:
      path: /sys
  - name: bpf
    hostPath:
      path: /sys/fs/bpf
  - name: debugfs
    hostPath:
      path: /sys/kernel/debug

volumeMounts:
  - name: lib-modules
    mountPath: /lib/modules
    readOnly: true
  - name: usr-src
    mountPath: /usr/src
    readOnly: true
  - name: sys
    mountPath: /host/sys
    readOnly: true
  - name: bpf
    mountPath: /sys/fs/bpf
  - name: debugfs
    mountPath: /sys/kernel/debug
EOF

# 4. Adicionar repositórios Helm
log_info "Adicionando repositórios Helm..."
helm repo add kepler https://sustainable-computing-io.github.io/kepler-helm-chart
helm repo add prometheus-community https://prometheus-community.github.io/helm-charts
helm repo add grafana https://grafana.github.io/helm-charts
helm repo update

# 5. Instalar Kepler
log_info "Instalando Kepler..."
helm upgrade --install kepler kepler/kepler -n kepler -f /tmp/kepler-values.yaml

log_info "Aguardando Kepler estar pronto..."
kubectl rollout status daemonset kepler -n kepler --timeout=3m || {
  log_error "Kepler falhou ao iniciar!"
  kubectl logs -n kepler -l app.kubernetes.io/name=kepler --tail=20
  exit 1
}

# 6. Criar Prometheus values
log_info "Criando valores do Prometheus..."
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

# 7. Instalar Prometheus
log_info "Instalando Prometheus..."
helm upgrade --install prometheus prometheus-community/prometheus -n monitoring -f /tmp/prom-values.yaml

log_info "Aguardando Prometheus estar pronto..."
kubectl rollout status deployment prometheus-server -n monitoring --timeout=3m || {
  log_error "Prometheus falhou ao iniciar!"
  kubectl logs -n monitoring -l app.kubernetes.io/name=prometheus,component=server --tail=20
  exit 1
}

# 8. Criar Grafana values
log_info "Criando valores do Grafana..."
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

# 9. Criar ConfigMap para dashboard (se existir)
if [ -f /tmp/Kepler-Exporter.json ]; then
  log_info "Importando dashboard do Kepler..."
  kubectl create configmap kepler-dashboard -n monitoring \
    --from-file=/tmp/Kepler-Exporter.json --dry-run=client -o yaml | kubectl apply -f -
else
  log_warn "Dashboard não encontrado em /tmp/Kepler-Exporter.json, criando ConfigMap vazio"
  kubectl create configmap kepler-dashboard -n monitoring --dry-run=client -o yaml | kubectl apply -f -
fi

# 10. Instalar Grafana
log_info "Instalando Grafana..."
helm upgrade --install grafana grafana/grafana -n monitoring -f /tmp/grafana-values.yaml

log_info "Aguardando Grafana estar pronto..."
kubectl rollout status deployment grafana -n monitoring --timeout=3m || {
  log_error "Grafana falhou ao iniciar!"
  kubectl logs -n monitoring -l app.kubernetes.io/name=grafana --tail=20
  exit 1
}

# 11. Resumo final
echo ""
log_info "=========================================="
log_info "✓ Instalação concluída com sucesso!"
log_info "=========================================="
echo ""

log_info "Pods em execução:"
kubectl get pods -n kepler -o wide
echo ""
kubectl get pods -n monitoring -o wide

echo ""
log_info "Para acessar:"
echo "  Grafana:    http://localhost:3000 (admin/admin123)"
echo "  Prometheus: http://localhost:9091"
echo ""
log_warn "Execute em terminais separados:"
echo "  kubectl port-forward -n monitoring svc/grafana 3000:80"
echo "  kubectl port-forward -n monitoring svc/prometheus-server 9091:80"

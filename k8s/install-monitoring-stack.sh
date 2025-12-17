#!/bin/bash

# Script para instalar Kepler + Prometheus + Grafana
# Uso: ./install-monitoring-stack.sh

set -e

# Cores
BLUE='\033[0;34m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${BLUE}"
echo "╔════════════════════════════════════════════════════════════╗"
echo "║   Monitoring Stack Installation - Kepler + Prometheus      ║"
echo "║                                                            ║"
echo "║   Este script instala:                                    ║"
echo "║   1. Kepler (coleta de energia)                           ║"
echo "║   2. Prometheus (time-series DB)                          ║"
echo "║   3. Grafana (visualização)                               ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo -e "${NC}"
echo ""

# Verificar pré-requisitos
echo -e "${YELLOW}[1/5] Verificando pré-requisitos...${NC}"

if ! command -v kubectl &> /dev/null; then
    echo -e "${RED}✗ kubectl não encontrado${NC}"
    exit 1
fi
echo -e "${GREEN}✓ kubectl${NC}"

if ! command -v helm &> /dev/null; then
    echo -e "${RED}✗ helm não encontrado${NC}"
    echo "  Instalando helm..."
    curl https://raw.githubusercontent.com/helm/helm/main/scripts/get-helm-3 | bash
fi
echo -e "${GREEN}✓ helm${NC}"

# Verificar cluster
echo -e "${YELLOW}Verificando conexão ao cluster...${NC}"
if ! kubectl cluster-info &> /dev/null; then
    echo -e "${RED}✗ Não conseguir conectar ao cluster${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Cluster acessível${NC}"

echo ""

# Instalar Kepler
echo -e "${YELLOW}[2/5] Instalando Kepler...${NC}"

helm repo add kepler https://sustainable-computing-io.github.io/kepler-helm-chart
helm repo update

kubectl create namespace kepler --dry-run=client -o yaml | kubectl apply -f -

helm upgrade --install kepler kepler/kepler \
  --namespace kepler \
  --set serviceAccount.create=true \
  --set rbac.create=true \
  --set daemonset.hostNetwork=true \
  --wait

echo -e "${GREEN}✓ Kepler instalado${NC}"
echo ""

# Aguardar Kepler estar pronto
echo -e "${YELLOW}Aguardando Kepler ficar pronto...${NC}"
kubectl wait --for=condition=ready pod \
  -l app.kubernetes.io/name=kepler \
  -n kepler \
  --timeout=120s || echo "Timeout aguardando Kepler"

echo ""

# Instalar Prometheus + Grafana
echo -e "${YELLOW}[3/5] Instalando Prometheus + Grafana Stack...${NC}"

helm repo add prometheus-community https://prometheus-community.github.io/helm-charts
helm repo update

kubectl create namespace prometheus --dry-run=client -o yaml | kubectl apply -f -

# Criar values file para Prometheus
cat > /tmp/prometheus-values.yaml <<EOF
prometheus:
  prometheusSpec:
    additionalScrapeConfigs:
    - job_name: 'kepler'
      static_configs:
      - targets: ['kepler.kepler.svc.cluster.local:8888']
      scrape_interval: 30s
      scrape_timeout: 10s
    
    retention: 15d
    storageSpec:
      volumeClaimTemplate:
        spec:
          accessModes: ["ReadWriteOnce"]
          resources:
            requests:
              storage: 10Gi

grafana:
  adminPassword: "grafana"
  ingress:
    enabled: false

alertmanager:
  enabled: false
EOF

helm upgrade --install prometheus prometheus-community/kube-prometheus-stack \
  --namespace prometheus \
  -f /tmp/prometheus-values.yaml \
  --wait

echo -e "${GREEN}✓ Prometheus + Grafana instalado${NC}"
echo ""

# Aguardar stack estar pronto
echo -e "${YELLOW}Aguardando stack ficar pronto...${NC}"
kubectl wait --for=condition=ready pod \
  -l app.kubernetes.io/name=prometheus \
  -n prometheus \
  --timeout=120s || echo "Timeout aguardando Prometheus"

kubectl wait --for=condition=ready pod \
  -l app.kubernetes.io/name=grafana \
  -n prometheus \
  --timeout=120s || echo "Timeout aguardando Grafana"

echo ""

# Informações de acesso
echo -e "${YELLOW}[4/5] Configurando port forwards...${NC}"

# Criar script de port forwards
cat > /tmp/port-forwards.sh <<'SCRIPT'
#!/bin/bash

# Port forwards para stack de monitoramento
kubectl port-forward -n prometheus svc/prometheus-grafana 3000:80 &
GRAFANA_PID=$!

kubectl port-forward -n prometheus svc/prometheus-kube-prometheus-prometheus 9090:9090 &
PROMETHEUS_PID=$!

kubectl port-forward -n kepler svc/kepler 8888:8888 &
KEPLER_PID=$!

echo "Port forwards iniciados:"
echo "  - Grafana: http://localhost:3000"
echo "  - Prometheus: http://localhost:9090"
echo "  - Kepler: http://localhost:8888/metrics"
echo ""
echo "PIDs: Grafana=$GRAFANA_PID, Prometheus=$PROMETHEUS_PID, Kepler=$KEPLER_PID"
echo "Para parar: kill $GRAFANA_PID $PROMETHEUS_PID $KEPLER_PID"

wait
SCRIPT

chmod +x /tmp/port-forwards.sh

echo -e "${GREEN}✓ Configuração concluída${NC}"

echo ""

# Resumo final
echo -e "${BLUE}"
echo "╔════════════════════════════════════════════════════════════╗"
echo "║   ✓ Stack de Monitoramento Instalado com Sucesso!         ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo -e "${NC}"
echo ""

# Obter senha do Grafana
GRAFANA_PASSWORD=$(kubectl get secret -n prometheus prometheus-grafana \
  -o jsonpath="{.data.admin-password}" 2>/dev/null | base64 --decode || echo "grafana")

echo -e "${GREEN}Próximas etapas:${NC}"
echo ""
echo "1. Iniciar port forwards:"
echo "   ${YELLOW}bash /tmp/port-forwards.sh${NC}"
echo "   (Ou execute em outro terminal)"
echo ""
echo "2. Acessar Grafana:"
echo "   ${YELLOW}http://localhost:3000${NC}"
echo "   User: admin"
echo "   Password: ${GRAFANA_PASSWORD}"
echo ""
echo "3. Acessar Prometheus:"
echo "   ${YELLOW}http://localhost:9090${NC}"
echo ""
echo "4. Ver métricas do Kepler:"
echo "   ${YELLOW}http://localhost:8888/metrics${NC}"
echo ""
echo -e "${GREEN}Verificar status dos pods:${NC}"
echo ""
echo "   ${YELLOW}kubectl get pods -n kepler${NC}"
echo "   ${YELLOW}kubectl get pods -n prometheus${NC}"
echo ""
echo -e "${GREEN}Ver logs:${NC}"
echo ""
echo "   ${YELLOW}kubectl logs -f -n kepler -l app.kubernetes.io/name=kepler${NC}"
echo "   ${YELLOW}kubectl logs -f -n prometheus -l app=prometheus${NC}"
echo ""
echo -e "${BLUE}Para mais informações, veja k8s/KEPLER_MONITORING.md${NC}"

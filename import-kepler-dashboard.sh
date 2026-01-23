#!/bin/bash

# Script para importar o dashboard Kepler no Grafana
# Requer acesso ao Grafana com credenciais admin

set -e

GRAFANA_URL="${GRAFANA_URL:-http://localhost:3000}"
ADMIN_USER="${ADMIN_USER:-admin}"
ADMIN_PASSWORD="${ADMIN_PASSWORD:-admin123}"
DASHBOARD_JSON="/home/anderson/dev/oai_isolation/k8s-manifests/kepler-energy-dashboard.json"

echo "📊 Importando Dashboard Kepler Energy no Grafana..."
echo "URL: $GRAFANA_URL"

# Função para tentar importar
import_dashboard() {
    curl -s -X POST "$GRAFANA_URL/api/dashboards/db" \
        -H "Content-Type: application/json" \
        -H "Authorization: Bearer $(get_grafana_token)" \
        -d @<(jq '.dashboard = . | {dashboard}' < "$DASHBOARD_JSON") \
        | jq '.id, .uid, .title'
}

# Função para obter token de API
get_grafana_token() {
    curl -s -X POST "$GRAFANA_URL/api/auth/login" \
        -H "Content-Type: application/json" \
        -d "{\"user\":\"$ADMIN_USER\",\"password\":\"$ADMIN_PASSWORD\"}" \
        | jq -r '.token'
}

# Executar importação
if import_dashboard; then
    echo "✅ Dashboard importado com sucesso!"
    echo ""
    echo "📈 Dashboard disponível em:"
    echo "   $GRAFANA_URL/d/kepler-energy-oai"
else
    echo "❌ Erro ao importar dashboard"
    exit 1
fi

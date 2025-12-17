#!/bin/bash

# Script interativo para monitorar energia com Kepler
# Uso: ./energy-queries.sh

set -e

# Cores
BLUE='\033[0;34m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

# URL do Prometheus
PROMETHEUS_URL="${PROMETHEUS_URL:-http://localhost:9090}"

# Função para executar query PromQL
query_prometheus() {
    local query="$1"
    local description="$2"
    
    echo -e "${YELLOW}${description}${NC}"
    echo "Query: ${BLUE}${query}${NC}"
    echo ""
    
    curl -s "${PROMETHEUS_URL}/api/v1/query" \
        --data-urlencode "query=${query}" | jq '.data.result[] | {pod: .metric.pod_name, value: .value[1]}' 2>/dev/null || echo "Erro na query"
    
    echo ""
    echo "---"
    echo ""
}

# Menu interativo
show_menu() {
    clear
    echo -e "${BLUE}"
    echo "╔════════════════════════════════════════════════════════════╗"
    echo "║        Energy Monitoring - Kepler + Prometheus             ║"
    echo "╚════════════════════════════════════════════════════════════╝"
    echo -e "${NC}"
    echo ""
    echo -e "${GREEN}Prometheus: ${PROMETHEUS_URL}${NC}"
    echo ""
    echo "Selecione uma query:"
    echo ""
    echo "  1. Consumo total de energia (todos os pods)"
    echo "  2. Consumo por função (últimas 5m)"
    echo "  3. Top 5 funções mais consumidoras"
    echo "  4. Comparação GNB vs UE"
    echo "  5. Taxa de consumo (Watts) por função"
    echo "  6. Energia CPU vs DRAM"
    echo "  7. Consumo normalizado por CPU"
    echo "  8. Anomalias detectadas"
    echo "  9. Ver todas as métricas disponíveis"
    echo "  10. Query customizada"
    echo "  0. Sair"
    echo ""
    read -p "Escolha: " choice
}

# Queries pré-definidas
case "$1" in
    --all)
        echo -e "${BLUE}Executando todas as queries...${NC}\n"
        
        query_prometheus \
            'sum(kepler_container_energy_total{namespace="oai-isolation"})' \
            "1. Consumo Total de Energia (Joules)"
        
        query_prometheus \
            'sum by (pod_name) (rate(kepler_container_energy_total{namespace="oai-isolation"}[5m]))' \
            "2. Taxa de Consumo por Função (últimas 5m)"
        
        query_prometheus \
            'topk(5, sum by (pod_name) (kepler_container_energy_total{namespace="oai-isolation"}))' \
            "3. Top 5 Funções Mais Consumidoras"
        
        query_prometheus \
            'sum(rate(kepler_container_energy_total{namespace="oai-isolation", pod_name=~"gnb-.*"}[5m])) / sum(rate(kepler_container_energy_total{namespace="oai-isolation", pod_name=~"ue-.*"}[5m]))' \
            "4. Razão GNB/UE"
        
        exit 0
        ;;
    *)
        # Menu interativo
        while true; do
            show_menu
            
            case "$choice" in
                1)
                    query_prometheus \
                        'sum(kepler_container_energy_total{namespace="oai-isolation"})' \
                        "Consumo Total de Energia"
                    ;;
                2)
                    query_prometheus \
                        'sum by (pod_name) (rate(kepler_container_energy_total{namespace="oai-isolation"}[5m]))' \
                        "Consumo por Função (últimas 5m)"
                    ;;
                3)
                    query_prometheus \
                        'topk(5, sum by (pod_name) (kepler_container_energy_total{namespace="oai-isolation"}))' \
                        "Top 5 Funções Mais Consumidoras"
                    ;;
                4)
                    query_prometheus \
                        'sum(rate(kepler_container_energy_total{namespace="oai-isolation", pod_name=~"gnb-.*"}[5m])) as gnb / sum(rate(kepler_container_energy_total{namespace="oai-isolation", pod_name=~"ue-.*"}[5m])) as ue' \
                        "Comparação GNB vs UE"
                    ;;
                5)
                    query_prometheus \
                        'rate(kepler_container_energy_total{namespace="oai-isolation"}[1m]) * 1000' \
                        "Taxa de Consumo - Watts por Função"
                    ;;
                6)
                    query_prometheus \
                        '(kepler_container_cpu_energy_total{namespace="oai-isolation"} + kepler_container_dram_energy_total{namespace="oai-isolation"}) by (pod_name)' \
                        "Energia CPU + DRAM por Função"
                    ;;
                7)
                    query_prometheus \
                        'sum by (pod_name) (kepler_container_energy_total{namespace="oai-isolation"}) / (sum by (pod_name) (container_cpu_usage_seconds_total{namespace="oai-isolation"}) + 0.001)' \
                        "Consumo Normalizado por CPU (J/s)"
                    ;;
                8)
                    query_prometheus \
                        'abs(rate(kepler_container_energy_total{namespace="oai-isolation"}[5m]) - avg_over_time(rate(kepler_container_energy_total{namespace="oai-isolation"}[5m])[1h:5m])) > 2 * stddev_over_time(rate(kepler_container_energy_total{namespace="oai-isolation"}[5m])[1h:5m])' \
                        "Anomalias Detectadas (desvio > 2σ)"
                    ;;
                9)
                    echo -e "${YELLOW}Métricas Disponíveis:${NC}"
                    echo ""
                    curl -s "${PROMETHEUS_URL}/api/v1/label/__name__/values" | jq '.data[] | select(. | contains("kepler") or contains("container")) | @json' | head -20
                    echo ""
                    ;;
                10)
                    echo ""
                    read -p "Enter PromQL query: " custom_query
                    query_prometheus "$custom_query" "Query Customizada"
                    ;;
                0)
                    echo -e "${GREEN}Saindo...${NC}"
                    exit 0
                    ;;
                *)
                    echo -e "${RED}Opção inválida${NC}"
                    ;;
            esac
            
            read -p "Pressione ENTER para continuar..."
        done
        ;;
esac

#!/bin/bash

# Script para monitorar status dos deployments
# Uso: ./monitor.sh [namespace] [watch-interval]

set -e

NAMESPACE=${1:-oai-isolation}
WATCH_INTERVAL=${2:-5}

# Cores para output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${YELLOW}=========================================="
echo "OAI Isolation Deployment Monitor"
echo "==========================================${NC}"
echo "Namespace: $NAMESPACE"
echo "Update Interval: ${WATCH_INTERVAL}s"
echo ""
echo "Press Ctrl+C to exit"
echo ""

while true; do
    clear
    echo -e "${YELLOW}=== OAI Isolation Deployment Status ===${NC}"
    echo "Last updated: $(date '+%Y-%m-%d %H:%M:%S')"
    echo ""
    
    # Mostrar deployments
    echo -e "${GREEN}Deployments:${NC}"
    kubectl get deployments -n "$NAMESPACE" --no-headers | while read line; do
        echo "  $line"
    done
    
    echo ""
    echo -e "${GREEN}Pods:${NC}"
    kubectl get pods -n "$NAMESPACE" --no-headers | while read line; do
        status=$(echo "$line" | awk '{print $3}')
        if [[ "$status" == "Running" ]]; then
            echo -e "  ${GREEN}✓${NC} $line"
        elif [[ "$status" == "Pending" ]]; then
            echo -e "  ${YELLOW}⏳${NC} $line"
        else
            echo -e "  ${RED}✗${NC} $line"
        fi
    done
    
    echo ""
    echo -e "${GREEN}Services:${NC}"
    kubectl get svc -n "$NAMESPACE" --no-headers | while read line; do
        echo "  $line"
    done
    
    echo ""
    echo -e "${GREEN}Resource Usage (Requests):${NC}"
    kubectl describe nodes 2>/dev/null | grep -A 5 "Allocated resources" | head -8 || echo "  Information not available"
    
    sleep "$WATCH_INTERVAL"
done

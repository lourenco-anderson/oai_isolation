#!/bin/bash

# Script para monitorar saúde dos pods OAI UE em tempo real

echo "=== OAI UE Pods Health Monitor ==="
echo ""

while true; do
    clear
    echo "=== OAI UE Pods Status ($(date)) ==="
    echo ""
    
    # Status dos pods
    kubectl get pods -l component=ue -o wide
    
    echo ""
    echo "=== Pod Details ==="
    
    # Contar pods por estado
    total=$(kubectl get pods -l component=ue --no-headers | wc -l)
    running=$(kubectl get pods -l component=ue --no-headers | grep Running | wc -l)
    
    echo "Total Pods: $total"
    echo "Running: $running"
    
    echo ""
    echo "=== Resource Usage ==="
    kubectl top pods -l component=ue 2>/dev/null || echo "Metrics not available yet"
    
    echo ""
    echo "Press Ctrl+C to exit | Refreshing every 10 seconds..."
    sleep 10
done

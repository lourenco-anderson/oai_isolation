#!/bin/bash

# Script para validar coleta de métricas Kepler
# Verifica: Kepler status, métricas no Prometheus, ServiceMonitor config

set -e

echo "🔍 Validação de Métricas Kepler"
echo "================================"
echo ""

# 1. Verificar se Kepler está rodando
echo "1️⃣  Verificando status do Kepler..."
if kubectl get pod -n kepler kepler-9bg99 &>/dev/null; then
    STATUS=$(kubectl get pod -n kepler kepler-9bg99 -o jsonpath='{.status.phase}')
    echo "   ✅ Kepler rodando: $STATUS"
    
    # Logs do Kepler
    echo "   Últimas linhas de log:"
    kubectl logs -n kepler kepler-9bg99 --tail=5 2>/dev/null | sed 's/^/   /'
else
    echo "   ❌ Kepler não encontrado"
    exit 1
fi

echo ""

# 2. Verificar ServiceMonitor
echo "2️⃣  Verificando ServiceMonitor..."
if kubectl get servicemonitor -n kepler 2>/dev/null | grep -q kepler; then
    echo "   ✅ ServiceMonitor configurado"
    kubectl get servicemonitor -n kepler -o jsonpath='{.items[*].spec.selector.matchLabels}' | sed 's/^/   /'
else
    echo "   ⚠️  Nenhum ServiceMonitor encontrado"
    echo "   Para criar, execute:"
    echo "   kubectl apply -f - <<EOF"
    echo "apiVersion: monitoring.coreos.com/v1"
    echo "kind: ServiceMonitor"
    echo "metadata:"
    echo "  name: kepler"
    echo "  namespace: kepler"
    echo "spec:"
    echo "  selector:"
    echo "    matchLabels:"
    echo "      app: kepler"
    echo "  endpoints:"
    echo "  - port: metrics"
    echo "    interval: 1s"
    echo "EOF"
    echo ""
fi

echo ""

# 3. Verificar se Prometheus está scrapeando Kepler
echo "3️⃣  Verificando targets no Prometheus..."
{
    # Porta-forward Prometheus em background
    kubectl port-forward -n monitoring svc/prometheus-operated 9090:9090 > /dev/null 2>&1 &
    PF_PID=$!
    sleep 2
    
    # Query Prometheus
    RESPONSE=$(curl -s "http://localhost:9090/api/v1/targets" 2>/dev/null || echo "")
    
    # Limpar port-forward
    kill $PF_PID 2>/dev/null || true
    
    if echo "$RESPONSE" | grep -q kepler; then
        echo "   ✅ Kepler aparece nos targets do Prometheus"
        echo "$RESPONSE" | jq '.data.activeTargets[] | select(.scrapePool | contains("kepler"))' 2>/dev/null | head -20 || echo "   (não conseguiu parsear JSON)"
    else
        echo "   ❌ Kepler NÃO aparece nos targets"
        echo "   Verifique: kubectl logs -n monitoring prometheus-prometheus-0"
    fi
} || true

echo ""

# 4. Verificar se há dados de métricas
echo "4️⃣  Verificando dados de métricas..."
{
    kubectl port-forward -n monitoring svc/prometheus-operated 9090:9090 > /dev/null 2>&1 &
    PF_PID=$!
    sleep 2
    
    # Verificar métrica de watts
    WATTS=$(curl -s "http://localhost:9090/api/v1/query?query=kepler_container_cpu_watts" 2>/dev/null | jq '.data.result | length' 2>/dev/null || echo "0")
    
    # Verificar métrica de joules
    JOULES=$(curl -s "http://localhost:9090/api/v1/query?query=kepler_container_cpu_joules" 2>/dev/null | jq '.data.result | length' 2>/dev/null || echo "0")
    
    kill $PF_PID 2>/dev/null || true
    
    echo "   📊 kepler_container_cpu_watts: $WATTS séries de tempo"
    echo "   📊 kepler_container_cpu_joules: $JOULES séries de tempo"
    
    if [ "$WATTS" -gt 0 ] || [ "$JOULES" -gt 0 ]; then
        echo "   ✅ Métricas sendo coletadas!"
    else
        echo "   ❌ Nenhuma métrica coletada ainda"
        echo "   Aguarde ~30s para primeira coleta"
    fi
} || true

echo ""
echo "✅ Validação completa!"
echo ""
echo "Próximo passo: Importar dashboard em k8s-manifests/kepler-energy-dashboard.json"

#!/bin/bash

echo "🔍 Verificação Completa: Kepler + Prometheus + OAI Pods"
echo "========================================================"
echo ""

echo "1️⃣  STATUS DOS PODS OAI"
echo "----------------------"
kubectl get pods -l component=ue -o wide | awk '{print $1, $3, $5}'
READY=$(kubectl get pods -l component=ue -o jsonpath='{range .items[*]}{.status.conditions[?(@.type=="Ready")].status}{"\n"}{end}' | grep -c True)
echo "✓ $READY/8 pods prontos"
echo ""

echo "2️⃣  STATUS DO KEPLER"
echo "-------------------"
kubectl get pods -n kepler -o wide
KEPLER_STATUS=$(kubectl get pods -n kepler -o jsonpath='{.items[0].status.phase}')
echo "Status: $KEPLER_STATUS"
echo ""

echo "3️⃣  LOGS DO KEPLER (últimas 5 linhas)"
echo "------------------------------------"
kubectl logs -n kepler -l app.kubernetes.io/name=kepler --tail=5
echo ""

echo "4️⃣  SERVICE MONITOR"
echo "------------------"
kubectl get servicemonitor -n kepler
echo ""

echo "5️⃣  PROMETHEUS STATUS"
echo "--------------------"
kubectl get pods -n monitoring -o wide | grep prometheus-server
echo ""

echo "6️⃣  TESTANDO ACESSO A MÉTRICAS"
echo "-----------------------------"

# Port forward Prometheus
kubectl port-forward -n monitoring svc/prometheus-server 9090:80 > /dev/null 2>&1 &
PF_PID=$!
sleep 3

echo "A) Métricas de cAdvisor disponíveis:"
CADVISOR_METRICS=$(curl -s 'http://localhost:9090/api/v1/query?query=container_memory_working_set_bytes' | jq '.data.result | length')
echo "   ✓ $CADVISOR_METRICS séries de memory encontradas"

echo ""
echo "B) Métricas do Kepler disponíveis:"
KEPLER_METRICS=$(curl -s 'http://localhost:9090/api/v1/query?query=kepler_container_joules_total' | jq '.data.result | length')
if [ "$KEPLER_METRICS" -gt 0 ]; then
  echo "   ✅ $KEPLER_METRICS séries de Kepler encontradas!"
else
  echo "   ⚠️  0 séries do Kepler (Kepler ainda não está enviando métricas)"
  echo "       Isso é esperado em primeira execução"
fi

# Limpar port forward
kill $PF_PID 2>/dev/null
wait $PF_PID 2>/dev/null

echo ""
echo "7️⃣  PRÓXIMOS PASSOS"
echo "------------------"
echo ""
echo "Option A: Usar métricas de cAdvisor (AGORA)"
echo "  $ kubectl port-forward -n monitoring svc/grafana 3000:80"
echo "  Abrir: http://localhost:3000"
echo "  Query: sum by (pod) (container_memory_working_set_bytes{namespace=\"default\"}) / 1024 / 1024"
echo ""
echo "Option B: Aguardar Kepler (para energia)"
echo "  Ver: KEPLER_INSTALLATION_STATUS.md"
echo ""
echo "========================================================"

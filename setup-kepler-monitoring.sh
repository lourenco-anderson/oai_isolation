#!/bin/bash
# Script completo para configurar Kepler + Prometheus + Dashboard

echo "🚀 Configuração Kepler Energy Monitoring"
echo "========================================"
echo ""

# 1. Verificar Kepler
echo "1️⃣ Verificando Kepler..."
if kubectl get pods -n kepler -l "app.kubernetes.io/name=kepler" | grep -q Running; then
    echo "   ✅ Kepler rodando"
else
    echo "   ❌ Kepler não encontrado"
    exit 1
fi

# 2. Verificar Service Kepler
echo ""
echo "2️⃣ Verificando Service..."
if kubectl get svc -n kepler kepler &>/dev/null; then
    PORTS=$(kubectl get svc -n kepler kepler -o jsonpath='{.spec.ports[*].port}')
    echo "   ✅ Service kepler expondo portas: $PORTS"
else
    echo "   ⚠️  Service não encontrado, criando..."
    kubectl apply -f k8s-manifests/kepler-servicemonitor.yaml
fi

# 3. Testar endpoint Kepler
echo ""
echo "3️⃣ Testando endpoint Kepler..."
kubectl port-forward -n kepler svc/kepler 8888:8888 > /dev/null 2>&1 &
PF_PID=$!
sleep 3

if curl -s http://localhost:8888/metrics | grep -q "kepler_container"; then
    echo "   ✅ Métricas disponíveis no endpoint"
    METRICS_COUNT=$(curl -s http://localhost:8888/metrics | grep "^kepler_container_cpu" | wc -l)
    echo "   📊 $METRICS_COUNT linhas de métricas CPU"
else
    echo "   ❌ Endpoint não responde corretamente"
fi

kill $PF_PID 2>/dev/null

# 4. Verificar ConfigMap Prometheus
echo ""
echo "4️⃣ Verificando job Prometheus..."
if kubectl get configmap prometheus-server -n monitoring -o yaml | grep -q "job_name.*kepler"; then
    TARGET=$(kubectl get configmap prometheus-server -n monitoring -o yaml | grep -A 5 "job_name.*kepler" | grep targets | awk '{print $3}')
    echo "   ✅ Job kepler configurado: $TARGET"
else
    echo "   ❌ Job kepler não encontrado no ConfigMap"
fi

# 5. Dashboard
echo ""
echo "5️⃣ Dashboard Grafana..."
if [ -f "k8s-manifests/kepler-energy-dashboard.json" ]; then
    echo "   ✅ Dashboard pronto: k8s-manifests/kepler-energy-dashboard.json"
    echo ""
    echo "📊 Para importar no Grafana:"
    echo "   1. Acessar: minikube service grafana -n monitoring --url"
    echo "   2. Login: admin / prom-operator"
    echo "   3. Create → Import → Colar conteúdo do JSON"
else
    echo "   ❌ Dashboard não encontrado"
fi

echo ""
echo "✅ Setup completo!"
echo ""
echo "📌 Próximos passos:"
echo "   - Aguardar ~30s para coleta inicial de métricas"
echo "   - Importar dashboard no Grafana"
echo "   - Verificar painéis em tempo real (refresh: 1s)"

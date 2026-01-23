# Instalação e Configuração do Kepler + Grafana

## Status Atual

✅ Prometheus está rodando e coletando métricas  
✅ Grafana está rodando  
❌ Kepler não está instalado  
❌ Pods OAI não têm configuração de monitoramento de energia  

---

## Passo 1: Instalar Kepler

Kepler é um exporter de energia para Kubernetes. Funciona melhor em bare metal, mas pode funcionar em VMs.

### Opção A: Instalação via Helm (Recomendado)

```bash
# Adicionar repo Kepler
helm repo add kepler https://sustainable-computing.io/kepler-helm-repo
helm repo update

# Instalar Kepler
helm install kepler kepler/kepler \
  -n kepler \
  --create-namespace \
  --set serviceMonitor.enabled=true

# Verificar se instalou
kubectl get pods -n kepler
```

### Opção B: Instalação via YAML

```bash
# Clonar repo Kepler
git clone https://github.com/sustainable-computing-io/kepler.git
cd kepler

# Instalar
kubectl apply -f manifests/kepler.yaml

# Verificar
kubectl get pods -n kepler
```

### Troubleshooting de Instalação

Se Kepler não inicia (common em minikube):

```bash
# Ver logs
kubectl logs -n kepler deployment/kepler

# Problema comum: Kepler precisa de acesso a hardware (Intel RAPL)
# Solução: Usar modo mock
kubectl patch deployment kepler -n kepler -p '{"spec":{"template":{"spec":{"containers":[{"name":"kepler","env":[{"name":"KEPLER_MOCK_MODE","value":"true"}]}]}}}}'
```

---

## Passo 2: Configurar Prometheus para Scrape Kepler

### Verificar ServiceMonitor

```bash
# Se Kepler foi instalado com Helm
kubectl get servicemonitor -n kepler

# Se não existir, criar manualmente
kubectl apply -f - <<EOF
apiVersion: monitoring.coreos.com/v1
kind: ServiceMonitor
metadata:
  name: kepler
  namespace: kepler
spec:
  selector:
    matchLabels:
      app.kubernetes.io/name: kepler
  endpoints:
  - port: metrics
    interval: 30s
EOF
```

### Verificar Prometheus está detectando Kepler

```bash
# Port forward Prometheus
kubectl port-forward -n monitoring svc/prometheus-server 9090:9090 &

# Abrir http://localhost:9090
# Ir em Status → Targets
# Procurar por "kepler" nos targets
```

---

## Passo 3: Configurar Pods para Monitoramento

### Adicionar Labels nos Pods OAI

Editar `k8s-manifests/ue-deployments.yaml` para cada deployment:

```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: ue-ch-est
spec:
  template:
    metadata:
      labels:
        app: ue-ch-est
        component: ue
        monitored: "true"          # ← Adicione isto
        prometheus.io/scrape: "true" # ← E isto (se aplicável)
    spec:
      containers:
      - name: ue-ch-est
        image: oai-ue-ch_est:latest
        imagePullPolicy: Never
```

Aplicar mudanças:

```bash
kubectl apply -f k8s-manifests/ue-deployments.yaml
```

---

## Passo 4: Verificar Coleta de Dados

### Query Teste no Prometheus

```bash
# Port forward Prometheus
kubectl port-forward -n monitoring svc/prometheus-server 9090:9090 &

# Acesse http://localhost:9090

# Execute estas queries (Graph tab):
```

#### Query 1: Verificar dados do Kepler
```promql
kepler_container_joules_total
```

#### Query 2: Verificar dados de Memory
```promql
container_memory_working_set_bytes{namespace="default", pod=~"ue-.*"}
```

#### Query 3: Verificar dados de CPU
```promql
container_cpu_usage_seconds_total{namespace="default", pod=~"ue-.*"}
```

---

## Passo 5: Atualizar Dashboard Grafana

### Opção A: Usar Dashboard Kepler Existente (Se Kepler funciona)

O arquivo `Kepler Exporter Dashboard-1767693829895.json` já tem queries prontas:

```bash
# Importar no Grafana:
# 1. Grafana → Dashboards → Import
# 2. Upload JSON file: Kepler Exporter Dashboard-1767693829895.json
# 3. Selecionar datasource Prometheus-Kepler
# 4. Import

# As variáveis devem ser:
# - datasource: Prometheus-Kepler
# - namespace: label_values(kepler_container_joules_total, container_namespace)
# - pod: label_values(kepler_container_joules_total{...}, pod_name)
```

### Opção B: Criar Dashboard Simplificado (Sem Kepler)

Se Kepler não funciona, use métricas do cAdvisor:

```bash
# 1. Criar novo dashboard no Grafana
# 2. Adicionar Panels com estas queries:

# Panel: Memory Usage
sum by (pod) (container_memory_working_set_bytes{namespace="default", pod=~"ue-.*"}) / 1024 / 1024

# Panel: CPU Usage  
sum by (pod) (rate(container_cpu_usage_seconds_total{namespace="default", pod=~"ue-.*"}[1m]))

# Panel: Network IN
sum by (pod) (rate(container_network_receive_bytes_total{namespace="default", pod=~"ue-.*"}[1m]))

# Panel: Network OUT
sum by (pod) (rate(container_network_transmit_bytes_total{namespace="default", pod=~"ue-.*"}[1m]))
```

---

## Passo 6: Troubleshooting

### Problema: Kepler não coleta dados

```bash
# 1. Verificar se está rodando
kubectl get pods -n kepler

# 2. Ver logs
kubectl logs -n kepler deployment/kepler

# 3. Verificar porta (geralmente 9102)
kubectl port-forward -n kepler svc/kepler 9102:9102
curl http://localhost:9102/metrics | head -20

# 4. Se usar mock mode:
kubectl set env deployment/kepler -n kepler KEPLER_MOCK_MODE=true
```

### Problema: Prometheus não scrapeando Kepler

```bash
# 1. Verificar ServiceMonitor
kubectl get servicemonitor -n kepler -o yaml

# 2. Verificar labels do service
kubectl get svc -n kepler -o yaml | grep -A 5 labels

# 3. Restar Prometheus
kubectl rollout restart deployment/prometheus-server -n monitoring
```

### Problema: Dashboard vazio no Grafana

```bash
# 1. Verificar datasource
# Grafana → Configuration → Data Sources
# Prometheus-Kepler deve estar "green" (conectado)

# 2. Testar queries
# Ir em Explore → Escolher datasource
# Pegar uma query do dashboard
# Copiar em Prometheus

# 3. Verificar variáveis
# Settings → Variables
# Cada variável deve retornar valores
```

---

## Script de Setup Completo

```bash
#!/bin/bash
set -e

echo "🚀 Setup Kepler + Grafana para OAI Pods"

# 1. Instalar Kepler
echo "📦 Instalando Kepler..."
helm repo add kepler https://sustainable-computing.io/kepler-helm-repo
helm repo update
helm install kepler kepler/kepler -n kepler --create-namespace 2>/dev/null || \
  helm upgrade kepler kepler/kepler -n kepler

# 2. Aguardar Kepler iniciar
echo "⏳ Aguardando Kepler iniciar..."
kubectl wait --for=condition=ready pod -l app.kubernetes.io/name=kepler -n kepler --timeout=300s

# 3. Criar ServiceMonitor (se não existir)
echo "📡 Criando ServiceMonitor..."
kubectl apply -f - <<EOF
apiVersion: monitoring.coreos.com/v1
kind: ServiceMonitor
metadata:
  name: kepler
  namespace: kepler
spec:
  selector:
    matchLabels:
      app.kubernetes.io/name: kepler
  endpoints:
  - port: metrics
    interval: 30s
EOF

# 4. Restar Prometheus
echo "🔄 Reiniciando Prometheus..."
kubectl rollout restart deployment/prometheus-server -n monitoring
kubectl wait --for=condition=available deployment/prometheus-server -n monitoring --timeout=300s

# 5. Exibir informações
echo "✅ Setup concluído!"
echo ""
echo "📊 Grafana: http://localhost:3000"
echo "📈 Prometheus: http://localhost:9090"
echo ""
echo "Para Port Forward:"
echo "  kubectl port-forward -n monitoring svc/grafana 3000:80"
echo "  kubectl port-forward -n monitoring svc/prometheus-server 9090:80"
echo ""
echo "Para importar dashboard:"
echo "  1. Grafana → Dashboards → Import"
echo "  2. Upload: k8s-manifests/Kepler Exporter Dashboard-1767693829895.json"
echo "  3. Selecionar datasource Prometheus-Kepler"
```

---

## Checklist Final

- [ ] Kepler instalado e rodando
- [ ] ServiceMonitor criado
- [ ] Prometheus scrapeando Kepler
- [ ] Pods OAI com labels corretos
- [ ] Datasource Prometheus no Grafana
- [ ] Dashboard importado
- [ ] Variáveis resolvidas
- [ ] Dados aparecem no dashboard

---

## Próximas Etapas

1. Execute o script de setup
2. Aguarde Kepler iniciar (pode levar alguns minutos)
3. Importe o dashboard Kepler
4. Configure variáveis conforme o guia anterior
5. Valide que dados aparecem

Se ainda não funcionar, veja seção "Troubleshooting".

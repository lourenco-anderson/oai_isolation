# Guia de Configuração: Visualizar Pods OAI no Grafana

## Problema
Os pods `ue-*` não aparecem no dashboard Grafana do Kepler.

## Causas Possíveis

1. **Kepler não está coletando dados** dos pods
2. **Labels incorretos** nos pods ou containers
3. **Variáveis do dashboard** não estão filtradas corretamente
4. **Datasource Prometheus** não está configurado

---

## Solução Passo a Passo

### 1. Verificar se Kepler está rodando

```bash
kubectl get pods -n kepler
```

Se Kepler não estiver instalado:

```bash
helm repo add kepler https://sustainable-computing.io/kepler-helm-repo
helm repo update
helm install kepler kepler/kepler -n kepler --create-namespace
```

### 2. Verificar Labels nos Pods

Os pods precisam ter labels para serem descobertos. Verifique:

```bash
kubectl get pods -l component=ue -o jsonpath='{range .items[*]}{.metadata.name}{"\t"}{.metadata.labels}{"\n"}{end}'
```

Se os labels estão ausentes, adicione-os ao `ue-deployments.yaml`:

```yaml
spec:
  template:
    metadata:
      labels:
        app: ue-ch-est
        component: ue
        monitored: "true"  # Adicione este label
```

### 3. Verificar Dados no Prometheus

Acesse o Prometheus e execute queries:

```bash
# Port forward para Prometheus
kubectl port-forward -n monitoring svc/prometheus-server 9090:9090

# Abra http://localhost:9090
```

Teste estas queries:

```promql
# Ver métricas disponíveis do Kepler
kepler_container_joules_total

# Ver métricas de memoria dos containers
container_memory_working_set_bytes

# Ver pods monitorados
count(container_memory_working_set_bytes) by (pod)
```

### 4. Configurar Variáveis no Grafana

No dashboard Grafana, vá para **Settings > Variables** e configure:

#### Variável: `namespace`
```
Name: namespace
Type: Query
Datasource: Prometheus-Kepler (ou seu datasource Prometheus)
Query: label_values(kepler_container_joules_total, container_namespace)
```

#### Variável: `pod`
```
Name: pod
Type: Query
Datasource: Prometheus-Kepler
Query: label_values(kepler_container_joules_total{container_namespace=~"$namespace"}, pod_name)
```

Se o Kepler não tem dados, use métricas do cAdvisor:

```
Query: label_values(container_memory_working_set_bytes{namespace=~"$namespace"}, pod)
```

### 5. Atualizar Queries do Dashboard

Se as métricas do Kepler não estão disponíveis, substitua as queries:

**Antes (Kepler):**
```promql
sum(irate(kepler_container_package_joules_total{container_namespace=~"$namespace", pod_name=~"$pod"}[1m]))
```

**Depois (cAdvisor):**
```promql
sum(rate(container_memory_working_set_bytes{namespace=~"$namespace", pod=~"$pod"}[1m]))
```

### 6. Adicionar Datasource Prometheus (Se necessário)

Se o Prometheus não está como datasource:

1. Grafana → **Configuration** → **Data Sources**
2. **Add Data Source** → **Prometheus**
3. URL: `http://prometheus-server.monitoring.svc.cluster.local:80`
4. **Save & Test**

---

## Verificações Rápidas

### Verificar Kepler está enviando dados

```bash
kubectl exec -n kepler <kepler-pod-name> -- \
  curl -s http://localhost:9102/metrics | grep kepler_container_joules
```

### Verificar Prometheus está scrapeando Kepler

```bash
kubectl exec -n monitoring prometheus-server-* -c prometheus -- \
  curl -s http://localhost:9090/api/v1/targets | jq '.data.activeTargets[] | select(.job=="kepler")'
```

### Listar namespaces com pods monitorados

```bash
kubectl exec -n monitoring prometheus-server-* -c prometheus -- \
  curl -s 'http://localhost:9090/api/v1/query?query=container_memory_working_set_bytes' | jq '.data.result[0].metric'
```

---

## Script para Verificação Completa

```bash
#!/bin/bash

echo "=== Verificando Pods OAI ==="
kubectl get pods -l component=ue -o wide

echo -e "\n=== Verificando Kepler ==="
kubectl get pods -n kepler

echo -e "\n=== Verificando Prometheus ==="
kubectl get svc -n monitoring | grep prometheus

echo -e "\n=== Verificando Grafana ==="
kubectl get svc -n monitoring | grep grafana

echo -e "\n=== Port Forward para Prometheus ==="
echo "Execute: kubectl port-forward -n monitoring svc/prometheus-server 9090:9090"
echo "Depois acesse: http://localhost:9090"
```

---

## Alternativa: Dashboard Simples com cAdvisor

Se Kepler não está funcionando, crie um dashboard apenas com métricas de container:

```json
{
  "panels": [
    {
      "title": "Memory Usage by Pod",
      "targets": [
        {
          "expr": "sum by (pod) (container_memory_working_set_bytes{namespace=~\"$namespace\"})"
        }
      ]
    },
    {
      "title": "CPU Usage by Pod",
      "targets": [
        {
          "expr": "sum by (pod) (rate(container_cpu_usage_seconds_total{namespace=~\"$namespace\"}[1m]))"
        }
      ]
    }
  ]
}
```

---

## Troubleshooting

### Problema: "No data" no dashboard
- ✅ Verificar se Prometheus está coletando métricas
- ✅ Verificar namespace do pod (pode ser `default` não `monitoring`)
- ✅ Verificar label names (pode ser `pod_name` vs `pod`)

### Problema: Variáveis vazias
- ✅ Verificar datasource está correto
- ✅ Verificar query retorna resultados
- ✅ Testar query direto no Prometheus

### Problema: Kepler não envia dados
- ✅ Verificar pods estão com labels `prometheus.io/scrape: "true"`
- ✅ Verificar porta de métricas (geralmente 8888 ou 9102)
- ✅ Verificar ServiceMonitor está criado

---

## Label Requirements para Kepler

Se usar Kepler, os pods precisam ter:

```yaml
metadata:
  labels:
    app: ue-ch-est
    prometheus.io/scrape: "true"
    prometheus.io/port: "8888"  # ou porta da aplicação
    prometheus.io/path: "/metrics"
```

Ou criar um ServiceMonitor:

```yaml
apiVersion: monitoring.coreos.com/v1
kind: ServiceMonitor
metadata:
  name: ue-services
spec:
  selector:
    matchLabels:
      component: ue
  endpoints:
  - port: metrics
    interval: 30s
```

---

## Próximos Passos

1. **Verifique qual datasource** está configurado no dashboard (veja no arquivo JSON)
2. **Teste queries no Prometheus** antes de adicionar ao dashboard
3. **Configure variáveis** baseado no que o Prometheus retorna
4. **Atualize o dashboard JSON** com as queries corretas

Para mais informações, veja `DOCKER_K8S_BEST_PRACTICES.md`.

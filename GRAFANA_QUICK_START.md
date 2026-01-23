# Como Visualizar Pods OAI no Grafana

## 🚀 Início Rápido

### 1. Port Forward Grafana

```bash
kubectl port-forward -n monitoring svc/grafana 3000:80
```

### 2. Abrir Grafana

```
http://localhost:3000
```

Credenciais padrão:
- **User**: admin
- **Password**: prom-operator (ou check no Helm values)

### 3. Importar Dashboard OAI

1. Grafana → **Dashboards** → **Import**
2. Upload JSON file: `k8s-manifests/OAI-Pods-Dashboard.json`
3. Select datasource: **Prometheus** (ou o que estiver disponível)
4. **Import**

Você verá 3 gráficos:
- Memory Usage by Pod
- CPU Usage by Pod
- Network Traffic by Pod

---

## 📊 Dashboard Disponível

### `OAI-Pods-Dashboard.json`

Dashboard pronto para usar com:

- **Memory Usage**: Uso de memória de cada pod em MB
- **CPU Usage**: Consumo de CPU em cores
- **Network Traffic**: IN/OUT de rede por pod

**Métrica**: cAdvisor (container_memory_working_set_bytes, container_cpu_usage_seconds_total, etc)

**Atualização**: A cada 30 segundos

---

## 🔧 Customizar Dashboard

### Adicionar novo painel

1. Grafana → Dashboard → Edit (lápis no topo)
2. **Add panel** → **Time series**
3. Escrever query:

```promql
sum by (pod) (container_memory_working_set_bytes{namespace="default", pod=~"ue-.*"}) / 1024 / 1024
```

4. Legend: `{{pod}}`
5. Title: "Memory Usage"
6. **Apply** → **Save**

### Queries Disponíveis

```promql
# Memory (MB)
sum by (pod) (container_memory_working_set_bytes{namespace="default", pod=~"ue-.*"}) / 1024 / 1024

# CPU (cores)
sum by (pod) (rate(container_cpu_usage_seconds_total{namespace="default", pod=~"ue-.*"}[1m]))

# Network RX (bytes/s)
sum by (pod) (rate(container_network_receive_bytes_total{namespace="default", pod=~"ue-.*"}[1m]))

# Network TX (bytes/s)
sum by (pod) (rate(container_network_transmit_bytes_total{namespace="default", pod=~"ue-.*"}[1m]))

# Disk I/O (bytes/s)
sum by (pod) (rate(container_fs_io_time_seconds_total{namespace="default", pod=~"ue-.*"}[1m]))

# Filesystem Used (bytes)
sum by (pod) (container_fs_usage_bytes{namespace="default", pod=~"ue-.*"})

# Contar pods ativos
count(container_memory_working_set_bytes{namespace="default", pod=~"ue-.*"})
```

---

## 📈 Quando Kepler Funcionar

Quando o Kepler começar a enviar dados `kepler_container_joules_total`, você terá:

```promql
# Power (watts)
sum(irate(kepler_container_package_joules_total{container_namespace="default", pod_name=~"ue-.*"}[1m]))

# Energy (joules)
sum(increase(kepler_container_joules_total{container_namespace="default", pod_name=~"ue-.*"}[24h:1m]))

# Energy (kWh)
sum(increase(kepler_container_joules_total{container_namespace="default", pod_name=~"ue-.*"}[24h:1m])) * 0.000000277777777777778
```

Nessa hora, importe o dashboard original `Kepler Exporter Dashboard-1767693829895.json`.

---

## 🔍 Troubleshooting

### Dashboard está vazio

**Problema**: Nenhum dado nos gráficos

**Solução**:
1. Verificar se Prometheus tem dados:
   - Grafana → **Explore**
   - Escolher datasource **Prometheus**
   - Query: `container_memory_working_set_bytes{namespace="default"}`
   - Clique em **Run query**

2. Se não retornar dados:
   ```bash
   kubectl port-forward -n monitoring svc/prometheus-server 9090:80 &
   curl 'http://localhost:9090/api/v1/query?query=container_memory_working_set_bytes' | jq .
   ```

### Datasource não aparece

**Solução**:
1. Grafana → **Configuration** → **Data Sources**
2. Se não tiver Prometheus:
   - **Add data source** → **Prometheus**
   - URL: `http://prometheus-server.monitoring.svc.cluster.local:80`
   - **Save & Test**

### Pods não aparecem nas queries

**Causa**: Namespace ou label incorreto

**Verificar**:
```bash
kubectl get pods -l component=ue -o jsonpath='{range .items[*]}{.metadata.namespace}{"\t"}{.metadata.name}{"\n"}{end}'
```

Se não for `default`, altere a query:
```promql
sum by (pod) (container_memory_working_set_bytes{namespace="SEU_NAMESPACE", pod=~"ue-.*"})
```

---

## 📋 Arquivos Necessários

| Arquivo | Descrição | Status |
|---------|-----------|--------|
| `k8s-manifests/OAI-Pods-Dashboard.json` | Dashboard pronto | ✅ Novo |
| `k8s-manifests/Kepler Exporter Dashboard-1767693829895.json` | Dashboard Kepler | ⏳ Para depois |
| `KEPLER_INSTALLATION_STATUS.md` | Status da instalação | ✅ Novo |
| `GRAFANA_SETUP_GUIDE.md` | Guia de setup | ✅ Novo |

---

## 🎯 Próximos Passos

1. **Agora**: Usar OAI-Pods-Dashboard.json com métricas de cAdvisor
2. **Depois**: Debugar Kepler para dados de energia
3. **Quando Kepler funcionar**: Importar Kepler dashboard

---

## 📞 Dúvidas?

Ver documentos:
- `DOCKER_K8S_BEST_PRACTICES.md` - Fundamentos
- `GRAFANA_SETUP_GUIDE.md` - Setup detalhado
- `KEPLER_INSTALLATION_STATUS.md` - Status e troubleshooting

Executar:
```bash
bash check-metrics.sh
```

Isso mostrará status de tudo e o próximo passo.

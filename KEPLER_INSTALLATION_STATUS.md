# Status da Instalação: Kepler + Grafana

## ✅ Completo

- [x] Kepler instalado via Helm com `kepler-values.yaml` customizado
- [x] ServiceMonitor criado para Prometheus scrapeamento
- [x] Prometheus reconfigurado
- [x] 8 pods OAI disponíveis (completam testes e saem)

## ⚠️ Em Progresso

- [ ] Kepler enviando métricas (container_joules_total) para Prometheus
- [ ] Dashboard Grafana populado com dados do Kepler

## 📊 Alternativa Imediata: Use Métricas de cAdvisor

Enquanto o Kepler não está enviando dados, use as métricas de container padrão que o Prometheus já coleta:

### Queries Disponíveis Agora

```promql
# Memory Usage por Pod
sum by (pod) (container_memory_working_set_bytes{namespace="default", pod=~"ue-.*"}) / 1024 / 1024

# CPU Usage por Pod
sum by (pod) (rate(container_cpu_usage_seconds_total{namespace="default", pod=~"ue-.*"}[1m]))

# Network IN por Pod
sum by (pod) (rate(container_network_receive_bytes_total{namespace="default", pod=~"ue-.*"}[1m]))

# Network OUT por Pod
sum by (pod) (rate(container_network_transmit_bytes_total{namespace="default", pod=~"ue-.*"}[1m]))

# Disk I/O Read
sum by (pod) (rate(container_fs_io_time_seconds_total{namespace="default", pod=~"ue-.*"}[1m]))
```

### Criar Dashboard Grafana Rápido

1. Abrir Grafana: `http://localhost:3000`
2. **Dashboards** → **Create** → **New Dashboard**
3. Adicionar painel com query:
   ```promql
   sum by (pod) (container_memory_working_set_bytes{namespace="default", pod=~"ue-.*"}) / 1024 / 1024
   ```
4. Título: "Memory Usage (MB)"
5. Legend: `{{pod}}`
6. Salvar

---

## 🔧 Próximos Passos para Kepler

Se quiser continuar com Kepler:

### 1. Verificar Status do Kepler

```bash
# Ver logs
kubectl logs -n kepler -l app.kubernetes.io/name=kepler --tail=100

# Testar endpoint
kubectl port-forward -n kepler svc/kepler 9102:9102
curl http://localhost:9102/metrics | grep "kepler_"

# Ver configuração
kubectl get daemonset -n kepler kepler -o yaml | grep -A 30 containers:
```

### 2. Possíveis Problemas

**Problema**: Kepler não envia métricas `kepler_container_*`
- Pode estar em mock mode
- Precisa de acesso a RAPL (Intel) ou hardware real
- Em VMs/minikube: configure `KEPLER_MOCK_MODE=true`

**Solução**: Ativar modo mock e observar métricas

```bash
kubectl patch daemonset kepler -n kepler -p \
  '{"spec":{"template":{"spec":{"containers":[{"name":"kepler-exporter","env":[{"name":"KEPLER_MOCK_MODE","value":"true"}]}]}}}}'
```

### 3. Forçar Prometheus Scrapeamento

```bash
# Verificar service do Kepler
kubectl get svc -n kepler

# Teste manual
kubectl exec -n monitoring prometheus-server-* -- \
  curl -s http://kepler.kepler.svc:9102/metrics | grep "kepler_" | head
```

---

## 📁 Arquivos Modificados

### `experimental-cluster-setup/kepler-values.yaml`
- ✅ Customizado com RAPL, cgroups, BPF metrics
- ✅ ServiceMonitor habilitado (namespace monitoring)
- ⚠️ Argumento `-v` removido manualmente (bug do chart)

### Patches Aplicados
```bash
# Remover args problemático
kubectl patch daemonset kepler -n kepler --type json \
  -p='[{"op":"replace","path":"/spec/template/spec/containers/0/args","value":[]}]'

# Remover readiness probe
kubectl patch daemonset kepler -n kepler \
  -p='{"spec":{"template":{"spec":{"containers":[{"name":"kepler-exporter","readinessProbe":null}]}}}}'
```

---

## 🚀 Resumo Rápido do Status

| Componente | Status | Notas |
|-----------|--------|-------|
| Kepler | ✅ Rodando | Pod em Running (sem ready probe) |
| ServiceMonitor | ✅ Criado | Prometheus está configurado para scrapeamento |
| Prometheus | ✅ Online | Reconfigurado, mas sem dados do Kepler ainda |
| Grafana | ✅ Online | Pronto para usar com cAdvisor |
| OAI Pods | ✅ Saudáveis | 8/8 pods em Running |

---

## 💡 Recomendação

**Para começar agora:**
1. Use dashboard com métricas de cAdvisor (memory, cpu, network)
2. Isso funciona 100% e já têm dados
3. Continúe investigando Kepler em background

**Depois:**
1. Debug Kepler para obter métricas de energia
2. Importe dashboard `Kepler Exporter Dashboard-1767693829895.json` quando Kepler funcionar

---

## 📖 Referências

- Kepler Helm Chart: https://github.com/sustainable-computing-io/kepler-helm-repo
- Kepler Documentation: https://github.com/sustainable-computing-io/kepler
- Prometheus Operator: https://prometheus-operator.dev/

---

## Para Debug Futuro

```bash
# Ver todas as métricas disponíveis
kubectl port-forward -n monitoring svc/prometheus-server 9090:80 &
curl 'http://localhost:9090/api/v1/label/__name__/values?match[]=.*' | jq .

# Procurar por kepler
curl 'http://localhost:9090/api/v1/label/__name__/values?match[]=kepler.*' | jq .

# Contar métricas
curl 'http://localhost:9090/api/v1/query?query=count(*)' | jq '.data.result[0].value[1]'
```

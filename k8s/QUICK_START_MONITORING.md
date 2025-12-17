# Quick Reference - Monitoramento de Energia com Kepler

## 🎯 Objetivo

Capturar consumo de energia **por pod** usando Kepler, Prometheus e Grafana.

## ⚡ Arquitetura

```
15 Pods (GNB + UE)
        ↓
   Kepler (coleta)
        ↓
   Prometheus (armazena)
        ↓
   Grafana (visualiza)
```

## 🚀 Setup em 3 Linhas

```bash
cd k8s
./quickstart.sh                      # Setup OAI + K8s
./install-monitoring-stack.sh        # Instalar Kepler + stack
```

## 🌐 Acessar Dashboards

```bash
# Terminal 1: Port forwards
bash /tmp/port-forwards.sh

# Terminal 2: Abrir navegador
# Grafana: http://localhost:3000 (admin/grafana)
# Prometheus: http://localhost:9090
```

## 📊 Pods Monitorados

### GNB (Transmitter - 7 pods)
```
gnb-crc → gnb-layer-map → gnb-ldpc → gnb-modulation → gnb-ofdm-mod → gnb-precoding → gnb-scramble
```

### UE (Receiver - 8 pods)
```
ue-ofdm-demod → ue-soft-demod → ue-layer-demap → ue-ldpc-dec → ue-descrambling → ue-ch-est/ch-mmse → ue-check-crc
```

**Cada pod = Métrica de energia independente**

## 🔍 Queries Principais

### 1. Consumo Total
```promql
sum(kepler_container_energy_total{namespace="oai-isolation"})
```

### 2. Por Função
```promql
sum by (pod_name) (kepler_container_energy_total{namespace="oai-isolation"})
```

### 3. Top 5
```promql
topk(5, sum by (pod_name) (kepler_container_energy_total{namespace="oai-isolation"}))
```

### 4. GNB vs UE
```promql
sum(rate(kepler_container_energy_total{namespace="oai-isolation", pod_name=~"gnb-.*"}[5m])) /
sum(rate(kepler_container_energy_total{namespace="oai-isolation", pod_name=~"ue-.*"}[5m]))
```

### 5. Watts (Potência Instantânea)
```promql
rate(kepler_container_energy_total{namespace="oai-isolation"}[1m]) * 1000
```

## 💻 Comandos Úteis

### Status
```bash
make status                          # Ver pods
make kepler-status                  # Ver Kepler
make energy-total                   # Consumo total
make energy-top                     # Top 5
```

### Monitoramento Interativo
```bash
make energy-interactive              # Menu de queries
make port-forward-all               # Port forwards
```

### Logs
```bash
kubectl logs -f -n kepler -l app.kubernetes.io/name=kepler
kubectl logs -f -n prometheus -l app.kubernetes.io/name=prometheus
```

## 📈 Criar Dashboard Grafana

1. Acesse http://localhost:3000
2. Login: admin/grafana
3. Clique em "Create" → "Dashboard"
4. Add Panel → Prometheus Query
5. Use queries acima

## 🔄 Exemplo de Workflow

```bash
# 1. Deploy OAI Isolation
make deploy

# 2. Instalar Kepler
make install-monitoring

# 3. Port forwards (novo terminal)
make port-forward-all

# 4. Abrir Grafana
# http://localhost:3000

# 5. Criar dashboard com queries

# 6. Ver dados em tempo real
make energy-interactive

# 7. Exportar dados
# Grafana → Export → CSV/JSON
```

## 📊 Métricas Disponíveis

| Métrica | Descrição | Unidade |
|---------|-----------|---------|
| `kepler_container_energy_total` | Energia total | Joules |
| `kepler_container_cpu_energy_total` | Energia CPU | Joules |
| `kepler_container_dram_energy_total` | Energia RAM | Joules |
| `rate(...[1m])` | Potência instantânea | Watts |

## 🎓 Análise Típica

### Pergunta 1: Qual função consome mais?
```bash
make energy-top
```
→ Vê top 5 na tela

### Pergunta 2: GNB ou UE mais eficiente?
```promql
# No Prometheus
sum(rate(kepler_container_energy_total{pod_name=~"gnb-.*"}[5m])) / 
sum(rate(kepler_container_energy_total{pod_name=~"ue-.*"}[5m]))
```
→ Se > 1, GNB consome mais. Se < 1, UE consome mais.

### Pergunta 3: Quanto economizo otimizando X?
```promql
# Consumo de X
sum(kepler_container_energy_total{pod_name="gnb-ldpc"})

# Percentual do total
sum(kepler_container_energy_total{pod_name="gnb-ldpc"}) / 
sum(kepler_container_energy_total{namespace="oai-isolation"}) * 100
```
→ Se for 15%, economizando 50% nela = 7.5% de economia total

### Pergunta 4: Escala linearmente?
```bash
# Terminal 1
kubectl scale deployment gnb-ldpc --replicas=3 -n oai-isolation

# Terminal 2: Monitorar Grafana
# Consumo deve 3x

# Se não 3x exata, há ganhos/perdas de eficiência
```

## 🚨 Alertas Comuns

### Pico de Energia
```promql
rate(kepler_container_energy_total{namespace="oai-isolation"}[1m]) > 10
```

### Anomalia (desvio padrão)
```promql
abs(rate(kepler_container_energy_total[5m]) - 
    avg_over_time(rate(kepler_container_energy_total[5m])[1h:5m])) > 
2 * stddev_over_time(rate(kepler_container_energy_total[5m])[1h:5m])
```

## 📁 Arquivos Importantes

| Arquivo | Propósito |
|---------|-----------|
| `KEPLER_MONITORING.md` | Documentação completa |
| `install-monitoring-stack.sh` | Script de instalação |
| `energy-queries.sh` | Queries interativas |
| `Makefile` | Comandos úteis |

## ❓ Troubleshooting

### Kepler não coleta dados
```bash
kubectl describe pod -n kepler -l app.kubernetes.io/name=kepler
```

### Prometheus não conecta Kepler
```bash
kubectl port-forward -n kepler svc/kepler 8888:8888
curl http://localhost:8888/metrics
```

### Grafana sem dados
- Verificar Prometheus targets: http://localhost:9090/targets
- Verificar query: http://localhost:9090/graph

## 🎯 Próximos Passos

1. ✅ Setup (3 scripts)
2. ✅ Monitoramento (Kepler + Prometheus + Grafana)
3. 📊 Criar dashboards customizados
4. 🔍 Analisar dados
5. 🎯 Identificar otimizações
6. 📈 Escalar e revalidar

---

**Comande Rápido:**
```bash
cd k8s && ./quickstart.sh && ./install-monitoring-stack.sh && bash /tmp/port-forwards.sh
# Abra http://localhost:3000 em outro terminal
```

**Energia por Função:**
```bash
make energy-top
```

**Menu Interativo:**
```bash
make energy-interactive
```

---

**Happy Monitoring! 🚀**

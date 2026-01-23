# 📊 Dashboard Kepler Energy Monitoring

## 🎯 Objetivo

Dashboard Grafana em tempo real para monitorar consumo de energia (watts) e energia acumulada (joules) dos containers OAI UE em Kubernetes, com suporte para pods em reinicialização.

---

## 📈 Painéis Disponíveis

### 1. **CPU Power Consumption (Watts) - Real Time**
- **Localização**: Topo-esquerda
- **Métrica**: `rate(kepler_container_cpu_watts[1s])`
- **Agrupamento**: Por `container_namespace/pod_name`
- **Tipo**: Gráfico de linha com preenchimento
- **Legenda**: Mostra mean, max, last (valor atual)
- **Uso**: Acompanhar consumo instantâneo de cada pod

### 2. **CPU Energy Consumption (Joules) - Cumulative**
- **Localização**: Topo-direita
- **Métrica**: `sum(kepler_container_cpu_joules) by (...)`
- **Agrupamento**: Por `container_namespace/pod_name`
- **Tipo**: Gráfico de linha com gradiente
- **Legenda**: Mostra mean, max, last (total acumulado)
- **Uso**: Ver energia total consumida mesmo após restarts

### 3. **Top Consumers by Namespace/Pod (Top 50)**
- **Localização**: Segunda linha, ocupando toda largura
- **Tipo**: Tabela
- **Colunas**:
  - Namespace: container_namespace
  - Pod: pod_name
  - Container: container_name
  - Current Power (W): Taxa instantânea
  - Total Energy (J): Energia acumulada
- **Ordenação**: Por poder atual (descendente)
- **Uso**: Identificar quick quais pods consomem mais

### 4. **Total CPU Power by Namespace (Stacked)**
- **Localização**: Terceira linha, esquerda
- **Métrica**: Agregada por namespace
- **Tipo**: Gráfico de linha stacked
- **Uso**: Comparar consumo entre namespaces (default vs monitoring vs kepler)

### 5. **Power Distribution by Namespace**
- **Localização**: Terceira linha, direita
- **Tipo**: Gráfico de pizza (pie chart)
- **Dados**: Proporção do consumo total por namespace
- **Uso**: Visualizar distribuição rápida de energia

### 6. **Total System Power (Stat)**
- **Localização**: Rodapé, esquerda
- **Métrica**: Soma total instantânea em Watts
- **Cores**: Verde (<2W), Amarelo (2-5W), Vermelho (>5W)
- **Tipo**: Card grande com gráfico de área

### 7. **Total Energy Consumed (Stat)**
- **Localização**: Rodapé, centro-esquerda
- **Métrica**: Soma total acumulada em Joules
- **Tipo**: Card grande com histórico
- **Uso**: Ver energia total desde que Kepler iniciou

### 8. **Active Containers (Stat)**
- **Localização**: Rodapé, centro-direita
- **Métrica**: Contagem de containers únicos
- **Tipo**: Número simples
- **Uso**: Monitorar quantos containers estão ativos

### 9. **Active Namespaces (Stat)**
- **Localização**: Rodapé, direita
- **Métrica**: Contagem de namespaces únicos
- **Tipo**: Número simples
- **Uso**: Rastrear ambientes ativos

---

## 🔄 Comportamento com Restarts de Pods

O dashboard foi projetado para **persistir métricas mesmo quando pods reiniciam**:

### Watts (Power Instantâneo)
```
rate(kepler_container_cpu_watts[1s])
```
- Mostra **consumo atual**
- Cada novo pod terá sua própria série temporal
- Histórico anterior permanece visível
- **Legenda automática** diferencia por `pod_name`

### Joules (Energia Acumulada)
```
kepler_container_cpu_joules
```
- Valor **cumulativo** dentro de cada pod
- Quando pod reinicia: **nova série** com novo pod_name
- Histórico anterior **não é perdido** (fica no gráfico)
- Podem haver gaps quando pod está down

### Agrupamento Inteligente
```
sum(...) by (container_namespace, container_name, pod_name)
```
- Cada pod tem seu próprio ID (pod_name)
- Namespaces mantêm agregação mesmo com restarts
- Container_name padronizado permite correlação

---

## 🚀 Como Usar

### 1. **Acessar o Dashboard**

#### Opção A: Via port-forward
```bash
kubectl port-forward -n monitoring svc/grafana 3000:80
# Acessar: http://localhost:3000
```

#### Opção B: Obter URL do minikube
```bash
minikube service grafana -n monitoring --url
```

### 2. **Credenciais Padrão**
- **Usuário**: `admin`
- **Senha**: `prom-operator`

### 3. **Importar o Dashboard**

#### Método Automático
```bash
./import-kepler-dashboard.sh
```

#### Método Manual
1. Ir em: **Create → Import Dashboard**
2. Colar conteúdo de `k8s-manifests/kepler-energy-dashboard.json`
3. Selecionar Datasource: `Prometheus`
4. Clicar em "Import"

### 4. **Visualizar Métricas**
- Dashboard atualiza **a cada 1 segundo** (`refresh: 1s`)
- Janela de tempo padrão: **Últimas 1 hora**
- Mudar intervalo: Clicar no botão de data/hora no topo

---

## 📊 Exemplos de Interpretação

### Cenário 1: Pod Reinicia
```
Before:  ue-check-crc-abc123 → 2.5W, 1000J total
Restart: ue-check-crc-abc123 vai down
After:   ue-check-crc-xyz789 → 0.5W, 0J (novo)
         ue-check-crc-abc123 → Histórico visível
```
**Interpretação**: Novo pod consome menos no começo, histórico do anterior fica registrado.

### Cenário 2: Escalamento de Replicas
```
1 replica: ue-ldpc-dec → 3.0W
2 replicas: ue-ldpc-dec (pod1) + ue-ldpc-dec (pod2) → cada 1.5W = 3.0W total
```
**Interpretação**: Carga distribuída entre replicas.

### Cenário 3: Comparação de Namespaces
```
default (OAI): 5.0W
monitoring: 0.3W
kepler: 0.1W
```
**Interpretação**: OAI consome ~94% da energia total do cluster.

---

## 🔧 Customizações Possíveis

### Adicionar Filtro por Namespace
Editar painel e adicionar variável template:
```json
{
  "templating": {
    "list": [
      {
        "name": "namespace",
        "type": "query",
        "query": "label_values(kepler_container_cpu_joules, container_namespace)",
        "allValue": null,
        "includeAll": true
      }
    ]
  }
}
```

Depois usar na query:
```promql
sum(rate(kepler_container_cpu_watts[1s])) by (pod_name) 
and on() group_left() count(count by (container_namespace) (kepler_container_cpu_joules{container_namespace=~"$namespace"}))
```

### Alertas
Criar alerta se consumo > 5W:
```promql
sum(rate(kepler_container_cpu_watts[1s])) > 5
```

### Exportar Métricas
```bash
# PromQL direto
curl "http://prometheus:9090/api/v1/query?query=kepler_container_cpu_joules"
```

---

## 🔍 Verificação de Dados

### 1. Verificar se Kepler está enviando métricas
```bash
kubectl logs -n kepler kepler-<pod-id>
# Procurar por "exported" ou "metrics"
```

### 2. Verificar no Prometheus
```bash
kubectl port-forward -n monitoring svc/prometheus-operated 9090:9090
# Acessar: http://localhost:9090/graph
# Query: kepler_container_cpu_watts
```

### 3. Verificar ServiceMonitor
```bash
kubectl get servicemonitor -n kepler
kubectl describe servicemonitor kepler -n kepler
```

### 4. Verificar scrape config
```bash
kubectl port-forward -n monitoring svc/prometheus-operated 9090:9090
# Acessar: http://localhost:9090/config
# Procurar por job_name: kepler
```

---

## 📋 Troubleshooting

### Problema: "No data points"

**Causa 1: Métricas não estão sendo coletadas**
```bash
kubectl exec -n kepler kepler-<pod-id> -- kepler version
# Se der erro, binário pode estar com problema
```

**Causa 2: ServiceMonitor não está ativado**
```bash
kubectl get servicemonitor -n kepler
# Se vazio, criar ServiceMonitor (veja próxima seção)
```

**Causa 3: Prometheus não está scrapeando**
```bash
kubectl logs -n monitoring prometheus-prometheus-0
# Procurar por erros de conexão com kepler
```

### Problema: Métricas do Kepler desaparecem após restart

**Solução**: Métricas são efêmeras. O histórico no Prometheus persiste por 15 dias (retenção padrão).
Para aumentar:
```bash
kubectl patch prometheus prometheus -n monitoring \
  --type merge -p '{"spec":{"retention":"30d"}}'
```

### Problema: Pod reinicia e perde referência

**Esperado**: Novo pod_name = nova série. Isso é correto.
Para correlacionar, usar `container_name` em vez de `pod_name`:
```promql
sum(rate(kepler_container_cpu_watts[1s])) by (container_namespace, container_name)
```

---

## 📌 Referências

- **Kepler Metrics**: https://sustainable-computing-io.github.io/kepler/
- **Prometheus Query Language**: https://prometheus.io/docs/prometheus/latest/querying/basics/
- **Grafana Dashboards**: https://grafana.com/docs/grafana/latest/dashboards/

---

## 🎯 Próximos Passos

1. **Validar dados**: Conferir se métricas aparecem em tempo real
2. **Criar alertas**: Baseado em limites de consumo
3. **Integrar com CI/CD**: Exportar métricas para relatórios
4. **Dashboard secundário**: Comparar OAI vs outros workloads

---

**Última atualização**: Janeiro 2026  
**Status**: ✅ Pronto para usar

# Guia Completo: Kepler + Prometheus + Grafana no Kubernetes

Este guia documenta a instalação e configuração completa do stack de monitoramento de energia com Kepler, Prometheus e Grafana em um cluster Kubernetes.

## Pré-requisitos

- Kubernetes cluster funcionando (ex: minikube, kind, ou cluster real)
- Helm 3 instalado
- kubectl configurado

## Arquitetura

```
Kepler (DaemonSet) → Prometheus (Server) → Grafana (Dashboard)
    :28282/metrics       scrape 5s          datasource proxy
```

## 1. Instalar Prometheus

### 1.1 Adicionar repositório Helm

```bash
helm repo add prometheus-community https://prometheus-community.github.io/helm-charts
helm repo update
```

### 1.2 Criar namespace

```bash
kubectl create namespace monitoring
```

### 1.3 Configurar valores do Prometheus

Criar arquivo `experimental-cluster-setup/prom-values.yaml`:

```yaml
server:
  persistentVolume:
    enabled: false

extraScrapeConfigs: |-
  - job_name: 'kepler'
    scrape_interval: 5s
    scrape_timeout: 2s
    static_configs:
      # Kepler v0.11.3 default listen address is :28282 (from ConfigMap web.listenAddresses)
      - targets: ['kepler.kepler:28282']

alertmanager:
  enabled: false
prometheus-pushgateway:
  enabled: false
prometheus-node-exporter:
  enabled: false
kubeStateMetrics:
  enabled: false
```

**⚠️ IMPORTANTE**: A porta do Kepler v0.11.3 é **28282**, não 9102. Esta foi a principal configuração corrigida.

### 1.4 Instalar Prometheus

```bash
helm upgrade --install prometheus prometheus-community/prometheus \
  -n monitoring \
  -f experimental-cluster-setup/prom-values.yaml
```

## 2. Instalar Kepler

### 2.1 Adicionar repositório Helm do Kepler

```bash
helm repo add kepler https://sustainable-computing-io.github.io/kepler-helm-chart/
helm repo update
```

### 2.2 Criar namespace

```bash
kubectl create ns kepler
```

### 2.3 Configurar valores do Kepler

Criar arquivo `experimental-cluster-setup/kepler-values.yaml`:

```yaml
image:
  repository: quay.io/sustainable_computing_io/kepler
  tag: "v0.11.3"
  pullPolicy: IfNotPresent

command: []

extraEnvVars:
  KEPLER_LOG_LEVEL: "5"
  ENABLE_GPU: "false"
  ENABLE_QAT: "false"
  ENABLE_EBPF_CGROUPID: "true"
  EXPOSE_HW_COUNTER_METRICS: "true"
  EXPOSE_IRQ_COUNTER_METRICS: "true"
  EXPOSE_CGROUP_METRICS: "true"
  ENABLE_PROCESS_METRICS: "true"
  EXPOSE_BPF_METRICS: "true"
  EXPOSE_COMPONENT_POWER: "true"
  RAPL_ENABLED: "true"
  EXPOSE_RAPL_METRICS: "true"

serviceMonitor:
  enabled: true
  namespace: monitoring
  interval: 2s
  scrapeTimeout: 2s

startupProbe:
  enabled: true
  failureThreshold: 15
  periodSeconds: 10

livenessProbe:
  enabled: true
  periodSeconds: 60

readinessProbe:
  enabled: true
  periodSeconds: 10

resources:
  limits:
    memory: 1Gi
    cpu: 2000m
  requests:
    memory: 512Mi
    cpu: 500m

hostNetwork: true
hostPID: true
securityContext:
  privileged: true
  capabilities:
    add:
      - SYS_ADMIN
      - SYS_PTRACE
      - SYS_RESOURCE
      - NET_ADMIN
      - PERFMON

volumes:
  - name: lib-modules
    hostPath:
      path: /lib/modules
  - name: usr-src
    hostPath:
      path: /usr/src
  - name: sys
    hostPath:
      path: /sys
  - name: bpf
    hostPath:
      path: /sys/fs/bpf
  - name: debugfs
    hostPath:
      path: /sys/kernel/debug

volumeMounts:
  - name: lib-modules
    mountPath: /lib/modules
    readOnly: true
  - name: usr-src
    mountPath: /usr/src
    readOnly: true
  - name: sys
    mountPath: /host/sys
    readOnly: true
  - name: bpf
    mountPath: /sys/fs/bpf
  - name: debugfs
    mountPath: /sys/kernel/debug
```

### 2.4 Instalar Kepler

```bash
helm upgrade --install kepler kepler/kepler \
  -n kepler \
  -f experimental-cluster-setup/kepler-values.yaml
```

## 3. Instalar Grafana

### 3.1 Adicionar repositório Helm do Grafana

```bash
helm repo add grafana https://grafana.github.io/helm-charts
helm repo update
```

### 3.2 Preparar dashboard do Kepler

Criar ConfigMap com o dashboard do Kepler:

```bash
kubectl create configmap kepler-dashboard \
  -n monitoring \
  --from-file=k8s-manifests/Kepler\ Exporter\ Dashboard-1767693829895.json
```

### 3.3 Configurar valores do Grafana

Criar arquivo `experimental-cluster-setup/grafana-values.yaml`:

```yaml
adminPassword: admin123
service:
  type: ClusterIP

datasources:
  datasources.yaml:
    apiVersion: 1
    datasources:
      - name: Prometheus-Kepler
        type: prometheus
        url: http://prometheus-server.monitoring.svc.cluster.local
        access: proxy
        isDefault: true

dashboardProviders:
  dashboardproviders.yaml:
    apiVersion: 1
    providers:
      - name: 'kepler'
        orgId: 1
        folder: ''
        type: file
        disableDeletion: false
        editable: true
        options:
          path: /var/lib/grafana/dashboards/kepler

dashboardsConfigMaps:
  kepler: kepler-dashboard
```

### 3.4 Instalar Grafana

```bash
helm upgrade --install grafana grafana/grafana \
  -n monitoring \
  -f experimental-cluster-setup/grafana-values.yaml
```

## 4. Verificação da Instalação

### 4.1 Verificar pods

```bash
# Verificar Kepler
kubectl get pods -n kepler

# Verificar Prometheus e Grafana
kubectl get pods -n monitoring
```

### 4.2 Verificar serviços

```bash
kubectl get svc -n kepler
kubectl get svc -n monitoring
```

### 4.3 Verificar se Prometheus está coletando do Kepler

```bash
# Via API do Prometheus dentro do cluster
kubectl run tmp-check --rm -i --tty --image=alpine --restart=Never -- \
  sh -c "apk add --no-cache jq wget >/dev/null && \
  wget -qO- 'http://prometheus-server.monitoring.svc.cluster.local/api/v1/targets' | \
  jq '.data.activeTargets[] | select(.labels.job==\"kepler\") | {health,lastError,scrapeUrl:.scrapeUrl,lastScrape:.lastScrape}'"
```

Saída esperada:
```json
{
  "health": "up",
  "lastError": "",
  "scrapeUrl": "http://kepler.kepler:28282/metrics",
  "lastScrape": "2026-01-12T13:35:39.019399437Z"
}
```

### 4.4 Testar consulta de métricas

```bash
# Consultar métricas do Kepler
kubectl run tmp-query --rm -i --tty --image=alpine --restart=Never -- \
  sh -c "apk add --no-cache jq wget >/dev/null && \
  wget -qO- 'http://prometheus-server.monitoring.svc.cluster.local/api/v1/query?query=kepler_container_cpu_joules_total' | \
  jq '.data.result | .[0:3]'"
```

### 4.5 Verificar Grafana consegue acessar dados

```bash
# Verificar datasource do Grafana
kubectl run tmp-grafana --rm -i --tty --image=alpine --restart=Never -- \
  sh -c "apk add --no-cache curl >/dev/null && \
  curl -s -u admin:admin123 http://grafana.monitoring.svc/api/datasources"

# Consultar via proxy do Grafana
kubectl run tmp-grafq --rm -i --tty --image=alpine --restart=Never -- \
  sh -c "apk add --no-cache curl >/dev/null && \
  curl -s -u admin:admin123 'http://grafana.monitoring.svc/api/datasources/proxy/1/api/v1/query?query=kepler_container_cpu_joules_total' | head"
```

## 5. Acessar as UIs

### 5.1 Acessar Prometheus

```bash
kubectl port-forward -n monitoring svc/prometheus-server 9090:80
```

Abrir navegador: http://localhost:9090

- Ir em **Status > Targets** e verificar job `kepler` como **UP**
- Ir em **Graph** e consultar métricas:
  - `kepler_container_cpu_joules_total`
  - `kepler_container_joules_total`
  - `kepler_node_power_milliwatts`

### 5.2 Acessar Grafana

```bash
kubectl port-forward -n monitoring svc/grafana 3000:80
```

Abrir navegador: http://localhost:3000

- **Usuário**: admin
- **Senha**: admin123

Navegar:
1. **Explore** → selecionar datasource **Prometheus-Kepler** → consultar métricas Kepler
2. **Dashboards** → localizar **Kepler Exporter Dashboard** importado automaticamente

## 6. Principais Métricas do Kepler

### Métricas de Container

- `kepler_container_joules_total` - Energia total por container
- `kepler_container_cpu_joules_total` - Energia consumida pela CPU do container
- `kepler_container_dram_joules_total` - Energia consumida pela DRAM do container
- `kepler_container_package_joules_total` - Energia do package CPU do container
- `kepler_container_gpu_joules_total` - Energia consumida pela GPU do container
- `kepler_container_other_joules_total` - Outras fontes de energia do container

### Métricas de Nó

- `kepler_node_power_milliwatts` - Potência total do nó em miliwatts
- `kepler_node_package_joules_total` - Energia do package CPU do nó
- `kepler_node_core_joules_total` - Energia dos cores da CPU do nó
- `kepler_node_dram_joules_total` - Energia da DRAM do nó

### Métricas de Processo

- `kepler_process_power_milliwatts` - Potência por processo
- `kepler_process_package_joules_total` - Energia do package por processo

## 7. Troubleshooting

### Problema: Target Kepler não aparece no Prometheus

**Solução**: Verificar se a porta está correta no scrape config:

```bash
# Verificar porta do Kepler
kubectl get configmap kepler -n kepler -o yaml | grep listenAddresses

# Deve retornar: - :28282
```

Se a porta estiver errada no `prom-values.yaml`, corrigir e fazer upgrade:

```bash
helm upgrade prometheus prometheus-community/prometheus \
  -n monitoring \
  -f experimental-cluster-setup/prom-values.yaml
```

### Problema: Kepler pod não inicia

**Solução**: Verificar logs:

```bash
kubectl logs -n kepler -l app.kubernetes.io/name=kepler
```

Causas comuns:
- Falta de permissões privilegiadas
- Kernel não suporta eBPF
- Falta de volumes hostPath

### Problema: Métricas não aparecem no Grafana

**Solução**: Testar datasource:

```bash
# Via proxy do Grafana
kubectl run test-ds --rm -i --tty --image=alpine --restart=Never -- \
  sh -c "apk add --no-cache curl >/dev/null && \
  curl -s -u admin:admin123 \
  'http://grafana.monitoring.svc/api/datasources/proxy/1/api/v1/query?query=up{job=\"kepler\"}'"
```

Se retornar erro, verificar:
1. URL do datasource no Grafana values
2. Conectividade entre Grafana e Prometheus

## 8. Ordem de Instalação Recomendada

1. **Prometheus** (primeiro para estar pronto quando Kepler subir)
2. **Kepler** (começa a exportar métricas)
3. **ConfigMap do dashboard** (antes do Grafana)
4. **Grafana** (por último, carrega datasource e dashboards)

## 9. Comandos Rápidos de Manutenção

### Reinstalar stack completo

```bash
# Remover tudo
helm uninstall prometheus -n monitoring
helm uninstall grafana -n monitoring
helm uninstall kepler -n kepler
kubectl delete configmap kepler-dashboard -n monitoring

# Reinstalar na ordem correta
helm upgrade --install prometheus prometheus-community/prometheus -n monitoring -f experimental-cluster-setup/prom-values.yaml
helm upgrade --install kepler kepler/kepler -n kepler -f experimental-cluster-setup/kepler-values.yaml
kubectl create configmap kepler-dashboard -n monitoring --from-file=k8s-manifests/Kepler\ Exporter\ Dashboard-1767693829895.json
helm upgrade --install grafana grafana/grafana -n monitoring -f experimental-cluster-setup/grafana-values.yaml
```

### Atualizar configuração do Prometheus

```bash
# Editar prom-values.yaml
helm upgrade prometheus prometheus-community/prometheus \
  -n monitoring \
  -f experimental-cluster-setup/prom-values.yaml
```

### Reiniciar Kepler

```bash
kubectl rollout restart daemonset kepler -n kepler
```

## 10. Diferenças Importantes entre Versões do Kepler

### Kepler v0.11.3 (atual)

- Porta padrão: **28282**
- ConfigMap `web.listenAddresses: [":28282"]`
- ServiceMonitor suportado

### Versões anteriores (< v0.11)

- Porta padrão: **9102**
- Possível inconsistência entre chart e imagem

**Sempre verificar a porta real consultando o ConfigMap do Kepler após instalação.**

## Resumo das Configurações Alteradas

Durante a configuração, foi necessário **corrigir a porta do Kepler** no arquivo `experimental-cluster-setup/prom-values.yaml`:

**Antes** (incorreto):
```yaml
- targets: ['kepler.kepler:9102']
```

**Depois** (correto):
```yaml
- targets: ['kepler.kepler:28282']
```

Esta foi a única mudança necessária para o stack funcionar corretamente.

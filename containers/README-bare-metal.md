# Kepler + Prometheus + Grafana (Bare Metal Guide)

Guia para habilitar métricas completas de energia (RAPL/PMU) e eBPF em bare metal, tomando como base o fluxo do README original.

## 0. Pré-requisitos no Host (bare metal)
- Linux com systemd e kernel >= 5.10
- Acesso root (sudo)
- pacotes: `curl`, `jq`, `helm`, `kubectl`, `docker` (opcional se usar container runtime), `conntrack`, `socat` (para minikube)
- Headers do kernel: `linux-headers-$(uname -r)`
- Perf/PMU liberado: `sysctl -w kernel.perf_event_paranoid=-1`
- kptr: `sysctl -w kernel.kptr_restrict=0`
- Montagens obrigatórias:
  - `mount -t bpf bpf /sys/fs/bpf`
  - `mount -t debugfs none /sys/kernel/debug`
- Certifique-se de que `/lib/modules` e `/usr/src` existem e correspondem ao kernel em uso.
```bash
echo "[kernel] $(uname -r)"; \
for bin in curl jq helm kubectl docker conntrack socat; do command -v $bin >/dev/null && echo "[ok] $bin" || echo "[faltando] $bin"; done; \
echo "[headers] /lib/modules/$(uname -r):" $(ls /lib/modules/$(uname -r) 2>/dev/null | wc -l) "entradas"; \
echo "[sysctl] perf_event_paranoid=$(sysctl -n kernel.perf_event_paranoid 2>/dev/null)"; \
echo "[sysctl] kptr_restrict=$(sysctl -n kernel.kptr_restrict 2>/dev/null)"; \
mount | grep -E "bpf|debugfs" | sed 's/^/[mount] /'
```


## 1. Ambiente Kubernetes (escolha 1)
### Opção A: Minikube (driver none, bare metal)
```bash
# Instale minikube: https://minikube.sigs.k8s.io/docs/start/
# Driver none (root)
sudo minikube start --driver=none --kubernetes-version=v1.28.0 \
  --extra-config=kubelet.authentication-anonymous-auth=false \
  --extra-config=kubelet.cgroup-driver=systemd
```

### Opção B: kubeadm em host único (simplificado)
```bash
# Pré-requisitos kubeadm/kubelet/kubectl
# https://kubernetes.io/docs/setup/production-environment/tools/kubeadm/install-kubeadm/

sudo kubeadm init --pod-network-cidr=10.244.0.0/16
mkdir -p $HOME/.kube
sudo cp /etc/kubernetes/admin.conf $HOME/.kube/config
sudo chown $(id -u):$(id -g) $HOME/.kube/config
# Instale CNI (ex: Calico)
kubectl apply -f https://raw.githubusercontent.com/projectcalico/calico/v3.27.2/manifests/calico.yaml
```

## 2. Ajustes de segurança para Kepler (necessários para RAPL + eBPF)
- Kepler precisa rodar com:
  - `privileged: true`
  - `hostNetwork: true`
  - `hostPID: true`
  - Capabilities: `SYS_ADMIN`, `SYS_PTRACE`, `SYS_RESOURCE`, `NET_ADMIN`, `PERFMON` (se suportado)
- Volumes hostPath a montar:
  - `/lib/modules`
  - `/usr/src` (opcional, para headers)
  - `/sys` (somente leitura)
  - `/sys/fs/bpf`
  - `/sys/kernel/debug`

## 3. Instalar Kepler com RAPL + eBPF
Crie o values para o Helm:
```bash
cat > /tmp/kepler-values.yaml << 'EOF'
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
  interval: 30s
  scrapeTimeout: 10s

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
EOF
```

Instale Kepler (namespace `kepler`):
```bash
kubectl create namespace kepler --dry-run=client -o yaml | kubectl apply -f -
helm repo add kepler https://sustainable-computing-io.github.io/kepler-helm-chart
helm repo update
helm upgrade --install kepler kepler/kepler -n kepler -f /tmp/kepler-values.yaml
kubectl rollout status daemonset kepler -n kepler --timeout=3m
```

## 4. Prometheus (scrape Kepler)
```bash
cat > /tmp/prom-values.yaml << 'EOF'
server:
  persistentVolume:
    enabled: false

extraScrapeConfigs: |-
  - job_name: 'kepler'
    scrape_interval: 30s
    scrape_timeout: 10s
    static_configs:
      - targets: ['kepler.kepler:9102']

alertmanager:
  enabled: false
prometheus-pushgateway:
  enabled: false
prometheus-node-exporter:
  enabled: false
kubeStateMetrics:
  enabled: false
EOF

kubectl create namespace monitoring --dry-run=client -o yaml | kubectl apply -f -
helm repo add prometheus-community https://prometheus-community.github.io/helm-charts
helm repo update
helm upgrade --install prometheus prometheus-community/prometheus -n monitoring -f /tmp/prom-values.yaml
kubectl rollout status deployment prometheus-server -n monitoring --timeout=3m
```

## 5. Grafana (dashboards Kepler)
```bash
cat > /tmp/grafana-values.yaml << 'EOF'
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
EOF

# Suponha que você já tenha o JSON do dashboard em /tmp/Kepler-Exporter.json
kubectl create configmap kepler-dashboard -n monitoring \
  --from-file=/tmp/Kepler-Exporter.json --dry-run=client -o yaml | kubectl apply -f -

helm repo add grafana https://grafana.github.io/helm-charts
helm repo update
helm upgrade --install grafana grafana/grafana -n monitoring -f /tmp/grafana-values.yaml
kubectl rollout status deployment grafana -n monitoring --timeout=3m
```

## 6. Deploy de workloads (Dockerfile ou docker-compose)

### 6.1 Imagem única a partir de Dockerfile
```bash
# Na raiz do seu projeto (com podman ou docker)
podman build -t local/app:latest -f Dockerfile .
# ou
docker build -t local/app:latest -f Dockerfile .

# Para kubeadm com containerd, importe diretamente
podman save local/app:latest | sudo ctr -n k8s.io images import -
# ou se usar docker
docker save local/app:latest | sudo ctr -n k8s.io images import -

# Se estiver em minikube (driver none, bare metal)
minikube image load local/app:latest

# Aplicar Deployment/Service
cat > /tmp/app.yaml << 'EOF'
apiVersion: apps/v1
kind: Deployment
metadata:
  name: app
  namespace: default
spec:
  replicas: 1
  selector:
    matchLabels:
      app: app
  template:
    metadata:
      labels:
        app: app
    spec:
      containers:
        - name: app
          image: local/app:latest
          imagePullPolicy: IfNotPresent
---
apiVersion: v1
kind: Service
metadata:
  name: app
  namespace: default
spec:
  type: ClusterIP
  selector:
    app: app
  ports:
    - port: 80
      targetPort: 80
EOF
kubectl apply -f /tmp/app.yaml
```

### 6.2 Múltiplos contêineres (docker-compose)
Opções:

**A. Build com podman (recomendado para kubeadm + containerd):**
```bash
# Build todas as imagens com podman
for dockerfile in containers/gnb/*/Dockerfile containers/ue/*/Dockerfile; do
  component=$(basename $(dirname $dockerfile))
  side=$(basename $(dirname $(dirname $dockerfile)))
  podman build -t oai-${side}-${component}:latest -f $dockerfile .
done

# Listar imagens criadas
podman images | grep oai

# Importar todas para containerd (k8s.io namespace)
for img in $(podman images --format "{{.Repository}}:{{.Tag}}" | grep oai); do
  podman save $img | sudo ctr -n k8s.io images import -
done

# Criar manifests Kubernetes manualmente ou usar os gerados em k8s-manifests/
kubectl apply -f k8s-manifests/
```

**B. Usar docker-compose (requer Docker daemon):**
```bash
# Iniciar Docker daemon
sudo systemctl start docker

# Build com docker-compose
docker-compose build

# Importar para containerd
for img in $(docker images --format "{{.Repository}}:{{.Tag}}" | grep oai); do
  docker save $img | sudo ctr -n k8s.io images import -
done

# Aplicar manifests
kubectl apply -f k8s-manifests/
```

**C. Conversão com kompose:**
```bash
kompose convert -f docker-compose.yml -o k8s-compose/
kubectl apply -f k8s-compose/
```

**Importante:** Use `imagePullPolicy: Never` nos Deployments para forçar uso das imagens locais importadas no containerd.

### 6.3 Registro de imagens
**Kubeadm + containerd (recomendado):**
- Build com podman/docker e importe diretamente: `podman save IMAGE | sudo ctr -n k8s.io images import -`
- Use `imagePullPolicy: Never` ou `IfNotPresent` nos manifests
- Verifique imagens no containerd: `sudo ctr -n k8s.io images ls | grep oai`

**Minikube (driver none):**
- Use `minikube image load IMAGE` após build

**Registry local (opcional):**
- Suba um registry: `docker run -d -p 5000:5000 --restart=always --name registry registry:2`
- Tag e push: `podman tag IMAGE localhost:5000/IMAGE && podman push localhost:5000/IMAGE --tls-verify=false`
- Configure containerd para registry inseguro se necessário

### 6.4 Dicas rápidas
- Sempre ajustar `resources.requests/limits` para aparecer em métricas de scheduling e consumo.
- Para pods que rodam testes e encerram: use `restartPolicy: Never` ou `OnFailure` se não quiser loops.
- Para expor externamente: troque o Service para `type: NodePort` ou use Ingress/LoadBalancer conforme o ambiente.

Port-forward para acesso local:
```bash
kubectl port-forward -n monitoring svc/grafana 3000:80
kubectl port-forward -n monitoring svc/prometheus-server 9091:80
```
Credenciais Grafana: `admin / admin123`.

## 7. Atualizar imagens após mudanças no código-fonte

Quando modificar arquivos em `src/`, siga este fluxo para reconstruir e atualizar os containers no cluster:

### 7.1 Rebuild das imagens afetadas
```bash
# Identificar componentes afetados pela mudança
# Exemplo: se mudou src/nr_dlsch_onelayer.c (usado pelo gnb-ldpc)

# Rebuild da imagem específica
cd /home/anderson/dev/oai_isolation
podman build -t oai-gnb-ldpc:latest -f containers/gnb/ldpc/Dockerfile .

# Ou rebuild de múltiplas imagens
for component in ldpc modulation; do
  podman build -t oai-gnb-${component}:latest -f containers/gnb/${component}/Dockerfile .
done
```

### 7.2 Importar imagens atualizadas para containerd
```bash
# Import única
podman save oai-gnb-ldpc:latest | sudo ctr -n k8s.io images import -

# Ou import de múltiplas
for img in oai-gnb-ldpc oai-gnb-modulation; do
  podman save ${img}:latest | sudo ctr -n k8s.io images import -
done

# Verificar import
sudo ctr -n k8s.io images ls | grep oai-gnb-ldpc
```

### 7.3 Forçar restart dos pods para usar nova imagem
```bash
# Opção A: Delete o pod (Deployment recria automaticamente)
kubectl delete pod -l app=gnb-ldpc

# Opção B: Rollout restart do Deployment
kubectl rollout restart deployment gnb-ldpc

# Opção C: Restart de múltiplos deployments
kubectl rollout restart deployment gnb-ldpc gnb-modulation

# Verificar status do rollout
kubectl rollout status deployment gnb-ldpc
```

### 7.4 Verificar nova versão rodando
```bash
# Ver logs do novo pod
kubectl logs -l app=gnb-ldpc --tail=50

# Verificar hash da imagem no pod
kubectl get pod -l app=gnb-ldpc -o jsonpath='{.items[0].status.containerStatuses[0].imageID}'

# Comparar com hash no containerd
sudo ctr -n k8s.io images ls | grep oai-gnb-ldpc
```

### 7.5 Rebuild completo (todas as imagens)
```bash
# Rebuild todas
for dockerfile in containers/gnb/*/Dockerfile containers/ue/*/Dockerfile; do
  component=$(basename $(dirname $dockerfile))
  side=$(basename $(dirname $(dirname $dockerfile)))
  echo "Building oai-${side}-${component}..."
  podman build -t oai-${side}-${component}:latest -f $dockerfile .
done

# Import todas
for img in $(podman images --format "{{.Repository}}:{{.Tag}}" | grep oai); do
  echo "Importing $img..."
  podman save $img | sudo ctr -n k8s.io images import -
done

# Restart todos os deployments OAI
kubectl rollout restart deployment -l component=gnb
kubectl rollout restart deployment -l component=ue

# Aguardar conclusão
kubectl rollout status deployment --all --timeout=5m
```

### 7.6 Troubleshooting de atualizações
```bash
# Se pod não pegar nova imagem, verificar:
# 1. imagePullPolicy deve ser Never ou IfNotPresent
kubectl get deployment gnb-ldpc -o yaml | grep imagePullPolicy

# 2. Verificar se imagem foi importada com sucesso
sudo ctr -n k8s.io images ls | grep oai-gnb-ldpc

# 3. Forçar remoção do pod antigo
kubectl delete pod -l app=gnb-ldpc --grace-period=0 --force

# 4. Ver eventos do deployment
kubectl describe deployment gnb-ldpc

# 5. Comparar hash antes/depois
kubectl get pod -l app=gnb-ldpc -o jsonpath='{.items[0].status.containerStatuses[0].image}'
```

## 8. Validação rápida
```bash
# Logs do Kepler (ver RAPL/eBPF habilitados)
kubectl logs -n kepler -l app.kubernetes.io/name=kepler --tail=100 | grep -Ei "rapl|bpf|pmu"

# Métricas RAPL
kubectl exec -n kepler $(kubectl get pod -n kepler -l app.kubernetes.io/name=kepler -o jsonpath='{.items[0].metadata.name}') -- \
  curl -s http://localhost:9102/metrics | grep rapl

# Métricas eBPF
kubectl exec -n kepler $(kubectl get pod -n kepler -l app.kubernetes.io/name=kepler -o jsonpath='{.items[0].metadata.name}') -- \
  curl -s http://localhost:9102/metrics | grep bpf
```

## 8. Validação rápida
```bash
# Logs do Kepler (ver RAPL/eBPF habilitados)
kubectl logs -n kepler -l app.kubernetes.io/name=kepler --tail=100 | grep -Ei "rapl|bpf|pmu"

# Métricas RAPL
kubectl exec -n kepler $(kubectl get pod -n kepler -l app.kubernetes.io/name=kepler -o jsonpath='{.items[0].metadata.name}') -- \
  curl -s http://localhost:9102/metrics | grep rapl

# Métricas eBPF
kubectl exec -n kepler $(kubectl get pod -n kepler -l app.kubernetes.io/name=kepler -o jsonpath='{.items[0].metadata.name}') -- \
  curl -s http://localhost:9102/metrics | grep bpf
```

## 9. Notas importantes
- Bare metal: garante acesso a PMU/Perf e eBPF completos. Kind em Docker geralmente bloqueia PMU; evite para RAPL.
- Certifique-se de que as sysctls e montagens persistam após reboot (adicione em `/etc/fstab` e `/etc/sysctl.d/*.conf`).
- Se usar SELinux enforcing, pode ser necessário ajustar contextos ou desabilitar para testes.

## 9. Notas importantes
- Bare metal: garante acesso a PMU/Perf e eBPF completos. Kind em Docker geralmente bloqueia PMU; evite para RAPL.
- Certifique-se de que as sysctls e montagens persistam após reboot (adicione em `/etc/fstab` e `/etc/sysctl.d/*.conf`).
- Se usar SELinux enforcing, pode ser necessário ajustar contextos ou desabilitar para testes.

## 10. Métricas-chave
- `kepler_core_rapl_joules_total`, `kepler_dram_rapl_joules_total`, `kepler_uncore_rapl_joules_total`
- `kepler_container_core_joules_total` (energia por container)
- `kepler_bpf_*` (coletas via eBPF)
- `kepler_irq_count`, `kepler_process_*` se habilitado

## 11. Troubleshooting
- Se Kepler falhar em iniciar: verifique mounts (`/sys/fs/bpf`, `/sys/kernel/debug`), sysctls (`perf_event_paranoid`, `kptr_restrict`) e permissões do daemonset (privileged/hostPID/hostNetwork).
- Se RAPL não aparecer: valide suporte em `/sys/class/powercap/intel-rapl` e se o driver está carregado.
- Se eBPF falhar: confira `dmesg | grep -i bpf` e se o kernel suporta CO-RE e cgroup-bpf.

## 11. Troubleshooting
- Se Kepler falhar em iniciar: verifique mounts (`/sys/fs/bpf`, `/sys/kernel/debug`), sysctls (`perf_event_paranoid`, `kptr_restrict`) e permissões do daemonset (privileged/hostPID/hostNetwork).
- Se RAPL não aparecer: valide suporte em `/sys/class/powercap/intel-rapl` e se o driver está carregado.
- Se eBPF falhar: confira `dmesg | grep -i bpf` e se o kernel suporta CO-RE e cgroup-bpf.

## 12. Desativar/limpar o cluster

### Minikube (driver none)
```bash
sudo minikube stop
sudo minikube delete --all --purge
```

### kubeadm (host único)
```bash
sudo kubeadm reset -f
sudo systemctl stop kubelet
sudo systemctl stop containerd || true
sudo rm -rf /etc/cni/net.d /var/lib/cni /var/lib/kubelet /var/lib/etcd /var/lib/containerd /var/run/kubernetes
sudo ip link delete cni0     2>/dev/null || true
sudo ip link delete flannel.1 2>/dev/null || true
sudo ip link delete kube-ipvs0 2>/dev/null || true
```

### Namespaces/Helm releases (se quiser apenas remover workloads)
```bash
helm uninstall kepler prometheus grafana -n monitoring || true
helm uninstall kepler -n kepler || true
kubectl delete namespace kepler monitoring || true
```

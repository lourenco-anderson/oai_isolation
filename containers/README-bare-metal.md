# Kepler + Prometheus + Grafana (Bare Metal Guide)

Guia para habilitar métricas completas de energia (RAPL/PMU) e eBPF em bare metal, tomando como base o fluxo do README original.

## 0. Pré-requisitos no Host (bare metal)
- Linux com systemd e kernel >= 5.10
- Acesso root (sudo)
- pacotes: `curl`, `jq`, `helm`, `kubectl`, `podman` (container runtime recomendado), `conntrack`, `socat` (para minikube), `uidmap`
- Headers do kernel: `linux-headers-$(uname -r)`
- Perf/PMU liberado: `sysctl -w kernel.perf_event_paranoid=-1`
- kptr: `sysctl -w kernel.kptr_restrict=0`
- Montagens obrigatórias:
  - `mount -t bpf bpf /sys/fs/bpf`
  - `mount -t debugfs none /sys/kernel/debug`
- Certifique-se de que `/lib/modules` e `/usr/src` existem e correspondem ao kernel em uso.

### 0.1 Verificação rápida de pré-requisitos
```bash
echo "[kernel] $(uname -r)"; \
for bin in curl jq helm kubectl podman conntrack socat; do command -v $bin >/dev/null && echo "[ok] $bin" || echo "[faltando] $bin"; done; \
echo "[headers] /lib/modules/$(uname -r):" $(ls /lib/modules/$(uname -r) 2>/dev/null | wc -l) "entradas"; \
echo "[sysctl] perf_event_paranoid=$(sysctl -n kernel.perf_event_paranoid 2>/dev/null)"; \
echo "[sysctl] kptr_restrict=$(sysctl -n kernel.kptr_restrict 2>/dev/null)"; \
mount | grep -E "bpf|debugfs" | sed 's/^/[mount] /'
```

### 0.2 Instalação de pacotes (Debian/Ubuntu)
```bash
sudo apt update
sudo apt install -y \
  curl jq helm kubectl podman conntrack socat uidmap \
  linux-headers-$(uname -r) \
  build-essential git
```

### 0.3 Configuração do Podman como daemon (alternativa a Docker)
```bash
# Instalar ou habilitar o serviço podman
sudo systemctl enable --now podman
sudo systemctl enable --now podman.socket

# Verificar status
sudo systemctl status podman
podman --version

# Configurar permissões para rootless podman (opcional)
sudo usermod -aG podman $USER
newgrp podman

# Testar podman
podman run --rm alpine echo "Podman funcionando!"
```

### 0.4 Configuração de sysctls e montagens (executar uma vez)
```bash
# Liberar acesso ao PMU/Perf
sudo sysctl -w kernel.perf_event_paranoid=-1
sudo sysctl -w kernel.kptr_restrict=0

# Montar bpf e debugfs (temporário - persistir em /etc/fstab se necessário)
sudo mount -t bpf bpf /sys/fs/bpf 2>/dev/null || true
sudo mount -t debugfs none /sys/kernel/debug 2>/dev/null || true

# Persistir sysctls no boot (opcional)
echo "kernel.perf_event_paranoid = -1" | sudo tee -a /etc/sysctl.d/99-perf.conf
echo "kernel.kptr_restrict = 0" | sudo tee -a /etc/sysctl.d/99-perf.conf
sudo sysctl -p /etc/sysctl.d/99-perf.conf
```


## 1. Ambiente Kubernetes (escolha 1)

### Opção A: Minikube com Podman (driver podman, recomendado)
```bash
# Instalar minikube (se não estiver instalado)
# https://minikube.sigs.k8s.io/docs/start/

# Versão rápida:
curl -LO https://github.com/kubernetes/minikube/releases/latest/download/minikube-linux-amd64
sudo install minikube-linux-amd64 /usr/local/bin/minikube

# Criar cluster com podman como driver
sudo minikube start \
  --driver=podman \
  --kubernetes-version=v1.28.0 \
  --extra-config=kubelet.authentication-anonymous-auth=false \
  --extra-config=kubelet.cgroup-driver=systemd \
  --container-runtime=containerd

# OU sem sudo (rootless, se podman rootless configurado)
minikube start \
  --driver=podman \
  --kubernetes-version=v1.28.0 \
  --extra-config=kubelet.authentication-anonymous-auth=false \
  --extra-config=kubelet.cgroup-driver=systemd

# Verificar status
minikube status
kubectl get nodes
```

### Opção A-bis: Minikube com driver none (bare metal direto)
```bash
# Requer instalar kubeadm, kubelet, kubectl
# https://kubernetes.io/docs/setup/production-environment/tools/kubeadm/install-kubeadm/

# Após instalar, execute:
sudo minikube start --driver=none --kubernetes-version=v1.28.0 \
  --extra-config=kubelet.authentication-anonymous-auth=false \
  --extra-config=kubelet.cgroup-driver=systemd
```

### Opção B: kubeadm em host único (simplificado, com containerd)
```bash
# Pré-requisitos kubeadm/kubelet/kubectl
# https://kubernetes.io/docs/setup/production-environment/tools/kubeadm/install-kubeadm/

# Instalar containerd (ou usar podman com CRI)
sudo apt install -y containerd

# Configurar containerd
sudo mkdir -p /etc/containerd
sudo containerd config default | sudo tee /etc/containerd/config.toml
sudo systemctl restart containerd

# Inicializar cluster
sudo kubeadm init --pod-network-cidr=10.244.0.0/16

# Configurar acesso ao kubectl
mkdir -p $HOME/.kube
sudo cp /etc/kubernetes/admin.conf $HOME/.kube/config
sudo chown $(id -u):$(id -g) $HOME/.kube/config

# Instale CNI (ex: Calico)
kubectl apply -f https://raw.githubusercontent.com/projectcalico/calico/v3.27.2/manifests/calico.yaml

# Aguardar nós ficarem ready
kubectl get nodes -w
```

### Opção B-bis: kubeadm com Podman CRI (experimental)
```bash
# Configurar podman para funcionar como CRI
sudo mkdir -p /etc/crio
sudo cat > /etc/crio/crio.conf.d/02-podman-cri << 'EOF'
[crio.runtime]
conmon = "/usr/bin/conmon"
conmon_cgroup = "pod"
cgroup_manager = "cgroupfs"
container_exits_dir = "/var/run/crio/exits"
container_cleanup_exit_code = 0
default_runtime = "crun"
enable_metrics = true
EOF

# Nota: Esta opção é menos comum; recomenda-se usar containerd ou Minikube com driver podman
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

### 6.1 Imagem única com Podman/Docker
```bash
# Na raiz do seu projeto (com podman - RECOMENDADO)
podman build -t local/app:latest -f Dockerfile .

# OU com docker (se preferir)
docker build -t local/app:latest -f Dockerfile .

# Para kubeadm + containerd: importar diretamente
podman save local/app:latest | sudo ctr -n k8s.io images import -

# OU se usar docker:
docker save local/app:latest | sudo ctr -n k8s.io images import -

# Se estiver em Minikube com driver podman:
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

### 6.2 Múltiplos contêineres com Podman (recomendado)
```bash
# Build todas as imagens com podman
for dockerfile in containers/gnb/*/Dockerfile containers/ue/*/Dockerfile; do
  component=$(basename $(dirname $dockerfile))
  side=$(basename $(dirname $(dirname $dockerfile)))
  echo "[BUILD] oai-${side}-${component}..."
  podman build -t oai-${side}-${component}:latest -f $dockerfile .
done

# Listar imagens criadas no podman
podman images | grep oai

# Para Minikube com driver podman: carregar imagens diretamente
for dockerfile in containers/gnb/*/Dockerfile containers/ue/*/Dockerfile; do
  component=$(basename $(dirname $dockerfile))
  side=$(basename $(dirname $(dirname $dockerfile)))
  echo "[LOAD] oai-${side}-${component} para Minikube..."
  minikube image load oai-${side}-${component}:latest
done

# OU Para kubeadm + containerd: importar todas para k8s.io namespace
for img in $(podman images --format "{{.Repository}}:{{.Tag}}" | grep oai); do
  echo "[IMPORT] $img para containerd..."
  podman save $img | sudo ctr -n k8s.io images import -
done

# Criar manifests Kubernetes manualmente ou usar os gerados em k8s-manifests/
kubectl apply -f k8s-manifests/

# Verificar pods em execução
kubectl get pods -o wide
kubectl logs -l component=gnb --tail=50
```

### 6.3 Docker-compose com Podman (alternativa)
```bash
# Usar podman-compose (similar ao docker-compose)
sudo apt install -y podman-compose

# Build com podman-compose
podman-compose build

# OU iniciar com docker-compose tradicional (se preferir Docker daemon)
sudo systemctl start docker
docker-compose build

# Importar para containerd (se necessário)
for img in $(docker images --format "{{.Repository}}:{{.Tag}}" | grep oai); do
  docker save $img | sudo ctr -n k8s.io images import -
done

# Aplicar manifests
kubectl apply -f k8s-manifests/

# OU converter para k8s com kompose
kompose convert -f docker-compose.yml -o k8s-compose/
kubectl apply -f k8s-compose/
```

### 6.4 Registro de imagens

**Minikube com driver podman (RECOMENDADO):**
- Build com `podman build -t IMAGE .`
- Carregar com `minikube image load IMAGE`
- Use `imagePullPolicy: IfNotPresent` nos manifests

**Kubeadm + containerd:**
- Build com `podman build -t IMAGE .`
- Importar com `podman save IMAGE | sudo ctr -n k8s.io images import -`
- Use `imagePullPolicy: Never` ou `IfNotPresent` nos manifests
- Verifique com `sudo ctr -n k8s.io images ls | grep IMAGE`

**Registry local (opcional, para ambos):**
```bash
# Suba um registry local
podman run -d -p 5000:5000 --name registry registry:2

# Tag e push com podman
podman tag IMAGE localhost:5000/IMAGE
podman push localhost:5000/IMAGE --tls-verify=false

# OU com docker
docker tag IMAGE localhost:5000/IMAGE
docker push localhost:5000/IMAGE

# Configurar containerd para registry inseguro (se necessário)
sudo mkdir -p /etc/containerd/certs.d/localhost:5000
sudo cat > /etc/containerd/certs.d/localhost:5000/hosts.toml << 'EOF'
server = "http://localhost:5000"
[host."http://localhost:5000"]
capabilities = ["pull", "resolve"]
skip_verify = true
EOF

sudo systemctl restart containerd
```

### 6.5 Dicas rápidas
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

## 12. Troubleshooting com Podman

### Podman socket não encontrado
```bash
# Se receber erro sobre /var/run/podman/podman.sock:
sudo systemctl enable --now podman.socket

# Verificar status
sudo systemctl status podman.socket
ls -la /var/run/podman/podman.sock
```

### Permissões com Podman rootless
```bash
# Se usar podman sem sudo:
sudo usermod -aG podman $USER
newgrp podman

# Verificar se funciona sem sudo
podman run --rm alpine echo "OK"
```

### Minikube com Podman não inicia
```bash
# Limpar Minikube anterior
minikube delete

# Restart do podman
sudo systemctl restart podman

# Tentar novamente
minikube start --driver=podman --kubernetes-version=v1.28.0

# Ver logs detalhados
minikube logs --follow
```

### Imagens não aparecem no Kubernetes
```bash
# Verificar imagens no podman
podman images | grep oai

# Se usar Minikube com driver podman:
minikube image ls | grep oai

# Se usar kubeadm + containerd:
sudo ctr -n k8s.io images ls | grep oai

# Se faltarem imagens, recarregar:
minikube image load oai-gnb-ldpc:latest
# OU
podman save oai-gnb-ldpc:latest | sudo ctr -n k8s.io images import -
```

### Erro: "rpc error: code = Unavailable"
```bash
# Containerd não está rodando (se usar kubeadm)
sudo systemctl status containerd

# Se não estiver rodando:
sudo systemctl start containerd
sudo systemctl enable containerd

# Verificar configuração:
sudo cat /etc/containerd/config.toml | grep -A 5 "\[plugins"
```

### Build lento ou falha ao importar imagens grandes
```bash
# Aumentar recursos disponíveis para Minikube
minikube delete
minikube start --driver=podman \
  --memory=8192 \
  --cpus=4 \
  --disk-size=50gb

# OU para podman diretamente, ajustar storage:
sudo podman system prune -a  # Limpar cache
sudo podman image prune -a   # Remover imagens não usadas
```

## 13. Desativar/limpar o cluster

### Minikube com Podman
```bash
minikube stop
minikube delete --all --purge

# Limpar containers e imagens orphãs do podman
podman container rm -a -f
podman image prune -a
```

### Minikube driver none
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

# Limpar imagens do containerd
sudo ctr -n k8s.io images rm $(sudo ctr -n k8s.io images ls -q) 2>/dev/null || true
```

### Namespaces/Helm releases (se quiser apenas remover workloads)
```bash
helm uninstall kepler prometheus grafana -n monitoring || true
helm uninstall kepler -n kepler || true
kubectl delete namespace kepler monitoring || true
```

## 14. Quick Start (Resumo do fluxo completo com Podman)

### Passo 1: Preparar ambiente
```bash
# Instalar dependências
sudo apt update && sudo apt install -y curl jq helm kubectl podman conntrack socat uidmap linux-headers-$(uname -r)

# Configurar sysctls e montagens
sudo sysctl -w kernel.perf_event_paranoid=-1
sudo sysctl -w kernel.kptr_restrict=0
sudo mount -t bpf bpf /sys/fs/bpf 2>/dev/null || true
sudo mount -t debugfs none /sys/kernel/debug 2>/dev/null || true

# Iniciar podman
sudo systemctl enable --now podman
```

### Passo 2: Criar cluster Kubernetes
```bash
# Instalar Minikube
curl -LO https://github.com/kubernetes/minikube/releases/latest/download/minikube-linux-amd64
sudo install minikube-linux-amd64 /usr/local/bin/minikube

# Iniciar com Podman
minikube start --driver=podman --kubernetes-version=v1.28.0 \
  --extra-config=kubelet.cgroup-driver=systemd \
  --memory=8192 --cpus=4
```

### Passo 3: Build e deploy de workloads
```bash
# Build das imagens
cd /home/anderson/dev/oai_isolation
for dockerfile in containers/gnb/*/Dockerfile containers/ue/*/Dockerfile; do
  component=$(basename $(dirname $dockerfile))
  side=$(basename $(dirname $(dirname $dockerfile)))
  podman build -t oai-${side}-${component}:latest -f $dockerfile .
done

# Carregar no Minikube
for dockerfile in containers/gnb/*/Dockerfile containers/ue/*/Dockerfile; do
  component=$(basename $(dirname $dockerfile))
  side=$(basename $(dirname $(dirname $dockerfile)))
  minikube image load oai-${side}-${component}:latest
done

# Deploy
kubectl apply -f k8s-manifests/
```

### Passo 4: Instalar Kepler + Prometheus + Grafana
```bash
# Executar as seções 3, 4, 5 do guia acima com helm
kubectl create namespace kepler monitoring
helm repo add kepler https://sustainable-computing-io.github.io/kepler-helm-chart
helm repo add prometheus-community https://prometheus-community.github.io/helm-charts
helm repo add grafana https://grafana.github.io/helm-charts
helm repo update

# Aplicar values (ver seções 3, 4, 5)
# ... (copiar comandos das seções 3, 4, 5)
```

### Passo 5: Acessar dashboards
```bash
kubectl port-forward -n monitoring svc/grafana 3000:80
kubectl port-forward -n monitoring svc/prometheus-server 9091:80

# Abrir no navegador:
# - Grafana: http://localhost:3000 (admin/admin123)
# - Prometheus: http://localhost:9091
```

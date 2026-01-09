# Quick Start - Cluster Kubernetes + Kepler + Prometheus + Grafana

## Guia rápido para inicializar o cluster do zero

Este documento resume todos os comandos necessários para subir o cluster Kubernetes com stack de monitoramento Kepler desde o início.

---

## Pré-requisitos

```bash
# Verificar versão do kernel (necessário >= 5.10)
uname -r

# Configurar sysctls necessários para Kepler
sudo sysctl -w kernel.perf_event_paranoid=-1
sudo sysctl -w kernel.kptr_restrict=0

# Verificar montagens necessárias
mount | grep -E 'bpf|debug'
```

---

## 1. Instalar Docker

```bash
# Instalar Docker
sudo apt-get update
sudo apt-get install -y docker.io

# Habilitar e iniciar serviço
sudo systemctl enable --now docker

# Adicionar usuário ao grupo docker (opcional, evita usar sudo)
sudo usermod -aG docker $USER
```

---

## 2. Limpar instalações anteriores (se necessário)

```bash
# Deletar cluster Minikube existente
minikube delete --all --purge

# Limpar kubeadm (se foi usado anteriormente)
sudo kubeadm reset -f
sudo systemctl stop kubelet containerd
sudo systemctl disable kubelet containerd
sudo rm -rf /etc/cni/net.d /var/lib/cni /var/lib/kubelet /var/lib/etcd /var/lib/containerd /var/run/kubernetes ~/.kube/config

# Limpar interfaces de rede
for link in cni0 flannel.1 kube-ipvs0 tunl0 vxlan.calico; do
  sudo ip link delete $link 2>/dev/null || true
done

# Limpar iptables
sudo iptables -t nat -F
sudo iptables -t filter -F
sudo iptables -t mangle -F
sudo iptables -X
```

---

## 3. Iniciar Cluster Minikube com Docker

```bash
# Desabilitar modo rootless (se estava configurado)
minikube config unset rootless

# Iniciar cluster Minikube
minikube start \
  --driver=docker \
  --kubernetes-version=v1.28.0 \
  --extra-config=kubelet.cgroup-driver=systemd \
  --memory=8192 \
  --cpus=4

# Verificar status do cluster
kubectl get nodes
kubectl get pods -A
```

---

## 4. Instalar Prometheus Operator CRDs

**CRÍTICO**: Instalar antes do Kepler para evitar erro de ServiceMonitor

```bash
# Instalar CRDs do Prometheus Operator
kubectl apply -f https://raw.githubusercontent.com/prometheus-operator/prometheus-operator/v0.70.0/example/prometheus-operator-crd/monitoring.coreos.com_servicemonitors.yaml

kubectl apply -f https://raw.githubusercontent.com/prometheus-operator/prometheus-operator/v0.70.0/example/prometheus-operator-crd/monitoring.coreos.com_prometheusrules.yaml

# Verificar CRDs instalados
kubectl get crd | grep monitoring
```

---

## 5. Instalar Stack Kepler + Prometheus + Grafana

```bash
# Navegar até o diretório dos containers
cd /home/anderson/dev/oai_isolation/containers

# Dar permissão de execução ao script
chmod +x install-kepler-stack.sh

# Executar instalação automatizada
./install-kepler-stack.sh
```

O script irá:
- Criar namespaces `kepler` e `monitoring`
- Instalar Kepler via Helm no namespace `kepler`
- Instalar Prometheus via Helm no namespace `monitoring`
- Instalar Grafana via Helm no namespace `monitoring`
- Configurar datasource Prometheus no Grafana
- Aguardar todos os pods estarem prontos

---

## 6. Configurar Port Forwarding

```bash
# Grafana (porta 3000)
kubectl port-forward -n monitoring svc/grafana 3000:80 >/dev/null 2>&1 &

# Prometheus (porta 9091)
kubectl port-forward -n monitoring svc/prometheus-server 9091:80 >/dev/null 2>&1 &

# Kepler (porta 9102)
kubectl port-forward -n kepler svc/kepler 9102:9102 >/dev/null 2>&1 &
```

---

## 7. Acessar Serviços

### Grafana
- **URL**: http://localhost:3000
- **Usuário**: `admin`
- **Senha**: Obter com:
  ```bash
  kubectl get secret --namespace monitoring grafana -o jsonpath="{.data.admin-password}" | base64 --decode ; echo
  ```
  Ou usar senha configurada: `admin123`

### Prometheus
- **URL**: http://localhost:9091

### Kepler Metrics
- **URL**: http://localhost:9102/metrics

---

## 8. Verificar Métricas

```bash
# Verificar métricas do Kepler
curl -s http://localhost:9102/metrics | grep kepler_container | head -10

# Verificar targets do Prometheus
curl -s http://localhost:9091/api/v1/targets | jq '.data.activeTargets[] | select(.labels.job=="kepler") | {state: .health, endpoint: .scrapeUrl}'

# Verificar pods
kubectl get pods -n kepler
kubectl get pods -n monitoring
```

---

## 9. Importar Dashboard do Kepler no Grafana

```bash
# Baixar dashboard oficial
curl -s https://raw.githubusercontent.com/sustainable-computing-io/kepler/main/grafana-dashboards/Kepler-Exporter.json -o /tmp/kepler-dashboard.json

# Importar no Grafana via API
curl -s -u admin:admin123 -X POST http://localhost:3000/api/dashboards/db \
  -H "Content-Type: application/json" \
  -d @/tmp/kepler-dashboard.json | jq
```

Ou importar manualmente:
1. Acessar Grafana em http://localhost:3000
2. Menu lateral → Dashboards → Import
3. Upload do arquivo JSON ou colar conteúdo
4. Selecionar datasource Prometheus
5. Clicar em Import

---

## 10. Deploy de Workloads (Opcional)

### Build e Deploy de Imagem com Podman

```bash
# Navegar até diretório do container
cd containers/gnb/ldpc

# Build da imagem
podman build -t gnb-ldpc:latest .

# Carregar no Minikube
minikube image load gnb-ldpc:latest

# Deploy
kubectl apply -f gnb-ldpc-deployment.yaml

# Verificar
kubectl get pods
kubectl logs <pod-name>
```

---

## Troubleshooting

### Cluster não inicia
```bash
# Verificar logs do Minikube
minikube logs

# Verificar status do Docker
sudo systemctl status docker
```

### Métricas Kepler não aparecem
```bash
# Verificar logs do Kepler
kubectl logs -n kepler -l app.kubernetes.io/name=kepler

# Verificar se eBPF está funcionando
kubectl exec -n kepler -it $(kubectl get pod -n kepler -l app.kubernetes.io/name=kepler -o name) -- ls /sys/fs/bpf
```

### Prometheus não coleta métricas
```bash
# Verificar ServiceMonitor
kubectl get servicemonitor -n kepler

# Verificar targets do Prometheus
kubectl port-forward -n monitoring svc/prometheus-server 9090:80
# Acessar http://localhost:9090/targets
```

### Grafana não acessa Prometheus
```bash
# Verificar datasource configurado
curl -s -u admin:admin123 http://localhost:3000/api/datasources | jq

# Testar conectividade interna
kubectl exec -n monitoring $(kubectl get pod -n monitoring -l app.kubernetes.io/name=grafana -o name) -- wget -qO- prometheus-server.monitoring.svc.cluster.local
```

---

## Comandos Úteis

```bash
# Ver todos os recursos
kubectl get all -A

# Parar port-forwards
pkill -f "kubectl port-forward"

# Reiniciar Minikube
minikube stop
minikube start

# Deletar e recriar cluster
minikube delete
# Depois seguir passos 3-6

# Verificar consumo de recursos
kubectl top nodes
kubectl top pods -A

# Logs de um pod específico
kubectl logs -n <namespace> <pod-name> --follow
```

---

## Configurações de Medição

- **Kepler**: Coleta contínua via eBPF
- **Prometheus**: Scrape a cada 5s (configurável no ServiceMonitor)
- **Métricas disponíveis**: 
  - RAPL (energia CPU/DRAM)
  - Contadores PMU
  - Energia por container/pod/namespace
  - Utilização CPU, memória, rede, I/O

---

## Referências

- **Kepler**: https://sustainable-computing.io/
- **Prometheus**: https://prometheus.io/
- **Grafana**: https://grafana.com/
- **Minikube**: https://minikube.sigs.k8s.io/

---

**Data**: 08/01/2026  
**Status**: Testado e funcional no Ubuntu 24.04 com kernel 6.8.0-88-generic

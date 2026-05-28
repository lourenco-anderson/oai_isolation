# Recriar Cluster e Reaplicar Stack

Este documento consolida os comandos do ponto 1 ate a checagem final de saude.

Assumi o cluster `oai-isolation`. Se o nome for diferente, ajuste a variavel `CLUSTER`.

## 1. Backup minimo do estado atual

```bash
REPO=/home/anderson/dev/oai_isolation
TS=$(date +%Y%m%d-%H%M%S)
BK="$REPO/backups/$TS"
mkdir -p "$BK"

kubectl get nodes -o wide > "$BK/nodes.txt"
kubectl get ns -o yaml > "$BK/namespaces.yaml"
kubectl get all -A -o wide > "$BK/all-wide.txt"
kubectl get deploy,ds,svc,cm,secret,pvc,ing,servicemonitor -A -o yaml > "$BK/cluster-state.yaml"
kubectl get storageclass -o yaml > "$BK/storageclasses.yaml"
kubectl get pvc -A -o yaml > "$BK/pvcs.yaml"
helm ls -A > "$BK/helm-releases.txt"
```

## 2. Derrubar e recriar o cluster

```bash
CLUSTER=oai-isolation

kind get clusters | while read -r c; do
  kind delete cluster --name "$c"
done

kind create cluster --name "$CLUSTER"

kubectl config use-context "kind-$CLUSTER"
kubectl wait --for=condition=Ready node --all --timeout=180s
```

## 3. Reaplicar a base do stack

```bash
kubectl create namespace kepler --dry-run=client -o yaml | kubectl apply -f -
kubectl create namespace monitoring --dry-run=client -o yaml | kubectl apply -f -

kubectl apply -f https://raw.githubusercontent.com/prometheus-operator/prometheus-operator/main/example/prometheus-operator-crd/monitoring.coreos.com_servicemonitors.yaml
kubectl apply -f https://raw.githubusercontent.com/prometheus-operator/prometheus-operator/main/example/prometheus-operator-crd/monitoring.coreos.com_prometheusrules.yaml

helm repo add prometheus-community https://prometheus-community.github.io/helm-charts
helm repo add grafana https://grafana.github.io/helm-charts
helm repo add kepler https://sustainable-computing-io.github.io/kepler-helm-chart
helm repo update
```

## 4. Reinstalar o monitoramento

```bash
cd /home/anderson/dev/oai_isolation

helm upgrade --install prometheus prometheus-community/prometheus -n monitoring -f experimental-cluster-setup/prom-values.yaml
kubectl apply -f k8s-manifests/kepler-servicemonitor.yaml
helm upgrade --install kepler kepler/kepler -n kepler -f experimental-cluster-setup/kepler-values.yaml

kubectl create configmap kepler-dashboard -n monitoring --from-file=k8s-manifests/Kepler\ Exporter\ Dashboard-1767693829895.json --dry-run=client -o yaml | kubectl apply -f -
helm upgrade --install grafana grafana/grafana -n monitoring -f experimental-cluster-setup/grafana-values.yaml
```

## 5. Rebuild e carregar as imagens locais no KIND

```bash
REPO=/home/anderson/dev/oai_isolation
CLUSTER=oai-isolation

for dockerfile in "$REPO"/containers/gnb/*/Dockerfile; do
  component=$(basename "$(dirname "$dockerfile")")
  docker build -t "localhost/oai-gnb-${component}:latest" -f "$dockerfile" "$REPO"
  kind load docker-image "localhost/oai-gnb-${component}:latest" --name "$CLUSTER"
done

for dockerfile in "$REPO"/containers/ue/*/Dockerfile; do
  component=$(basename "$(dirname "$dockerfile")")
  docker build -t "oai-ue-${component}:latest" -f "$dockerfile" "$REPO"
  kind load docker-image "oai-ue-${component}:latest" --name "$CLUSTER"
done
```

## 6. Reaplicar os workloads

```bash
kubectl apply -f k8s-manifests/gnb-deployments.yaml
kubectl apply -f k8s-manifests/ue-deployments.yaml

kubectl wait --for=condition=available deployment -l component=gnb -n default --timeout=300s
kubectl wait --for=condition=available deployment -l component=ue -n default --timeout=300s
kubectl get pods -A -o wide
```

## 7. Checagem final de saude

```bash
kubectl -n local-path-storage get pods -o wide
kubectl -n monitoring get pods,pvc -o wide
kubectl -n kepler get pods -o wide

kubectl run --rm -i --restart=Never curl --image=curlimages/curl -- sh -c "curl -k -sS -I --max-time 5 https://10.96.0.1:443 || echo CURL_FAIL"

bash check-metrics.sh
```

## Observacoes

- O backup e util para preservar o estado antes da recriacao do cluster.
- As imagens do gNB usam `localhost/oai-gnb-*` porque os manifests estao configurados para `imagePullPolicy: Never`.
- Se o contexto do kubectl nao for `kind-oai-isolation`, ajuste a variavel `CLUSTER`.
#!/bin/bash
# Deploy OAI functions to Kubernetes cluster

set -e

NAMESPACE="oai-functions"
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Deploying OAI Functions to Kubernetes${NC}"
echo -e "${BLUE}========================================${NC}"

# Check if kubectl is available
if ! command -v kubectl &> /dev/null; then
    echo -e "${YELLOW}kubectl not found. Please install kubectl first.${NC}"
    exit 1
fi

# Check cluster connectivity
echo -e "${BLUE}[1/5]${NC} Checking cluster connectivity..."
if kubectl cluster-info &> /dev/null; then
    echo -e "${GREEN}[✓]${NC} Connected to cluster"
else
    echo -e "${YELLOW}Cannot connect to cluster. Make sure kubectl is configured.${NC}"
    exit 1
fi

# Create namespace
echo -e "${BLUE}[2/5]${NC} Creating namespace..."
kubectl apply -f kubernetes/namespace.yaml
echo -e "${GREEN}[✓]${NC} Namespace ready"

# Apply ConfigMap
echo -e "${BLUE}[3/5]${NC} Applying ConfigMap..."
kubectl apply -f kubernetes/configmap.yaml
echo -e "${GREEN}[✓]${NC} ConfigMap applied"

# Deploy services
echo -e "${BLUE}[4/5]${NC} Deploying services..."
for deployment in kubernetes/deployments/*.yaml; do
    func_name=$(basename $deployment .yaml)
    echo -e "  Deploying ${func_name}..."
    kubectl apply -f $deployment
    kubectl apply -f kubernetes/services/${func_name}.yaml
done
echo -e "${GREEN}[✓]${NC} All services deployed"

# Apply Ingress
echo -e "${BLUE}[5/5]${NC} Configuring Ingress..."
kubectl apply -f kubernetes/ingress.yaml
echo -e "${GREEN}[✓]${NC} Ingress configured"

echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${GREEN}Deployment completed successfully!${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo "Check deployment status:"
echo "  kubectl get pods -n ${NAMESPACE}"
echo "  kubectl get svc -n ${NAMESPACE}"
echo ""
echo "Access services via Ingress:"
echo "  Add to /etc/hosts: <ingress-ip> oai-functions.local"
echo "  curl http://oai-functions.local/scramble -X POST"

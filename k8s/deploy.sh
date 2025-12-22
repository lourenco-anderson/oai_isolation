#!/bin/bash

# Script para fazer deploy dos containers no Kubernetes
# Uso: ./deploy.sh [namespace] [image-registry]

set -e

NAMESPACE=${1:-oai-isolation}
REGISTRY=${2:-localhost:5000}
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=========================================="
echo "Deploying OAI Isolation to Kubernetes"
echo "=========================================="
echo "Namespace: $NAMESPACE"
echo "Registry: $REGISTRY"
echo ""

# Cores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Verificar se kubectl está disponível
if ! command -v kubectl &> /dev/null; then
    echo -e "${RED}✗ kubectl not found. Please install kubectl.${NC}"
    exit 1
fi

# Verificar conectividade ao cluster
echo -e "${YELLOW}Checking Kubernetes cluster...${NC}"
if ! kubectl cluster-info &> /dev/null; then
    echo -e "${RED}✗ Cannot connect to Kubernetes cluster${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Connected to cluster${NC}"
echo ""

# Verificar se Kustomize está disponível e preparar comando com permissões de carga
if command -v kustomize &> /dev/null; then
    KUSTOMIZE_BUILD_CMD=(kustomize build --load-restrictor=LoadRestrictionsNone "$SCRIPT_DIR")
else
    echo -e "${YELLOW}Kustomize not found, attempting with kubectl kustomize support...${NC}"
    KUSTOMIZE_BUILD_CMD=(kubectl kustomize --load-restrictor=LoadRestrictionsNone "$SCRIPT_DIR")
fi

# Deploy usando Kustomize
echo -e "${YELLOW}Applying Kustomize manifests...${NC}"
if eval "${KUSTOMIZE_BUILD_CMD[@]}" | kubectl apply -f -; then
    echo -e "${GREEN}✓ Successfully deployed manifests${NC}"
else
    echo -e "${RED}✗ Failed to deploy manifests${NC}"
    exit 1
fi

echo ""
echo -e "${YELLOW}Checking deployment status...${NC}"
sleep 2

# Aguardar e listar os deployments
kubectl get deployments -n "$NAMESPACE" 2>/dev/null || echo "Waiting for deployments..."
sleep 2

# Mostrar status detalhado
echo ""
echo -e "${GREEN}=========================================="
echo "✓ Deployment completed!"
echo "==========================================${NC}"
echo ""
echo "To check pod status:"
echo "  kubectl get pods -n $NAMESPACE"
echo ""
echo "To check services:"
echo "  kubectl get svc -n $NAMESPACE"
echo ""
echo "To view logs of a pod:"
echo "  kubectl logs -n $NAMESPACE <pod-name>"
echo ""
echo "To delete deployment:"
echo "  kubectl delete namespace $NAMESPACE"

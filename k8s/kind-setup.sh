#!/bin/bash

# Script para gerenciar o cluster Kubernetes com Kind
# Uso: ./kind-setup.sh [create|delete|status]

set -e

ACTION=${1:-status}
CLUSTER_NAME="oai-isolation-cluster"

# Cores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}=========================================="
echo "OAI Isolation Kind Cluster Manager"
echo "==========================================${NC}"
echo ""

# Verificar se Kind está instalado
if ! command -v kind &> /dev/null; then
    echo -e "${RED}✗ Kind not found. Installing...${NC}"
    curl -Lo ./kind https://kind.sigs.k8s.io/dl/v0.27.0/kind-linux-amd64
    chmod +x ./kind
    sudo mv ./kind /usr/local/bin/kind
fi

# Verificar se kubectl está instalado
if ! command -v kubectl &> /dev/null; then
    echo -e "${RED}✗ kubectl not found. Installing...${NC}"
    curl -LO "https://dl.k8s.io/release/$(curl -L -s https://dl.k8s.io/release/stable.txt)/bin/linux/amd64/kubectl"
    chmod +x kubectl
    sudo mv kubectl /usr/local/bin/
fi

case "$ACTION" in
    create)
        echo -e "${YELLOW}Creating Kind cluster: $CLUSTER_NAME${NC}"
        
        # Criar config para Kind cluster com registry local
        cat > /tmp/kind-config.yaml <<EOF
kind: Cluster
apiVersion: kind.x-k8s.io/v1alpha4
name: $CLUSTER_NAME
nodes:
  - role: control-plane
    ports:
      - containerPort: 80
        hostPort: 80
        protocol: TCP
      - containerPort: 443
        hostPort: 443
        protocol: TCP
  - role: worker
  - role: worker
EOF
        
        kind create cluster --config /tmp/kind-config.yaml
        
        echo -e "${GREEN}✓ Cluster created successfully${NC}"
        echo ""
        echo -e "${YELLOW}Setting kubectl context...${NC}"
        kubectl cluster-info --context kind-$CLUSTER_NAME
        echo -e "${GREEN}✓ Context configured${NC}"
        ;;
        
    delete)
        echo -e "${YELLOW}Deleting Kind cluster: $CLUSTER_NAME${NC}"
        kind delete cluster --name $CLUSTER_NAME
        echo -e "${GREEN}✓ Cluster deleted${NC}"
        ;;
        
    status)
        echo -e "${YELLOW}Checking Kind cluster status...${NC}"
        
        if kind get clusters | grep -q $CLUSTER_NAME; then
            echo -e "${GREEN}✓ Cluster exists: $CLUSTER_NAME${NC}"
            echo ""
            kubectl cluster-info --context kind-$CLUSTER_NAME
            echo ""
            echo -e "${YELLOW}Cluster nodes:${NC}"
            kubectl get nodes
        else
            echo -e "${RED}✗ Cluster not found: $CLUSTER_NAME${NC}"
            echo "Available clusters:"
            kind get clusters
        fi
        ;;
        
    *)
        echo -e "${RED}Invalid action: $ACTION${NC}"
        echo "Usage: $0 [create|delete|status]"
        exit 1
        ;;
esac

echo ""

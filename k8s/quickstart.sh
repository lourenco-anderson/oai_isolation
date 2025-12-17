#!/bin/bash

# Script de Quick Start para OAI Isolation + Kubernetes
# Este script automatiza todos os passos iniciais

set -e

# Cores
BLUE='\033[0;34m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${BLUE}"
echo "╔════════════════════════════════════════════════════════════╗"
echo "║   OAI Isolation + Kubernetes Quick Start                   ║"
echo "║                                                            ║"
echo "║   Este script automatiza:                                  ║"
echo "║   1. Criar cluster Kind local                              ║"
echo "║   2. Build das imagens Docker                              ║"
echo "║   3. Deploy no Kubernetes                                  ║"
echo "║   4. Verificar status                                      ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo -e "${NC}"
echo ""

# Verificar pré-requisitos
echo -e "${YELLOW}[1/5] Verificando pré-requisitos...${NC}"

if ! command -v docker &> /dev/null; then
    echo -e "${RED}✗ Docker não encontrado${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Docker${NC}"

if ! command -v kubectl &> /dev/null; then
    echo -e "${RED}✗ kubectl não encontrado${NC}"
    echo "  Instalando kubectl..."
    curl -LO "https://dl.k8s.io/release/$(curl -L -s https://dl.k8s.io/release/stable.txt)/bin/linux/amd64/kubectl"
    chmod +x kubectl
    sudo mv kubectl /usr/local/bin/
fi
echo -e "${GREEN}✓ kubectl${NC}"

if ! command -v kind &> /dev/null; then
    echo -e "${RED}✗ Kind não encontrado${NC}"
    echo "  Instalando Kind..."
    curl -Lo ./kind https://kind.sigs.k8s.io/dl/v0.27.0/kind-linux-amd64
    chmod +x ./kind
    sudo mv ./kind /usr/local/bin/kind
fi
echo -e "${GREEN}✓ Kind${NC}"

echo ""

# Criar cluster Kind
echo -e "${YELLOW}[2/5] Criando cluster Kind...${NC}"
CLUSTER_NAME="oai-isolation-cluster"

if kind get clusters | grep -q "^${CLUSTER_NAME}$"; then
    echo -e "${YELLOW}! Cluster já existe, pulando...${NC}"
else
    ./kind-setup.sh create
fi
echo -e "${GREEN}✓ Cluster Kind pronto${NC}"

echo ""

# Build das imagens
echo -e "${YELLOW}[3/5] Building das imagens Docker (isso pode demorar...)${NC}"
./build-images.sh
echo -e "${GREEN}✓ Imagens built com sucesso${NC}"

echo ""

# Deploy no Kubernetes
echo -e "${YELLOW}[4/5] Deploy no Kubernetes...${NC}"
./deploy.sh oai-isolation
echo -e "${GREEN}✓ Deploy realizado${NC}"

echo ""

# Aguardar pods ficarem prontos
echo -e "${YELLOW}[5/5] Aguardando pods ficarem prontos (máx 60s)...${NC}"
kubectl wait --for=condition=ready pod \
    -l project=oai-isolation \
    -n oai-isolation \
    --timeout=60s 2>/dev/null || echo "  (Alguns pods podem ainda estar iniciando)"

echo ""
echo -e "${BLUE}"
echo "╔════════════════════════════════════════════════════════════╗"
echo "║   ✓ Setup concluído com sucesso!                          ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo -e "${NC}"
echo ""
echo -e "${GREEN}Próximos passos:${NC}"
echo ""
echo "1. Verificar status dos pods:"
echo "   ${YELLOW}kubectl get pods -n oai-isolation${NC}"
echo ""
echo "2. Ver logs de um serviço:"
echo "   ${YELLOW}kubectl logs -n oai-isolation <pod-name>${NC}"
echo ""
echo "3. Monitorar em tempo real:"
echo "   ${YELLOW}./monitor.sh${NC}"
echo ""
echo "4. Usar o Makefile para gerenciar:"
echo "   ${YELLOW}make help${NC}"
echo ""
echo "5. Remover deployment (quando necessário):"
echo "   ${YELLOW}make clean${NC}"
echo ""
echo -e "${BLUE}Para mais informações, veja k8s/README.md${NC}"

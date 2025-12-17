#!/bin/bash

# Script para fazer build de todas as imagens Docker do projeto
# Uso: ./build-images.sh [registry] [tag]

set -e

REGISTRY=${1:-localhost:5000}
TAG=${2:-latest}
REPO_ROOT=$(cd "$(dirname "$0")/../" && pwd)

echo "=========================================="
echo "Building OAI Isolation Docker Images"
echo "=========================================="
echo "Registry: $REGISTRY"
echo "Tag: $TAG"
echo "Repo Root: $REPO_ROOT"
echo ""

# Cores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

build_image() {
    local dockerfile_path=$1
    local image_name=$2
    local full_image="${REGISTRY}/${image_name}:${TAG}"
    local abs_dockerfile_path="${REPO_ROOT}/${dockerfile_path}"
    
    echo -e "${YELLOW}Building: $full_image${NC}"
    docker build -f "$abs_dockerfile_path" -t "$full_image" "$REPO_ROOT"
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ Successfully built: $full_image${NC}"
    else
        echo -e "${RED}✗ Failed to build: $full_image${NC}"
        return 1
    fi
}

# Build GNB images
echo -e "${YELLOW}Building GNB images...${NC}"
build_image "containers/gnb/crc/Dockerfile" "oai-isolation/gnb-crc"
build_image "containers/gnb/layer_map/Dockerfile" "oai-isolation/gnb-layer-map"
build_image "containers/gnb/ldpc/Dockerfile" "oai-isolation/gnb-ldpc"
build_image "containers/gnb/modulation/Dockerfile" "oai-isolation/gnb-modulation"
build_image "containers/gnb/ofdm_mod/Dockerfile" "oai-isolation/gnb-ofdm-mod"
build_image "containers/gnb/precoding/Dockerfile" "oai-isolation/gnb-precoding"
build_image "containers/gnb/scramble/Dockerfile" "oai-isolation/gnb-scramble"

echo ""

# Build UE images
echo -e "${YELLOW}Building UE images...${NC}"
build_image "containers/ue/ch_est/Dockerfile" "oai-isolation/ue-ch-est"
build_image "containers/ue/ch_mmse/Dockerfile" "oai-isolation/ue-ch-mmse"
build_image "containers/ue/check_crc/Dockerfile" "oai-isolation/ue-check-crc"
build_image "containers/ue/descrambling/Dockerfile" "oai-isolation/ue-descrambling"
build_image "containers/ue/layer_demap/Dockerfile" "oai-isolation/ue-layer-demap"
build_image "containers/ue/ldpc_dec/Dockerfile" "oai-isolation/ue-ldpc-dec"
build_image "containers/ue/ofdm_demod/Dockerfile" "oai-isolation/ue-ofdm-demod"
build_image "containers/ue/soft_demod/Dockerfile" "oai-isolation/ue-soft-demod"

echo ""
echo -e "${GREEN}=========================================="
echo "✓ All images built successfully!"
echo "==========================================${NC}"
echo ""
echo "To push images to registry ($REGISTRY), run:"
echo "  for i in gnb-crc gnb-layer-map gnb-ldpc gnb-modulation gnb-ofdm-mod gnb-precoding gnb-scramble ue-ch-est ue-ch-mmse ue-check-crc ue-descrambling ue-layer-demap ue-ldpc-dec ue-ofdm-demod ue-soft-demod; do docker push $REGISTRY/oai-isolation/$i:$TAG; done"

#!/bin/bash
# Build all Docker images for OAI functions

set -e

# Configuration
DOCKER_REGISTRY="${DOCKER_REGISTRY:-docker.io/your-username}"
VERSION="${VERSION:-latest}"
BUILD_PARALLEL="${BUILD_PARALLEL:-4}"

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m'

# Functions list
FUNCTIONS=(
    "nr_scramble"
    "nr_crc"
    "nr_ofdm_modulation"
    "nr_precoding"
    "nr_modulation_test"
    "nr_layermapping"
    "nr_ldpc"
    "nr_ofdm_demo"
    "nr_ch_estimation"
    "nr_descrambling"
    "nr_layer_demapping_test"
    "nr_crc_check"
    "nr_soft_demod"
    "nr_mmse_eq"
    "nr_ldpc_dec"
)

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Building OAI Function Docker Images${NC}"
echo -e "${BLUE}========================================${NC}"
echo -e "Registry: ${GREEN}${DOCKER_REGISTRY}${NC}"
echo -e "Version: ${GREEN}${VERSION}${NC}"
echo ""

# Build function
build_image() {
    local func_name=$1
    local image_tag="${DOCKER_REGISTRY}/${func_name}:${VERSION}"
    
    echo -e "${BLUE}[Building]${NC} ${func_name}..."
    
    if docker build \
        -f containers/services/${func_name}/Dockerfile \
        -t ${image_tag} \
        --build-arg BUILDKIT_INLINE_CACHE=1 \
        . > /tmp/build_${func_name}.log 2>&1; then
        echo -e "${GREEN}[✓]${NC} ${func_name} built successfully"
        return 0
    else
        echo -e "${RED}[✗]${NC} ${func_name} build failed (see /tmp/build_${func_name}.log)"
        return 1
    fi
}

# Build all images
success_count=0
fail_count=0

for func in "${FUNCTIONS[@]}"; do
    if build_image "$func"; then
        ((success_count++))
    else
        ((fail_count++))
    fi
done

echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Build Summary${NC}"
echo -e "${BLUE}========================================${NC}"
echo -e "${GREEN}Successful:${NC} ${success_count}/${#FUNCTIONS[@]}"
echo -e "${RED}Failed:${NC} ${fail_count}/${#FUNCTIONS[@]}"

if [ $fail_count -eq 0 ]; then
    echo ""
    echo -e "${GREEN}All images built successfully!${NC}"
    echo ""
    echo "Next steps:"
    echo "  1. Push images: ./scripts/push-images.sh"
    echo "  2. Deploy to K8s: ./scripts/deploy-to-k8s.sh"
    exit 0
else
    echo -e "${RED}Some builds failed. Check logs in /tmp/build_*.log${NC}"
    exit 1
fi

#!/bin/bash
# Push all Docker images to registry

set -e

DOCKER_REGISTRY="${DOCKER_REGISTRY:-docker.io/your-username}"
VERSION="${VERSION:-latest}"

GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

FUNCTIONS=(
    "nr_scramble" "nr_crc" "nr_ofdm_modulation" "nr_precoding"
    "nr_modulation_test" "nr_layermapping" "nr_ldpc" "nr_ofdm_demo"
    "nr_ch_estimation" "nr_descrambling" "nr_layer_demapping_test"
    "nr_crc_check" "nr_soft_demod" "nr_mmse_eq" "nr_ldpc_dec"
)

echo -e "${BLUE}Pushing images to ${DOCKER_REGISTRY}${NC}"
echo ""

for func in "${FUNCTIONS[@]}"; do
    image_tag="${DOCKER_REGISTRY}/${func}:${VERSION}"
    echo -e "${BLUE}[Pushing]${NC} ${func}..."
    docker push ${image_tag}
    echo -e "${GREEN}[✓]${NC} ${func} pushed"
done

echo ""
echo -e "${GREEN}All images pushed successfully!${NC}"

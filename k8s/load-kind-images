#!/usr/bin/env bash
set -euo pipefail

# Load local Docker images into a Kind cluster with unified naming
# Usage:
#   ./k8s/load-kind-images.sh [--cluster <name>] [--only gnb|ue|all]
#
# - Retags local images built as oai-gnb-<comp>:latest and oai-ue-<comp>:latest
#   to the scheme used by Kubernetes manifests: oai-isolation:<component>
# - Loads the retagged images into the target Kind cluster.

CLUSTER=""
SCOPE="all"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --cluster)
      CLUSTER="$2"; shift 2 ;;
    --only)
      SCOPE="$2"; shift 2 ;;
    -h|--help)
      echo "Usage: $0 [--cluster <name>] [--only gnb|ue|all]"; exit 0 ;;
    *)
      echo "Unknown argument: $1"; exit 1 ;;
  esac
done

# Auto-detect Kind cluster from current kube context if not provided
if [[ -z "$CLUSTER" ]]; then
  ctx=$(kubectl config current-context 2>/dev/null || true)
  if [[ "$ctx" =~ ^kind-(.+)$ ]]; then
    CLUSTER="${BASH_REMATCH[1]}"
  else
    # fallback default
    CLUSTER="kepler-cluster"
  fi
fi

echo "Cluster: $CLUSTER"
echo "Scope:   $SCOPE"

retag_and_load() {
  local src_repo="$1"      # preferred local name, e.g., oai-gnb-layer-map
  local dst_tag="$2"       # final tag used by k8s, e.g., oai-isolation:gnb-layer-map
  local alt_repo="$3"      # optional alternative source, e.g., localhost:5000/oai-isolation/gnb-layer-map

  local found=false
  if docker images --format '{{.Repository}}:{{.Tag}}' | grep -q "^${src_repo}:latest$"; then
    docker tag "${src_repo}:latest" "$dst_tag" && found=true
    echo "Tagged ${src_repo}:latest -> ${dst_tag}"
  elif [[ -n "$alt_repo" ]] && docker images --format '{{.Repository}}:{{.Tag}}' | grep -q "^${alt_repo}:latest$"; then
    docker tag "${alt_repo}:latest" "$dst_tag" && found=true
    echo "Tagged ${alt_repo}:latest -> ${dst_tag}"
  fi

  if [[ "$found" == true ]]; then
    echo "Loading ${dst_tag} into kind ${CLUSTER}..."
    kind load docker-image "$dst_tag" --name "$CLUSTER"
  else
    echo "WARN: source image not found for ${dst_tag} (tried ${src_repo}:latest${alt_repo:+ and ${alt_repo}:latest})"
  fi
}

GNB_COMPONENTS=(
  crc layer-map ldpc modulation ofdm-mod precoding scramble
)
UE_COMPONENTS=(
  ch-est ch-mmse check-crc descrambling layer-demap ldpc-dec ofdm-demod soft-demod
)

if [[ "$SCOPE" == "all" || "$SCOPE" == "gnb" ]]; then
  for c in "${GNB_COMPONENTS[@]}"; do
    local_repo_name="oai-gnb-${c}"
    alt_repo_name="localhost:5000/oai-isolation/gnb-${c}"
    retag_and_load "$local_repo_name" "oai-isolation:gnb-${c}" "$alt_repo_name"
  done
fi

if [[ "$SCOPE" == "all" || "$SCOPE" == "ue" ]]; then
  for c in "${UE_COMPONENTS[@]}"; do
    local_repo_name="oai-ue-${c}"
    alt_repo_name="localhost:5000/oai-isolation/ue-${c}"
    retag_and_load "$local_repo_name" "oai-isolation:ue-${c}" "$alt_repo_name"
  done
fi

echo "Done."

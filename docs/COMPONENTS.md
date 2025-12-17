# Componentes OAI Isolation - Tabela Central

## 📊 Visão Geral

| Layer | Componentes | Total | Portas | Função |
|-------|-------------|-------|--------|--------|
| **GNB (Transmitter)** | CRC → Layer-Map → LDPC → Modulation → OFDM-Mod → Precoding → Scramble | 7 | 8080-8086 | Processamento TX 5G |
| **UE (Receiver)** | OFDM-Demod → Soft-Demod → Layer-Demap → LDPC-Dec → Descrambling → Ch-Est/Ch-MMSE → CRC-Check | 8 | 9080-9087 | Processamento RX 5G |

---

## 🔴 GNB (7 Componentes - gNodeB / Transmitter)

| # | Componente | Container | Porta | Dockerfile | Manifest | Memória | CPU | Função |
|---|-----------|-----------|-------|-----------|----------|---------|-----|--------|
| 1 | **gnb-crc** | `oai-isolation:gnb-crc` | 8080 | `containers/gnb/crc/Dockerfile` | `k8s/manifests/deployments/gnb/gnb-crc.yaml` | 256 Mi | 250 m | CRC Encoding/Decoding |
| 2 | **gnb-layer-map** | `oai-isolation:gnb-layer-map` | 8081 | `containers/gnb/layer_map/Dockerfile` | `k8s/manifests/deployments/gnb/gnb-layer-map.yaml` | 512 Mi | 500 m | Layer Mapping |
| 3 | **gnb-ldpc** | `oai-isolation:gnb-ldpc` | 8082 | `containers/gnb/ldpc/Dockerfile` | `k8s/manifests/deployments/gnb/gnb-ldpc.yaml` | 512 Mi | 500 m | LDPC Encoding |
| 4 | **gnb-modulation** | `oai-isolation:gnb-modulation` | 8083 | `containers/gnb/modulation/Dockerfile` | `k8s/manifests/deployments/gnb/gnb-modulation.yaml` | 512 Mi | 500 m | 256-QAM Modulation |
| 5 | **gnb-ofdm-mod** | `oai-isolation:gnb-ofdm-mod` | 8084 | `containers/gnb/ofdm_mod/Dockerfile` | `k8s/manifests/deployments/gnb/gnb-ofdm-mod.yaml` | 512 Mi | 500 m | OFDM Modulation |
| 6 | **gnb-precoding** | `oai-isolation:gnb-precoding` | 8085 | `containers/gnb/precoding/Dockerfile` | `k8s/manifests/deployments/gnb/gnb-precoding.yaml` | 512 Mi | 500 m | Precoding (MIMO) |
| 7 | **gnb-scramble** | `oai-isolation:gnb-scramble` | 8086 | `containers/gnb/scramble/Dockerfile` | `k8s/manifests/deployments/gnb/gnb-scramble.yaml` | 512 Mi | 500 m | Scrambling |

---

## 🔵 UE (8 Componentes - User Equipment / Receiver)

| # | Componente | Container | Porta | Dockerfile | Manifest | Memória | CPU | Função |
|---|-----------|-----------|-------|-----------|----------|---------|-----|--------|
| 1 | **ue-ofdm-demod** | `oai-isolation:ue-ofdm-demod` | 9086 | `containers/ue/ofdm_demod/Dockerfile` | `k8s/manifests/deployments/ue/ue-ofdm-demod-deployment.yaml` | 512 Mi | 500 m | OFDM Demodulation |
| 2 | **ue-soft-demod** | `oai-isolation:ue-soft-demod` | 9087 | `containers/ue/soft_demod/Dockerfile` | `k8s/manifests/deployments/ue/ue-soft-demod-deployment.yaml` | 512 Mi | 500 m | Soft Demodulation |
| 3 | **ue-layer-demap** | `oai-isolation:ue-layer-demap` | 9084 | `containers/ue/layer_demap/Dockerfile` | `k8s/manifests/deployments/ue/ue-layer-demap-deployment.yaml` | 512 Mi | 500 m | Layer Demapping |
| 4 | **ue-ldpc-dec** | `oai-isolation:ue-ldpc-dec` | 9085 | `containers/ue/ldpc_dec/Dockerfile` | `k8s/manifests/deployments/ue/ue-ldpc-dec-deployment.yaml` | 512 Mi | 500 m | LDPC Decoding |
| 5 | **ue-descrambling** | `oai-isolation:ue-descrambling` | 9083 | `containers/ue/descrambling/Dockerfile` | `k8s/manifests/deployments/ue/ue-descrambling-deployment.yaml` | 512 Mi | 500 m | Descrambling |
| 6 | **ue-ch-est** | `oai-isolation:ue-ch-est` | 9080 | `containers/ue/ch_est/Dockerfile` | `k8s/manifests/deployments/ue/ue-ch-est-deployment.yaml` | 512 Mi | 500 m | Channel Estimation |
| 7 | **ue-ch-mmse** | `oai-isolation:ue-ch-mmse` | 9081 | `containers/ue/ch_mmse/Dockerfile` | `k8s/manifests/deployments/ue/ue-ch-mmse-deployment.yaml` | 512 Mi | 500 m | MMSE Channel Estimation |
| 8 | **ue-check-crc** | `oai-isolation:ue-check-crc` | 9082 | `containers/ue/check_crc/Dockerfile` | `k8s/manifests/deployments/ue/ue-check-crc-deployment.yaml` | 256 Mi | 250 m | CRC Check |

---

## 📍 Localizações Centrais

### Dockerfiles
```
containers/
├── gnb/
│   ├── crc/Dockerfile
│   ├── layer_map/Dockerfile
│   ├── ldpc/Dockerfile
│   ├── modulation/Dockerfile
│   ├── ofdm_mod/Dockerfile
│   ├── precoding/Dockerfile
│   └── scramble/Dockerfile
└── ue/
    ├── ch_est/Dockerfile
    ├── ch_mmse/Dockerfile
    ├── check_crc/Dockerfile
    ├── descrambling/Dockerfile
    ├── layer_demap/Dockerfile
    ├── ldpc_dec/Dockerfile
    ├── ofdm_demod/Dockerfile
    └── soft_demod/Dockerfile
```

### Kubernetes Manifests (Consolidados)
```
k8s/manifests/
├── namespace.yaml
├── services/
│   ├── gnb-services.yaml
│   └── ue-services.yaml
├── deployments/
│   ├── gnb/
│   │   ├── gnb-crc.yaml
│   │   ├── gnb-layer-map.yaml
│   │   ├── gnb-ldpc.yaml
│   │   ├── gnb-modulation.yaml
│   │   ├── gnb-ofdm-mod.yaml
│   │   ├── gnb-precoding.yaml
│   │   └── gnb-scramble.yaml
│   └── ue/
│       ├── ue-ch-est-deployment.yaml
│       ├── ue-ch-mmse-deployment.yaml
│       ├── ue-check-crc-deployment.yaml
│       ├── ue-descrambling-deployment.yaml
│       ├── ue-layer-demap-deployment.yaml
│       ├── ue-ldpc-dec-deployment.yaml
│       ├── ue-ofdm-demod-deployment.yaml
│       └── ue-soft-demod-deployment.yaml
└── kustomization.yaml
```

### Build & Deploy Scripts
```
k8s/scripts/
├── build-images.sh          # Build todas as imagens (oai-isolation:*)
├── load-kind-images.sh      # Retag + carregar no Kind
├── deploy.sh                # Deploy no k8s
├── kind-setup.sh            # Setup cluster Kind
├── monitor.sh               # Monitorar pods
├── install-monitoring-stack.sh  # Install Kepler+Prometheus+Grafana
└── energy-queries.sh        # Queries de energia
```

---

## 🚀 Quick Reference

### Build de Todas as Imagens
```bash
cd /home/anderson/dev/oai_isolation
k8s/scripts/build-images.sh localhost:5000 latest
```

### Deploy no Kubernetes
```bash
k8s/scripts/load-kind-images.sh --only gnb
k8s/scripts/load-kind-images.sh --only ue
k8s/scripts/deploy.sh oai-isolation
```

### Verificar Status
```bash
kubectl get pods -n oai-isolation -o wide
kubectl get svc -n oai-isolation
```

### Monitorar com Kepler
```bash
k8s/scripts/install-monitoring-stack.sh
k8s/scripts/monitor.sh
```

---

## 🔗 Referências Relacionadas

- **Deployment Guide**: `docs/DEPLOYMENT_K8S.md`
- **Monitoring**: `docs/MONITORING_KEPLER.md`
- **Troubleshooting**: `docs/TROUBLESHOOTING.md`
- **Architecture**: `docs/ARCHITECTURE.md`


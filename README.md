# OAI Isolation

Isolamento e análise de componentes do OpenAir para 5G (gNB + UE) em containers Docker e Kubernetes com monitoramento de energia via Kepler.

---

## 🚀 Quick Start

### Opção 1: Docker Compose (Local)
```bash
# Build e run com docker-compose (mais rápido para dev)
docker-compose up --build -d

# Ver logs
docker-compose logs -f gnb-crc

# Parar
docker-compose down
```

### Opção 2: Kubernetes (com Kind)
```bash
# 1. Setup cluster Kind
cd k8s
make kind-create

# 2. Build imagens
make build

# 3. Deploy
make deploy

# 4. Ver status
make status

# 5. Monitorar energia
make install-monitoring
make energy-queries
```

---

## 📚 Documentação Completa

Toda documentação está consolidada em **[`docs/`](docs/README.md)**:

### 📖 Leitura Recomendada
1. **[`docs/COMPONENTS.md`](docs/COMPONENTS.md)** — Tabela central de todos os 15 componentes (7 gNB + 8 UE)
2. **[`docs/DEPLOYMENT_K8S.md`](docs/DEPLOYMENT_K8S.md)** — Guia completo de deploy em Kubernetes
3. **[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)** — Arquitetura, fluxos, escalabilidade
4. **[`docs/MONITORING_KEPLER.md`](docs/MONITORING_KEPLER.md)** — Setup Kepler + Prometheus + Grafana
5. **[`docs/TROUBLESHOOTING.md`](docs/TROUBLESHOOTING.md)** — Problemas comuns

---

## 📦 Estrutura Atual (Reorganizada)

```
.
├── 📚 docs/                          # Documentação centralizada
│   ├── README.md                     # Index de navegação
│   ├── COMPONENTS.md                 # Tabela central (gNB + UE)
│   ├── DEPLOYMENT_K8S.md             # Guia K8s
│   ├── ARCHITECTURE.md               # Arquitetura
│   ├── MONITORING_KEPLER.md          # Kepler setup
│   └── TROUBLESHOOTING.md            # Troubleshooting
│
├── 🐳 containers/                    # Dockerfiles
│   ├── gnb/                          # 7 gNB components
│   └── ue/                           # 8 UE components
│
├── k8s/                              # Kubernetes
│   ├── manifests/                    # Manifests centralizados
│   │   ├── namespace.yaml
│   │   ├── deployments/gnb/          # 7 gNB deployments
│   │   ├── deployments/ue/           # 8 UE deployments
│   │   ├── services/                 # Services (em desenvolvimento)
│   │   └── kustomization.yaml
│   ├── scripts/                      # Automation scripts
│   │   ├── build-images.sh
│   │   ├── load-kind-images.sh
│   │   ├── deploy.sh
│   │   └── ...
│   └── Makefile                      # Automação
│
├── src/                              # Source code
├── build/                            # Build outputs
├── ext/openair/                      # OpenAir (external)
├── docker-compose.yaml               # Local dev
└── README.md                         # Você está aqui
```

---

## 🎯 Componentes

### 🔴 GNB (Transmitter - 7 componentes)
**Pipeline TX**: CRC → Layer-Map → LDPC → Modulation → OFDM-Mod → Precoding → Scramble

| Porta | Função | Container |
|-------|--------|-----------|
| 8080 | CRC Encoding | `oai-isolation:gnb-crc` |
| 8081 | Layer Mapping | `oai-isolation:gnb-layer-map` |
| 8082 | LDPC Encoding | `oai-isolation:gnb-ldpc` |
| 8083 | 256-QAM Modulation | `oai-isolation:gnb-modulation` |
| 8084 | OFDM Modulation | `oai-isolation:gnb-ofdm-mod` |
| 8085 | Precoding (MIMO) | `oai-isolation:gnb-precoding` |
| 8086 | Scrambling | `oai-isolation:gnb-scramble` |

### 🔵 UE (Receiver - 8 componentes)
**Pipeline RX**: OFDM-Demod → Soft-Demod → Layer-Demap → LDPC-Dec → Descrambling → Ch-Est/Ch-MMSE → CRC-Check

| Porta | Função | Container |
|-------|--------|-----------|
| 9086 | OFDM Demodulation | `oai-isolation:ue-ofdm-demod` |
| 9087 | Soft Demodulation | `oai-isolation:ue-soft-demod` |
| 9084 | Layer Demapping | `oai-isolation:ue-layer-demap` |
| 9085 | LDPC Decoding | `oai-isolation:ue-ldpc-dec` |
| 9083 | Descrambling | `oai-isolation:ue-descrambling` |
| 9080 | Channel Estimation | `oai-isolation:ue-ch-est` |
| 9081 | MMSE Channel Est. | `oai-isolation:ue-ch-mmse` |
| 9082 | CRC Check | `oai-isolation:ue-check-crc` |

**Veja [docs/COMPONENTS.md](docs/COMPONENTS.md) para tabela completa**

---

## 🔧 Comandos Essenciais

### Local (Docker Compose)
```bash
docker-compose up --build -d          # Build e run
docker-compose ps                      # Status
docker-compose logs -f gnb-crc        # Logs
docker-compose down                   # Stop
```

### Kubernetes
```bash
cd k8s

make kind-create                      # Criar cluster Kind
make build                            # Build imagens
make deploy                           # Deploy
make status                           # Ver status
make monitor                          # Monitor pods
make install-monitoring               # Install Kepler+Prometheus+Grafana
make energy-queries                   # Queries de energia
make kind-delete                      # Deletar cluster
```

---

## 📊 Monitoramento (Kepler)

Acompanhe consumo de energia em tempo real:

```bash
cd k8s
make install-monitoring               # Setup Kepler+Prometheus+Grafana
make energy-queries --all             # Queries de energia
```

Dashboard: http://localhost:3000 (Grafana)

---

## 🔗 Links Importantes

- **Documentação**: [`docs/README.md`](docs/README.md)
- **Quick Start (local)**: [`QUICK_START.md`](QUICK_START.md)
- **Componentes**: [`docs/COMPONENTS.md`](docs/COMPONENTS.md)
- **Deploy K8s**: [`docs/DEPLOYMENT_K8S.md`](docs/DEPLOYMENT_K8S.md)
- **Troubleshooting**: [`docs/TROUBLESHOOTING.md`](docs/TROUBLESHOOTING.md)

---

## 🏗️ Arquitetura

```
┌─────────────────────────────────────────┐
│            INPUT (Data)                  │
└──────────────────┬──────────────────────┘
                   │
        ┌──────────▼──────────┐
        │  GNB (Transmitter)  │ Portas 8080-8086
        ├─────────────────────┤
        │ • CRC Encoding      │
        │ • Layer Mapping     │
        │ • LDPC Encoding     │
        │ • Modulation 256QAM │
        │ • OFDM Modulation   │
        │ • Precoding (MIMO)  │
        │ • Scrambling        │
        └──────────┬──────────┘
                   │
        ┌──────────▼──────────┐
        │   CHANNEL (PHY)     │
        └──────────┬──────────┘
                   │
        ┌──────────▼──────────┐
        │  UE (Receiver)      │ Portas 9080-9087
        ├─────────────────────┤
        │ • OFDM Demodulation │
        │ • Soft Demodulation │
        │ • Layer Demapping   │
        │ • LDPC Decoding     │
        │ • Descrambling      │
        │ • Ch. Estimation    │
        │ • CRC Check         │
        └──────────┬──────────┘
                   │
        ┌──────────▼──────────┐
        │     OUTPUT (Data)   │
        └─────────────────────┘

Monitor de Energia: Kepler + Prometheus + Grafana
```

**Veja [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) para detalhes completos**

---

## 🤝 Suporte

- **Problemas?** → Veja [`docs/TROUBLESHOOTING.md`](docs/TROUBLESHOOTING.md)
- **Como funciona?** → Leia [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)
- **Qual é a porta?** → Confira [`docs/COMPONENTS.md`](docs/COMPONENTS.md)

---

## 📝 Histórico

- **Reorganização Fase 1**: ✅ Estrutura centralizada (k8s/manifests/, k8s/scripts/)
- **Reorganização Fase 2**: ✅ Documentação centralizada (docs/)
- **Reorganização Fase 3**: ⏳ Services consolidados
- **Reorganização Fase 4**: ⏳ Limpeza final

Veja [`REORGANIZATION_STATUS.md`](REORGANIZATION_STATUS.md) para detalhes completos.

---

## Getting started (Original)

Clone this repo. Run `git submodule init` and `git submodule update` then follow the steps [here](https://gitlab.eurecom.fr/oai/openairinterface5g/-/blob/develop/doc/NR_SA_Tutorial_OAI_nrUE.md).

or just run inside `ext/openair/cmake_targets` (after `git submodule init`)

```bash
./build_oai -I

./build_oai -w USRP --ninja --nrUE --gNB --build-lib "nrscope" -C
```

Build the code:
```bash
cmake -B build
cd build
make
```

## Containers (funções isoladas)

Pré-requisito: binário atualizado na raiz (`build/oai_isolation`) e libs geradas em `ext/openair/cmake_targets/ran_build/build/`.

1) Recompile o binário local:
```bash
cmake -B build && cmake --build build -j"$(nproc)"
```

2) Build da imagem (exemplo ldpc):
```bash
docker build -f containers/gnb/ldpc/Dockerfile -t oai-nr-ldpc .
```

3) Rodar a função default do Dockerfile (ldpc):
```bash
docker run --rm oai-nr-ldpc
```

4) Rodar outra função usando o mesmo binário (via argumento):
```bash
docker run --rm oai-nr-ldpc /app/oai_isolation nr_crc
```

### Mapas de Dockerfiles por função

Contexto de build sempre na raiz do repo (`.`). Cada pasta já tem um Dockerfile com `CMD` configurado:

**gNB**
- `containers/gnb/crc` → `nr_crc`
- `containers/gnb/ldpc` → `nr_ldpc`
- `containers/gnb/modulation` → `nr_modulation`
- `containers/gnb/layer_map` → `nr_layermapping`
- `containers/gnb/ofdm_mod` → `nr_ofdm_mod`
- `containers/gnb/precoding` → `nr_precoding`
- `containers/gnb/scramble` → `nr_scramble`

**UE**
- `containers/ue/ch_est` → `nr_ch_estimation`
- `containers/ue/ch_mmse` → `nr_mmse_eq`
- `containers/ue/check_crc` → `nr_crc_check`
- `containers/ue/descrambling` → `nr_descrambling`
- `containers/ue/layer_demap` → `nr_layer_demapping_test`
- `containers/ue/ldpc_dec` → `nr_ldpc_dec`
- `containers/ue/ofdm_demod` → `nr_ofdm_demo`
- `containers/ue/soft_demod` → `nr_soft_demod`

Exemplo para outra função (precoding):
```bash
docker build -f containers/gnb/precoding/Dockerfile -t oai-precoding .
docker run --rm oai-precoding
```


## My Functions

```bash
./find_my_function < function name >
``` 

## Outputs from OAI
To get the outputs from OAI it is necessary to run the gNB or the UE (depending on the target function) side with:

 ```bash
 gdb --args <oai_launch_nr_command>
 break <target function>
 run 
 ``` 
find `oai_launch_nr_command` [ohere](https://gitlab.eurecom.fr/oai/openairinterface5g/-/blob/develop/doc/NR_SA_Tutorial_OAI_nrUE.md)

 it will break the execution at the run of the target function
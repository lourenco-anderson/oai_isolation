# OAI Isolation - Documentação

Bem-vindo à documentação centralizada do **OAI Isolation**. Esta pasta contém todos os guias, arquiteturas, troubleshooting e referências para usar este projeto.

---

## 📚 Guias Rápidos

### ⚡ Comece por Aqui
1. **[QUICK_START.md](../QUICK_START.md)** — Setup em 5 minutos (docker-compose ou k8s)
2. **[COMPONENTS.md](COMPONENTS.md)** — Tabela central de todos os 15 componentes (gNB + UE)
3. **[DEPLOYMENT_K8S.md](DEPLOYMENT_K8S.md)** — Guia completo de deployment em Kubernetes

### 🏗️ Arquitetura & Design
- **[ARCHITECTURE.md](ARCHITECTURE.md)** — Arquitetura geral, camadas, fluxos de dados
- **[COMPONENTS.md](COMPONENTS.md)** — Especificação detalhada: imagens, portas, recursos

### 📊 Monitoramento & Energia
- **[MONITORING_KEPLER.md](MONITORING_KEPLER.md)** — Setup Kepler + Prometheus + Grafana para medir consumo de energia

### 🔧 Build & Deploy
- **[DEPLOYMENT_K8S.md](DEPLOYMENT_K8S.md)** — Deploy em Kubernetes (com Kustomize)
- [../../k8s/scripts/](../../k8s/scripts/) — Scripts de automação (build, deploy, monitor)

### 🐛 Troubleshooting
- **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** — Problemas comuns, soluções, debug

---

## 📂 Estrutura de Documentação

```
docs/
├── README.md                    # ← Você está aqui
├── COMPONENTS.md                # Tabela central: 7 gNB + 8 UE, Dockerfiles, Manifests
├── QUICK_START.md               # (raiz) Quick start unificado
├── DEPLOYMENT_K8S.md            # Guia K8s (de k8s/README.md + KUBERNETES_SETUP.md)
├── ARCHITECTURE.md              # Arquitetura detalhada
├── MONITORING_KEPLER.md         # Setup Kepler+Prometheus+Grafana
├── TROUBLESHOOTING.md           # Solução de problemas
├── DEPLOYMENT_GUIDE.md          # (original k8s/)
├── QUICK_START_MONITORING.md    # (original k8s/, parcialmente redundante)
└── SUMÁRIO.md                   # (original k8s/, referência histórica)
```

---

## 🎯 Roteiros por Caso de Uso

### 👤 Desenvolvedor: "Quero rodar localmente"
1. Ler [../../QUICK_START.md](../../QUICK_START.md) (docker-compose)
2. Ver [COMPONENTS.md](COMPONENTS.md) para entender o que cada container faz
3. Se tiver erro, consultar [TROUBLESHOOTING.md](TROUBLESHOOTING.md)

### 👨‍💻 DevOps: "Quero fazer deploy em K8s"
1. Ler [DEPLOYMENT_K8S.md](DEPLOYMENT_K8S.md) (setup completo)
2. Ver [COMPONENTS.md](COMPONENTS.md) para customize (CPU, memória, replicas)
3. Usar scripts em [../../k8s/scripts/](../../k8s/scripts/)

### 🔍 Engenheiro de Observabilidade: "Quero monitorar energia/performance"
1. Ler [MONITORING_KEPLER.md](MONITORING_KEPLER.md) (Kepler setup)
2. Usar script `k8s/scripts/install-monitoring-stack.sh`
3. Consultar queries em `k8s/scripts/energy-queries.sh`

### 🏗️ Arquiteto: "Quero entender o design"
1. Ler [ARCHITECTURE.md](ARCHITECTURE.md) (fluxos, componentes, escalabilidade)
2. Ver [COMPONENTS.md](COMPONENTS.md) (especificação de cada serviço)

---

## 🗂️ Arquivos & Localizações Centrais

### Dockerfiles (Origem)
```
containers/
├── gnb/      → 7 Dockerfiles (crc, layer-map, ldpc, modulation, ofdm-mod, precoding, scramble)
└── ue/       → 8 Dockerfiles (ch-est, ch-mmse, check-crc, descrambling, layer-demap, ldpc-dec, ofdm-demod, soft-demod)
```

### Kubernetes Manifests (Centralizados)
```
k8s/manifests/
├── deployments/
│   ├── gnb/      → 7 deployments (renovado com tags oai-isolation:gnb-*)
│   └── ue/       → 8 deployments (renovado com tags oai-isolation:ue-*)
├── services/     → Services consolidados (em desenvolvimento)
├── namespace.yaml
└── kustomization.yaml  → Orquestra tudo
```

### Scripts de Automação
```
k8s/scripts/
├── build-images.sh               # Build todas imagens
├── load-kind-images.sh           # Retag + carregar no Kind
├── deploy.sh                     # Deploy no K8s
├── kind-setup.sh                 # Setup cluster Kind
├── monitor.sh                    # Monitor pods
├── install-monitoring-stack.sh   # Install Kepler+Prometheus+Grafana
└── energy-queries.sh             # Queries de energia
```

---

## 🚀 Comandos Essenciais

### Build
```bash
# Build todas as imagens (gNB + UE)
./k8s/scripts/build-images.sh localhost:5000 latest

# Ou apenas gNB
docker build -f containers/gnb/crc/Dockerfile -t oai-isolation:gnb-crc .
```

### Deploy em Kubernetes
```bash
# 1. Retag e carregar imagens no Kind
./k8s/scripts/load-kind-images.sh --only gnb
./k8s/scripts/load-kind-images.sh --only ue

# 2. Deploy via Kustomize
kubectl apply -k k8s/manifests/

# 3. Verificar status
kubectl get pods -n oai-isolation -o wide
kubectl get svc -n oai-isolation
```

### Monitoramento
```bash
# Install Kepler + Prometheus + Grafana
./k8s/scripts/install-monitoring-stack.sh

# Queries de energia
./k8s/scripts/energy-queries.sh --all
```

---

## 📊 Componentes (Resumido)

Veja [COMPONENTS.md](COMPONENTS.md) para tabela completa com:
- **7 gNB** (portas 8080-8086): CRC → Layer-Map → LDPC → Modulation → OFDM-Mod → Precoding → Scramble
- **8 UE** (portas 9080-9087): OFDM-Demod → Soft-Demod → Layer-Demap → LDPC-Dec → Descrambling → Ch-Est/MMSE → CRC-Check

---

## 📞 Referências

### Documentos Relacionados
- **Raiz**: [../../README.md](../../README.md) — Overview do projeto
- **Raiz**: [../../QUICK_START.md](../../QUICK_START.md) — Quick start com docker-compose
- **Raiz**: [../../START_HERE.md](../../START_HERE.md) — Ponto de entrada inicial

### Histórico & Archivos
- `DEPLOYMENT_GUIDE.md` — Guia antigo (referência)
- `SUMÁRIO.md` — Sumário histórico
- `QUICK_START_MONITORING.md` — Quick start de monitoramento (parcial)

### Ferramentas Externas
- [Kind](https://kind.sigs.k8s.io/) — Local Kubernetes
- [Kustomize](https://kustomize.io/) — Gerenciamento de manifests
- [Kepler](https://github.com/sustainable-computing-io/kepler) — Energy tracking
- [Prometheus](https://prometheus.io/) — Time-series DB
- [Grafana](https://grafana.com/) — Visualização

---

## ❓ Dúvidas?

1. **Problema comum?** → Veja [TROUBLESHOOTING.md](TROUBLESHOOTING.md)
2. **Como funciona X?** → Procure em [ARCHITECTURE.md](ARCHITECTURE.md)
3. **Qual é a porta de Y?** → Confira [COMPONENTS.md](COMPONENTS.md)
4. **Como deploy?** → Siga [DEPLOYMENT_K8S.md](DEPLOYMENT_K8S.md)

---

**Última atualização**: 2025-12-17  
**Estrutura**: Consolidada em `docs/` com tabela central em COMPONENTS.md

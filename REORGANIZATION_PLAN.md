# Plano de Reorganização - OAI Isolation

## 🔴 Redundâncias Identificadas

### 1. **Manifests Duplicados (containers/ vs k8s/)**
- ✗ `containers/gnb/*/gnb-*-deployment.yaml` (7 arquivos)
- ✗ `k8s/gnb-*-deployment.yaml` (7 arquivos)
- **Impacto**: Manutenção duplicada, desincronização

### 2. **Documentação Dispersa & Redundante**
- `KUBERNETES_SETUP.md` (raiz) — informações similares a k8s/README.md
- `k8s/SUMÁRIO.md` — duplica conteúdo de k8s/README.md + ARCHITECTURE.md
- `k8s/KEPLER_MONITORING.md` + `k8s/TROUBLESHOOTING.md` — redundância parcial
- `k8s/QUICK_START_MONITORING.md` — overlaps com README.md

### 3. **Scripts de Build Desalinhados**
- `k8s/build-images.sh` — tags `localhost:5000/oai-isolation/*`
- `k8s/load-kind-images.sh` — tags `oai-isolation:*` (recentemente criado)
- `docker-compose.yaml` — usa `oai-gnb-*:latest`
- `k8s/Makefile` — também builds com prefixo registry

### 4. **Estrutura de Pastas Confusa**
- `containers/gnb/*/*.yaml` + `k8s/*-deployment.yaml` — 2 locais para manifests
- Dockerfiles em `containers/` mas manifests em `k8s/`
- Sem separação clara entre "configuração de container" vs "deployment em k8s"

### 5. **Automação Redundante**
- `k8s/deploy.sh` + `k8s/quickstart.sh` + `k8s/Makefile` — 3 interfaces para mesma coisa
- `k8s/kind-setup.sh` + lógica dentro de `quickstart.sh`

---

## ✅ Estrutura Proposta (Otimizada)

```
/home/anderson/dev/oai_isolation/
│
├── 📚 docs/                          # Documentação centralizada
│   ├── README.md                     # Overview principal
│   ├── QUICK_START.md                # Quick start unificado (agora em docs/)
│   ├── ARCHITECTURE.md               # Arquitetura geral
│   ├── DEPLOYMENT_K8S.md             # Deploy em k8s (merge de README.md + KUBERNETES_SETUP.md)
│   ├── MONITORING_KEPLER.md          # Monitoramento (rename de KEPLER_MONITORING.md)
│   ├── TROUBLESHOOTING.md            # Troubleshooting
│   └── COMPONENTS.md                 # Tabela central de componentes (gNB + UE)
│
├── 🐳 containers/                    # Dockerfiles APENAS
│   ├── gnb/
│   │   ├── crc/
│   │   │   └── Dockerfile
│   │   ├── layer_map/
│   │   │   └── Dockerfile
│   │   ├── ldpc/
│   │   │   └── Dockerfile
│   │   ├── modulation/
│   │   │   └── Dockerfile
│   │   ├── ofdm_mod/
│   │   │   └── Dockerfile
│   │   ├── precoding/
│   │   │   └── Dockerfile
│   │   └── scramble/
│   │       └── Dockerfile
│   └── ue/
│       ├── ch_est/
│       │   └── Dockerfile
│       ├── ch_mmse/
│       │   └── Dockerfile
│       ├── check_crc/
│       │   └── Dockerfile
│       ├── descrambling/
│       │   └── Dockerfile
│       ├── layer_demap/
│       │   └── Dockerfile
│       ├── ldpc_dec/
│       │   └── Dockerfile
│       ├── ofdm_demod/
│       │   └── Dockerfile
│       └── soft_demod/
│           └── Dockerfile
│
├── k8s/                              # Kubernetes (configs + scripts)
│   ├── manifests/                    # NOVO: todos os YAMLs aqui
│   │   ├── namespace.yaml
│   │   ├── services/                 # NOVO: organizados por tipo
│   │   │   ├── gnb-services.yaml
│   │   │   └── ue-services.yaml
│   │   ├── deployments/
│   │   │   ├── gnb/
│   │   │   │   ├── gnb-crc.yaml
│   │   │   │   ├── gnb-layer-map.yaml
│   │   │   │   ├── gnb-ldpc.yaml
│   │   │   │   ├── gnb-modulation.yaml
│   │   │   │   ├── gnb-ofdm-mod.yaml
│   │   │   │   ├── gnb-precoding.yaml
│   │   │   │   └── gnb-scramble.yaml
│   │   │   └── ue/
│   │   │       ├── ue-ch-est.yaml
│   │   │       ├── ue-ch-mmse.yaml
│   │   │       ├── ue-check-crc.yaml
│   │   │       ├── ue-descrambling.yaml
│   │   │       ├── ue-layer-demap.yaml
│   │   │       ├── ue-ldpc-dec.yaml
│   │   │       ├── ue-ofdm-demod.yaml
│   │   │       └── ue-soft-demod.yaml
│   │   └── kustomization.yaml        # Referencia manifests/ de forma organizada
│   │
│   ├── scripts/                      # NOVO: scripts em subpasta
│   │   ├── build-images.sh           # Uniforme: tags `oai-isolation:*`
│   │   ├── load-kind-images.sh       # Retag + kind load
│   │   ├── deploy.sh                 # Deploy no k8s
│   │   ├── kind-setup.sh             # Setup Kind
│   │   ├── monitor.sh                # Monitor pods
│   │   └── energy-queries.sh         # Queries Kepler
│   │
│   ├── Makefile                      # Entry point para automação
│   └── docker-compose.yaml           # Alternativa local (sem k8s)
│
├── 🛠️ tools/                         # NOVO: ferramentas de suporte
│   ├── build.sh                      # Smart build (docker-compose ou docker build)
│   └── setup-dev.sh                  # Ambiente de dev
│
├── src/                              # Código-fonte (binários pré-compilados)
│   ├── find_function.txt
│   ├── functions.c
│   ├── functions.h
│   ├── main.c
│   └── ...
│
├── build/                            # Build outputs
│   └── oai_isolation
│
├── ext/                              # OpenAIR (external repo)
│   └── openair/
│
├── README.md                         # Overview raiz (aponta para docs/)
├── QUICK_START.md                    # Mantém na raiz para visibilidade inicial
└── docker-compose.yaml               # Mantém na raiz (compatibilidade)
```

---

## 🎯 Ações Propostas

### **Fase 1: Consolidação de Documentação**
1. Merge `KUBERNETES_SETUP.md` → `docs/DEPLOYMENT_K8S.md`
2. Rename `k8s/KEPLER_MONITORING.md` → `docs/MONITORING_KEPLER.md`
3. Consolidar tabelas de componentes → `docs/COMPONENTS.md`
4. Cleanup redundâncias em `k8s/README.md`, `k8s/SUMÁRIO.md`

### **Fase 2: Consolidação de Manifests**
1. Move `containers/gnb/*/*.yaml` → `k8s/manifests/deployments/gnb/`
2. Move `containers/ue/*/*.yaml` → `k8s/manifests/deployments/ue/` (quando existirem)
3. Reorganizar Services → `k8s/manifests/services/`
4. Atualizar `kustomization.yaml` para referenciar nova estrutura

### **Fase 3: Unificação de Scripts & Build**
1. Padronizar todas as tags → `oai-isolation:gnb-*` e `oai-isolation:ue-*`
2. Move scripts → `k8s/scripts/`
3. Atualizar Makefile para chamar scripts da nova localização
4. Criar wrapper simples (ex.: `tools/build.sh`) para escolher entre docker-compose e docker build

### **Fase 4: Validação & Limpeza**
1. Testar deploy com nova estrutura
2. Remover arquivos redundantes (containers/*.yaml duplicados)
3. Atualizar referências em scripts

---

## 📊 Benefícios

| Antes | Depois |
|-------|--------|
| 15 manifests em 2 locais | 15 manifests em 1 lugar (k8s/manifests/) |
| 6 docs dispersas | 1 raiz (README.md) + 7 docs focadas em `docs/` |
| 3 interfaces de build/deploy | 1 Makefile + scripts claros em `k8s/scripts/` |
| Tags inconsistentes | Tags padronizadas: `oai-isolation:*` |
| Containers sem docs | Referências claras em `docs/COMPONENTS.md` |

---

## ⏱️ Próximos Passos
1. ✅ Revisar plano
2. ⏳ Implementar Fase 1-4 sequencialmente
3. ⏳ Testar integridade completa
4. ⏳ Commit & update referências (GitHub, etc.)


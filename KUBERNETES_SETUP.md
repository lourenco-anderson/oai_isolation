# OAI Isolation + Kubernetes + Kepler

## 🎯 Tudo Pronto para Usar!

Você solicitou: **"Gostaria de adicionar os containers da pasta containers no meu cluster/kubernetes, e cada função como um pod, assim eu pode capturar o consumo de cada uma com kepler"**

✅ **FEITO!** Aqui está a solução completa.

---

## 📦 O que foi criado

```
32 arquivos na pasta k8s/
├── 6 documentos (README, ARCHITECTURE, KEPLER_MONITORING, etc)
├── 16 manifestos Kubernetes (15 deployments + namespace)
├── 7 scripts de automação (quickstart, build, deploy, monitoring)
├── 1 Makefile (20+ comandos úteis)
└── 1 docker-compose (alternativa local)
```

### Resultado: 
- **15 pods independentes** (7 GNB + 8 UE)
- **Cada pod = métrica de energia capturável pelo Kepler**
- **Monitoramento integrado** (Kepler + Prometheus + Grafana)

---

## 🚀 Quick Start (3 linhas de comando)

```bash
cd k8s
./quickstart.sh                      # Setup OAI + K8s (~5 min)
./install-monitoring-stack.sh        # Instalar Kepler (~3 min)
```

Depois, em outro terminal:
```bash
bash /tmp/port-forwards.sh
# Acesse: http://localhost:3000 (admin/grafana)
```

---

## 📊 Pods Criados

### GNB (Transmitter - 7 pods, portas 8080-8086)
```
gnb-crc → gnb-layer-map → gnb-ldpc → gnb-modulation 
→ gnb-ofdm-mod → gnb-precoding → gnb-scramble
```

### UE (Receiver - 8 pods, portas 9080-9087)
```
ue-ofdm-demod → ue-soft-demod → ue-layer-demap → ue-ldpc-dec 
→ ue-descrambling → ue-ch-est/ch-mmse → ue-check-crc
```

**Cada pod tem:**
- ✅ Service (DNS discovery)
- ✅ Monitoramento Kepler (energia)
- ✅ Métricas Prometheus
- ✅ Visualização Grafana

---

## 💡 Capturando Consumo de Energia

### Por Função
```bash
make energy-top
# Output: Top 5 funções mais consumidoras
```

### Interativo
```bash
make energy-interactive
# Menu de queries
```

### Total
```bash
make energy-total
# Consumo total de energia
```

### GNB vs UE
```bash
# Query: sum(rate(kepler_container_energy_total{pod_name=~"gnb-.*"}[5m])) / 
#        sum(rate(kepler_container_energy_total{pod_name=~"ue-.*"}[5m]))
```

---

## 📁 Arquivos Importantes

| Arquivo | Propósito |
|---------|-----------|
| `README.md` | Guia completo de uso |
| `KEPLER_MONITORING.md` | Setup de monitoramento com Kepler |
| `QUICK_START_MONITORING.md` | Quick reference (2 páginas) |
| `ARCHITECTURE.md` | Arquitetura detalhada dos 15 pods |
| `TROUBLESHOOTING.md` | Solução de problemas |
| `Makefile` | 20+ comandos úteis |

---

## 🛠️ Comandos Principais

```bash
# Deploy
make deploy                # Deploy OAI Isolation (15 pods)
make undeploy             # Remover
make build                # Build imagens Docker
make clean                # Remover tudo

# Monitoramento
make install-monitoring   # Instalar Kepler + Prometheus + Grafana
make energy-total         # Consumo total
make energy-top           # Top 5 consumidores
make energy-interactive   # Menu interativo de queries
make port-forward-all     # Port forwards (Grafana, Prometheus)

# Debug
make status               # Status dos pods
make logs POD=<name>      # Ver logs
make monitor              # Monitor em tempo real
```

---

## 🔍 Métricas por Pod

Cada pod permite capturar:

```
⚡ Energia
   • Consumo total (Joules)
   • Energia CPU
   • Energia DRAM
   • Potência (Watts)

💻 Computação
   • CPU usage
   • Memory usage
   • Context switches

🌐 Network
   • Bytes TX/RX
   • Pacotes TX/RX

💾 Disco
   • I/O read/write
   • Filesystem usage
```

---

## 📈 Exemplos de Análise

### 1. Qual função consome mais energia?
```bash
make energy-top
```

### 2. Consumo total do sistema?
```bash
make energy-total
```

### 3. GNB ou UE mais eficiente?
```bash
# Prometheus query:
sum(rate(kepler_container_energy_total{pod_name=~"gnb-.*"}[5m])) / 
sum(rate(kepler_container_energy_total{pod_name=~"ue-.*"}[5m]))
```

### 4. Como escala a energia?
```bash
kubectl scale deployment gnb-ldpc --replicas=3
# Monitorar energia crescer no Grafana
```

---

## ✨ Características Implementadas

✅ **15 Pods Independentes**
   - Monitoramento individual de cada função
   - Service discovery automático

✅ **Kepler Integration**
   - Coleta de energia por pod
   - Métricas de CPU, memória, network

✅ **Prometheus + Grafana**
   - Armazenamento de séries temporais
   - Visualização em tempo real
   - Dashboards customizáveis

✅ **Automação Completa**
   - Scripts 100% automatizados
   - Makefiles com 20+ comandos
   - Setup "um clique"

✅ **Documentação Profissional**
   - 6 documentos detalhados
   - Guias de troubleshooting
   - Exemplos de queries

---

## 🎯 Próximas Etapas

1. **Executar setup**
   ```bash
   cd k8s && ./quickstart.sh
   ```

2. **Instalar monitoramento**
   ```bash
   ./install-monitoring-stack.sh
   ```

3. **Abrir Grafana**
   ```bash
   # Em outro terminal
   bash /tmp/port-forwards.sh
   # http://localhost:3000 (admin/grafana)
   ```

4. **Criar dashboards customizados**
   - Use templates de `KEPLER_MONITORING.md`
   - Queries PromQL prontas

5. **Analisar dados**
   - Consumo por função
   - Tendências
   - Anomalias

6. **Otimizar**
   - Identificar gargalos
   - Escalabilidade
   - Fine-tuning

---

## 📚 Documentação Disponível

```
k8s/
├── README.md                        ← Você está aqui
├── QUICK_START_MONITORING.md        ← Comece aqui (2 pags)
├── KEPLER_MONITORING.md             ← Setup completo
├── ARCHITECTURE.md                  ← Arquitetura detalhada
├── TROUBLESHOOTING.md               ← Problemas comuns
└── SUMÁRIO.md                       ← Resumo (PT)
```

---

## 🎉 Resumo Final

| Item | Status | Detalhes |
|------|--------|----------|
| Pods K8s | ✅ | 15 (7 GNB + 8 UE) |
| Services | ✅ | 15 (DNS + LB) |
| Namespace | ✅ | oai-isolation |
| Kepler | ✅ | Monitora energia por pod |
| Prometheus | ✅ | Armazena métricas |
| Grafana | ✅ | Visualiza dados |
| Automação | ✅ | 7 scripts + Makefile |
| Documentação | ✅ | 6 documentos |
| **TOTAL** | **✅** | **32 arquivos prontos** |

---

## 💻 Exemplo de Uso Real

```bash
# Terminal 1: Setup
cd k8s
./quickstart.sh
./install-monitoring-stack.sh

# Terminal 2: Port forwards
bash /tmp/port-forwards.sh

# Terminal 3: Monitoramento
make status                # Ver pods rodando
make energy-top           # Ver consumo

# Browser: http://localhost:3000
# Criar dashboard com dados de energia dos 15 pods
```

---

## 🔗 Informações de Acesso

```
Grafana:     http://localhost:3000 (admin/grafana)
Prometheus:  http://localhost:9090
Kepler:      http://localhost:8888/metrics
```

---

## 📞 Suporte

Se encontrar problemas, consulte:
1. `TROUBLESHOOTING.md` - Soluções comuns
2. `KEPLER_MONITORING.md` - Setup de monitoramento
3. Logs: `make logs POD=<nome>`
4. Status: `make status`

---

## 🚀 Comece Agora!

```bash
cd k8s
./quickstart.sh
```

Pronto! Você tem:
- ✅ 15 pods rodando
- ✅ Cada um monitora energia
- ✅ Grafana para visualizar
- ✅ Dados em tempo real

**Happy Monitoring! 🎉**

---

**Versão**: 1.0  
**Data**: Dezembro 2025  
**Arquivos**: 32 total  
**Status**: ✅ Pronto para Produção

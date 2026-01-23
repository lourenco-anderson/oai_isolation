# OAI Isolation - Monitoramento Kubernetes

Este repositório contém uma implementação completa de monitoramento para pods OAI em um cluster Kubernetes (minikube).

## 📚 Documentação

### 🎯 **Comece Aqui**
1. **[INSTALLATION_SUMMARY.md](INSTALLATION_SUMMARY.md)** ⭐ - Resumo do que foi instalado
2. **[GRAFANA_QUICK_START.md](GRAFANA_QUICK_START.md)** ⭐ - Como ver métricas em 3 passos

### 📖 **Guias Detalhados**
- **[DOCKER_K8S_BEST_PRACTICES.md](DOCKER_K8S_BEST_PRACTICES.md)** - Best practices para Docker e Kubernetes
- **[GRAFANA_SETUP_GUIDE.md](GRAFANA_SETUP_GUIDE.md)** - Setup completo do Grafana
- **[KEPLER_SETUP.md](KEPLER_SETUP.md)** - Instalação e configuração do Kepler
- **[KEPLER_INSTALLATION_STATUS.md](KEPLER_INSTALLATION_STATUS.md)** - Status atual e troubleshooting

### 🛠️ **Ferramentas**
- **[check-metrics.sh](check-metrics.sh)** - Script para verificar status de tudo

---

## 🚀 Quick Start (3 minutos)

### Terminal 1: Port Forward Grafana
```bash
cd /home/anderson/dev/oai_isolation
kubectl port-forward -n monitoring svc/grafana 3000:80
```

### Terminal 2: Browser
```
http://localhost:3000
User: admin
Password: prom-operator
```

### No Grafana:
1. **Dashboards** → **Import**
2. Upload: `k8s-manifests/OAI-Pods-Dashboard.json`
3. Visualize métricas dos seus pods!

---

## 📊 O que Você Pode Ver

### Agora (cAdvisor)
- 📈 Memory Usage por pod
- 📈 CPU Usage por pod
- 📈 Network Traffic (IN/OUT)

### Depois (quando Kepler funcionar)
- ⚡ Power Consumption (watts)
- ⚡ Energy Usage (joules/kWh)
- ⚡ Carbon Footprint

---

## 🏗️ Arquitetura

```
┌─────────────────┐
│   OAI Pods      │
│  (8 containers) │
└────────┬────────┘
         │
         ├─→ cAdvisor (container metrics)
         │
         └─→ Kepler (energy metrics) [WIP]
              │
              ▼
         ┌──────────────┐
         │ Prometheus   │
         │  (data store)│
         └──────┬───────┘
                │
                ▼
         ┌──────────────┐
         │   Grafana    │
         │ (dashboards) │
         └──────────────┘
```

---

## 📋 Componentes Instalados

| Componente | Status | Versão | Namespace |
|-----------|--------|--------|-----------|
| Prometheus | ✅ Online | 2.x | monitoring |
| Grafana | ✅ Online | Latest | monitoring |
| Kepler | ⚠️ Rodando | v0.11.3 | kepler |
| OAI Pods | ✅ Disponíveis | Latest | default |

---

## 📁 Estrutura de Arquivos

```
📁 /home/anderson/dev/oai_isolation/
├── 📄 INSTALLATION_SUMMARY.md         ⭐ Resumo
├── 📄 GRAFANA_QUICK_START.md          ⭐ Comece aqui
├── 📄 DOCKER_K8S_BEST_PRACTICES.md   📖 Aprender
├── 📄 GRAFANA_SETUP_GUIDE.md         📖 Detalhe
├── 📄 KEPLER_SETUP.md                📖 Kepler
├── 📄 KEPLER_INSTALLATION_STATUS.md  📖 Status
├── 📄 check-metrics.sh               🛠️ Ferramenta
├── 📁 k8s-manifests/
│   ├── 📄 ue-deployments.yaml
│   ├── 📄 OAI-Pods-Dashboard.json     ✨ Importe no Grafana
│   └── 📄 Kepler Exporter Dashboard-*.json
├── 📁 experimental-cluster-setup/
│   └── 📄 kepler-values.yaml
└── ...
```

---

## 🎯 Roadmap

- [x] Instalar Kepler via Helm
- [x] Configurar Prometheus + ServiceMonitor
- [x] Criar dashboard OAI com cAdvisor
- [x] Documentação completa
- [ ] Debug Kepler para enviar métricas de energia
- [ ] Alertas Grafana
- [ ] Integração com CI/CD

---

## 🔍 Verificações de Status

### Ver todos os pods
```bash
kubectl get pods -A
```

### Ver apenas OAI pods
```bash
kubectl get pods -l component=ue
```

### Checar Prometheus
```bash
kubectl port-forward -n monitoring svc/prometheus-server 9090:80
# http://localhost:9090
```

### Executar script de verificação
```bash
bash check-metrics.sh
```

---

## 🐛 Troubleshooting

### Problema: Dashboard vazio no Grafana

**Solução:**
1. Verificar se Prometheus tem dados:
   ```bash
   kubectl port-forward -n monitoring svc/prometheus-server 9090:80
   # Query: container_memory_working_set_bytes
   ```

2. Se datasource Prometheus não aparece:
   - Grafana → Configuration → Data Sources
   - Add: `http://prometheus-server.monitoring.svc.cluster.local:80`

### Problema: Pods OAI em CrashLoop

**Causa:** Normal - eles executam testes e terminam

**Solução:** Isso é esperado, eles são containers de teste

---

## 📚 Documentação Adicional

Para aprender mais sobre os componentes:

- **Docker Best Practices**: [DOCKER_K8S_BEST_PRACTICES.md](DOCKER_K8S_BEST_PRACTICES.md#3-integração-com-kubernetes-minikube)
- **Grafana Setup**: [GRAFANA_SETUP_GUIDE.md](GRAFANA_SETUP_GUIDE.md)
- **Kepler**: [KEPLER_SETUP.md](KEPLER_SETUP.md)

---

## 💡 Próximas Ações

1. **Imediato**: Visualizar métricas no dashboard OAI
2. **Curto prazo**: Customizar dashboard com mais queries
3. **Médio prazo**: Debugar Kepler para energética
4. **Longo prazo**: Alertas e integração com CI/CD

---

## 🔗 Links Úteis

- [Kepler Documentation](https://github.com/sustainable-computing-io/kepler)
- [Prometheus Operator](https://prometheus-operator.dev/)
- [Grafana Dashboards](https://grafana.com/grafana/dashboards)
- [Kubernetes Best Practices](https://kubernetes.io/docs/concepts/configuration/overview/)

---

## 📞 Dúvidas?

1. Ler a documentação apropriada acima
2. Executar `bash check-metrics.sh`
3. Verificar logs: `kubectl logs <pod-name>`

---

**Status**: ✅ Pronto para usar  
**Última atualização**: 12 Jan 2026  
**Maintainer**: Anderson Lourenço

---

## 📝 Notas Técnicas

### Métricas Disponíveis

**cAdvisor** (todas as containers):
- `container_memory_working_set_bytes` - Memory
- `container_cpu_usage_seconds_total` - CPU
- `container_network_receive_bytes_total` - Network RX
- `container_network_transmit_bytes_total` - Network TX
- `container_fs_usage_bytes` - Disk space
- `container_fs_io_time_seconds_total` - Disk I/O

**Kepler** (quando funcionando):
- `kepler_container_joules_total` - Energy (joules)
- `kepler_container_package_joules_total` - CPU energy
- `kepler_container_dram_joules_total` - Memory energy
- `kepler_container_gpu_joules_total` - GPU energy

### Labels Importantes

```
namespace="default"  # Namespace do pod
pod="ue-*"          # Nome do pod OAI
container="..."     # Nome do container
```

### Queries de Exemplo

```promql
# Top 5 memory consumers
topk(5, container_memory_working_set_bytes)

# Pods by namespace
count by (namespace) (container_memory_working_set_bytes)

# CPU per pod
sum by (pod) (rate(container_cpu_usage_seconds_total[1m]))
```

---

## 🎓 Learning Path

1. **Iniciante**: GRAFANA_QUICK_START.md
2. **Intermediário**: DOCKER_K8S_BEST_PRACTICES.md
3. **Avançado**: KEPLER_SETUP.md + GRAFANA_SETUP_GUIDE.md


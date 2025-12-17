.PHONY: help build deploy undeploy logs status monitor kind-create kind-delete kind-status clean

NAMESPACE ?= oai-isolation
REGISTRY ?= localhost:5000
TAG ?= latest
CLUSTER_NAME ?= oai-isolation-cluster

# Cores para output
BLUE := \033[0;34m
GREEN := \033[0;32m
YELLOW := \033[1;33m
RED := \033[0;31m
NC := \033[0m # No Color

help:
	@echo "$(BLUE)=========================================="
	@echo "OAI Isolation - Kubernetes Management"
	@echo "==========================================$(NC)"
	@echo ""
	@echo "$(YELLOW)Cluster Setup:$(NC)"
	@echo "  make kind-create          - Criar cluster Kind local"
	@echo "  make kind-delete          - Deletar cluster Kind local"
	@echo "  make kind-status          - Verificar status do cluster Kind"
	@echo ""
	@echo "$(YELLOW)Image Building:$(NC)"
	@echo "  make build                - Build de todas as imagens Docker"
	@echo "  make build-gnb            - Build apenas imagens GNB"
	@echo "  make build-ue             - Build apenas imagens UE"
	@echo ""
	@echo "$(YELLOW)Deployment:$(NC)"
	@echo "  make deploy               - Deploy no Kubernetes"
	@echo "  make undeploy             - Remover deployment"
	@echo "  make logs                 - Mostrar logs de um pod (POD=<name>)"
	@echo "  make status               - Ver status dos pods"
	@echo "  make monitor              - Monitorar em tempo real"
	@echo ""
	@echo "$(YELLOW)Monitoring & Energy Analysis:$(NC)"
	@echo "  make install-monitoring   - Instalar Kepler + Prometheus + Grafana"
	@echo "  make energy-queries       - Executar queries de energia"
	@echo "  make energy-interactive   - Menu interativo de queries"
	@echo "  make energy-total         - Ver consumo total de energia"
	@echo "  make energy-top           - Ver top 5 consumidores"
	@echo "  make port-forward-all     - Iniciar port forwards (Grafana/Prometheus)"
	@echo "  make kepler-status        - Status do Kepler"
	@echo "  make prometheus-status    - Status do Prometheus"
	@echo "  make grafana-status       - Status do Grafana"
	@echo ""
	@echo "$(YELLOW)Cleanup:$(NC)"
	@echo "  make clean                - Remover namespace e todos os recursos"
	@echo ""
	@echo "$(YELLOW)Utilities:$(NC)"
	@echo "  make validate-manifests   - Validar manifestos K8s"
	@echo "  make describe-resources   - Descrever recursos detalhadamente"
	@echo "  make push-images          - Push imagens para registry"
	@echo "  make docs                 - Ver lista de documentação"
	@echo ""
	@echo "$(YELLOW)Exemplos:$(NC)"
	@echo "  make build REGISTRY=myregistry.com TAG=v1.0"
	@echo "  make deploy NAMESPACE=production"
	@echo "  make logs POD=gnb-crc-xyz"
	@echo ""

kind-create:
	@echo "$(YELLOW)Criando cluster Kind: $(CLUSTER_NAME)$(NC)"
	./scripts/kind-setup.sh create
	@echo "$(GREEN)✓ Cluster criado$(NC)"

kind-delete:
	@echo "$(YELLOW)Deletando cluster Kind: $(CLUSTER_NAME)$(NC)"
	./scripts/kind-setup.sh delete
	@echo "$(GREEN)✓ Cluster deletado$(NC)"

kind-status:
	@./scripts/kind-setup.sh status

build:
	@echo "$(YELLOW)Building images with REGISTRY=$(REGISTRY) TAG=$(TAG)$(NC)"
	./scripts/build-images.sh $(REGISTRY) $(TAG)

build-gnb:
	@echo "$(YELLOW)Building GNB images$(NC)"
	docker build -f ../containers/gnb/crc/Dockerfile -t $(REGISTRY)/oai-isolation:gnb-crc-$(TAG) ..
	docker build -f ../containers/gnb/layer_map/Dockerfile -t $(REGISTRY)/oai-isolation:gnb-layer-map-$(TAG) ..
	docker build -f ../containers/gnb/ldpc/Dockerfile -t $(REGISTRY)/oai-isolation:gnb-ldpc-$(TAG) ..
	docker build -f ../containers/gnb/modulation/Dockerfile -t $(REGISTRY)/oai-isolation:gnb-modulation-$(TAG) ..
	docker build -f ../containers/gnb/ofdm_mod/Dockerfile -t $(REGISTRY)/oai-isolation:gnb-ofdm-mod-$(TAG) ..
	docker build -f ../containers/gnb/precoding/Dockerfile -t $(REGISTRY)/oai-isolation:gnb-precoding-$(TAG) ..
	docker build -f ../containers/gnb/scramble/Dockerfile -t $(REGISTRY)/oai-isolation:gnb-scramble-$(TAG) ..
	@echo "$(GREEN)✓ GNB images built$(NC)"

build-ue:
	@echo "$(YELLOW)Building UE images$(NC)"
	docker build -f ../containers/ue/ch_est/Dockerfile -t $(REGISTRY)/oai-isolation:ue-ch-est-$(TAG) ..
	docker build -f ../containers/ue/ch_mmse/Dockerfile -t $(REGISTRY)/oai-isolation:ue-ch-mmse-$(TAG) ..
	docker build -f ../containers/ue/check_crc/Dockerfile -t $(REGISTRY)/oai-isolation:ue-check-crc-$(TAG) ..
	docker build -f ../containers/ue/descrambling/Dockerfile -t $(REGISTRY)/oai-isolation:ue-descrambling-$(TAG) ..
	docker build -f ../containers/ue/layer_demap/Dockerfile -t $(REGISTRY)/oai-isolation:ue-layer-demap-$(TAG) ..
	docker build -f ../containers/ue/ldpc_dec/Dockerfile -t $(REGISTRY)/oai-isolation:ue-ldpc-dec-$(TAG) ..
	docker build -f ../containers/ue/ofdm_demod/Dockerfile -t $(REGISTRY)/oai-isolation:ue-ofdm-demod-$(TAG) ..
	docker build -f ../containers/ue/soft_demod/Dockerfile -t $(REGISTRY)/oai-isolation:ue-soft-demod-$(TAG) ..
	@echo "$(GREEN)✓ UE images built$(NC)"

deploy:
	@echo "$(YELLOW)Deploying to Kubernetes (NAMESPACE=$(NAMESPACE))$(NC)"
	./scripts/deploy.sh $(NAMESPACE) $(REGISTRY)
	@echo "$(GREEN)✓ Deployment completed$(NC)"

undeploy:
	@echo "$(YELLOW)Removing deployment from Kubernetes$(NC)"
	kubectl delete namespace $(NAMESPACE) --ignore-not-found=true
	@echo "$(GREEN)✓ Deployment removed$(NC)"

status:
	@echo "$(YELLOW)Deployment Status (Namespace: $(NAMESPACE))$(NC)"
	@echo ""
	@echo "$(BLUE)Deployments:$(NC)"
	@kubectl get deployments -n $(NAMESPACE) 2>/dev/null || echo "No deployments found"
	@echo ""
	@echo "$(BLUE)Pods:$(NC)"
	@kubectl get pods -n $(NAMESPACE) 2>/dev/null || echo "No pods found"
	@echo ""
	@echo "$(BLUE)Services:$(NC)"
	@kubectl get svc -n $(NAMESPACE) 2>/dev/null || echo "No services found"

logs:
	@if [ -z "$(POD)" ]; then \
		echo "$(RED)Error: POD variable not set$(NC)"; \
		echo "Usage: make logs POD=<pod-name>"; \
		exit 1; \
	fi
	@echo "$(YELLOW)Fetching logs for pod: $(POD)$(NC)"
	@kubectl logs -n $(NAMESPACE) $(POD)

monitor:
	@./monitor.sh $(NAMESPACE)

clean:
	@echo "$(RED)WARNING: This will delete the entire namespace and all resources!$(NC)"
	@echo "Press Ctrl+C to cancel..."
	@sleep 3
	@echo "$(YELLOW)Cleaning up namespace: $(NAMESPACE)$(NC)"
	@kubectl delete namespace $(NAMESPACE) --ignore-not-found=true
	@echo "$(GREEN)✓ Cleanup completed$(NC)"

push-images:
	@echo "$(YELLOW)Pushing images to registry: $(REGISTRY)$(NC)"
	@docker images | grep oai-isolation | awk '{print $$1":"$$2}' | xargs -I {} docker push {}
	@echo "$(GREEN)✓ Images pushed$(NC)"

validate-manifests:
	@echo "$(YELLOW)Validating Kubernetes manifests$(NC)"
	@kubectl apply -f manifests/ --dry-run=client
	@echo "$(GREEN)✓ All manifests are valid$(NC)"

describe-resources:
	@echo "$(YELLOW)Describing all resources in namespace: $(NAMESPACE)$(NC)"
	@kubectl describe all -n $(NAMESPACE)

# ============= MONITORING & ENERGY ANALYSIS =============

install-monitoring:
	@echo "$(YELLOW)Installing Kepler + Prometheus + Grafana$(NC)"
	./scripts/install-monitoring-stack.sh

energy-queries:
	@echo "$(YELLOW)Running energy queries...$(NC)"
	./scripts/energy-queries.sh --all

energy-interactive:
	@./scripts/energy-queries.sh

port-forward-all:
	@echo "$(YELLOW)Starting port forwards...$(NC)"
	@kubectl port-forward -n prometheus svc/prometheus-grafana 3000:80 &
	@kubectl port-forward -n prometheus svc/prometheus-kube-prometheus-prometheus 9090:9090 &
	@kubectl port-forward -n kepler svc/kepler 8888:8888 &
	@echo "$(GREEN)Port forwards started:$(NC)"
	@echo "  Grafana: http://localhost:3000 (admin/grafana)"
	@echo "  Prometheus: http://localhost:9090"
	@echo "  Kepler: http://localhost:8888/metrics"

kepler-status:
	@echo "$(YELLOW)Kepler Status:$(NC)"
	@kubectl get pods -n kepler
	@echo ""
	@echo "$(YELLOW)Kepler Logs:$(NC)"
	@kubectl logs -n kepler -l app.kubernetes.io/name=kepler --tail=20

prometheus-status:
	@echo "$(YELLOW)Prometheus Status:$(NC)"
	@kubectl get pods -n prometheus
	@echo ""
	@echo "$(YELLOW)Prometheus Logs:$(NC)"
	@kubectl logs -n prometheus -l app.kubernetes.io/name=prometheus --tail=20

grafana-status:
	@echo "$(YELLOW)Grafana Status:$(NC)"
	@kubectl get pods -n prometheus
	@echo ""
	@echo "$(YELLOW)Grafana Logs:$(NC)"
	@kubectl logs -n prometheus -l app.kubernetes.io/name=grafana --tail=20

energy-total:
	@echo "$(YELLOW)Total Energy Consumption (all pods):$(NC)"
	@curl -s http://localhost:9090/api/v1/query --data-urlencode 'query=sum(kepler_container_energy_total{namespace="oai-isolation"})' | jq '.data.result[0].value[1]' 2>/dev/null || echo "Prometheus not accessible"

energy-top:
	@echo "$(YELLOW)Top 5 Energy Consumers:$(NC)"
	@curl -s http://localhost:9090/api/v1/query --data-urlencode 'query=topk(5, sum by (pod_name) (kepler_container_energy_total{namespace="oai-isolation"}))' | jq '.data.result[] | {pod: .metric.pod_name, energy_joules: .value[1]}' 2>/dev/null || echo "Prometheus not accessible"

# ============= DOCUMENTATION =============

docs:
	@echo "$(BLUE)OAI Isolation Documentation (em docs/):$(NC)"
	@echo ""
	@echo "docs/README.md                - Index de documentação"
	@echo "docs/COMPONENTS.md            - Tabela central (7 gNB + 8 UE)"
	@echo "docs/DEPLOYMENT_K8S.md        - Guia K8s"
	@echo "docs/ARCHITECTURE.md          - Arquitetura detalhada"
	@echo "docs/MONITORING_KEPLER.md     - Setup Kepler+Prometheus+Grafana"
	@echo "docs/TROUBLESHOOTING.md       - Problemas e soluções"
	@echo ""
	@echo "Ver: cat docs/README.md"
	@echo ""

.DEFAULT_GOAL := help

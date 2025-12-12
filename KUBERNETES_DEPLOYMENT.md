# OAI Functions - Kubernetes Deployment

Complete REST + Kubernetes deployment for 15 OAI PHY layer functions.

## Architecture

```
┌─────────────────────────────────────────────┐
│         Kubernetes Cluster                  │
├─────────────────────────────────────────────┤
│                                             │
│  Namespace: oai-functions                   │
│                                             │
│  ┌─────────────────────────────────────┐   │
│  │ Ingress (nginx)                     │   │
│  │ Host: oai-functions.local           │   │
│  └────┬────────────────────────────────┘   │
│       │                                     │
│  ┌────▼─────────────────────────────────┐  │
│  │ 15 Microservices (Flask + C libs)   │  │
│  ├──────────────────────────────────────┤  │
│  │ nr_scramble      :8001 (2 replicas) │  │
│  │ nr_crc           :8002 (2 replicas) │  │
│  │ nr_ldpc_dec      :8015 (3 replicas) │  │
│  │ ...                                  │  │
│  └──────────────────────────────────────┘  │
│                                             │
└─────────────────────────────────────────────┘
```

## Quick Start

### 1. Build Shared Library

```bash
cd /home/anderson/dev/oai_isolation
mkdir -p build && cd build
cmake ..
make -j4 oai_functions
```

### 2. Test Locally with Docker Compose

```bash
# Start all services
docker-compose up -d

# Test nr_scramble
curl -X POST http://localhost:8001/scramble

# Test nr_ldpc_dec
curl -X POST http://localhost:8015/ldpc_dec

# View logs
docker-compose logs -f nr_scramble

# Stop all
docker-compose down
```

### 3. Build Docker Images

```bash
# Set your registry
export DOCKER_REGISTRY=docker.io/your-username

# Build all images (takes ~30-45 minutes)
./scripts/build-all-images.sh

# Or build single service
docker build -f containers/services/nr_scramble/Dockerfile -t nr_scramble:latest .
```

### 4. Push to Registry

```bash
# Login to registry
docker login

# Push all images
./scripts/push-images.sh
```

### 5. Deploy to Kubernetes

```bash
# Make sure kubectl is configured
kubectl cluster-info

# Deploy everything
./scripts/deploy-to-k8s.sh

# Check status
kubectl get pods -n oai-functions
kubectl get svc -n oai-functions
kubectl get ingress -n oai-functions
```

## Services

| Service | Port | Endpoint | Replicas | CPU | Memory |
|---------|------|----------|----------|-----|--------|
| nr_scramble | 8001 | /scramble | 2 | 200m | 256Mi |
| nr_crc | 8002 | /crc | 2 | 100m | 128Mi |
| nr_ofdm_modulation | 8003 | /ofdm_modulation | 2 | 200m | 256Mi |
| nr_precoding | 8004 | /precoding | 2 | 150m | 192Mi |
| nr_modulation_test | 8005 | /modulation_test | 1 | 150m | 192Mi |
| nr_layermapping | 8006 | /layermapping | 2 | 150m | 192Mi |
| nr_ldpc | 8007 | /ldpc | 2 | 250m | 512Mi |
| nr_ofdm_demo | 8008 | /ofdm_demo | 1 | 150m | 192Mi |
| nr_ch_estimation | 8009 | /ch_estimation | 2 | 200m | 256Mi |
| nr_descrambling | 8010 | /descrambling | 2 | 200m | 256Mi |
| nr_layer_demapping_test | 8011 | /layer_demapping_test | 2 | 150m | 192Mi |
| nr_crc_check | 8012 | /crc_check | 2 | 150m | 192Mi |
| nr_soft_demod | 8013 | /soft_demod | 2 | 200m | 256Mi |
| nr_mmse_eq | 8014 | /mmse_eq | 2 | 250m | 384Mi |
| nr_ldpc_dec | 8015 | /ldpc_dec | 3 | 300m | 512Mi |

## API Usage

### Health Check

```bash
curl http://oai-functions.local/scramble/health
# or direct pod access:
kubectl port-forward -n oai-functions svc/nr-scramble 8001:80
curl http://localhost:8001/health
```

### Execute Function

```bash
curl -X POST http://oai-functions.local/scramble \
  -H "Content-Type: application/json" \
  -d '{}'

# Response:
{
  "status": "success",
  "function": "nr_scramble",
  "message": "Execution completed",
  "output": "iter 0: out[0]=0xDCE94138..."
}
```

### Service Info

```bash
curl http://oai-functions.local/scramble/info
```

## Kubernetes Commands

```bash
# View all pods
kubectl get pods -n oai-functions -o wide

# View logs
kubectl logs -n oai-functions -l app=nr-scramble -f

# Scale deployment
kubectl scale -n oai-functions deployment/nr-scramble --replicas=5

# Delete specific service
kubectl delete -n oai-functions deployment/nr-scramble
kubectl delete -n oai-functions svc/nr-scramble

# Delete all
kubectl delete namespace oai-functions
```

## Monitoring

### Prometheus Metrics (TODO)

```yaml
# Add to deployment annotations:
prometheus.io/scrape: "true"
prometheus.io/port: "8001"
prometheus.io/path: "/metrics"
```

### Grafana Dashboard (TODO)

Import dashboard from `monitoring/grafana-dashboard.json`

## CI/CD Pipeline

### GitHub Actions (TODO)

`.github/workflows/build-and-deploy.yml`:
- Build images on push to main
- Run tests
- Push to registry
- Deploy to staging cluster
- Manual approval for production

## Troubleshooting

### Pod not starting

```bash
kubectl describe pod -n oai-functions <pod-name>
kubectl logs -n oai-functions <pod-name>
```

### Image pull errors

```bash
# Check imagePullSecrets
kubectl get secrets -n oai-functions

# Create registry secret
kubectl create secret docker-registry regcred \
  --docker-server=<registry> \
  --docker-username=<username> \
  --docker-password=<password> \
  -n oai-functions
```

### Service not accessible

```bash
# Check ingress
kubectl get ingress -n oai-functions
kubectl describe ingress -n oai-functions oai-functions-ingress

# Port forward for direct access
kubectl port-forward -n oai-functions svc/nr-scramble 8001:80
```

## Resource Requirements

**Per Pod:**
- Minimum: 100m CPU, 128Mi RAM
- Maximum: 300m CPU, 512Mi RAM

**Total Cluster (15 services, 31 pods):**
- CPU: ~6-7 cores
- Memory: ~8-10 GB

## Next Steps

- [ ] Add Prometheus metrics endpoint
- [ ] Create Grafana dashboards
- [ ] Implement CI/CD pipeline
- [ ] Add horizontal pod autoscaling (HPA)
- [ ] Implement request caching
- [ ] Add authentication/authorization
- [ ] Create Helm chart for easier deployment

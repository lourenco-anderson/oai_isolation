#!/usr/bin/env python3
"""
Generate Kubernetes manifests for all OAI functions
"""
from pathlib import Path

FUNCTIONS = [
    {"name": "nr_scramble", "port": 8001, "replicas": 2, "cpu": "200m", "mem": "256Mi"},
    {"name": "nr_crc", "port": 8002, "replicas": 2, "cpu": "100m", "mem": "128Mi"},
    {"name": "nr_ofdm_modulation", "port": 8003, "replicas": 2, "cpu": "200m", "mem": "256Mi"},
    {"name": "nr_precoding", "port": 8004, "replicas": 2, "cpu": "150m", "mem": "192Mi"},
    {"name": "nr_modulation_test", "port": 8005, "replicas": 1, "cpu": "150m", "mem": "192Mi"},
    {"name": "nr_layermapping", "port": 8006, "replicas": 2, "cpu": "150m", "mem": "192Mi"},
    {"name": "nr_ldpc", "port": 8007, "replicas": 2, "cpu": "250m", "mem": "512Mi"},
    {"name": "nr_ofdm_demo", "port": 8008, "replicas": 1, "cpu": "150m", "mem": "192Mi"},
    {"name": "nr_ch_estimation", "port": 8009, "replicas": 2, "cpu": "200m", "mem": "256Mi"},
    {"name": "nr_descrambling", "port": 8010, "replicas": 2, "cpu": "200m", "mem": "256Mi"},
    {"name": "nr_layer_demapping_test", "port": 8011, "replicas": 2, "cpu": "150m", "mem": "192Mi"},
    {"name": "nr_crc_check", "port": 8012, "replicas": 2, "cpu": "150m", "mem": "192Mi"},
    {"name": "nr_soft_demod", "port": 8013, "replicas": 2, "cpu": "200m", "mem": "256Mi"},
    {"name": "nr_mmse_eq", "port": 8014, "replicas": 2, "cpu": "250m", "mem": "384Mi"},
    {"name": "nr_ldpc_dec", "port": 8015, "replicas": 3, "cpu": "300m", "mem": "512Mi"},
]

DEPLOYMENT_TEMPLATE = '''apiVersion: apps/v1
kind: Deployment
metadata:
  name: {name}
  namespace: oai-functions
  labels:
    app: {name}
    component: oai-function
spec:
  replicas: {replicas}
  selector:
    matchLabels:
      app: {name}
  template:
    metadata:
      labels:
        app: {name}
        component: oai-function
    spec:
      containers:
      - name: {name}
        image: {registry}/{name}:latest
        imagePullPolicy: Always
        ports:
        - containerPort: {port}
          name: http
          protocol: TCP
        env:
        - name: PORT
          value: "{port}"
        - name: OAI_LIB_PATH
          value: "/usr/local/lib/liboai_functions.so"
        resources:
          requests:
            memory: "{mem}"
            cpu: "{cpu}"
          limits:
            memory: "{mem_limit}"
            cpu: "{cpu_limit}"
        livenessProbe:
          httpGet:
            path: /health
            port: {port}
          initialDelaySeconds: 15
          periodSeconds: 10
          timeoutSeconds: 5
          failureThreshold: 3
        readinessProbe:
          httpGet:
            path: /ready
            port: {port}
          initialDelaySeconds: 10
          periodSeconds: 5
          timeoutSeconds: 3
          failureThreshold: 3
      restartPolicy: Always
'''

SERVICE_TEMPLATE = '''apiVersion: v1
kind: Service
metadata:
  name: {name}
  namespace: oai-functions
  labels:
    app: {name}
    component: oai-function
spec:
  type: ClusterIP
  selector:
    app: {name}
  ports:
  - port: 80
    targetPort: {port}
    protocol: TCP
    name: http
'''


def parse_resource(value):
    """Parse resource value and calculate limit (2x request)"""
    if value.endswith('Mi'):
        base = int(value[:-2])
        return f"{base * 2}Mi"
    elif value.endswith('m'):
        base = int(value[:-1])
        return f"{base * 2}m"
    return value


def generate_manifests(func):
    """Generate Kubernetes manifests for a function"""
    name = func["name"]
    
    # Calculate resource limits (2x requests)
    mem_limit = parse_resource(func["mem"])
    cpu_limit = parse_resource(func["cpu"])
    
    # Generate Deployment
    deployment = DEPLOYMENT_TEMPLATE.format(
        name=name,
        registry="${DOCKER_REGISTRY:-docker.io/your-username}",
        port=func["port"],
        replicas=func["replicas"],
        cpu=func["cpu"],
        mem=func["mem"],
        cpu_limit=cpu_limit,
        mem_limit=mem_limit
    )
    
    # Generate Service
    service = SERVICE_TEMPLATE.format(
        name=name,
        port=func["port"]
    )
    
    # Write files
    Path(f"kubernetes/deployments/{name}.yaml").write_text(deployment)
    Path(f"kubernetes/services/{name}.yaml").write_text(service)
    
    print(f"✅ Generated manifests: {name}")


def generate_namespace():
    """Generate namespace manifest"""
    namespace = '''apiVersion: v1
kind: Namespace
metadata:
  name: oai-functions
  labels:
    name: oai-functions
    component: oai-phy-layer
'''
    Path("kubernetes/namespace.yaml").write_text(namespace)
    print("✅ Generated namespace.yaml")


def generate_ingress():
    """Generate ingress manifest"""
    ingress_routes = []
    for func in FUNCTIONS:
        endpoint = func["name"].replace("nr_", "")
        ingress_routes.append(f'''  - path: /{endpoint}
    pathType: Prefix
    backend:
      service:
        name: {func["name"]}
        port:
          number: 80''')
    
    ingress = f'''apiVersion: networking.k8s.io/v1
kind: Ingress
metadata:
  name: oai-functions-ingress
  namespace: oai-functions
  annotations:
    nginx.ingress.kubernetes.io/rewrite-target: /$1
    nginx.ingress.kubernetes.io/ssl-redirect: "false"
spec:
  ingressClassName: nginx
  rules:
  - host: oai-functions.local
    http:
      paths:
{chr(10).join(ingress_routes)}
'''
    Path("kubernetes/ingress.yaml").write_text(ingress)
    print("✅ Generated ingress.yaml")


def generate_configmap():
    """Generate ConfigMap for shared configuration"""
    configmap = '''apiVersion: v1
kind: ConfigMap
metadata:
  name: oai-functions-config
  namespace: oai-functions
data:
  OAI_LIB_PATH: "/usr/local/lib/liboai_functions.so"
  LOG_LEVEL: "INFO"
'''
    Path("kubernetes/configmap.yaml").write_text(configmap)
    print("✅ Generated configmap.yaml")


def main():
    """Generate all Kubernetes manifests"""
    print("=" * 60)
    print("Generating Kubernetes manifests for OAI functions")
    print("=" * 60)
    
    generate_namespace()
    generate_configmap()
    
    for func in FUNCTIONS:
        generate_manifests(func)
    
    generate_ingress()
    
    print("=" * 60)
    print(f"✅ Generated manifests for {len(FUNCTIONS)} services")
    print("=" * 60)


if __name__ == "__main__":
    main()

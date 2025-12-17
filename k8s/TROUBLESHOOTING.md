# Troubleshooting Guide - OAI Isolation + Kubernetes

## 🔧 Problemas Comuns e Soluções

### 1. Pods não iniciam (Pending)

**Sintomas:**
```bash
$ kubectl get pods -n oai-isolation
NAME                          READY   STATUS    RESTARTS   AGE
gnb-crc-7d8f9c6b5-abc12       0/1     Pending   0          5m
```

**Causas possíveis:**
- Recursos insuficientes no cluster
- Imagem Docker não encontrada
- Persistent Volume não disponível

**Soluções:**

```bash
# 1. Verificar descrição do pod
kubectl describe pod gnb-crc-7d8f9c6b5-abc12 -n oai-isolation

# Procurar por:
# - "Insufficient memory" ou "Insufficient cpu"
# - "ImagePullBackOff"
# - "SchedulingFailed"

# 2. Verificar recursos disponíveis no cluster
kubectl top nodes

# 3. Se for Kind, aumentar resources
kind delete cluster --name oai-isolation-cluster
# Editar kind-setup.sh e aumentar --memory e --cpus

# 4. Se for imagem, verificar se existe
docker images | grep oai-isolation
# Se não existir, fazer build
./build-images.sh

# 5. Se usar Kind, carregar imagem
kind load docker-image oai-isolation:gnb-crc --name oai-isolation-cluster
```

---

### 2. Pods em CrashLoopBackOff

**Sintomas:**
```bash
$ kubectl get pods -n oai-isolation
NAME                          READY   STATUS             RESTARTS   AGE
gnb-crc-7d8f9c6b5-abc12       0/1     CrashLoopBackOff   5          2m
```

**Causas possíveis:**
- Aplicação falha ao iniciar
- Biblioteca compartilhada faltando
- Comando de inicialização errado

**Soluções:**

```bash
# 1. Ver logs do container
kubectl logs gnb-crc-7d8f9c6b5-abc12 -n oai-isolation

# 2. Ver logs anteriores (se crashou rapidamente)
kubectl logs gnb-crc-7d8f9c6b5-abc12 -n oai-isolation --previous

# 3. Entrar no container para debug (antes de falhar)
kubectl run -it --rm debug --image=oai-isolation:gnb-crc --restart=Never -- bash

# 4. Verificar se Dockerfile está correto
docker build -f containers/gnb/crc/Dockerfile -t oai-isolation:gnb-crc .

# 5. Testar imagem localmente
docker run --rm -it oai-isolation:gnb-crc bash
# Dentro do container, verificar se binários e libs existem
ls -la /app/oai_isolation
ldd /app/oai_isolation  # Verificar dependências
```

---

### 3. ImagePullBackOff

**Sintomas:**
```bash
$ kubectl get pods -n oai-isolation
NAME                          READY   STATUS              RESTARTS   AGE
gnb-crc-7d8f9c6b5-abc12       0/1     ImagePullBackOff    0          1m
```

**Causas possíveis:**
- Imagem não existe no registry
- Registry não está acessível
- ImagePullPolicy configurado incorretamente

**Soluções:**

```bash
# 1. Verificar imagem existe
docker images | grep oai-isolation:gnb-crc

# 2. Se não existir, fazer build
./build-images.sh localhost:5000 latest

# 3. Se usar Kind, carregar imagem no cluster
kind load docker-image oai-isolation:gnb-crc

# 4. Verificar ImagePullPolicy no manifest
kubectl get deployment gnb-crc -n oai-isolation -o yaml | grep imagePullPolicy
# Deve ser "IfNotPresent" para Kind local

# 5. Forçar re-pull da imagem
kubectl set image deployment/gnb-crc \
  gnb-crc=oai-isolation:gnb-crc \
  -n oai-isolation --record
```

---

### 4. Pods não conseguem se comunicar

**Sintomas:**
```bash
# Dentro de um pod
$ nslookup gnb-crc.oai-isolation.svc.cluster.local
nslookup: can't resolve 'gnb-crc.oai-isolation.svc.cluster.local'
```

**Causas possíveis:**
- DNS não configurado
- Network plugin não instalado
- Services não criados

**Soluções:**

```bash
# 1. Verificar se Service DNS está resolvendo
kubectl run -it --rm debug --image=busybox --restart=Never -- nslookup gnb-crc.oai-isolation.svc.cluster.local

# 2. Verificar se Services foram criados
kubectl get svc -n oai-isolation

# 3. Se não existirem, aplicar manifests novamente
kustomize build . | kubectl apply -f -

# 4. Verificar configuração de DNS no pod
kubectl run -it --rm debug --image=busybox --restart=Never -- cat /etc/resolv.conf

# 5. Verificar network plugin
kubectl get daemonset -n kube-system
# Deve ter flannel, calico, weave, etc.

# 6. Testar conectividade TCP/UDP
kubectl run -it --rm debug --image=busybox --restart=Never -- nc -zv gnb-crc.oai-isolation.svc.cluster.local 8080
```

---

### 5. Out of Memory (OOM)

**Sintomas:**
```bash
$ kubectl get pods -n oai-isolation
NAME                          READY   STATUS      RESTARTS   AGE
gnb-crc-7d8f9c6b5-abc12       1/1     OOMKilled   2          3m
```

**Causas possíveis:**
- Memory limit muito baixo
- Memory leak na aplicação
- Mais replicas do que o esperado

**Soluções:**

```bash
# 1. Aumentar memory limit
kubectl set resources deployment gnb-crc \
  -n oai-isolation \
  --limits=memory=1Gi,cpu=1000m \
  --requests=memory=512Mi,cpu=500m

# 2. Monitorar uso de memória
kubectl top pods -n oai-isolation

# 3. Ver histórico de uso
kubectl describe pod gnb-crc-7d8f9c6b5-abc12 -n oai-isolation | grep -A 5 "Last State"

# 4. Reduzir number of replicas
kubectl scale deployment gnb-crc -n oai-isolation --replicas=1

# 5. Aumentar memory do node (se usar Kind)
kind delete cluster --name oai-isolation-cluster
# Editar kind-setup.sh com mais memória
./kind-setup.sh create
```

---

### 6. CPU Throttling

**Sintomas:**
- Pods ficam lentos
- Container tomando mais tempo que o esperado

**Soluções:**

```bash
# 1. Verificar limites atuais
kubectl get deployment gnb-crc -n oai-isolation -o yaml | grep -A 5 resources

# 2. Aumentar CPU limit
kubectl set resources deployment gnb-crc \
  -n oai-isolation \
  --limits=cpu=2000m \
  --requests=cpu=1000m

# 3. Monitorar uso de CPU
kubectl top pods -n oai-isolation

# 4. Verificar requests vs limits
# Ensure: requests <= limits
# Exemplo bom:
#   requests: 500m
#   limits: 1000m
```

---

### 7. Service Discovery não funciona

**Sintomas:**
```bash
# Pod não consegue conectar via DNS
$ curl http://gnb-crc.oai-isolation.svc.cluster.local:8080
curl: (6) Could not resolve host
```

**Soluções:**

```bash
# 1. Verificar CoreDNS
kubectl get pods -n kube-system | grep coredns
kubectl logs -n kube-system coredns-<id>

# 2. Verificar ConfigMap de DNS
kubectl get configmap -n kube-system coredns -o yaml

# 3. Restartar CoreDNS
kubectl rollout restart deployment coredns -n kube-system

# 4. Verificar resolução interna
kubectl run -it --rm debug --image=busybox --restart=Never -- nslookup kubernetes.default

# 5. Verificar endpoints do Service
kubectl get endpoints -n oai-isolation
# Deve mostrar pods como endpoints
```

---

### 8. Pod não consegue acessar volumes

**Sintomas:**
```bash
# Dentro do pod
$ ls /app
ls: cannot access '/app': No such file or directory
```

**Soluções:**

```bash
# 1. Verificar volume está montado
kubectl describe pod gnb-crc-7d8f9c6b5-abc12 -n oai-isolation | grep -A 5 "Mounts:"

# 2. Verificar permissões
kubectl exec gnb-crc-7d8f9c6b5-abc12 -n oai-isolation -- ls -la /

# 3. Verificar se volume existe
kubectl get pv
kubectl get pvc -n oai-isolation

# 4. Se usar emptyDir, verificar se pod consegue escrever
kubectl exec gnb-crc-7d8f9c6b5-abc12 -n oai-isolation -- touch /app/test.txt
```

---

### 9. Deployment não atualiza

**Sintomas:**
```bash
# Fez mudanças no manifest mas pods não atualizam
$ kubectl apply -f gnb-crc-deployment.yaml
# Nenhuma mudança aplicada
```

**Soluções:**

```bash
# 1. Forçar atualização
kubectl rollout restart deployment gnb-crc -n oai-isolation

# 2. Verificar histórico de rollout
kubectl rollout history deployment gnb-crc -n oai-isolation

# 3. Voltar para versão anterior
kubectl rollout undo deployment gnb-crc -n oai-isolation

# 4. Se usar Kustomize, reconstruir
kustomize build . | kubectl apply -f -

# 5. Deletar deployment e recriar
kubectl delete deployment gnb-crc -n oai-isolation
kubectl apply -f gnb-crc-deployment.yaml
```

---

### 10. Cluster Kind não inicia

**Sintomas:**
```bash
$ ./kind-setup.sh create
ERROR: failed to create cluster: ... bind: permission denied
```

**Soluções:**

```bash
# 1. Verificar se Docker está rodando
docker ps

# 2. Se não estiver, iniciar
sudo systemctl start docker

# 3. Verificar permissões Docker
docker run hello-world
# Se não funcionar, adicionar user ao grupo docker
sudo usermod -aG docker $USER
newgrp docker

# 4. Verificar se porta já está em uso
lsof -i :6443
# Se estiver, deletar cluster existente
kind delete cluster --name oai-isolation-cluster

# 5. Aumentar memória virtual
free -h
# Se memória for insuficiente, fechar outras aplicações
```

---

## 📊 Comandos de Debug Úteis

### Inspecionar Recursos
```bash
# Ver status detalhado
kubectl describe pod <pod-name> -n oai-isolation

# Ver YAML do recurso
kubectl get pod <pod-name> -n oai-isolation -o yaml

# Ver eventos recentes
kubectl get events -n oai-isolation --sort-by='.lastTimestamp'
```

### Acessar Containers
```bash
# Executar comando dentro do pod
kubectl exec -it <pod-name> -n oai-isolation -- bash

# Copiar arquivo do pod
kubectl cp oai-isolation/<pod-name>:/app/file.txt ./local-file.txt

# Copiar arquivo para o pod
kubectl cp ./local-file.txt oai-isolation/<pod-name>:/app/file.txt
```

### Monitorar Recursos
```bash
# Ver uso de CPU/Memória (requer metrics-server)
kubectl top pods -n oai-isolation
kubectl top nodes

# Ver logs em tempo real
kubectl logs -f <pod-name> -n oai-isolation

# Ver logs dos últimos N linhas
kubectl logs --tail=100 <pod-name> -n oai-isolation
```

### Testar Conectividade
```bash
# Pod de debug
kubectl run -it --rm debug --image=busybox --restart=Never -- sh

# Dentro do pod
nslookup gnb-crc.oai-isolation.svc.cluster.local
nc -zv gnb-crc.oai-isolation.svc.cluster.local 8080
curl http://gnb-crc.oai-isolation.svc.cluster.local:8080
```

---

## 🔗 Recursos Externos

- [Kubernetes Official Debugging](https://kubernetes.io/docs/tasks/debug-application-cluster/)
- [Kind Troubleshooting](https://kind.sigs.k8s.io/docs/user/known-issues/)
- [Docker Troubleshooting](https://docs.docker.com/config/daemon/troubleshoot/)
- [CNI Plugins](https://kubernetes.io/docs/concepts/extend-kubernetes/compute-storage-net/network-plugins/)

---

## 📞 Checklist de Troubleshooting

1. **Pod Status**
   - [ ] `kubectl get pods -n oai-isolation`
   - [ ] `kubectl describe pod <pod-name> -n oai-isolation`
   - [ ] `kubectl logs <pod-name> -n oai-isolation`

2. **Recursos**
   - [ ] `kubectl top nodes`
   - [ ] `kubectl top pods -n oai-isolation`
   - [ ] `kubectl describe node`

3. **Network**
   - [ ] `kubectl get svc -n oai-isolation`
   - [ ] `kubectl get endpoints -n oai-isolation`
   - [ ] Testar DNS dentro de um debug pod

4. **Volumes**
   - [ ] `kubectl get pv`
   - [ ] `kubectl get pvc -n oai-isolation`
   - [ ] Verificar mount paths

5. **Events**
   - [ ] `kubectl get events -n oai-isolation`
   - [ ] Procurar por warnings ou errors

---

**Última atualização**: Dezembro 2025

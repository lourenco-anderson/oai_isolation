# Build & Run de todos os containers (gnb + ue)

Este guia mostra como **construir e subir de uma vez** todos os containers definidos em `containers/gnb/*` e `containers/ue/*` usando Docker Compose.

## Pré-requisitos
- Docker e Docker Compose instalados.
- Artefatos já construídos localmente (binário e libs copiadas pelos Dockerfiles):
  - `build/oai_isolation`
  - `ext/openair/cmake_targets/ran_build/build/libnrscope.so`
  - Outras `.so` geradas em `ext/openair/cmake_targets/ran_build/build/`

> Dica: se o build ainda não foi feito, rode na raiz do repo:
> ```bash
> ./ext/openair/cmake_targets/build_oai -w USRP --ninja --nrUE --gNB --build-lib "nrscope" -C
> ```

## Opção 1: Usar docker-compose.yaml da raiz (recomendado)
Na raiz do repositório já existe o arquivo `docker-compose.yaml` com todos os serviços.

1) Build + run de todos os serviços em background:
   ```bash
   docker compose up --build -d
   ```

2) Verificar status:
   ```bash
   docker compose ps
   ```

3) Logs de um serviço específico (ex.: gNB scramble):
   ```bash
   docker compose logs -f gnb-scramble
   ```

4) Parar e limpar os containers:
   ```bash
   docker compose down --remove-orphans
   ```

5) Build sem subir (apenas gerar as imagens):
   ```bash
   docker compose build
   ```

6) Rodar apenas alguns serviços (ex.: só gNB):
   ```bash
   docker compose up -d gnb-crc gnb-layer-map gnb-ldpc gnb-modulation gnb-ofdm-mod gnb-precoding gnb-scramble
   ```

## Opção 2: Gerar docker-compose temporário
Se preferir não usar o arquivo da raiz ou quiser customizar:

1) Gere um Compose temporário com todos os serviços:
   ```bash
   cat > /tmp/oai-isolation-compose.yaml <<'EOF'
   version: "3.9"
   services:
     gnb-crc:
       image: oai-gnb-crc:latest
       build:
         context: .
         dockerfile: containers/gnb/crc/Dockerfile
     gnb-layer-map:
       image: oai-gnb-layer-map:latest
       build:
         context: .
         dockerfile: containers/gnb/layer_map/Dockerfile
     gnb-ldpc:
       image: oai-gnb-ldpc:latest
       build:
         context: .
         dockerfile: containers/gnb/ldpc/Dockerfile
     gnb-modulation:
       image: oai-gnb-modulation:latest
       build:
         context: .
         dockerfile: containers/gnb/modulation/Dockerfile
     gnb-ofdm-mod:
       image: oai-gnb-ofdm-mod:latest
       build:
         context: .
         dockerfile: containers/gnb/ofdm_mod/Dockerfile
     gnb-precoding:
       image: oai-gnb-precoding:latest
       build:
         context: .
         dockerfile: containers/gnb/precoding/Dockerfile
     gnb-scramble:
       image: oai-gnb-scramble:latest
       build:
         context: .
         dockerfile: containers/gnb/scramble/Dockerfile

     ue-ch-est:
       image: oai-ue-ch-est:latest
       build:
         context: .
         dockerfile: containers/ue/ch_est/Dockerfile
     ue-ch-mmse:
       image: oai-ue-ch-mmse:latest
       build:
         context: .
         dockerfile: containers/ue/ch_mmse/Dockerfile
     ue-check-crc:
       image: oai-ue-check-crc:latest
       build:
         context: .
         dockerfile: containers/ue/check_crc/Dockerfile
     ue-descrambling:
       image: oai-ue-descrambling:latest
       build:
         context: .
         dockerfile: containers/ue/descrambling/Dockerfile
     ue-layer-demap:
       image: oai-ue-layer-demap:latest
       build:
         context: .
         dockerfile: containers/ue/layer_demap/Dockerfile
     ue-ldpc-dec:
       image: oai-ue-ldpc-dec:latest
       build:
         context: .
         dockerfile: containers/ue/ldpc_dec/Dockerfile
     ue-ofdm-demod:
       image: oai-ue-ofdm-demod:latest
       build:
         context: .
         dockerfile: containers/ue/ofdm_demod/Dockerfile
     ue-soft-demod:
       image: oai-ue-soft-demod:latest
       build:
         context: .
         dockerfile: containers/ue/soft_demod/Dockerfile
   EOF
   ```

2) Build + run de todos os serviços em background:
   ```bash
   docker compose -f /tmp/oai-isolation-compose.yaml up --build -d
   ```

3) Verificar status:
   ```bash
   docker compose -f /tmp/oai-isolation-compose.yaml ps
   ```

4) Logs de um serviço específico (ex.: gNB scramble):
   ```bash
   docker compose -f /tmp/oai-isolation-compose.yaml logs -f gnb-scramble
   ```

5) Parar e limpar os containers:
   ```bash
   docker compose -f /tmp/oai-isolation-compose.yaml down --remove-orphans
   ```

## Build manual em lote (alternativo)
Se preferir apenas gerar as imagens (sem subir), use loops:
```bash
cd /home/anderson/dev/oai_isolation
components_gnb=(crc layer_map ldpc modulation ofdm_mod precoding scramble)
components_ue=(ch_est ch_mmse check_crc descrambling layer_demap ldpc_dec ofdm_demod soft_demod)
for c in "${components_gnb[@]}"; do
  docker build -t oai-gnb-${c}:latest -f containers/gnb/${c}/Dockerfile .
done
for c in "${components_ue[@]}"; do
  docker build -t oai-ue-${c}:latest -f containers/ue/${c}/Dockerfile .
done
```
Depois, suba qualquer imagem individual com `docker run --rm -it oai-gnb-scramble:latest` (ou a correspondente).

## Notas
- Os containers usam o binário `oai_isolation` e libs geradas localmente; se rebuildar o código, execute novamente o `docker compose ... --build` para atualizar as imagens.
- O comando padrão de cada imagem executa a função isolada correspondente (ver `CMD` nos Dockerfiles). Se ela terminar imediatamente, o container sairá; use `docker logs <serviço>` para ver a saída.

# OAI ISOLATION

## Getting started

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
# OAI ISOLATION

## Getting started

Clone this repo. Run `git submodule init` then follow the steps [here](https://gitlab.eurecom.fr/oai/openairinterface5g/-/blob/develop/doc/NR_SA_Tutorial_OAI_nrUE.md).

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


## My Functions

```bash
./find_my_function <function_name>
``` 
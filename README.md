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
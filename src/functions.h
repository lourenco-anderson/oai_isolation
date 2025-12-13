#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include <string.h>
#include "PHY/NR_TRANSPORT/nr_transport_common_proto.h"

#define POLY 0x1864CFB
#define INIT 0xB704CE
#define N 24072

#include "PHY/CODING/coding_defs.h"

void nr_scramble();
void nr_crc();
void nr_ldpc();
void nr_modulation_test();
void nr_layermapping();
void nr_precoding();
void nr_ofdm_modulation();
void nr_ofdm_demo();
void nr_ch_estimation();
void nr_mmse_eq();
void nr_layer_demapping_test();
void nr_soft_demod();
void nr_descrambling();
void nr_ldpc_dec();
void nr_crc_check();

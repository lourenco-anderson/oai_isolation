#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include <string.h>
#include "PHY/NR_TRANSPORT/nr_transport_common_proto.h"
#include "common/utils/LOG/log.h"
#include "PHY/MODULATION/nr_modulation.h"
#include "PHY/MODULATION/modulation_common.h"
#include "softmodem-common.h"
#include "PHY/impl_defs_top.h"
#include "nr_dlsch_onelayer.h"

#define POLY 0x1864CFB
#define INIT 0xB704CE
#define N 24072

// #include "PHY/CODING/coding_defs.h"

void nr_scramble();
void nr_crc();
void nr_ofdm_modulation();
void nr_layermapping();
void nr_ldpc();
void nr_precoding();
void nr_modulation_test();

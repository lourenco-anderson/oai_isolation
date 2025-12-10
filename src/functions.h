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

/* SIMD initialization needed for unscrambling */
extern void init_byte2m128i(void);

#define POLY 0x1864CFB
#define INIT 0xB704CE
#define N 24072

/* Forward declaration of PHY_VARS_NR_UE (avoid including defs_nr_UE.h due to dependencies) */
typedef struct PHY_VARS_NR_UE_s PHY_VARS_NR_UE;

// #include "PHY/CODING/coding_defs.h"

void nr_scramble();
void nr_crc();
void nr_ofdm_modulation();
void nr_layermapping();
void nr_ldpc();
void nr_precoding();
void nr_modulation_test();
void nr_ofdm_demo();
void nr_ch_estimation();
void nr_descrambling();

/* OAI real function declarations - using simple signatures to avoid header dependencies */
int nr_slot_fep(void *ue, const void *frame_parms, unsigned int slot,
                unsigned int symbol, void *rxdataF, int linktype,
                uint32_t sample_offset, void *rxdata);

void nr_pdsch_channel_estimation(void *ue, const void *frame_parms,
                                  unsigned int symbol, uint8_t gNB_id,
                                  uint8_t nb_antennas_rx, void *dl_ch,
                                  void *rxdataF, uint32_t *nvar);

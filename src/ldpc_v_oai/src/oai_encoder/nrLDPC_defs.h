/* ABI-compatible standalone view of OAI nrLDPC_defs.h for encoder build. */
#pragma once

#include <stdint.h>
#include <openair1/PHY/defs_nr_common.h>
#include "openair1/PHY/CODING/nrLDPC_decoder/nrLDPC_types.h"

typedef struct {
    unsigned int n_segments;
    unsigned int first_seg;
    unsigned char gen_code;
    time_stats_t *tinput;
    time_stats_t *tprep;
    time_stats_t *tparity;
    time_stats_t *toutput;
    uint32_t K;
    uint32_t Kb;
    uint32_t Zc;
    uint32_t F;
    uint8_t BG;
    unsigned char *output;
    task_ans_t *ans;
} encoder_implemparams_t;

int LDPCencoder(uint8_t **input, uint8_t *output, encoder_implemparams_t *impp);

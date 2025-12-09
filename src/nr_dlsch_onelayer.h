#ifndef NR_DLSCH_ONELAYER_H
#define NR_DLSCH_ONELAYER_H

#include "common/platform_types.h"
#include "PHY/NR_TRANSPORT/nr_transport_common_proto.h"
#include "nfapi_nr_interface_scf.h"

/* Main precoding layer mapping function */
int do_onelayer(NR_DL_FRAME_PARMS *frame_parms,
                int slot,
                nfapi_nr_dl_tti_pdsch_pdu_rel15_t *rel15,
                int layer,
                c16_t *output,
                c16_t *txl_start,
                int start_sc,
                int symbol_sz,
                int l_symbol,
                uint16_t dlPtrsSymPos,
                int n_ptrs,
                int amp,
                int16_t amp_dmrs,
                int l_prime,
                nfapi_nr_dmrs_type_e dmrs_Type,
                c16_t *dmrs_start);

#endif

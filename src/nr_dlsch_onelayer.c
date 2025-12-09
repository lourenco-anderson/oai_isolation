/* 
 * Extracted from openair/openair1/PHY/NR_TRANSPORT/nr_dlsch.c
 * Contains do_onelayer and helper functions for precoding layer mapping
 */

#include "nr_dlsch_onelayer.h"
#include "common/platform_types.h"
#include "PHY/MODULATION/nr_modulation.h"
#include <string.h>

/* Helper functions extracted from nr_dlsch.c */

/* Check if a given symbol is a PTRS symbol */
static inline int is_ptrs_symbol(int l_symbol, uint16_t dlPtrsSymPos)
{
    return (dlPtrsSymPos >> l_symbol) & 1;
}

/* Interleave DMRS with zeros (signal first) */
static inline int interleave_with_0_signal_first(c16_t *output, c16_t *mod_dmrs, const int16_t amp_dmrs, int sz)
{
    c16_t *out = output;
    for (int i = 0; i < sz / 2; i++) {
        *out++ = c16mulRealShift(mod_dmrs[i], amp_dmrs, 15);
        *out++ = (c16_t){0, 0};
    }
    return sz;
}

/* Interleave DMRS with zeros (zero first) */
static inline int interleave_with_0_start_with_0(c16_t *output, c16_t *mod_dmrs, const int16_t amp_dmrs, int sz)
{
    c16_t *out = output;
    for (int i = 0; i < sz / 2; i++) {
        *out++ = (c16_t){0, 0};
        *out++ = c16mulRealShift(mod_dmrs[i], amp_dmrs, 15);
    }
    return sz;
}

/* Interleave two signals */
static inline int interleave_signals(c16_t *output, c16_t *signal1, const int amp, c16_t *signal2, const int amp2, int sz)
{
    c16_t *out = output;
    for (int i = 0; i < sz / 2; i++) {
        *out++ = c16mulRealShift(signal1[i], amp, 15);
        *out++ = c16mulRealShift(signal2[i], amp2, 15);
    }
    return sz;
}

/* Negate DMRS signal */
static inline void neg_dmrs(c16_t *in, c16_t *out, int sz)
{
    for (int i = 0; i < sz; i++) {
        out[i].r = -in[i].r;
        out[i].i = -in[i].i;
    }
}

/* Process case with no PTRS/DMRS */
static inline int no_ptrs_dmrs_case(c16_t *output, c16_t *txl, const int amp, const int sz)
{
    for (int i = 0; i < sz; i++) {
        output[i] = c16mulRealShift(txl[i], amp, 15);
    }
    return sz;
}

/* Main do_onelayer function - maps one layer to output with basic DMRS/PTRS handling */
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
                c16_t *dmrs_start)
{
    c16_t *txl = txl_start;
    const int sz = rel15->rbSize * 12;  /* 12 subcarriers per RB */
    int upper_limit = sz;
    int remaining_re = 0;
    
    if (start_sc + upper_limit > symbol_sz) {
        upper_limit = symbol_sz - start_sc;
        remaining_re = sz - upper_limit;
    }

    /* Calculate if current symbol is PTRS symbol */
    int ptrs_symbol = 0;
    if (rel15->pduBitmap & 0x1) {
        ptrs_symbol = is_ptrs_symbol(l_symbol, dlPtrsSymPos);
    }

    if (ptrs_symbol) {
        /* PTRS modulation - skip for simplified test */
        txl += upper_limit;
        if (remaining_re > 0) txl += remaining_re;
    } 
    else if (rel15->dlDmrsSymbPos & (1 << l_symbol)) {
        /* DMRS symbol - handle basic case */
        int dmrs_port = layer % 4;  /* Simplified - use layer as port */
        
        if (l_prime == 0 && dmrs_Type == NFAPI_NR_DMRS_TYPE1) {
            if (rel15->numDmrsCdmGrpsNoData == 2) {
                /* Type 1, 2 CDM groups - DMRS-only symbols */
                if (dmrs_port == 0) {
                    txl += interleave_with_0_signal_first(output + start_sc, dmrs_start, amp_dmrs, upper_limit);
                    if (remaining_re > 0)
                        txl += interleave_with_0_signal_first(output, dmrs_start + upper_limit / 2, amp_dmrs, remaining_re);
                } 
                else if (dmrs_port == 1) {
                    c16_t dmrs[sz / 2];
                    neg_dmrs(dmrs_start, dmrs, sz / 2);
                    txl += interleave_with_0_signal_first(output + start_sc, dmrs, amp_dmrs, upper_limit);
                    if (remaining_re > 0)
                        txl += interleave_with_0_signal_first(output, dmrs + upper_limit / 2, amp_dmrs, remaining_re);
                } 
                else if (dmrs_port == 2) {
                    txl += interleave_with_0_start_with_0(output + start_sc, dmrs_start, amp_dmrs, upper_limit);
                    if (remaining_re > 0)
                        txl += interleave_with_0_start_with_0(output, dmrs_start + upper_limit / 2, amp_dmrs, remaining_re);
                }
                else {
                    c16_t dmrs[sz / 2];
                    neg_dmrs(dmrs_start, dmrs, sz / 2);
                    txl += interleave_with_0_start_with_0(output + start_sc, dmrs, amp_dmrs, upper_limit);
                    if (remaining_re > 0)
                        txl += interleave_with_0_start_with_0(output, dmrs + upper_limit / 2, amp_dmrs, remaining_re);
                }
            } 
            else {
                /* Type 1, 1 CDM group - DMRS + data co-existence */
                txl += interleave_signals(output + start_sc, txl, amp, dmrs_start, amp_dmrs, upper_limit);
                if (remaining_re > 0)
                    txl += interleave_signals(output, txl, amp, dmrs_start + upper_limit / 2, amp_dmrs, remaining_re);
            }
        } 
        else {
            /* Generic DMRS case (Type 2 or other l_prime) */
            txl += no_ptrs_dmrs_case(output + start_sc, txl, amp, upper_limit);
            if (remaining_re > 0)
                txl += no_ptrs_dmrs_case(output, txl, amp, remaining_re);
        }
    } 
    else {
        /* No PTRS or DMRS in this symbol - map data directly */
        txl += no_ptrs_dmrs_case(output + start_sc, txl, amp, upper_limit);
        if (remaining_re > 0)
            txl += no_ptrs_dmrs_case(output, txl, amp, remaining_re);
    }
    
    return txl - txl_start;
}

#include "functions.h"

static void dispatch(const char *fn)
{
    /* gNB side */
    if (!strcmp(fn, "nr_crc"))          { nr_crc(); return; }
    if (!strcmp(fn, "nr_ldpc"))         { nr_ldpc(); return; }
    if (!strcmp(fn, "nr_scramble"))     { nr_scramble(); return; }
    if (!strcmp(fn, "nr_modulation"))   { nr_modulation_test(); return; }
    if (!strcmp(fn, "nr_layermapping")) { nr_layermapping(); return; }
    if (!strcmp(fn, "nr_precoding"))    { nr_precoding(); return; }
    if (!strcmp(fn, "nr_ofdm_mod"))     { nr_ofdm_modulation(); return; }

    /* UE side */
    if (!strcmp(fn, "nr_ofdm_demo"))        { nr_ofdm_demo(); return; }
    if (!strcmp(fn, "nr_ch_estimation"))    { nr_ch_estimation(); return; }
    if (!strcmp(fn, "nr_mmse_eq"))          { nr_mmse_eq(); return; }
    if (!strcmp(fn, "nr_layer_demapping"))  { nr_layer_demapping_test(); return; }
    if (!strcmp(fn, "nr_soft_demod"))       { nr_soft_demod(); return; }
    if (!strcmp(fn, "nr_descrambling"))     { nr_descrambling(); return; }
    if (!strcmp(fn, "nr_ldpc_dec"))         { nr_ldpc_dec(); return; }
    if (!strcmp(fn, "nr_crc_check"))        { nr_crc_check(); return; }

    printf("Unknown function '%s'.\n", fn);
}

int main(int argc, char **argv)
{
    const char *fn = (argc > 1) ? argv[1] : "nr_ch_estimation"; /* default to channel estimation */
    dispatch(fn);  
    return 0;
}
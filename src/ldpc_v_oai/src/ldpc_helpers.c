/*
 * ldpc_helpers.c  –  Funções utilitárias para o encoder LDPC standalone.
 */
#include <string.h>
#include <stdio.h>
#include "ldpc_encoder_isolated.h"

static const int ILS_ZC[8][9] = {
    {  2,  4,  8, 16,  32,  64, 128, 256, -1 },
    {  3,  6, 12, 24,  48,  96, 192, 384, -1 },
    {  5, 10, 20, 40,  80, 160, 320,  -1, -1 },
    {  7, 14, 28, 56, 112, 224,  -1,  -1, -1 },
    {  9, 18, 36, 72, 144, 288,  -1,  -1, -1 },
    { 11, 22, 44, 88, 176, 352,  -1,  -1, -1 },
    { 13, 26, 52,104, 208,  -1,  -1,  -1, -1 },
    { 15, 30, 60,120, 240,  -1,  -1,  -1, -1 },
};

int ldpc_get_iLS(int Zc)
{
    for (int ils = 0; ils < 8; ils++)
        for (int k = 0; ILS_ZC[ils][k] != -1; k++)
            if (ILS_ZC[ils][k] == Zc)
                return ils;
    return -1;
}

int ldpc_select_params(int BG, int Zc, encoder_implemparams_t *out)
{
    if (BG != 1 && BG != 2)    return -1;
    if (ldpc_get_iLS(Zc) < 0)  return -1;

    memset(out, 0, sizeof(*out));
    out->BG         = BG;
    out->Zc         = Zc;
    out->Kb         = (BG == 1) ? LDPC_BG1_KB : LDPC_BG2_KB;
    out->K          = (short)(out->Kb * Zc);  /* OAI: K=Kb*Zc; filler handled by caller */
    out->gen_code   = 1;
    out->n_segments = 1;
    out->first_seg  = 0;
    out->tinput     = NULL;
    out->tprep      = NULL;
    out->tparity    = NULL;
    out->toutput    = NULL;
    return 0;
}

int ldpc_encoder_init(void)
{
    fprintf(stderr, "[LDPC] Encoder OAI real (commit 5fcd456a, AVX2).\n");
    return 0;
}

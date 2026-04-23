/*
 * oai_encoder_adapter.c
 *
 * Wrapper mínimo que compila o encoder OAI como biblioteca standalone.
 *
 * Fonte  : /home/ndrs/oai_ldpc_standalone/openairinterface5g/openair1/PHY/CODING/nrLDPC_encoder
 * Commit : 5fcd456a4606cc5c772877e5459e61cf0538f55b
 *
 * O encoder OAI (ldpc_encoder_optim8segmulti.c) expõe:
 *   int LDPCencoder(uint8_t **input, uint8_t *output,
 *                   encoder_implemparams_t *impp)
 *
 * Esta API é idêntica à nossa encoder_implemparams_t – compilação direta.
 *
 * NOTA: impp->first_seg deve ser 0; impp->n_segments o total de segmentos.
 */
#include "nrLDPC_defs.h"

/*
 * O arquivo principal inclui todos os sub-arquivos via #include interno.
 * Basta compilar ldpc_encoder_optim8segmulti.c e tudo é puxado junto.
 */
#include "ldpc_encoder_optim8segmulti.c"

/*
 * ldpc_encoder_isolated.h  –  API pública do encoder LDPC standalone.
 *
 * NÃO redefine encoder_implemparams_t: inclui nrLDPC_defs.h diretamente.
 * Isso garante que há apenas UMA definição do struct em todo o projeto.
 */
#pragma once
#include <stdint.h>

/* Struct encoder_implemparams_t e LDPCencoder vêm daqui (fonte única): */
#include "nrLDPC_defs.h"

/* Constantes BG (3GPP TS 38.212 §5.2.2) */
#define LDPC_BG1_KB  22
#define LDPC_BG1_MB  46
#define LDPC_BG1_NB  68
#define LDPC_BG2_KB  10
#define LDPC_BG2_MB  42
#define LDPC_BG2_NB  52
#define LDPC_ZC_MAX  384
#define LDPC_MAX_CBS 52

/* Funções utilitárias */
int ldpc_get_iLS(int Zc);
int ldpc_select_params(int BG, int Zc, encoder_implemparams_t *out);
int ldpc_encoder_init(void);

/*
 * FORMATO DE SAÍDA:
 *   output[] é byte-per-bit: output[b] = 0x00 ou 0x01
 *   Tamanho: (K - 2*Zc) + (nrows - no_punc)*Zc - removed_bit  bytes
 *   K = Kb * Zc  (primeiros 2*Zc bits = filler, zerados no input)
 *   Retorno LDPCencoder: 0 = sucesso
 */
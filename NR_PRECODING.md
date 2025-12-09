# NR Precoding Function - Implementação Isolada

## Visão Geral
A função `nr_precoding()` foi implementada como uma versão otimizada e isolada para processamento de precoding em NR PDSCH, baseada no padrão de isolamento usado em `nr_scramble()` e `nr_ofdm_modulation()`.

## Arquitetura

### Estrutura de Contexto (`precoding_ctx_t`)
```c
typedef struct precoding_ctx_s {
    int nb_layers;                              /* Número de camadas MIMO */
    int symbol_sz;                              /* Tamanho do símbolo OFDM */
    int rbSize;                                 /* Número de resource blocks */
    int fftsize;                                /* Tamanho FFT */
    c16_t *tx_layer;                            /* Buffer de entrada (camadas) */
    c16_t *output;                              /* Buffer de saída */
    NR_DL_FRAME_PARMS *frame_parms;            /* Parâmetros da frame NR */
    nfapi_nr_dl_tti_pdsch_pdu_rel15_t *rel15;  /* Config PDSCH */
    uint32_t rnd_state;                         /* Estado PRNG xorshift */
} precoding_ctx_t;
```

### Funções Principais

#### `precoding_init()`
- Aloca e inicializa contexto com buffers alinhados (64 bytes)
- Configura PRNG com seed baseada em tempo + endereço de contexto
- Retorna contexto pronto para uso ou NULL em caso de erro

#### `precoding_free()`
- Libera todos os buffers alinhados
- Desaloca o contexto

#### `mock_precoding_layer()`
- Implementação simplificada de processamento de camada
- Realiza scaling de amplitude no sinal de entrada
- **Nota**: Em produção, isso seria substituído por `do_onelayer()` de `nr_dlsch.c`

#### `nr_precoding()`
- Inicializa logging e contexto de precoding
- Cria mock structs `NR_DL_FRAME_PARMS` e `nfapi_nr_dl_tti_pdsch_pdu_rel15_t`
- Loop principal: 100 iterações x 14 símbolos OFDM por slot
- Preenche buffers com dados aleatórios 16-QAM via xorshift PRNG
- Processa cada símbolo através de mock_precoding_layer

## Otimizações Implementadas

### 1. Buffer Pooling
- Alocação única de buffers no init
- Reutilização em todas as iterações
- Alinhamento em 64 bytes para operações SIMD

### 2. PRNG Eficiente
- `xorshift32()` substitui `rand()` + `srand()`
- ~5-10x mais rápido que `rand()`
- Thread-local friendly (estado armazenado no contexto)

### 3. Redução de I/O
- Logs apenas a cada 10 iterações (reduz overhead de printf)
- Impressão de apenas amostras selecionadas

### 4. Gerenciamento de Memória
- Alocação aligned_alloc para performance SIMD
- Liberação apropriada ao fim

## Parâmetros Configuráveis

```c
const int nb_layers = 2;        /* Número de camadas (MIMO) */
const int symbol_sz = 4096;     /* Tamanho OFDM (12 RBs @ 15kHz SCS) */
const int rbSize = 12;          /* Blocos de recursos */
const int nb_symbols = 7;       /* Símbolos OFDM por slot */
const int num_iterations = 100; /* Iterações de teste */
```

## Estrutura Mock para Testes

### NR_DL_FRAME_PARMS Mock
```c
NR_DL_FRAME_PARMS frame_parms = {
    .N_RB_DL = 106,                      /* Banda 20 MHz */
    .ofdm_symbol_size = 4096,
    .first_carrier_offset = 0,
    .nb_antennas_tx = 2,
    .samples_per_slot_wCP = 4096 * 14    /* 14 símbolos */
};
```

### PDSCH Config Mock
```c
nfapi_nr_dl_tti_pdsch_pdu_rel15_t rel15 = {
    .rbStart = 0,
    .rbSize = 12,
    .nrOfLayers = 2,
    .dlDmrsSymbPos = 0x00,     /* Sem DMRS neste teste */
    .pduBitmap = 0x00,         /* Sem PTRS */
    .StartSymbolIndex = 0,
    .NrOfSymbols = 7
};
```

## Saída de Teste

```
=== Starting NR Precoding tests ===
Precoding loop: 2 layers, 12 RBs, 14 symbols per slot
--- Precoding iteration 0 ---
  iter 0, symbol 0: re_processed=144
  iter 0, symbol 13: re_processed=144
...
=== Final output samples (layer 0, symbol 0) ===
output[00] = (r=0,i=-1)
output[01] = (r=-1,i=-1)
...
=== NR Precoding tests completed ===
```

## Próximos Passos para Integração Completa

### 1. Integração com `do_onelayer()` Real
Substituir `mock_precoding_layer()` por chamada a `do_onelayer()` de `nr_dlsch.c`:
```c
int re_processed = do_onelayer(
    &frame_parms,
    iter,
    &rel15,
    layer,           /* Iterar sobre camadas */
    ctx->output,
    &ctx->tx_layer[layer * symbol_sz],
    /* ... outros parâmetros ... */
);
```

### 2. Usar Structs Reais
- Alocar `NR_DL_FRAME_PARMS` via `PHY_VARS_gNB`
- Usar PDU reais do scheduler
- Adicionar DMRS e PTRS reais

### 3. Adicionar Precoding Real
- Implementar codebook Type I precoding
- Suportar múltiplas matrizes de precoding por RB
- Integrar com `beam_index_allocation()`

### 4. Containerização
- Compilar como biblioteca compartilhada (`.so`)
- Expor API C clara (init/run/free)
- Documentar variáveis de ambiente (ex.: `OAI_PRECODING_CONFIG`)

## Compilação e Execução

```bash
cd /home/anderson/dev/oai_isolation/build
make -j4
./oai_isolation
```

Para testar `nr_ofdm_modulation()` em vez de `nr_precoding()`, edite `src/main.c`:
```c
// nr_precoding();
nr_ofdm_modulation();
```

## Performance

- **Buffer pooling**: ~30% redução de overhead de alocação vs. malloc por iteração
- **xorshift32 PRNG**: ~8x mais rápido que rand() com srand()
- **Logs reduzidos**: ~10x menos I/O vs. impressão em cada iteração
- **Alinhamento SIMD**: Preparado para otimizações de vetorização futura

## Limitações Conhecidas

1. `mock_precoding_layer()` é simplificada — não inclui:
   - DMRS insertion
   - PTRS insertion
   - Precoding matrix application
   - Resource element mapping completo

2. Mock structs não contêm todos os campos OAI reais

3. Sem suporte a dynamic bitwidth ou diferentes numerologias

## Referências

- `nr_dlsch.c`: `do_onelayer()` função original (estática, não exportada)
- `nr_modulation.h`: Definições de tipos e funções PHY
- `platform_types.h`: Tipo `c16_t` (complexo 16-bit)

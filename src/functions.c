#include "functions.h"
#include "modulation_tables.h"
#include "PHY/NR_UE_TRANSPORT/nr_transport_ue.h"
#include "PHY/NR_UE_ESTIMATION/nr_estimation.h"
#include "PHY/INIT/nr_phy_init.h"
#include <math.h>
#include "PHY/defs_nr_UE.h"
#include "common/platform_types.h"
#include "PHY/TOOLS/tools_defs.h"
#include "PHY/CODING/nrLDPC_decoder/nrLDPC_types.h"
#include "PHY/CODING/nrLDPC_extern.h"
#include <dlfcn.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int32_t write_file_matlab(const char *fname,
                          const char *vname,
                          const void *data,
                          int length,
                          int dec,
                          unsigned int format,
                          int multiVec)
{
    (void)fname;
    (void)vname;
    (void)data;
    (void)length;
    (void)dec;
    (void)format;
    (void)multiVec;
    return 0;
}

static int size_tb           = 40976; /* bits */
static int segments          = 5;     /* LDPC segments (1 to 8) */
static int lift_size         = 384;   /* Lifting size */
static int coded_tb_size     = 82368;
static int g_modulation_order = 6;   /* 2=QPSK,4=16QAM,6=64QAM,8=256QAM */
static int coded_symbols     = 13728;
static int n_layers          = 2;
static int n_tx              = 2;
static int n_rx              = 2;
static int n_taps            = 11;
static int snr_db_input      = 10;

static int get_central_modulation_order(void)
{
    switch (g_modulation_order) {
        case 2:
        case 4:
        case 6:
        case 8:
            return g_modulation_order;
        default:
            return 2;
    }
}

/* Gray-coded amplitude levels for 16-QAM */
static const int32_t qam16_levels[4] = { -3, -1, +1, +3 };

/* Forward declaration for do_onelayer from nr_dlsch.c */
/* Note: do_onelayer is a static function in nr_dlsch.c, so we can't link to it directly.
 * Instead, we create a simplified mock precoding function that demonstrates the structure
 * and could call do_onelayer if it were exposed. */

/* Mock implementation of precoding processing (simplified version of do_onelayer logic) */
static int mock_precoding_layer(c16_t *output, c16_t *input, int sz, int amp)
{
    /* Simple amplitude scaling of input layer signal */
    for (int i = 0; i < sz; i++) {
        output[i].r = c16mulRealShift(input[i], amp, 15).r;
        output[i].i = c16mulRealShift(input[i], amp, 15).i;
    }
    return sz;
}

/* Lightweight OFDM context to reuse buffers and hold dynamic lib handle */
typedef struct ofdm_ctx_s {
    int fftsize;
    int nb_symbols;
    int nb_prefix_samples;
    int nb_tx;        /* number of TX antennas */
    c16_t **input;    /* frequency-domain input per antenna [nb_tx][nb_symbols * fftsize] */
    c16_t **output;   /* time-domain output per antenna [nb_tx][nb_symbols * (prefix+fftsize)] */
    void *dlh;        /* handle from dlopen for libdfts.so */
    uint32_t *rnd_state; /* xorshift PRNG state per antenna [nb_tx] */
} ofdm_ctx_t;

/* xorshift32 PRNG: faster and thread-local friendly than rand() */
static inline uint32_t xorshift32(uint32_t *state)
{
    uint32_t x = *state;
    if (x == 0) x = 0xDEADBEEF;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

/* Box-Muller normal RNG using the xorshift PRNG */
static inline double gaussian_noise(uint32_t *state)
{
    /* Avoid log(0) by keeping u1 in (0,1] */
    double u1 = ((double)(xorshift32(state) & 0xFFFFFF)) / 16777216.0;
    if (u1 == 0.0) u1 = 1e-7;
    double u2 = ((double)(xorshift32(state) & 0xFFFFFF)) / 16777216.0;
    const double two_pi = 6.28318530717958647692;
    return sqrt(-2.0 * log(u1)) * cos(two_pi * u2);
}

/* Helper: read integer from environment with default */
static int getenv_int(const char *name, int defval)
{
    const char *s = getenv(name);
    if (!s || !*s) return defval;
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s) return defval;
    if (v < INT32_MIN) v = INT32_MIN;
    if (v > INT32_MAX) v = INT32_MAX;
    return (int)v;
}

/* Initialize OFDM context: load libdfts.so directly and allocate aligned buffers */
static ofdm_ctx_t *ofdm_init(int fftsize, int nb_symbols, int nb_prefix_samples, int nb_tx)
{
    ofdm_ctx_t *c = calloc(1, sizeof(*c));
    if (!c) return NULL;

    c->fftsize = fftsize;
    c->nb_symbols = nb_symbols;
    c->nb_prefix_samples = nb_prefix_samples;
    c->nb_tx = nb_tx;

    size_t in_sz = sizeof(c16_t) * (size_t)nb_symbols * (size_t)fftsize;
    size_t out_sz = sizeof(c16_t) * (size_t)nb_symbols * (size_t)(nb_prefix_samples + fftsize);

    /* Allocate per-antenna buffers */
    c->input = calloc(nb_tx, sizeof(c16_t*));
    c->output = calloc(nb_tx, sizeof(c16_t*));
    c->rnd_state = calloc(nb_tx, sizeof(uint32_t));

    if (!c->input || !c->output || !c->rnd_state) {
        free(c->input);
        free(c->output);
        free(c->rnd_state);
        free(c);
        return NULL;
    }

    for (int aa = 0; aa < nb_tx; aa++) {
        c->input[aa] = aligned_alloc(32, in_sz);
        c->output[aa] = aligned_alloc(32, out_sz);
        if (!c->input[aa] || !c->output[aa]) {
            for (int i = 0; i <= aa; i++) {
                free(c->input[i]);
                free(c->output[i]);
            }
            free(c->input);
            free(c->output);
            free(c->rnd_state);
            free(c);
            return NULL;
        }
        memset(c->input[aa], 0, in_sz);
        memset(c->output[aa], 0, out_sz);
        /* Independent PRNG seed per antenna */
        c->rnd_state[aa] = (uint32_t)time(NULL) ^ (uint32_t)aa;
    }

    const char *dfts_path = getenv("OAI_DFTS_LIB");
    if (!dfts_path || !*dfts_path) {
        dfts_path = "/home/anderson/dev/oai_isolation_Old/ext/openair/cmake_targets/ran_build/build/libdfts.so";
    }

    c->dlh = dlopen(dfts_path, RTLD_NOW | RTLD_GLOBAL);
    if (!c->dlh) {
        printf("ofdm_init: dlopen(%s) failed: %s\n", dfts_path, dlerror());
        for (int aa = 0; aa < nb_tx; aa++) {
            free(c->input[aa]);
            free(c->output[aa]);
        }
        free(c->input);
        free(c->output);
        free(c->rnd_state);
        free(c);
        return NULL;
    }

    idft = (idftfunc_t)dlsym(c->dlh, "idft_implementation");
    if (!idft) {
        printf("ofdm_init: dlsym(idft_implementation) failed: %s\n", dlerror());
        dlclose(c->dlh);
        for (int aa = 0; aa < nb_tx; aa++) {
            free(c->input[aa]);
            free(c->output[aa]);
        }
        free(c->input);
        free(c->output);
        free(c->rnd_state);
        free(c);
        return NULL;
    }

    return c;
}

static void ofdm_free(ofdm_ctx_t *c)
{
    if (!c) return;

    if (c->dlh) dlclose(c->dlh);

    /* Free per-antenna buffers */
    if (c->input) {
        for (int aa = 0; aa < c->nb_tx; aa++) {
            free(c->input[aa]);
        }
        free(c->input);
    }
    if (c->output) {
        for (int aa = 0; aa < c->nb_tx; aa++) {
            free(c->output[aa]);
        }
        free(c->output);
    }
    free(c->rnd_state);
    free(c);
}

/* Lightweight precoding context to hold buffers and frame params for do_onelayer */
typedef struct precoding_ctx_s {
    int nb_layers;
    int symbol_sz;         /* OFDM symbol size */
    int rbSize;            /* Resource block size */
    int fftsize;
    c16_t *tx_layer;       /* input layer signal (per layer) */
    c16_t *output;         /* output after precoding (per symbol) */
    NR_DL_FRAME_PARMS *frame_parms;
    nfapi_nr_dl_tti_pdsch_pdu_rel15_t *rel15;
    uint32_t rnd_state;    /* xorshift PRNG state */
} precoding_ctx_t;

/* Initialize precoding context with aligned buffers */
static precoding_ctx_t *precoding_init(int nb_layers, int symbol_sz, int rbSize)
{
    precoding_ctx_t *c = calloc(1, sizeof(*c));
    if (!c) return NULL;

    c->nb_layers = nb_layers;
    c->symbol_sz = symbol_sz;
    c->rbSize = rbSize;
    c->fftsize = symbol_sz;

    /* Allocate buffers: per-layer input and per-symbol output */
    size_t layer_sz = sizeof(c16_t) * (size_t)symbol_sz * (size_t)nb_layers;
    size_t output_sz = sizeof(c16_t) * (size_t)symbol_sz;

    c->tx_layer = aligned_alloc(64, layer_sz);
    c->output = aligned_alloc(64, output_sz);

    if (!c->tx_layer || !c->output) {
        free(c->tx_layer);
        free(c->output);
        free(c);
        return NULL;
    }

    memset(c->tx_layer, 0, layer_sz);
    memset(c->output, 0, output_sz);

    /* PRNG seed per-context */
    c->rnd_state = (uint32_t)time(NULL) ^ (uint32_t)(uintptr_t)c;

    return c;
}

static void precoding_free(precoding_ctx_t *c)
{
    if (!c) return;
    free(c->tx_layer);
    free(c->output);
    free(c);
}

void nr_precoding()
{
    /* Initialize the logging system first */
    logInit();

    printf("=== Starting NR Precoding tests ===\n");

    /* Precoding parameters: configurable via environment variables */
    const int nb_layers      = n_layers;
    const int nb_rb          = getenv_int("OAI_RB", 52);
    const int mod_order      = get_central_modulation_order();
    const int symbol_sz      = nb_rb * 12;
    const int nb_symbols     = 14;
    const int num_iterations = getenv_int("OAI_ITERS", 5000000);

    /* Initialize precoding context and allocate buffers */
    precoding_ctx_t *ctx = precoding_init(nb_layers, symbol_sz, nb_rb);
    if (!ctx) {
        printf("precoding_init failed\n");
        return;
    }

    /* Mock NR_DL_FRAME_PARMS for minimal do_onelayer support */
    NR_DL_FRAME_PARMS frame_parms = {
        .N_RB_DL = nb_rb,
        .ofdm_symbol_size = symbol_sz,
        .first_carrier_offset = 0,
        .nb_antennas_tx = n_tx,
        .samples_per_slot_wCP = symbol_sz * 14
    };
    ctx->frame_parms = &frame_parms;

    /* Mock nfapi_nr_dl_tti_pdsch_pdu_rel15_t for minimal do_onelayer support */
    nfapi_nr_dl_tti_pdsch_pdu_rel15_t rel15 = {
        .rbStart = 0,
        .rbSize = nb_rb,
        .BWPStart = 0,
        .qamModOrder = { (uint8_t)mod_order, (uint8_t)mod_order },
        .nrOfLayers = nb_layers,
        .dlDmrsSymbPos = 0x00,
        .pduBitmap = 0x00,
        .NrOfCodewords = 1,
        .StartSymbolIndex = 0,
        .NrOfSymbols = nb_symbols,
        .dlDmrsScramblingId = 0,
        .SCID = 0
    };
    ctx->rel15 = &rel15;

    printf("Precoding loop: layers=%d, RBs=%d, mod_order=%d, symbol_sz=%d, symbols/slot=%d\n",
           nb_layers, nb_rb, mod_order, symbol_sz, nb_symbols);

    /* Main precoding loop: iterate and call do_onelayer for each symbol */
    for (int iter = 0; iter < num_iterations; iter++) {
        if ((iter % 100) == 0) printf("--- Precoding iteration %d ---\n", iter);

        /* Fill tx_layer with random constellation samples according to mod_order */
        for (int layer = 0; layer < nb_layers; layer++) {
            for (int k = 0; k < symbol_sz; k++) {
                uint32_t r = xorshift32(&ctx->rnd_state);
                int idx = layer * symbol_sz + k;

                switch (mod_order) {
                    case 2: {
                        uint8_t sidx = (uint8_t)(r & 0x3);
                        ctx->tx_layer[idx].r = qpsk_table[sidx][0];
                        ctx->tx_layer[idx].i = qpsk_table[sidx][1];
                        break;
                    }
                    case 6: {
                        uint8_t sidx = (uint8_t)(r & 0x3F);
                        ctx->tx_layer[idx].r = qam64_table[sidx][0];
                        ctx->tx_layer[idx].i = qam64_table[sidx][1];
                        break;
                    }
                    case 8: {
                        uint8_t sidx = (uint8_t)(r & 0xFF);
                        ctx->tx_layer[idx].r = qam256_table[sidx][0];
                        ctx->tx_layer[idx].i = qam256_table[sidx][1];
                        break;
                    }
                    case 4:
                    default: {
                        uint8_t idx_re = (uint8_t)(r & 3);
                        uint8_t idx_im = (uint8_t)((r >> 2) & 3);
                        ctx->tx_layer[idx].r = (int16_t)qam16_levels[idx_re];
                        ctx->tx_layer[idx].i = (int16_t)qam16_levels[idx_im];
                        break;
                    }
                }
            }
        }

        /* Process each OFDM symbol in the slot */
        for (int l_symbol = 0; l_symbol < nb_symbols; l_symbol++) {
            for (int layer = 0; layer < nb_layers; layer++) {
                int re_processed = do_onelayer(
                    ctx->frame_parms,
                    0,
                    ctx->rel15,
                    layer,
                    ctx->output,
                    &ctx->tx_layer[layer * symbol_sz],
                    0,
                    symbol_sz,
                    l_symbol,
                    0,
                    0,
                    1000,
                    15000,
                    0,
                    NFAPI_NR_DMRS_TYPE1,
                    NULL
                );

                if ((iter % 100) == 0 && l_symbol == 0) {
                    printf("  iter %d, symbol %d, layer %d: re_processed=%d\n",
                           iter, l_symbol, layer, re_processed);
                }
            }
        }
    }

    printf("\n=== Final output samples (layer 0, symbol 0) ===\n");
    for (int i = 0; i < 16 && i < symbol_sz; i++) {
        printf("output[%02d] = (r=%d,i=%d)\n", i, ctx->output[i].r, ctx->output[i].i);
    }

    precoding_free(ctx);
    printf("=== NR Precoding tests completed ===\n");
}

void nr_scramble(){
    /* Initialize the logging system first */
    logInit();

    /* Requested parameters */
    const uint32_t size   = (uint32_t)coded_tb_size;  /* bits */
    const uint8_t  q      = 0;
    const uint32_t Nid    = 0;
    const uint32_t n_RNTI = 0xFFFF;    /* 65535 */

    /* Derived buffer sizes based on requested bit size */
    const uint32_t in_bytes  = (size + 7) / 8;
    const uint32_t out_words = (size + 31) / 32;

    uint8_t  *in  = malloc(in_bytes);
    uint32_t *out = malloc(out_words * sizeof(uint32_t));
    if (!in || !out) {
        printf("nr_scramble: buffer allocation failed (in=%p, out=%p)\n", (void*)in, (void*)out);
        free(in);
        free(out);
        return;
    }

    memset(in, 0, in_bytes);
    memset(out, 0, out_words * sizeof(uint32_t));

    /* Optional: seed the input with a small sample pattern */
    const uint8_t sample[] = {
        0x69, 0xBE, 0xF6, 0x02, 0xAD, 0xFB, 0x50, 0xE6,
        0xFF, 0x35, 0xA8, 0x44, 0xF9, 0x21, 0x92, 0xAA,
        0x68, 0x28, 0x2A
    };
    memcpy(in, sample, sizeof(sample) < in_bytes ? sizeof(sample) : in_bytes);

    const int iterations = getenv_int("OAI_ITERS", 1000000000);
    const int verbose    = getenv("OAI_VERBOSE") != NULL;

    printf("Starting scrambling tests (%d iterations)...\n", iterations);

    for (int iter = 0; iter < iterations; ++iter) {
        in[0] = (uint8_t)iter;
        nr_codeword_scrambling(in, size, q, Nid, n_RNTI, out);
        if (verbose && (iter % 100) == 0) {
            printf("iter %d: out[0]=0x%08X\n", iter, (unsigned)out[0]);
        }
    }

    int roundedSz   = (size + 31) / 32;
    int print_cap   = verbose ? 64 : 8;
    int print_limit = roundedSz < print_cap ? roundedSz : print_cap;
    printf("Final output (showing first %d of %d words):\n", print_limit, roundedSz);
    for (int i = 0; i < print_limit; ++i) {
        printf("out[%02d] = 0x%08X\n", i, out[i]);
    }

    free(in);
    free(out);
}

void nr_crc(){
    const int    n_bits  = size_tb;
    const size_t n_bytes = (size_t)n_bits / 8;
    unsigned char *crc_buf = malloc(n_bytes);

    printf("Start\n");
    if (!crc_buf) {
        printf("nr_crc: data allocation failed\n");
        return;
    }
    srand(time(NULL));
    for (long long i = 0; i < 5000000; ++i) {
        printf("\rIteration: %lld", i);
        for (size_t j = 0; j < n_bytes; ++j) {
            crc_buf[j] = rand() & 0xFF;
        }
        volatile unsigned int crc = crc24a(crc_buf, n_bytes);
        (void)crc;
    }
    printf("\nEnd\n");
    free(crc_buf);
}

/* Fill input buffer with random 16-QAM symbols in OAI split-complex format */
void fill_random_16qam(int32_t *input, int fftsize)
{
    for (int k = 0; k < fftsize; k++) {
        uint8_t idx_re = rand() & 3;
        uint8_t idx_im = rand() & 3;
        input[k]           = qam16_levels[idx_re];
        input[k + fftsize] = qam16_levels[idx_im];
    }
}

/* Main OFDM test function – complete version */
void nr_ofdm_modulation()
{
    logInit();

    const int fftsize           = 1024;
    const int nb_symbols        = 14;
    const int num_iterations    = getenv_int("OAI_ITERS", 10000000);
    const int nb_tx             = n_tx;
    const int nb_prefix_samples = (fftsize == 2048)
                                  ? 176
                                  : (fftsize == 1024 ? 88 : (176 * fftsize / 2048));
    const Extension_t extype    = CYCLIC_PREFIX;

    ofdm_ctx_t *ctx = ofdm_init(fftsize, nb_symbols, nb_prefix_samples, nb_tx);
    if (!ctx) {
        printf("ofdm_init failed\n");
        return;
    }

    printf("=== Starting OFDM modulation tests (16-QAM random input) ===\n");
    printf("Parameters: fftsize=%d, nb_symbols=%d, nb_prefix_samples=%d, nb_tx=%d, iterations=%d\n",
           fftsize, nb_symbols, nb_prefix_samples, nb_tx, num_iterations);

    for (int iter = 0; iter < num_iterations; iter++) {
        if ((iter % 10) == 0) printf("\n--- OFDM modulation iteration %d ---\n", iter);

        for (int aa = 0; aa < nb_tx; aa++) {
            for (int l_symbol = 0; l_symbol < nb_symbols; l_symbol++) {
                c16_t *symbol_input = &ctx->input[aa][l_symbol * fftsize];
                for (int k = 0; k < fftsize; k++) {
                    uint32_t r = xorshift32(&ctx->rnd_state[aa]);
                    uint8_t idx_re = r & 3;
                    uint8_t idx_im = (r >> 2) & 3;
                    symbol_input[k].r = (int16_t)qam16_levels[idx_re];
                    symbol_input[k].i = (int16_t)qam16_levels[idx_im];
                }
            }
            ctx->input[aa][100].r = (int16_t)(iter + aa * 1000);
            if ((iter % 10) == 0) printf("Input sample [ant %d][100]: %d\n", aa, ctx->input[aa][100].r);

            PHY_ofdm_mod((int *)ctx->input[aa],
                         (int *)ctx->output[aa],
                         fftsize,
                         nb_symbols,
                         nb_prefix_samples,
                         extype);

            if ((iter % 10) == 0) {
                printf("iter %d ant %d: output[0]=(r=%d,i=%d), output[1]=(r=%d,i=%d)\n",
                       iter, aa,
                       ctx->output[aa][0].r, ctx->output[aa][0].i,
                       ctx->output[aa][1].r, ctx->output[aa][1].i);
            }
        }
    }

    printf("\n=== Final OFDM time-domain samples ===\n");
    for (int aa = 0; aa < nb_tx; aa++) {
        printf("Antenna %d:\n", aa);
        for (int i = 0; i < 16; i++) {
            printf("  output[%02d] = (r=%d,i=%d)\n", i, ctx->output[aa][i].r, ctx->output[aa][i].i);
        }
    }

    ofdm_free(ctx);
}

void nr_modulation_test()
{
    logInit();

    const uint32_t modulation_order = (uint32_t)get_central_modulation_order();
    const uint32_t length           = (uint32_t)coded_tb_size;
    const int      num_iterations   = 10000000;

    const int16_t *mod_table;
    const char    *mod_name;
    uint32_t       table_size;

    switch (modulation_order) {
        case 2:
            mod_table  = (int16_t *)qpsk_table;
            mod_name   = "QPSK";
            table_size = 4;
            break;
        case 4:
            mod_table  = (int16_t *)qam16_table;
            mod_name   = "16-QAM";
            table_size = 16;
            break;
        case 6:
            mod_table  = (int16_t *)qam64_table;
            mod_name   = "64-QAM";
            table_size = 64;
            break;
        case 8:
            mod_table  = (int16_t *)qam256_table;
            mod_name   = "256-QAM";
            table_size = 256;
            break;
        default:
            printf("Error: Invalid modulation order %u (must be 2, 4, 6, or 8)\n", modulation_order);
            return;
    }

    uint32_t num_symbols = length / modulation_order;

    printf("=== Starting NR Modulation test: %s ===\n", mod_name);
    printf("Parameters: mod_order=%u, length=%u bits, num_symbols=%u\n",
           modulation_order, length, num_symbols);
    printf("Running %d iterations...\n", num_iterations);

    const size_t out_sz = num_symbols * sizeof(int16_t) * 2;
    const size_t in_sz  = ((length + 31) / 32) * sizeof(uint32_t);

    uint32_t *in  = aligned_alloc(32, in_sz);
    int16_t  *out = aligned_alloc(32, out_sz);

    if (!in || !out) {
        printf("nr_modulation_test: buffer allocation failed\n");
        free(in);
        free(out);
        return;
    }
    memset(out, 0, out_sz);

    for (int iter = 0; iter < num_iterations; iter++) {
        uint32_t pattern = (iter % 2) ? 0x55555555 : 0xAAAAAAAA;
        size_t n_words = in_sz / sizeof(uint32_t);
        for (uint32_t i = 0; i < n_words; i++) {
            in[i] = pattern ^ (i * 0x11111111);
        }

        int16_t  *out_ptr  = out;
        uint8_t  *in_bytes = (uint8_t *)in;
        uint32_t  bit_mask = (1 << modulation_order) - 1;

        for (uint32_t i = 0; i < num_symbols; i++) {
            uint32_t byte_idx  = (i * modulation_order) / 8;
            uint32_t bit_offset = (i * modulation_order) & 0x7;
            uint32_t symbol_idx = (in_bytes[byte_idx] >> bit_offset) & bit_mask;
            symbol_idx = symbol_idx % table_size;
            *out_ptr++ = mod_table[symbol_idx * 2 + 0];
            *out_ptr++ = mod_table[symbol_idx * 2 + 1];
        }

        if ((iter % 100) == 0) {
            printf("  iter %6d: out[0..7]=[%6d, %6d, %6d, %6d, %6d, %6d, %6d, %6d]\n",
                   iter,
                   out[0], out[1], out[2], out[3],
                   out[4], out[5], out[6], out[7]);
        }
    }

    printf("\n%s constellation points (first 16 of %u):\n", mod_name, table_size);
    int print_limit = table_size < 16 ? table_size : 16;
    for (int idx = 0; idx < print_limit; idx++) {
        printf("  Symbol[%2u]: I=%7d, Q=%7d\n",
               idx,
               mod_table[idx * 2 + 0],
               mod_table[idx * 2 + 1]);
    }
    if (table_size > 16) {
        printf("  ... (%u more points)\n", table_size - 16);
    }

    free(in);
    free(out);
    printf("\n=== NR Modulation test completed (%s) ===\n", mod_name);
}

void nr_layermapping()
{
    logInit();

    printf("=== Starting NR Layer Mapping tests ===\n");

    const uint32_t n_symbs      = (uint32_t)coded_symbols;
    const int      nbCodes      = 1;
    const uint8_t  n_layers_lm = (uint8_t)n_layers;
    const int      num_iterations = 10000000;

    const int encoded_len = (int)n_symbs;
    const int layerSz     = (int)n_symbs / (int)n_layers_lm;

    printf("Parameters: n_symbs=%u, n_layers=%u, encoded_len=%d, layerSz=%d\n",
           n_symbs, n_layers_lm, encoded_len, layerSz);
    printf("Running %d iterations...\n", num_iterations);

    c16_t (*mod_symbs)[nbCodes][encoded_len] = aligned_alloc(64, sizeof(c16_t) * nbCodes * encoded_len);
    c16_t (*tx_layers)[n_layers_lm][layerSz] = aligned_alloc(64, sizeof(c16_t) * n_layers_lm * layerSz);

    if (!mod_symbs || !tx_layers) {
        printf("nr_layermapping: buffer allocation failed\n");
        free(mod_symbs);
        free(tx_layers);
        return;
    }

    memset(mod_symbs, 0, sizeof(c16_t) * nbCodes * encoded_len);
    memset(tx_layers, 0, sizeof(c16_t) * n_layers_lm * layerSz);

    for (int iter = 0; iter < num_iterations; iter++) {
        uint32_t rnd_state = (uint32_t)time(NULL) ^ (uint32_t)(uintptr_t)mod_symbs ^ iter;

        for (int k = 0; k < encoded_len; k++) {
            uint32_t r     = xorshift32(&rnd_state);
            uint8_t idx_re = r & 3;
            uint8_t idx_im = (r >> 2) & 3;
            (*mod_symbs)[0][k].r = (int16_t)qam16_levels[idx_re];
            (*mod_symbs)[0][k].i = (int16_t)qam16_levels[idx_im];
        }

        nr_layer_mapping(
            nbCodes,
            encoded_len,
            (c16_t (*)[encoded_len])(*mod_symbs),
            n_layers_lm,
            layerSz,
            n_symbs,
            (c16_t (*)[layerSz])(*tx_layers)
        );

        if ((iter % 100) == 0) {
            printf("  iter %6d: tx_layer[0][0..3]=[(%6d,%6d), (%6d,%6d), (%6d,%6d), (%6d,%6d)]\n",
                   iter,
                   (*tx_layers)[0][0].r, (*tx_layers)[0][0].i,
                   (*tx_layers)[0][1].r, (*tx_layers)[0][1].i,
                   (*tx_layers)[0][2].r, (*tx_layers)[0][2].i,
                   (*tx_layers)[0][3].r, (*tx_layers)[0][3].i);
        }
    }

    printf("\n=== Final layer mapping output (first 8 symbols per layer) ===\n");
    for (uint8_t layer = 0; layer < n_layers_lm; layer++) {
        printf("Layer %u:\n", layer);
        for (int i = 0; i < 8; i++) {
            printf("  symbol[%d] = (I=%6d, Q=%6d)\n",
                   i,
                   (*tx_layers)[layer][i].r,
                   (*tx_layers)[layer][i].i);
        }
    }

    free(mod_symbs);
    free(tx_layers);
    printf("=== NR Layer Mapping tests completed ===\n");
}

void nr_ldpc()
{
    /* Initialize the logging system first */
    logInit();

    printf("=== Starting NR LDPC Encoder tests ===\n");

    const int BG  = 1;
    const int Zc  = lift_size;
    const int Kb  = 22;
    const int K   = Kb * Zc;
    const int num_runs   = getenv_int("OAI_ITERS", 10000000);
    const int n_segments_ldpc = 1;

    int nrows = (BG == 1) ? 46 : 42;
    int rate  = (BG == 1) ? 3  : 5;
    int no_punctured_columns = ((nrows - 2) * Zc + K - K * rate) / Zc;
    int removed_bit = (nrows - no_punctured_columns - 2) * Zc + K - (int)(K * rate);
    int output_length = K / 8 + ((nrows - no_punctured_columns) * Zc - removed_bit) / 8;
    int output_buffer_size = output_length + 4096;

    printf("LDPC parameters: BG=%d, Zc=%d, Kb=%d, K=%d bits\n", BG, Zc, Kb, K);
    printf("Output size: %d bytes (buffer: %d bytes)\n", output_length, output_buffer_size);
    printf("Running %d test runs...\n", num_runs);

    uint8_t *input_seg = malloc((K + 7) / 8);
    if (!input_seg) {
        printf("nr_ldpc: input buffer allocation failed\n");
        return;
    }
    memset(input_seg, 0, (K + 7) / 8);

    uint8_t *input[1];
    input[0] = input_seg;

    uint8_t *output = malloc(output_buffer_size);
    if (!output) {
        printf("nr_ldpc: output buffer allocation failed\n");
        free(input_seg);
        return;
    }
    memset(output, 0, output_buffer_size);

    encoder_implemparams_t encoder_params = {
        .BG        = BG,
        .Zc        = Zc,
        .Kb        = Kb,
        .K         = K,
        .n_segments = n_segments_ldpc,
        .first_seg = 0,
        .gen_code  = 0,
        .tinput    = NULL,
        .tprep     = NULL,
        .tparity   = NULL
    };

    printf("Starting LDPC encoding test loop...\n");

    for (int iter = 0; iter < num_runs; iter++) {
        uint32_t pattern = (iter % 2) ? 0x55 : 0xAA;
        for (int byte_idx = 0; byte_idx < (K + 7) / 8; byte_idx++) {
            input_seg[byte_idx] = (uint8_t)(pattern ^ (byte_idx ^ iter));
        }

        int result = LDPCencoder(input, output, &encoder_params);

        if ((iter % 5) == 0) {
            printf("  iter %2d: encoder returned %d, output[0..3]=0x%02X 0x%02X 0x%02X 0x%02X\n",
                   iter, result,
                   output[0], output[1], output[2], output[3]);
        }

        if (result != 0) {
            printf("ERROR: LDPCencoder failed at run %d\n", iter);
        }
    }

    printf("\n=== Final LDPC encoded output (first 16 bytes) ===\n");
    for (int i = 0; i < 16 && i < output_length; i++) {
        printf("output[%02d] = 0x%02X\n", i, output[i]);
    }

    free(input_seg);
    free(output);
    printf("=== NR LDPC Encoder tests completed ===\n");
}

void nr_ch_estimation()
{
    logInit();

    printf("=== Starting NR Channel Estimation (PDSCH) tests ===\n");

    const int nb_antennas_rx  = n_rx;
    const int nb_antennas_tx  = n_tx;
    const int nb_rb_pdsch     = 52;
    const int ofdm_symbol_size = 1024;
    const int first_carrier_offset = ofdm_symbol_size - ((nb_rb_pdsch * 12) / 2);
    const int symbols_per_slot = 14;
    const int num_iterations  = getenv_int("OAI_ITERS", 100000);
    const int verbose         = getenv("OAI_VERBOSE") != NULL;
    const int snr_db          = snr_db_input;

    double snr_linear = pow(10.0, snr_db / 10.0);
    double noise_std  = sqrt(1.0 / snr_linear);

    printf("Channel estimation parameters: RX ant=%d, RB=%d, FFT=%d, symbols=%d, SNR=%d dB (noise_std=%.4f)\n",
           nb_antennas_rx, nb_rb_pdsch, ofdm_symbol_size, symbols_per_slot, snr_db, noise_std);

    struct mini_fp { int ofdm_symbol_size; int N_RB_DL; int nb_antennas_rx; } mini_fp = {
        .ofdm_symbol_size = ofdm_symbol_size,
        .N_RB_DL          = nb_rb_pdsch,
        .nb_antennas_rx   = nb_antennas_rx
    };

    const int rx_len = nb_antennas_rx * ofdm_symbol_size;
    int32_t *rxdataF_data = aligned_alloc(32, rx_len * sizeof(int32_t));
    if (!rxdataF_data) { printf("nr_ch_estimation: rxdataF allocation failed\n"); return; }
    memset(rxdataF_data, 0, rx_len * sizeof(int32_t));

    int32_t *dl_ch_data = aligned_alloc(32, rx_len * sizeof(int32_t));
    if (!dl_ch_data) {
        printf("nr_ch_estimation: dl_ch allocation failed\n");
        free(rxdataF_data);
        return;
    }
    memset(dl_ch_data, 0, rx_len * sizeof(int32_t));

    uint32_t *nvar = aligned_alloc(32, nb_antennas_rx * sizeof(uint32_t));
    if (!nvar) {
        printf("nr_ch_estimation: nvar allocation failed\n");
        free(rxdataF_data);
        free(dl_ch_data);
        return;
    }
    for (int i = 0; i < nb_antennas_rx; i++) nvar[i] = 255;

    printf("Running %d iterations of PDSCH channel estimation...\n", num_iterations);

    int use_real = 0;
    const char *use_real_env = getenv("OAI_USE_REAL_EST");
    if (use_real_env && (*use_real_env == '1')) {
        printf("Using REAL OAI channel estimation path (nr_pdsch_channel_estimation).\n");
        use_real = 1;
    } else {
        printf("Using simplified LS channel estimation path. Set OAI_USE_REAL_EST=1 to enable real path.\n");
    }

    const int dl_ch_words = rx_len;
    uint32_t noise_seed = 0x12345678u;

    for (int iter = 0; iter < num_iterations; iter++) {
        uint32_t seed = 0xA5A5A5A5u + iter;
        for (int n = 0; n < rx_len; n++) {
            seed = seed * 1103515245 + 12345;
            int16_t re = (int16_t)((seed >> 16) & 0xFFFF);
            seed = seed * 1103515245 + 12345;
            int16_t im = (int16_t)((seed >> 16) & 0xFFFF);
            rxdataF_data[n] = ((int32_t)im << 16) | (uint16_t)re;
        }

        for (int n = 0; n < rx_len; n += 2) {
            noise_seed = noise_seed * 1103515245 + 12345;
            double u1 = (double)(noise_seed & 0xFFFFFF) / (1u << 24);
            noise_seed = noise_seed * 1103515245 + 12345;
            double u2 = (double)(noise_seed & 0xFFFFFF) / (1u << 24);
            double z0 = sqrt(-2.0 * log(u1 + 1e-10)) * cos(2.0 * M_PI * u2);
            double z1 = sqrt(-2.0 * log(u1 + 1e-10)) * sin(2.0 * M_PI * u2);

            int32_t curr    = rxdataF_data[n];
            int16_t curr_re = (int16_t)(curr & 0xFFFF);
            int16_t curr_im = (int16_t)((curr >> 16) & 0xFFFF);
            double noisy_re = curr_re + noise_std * z0 * 32767.0;
            double noisy_im = curr_im + noise_std * z1 * 32767.0;
            int16_t clipped_re = (noisy_re > 32767) ? 32767 : ((noisy_re < -32768) ? -32768 : (int16_t)noisy_re);
            int16_t clipped_im = (noisy_im > 32767) ? 32767 : ((noisy_im < -32768) ? -32768 : (int16_t)noisy_im);
            rxdataF_data[n] = ((int32_t)(uint16_t)clipped_im << 16) | (uint16_t)clipped_re;

            if (n + 1 < rx_len) {
                curr    = rxdataF_data[n + 1];
                curr_re = (int16_t)(curr & 0xFFFF);
                curr_im = (int16_t)((curr >> 16) & 0xFFFF);
                noisy_re = curr_re + noise_std * z1 * 32767.0;
                noise_seed = noise_seed * 1103515245 + 12345;
                double u3 = (double)(noise_seed & 0xFFFFFF) / (1u << 24);
                noise_seed = noise_seed * 1103515245 + 12345;
                double u4 = (double)(noise_seed & 0xFFFFFF) / (1u << 24);
                double z2 = sqrt(-2.0 * log(u3 + 1e-10)) * cos(2.0 * M_PI * u4);
                noisy_im = curr_im + noise_std * z2 * 32767.0;
                clipped_re = (noisy_re > 32767) ? 32767 : ((noisy_re < -32768) ? -32768 : (int16_t)noisy_re);
                clipped_im = (noisy_im > 32767) ? 32767 : ((noisy_im < -32768) ? -32768 : (int16_t)noisy_im);
                rxdataF_data[n + 1] = ((int32_t)(uint16_t)clipped_im << 16) | (uint16_t)clipped_re;
            }
        }

        if (!use_real) {
            const int pr = 8192;
            for (int ant = 0; ant < nb_antennas_rx; ant++) {
                int base = ant * ofdm_symbol_size;
                for (int k = 0; k < ofdm_symbol_size; k++) {
                    int32_t y   = rxdataF_data[base + k];
                    int16_t yr  = (int16_t)(y & 0xFFFF);
                    int16_t yi  = (int16_t)((y >> 16) & 0xFFFF);
                    int16_t hr  = (int16_t)(yr / pr);
                    int16_t hi  = (int16_t)(yi / pr);
                    dl_ch_data[base + k] = ((int32_t)(uint16_t)hi << 16) | (uint16_t)hr;
                }
            }
        } else {
            struct { int ofdm_symbol_size; int N_RB_DL; int nb_antennas_rx; } fp_min = {
                .ofdm_symbol_size = ofdm_symbol_size,
                .N_RB_DL          = nb_rb_pdsch,
                .nb_antennas_rx   = nb_antennas_rx
            };
            unsigned int dmrs_sym = 2;
            typedef void (*nr_pdsch_ch_est_min_t)(void*, const void*, unsigned int, uint8_t, uint8_t, void*, void*, uint32_t*);
            ((nr_pdsch_ch_est_min_t)nr_pdsch_channel_estimation)(
                NULL, &fp_min, dmrs_sym, 0, nb_antennas_rx,
                dl_ch_data, rxdataF_data, nvar);

            if (verbose && iter == 0) {
                int wrote_real = 0;
                for (int i = 0; i < dl_ch_words; i++) {
                    if (((uint32_t *)dl_ch_data)[i] != 0) { wrote_real = 1; break; }
                }
                printf("    real-path wrote_real=%d dl_ch_data[0]=0x%08X rxF[0]=0x%08X\n",
                       wrote_real,
                       ((uint32_t *)dl_ch_data)[0],
                       ((uint32_t *)rxdataF_data)[0]);
            }
        }

        int wrote = 0;
        for (int i = 0; i < dl_ch_words; i++) {
            if (((uint32_t *)dl_ch_data)[i] != 0) { wrote = 1; break; }
        }
        if (verbose && (iter % 10) == 0) {
            printf("  iter %6d: wrote=%d dl_ch[0]=0x%08X dl_ch[1]=0x%08X\n",
                   iter, wrote,
                   ((uint32_t *)dl_ch_data)[0],
                   ((uint32_t *)dl_ch_data)[1]);
        }
    }

    printf("\n=== Final channel estimation output (first 8 samples) ===\n");
    for (int i = 0; i < 8; i++) {
        printf("dl_ch[0][%02d] = 0x%08X\n", i, ((uint32_t *)dl_ch_data)[i]);
    }

    free(rxdataF_data);
    free(dl_ch_data);
    free(nvar);
    printf("=== NR Channel Estimation (PDSCH) tests completed ===\n");
}

void nr_descrambling()
{
    logInit();
    init_byte2m128i();

    printf("=== Starting NR DLSCH Descrambling tests ===\n");

    const uint32_t size   = (uint32_t)coded_tb_size;
    const uint8_t  q      = 0;
    const uint32_t Nid    = 0;
    const uint32_t n_RNTI = 0xFFFF;

    const int buffer_size   = ((int)size + 15) & ~15;
    const int num_iterations = getenv_int("OAI_ITERS", 100000000);
    const int verbose        = getenv("OAI_VERBOSE") != NULL;

    printf("Descrambling parameters: size=%u bits, q=%u, Nid=%u, n_RNTI=0x%X\n",
           size, q, Nid, n_RNTI);
    printf("Buffer size: %d LLRs (allocated for %u LLRs needed)\n", buffer_size, size);
    printf("Iterations: %d (set OAI_ITERS), verbose=%d (set OAI_VERBOSE)\n", num_iterations, verbose);

    int16_t *llr = aligned_alloc(32, buffer_size * sizeof(int16_t));
    if (!llr) {
        printf("nr_descrambling: llr allocation failed\n");
        return;
    }
    memset(llr, 0, buffer_size * sizeof(int16_t));

    const int16_t sample_llr[] = {
        127, -120, 115, -110, 105, -100, 95, -90,
        85, -80, 75, -70, 65, -60, 55, -50, 45, -40, 35
    };
    int sample_size = sizeof(sample_llr) / sizeof(sample_llr[0]);
    for (int i = 0; i < sample_size && i < (int)size; i++) {
        llr[i] = sample_llr[i];
    }

    printf("Running %d iterations of DLSCH descrambling...\n", num_iterations);

    for (int iter = 0; iter < num_iterations; iter++) {
        llr[0] = (int16_t)((iter * 7) & 0xFF);
        nr_dlsch_unscrambling(llr, size, q, Nid, n_RNTI);
        if (verbose && (iter % 100) == 0) {
            printf("  iter %4d: llr[0]=%d llr[1]=%d llr[2]=%d\n",
                   iter, llr[0], llr[1], llr[2]);
        }
    }

    printf("\n=== Final descrambled LLR output (first 16 values) ===\n");
    for (int i = 0; i < 16 && i < (int)size; i++) {
        printf("llr[%02d] = %d\n", i, llr[i]);
    }

    free(llr);
    printf("=== NR DLSCH Descrambling tests completed ===\n");
}

void nr_layer_demapping_test()
{
    logInit();

    printf("=== Starting NR Layer Demapping tests ===\n");

    const uint8_t  Nl            = (uint8_t)n_layers;
    const uint8_t  mod_order     = (uint8_t)get_central_modulation_order();
    const uint32_t length        = (uint32_t)coded_symbols;
    const int32_t  codeword_TB0  = 0;
    const int32_t  codeword_TB1  = -1;
    const int      num_iterations = 10000000;
    const uint32_t layer_sz      = length;

    printf("Layer demapping parameters: Nl=%u, mod_order=%u, length=%u\n",
           Nl, mod_order, length);
    printf("Codewords: TB0=%d, TB1=%d\n", codeword_TB0, codeword_TB1);
    printf("Running %d iterations...\n", num_iterations);

    int16_t (*llr_layers)[layer_sz] = aligned_alloc(32, Nl * layer_sz * sizeof(int16_t));
    if (!llr_layers) {
        printf("nr_layer_demapping_test: llr_layers allocation failed\n");
        return;
    }
    memset(llr_layers, 0, Nl * layer_sz * sizeof(int16_t));

    int16_t *llr_cw[2];
    llr_cw[0] = aligned_alloc(32, length * sizeof(int16_t));
    llr_cw[1] = aligned_alloc(32, length * sizeof(int16_t));

    if (!llr_cw[0] || !llr_cw[1]) {
        printf("nr_layer_demapping_test: llr_cw allocation failed\n");
        free(llr_layers);
        return;
    }
    memset(llr_cw[0], 0, length * sizeof(int16_t));
    memset(llr_cw[1], 0, length * sizeof(int16_t));

    const int16_t sample_llr[] = {
        127, -120, 115, -110, 105, -100, 95, -90,
        85, -80, 75, -70, 65, -60, 55, -50
    };
    int sample_size = sizeof(sample_llr) / sizeof(sample_llr[0]);
    for (int layer = 0; layer < Nl; layer++) {
        for (int i = 0; i < sample_size && i < (int)layer_sz; i++) {
            llr_layers[layer][i] = sample_llr[i] + (layer * 10);
        }
    }

    printf("Starting layer demapping loop...\n");

    for (int iter = 0; iter < num_iterations; iter++) {
        llr_layers[0][0] = (int16_t)((iter * 5) & 0xFF);
        if (Nl > 1) {
            llr_layers[1][0] = (int16_t)((iter * 7) & 0xFF);
        }

        nr_dlsch_layer_demapping(llr_cw, Nl, mod_order, length,
                                 codeword_TB0, codeword_TB1,
                                 layer_sz, llr_layers);

        if ((iter % 20) == 0) {
            printf("  iter %3d: cw0[0]=%d cw0[1]=%d cw0[2]=%d\n",
                   iter, llr_cw[0][0], llr_cw[0][1], llr_cw[0][2]);
        }
    }

    printf("\n=== Final layer demapped output (first 16 LLRs of codeword 0) ===\n");
    for (int i = 0; i < 16 && i < (int)length; i++) {
        printf("llr_cw[0][%02d] = %d\n", i, llr_cw[0][i]);
    }

    free(llr_layers);
    free(llr_cw[0]);
    free(llr_cw[1]);
    printf("=== NR Layer Demapping tests completed ===\n");
}

void nr_crc_check()
{
    logInit();

    printf("=== Starting NR CRC Check tests ===\n");

    crcTableInit();

    const uint32_t payload_bits  = (uint32_t)size_tb;
    const uint32_t total_bits    = payload_bits + 24;
    const uint8_t  crc_type      = CRC24_A;
    const int      num_iterations = 10000000;
    const uint32_t total_bytes   = (total_bits + 7) / 8;

    printf("CRC check parameters: payload=%u bits, crc=24 bits (CRC24_A)\n", payload_bits);
    printf("Total: %u bits = %u bytes\n", total_bits, total_bytes);
    printf("Running %d iterations...\n", num_iterations);

    uint8_t *data = aligned_alloc(32, total_bytes);
    if (!data) {
        printf("nr_crc_check: data buffer allocation failed\n");
        return;
    }
    memset(data, 0, total_bytes);

    const uint8_t sample[] = {
        0x69, 0xBE, 0xF6, 0x02, 0xAD, 0xFB, 0x50, 0xE6,
        0xFF, 0x35, 0xA8, 0x44, 0xF9, 0x21, 0x92, 0xAA,
        0x68, 0x28, 0x2A
    };
    int sample_size = sizeof(sample) < total_bytes ? sizeof(sample) : total_bytes;
    memcpy(data, sample, sample_size);

    int crc_valid_count   = 0;
    int crc_invalid_count = 0;

    printf("Starting CRC check loop...\n");
    printf("Iterations 0-99:   Valid CRC (freshly computed)\n");
    printf("Iterations 100-199: Invalid CRC (corrupted data)\n");
    printf("Iterations 200-999: Valid CRC (regenerated)\n\n");

    for (int iter = 0; iter < num_iterations; iter++) {
        data[0] = (uint8_t)iter;

        uint32_t crc_val      = crc24a(data, payload_bits);
        uint32_t payload_bytes = (payload_bits + 7) / 8;
        uint32_t crc_shifted  = crc_val >> 8;
        data[payload_bytes]     = (uint8_t)((crc_shifted >> 16) & 0xFF);
        data[payload_bytes + 1] = (uint8_t)((crc_shifted >>  8) & 0xFF);
        data[payload_bytes + 2] = (uint8_t)( crc_shifted         & 0xFF);

        if (iter >= 100 && iter < 200) {
            /* Corrupt the last payload bit to make CRC invalid */
            data[payload_bytes - 1] ^= 0x01;
        }

        int is_valid = check_crc(data, total_bits, crc_type);

        if (is_valid) crc_valid_count++;
        else          crc_invalid_count++;

        if ((iter % 100) == 0 || (iter >= 99 && iter <= 101) || (iter >= 199 && iter <= 201)) {
            printf("  iter %3d: CRC %s (valid=%d, invalid=%d)\n",
                   iter, is_valid ? "OK  " : "FAIL", crc_valid_count, crc_invalid_count);
        }
    }

    printf("\n=== CRC Check Statistics ===\n");
    printf("Total iterations: %d\n", num_iterations);
    printf("Valid CRC count: %d\n", crc_valid_count);
    printf("Invalid CRC count: %d\n", crc_invalid_count);
    printf("Valid rate: %.1f%%\n", (100.0 * crc_valid_count) / num_iterations);

    printf("\n=== Final data buffer (first 16 bytes + CRC) ===\n");
    uint32_t payload_bytes = (payload_bits + 7) / 8;
    for (int i = 0; i < 16 && i < (int)payload_bytes; i++) {
        printf("data[%02d] = 0x%02X\n", i, data[i]);
    }
    printf("...\n");
    printf("data[%u] = 0x%02X (CRC byte 0)\n", payload_bytes,     data[payload_bytes]);
    printf("data[%u] = 0x%02X (CRC byte 1)\n", payload_bytes + 1, data[payload_bytes + 1]);
    printf("data[%u] = 0x%02X (CRC byte 2)\n", payload_bytes + 2, data[payload_bytes + 2]);

    free(data);
    printf("=== NR CRC Check tests completed ===\n");
}

void nr_soft_demod()
{
    logInit();

    printf("=== Starting NR Soft Demodulation (LLR computation) tests ===\n");

    const uint32_t rx_size_symbol  = 1024;
    const int      nbRx            = n_rx;
    const int      Nl              = n_layers;
    const uint32_t len             = rx_size_symbol;
    const unsigned char symbol     = 5;
    const uint32_t llr_offset_symbol = 0;
    const int      num_iterations  = getenv_int("OAI_ITERS", 10000000);
    const int      snr_db          = snr_db_input;
    const int      mod_order       = get_central_modulation_order();

    printf("Soft demod parameters: rx_symbol_size=%u, nbRx=%d, Nl=%d, len=%u\n",
           rx_size_symbol, nbRx, Nl, len);
    printf("mod_order=%d, SNR=%d dB\n", mod_order, snr_db);
    printf("Running %d iterations...\n", num_iterations);

    int32_t (*rxdataF_comp)[nbRx][rx_size_symbol * NR_SYMBOLS_PER_SLOT] =
        aligned_alloc(32, sizeof(int32_t) * Nl * nbRx * rx_size_symbol * NR_SYMBOLS_PER_SLOT);
    if (!rxdataF_comp) {
        printf("nr_soft_demod: rxdataF_comp allocation failed\n");
        return;
    }
    memset(rxdataF_comp, 0, sizeof(int32_t) * Nl * nbRx * rx_size_symbol * NR_SYMBOLS_PER_SLOT);

    c16_t *dl_ch_mag  = aligned_alloc(32, sizeof(c16_t) * rx_size_symbol);
    c16_t *dl_ch_magb = aligned_alloc(32, sizeof(c16_t) * rx_size_symbol);
    c16_t *dl_ch_magr = aligned_alloc(32, sizeof(c16_t) * rx_size_symbol);

    if (!dl_ch_mag || !dl_ch_magb || !dl_ch_magr) {
        printf("nr_soft_demod: channel magnitude buffer allocation failed\n");
        free(rxdataF_comp);
        free(dl_ch_mag);
        free(dl_ch_magb);
        return;
    }
    memset(dl_ch_mag,  0, sizeof(c16_t) * rx_size_symbol);
    memset(dl_ch_magb, 0, sizeof(c16_t) * rx_size_symbol);
    memset(dl_ch_magr, 0, sizeof(c16_t) * rx_size_symbol);

    const int layer_llr_size = len * 8;
    int16_t (*layer_llr)[layer_llr_size] = aligned_alloc(32, sizeof(int16_t) * Nl * layer_llr_size);
    if (!layer_llr) {
        printf("nr_soft_demod: layer_llr allocation failed\n");
        free(rxdataF_comp);
        free(dl_ch_mag);
        free(dl_ch_magb);
        free(dl_ch_magr);
        return;
    }
    memset(layer_llr, 0, sizeof(int16_t) * Nl * layer_llr_size);

    NR_UE_DLSCH_t dlsch[2];
    memset(dlsch, 0, sizeof(dlsch));
    dlsch[0].Nl = Nl;
    dlsch[0].dlsch_config.qamModOrder = (uint8_t)mod_order;

    NR_DL_UE_HARQ_t *dlsch0_harq = NULL;
    NR_DL_UE_HARQ_t *dlsch1_harq = NULL;

    for (uint32_t i = 0; i < len; i++) {
        if (mod_order >= 8) {
            dl_ch_mag[i].r  = 0x1800; dl_ch_mag[i].i  = 0x1800;
            dl_ch_magb[i].r = 0x3000; dl_ch_magb[i].i = 0x3000;
            dl_ch_magr[i].r = 0x6000; dl_ch_magr[i].i = 0x6000;
        } else if (mod_order >= 6) {
            dl_ch_mag[i].r  = 0x2000; dl_ch_mag[i].i  = 0x2000;
            dl_ch_magb[i].r = 0x4000; dl_ch_magb[i].i = 0x4000;
            dl_ch_magr[i].r = 0x5000; dl_ch_magr[i].i = 0x5000;
        } else if (mod_order >= 4) {
            dl_ch_mag[i].r  = 0x3000; dl_ch_mag[i].i  = 0x3000;
            dl_ch_magb[i].r = 0x4000; dl_ch_magb[i].i = 0x4000;
            dl_ch_magr[i].r = 0x5000; dl_ch_magr[i].i = 0x5000;
        } else {
            dl_ch_mag[i].r  = 0x4000; dl_ch_mag[i].i  = 0x4000;
            dl_ch_magb[i].r = 0x4000; dl_ch_magb[i].i = 0x4000;
            dl_ch_magr[i].r = 0x4000; dl_ch_magr[i].i = 0x4000;
        }
    }

    printf("Starting soft demodulation loop...\n");

    uint32_t rng_state = 0xDEADBEEFu ^ (uint32_t)time(NULL);

    for (int iter = 0; iter < num_iterations; iter++) {
        const double snr_lin    = pow(10.0, snr_db / 10.0);
        const double sigma      = 1.0 / sqrt(2.0 * snr_lin);
        const double inv_sigma2 = 1.0 / (sigma * sigma);

        for (int i = 0; i < (int)len; i++) {
            int bits_per_sym = mod_order >> 1;
            for (int b = 0; b < bits_per_sym && (i * bits_per_sym + b) < layer_llr_size; b++) {
                double sym   = 1.0;
                double noise = sigma * gaussian_noise(&rng_state);
                double y     = sym + noise;
                double llr   = 2.0 * y * inv_sigma2;
                if (llr >  32767.0) llr =  32767.0;
                if (llr < -32768.0) llr = -32768.0;
                layer_llr[0][i * bits_per_sym + b] = (int16_t)llr;
            }
        }

        nr_dlsch_llr(rx_size_symbol, nbRx, layer_llr_size, layer_llr,
                     rxdataF_comp, dl_ch_mag, dl_ch_magb, dl_ch_magr,
                     dlsch0_harq, dlsch1_harq, symbol, len, dlsch,
                     llr_offset_symbol);

        if ((iter % 20) == 0) {
            printf("  iter %3d: layer_llr[0][0]=%4d, layer_llr[0][1]=%4d\n",
                   iter, layer_llr[0][0], layer_llr[0][1]);
        }
    }

    printf("\n=== Final LLR output (first 16 soft bits, layer 0) ===\n");
    for (int i = 0; i < 16 && i < layer_llr_size; i++) {
        printf("  layer_llr[0][%02d] = %4d\n", i, layer_llr[0][i]);
    }

    free(rxdataF_comp);
    free(dl_ch_mag);
    free(dl_ch_magb);
    free(dl_ch_magr);
    free(layer_llr);
    printf("=== NR Soft Demodulation tests completed ===\n");
}

void nr_mmse_eq()
{
    logInit();

    printf("=== Starting NR MMSE Equalization tests ===\n");

    const uint32_t       rx_size_symbol = 1024;
    const unsigned char  n_rx_ue        = (unsigned char)n_rx;
    const unsigned char  nl             = (unsigned char)n_layers;
    const unsigned short nb_rb          = 52;
    const unsigned char  mod_order      = (unsigned char)get_central_modulation_order();
    const int            length         = (int)rx_size_symbol;
    const int            num_iterations = getenv_int("OAI_ITERS", 100000);
    const int            snr_db         = snr_db_input;

    const double snr_lin    = pow(10.0, snr_db / 10.0);
    const double noise_var_f = 1.0 / snr_lin;
    const uint32_t noise_var = (uint32_t)(noise_var_f * (1u << 15));

    printf("MMSE EQ parameters: rx_size=%u, n_rx=%u, nl=%u, nb_rb=%u, mod_order=%u\n",
           rx_size_symbol, n_rx_ue, nl, nb_rb, mod_order);
    printf("length=%d, SNR=%d dB, noise_var=%u\n", length, snr_db, noise_var);
    printf("Running %d iterations...\n", num_iterations);
    printf("NOTE: Using simplified MMSE wrapper (demonstrates structure)\n\n");

    int32_t (*rxdataF_comp)[n_rx_ue][rx_size_symbol * NR_SYMBOLS_PER_SLOT] =
        aligned_alloc(32, sizeof(int32_t) * nl * n_rx_ue * rx_size_symbol * NR_SYMBOLS_PER_SLOT);
    if (!rxdataF_comp) {
        printf("nr_mmse_eq: rxdataF_comp allocation failed\n");
        return;
    }

    for (int layer = 0; layer < nl; layer++) {
        for (int ant = 0; ant < n_rx_ue; ant++) {
            for (int k = 0; k < (int)rx_size_symbol; k++) {
                int16_t real_val = (int16_t)(20000 + ((layer * 4000 + ant * 1000 + k) % 10000) - 5000);
                int16_t imag_val = (int16_t)(18000 + ((layer * 3000 + ant *  750 + k) % 10000) - 5000);
                rxdataF_comp[layer][ant][k] = ((int32_t)imag_val << 16) | (real_val & 0xFFFF);
            }
        }
    }

    c16_t (*dl_ch_mag) [n_rx_ue][rx_size_symbol] = aligned_alloc(32, sizeof(c16_t) * nl * n_rx_ue * rx_size_symbol);
    c16_t (*dl_ch_magb)[n_rx_ue][rx_size_symbol] = aligned_alloc(32, sizeof(c16_t) * nl * n_rx_ue * rx_size_symbol);
    c16_t (*dl_ch_magr)[n_rx_ue][rx_size_symbol] = aligned_alloc(32, sizeof(c16_t) * nl * n_rx_ue * rx_size_symbol);

    if (!dl_ch_mag || !dl_ch_magb || !dl_ch_magr) {
        printf("nr_mmse_eq: channel magnitude buffer allocation failed\n");
        free(rxdataF_comp);
        free(dl_ch_mag);
        free(dl_ch_magb);
        return;
    }
    memset(dl_ch_mag,  0, sizeof(c16_t) * nl * n_rx_ue * rx_size_symbol);
    memset(dl_ch_magb, 0, sizeof(c16_t) * nl * n_rx_ue * rx_size_symbol);
    memset(dl_ch_magr, 0, sizeof(c16_t) * nl * n_rx_ue * rx_size_symbol);

    const int matrixSz = n_rx_ue * nl;
    int32_t (*dl_ch_estimates_ext)[rx_size_symbol] =
        aligned_alloc(32, sizeof(int32_t) * matrixSz * rx_size_symbol);
    if (!dl_ch_estimates_ext) {
        printf("nr_mmse_eq: dl_ch_estimates_ext allocation failed\n");
        free(rxdataF_comp);
        free(dl_ch_mag);
        free(dl_ch_magb);
        free(dl_ch_magr);
        return;
    }
    memset(dl_ch_estimates_ext, 0, sizeof(int32_t) * matrixSz * rx_size_symbol);

    const int32_t sample_rx[] = {
        0x10001000, 0x20002000, 0xF000F000, 0xE000E000,
        0x30003000, 0x40004000, 0xD000D000, 0xC000C000,
        0x50005000, 0x60006000, 0xB000B000, 0xA0009000
    };
    int sample_size = sizeof(sample_rx) / sizeof(sample_rx[0]);

    const unsigned char symbol = 5;
    for (int layer = 0; layer < nl; layer++) {
        for (int ant = 0; ant < n_rx_ue; ant++) {
            for (int i = 0; i < sample_size && i < length; i++) {
                rxdataF_comp[layer][ant][symbol * rx_size_symbol + i] =
                    sample_rx[i % sample_size] + (layer << 16) + (ant << 8);
            }
        }
    }

    const int32_t sample_ch[] = {
        0x30003000, 0x2F002F00, 0x31003100, 0x30003000
    };
    for (int idx = 0; idx < matrixSz; idx++) {
        for (int i = 0; i < length; i++) {
            int16_t ch_r = (int16_t)(12000 + ((idx * 1000 + i) % 4000) - 2000);
            int16_t ch_i = (int16_t)(11000 + ((idx * 1200 + i) % 3500) - 1750);
            dl_ch_estimates_ext[idx][i] = ((int32_t)ch_i << 16) | (int32_t)(uint16_t)ch_r;
        }
    }

    for (int layer = 0; layer < nl; layer++) {
        for (int ant = 0; ant < n_rx_ue; ant++) {
            for (int i = 0; i < length; i++) {
                dl_ch_mag [layer][ant][i].r = 0x2000;
                dl_ch_mag [layer][ant][i].i = 0x2000;
                dl_ch_magb[layer][ant][i].r = 0x4000;
                dl_ch_magb[layer][ant][i].i = 0x4000;
                dl_ch_magr[layer][ant][i].r = 0x6000;
                dl_ch_magr[layer][ant][i].i = 0x6000;
            }
        }
    }

    printf("Starting MMSE equalization loop...\n");
    printf("(Calling nr_dlsch_mmse for MMSE equalization)\n\n");

    const int total_size = rx_size_symbol * NR_SYMBOLS_PER_SLOT;
    int32_t (*original_data)[n_rx_ue][total_size] =
        aligned_alloc(32, sizeof(int32_t) * nl * n_rx_ue * total_size);
    if (!original_data) {
        printf("nr_mmse_eq: original_data allocation failed\n");
        free(rxdataF_comp);
        free(dl_ch_mag);
        free(dl_ch_magb);
        free(dl_ch_magr);
        free(dl_ch_estimates_ext);
        return;
    }

    for (int layer = 0; layer < nl; layer++) {
        for (int ant = 0; ant < n_rx_ue; ant++) {
            for (int k = 0; k < total_size; k++) {
                original_data[layer][ant][k] = rxdataF_comp[layer][ant][k];
            }
        }
    }

    const int check_idx = symbol * rx_size_symbol;
    for (int iter = 0; iter < num_iterations; iter++) {
        for (int layer = 0; layer < nl; layer++) {
            for (int ant = 0; ant < n_rx_ue; ant++) {
                for (int k = 0; k < total_size; k++) {
                    rxdataF_comp[layer][ant][k] = original_data[layer][ant][k];
                }
            }
        }

        nr_dlsch_mmse(rx_size_symbol, n_rx_ue, nl,
                      rxdataF_comp, dl_ch_mag, dl_ch_magb, dl_ch_magr,
                      dl_ch_estimates_ext, nb_rb, mod_order,
                      0, symbol, length, noise_var);

        if ((iter % 100000) == 0) {
            printf("  iter %7d: rxdataF_comp[0][0][%d] = 0x%08X\n",
                   iter, check_idx, (uint32_t)rxdataF_comp[0][0][check_idx]);
        }
    }

    printf("\n=== Final equalized output (first 8 samples, layer 0, ant 0) ===\n");
    int32_t *final_ptr = &rxdataF_comp[0][0][symbol * rx_size_symbol];
    for (int i = 0; i < 8; i++) {
        printf("  rxdataF_comp[%d] = 0x%08X\n", i, final_ptr[i]);
    }

    free(rxdataF_comp);
    free(dl_ch_mag);
    free(dl_ch_magb);
    free(dl_ch_magr);
    free(dl_ch_estimates_ext);
    free(original_data);
    printf("=== NR MMSE Equalization tests completed ===\n");
}

void nr_ldpc_dec()
{
    logInit();

    printf("=== Starting NR LDPC Decoder tests ===\n");

    const uint8_t  BG          = 1;
    const uint16_t Z           = (uint16_t)lift_size;
    const uint8_t  R           = 15;
    const uint8_t  numMaxIter  = 6;
    const int      Kprime      = 22 * Z;
    const int      num_iterations = getenv_int("OAI_ITERS", 100000);
    const int      snr_db      = snr_db_input;

    printf("LDPC parameters: BG=%u, Z=%u, R=%u, Kprime=%d bits\n", BG, Z, R, Kprime);
    printf("Max iterations: %u, SNR=%d dB\n", numMaxIter, snr_db);
    printf("Running %d LDPC decoding iterations...\n\n", num_iterations);

    const int output_bytes   = (Kprime + 7) / 8;
    const int input_llr_size = 66 * Z;

    int8_t *p_llr = aligned_alloc(32, input_llr_size * sizeof(int8_t));
    if (!p_llr) {
        printf("nr_ldpc_dec: Failed to allocate input LLR buffer\n");
        return;
    }
    memset(p_llr, 0, input_llr_size * sizeof(int8_t));

    int8_t *p_out = aligned_alloc(32, output_bytes * sizeof(int8_t));
    if (!p_out) {
        printf("nr_ldpc_dec: Failed to allocate output buffer\n");
        free(p_llr);
        return;
    }
    memset(p_out, 0, output_bytes * sizeof(int8_t));

    t_nrLDPC_dec_params decParams = {
        .BG         = BG,
        .Z          = Z,
        .R          = R,
        .numMaxIter = numMaxIter,
        .Kprime     = Kprime,
        .outMode    = nrLDPC_outMode_BITINT8,
        .crc_type   = 24,
        .check_crc  = NULL
    };

    t_nrLDPC_time_stats timeStats = {0};
    decode_abort_t      abortFlag = {0};

    printf("Starting LDPC decoding loop...\n");
    printf("(Calling LDPCdecoder from real OAI library)\n\n");

    uint32_t rng_state  = 0xACEDFACEu ^ (uint32_t)time(NULL);
    const double snr_lin   = pow(10.0, snr_db / 10.0);
    const double sigma     = 1.0 / sqrt(2.0 * snr_lin);
    const double inv_sigma2 = 1.0 / (sigma * sigma);

    const int info_bytes = (Kprime + 7) / 8;
    const int code_bits  = 66 * Z;
    uint8_t *info_bits   = aligned_alloc(32, info_bytes);
    uint8_t *coded_bits  = aligned_alloc(32, code_bits);
    if (!info_bits || !coded_bits) {
        printf("nr_ldpc_dec: Failed to allocate info/coded buffers\n");
        free(info_bits);
        free(coded_bits);
        free(p_llr);
        free(p_out);
        return;
    }
    memset(info_bits,  0, info_bytes);
    memset(coded_bits, 0, code_bits);

    encoder_implemparams_t encParams = {
        .Zc         = Z,
        .Kb         = 22,
        .BG         = BG,
        .K          = Kprime,
        .n_segments = segments,
        .first_seg  = 0,
        .gen_code   = 0,
        .tinput     = NULL,
        .tprep      = NULL,
        .tparity    = NULL,
        .toutput    = NULL,
        .F          = 0,
        .output     = NULL,
        .ans        = NULL
    };

    int total_iterations = 0;
    for (int iter = 0; iter < num_iterations; iter++) {
        for (int i = 0; i < info_bytes; i++) {
            uint32_t r  = xorshift32(&rng_state);
            info_bits[i] = (uint8_t)(r & 0xFF);
        }
        if ((Kprime & 7) != 0) {
            uint8_t mask = (1u << (Kprime & 7)) - 1u;
            info_bits[info_bytes - 1] &= mask;
        }

        uint8_t *info_ptr = info_bits;
        int enc_ret = LDPCencoder(&info_ptr, coded_bits, &encParams);
        if (enc_ret != 0) {
            printf("nr_ldpc_dec: LDPCencoder failed (%d) on iter %d\n", enc_ret, iter);
            break;
        }

        for (int idx = 0; idx < input_llr_size && idx < code_bits; idx++) {
            int    bit = coded_bits[idx] & 0x1;
            double y   = bit ? +1.0 : -1.0;
            y += sigma * gaussian_noise(&rng_state);
            double llr = 2.0 * y * inv_sigma2;
            if (llr >  127.0) llr =  127.0;
            if (llr < -127.0) llr = -127.0;
            p_llr[idx] = (int8_t)llr;
        }

        memset(p_out,    0, output_bytes * sizeof(int8_t));
        memset(&abortFlag, 0, sizeof(decode_abort_t));

        int32_t numIter = LDPCdecoder(&decParams, p_llr, p_out, &timeStats, &abortFlag);
        total_iterations += numIter;

        if ((iter % 3) == 0) {
            printf("  iter %2d: LDPCdecoder returned %d iterations, p_out[0]=0x%02X\n",
                   iter, numIter, (unsigned char)p_out[0]);
        }
    }

    printf("\n=== Final LDPC decoded output (first 16 bytes) ===\n");
    for (int i = 0; i < 16 && i < output_bytes; i++) {
        printf("  p_out[%2d] = 0x%02X\n", i, (unsigned char)p_out[i]);
    }

    printf("\n=== LDPC Decoding Statistics ===\n");
    printf("Total iterations across all %d decodings: %d\n", num_iterations, total_iterations);
    printf("Average iterations per decoding: %.1f\n",
           (float)total_iterations / num_iterations);

    free(coded_bits);
    free(info_bits);
    free(p_llr);
    free(p_out);
    printf("=== NR LDPC Decoder tests completed ===\n");
}

void nr_ofdm_demo()
{
    logInit();

    printf("=== Starting NR OFDM FEP Demonstration ===\n");

    const int ofdm_symbol_size  = 1024;
    const int nb_antennas_rx    = n_rx;
    const int nb_antennas_tx    = n_tx;
    const int symbols_per_slot  = 14;
    const int slots_per_frame   = 10;
    const int nb_prefix_samples = (ofdm_symbol_size == 2048)
                                  ? 176
                                  : (ofdm_symbol_size == 1024 ? 88 : (176 * ofdm_symbol_size / 2048));
    const int samples_per_frame = (ofdm_symbol_size + nb_prefix_samples) * symbols_per_slot * slots_per_frame;
    const int num_iterations    = 10000000;

    printf("OFDM parameters: FFT=%d, CP=%d, symbols/slot=%d, antennas=%d\n",
           ofdm_symbol_size, nb_prefix_samples, symbols_per_slot, nb_antennas_rx);

    int32_t *rxdata = aligned_alloc(32, samples_per_frame * sizeof(int32_t));
    if (!rxdata) {
        printf("nr_ofdm_demo: rxdata allocation failed\n");
        return;
    }
    memset(rxdata, 0, samples_per_frame * sizeof(int32_t));

    const int fd_per_slot_words = ofdm_symbol_size * symbols_per_slot;
    int32_t *rxdataF = aligned_alloc(32, fd_per_slot_words * sizeof(int32_t));
    if (!rxdataF) {
        printf("nr_ofdm_demo: rxdataF allocation failed\n");
        free(rxdata);
        return;
    }
    memset(rxdataF, 0, fd_per_slot_words * sizeof(int32_t));

    struct fep_frame_parms {
        int ofdm_symbol_size;
        int samples_per_slot_wCP;
        int nb_prefix_samples;
        int nb_prefix_samples0;
        int nb_antennas_rx;
        int symbols_per_slot;
        int slots_per_frame;
        int ofdm_offset_divisor;
    } frame_parms = {
        .ofdm_symbol_size      = ofdm_symbol_size,
        .samples_per_slot_wCP  = (ofdm_symbol_size + nb_prefix_samples) * symbols_per_slot,
        .nb_prefix_samples     = nb_prefix_samples,
        .nb_prefix_samples0    = nb_prefix_samples,
        .nb_antennas_rx        = nb_antennas_rx,
        .symbols_per_slot      = symbols_per_slot,
        .slots_per_frame       = slots_per_frame,
        .ofdm_offset_divisor   = 8
    };

    printf("Running %d iterations of OFDM FEP...\n", num_iterations);
    printf("Total slots available: %d slots/frame\n", slots_per_frame);
    printf("Processing pattern: each iteration processes all %d symbols in one slot\n\n", symbols_per_slot);

    uint32_t seed = 0xDEADBEEF;
    for (int i = 0; i < samples_per_frame; i++) {
        seed = seed * 1103515245 + 12345;
        int16_t real = (int16_t)((seed >> 16) & 0xFFFF);
        seed = seed * 1103515245 + 12345;
        int16_t imag = (int16_t)((seed >> 16) & 0xFFFF);
        rxdata[i] = (int32_t)real | ((int32_t)imag << 16);
    }

    printf("Frame data generated: %d total samples (%d per symbol including CP)\n",
           samples_per_frame, (ofdm_symbol_size + nb_prefix_samples) * symbols_per_slot);
    printf("Starting OFDM FEP processing loop (1 slot = %d symbols per iteration)...\n\n", symbols_per_slot);

    for (int iter = 0; iter < num_iterations; iter++) {
        int slot = iter % slots_per_frame;
        int samples_per_slot_data = (ofdm_symbol_size + nb_prefix_samples) * symbols_per_slot;
        int slot_offset = slot * samples_per_slot_data;

        for (int sym = 0; sym < symbols_per_slot; sym++) {
            int samples_per_symbol = ofdm_symbol_size + nb_prefix_samples;
            int symbol_offset = slot_offset + (sym * samples_per_symbol);

            memset(&rxdataF[sym * ofdm_symbol_size], 0, ofdm_symbol_size * sizeof(int32_t));

            int result = nr_slot_fep(
                NULL,
                &frame_parms,
                slot,
                sym,
                (void *)rxdataF,
                0,
                0,
                (void *)(&rxdata[symbol_offset])
            );

            if (iter == 0 && sym == 0) {
                printf("DEBUG: First slot processing started\n");
                printf("  Slot %d, symbols 0-%d, slot_offset=%d samples\n\n",
                       slot, symbols_per_slot - 1, slot_offset);
            }
        }

        if ((iter % (slots_per_frame * 10)) == 0) {
            printf("  iter %6d: processed slot %d (%d symbols, offset %5d)\n",
                   iter, slot, symbols_per_slot, slot_offset);
        }
    }

    printf("\n=== Final OFDM FEP output (first 8 samples of symbol 0) ===\n");
    for (int i = 0; i < 8 && i < ofdm_symbol_size; i++) {
        printf("rxdataF[%02d] = 0x%08X\n", i, ((uint32_t *)rxdataF)[i]);
    }

    free(rxdata);
    free(rxdataF);
    printf("=== NR OFDM FEP Demonstration completed ===\n");
}
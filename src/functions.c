#include "functions.h"
#include "modulation_tables.h"
#include "common/platform_types.h"
#include "PHY/TOOLS/tools_defs.h"
#include <dlfcn.h>

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
    c16_t *input;     /* frequency-domain input (fftsize) */
    c16_t *output;    /* time-domain output (nb_symbols * (prefix+fftsize)) */
    void *dlh;        /* handle from dlopen for libdfts.so (optional) */
    uint32_t rnd_state; /* xorshift PRNG state */
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

/* Initialize OFDM context: load dfts lib (path via env or default), allocate aligned buffers */
static ofdm_ctx_t *ofdm_init(const char *dfts_path, int fftsize, int nb_symbols, int nb_prefix_samples)
{
    ofdm_ctx_t *c = calloc(1, sizeof(*c));
    if (!c) return NULL;

    c->fftsize = fftsize;
    c->nb_symbols = nb_symbols;
    c->nb_prefix_samples = nb_prefix_samples;

    size_t in_sz = sizeof(c16_t) * (size_t)fftsize;
    size_t out_sz = sizeof(c16_t) * (size_t)nb_symbols * (size_t)(nb_prefix_samples + fftsize);

    c->input = aligned_alloc(32, in_sz);
    c->output = aligned_alloc(32, out_sz);

    if (!c->input || !c->output) {
        free(c->input);
        free(c->output);
        free(c);
        return NULL;
    }

    memset(c->input, 0, in_sz);
    memset(c->output, 0, out_sz);

    /* PRNG seed per-context */
    c->rnd_state = (uint32_t)time(NULL) ^ (uint32_t)(uintptr_t)c;

    /* Load libdfts: prefer env var, then provided path, then default relative path */
    const char *env = getenv("OAI_DFTS_LIB");
    const char *path = env ? env : dfts_path;
    if (!path) path = "./libdfts.so";

    c->dlh = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!c->dlh) {
        /* try relative to project build dir */
        c->dlh = dlopen("/home/anderson/dev/oai_isolation/ext/openair/cmake_targets/ran_build/build/libdfts.so", RTLD_NOW | RTLD_LOCAL);
        if (!c->dlh) {
            /* leave idft as-is; caller should check */
            printf("ofdm_init: dlopen failed (%s)\n", dlerror());
        }
    }

    if (c->dlh) {
        idft = (idftfunc_t)dlsym(c->dlh, "idft_implementation");
        if (!idft) {
            printf("ofdm_init: dlsym idft_implementation failed: %s\n", dlerror());
        }
    }

    return c;
}

static void ofdm_free(ofdm_ctx_t *c)
{
    if (!c) return;
    if (c->dlh) dlclose(c->dlh);
    free(c->input);
    free(c->output);
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

    /* Precoding parameters: basic PDSCH config */
    const int nb_layers = 2;                /* 2 layers (MIMO) */
    const int symbol_sz = 4096;             /* OFDM symbol size (12 RBs) */
    const int rbSize = 12;                  /* 12 resource blocks */
    const int nb_symbols = 14;               /* 14 OFDM symbols per slot */
    const int num_iterations = 100;

    /* Initialize precoding context and allocate buffers */
    precoding_ctx_t *ctx = precoding_init(nb_layers, symbol_sz, rbSize);
    if (!ctx) {
        printf("precoding_init failed\n");
        return;
    }

    /* Mock NR_DL_FRAME_PARMS for minimal do_onelayer support */
    NR_DL_FRAME_PARMS frame_parms = {
        .N_RB_DL = 106,                     /* 106 RBs = 20 MHz BW */
        .ofdm_symbol_size = symbol_sz,
        .first_carrier_offset = 0,
        .nb_antennas_tx = 2,
        .samples_per_slot_wCP = symbol_sz * 14  /* 14 symbols per slot */
    };
    ctx->frame_parms = &frame_parms;

    /* Mock nfapi_nr_dl_tti_pdsch_pdu_rel15_t for minimal do_onelayer support */
    nfapi_nr_dl_tti_pdsch_pdu_rel15_t rel15 = {
        .rbStart = 0,
        .rbSize = rbSize,
        .BWPStart = 0,
        .qamModOrder = {2, 2},  /* QPSK */
        .nrOfLayers = nb_layers,
        .dlDmrsSymbPos = 0x00,  /* No DMRS in this test */
        .pduBitmap = 0x00,      /* No PTRS in this test */
        .NrOfCodewords = 1,
        .StartSymbolIndex = 0,
        .NrOfSymbols = nb_symbols,
        .dlDmrsScramblingId = 0,
        .SCID = 0
    };
    ctx->rel15 = &rel15;

    printf("Precoding loop: %d layers, %d RBs, %d symbols per slot\n", 
           nb_layers, rbSize, nb_symbols);

    /* Main precoding loop: iterate and call do_onelayer for each symbol */
    for (int iter = 0; iter < num_iterations; iter++) {
        if ((iter % 10) == 0) printf("--- Precoding iteration %d ---\n", iter);

        /* Fill tx_layer with random 16-QAM data using xorshift */
        for (int layer = 0; layer < nb_layers; layer++) {
            for (int k = 0; k < symbol_sz; k++) {
                uint32_t r = xorshift32(&ctx->rnd_state);
                uint8_t idx_re = r & 3;
                uint8_t idx_im = (r >> 2) & 3;

                int idx = layer * symbol_sz + k;
                ctx->tx_layer[idx].r = (int16_t)qam16_levels[idx_re];
                ctx->tx_layer[idx].i = (int16_t)qam16_levels[idx_im];
            }
        }

        /* Process each OFDM symbol in the slot */
        for (int l_symbol = 0; l_symbol < nb_symbols; l_symbol++) {
            /* Call real do_onelayer for each layer */
            for (int layer = 0; layer < nb_layers; layer++) {
                int re_processed = do_onelayer(
                    ctx->frame_parms,           /* frame parameters */
                    0,                          /* slot */
                    ctx->rel15,                 /* PDU config */
                    layer,                      /* layer index */
                    ctx->output,                /* output buffer */
                    &ctx->tx_layer[layer * symbol_sz],  /* input layer signal */
                    0,                          /* start_sc */
                    symbol_sz,                  /* symbol size */
                    l_symbol,                   /* symbol index */
                    0,                          /* dlPtrsSymPos */
                    0,                          /* n_ptrs */
                    1000,                       /* amplitude */
                    15000,                      /* amplitude_dmrs */
                    0,                          /* l_prime */
                    NFAPI_NR_DMRS_TYPE1,        /* dmrs_type */
                    NULL                        /* dmrs_start */
                );

                if ((iter % 10) == 0 && l_symbol == 0) {
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
    const uint32_t size   = 2688;      /* bits */
    const uint8_t  q      = 0;
    const uint32_t Nid    = 0;
    const uint32_t n_RNTI = 0xFFFF;    /* 65535 */
    
    /* Derived buffer sizes: 2688 bits = 336 bytes; 2688/32 = 84 words */
    uint8_t  in[336] __attribute__((aligned(32)));
    uint32_t out[84]  __attribute__((aligned(32)));
    
    memset(in, 0, sizeof(in));
    memset(out, 0, sizeof(out));
    
    /* Optional: seed the input with a small sample pattern */
    const uint8_t sample[] = {
        0x69, 0xBE, 0xF6, 0x02, 0xAD, 0xFB, 0x50, 0xE6,
        0xFF, 0x35, 0xA8, 0x44, 0xF9, 0x21, 0x92, 0xAA,
        0x68, 0x28, 0x2A
    };
    memcpy(in, sample, sizeof(sample) < sizeof(in) ? sizeof(sample) : sizeof(in));
    
    printf("Starting scrambling tests...\n");
    
    /* Run the scrambling 1000 times (as requested earlier) with small variation */
    for (int iter = 0; iter < 1000; ++iter) {
        /* vary a byte so each run differs */
        in[0] = (uint8_t)iter;
        
        nr_codeword_scrambling(in, size, q, Nid, n_RNTI, out);
        
        if ((iter % 100) == 0) {
            printf("iter %d: out[0]=0x%08X\n", iter, (unsigned)out[0]);
        }
    }
    
    /* Print final output buffer (rounded size in 32-bit words) */
    int roundedSz = (size + 31) / 32;
    printf("Final output (roundedSz=%d):\n", roundedSz);
    for (int i = 0; i < roundedSz; ++i) {
        printf("out[%02d] = 0x%08X\n", i, out[i]);
    }    
}

void nr_crc(){
    printf("Start\n");
	unsigned char data[N / 8];
	srand(time(NULL));
	// Loop for 1000000 executions
	for (long long i = 0; i < 1000000; ++i) {
		printf("\rIteration: %lld", i);
		// Loop for CRC code
		for (int j = 0; j < N / 8; ++j){
			data[j] = rand() & 0xFF;
		}

		volatile unsigned int crc = crc24a(data, sizeof(data));
		(void)crc; // suppress unused variable warning
	}

	printf("\nEnd\n");
    }

/* Fill input buffer with random 16-QAM symbols in OAI split-complex format */
void fill_random_16qam(int32_t *input, int fftsize)
{
    for (int k = 0; k < fftsize; k++) {
        uint8_t idx_re = rand() & 3;
        uint8_t idx_im = rand() & 3;

        input[k]           = qam16_levels[idx_re];     // Real part of X[k]
        input[k + fftsize] = qam16_levels[idx_im];     // Imag part of X[k]
    }
}


/* Main OFDM test function – complete version */
void nr_ofdm_modulation()
{
    logInit();   // required by OAI

    /* Try to load the dfts shared object directly and retrieve the
     * dispatcher `idft_implementation` to set the global `idft` pointer.
     * This avoids the higher-level loader which depends on the config
     * subsystem (which isn't initialized in this small test program).
     */
    #include <dlfcn.h>
    // See if this wont pose a problem for contenainers
    const char *libpath = "/home/anderson/dev/oai_isolation/ext/openair/cmake_targets/ran_build/build/libdfts.so";
    void *dlh = dlopen(libpath, RTLD_NOW | RTLD_LOCAL);
    if (!dlh) {
        printf("Warning: dlopen(%s) failed: %s\n", libpath, dlerror());
    } else {
        idft = (idftfunc_t)dlsym(dlh, "idft_implementation");
        if (!idft) {
            printf("Warning: dlsym(idft_implementation) failed: %s\n", dlerror());
        }
    }

    const int fftsize           = 2048;
    const int nb_symbols        = 1;       // 1 OFDM symbol
    const int nb_prefix_samples = 176;     // CP for 2048-size FFT
    const Extension_t extype    = CYCLIC_PREFIX;

    /* Initialize context and buffers once, reuse across iterations */
    ofdm_ctx_t *ctx = ofdm_init("/home/anderson/dev/oai_isolation/ext/openair/cmake_targets/ran_build/build/libdfts.so",
                                fftsize, nb_symbols, nb_prefix_samples);
    if (!ctx) {
        printf("ofdm_init failed\n");
        return;
    }

    printf("=== Starting OFDM modulation tests (16-QAM random input) ===\n");

    const int iterations = 1000;
    for (int iter = 0; iter < iterations; iter++) {
        if ((iter % 10) == 0) printf("\n--- OFDM modulation iteration %d ---\n", iter);

        /* Fill entire frequency-domain buffer with random 16-QAM using xorshift */
        for (int k = 0; k < fftsize; k++) {
            uint32_t r = xorshift32(&ctx->rnd_state);
            uint8_t idx_re = r & 3;
            uint8_t idx_im = (r >> 2) & 3;
            ctx->input[k].r = (int16_t)qam16_levels[idx_re];
            ctx->input[k].i = (int16_t)qam16_levels[idx_im];
        }

        /* Optional: vary one subcarrier so we see change */
        ctx->input[100].r = (int16_t)iter;
        if ((iter % 10) == 0) printf("Input sample [100]: %d\n", ctx->input[100].r);

        /* Call OFDM mod using pre-allocated aligned buffers */
        PHY_ofdm_mod((int *)ctx->input,
                     (int *)ctx->output,
                     fftsize,
                     nb_symbols,
                     nb_prefix_samples,
                     extype);

        if ((iter % 10) == 0) {
            printf("iter %d: output[0]=(r=%d,i=%d), output[1]=(r=%d,i=%d)\n",
                   iter,
                   ctx->output[0].r, ctx->output[0].i,
                   ctx->output[1].r, ctx->output[1].i);
        }
    }

    printf("\n=== Final OFDM time-domain samples ===\n");
    for (int i = 0; i < 16; i++) {
        printf("output[%02d] = (r=%d,i=%d)\n", i, ctx->output[i].r, ctx->output[i].i);
    }

    ofdm_free(ctx);
}

void nr_modulation_test()
{
    /* Initialize the logging system first */
    logInit();
    
    printf("=== Starting NR Modulation tests (all QAM orders) ===\n");
    
    /* Test parameters */
    const uint32_t bits_per_mod[4] = { 2, 4, 6, 8 };              /* QPSK, 16-QAM, 64-QAM, 256-QAM */
    const char *mod_names[4] = { "QPSK", "16-QAM", "64-QAM", "256-QAM" };
    const int16_t *const tables[4] = { (int16_t *)qpsk_table, (int16_t *)qam16_table, (int16_t *)qam64_table, (int16_t *)qam256_table };
    const uint32_t table_sizes[4] = { 4, 16, 64, 256 };           /* Number of constellation points per modulation */
    const int num_iterations = 50;                                 /* 50 iterations per modulation order */
    const uint32_t length = 512;                                   /* Input bits per iteration */
    
    /* Allocate aligned buffers (max size for 256-QAM) */
    const uint32_t max_symbols = length / bits_per_mod[0];         /* length/2 for QPSK */
    const size_t out_sz = max_symbols * sizeof(int16_t) * 2;
    const size_t in_sz = ((length + 31) / 32) * sizeof(uint32_t);
    
    uint32_t *in = aligned_alloc(32, in_sz);
    int16_t *out = aligned_alloc(32, out_sz);
    
    if (!in || !out) {
        printf("nr_modulation_test: buffer allocation failed\n");
        free(in);
        free(out);
        return;
    }
    
    memset(out, 0, out_sz);
    
    /* Process each modulation order */
    for (int mod_idx = 0; mod_idx < 4; mod_idx++) {
        uint32_t mod_bits = bits_per_mod[mod_idx];
        const char *mod_name = mod_names[mod_idx];
        const int16_t *mod_table = tables[mod_idx];
        uint32_t table_size = table_sizes[mod_idx];
        uint32_t num_symbols = length / mod_bits;
        
        printf("\n=== Testing %s (mod_order=%u, %u symbols per iteration) ===\n", 
               mod_name, mod_bits, num_symbols);
        printf("Running %d iterations...\n", num_iterations);
        
        /* Main modulation loop */
        for (int iter = 0; iter < num_iterations; iter++) {
            /* Generate test pattern: alternate between 0x55555555 and 0xAAAAAAAA */
            uint32_t pattern = (iter % 2) ? 0x55555555 : 0xAAAAAAAA;
            size_t n_words = in_sz / sizeof(uint32_t);
            for (uint32_t i = 0; i < n_words; i++) {
                in[i] = pattern ^ (i * 0x11111111);
            }
            
            /* Manual modulation: extract mod_bits at a time and map to constellation */
            int16_t *out_ptr = out;
            uint8_t *in_bytes = (uint8_t *)in;
            uint32_t bit_mask = (1 << mod_bits) - 1;      /* Mask for mod_bits bits */
            
            for (uint32_t i = 0; i < num_symbols; i++) {
                /* Extract mod_bits bits from input */
                uint32_t byte_idx = (i * mod_bits) / 8;
                uint32_t bit_offset = (i * mod_bits) & 0x7;
                uint32_t symbol_idx = (in_bytes[byte_idx] >> bit_offset) & bit_mask;
                
                /* Wrap index if it exceeds table size (shouldn't happen with proper masks) */
                symbol_idx = symbol_idx % table_size;
                
                /* Map to constellation table */
                *out_ptr++ = mod_table[symbol_idx * 2 + 0];      /* I component */
                *out_ptr++ = mod_table[symbol_idx * 2 + 1];      /* Q component */
            }
            
            if ((iter % 10) == 0) {
                printf("  iter %3d: out[0..7]=[%6d, %6d, %6d, %6d, %6d, %6d, %6d, %6d]\n", 
                       iter,
                       out[0], out[1], out[2], out[3],
                       out[4], out[5], out[6], out[7]);
            }
        }
        
        /* Print constellation for this modulation order */
        printf("\n%s constellation points (first 8 of %u):\n", mod_name, table_size);
        for (uint32_t idx = 0; idx < 8 && idx < table_size; idx++) {
            printf("  Symbol[%2u]: I=%7d, Q=%7d\n", 
                   idx, 
                   mod_table[idx * 2 + 0], 
                   mod_table[idx * 2 + 1]);
        }
        if (table_size > 8) {
            printf("  ... (%u more points)\n", table_size - 8);
        }
    }
    
    free(in);
    free(out);
    printf("\n=== NR Modulation tests completed (all 4 modulation orders) ===\n");
}

void nr_layermapping()
{
    /* Initialize the logging system first */
    logInit();
    
    printf("=== Starting NR Layer Mapping tests ===\n");
    
    /* Layer mapping parameters */
    const int nbCodes = 1;                  /* 1 codeword */
    const int encoded_len = 512;            /* Encoded symbols length */
    const uint8_t n_layers = 2;             /* 2 layers (MIMO) */
    const int layerSz = 512;                /* Layer symbol size */
    const uint32_t n_symbs = 512;           /* Number of symbols */
    const int num_iterations = 50;          /* 50 iterations */
    
    /* Allocate modulated symbols buffer (aligned) */
    c16_t (*mod_symbs)[nbCodes][encoded_len] = aligned_alloc(64, sizeof(c16_t) * nbCodes * encoded_len);
    
    /* Allocate tx_layers output buffer (aligned) */
    c16_t (*tx_layers)[n_layers][layerSz] = aligned_alloc(64, sizeof(c16_t) * n_layers * layerSz);
    
    if (!mod_symbs || !tx_layers) {
        printf("nr_layermapping: buffer allocation failed\n");
        free(mod_symbs);
        free(tx_layers);
        return;
    }
    
    memset(mod_symbs, 0, sizeof(c16_t) * nbCodes * encoded_len);
    memset(tx_layers, 0, sizeof(c16_t) * n_layers * layerSz);
    
    printf("Layer mapping loop: %d codewords, %d layers, %u symbols per iteration\n", 
           nbCodes, n_layers, n_symbs);
    printf("Running %d iterations...\n", num_iterations);
    
    /* Main layer mapping loop */
    for (int iter = 0; iter < num_iterations; iter++) {
        /* Fill modulated symbols with random 16-QAM data using xorshift */
        uint32_t rnd_state = (uint32_t)time(NULL) ^ (uint32_t)(uintptr_t)mod_symbs ^ iter;
        
        for (int k = 0; k < encoded_len; k++) {
            uint32_t r = xorshift32(&rnd_state);
            uint8_t idx_re = r & 3;
            uint8_t idx_im = (r >> 2) & 3;
            
            (*mod_symbs)[0][k].r = (int16_t)qam16_levels[idx_re];
            (*mod_symbs)[0][k].i = (int16_t)qam16_levels[idx_im];
        }
        
        /* Call nr_layer_mapping to distribute symbols across layers */
        nr_layer_mapping(
            nbCodes,                    /* number of codewords */
            encoded_len,                /* encoded length per codeword */
            (c16_t (*)[encoded_len])(*mod_symbs),   /* modulated symbols - cast to proper array pointer */
            n_layers,                   /* number of layers */
            layerSz,                    /* layer symbol size */
            n_symbs,                    /* number of symbols */
            (c16_t (*)[layerSz])(*tx_layers)        /* output tx_layers - cast to proper array pointer */
        );
        
        if ((iter % 10) == 0) {
            printf("  iter %3d: tx_layer[0][0..7]=[(%6d,%6d), (%6d,%6d), (%6d,%6d), (%6d,%6d)]\n", 
                   iter,
                   (*tx_layers)[0][0].r, (*tx_layers)[0][0].i,
                   (*tx_layers)[0][1].r, (*tx_layers)[0][1].i,
                   (*tx_layers)[0][2].r, (*tx_layers)[0][2].i,
                   (*tx_layers)[0][3].r, (*tx_layers)[0][3].i);
        }
    }
    
    printf("\n=== Final layer mapping output (first 8 symbols per layer) ===\n");
    for (uint8_t layer = 0; layer < n_layers; layer++) {
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
    
    /* LDPC encoder parameters */
    const int BG = 1;                       /* Base Graph 1 (default) */
    const int Zc = 256;                     /* Lifting size (must be valid for BG1) */
    const int Kb = 22;                      /* Information bit columns */
    const int K = Kb * Zc;                  /* Total information bits */
    const int num_iterations = 10;          /* Reduced iterations for LDPC (slower) */
    const int n_segments = 1;               /* Single transport block segment */
    
    /* Calculate code rate and output size */
    int nrows = (BG == 1) ? 46 : 42;        /* Parity check rows */
    int rate = (BG == 1) ? 3 : 5;           /* Code rate */
    int no_punctured_columns = ((nrows - 2) * Zc + K - K * rate) / Zc;
    int removed_bit = (nrows - no_punctured_columns - 2) * Zc + K - (int)(K * rate);
    int output_length = K / 8 + ((nrows - no_punctured_columns) * Zc - removed_bit) / 8;
    
    /* Add extra buffer to prevent overrun issues */
    int output_buffer_size = output_length + 4096;
    
    printf("LDPC parameters: BG=%d, Zc=%d, Kb=%d, K=%d bits\n", BG, Zc, Kb, K);
    printf("Output size: %d bytes (buffer: %d bytes)\n", output_length, output_buffer_size);
    printf("Running %d iterations...\n", num_iterations);
    
    /* Allocate input buffer - only for the single segment we need */
    uint8_t *input_seg = malloc((K + 7) / 8);
    if (!input_seg) {
        printf("nr_ldpc: input buffer allocation failed\n");
        return;
    }
    memset(input_seg, 0, (K + 7) / 8);
    
    /* Create array of pointers for the encoder (expects uint8_t **) */
    uint8_t *input[1];
    input[0] = input_seg;
    
    /* Allocate output buffer */
    uint8_t *output = malloc(output_buffer_size);
    if (!output) {
        printf("nr_ldpc: output buffer allocation failed\n");
        free(input_seg);
        return;
    }
    memset(output, 0, output_buffer_size);
    
    /* LDPC encoder parameters structure */
    encoder_implemparams_t encoder_params = {
        .BG = BG,
        .Zc = Zc,
        .Kb = Kb,
        .K = K,
        .n_segments = n_segments,
        .first_seg = 0,
        .tinput = NULL,
        .tprep = NULL,
        .tparity = NULL
    };
    
    printf("Starting LDPC encoding loop...\n");
    
    /* Main LDPC encoding loop */
    for (int iter = 0; iter < num_iterations; iter++) {
        /* Fill input buffer with test pattern - varies per iteration */
        uint32_t pattern = (iter % 2) ? 0x55 : 0xAA;
        
        for (int byte_idx = 0; byte_idx < (K + 7) / 8; byte_idx++) {
            input_seg[byte_idx] = (uint8_t)(pattern ^ (byte_idx ^ iter));
        }
        
        /* Call LDPC encoder */
        int result = LDPCencoder(input, output, &encoder_params);
        
        if ((iter % 5) == 0) {
            printf("  iter %2d: encoder returned %d, output[0..3]=0x%02X 0x%02X 0x%02X 0x%02X\n", 
                   iter, result,
                   output[0], output[1], output[2], output[3]);
        }
        
        if (result != 0) {
            printf("ERROR: LDPCencoder failed at iteration %d\n", iter);
        }
    }
    
    printf("\n=== Final LDPC encoded output (first 16 bytes) ===\n");
    for (int i = 0; i < 16 && i < output_length; i++) {
        printf("output[%02d] = 0x%02X\n", i, output[i]);
    }
    
    /* Cleanup */
    free(input_seg);
    free(output);
    printf("=== NR LDPC Encoder tests completed ===\n");
}
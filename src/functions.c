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

void nr_ch_estimation()
{
    /* Initialize the logging system first */
    logInit();
    
    printf("=== Starting NR Channel Estimation (PDSCH) tests ===\n");
    
    /* Channel estimation parameters */
    const int nb_antennas_rx = 2;           /* 2 RX antennas */
    const int nb_antennas_tx = 1;           /* 1 TX antenna */
    const int ofdm_symbol_size = 2048;      /* FFT size */
    const int first_carrier_offset = 0;
    const int nb_rb_pdsch = 106;            /* 106 RBs = 20 MHz */
    const int symbols_per_slot = 14;        /* 14 symbols per slot */
    const int num_iterations = 10;
    
    printf("Channel estimation parameters: RX ant=%d, RB=%d, FFT=%d, symbols=%d\n",
           nb_antennas_rx, nb_rb_pdsch, ofdm_symbol_size, symbols_per_slot);
    
    /* Allocate RX frequency-domain data buffer (rxdataF) */
    int32_t *rxdataF_data = aligned_alloc(32, 
        nb_antennas_rx * symbols_per_slot * ofdm_symbol_size * sizeof(int32_t));
    if (!rxdataF_data) {
        printf("nr_ch_estimation: rxdataF allocation failed\n");
        return;
    }
    memset(rxdataF_data, 0, nb_antennas_rx * symbols_per_slot * ofdm_symbol_size * sizeof(int32_t));
    
    /* Create 2D array view for rxdataF */
    int32_t (*rxdataF)[ofdm_symbol_size] = (int32_t (*)[ofdm_symbol_size])rxdataF_data;
    
    /* Allocate downlink channel estimation buffer (dl_ch) */
    int32_t *dl_ch_data = aligned_alloc(32,
        nb_antennas_rx * nb_rb_pdsch * 12 * symbols_per_slot * sizeof(int32_t));
    if (!dl_ch_data) {
        printf("nr_ch_estimation: dl_ch allocation failed\n");
        free(rxdataF_data);
        return;
    }
    memset(dl_ch_data, 0, nb_antennas_rx * nb_rb_pdsch * 12 * symbols_per_slot * sizeof(int32_t));
    
    /* Create 2D array view for dl_ch */
    int32_t (*dl_ch)[nb_rb_pdsch * 12 * symbols_per_slot] = 
        (int32_t (*)[nb_rb_pdsch * 12 * symbols_per_slot])dl_ch_data;
    
    /* Create minimal frame parameters (use simple malloc to avoid incomplete type) */
    void *frame_parms_data = malloc(sizeof(NR_DL_FRAME_PARMS));
    if (!frame_parms_data) {
        printf("nr_ch_estimation: frame_parms allocation failed\n");
        free(rxdataF_data);
        free(dl_ch_data);
        return;
    }
    
    NR_DL_FRAME_PARMS *frame_parms = (NR_DL_FRAME_PARMS *)frame_parms_data;
    memset(frame_parms, 0, sizeof(NR_DL_FRAME_PARMS));
    frame_parms->N_RB_DL = nb_rb_pdsch;
    frame_parms->ofdm_symbol_size = ofdm_symbol_size;
    frame_parms->first_carrier_offset = first_carrier_offset;
    frame_parms->nb_antennas_rx = nb_antennas_rx;
    frame_parms->nb_antennas_tx = nb_antennas_tx;
    frame_parms->symbols_per_slot = symbols_per_slot;
    frame_parms->slots_per_frame = 10;
    frame_parms->nb_prefix_samples = 176;
    frame_parms->nb_prefix_samples0 = 176;
    frame_parms->samples_per_slot_wCP = (ofdm_symbol_size + 176) * symbols_per_slot;
    frame_parms->numerology_index = 0;
    frame_parms->ofdm_offset_divisor = 8;
    
    /* Initialize noise variance array */
    uint32_t *nvar = aligned_alloc(32, nb_antennas_rx * sizeof(uint32_t));
    if (!nvar) {
        printf("nr_ch_estimation: nvar allocation failed\n");
        free(rxdataF_data);
        free(dl_ch_data);
        free(frame_parms_data);
        return;
    }
    for (int i = 0; i < nb_antennas_rx; i++) {
        nvar[i] = 255;  /* Default noise variance estimate */
    }
    
    printf("Running %d iterations of PDSCH channel estimation...\n", num_iterations);
    
    /* Main channel estimation loop */
    for (int iter = 0; iter < num_iterations; iter++) {
        /* Fill rxdataF with test pattern - pseudo-random I/Q values */
        uint32_t seed = 0xDEADBEEF + iter;
        for (int ant = 0; ant < nb_antennas_rx; ant++) {
            for (int sym = 0; sym < symbols_per_slot; sym++) {
                for (int k = 0; k < ofdm_symbol_size; k++) {
                    seed = seed * 1103515245 + 12345;
                    int16_t real = (int16_t)((seed >> 16) & 0xFFFF);
                    
                    seed = seed * 1103515245 + 12345;
                    int16_t imag = (int16_t)((seed >> 16) & 0xFFFF);
                    
                    rxdataF[ant * symbols_per_slot + sym][k] = 
                        (int32_t)real | ((int32_t)imag << 16);
                }
            }
        }
        
        /* Call nr_pdsch_channel_estimation for each symbol */
        for (int symbol = 0; symbol < symbols_per_slot; symbol++) {
            nr_pdsch_channel_estimation(
                NULL,                          /* PHY_VARS_NR_UE (can be NULL) */
                frame_parms,                   /* NR_DL_FRAME_PARMS */
                symbol,                        /* Symbol index */
                0,                             /* gNB index */
                nb_antennas_rx,                /* Number of RX antennas */
                &dl_ch[0][0],                  /* DL channel estimation output */
                (void *)rxdataF,               /* RX frequency-domain data */
                nvar                           /* Noise variance estimates */
            );
        }
        
        if ((iter % 5) == 0) {
            printf("  iter %2d: dl_ch[0][0]=0x%08X dl_ch[0][1]=0x%08X\n",
                   iter,
                   ((uint32_t *)dl_ch)[0],
                   ((uint32_t *)dl_ch)[1]);
        }
    }
    
    printf("\n=== Final channel estimation output (first 8 samples) ===\n");
    for (int i = 0; i < 8; i++) {
        printf("dl_ch[0][%02d] = 0x%08X\n", i, ((uint32_t *)dl_ch)[i]);
    }
    
    /* Cleanup */
    free(rxdataF_data);
    free(dl_ch_data);
    free(frame_parms_data);
    free(nvar);
    printf("=== NR Channel Estimation (PDSCH) tests completed ===\n");
}

void nr_descrambling()
{
    /* Initialize the logging system first */
    logInit();
    
    /* Initialize SIMD byte2m128i lookup table for unscrambling */
    init_byte2m128i();
    
    printf("=== Starting NR DLSCH Descrambling tests ===\n");
    
    /* Descrambling parameters */
    const uint32_t size = 2688;        /* bits (LLR size) */
    const uint8_t q = 0;
    const uint32_t Nid = 0;
    const uint32_t n_RNTI = 0xFFFF;    /* 65535 */
    
    /* Buffer size: allocate extra space for SIMD operations safety margin */
    const int buffer_size = 4096;      /* Large safe buffer for SIMD */
    const int num_iterations = 100;    /* Reduced iterations for descrambling */
    
    printf("Descrambling parameters: size=%u bits, q=%u, Nid=%u, n_RNTI=0x%X\n",
           size, q, Nid, n_RNTI);
    printf("Buffer size: %d LLRs (allocated for %u LLRs needed)\n", buffer_size, size);
    
    /* Allocate LLR buffer (aligned to 32 bytes for SIMD) - int16_t for soft bits */
    int16_t *llr = aligned_alloc(32, buffer_size * sizeof(int16_t));
    if (!llr) {
        printf("nr_descrambling: llr allocation failed\n");
        return;
    }
    memset(llr, 0, buffer_size * sizeof(int16_t));
    
    /* Optional: seed the LLR input with a sample pattern (soft values) */
    const int16_t sample_llr[] = {
        127, -120, 115, -110, 105, -100, 95, -90,
        85, -80, 75, -70, 65, -60, 55, -50,
        45, -40, 35
    };
    
    int sample_size = sizeof(sample_llr) / sizeof(sample_llr[0]);
    for (int i = 0; i < sample_size && i < (int)size; i++) {
        llr[i] = sample_llr[i];
    }
    
    printf("Running %d iterations of DLSCH descrambling...\n", num_iterations);
    
    /* Main descrambling loop */
    for (int iter = 0; iter < num_iterations; iter++) {
        /* Vary first LLR so each run differs */
        llr[0] = (int16_t)((iter * 7) & 0xFF);
        
        /* Call nr_dlsch_unscrambling to descramble the LLRs */
        nr_dlsch_unscrambling(llr, size, q, Nid, n_RNTI);
        
        if ((iter % 20) == 0) {
            printf("  iter %3d: llr[0]=%d llr[1]=%d llr[2]=%d\n",
                   iter, llr[0], llr[1], llr[2]);
        }
    }
    
    printf("\n=== Final descrambled LLR output (first 16 values) ===\n");
    for (int i = 0; i < 16 && i < (int)size; i++) {
        printf("llr[%02d] = %d\n", i, llr[i]);
    }
    
    /* Cleanup */
    free(llr);
    printf("=== NR DLSCH Descrambling tests completed ===\n");
}

void nr_layer_demapping_test()
{
    /* Initialize the logging system first */
    logInit();
    
    printf("=== Starting NR Layer Demapping tests ===\n");
    
    /* Layer demapping parameters */
    const uint8_t Nl = 2;                   /* 2 layers */
    const uint8_t mod_order = 4;            /* 16-QAM (4 bits per symbol) */
    const uint32_t length = 2048;           /* Total LLRs to process */
    const int32_t codeword_TB0 = 0;         /* Codeword 0 active */
    const int32_t codeword_TB1 = -1;        /* Codeword 1 inactive */
    const int num_iterations = 100;
    
    /* Calculate layer buffer size */
    const uint32_t layer_sz = length;       /* Each layer holds all LLRs */
    
    printf("Layer demapping parameters: Nl=%u, mod_order=%u, length=%u\n",
           Nl, mod_order, length);
    printf("Codewords: TB0=%d, TB1=%d\n", codeword_TB0, codeword_TB1);
    printf("Running %d iterations...\n", num_iterations);
    
    /* Allocate layer LLR buffers (input to demapping) */
    int16_t (*llr_layers)[layer_sz] = aligned_alloc(32, Nl * layer_sz * sizeof(int16_t));
    if (!llr_layers) {
        printf("nr_layer_demapping_test: llr_layers allocation failed\n");
        return;
    }
    memset(llr_layers, 0, Nl * layer_sz * sizeof(int16_t));
    
    /* Allocate codeword LLR buffers (output from demapping) */
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
    
    /* Seed layer LLRs with sample pattern */
    const int16_t sample_llr[] = {
        127, -120, 115, -110, 105, -100, 95, -90,
        85, -80, 75, -70, 65, -60, 55, -50
    };
    int sample_size = sizeof(sample_llr) / sizeof(sample_llr[0]);
    
    for (int layer = 0; layer < Nl; layer++) {
        for (int i = 0; i < sample_size && i < (int)layer_sz; i++) {
            llr_layers[layer][i] = sample_llr[i] + (layer * 10);  /* Offset per layer */
        }
    }
    
    printf("Starting layer demapping loop...\n");
    
    /* Main layer demapping loop */
    for (int iter = 0; iter < num_iterations; iter++) {
        /* Vary first LLR so each run differs */
        llr_layers[0][0] = (int16_t)((iter * 5) & 0xFF);
        if (Nl > 1) {
            llr_layers[1][0] = (int16_t)((iter * 7) & 0xFF);
        }
        
        /* Call nr_dlsch_layer_demapping to perform layer demapping */
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
    
    /* Cleanup */
    free(llr_layers);
    free(llr_cw[0]);
    free(llr_cw[1]);
    printf("=== NR Layer Demapping tests completed ===\n");
}

void nr_crc_check()
{
    /* Initialize the logging system first */
    logInit();
    
    printf("=== Starting NR CRC Check tests ===\n");
    
    /* Initialize CRC tables (required before using check_crc) */
    crcTableInit();
    
    /* CRC check parameters */
    const uint32_t payload_bits = 2688;     /* Payload length in bits (without CRC) */
    const uint32_t total_bits = payload_bits + 24;  /* Total with CRC24 */
    const uint8_t crc_type = CRC24_A;       /* CRC24-A (default for NR) */
    const int num_iterations = 1000;        /* 1000 iterations */
    
    /* Buffer size in bytes */
    const uint32_t total_bytes = (total_bits + 7) / 8;
    
    printf("CRC check parameters: payload=%u bits, crc=24 bits (CRC24_A)\n", payload_bits);
    printf("Total: %u bits = %u bytes\n", total_bits, total_bytes);
    printf("Running %d iterations...\n", num_iterations);
    
    /* Allocate data buffer with CRC space (aligned) */
    uint8_t *data = aligned_alloc(32, total_bytes);
    if (!data) {
        printf("nr_crc_check: data buffer allocation failed\n");
        return;
    }
    memset(data, 0, total_bytes);
    
    /* Seed data with sample pattern */
    const uint8_t sample[] = {
        0x69, 0xBE, 0xF6, 0x02, 0xAD, 0xFB, 0x50, 0xE6,
        0xFF, 0x35, 0xA8, 0x44, 0xF9, 0x21, 0x92, 0xAA,
        0x68, 0x28, 0x2A
    };
    int sample_size = sizeof(sample) < total_bytes ? sizeof(sample) : total_bytes;
    memcpy(data, sample, sample_size);
    
    /* Statistics counters */
    int crc_valid_count = 0;
    int crc_invalid_count = 0;
    
    printf("Starting CRC check loop...\n");
    printf("Iterations 0-99:   Valid CRC (freshly computed)\n");
    printf("Iterations 100-199: Invalid CRC (corrupted data)\n");
    printf("Iterations 200-999: Valid CRC (regenerated)\n\n");
    
    /* Main CRC check loop */
    for (int iter = 0; iter < num_iterations; iter++) {
        /* Vary first byte so each run differs */
        data[0] = (uint8_t)iter;
        
        if (iter < 100) {
            /* Compute CRC24_A over the payload bits only */
            uint32_t crc_val = crc24a(data, payload_bits);
            
            /* Store CRC24 in the last 24 bits (3 bytes) of buffer
             * check_crc() expects CRC in bits 8-31 of the crc24a() result
             * So we shift right by 8 before storing */
            uint32_t payload_bytes = (payload_bits + 7) / 8;
            uint32_t crc_shifted = crc_val >> 8;  /* Get bits 8-31 */
            data[payload_bytes] = (uint8_t)((crc_shifted >> 16) & 0xFF);
            data[payload_bytes + 1] = (uint8_t)((crc_shifted >> 8) & 0xFF);
            data[payload_bytes + 2] = (uint8_t)(crc_shifted & 0xFF);
            
        } else if (iter < 200) {
            /* For corrupted case: compute valid CRC first, then corrupt it */
            uint32_t crc_val = crc24a(data, payload_bits);
            uint32_t payload_bytes = (payload_bits + 7) / 8;
            uint32_t crc_shifted = crc_val >> 8;  /* Get bits 8-31 */
            data[payload_bytes] = (uint8_t)((crc_shifted >> 16) & 0xFF);
            data[payload_bytes + 1] = (uint8_t)((crc_shifted >> 8) & 0xFF);
            data[payload_bytes + 2] = (uint8_t)(crc_shifted & 0xFF);
            
            /* Corrupt the last bit of payload to make CRC invalid */
            data[payload_bytes - 1] ^= 0x01;
            
        } else {
            /* Regenerate valid CRC */
            uint32_t crc_val = crc24a(data, payload_bits);
            uint32_t payload_bytes = (payload_bits + 7) / 8;
            uint32_t crc_shifted = crc_val >> 8;  /* Get bits 8-31 */
            data[payload_bytes] = (uint8_t)((crc_shifted >> 16) & 0xFF);
            data[payload_bytes + 1] = (uint8_t)((crc_shifted >> 8) & 0xFF);
            data[payload_bytes + 2] = (uint8_t)(crc_shifted & 0xFF);
        }
        
        /* Call check_crc to verify CRC
         * Returns 1 if CRC is valid, 0 if invalid
         * Parameter n is TOTAL bits (payload + CRC) */
        int is_valid = check_crc(data, total_bits, crc_type);
        
        if (is_valid) {
            crc_valid_count++;
        } else {
            crc_invalid_count++;
        }
        
        if ((iter % 100) == 0 || (iter >= 99 && iter <= 101) || (iter >= 199 && iter <= 201)) {
            printf("  iter %3d: CRC %s (valid=%d, invalid=%d)\n",
                   iter, is_valid ? "OK  " : "FAIL", crc_valid_count, crc_invalid_count);
        }
    }
    
    printf("\n=== CRC Check Statistics ===\n");
    printf("Total iterations: %d\n", num_iterations);
    printf("Valid CRC count: %d (expected ~800: iters 0-99 + 200-999)\n", crc_valid_count);
    printf("Invalid CRC count: %d (expected ~200: iters 100-199)\n", crc_invalid_count);
    printf("Valid rate: %.1f%%\n", (100.0 * crc_valid_count) / num_iterations);
    
    printf("\n=== Final data buffer (first 16 bytes + CRC) ===\n");
    uint32_t payload_bytes = (payload_bits + 7) / 8;
    for (int i = 0; i < 16 && i < (int)payload_bytes; i++) {
        printf("data[%02d] = 0x%02X\n", i, data[i]);
    }
    printf("...\n");
    printf("data[%u] = 0x%02X (CRC byte 0)\n", payload_bytes, data[payload_bytes]);
    printf("data[%u] = 0x%02X (CRC byte 1)\n", payload_bytes + 1, data[payload_bytes + 1]);
    printf("data[%u] = 0x%02X (CRC byte 2)\n", payload_bytes + 2, data[payload_bytes + 2]);
    
    /* Cleanup */
    free(data);
    printf("=== NR CRC Check tests completed ===\n");
}

void nr_ofdm_demo()
{
    /* Initialize the logging system first */
    logInit();
    
    printf("=== Starting NR OFDM FEP Demonstration ===\n");
    
    /* OFDM Frame Parameters */
    const int ofdm_symbol_size = 2048;      /* FFT size */
    const int nb_antennas_rx = 1;           /* 1 RX antenna */
    const int nb_antennas_tx = 1;           /* 1 TX antenna */
    const int symbols_per_slot = 14;        /* 14 symbols per slot (normal CP) */
    const int slots_per_frame = 10;         /* 10 slots per 10ms frame */
    const int nb_prefix_samples = 176;      /* Cyclic prefix samples */
    const int samples_per_frame = (ofdm_symbol_size + nb_prefix_samples) * symbols_per_slot * slots_per_frame;
    const int num_iterations = 10;
    
    printf("OFDM parameters: FFT=%d, CP=%d, symbols/slot=%d, antennas=%d\n",
           ofdm_symbol_size, nb_prefix_samples, symbols_per_slot, nb_antennas_rx);
    
    /* Allocate RX data buffers (aligned) */
    int32_t *rxdata = aligned_alloc(32, samples_per_frame * sizeof(int32_t));
    if (!rxdata) {
        printf("nr_ofdm_demo: rxdata allocation failed\n");
        return;
    }
    memset(rxdata, 0, samples_per_frame * sizeof(int32_t));
    
    /* Allocate RX frequency-domain data buffer for one OFDM symbol */
    int32_t *rxdataF = aligned_alloc(32, ofdm_symbol_size * 2 * sizeof(int32_t));
    if (!rxdataF) {
        printf("nr_ofdm_demo: rxdataF allocation failed\n");
        free(rxdata);
        return;
    }
    memset(rxdataF, 0, ofdm_symbol_size * 2 * sizeof(int32_t));
    
    /* Initialize NR DL Frame Parameters structure */
    NR_DL_FRAME_PARMS frame_parms = {
        .N_RB_DL = 106,                             /* 106 RBs = 20 MHz */
        .ofdm_symbol_size = ofdm_symbol_size,
        .first_carrier_offset = 0,
        .nb_antennas_rx = nb_antennas_rx,
        .nb_antennas_tx = nb_antennas_tx,
        .symbols_per_slot = symbols_per_slot,
        .slots_per_frame = slots_per_frame,
        .nb_prefix_samples = nb_prefix_samples,
        .nb_prefix_samples0 = nb_prefix_samples,
        .samples_per_slot_wCP = (ofdm_symbol_size + nb_prefix_samples) * symbols_per_slot,
        .samples_per_frame = samples_per_frame,
        .numerology_index = 0,                      /* 15 kHz subcarrier spacing */
        .ofdm_offset_divisor = 8
    };
    
    printf("Running %d iterations of OFDM FEP...\n", num_iterations);
    
    /* Main OFDM FEP loop */
    for (int iter = 0; iter < num_iterations; iter++) {
        /* Fill RX data with test pattern - simple sinusoidal-like values */
        uint32_t seed = 0xDEADBEEF;
        for (int i = 0; i < samples_per_frame; i++) {
            seed = seed * 1103515245 + 12345;
            int16_t real = (int16_t)((seed >> 16) & 0xFFFF);
            
            seed = seed * 1103515245 + 12345;
            int16_t imag = (int16_t)((seed >> 16) & 0xFFFF);
            
            rxdata[i] = (int32_t)real | ((int32_t)imag << 16);
        }
        
        /* Process each slot in the frame */
        for (int slot = 0; slot < slots_per_frame; slot++) {
            /* Process each OFDM symbol in the slot */
            for (int symbol = 0; symbol < symbols_per_slot; symbol++) {
                /* Call nr_slot_fep to perform DFT on time-domain OFDM symbol */
                int result = nr_slot_fep(
                    NULL,                           /* PHY_VARS_NR_UE (NULL for this demo) */
                    &frame_parms,                   /* Frame parameters */
                    slot,                           /* Slot number */
                    symbol,                         /* Symbol index */
                    (void *)rxdataF,                /* RX data FD */
                    0,                              /* Downlink (link_type_dl = 0) */
                    0,                              /* Sample offset */
                    (void *)&rxdata                 /* RX data TD */
                );
                
                if (result != 0) {
                    printf("ERROR: nr_slot_fep failed at slot %d, symbol %d\n", slot, symbol);
                }
            }
        }
        
        if ((iter % 5) == 0) {
            printf("  iter %2d: processed slot 0, rxdataF[0]=0x%08X rxdataF[1]=0x%08X\n",
                   iter,
                   ((uint32_t *)rxdataF)[0],
                   ((uint32_t *)rxdataF)[1]);
        }
    }
    
    printf("\n=== Final OFDM FEP output (first 8 frequency-domain samples) ===\n");
    for (int i = 0; i < 8 && i < ofdm_symbol_size * 2; i++) {
        printf("rxdataF[%02d] = 0x%08X\n", i, ((uint32_t *)rxdataF)[i]);
    }
    
    /* Cleanup */
    free(rxdata);
    free(rxdataF);
    printf("=== NR OFDM FEP Demonstration completed ===\n");
}
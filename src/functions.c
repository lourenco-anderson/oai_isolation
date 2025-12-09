#include "functions.h"
#include "common/platform_types.h"
#include "PHY/TOOLS/tools_defs.h"
#include <dlfcn.h>

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

/* Gray-coded amplitude levels for 16-QAM */
static const int32_t qam16_levels[4] = { -3, -1, +1, +3 };

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

    const int iterations = 10;
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
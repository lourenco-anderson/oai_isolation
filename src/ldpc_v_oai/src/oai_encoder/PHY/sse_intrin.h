/* sse_intrin.h – compatibilidade standalone
 * Gerado por fix_build.py / adapt_oai_encoder.py
 *
 * Usa SIMDE se instalado (libsimde-dev), senão intrinsics nativos,
 * senão fallback escalar puro.
 */
#pragma once

#if __has_include(<simde/x86/avx2.h>)
/* ── SIMDE instalado ─────────────────────────────────────────────────── */
# define SIMDE_ENABLE_NATIVE_ALIASES
# include <simde/x86/avx2.h>
# if __has_include(<simde/x86/avx512f.h>)
#  include <simde/x86/avx512f.h>
# endif
# if __has_include(<simde/x86/avx512vbmi.h>)
#  include <simde/x86/avx512vbmi.h>
# endif
# if __has_include(<simde/x86/ssse3.h>)
#  include <simde/x86/ssse3.h>
# endif
# if __has_include(<simde/x86/sse4.1.h>)
#  include <simde/x86/sse4.1.h>
# endif

#elif defined(__AVX2__)
/* ── Intrinsics GCC/clang nativos ───────────────────────────────────── */
# include <immintrin.h>
  typedef __m128i  simde__m128i;
  typedef __m256i  simde__m256i;
  typedef __m512i  simde__m512i;
# define simde_mm_xor_si128(a,b)                   _mm_xor_si128(a,b)
# define simde_mm256_xor_si256(a,b)                _mm256_xor_si256(a,b)
# define simde_mm_alignr_epi8(a,b,c)               _mm_alignr_epi8(a,b,c)
# define simde_mm256_alignr_epi8(a,b,c)            _mm256_alignr_epi8(a,b,c)
# define simde_mm256_permute2x128_si256(a,b,c)     _mm256_permute2x128_si256(a,b,c)
# define simde_mm256_inserti128_si256(a,b,c)       _mm256_inserti128_si256(a,b,c)
# define simde_mm256_extracti128_si256(a,b)        _mm256_extracti128_si256(a,b)
# define simde_mm_loadu_si128(p)                   _mm_loadu_si128(p)
# define simde_mm_storeu_si128(p,a)                _mm_storeu_si128(p,a)
# define simde_mm256_loadu_si256(p)                _mm256_loadu_si256(p)
# define simde_mm256_storeu_si256(p,a)             _mm256_storeu_si256(p,a)
# ifdef __AVX512F__
#  define simde_mm512_xor_si512(a,b)               _mm512_xor_si512(a,b)
#  define simde_mm512_alignr_epi8(a,b,c)           _mm512_alignr_epi8(a,b,c)
#  ifdef __AVX512VBMI__
#   define simde_mm512_permutex2var_epi8(a,b,c)    _mm512_permutex2var_epi8(a,b,c)
#  endif
# endif

#else
/* ── Fallback escalar puro (sem SIMD) ───────────────────────────────── */
# include <stdint.h>
  typedef struct { uint8_t b[16]; } simde__m128i;
  typedef struct { uint8_t b[32]; } simde__m256i;
  typedef struct { uint8_t b[64]; } simde__m512i;
  static inline simde__m128i simde_mm_xor_si128(simde__m128i a, simde__m128i b)
  { simde__m128i r; for(int i=0;i<16;i++) r.b[i]=a.b[i]^b.b[i]; return r; }
  static inline simde__m256i simde_mm256_xor_si256(simde__m256i a, simde__m256i b)
  { simde__m256i r; for(int i=0;i<32;i++) r.b[i]=a.b[i]^b.b[i]; return r; }
  static inline simde__m512i simde_mm512_xor_si512(simde__m512i a, simde__m512i b)
  { simde__m512i r; for(int i=0;i<64;i++) r.b[i]=a.b[i]^b.b[i]; return r; }
#endif

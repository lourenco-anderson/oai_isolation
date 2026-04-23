/* sse_intrin.h – compatibilidade standalone usando SIMDE */
#pragma once
#define SIMDE_ENABLE_NATIVE_ALIASES
#include <simde/x86/avx2.h>
#include <simde/x86/avx512f.h>
#include <simde/x86/avx512vbmi.h>
#include <simde/x86/ssse3.h>
#include <simde/x86/sse4.1.h>

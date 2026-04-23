/* assertions.h – compatibilidade standalone */
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#define AssertFatal(cond, fmt, ...) \
    do { if (!(cond)) { fprintf(stderr, "ASSERT FAIL: " fmt "\n", ##__VA_ARGS__); abort(); } } while(0)
#define DevAssert(cond)  AssertFatal(cond, "DevAssert failed at %s:%d", __FILE__, __LINE__)
#define DevCheck(cond, x, y, z) AssertFatal(cond, "DevCheck failed")

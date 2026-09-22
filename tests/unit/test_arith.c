#include <stdint.h>

#include "../binary_stdio.h"
#include "arith.h"
#include "check.h"

/* The upper half of the full product and the saturating operations at
   every width, signed and unsigned. A value holds the bits of its width,
   and a signed result carries its sign above them. */
void test_arith(void)
{
    /* The upper half of the full product. */
    CHECK(arith_mul_high(UINT64_MAX, UINT64_MAX, 64, false) ==
          UINT64_MAX - 1);
    CHECK(arith_mul_high((uint64_t)1 << 63, 4, 64, false) == 2);
    CHECK(arith_mul_high(UINT64_MAX, 1, 64, false) == 0);
    CHECK(arith_mul_high((uint64_t)-1, 1, 64, true) == (uint64_t)-1);
    CHECK(arith_mul_high((uint64_t)1 << 63, (uint64_t)1 << 63, 64, true) ==
          (uint64_t)1 << 62);
    CHECK(arith_mul_high((uint64_t)1 << 63, (uint64_t)-1, 64, true) == 0);
    CHECK(arith_mul_high((uint64_t)1 << 40, (uint64_t)-((int64_t)1 << 40), 64,
                         true) == (uint64_t)-65536);
    CHECK(arith_mul_high(0xffffffff, 2, 32, false) == 1);
    CHECK(arith_mul_high((uint64_t)-2, 3, 32, true) == (uint64_t)-1);
    CHECK(arith_mul_high(60000, 60000, 16, false) == 54931);
    CHECK(arith_mul_high((uint64_t)-300, 300, 16, true) == (uint64_t)-2);
    CHECK(arith_mul_high(200, 200, 8, false) == 156);
    CHECK(arith_mul_high((uint64_t)-128, 127, 8, true) == (uint64_t)-64);

    /* Saturation at the maximum and the minimum. */
    CHECK(arith_saturate('+', 200, 100, 8, false) == 255);
    CHECK(arith_saturate('-', 100, 200, 8, false) == 0);
    CHECK(arith_saturate('*', 20, 20, 8, false) == 255);
    CHECK(arith_saturate('*', 15, 17, 8, false) == 255);
    CHECK(arith_saturate('+', 100, 100, 8, true) == 127);
    CHECK(arith_saturate('+', (uint64_t)-100, (uint64_t)-100, 8, true) ==
          (uint64_t)-128);
    CHECK(arith_saturate('-', (uint64_t)-100, 100, 8, true) ==
          (uint64_t)-128);
    CHECK(arith_saturate('*', (uint64_t)-100, (uint64_t)-100, 8, true) ==
          127);
    CHECK(arith_saturate('*', 3, (uint64_t)-4, 8, true) == (uint64_t)-12);
    CHECK(arith_saturate('+', UINT64_MAX, 1, 64, false) == UINT64_MAX);
    CHECK(arith_saturate('-', 0, 1, 64, false) == 0);
    CHECK(arith_saturate('*', (uint64_t)1 << 32, (uint64_t)1 << 32, 64,
                         false) == UINT64_MAX);
    CHECK(arith_saturate('+', INT64_MAX, 1, 64, true) == INT64_MAX);
    CHECK(arith_saturate('-', (uint64_t)INT64_MIN, 1, 64, true) ==
          (uint64_t)INT64_MIN);
    CHECK(arith_saturate('*', (uint64_t)INT64_MIN, (uint64_t)-1, 64, true) ==
          INT64_MAX);
    CHECK(arith_saturate('*', (uint64_t)1 << 32, (uint64_t)-((int64_t)1 << 32),
                         64, true) == (uint64_t)INT64_MIN);
    CHECK(arith_saturate('*', (uint64_t)-1, (uint64_t)-1, 64, true) == 1);
    CHECK(arith_saturate('+', 20, 30, 32, false) == 50);
}

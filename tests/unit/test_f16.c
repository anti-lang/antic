/* The conversions between f16 and f32 that the runtime routines make at
   x86-64-v1 and v2, and that antic uses to fold a constant. Every half
   is checked against its value computed with ldexp, and every rounding
   boundary between two halves against the neighbour it must reach. */

#include "../binary_stdio.h"
#include "check.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "f16.h"

static uint32_t bits_of(float f)
{
    uint32_t b;
    memcpy(&b, &f, sizeof b);
    return b;
}

static float float_of(uint32_t b)
{
    float f;
    memcpy(&f, &b, sizeof f);
    return f;
}

/* The value of a finite half, from its fields and nothing else. */
static double value_of(uint16_t h)
{
    int exponent = (h >> 10) & 0x1F;
    int mantissa = h & 0x3FF;
    double v = exponent == 0 ? ldexp(mantissa, -24)
                             : ldexp(1024 + mantissa, exponent - 25);
    return (h & 0x8000) != 0 ? -v : v;
}

/* Every half widens to the f32 of its value. A NaN keeps its sign and
   payload and becomes quiet, as fcvt and vcvtph2ps make it. */
static void widens(void)
{
    uint32_t h;
    int wrong = 0;

    for (h = 0; h <= 0xFFFF; h++) {
        float f = anti_rt_f16_to_f32(h);
        int exponent = (h >> 10) & 0x1F;
        uint32_t mantissa = h & 0x3FF;
        uint32_t sign = (h & 0x8000) << 16;
        if (exponent == 0x1F) {
            uint32_t expected = sign | 0x7F800000 | (mantissa << 13) |
                                (mantissa != 0 ? 0x400000 : 0);
            wrong += bits_of(f) != expected;
        } else {
            wrong += (double)f != value_of((uint16_t)h) ||
                     (bits_of(f) & 0x80000000) != sign;
        }
    }
    CHECK(wrong == 0);
    /* The high half of the argument is not read. */
    CHECK(anti_rt_f16_to_f32(0xABCD3C00) == 1.0f);
}

/* Every f32 between two halves goes to the nearer one, and a value
   halfway goes to the one with an even mantissa. */
static void rounds_to_nearest_even(void)
{
    uint32_t h;
    int wrong = 0;

    for (h = 0; h < 0x7C00; h++) {
        uint32_t sign;
        for (sign = 0; sign <= 0x8000; sign += 0x8000) {
            float here = (float)value_of((uint16_t)(h | sign));
            float next = h + 1 == 0x7C00 ? (sign ? -65536.0f : 65536.0f)
                                          : (float)value_of((uint16_t)((h + 1) | sign));
            float middle = (here + next) / 2;
            float toward = sign ? -INFINITY : INFINITY;
            uint32_t even = (h & 1) == 0 ? h : h + 1;
            wrong += anti_rt_f32_to_f16(here) != (h | sign);
            wrong += anti_rt_f32_to_f16(middle) != (even | sign);
            wrong += anti_rt_f32_to_f16(nextafterf(middle, 0.0f)) != (h | sign);
            wrong += anti_rt_f32_to_f16(nextafterf(middle, toward)) !=
                     ((h + 1) | sign);
        }
    }
    CHECK(wrong == 0);
}

/* Past the largest half lies infinity, and below half the smallest
   subnormal lies zero, each keeping its sign. */
static void edges(void)
{
    CHECK(anti_rt_f32_to_f16(65504.0f) == 0x7BFF);
    CHECK(anti_rt_f32_to_f16(65519.99609375f) == 0x7BFF);
    CHECK(anti_rt_f32_to_f16(65520.0f) == 0x7C00);
    CHECK(anti_rt_f32_to_f16(1.0e10f) == 0x7C00);
    CHECK(anti_rt_f32_to_f16(-3.4028234663852886e38f) == 0xFC00);
    CHECK(anti_rt_f32_to_f16(INFINITY) == 0x7C00);
    CHECK(anti_rt_f32_to_f16(-INFINITY) == 0xFC00);
    CHECK(anti_rt_f32_to_f16(0.0f) == 0x0000);
    CHECK(anti_rt_f32_to_f16(-0.0f) == 0x8000);
    CHECK(anti_rt_f32_to_f16(ldexpf(1.0f, -25)) == 0x0000);
    CHECK(anti_rt_f32_to_f16(nextafterf(ldexpf(1.0f, -25), 1.0f)) == 0x0001);
    CHECK(anti_rt_f32_to_f16(ldexpf(1.0f, -26)) == 0x0000);
    CHECK(anti_rt_f32_to_f16(-ldexpf(1.0f, -26)) == 0x8000);
    CHECK(anti_rt_f32_to_f16(float_of(0x00000001)) == 0x0000);
    /* The largest subnormal rounds up into the smallest normal. */
    CHECK(anti_rt_f32_to_f16(nextafterf(ldexpf(1.0f, -14), 0.0f)) == 0x0400);
}

/* A NaN narrows to a quiet NaN with the top of its payload. */
static void nans(void)
{
    CHECK(anti_rt_f32_to_f16(float_of(0x7FC00000)) == 0x7E00);
    CHECK(anti_rt_f32_to_f16(float_of(0x7FD00000)) == 0x7E80);
    CHECK(anti_rt_f32_to_f16(float_of(0xFFC00000)) == 0xFE00);
    CHECK(anti_rt_f32_to_f16(float_of(0x7F800001)) == 0x7E00);
    CHECK(anti_rt_f32_to_f16(float_of(0x7FBFE000)) == 0x7FFF);
}

/* A half that is no NaN comes back from f32 unchanged. */
static void round_trip(void)
{
    uint32_t h;
    int wrong = 0;

    for (h = 0; h <= 0xFFFF; h++) {
        uint32_t back = anti_rt_f32_to_f16(anti_rt_f16_to_f32(h));
        bool nan = (h & 0x7C00) == 0x7C00 && (h & 0x3FF) != 0;
        wrong += back != (nan ? (h | 0x200) : h);
    }
    CHECK(wrong == 0);
}

void test_f16(void)
{
    widens();
    rounds_to_nearest_even();
    edges();
    nans();
    round_trip();
}

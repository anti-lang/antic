/* C functions that take and return the vector types of C by value. The
   program test abi_simd calls them from Anti with simd structs of 16
   bytes, so that clang checks that antic passes those in vector
   registers. A simd struct of another size passes as the struct of its
   lanes, which the functions on F2 and F8 check. */
#include "../binary_stdio.h"
#include <stdint.h>
#include <string.h>

#if defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>
typedef float32x4_t f4;
typedef int32x4_t i4;
typedef float64x2_t d2;
#else
#include <immintrin.h>
typedef __m128 f4;
typedef __m128i i4;
typedef __m128d d2;
#endif

typedef struct { _Alignas(8) float x; float y; } F2;
typedef struct { _Alignas(16) float v[8]; } F8;

f4 abi_simd_mix(f4 a, float k, f4 b)
{
    float x[4], y[4];
    int i;
    memcpy(x, &a, sizeof x);
    memcpy(y, &b, sizeof y);
    for (i = 0; i < 4; i++) {
        x[i] = x[i] * k + y[i];
    }
    memcpy(&a, x, sizeof x);
    return a;
}

i4 abi_simd_ints(int64_t n, i4 a, i4 b)
{
    int32_t x[4], y[4];
    int i;
    memcpy(x, &a, sizeof x);
    memcpy(y, &b, sizeof y);
    for (i = 0; i < 4; i++) {
        x[i] = x[i] + y[i] + (int32_t)n;
    }
    memcpy(&a, x, sizeof x);
    return a;
}

d2 abi_simd_doubles(d2 a, double s)
{
    double x[2];
    memcpy(x, &a, sizeof x);
    x[0] *= s;
    x[1] *= s;
    memcpy(&a, x, sizeof x);
    return a;
}

/* Nine vectors, so that the last one goes to the stack. */
f4 abi_simd_nine(f4 a, f4 b, f4 c, f4 d, f4 e, f4 f, f4 g, f4 h, f4 last)
{
    f4 all[9] = {a, b, c, d, e, f, g, h, last};
    float sum[4] = {0, 0, 0, 0};
    float x[4];
    int i;
    int k;
    for (k = 0; k < 9; k++) {
        memcpy(x, &all[k], sizeof x);
        for (i = 0; i < 4; i++) {
            sum[i] += x[i] * (float)(k + 1);
        }
    }
    memcpy(&a, sum, sizeof sum);
    return a;
}

/* An Anti function takes and returns the vectors of C. */
f4 abi_simd_apply(f4 (*fn)(f4, f4), f4 a, f4 b)
{
    return fn(fn(a, b), b);
}

F2 abi_simd_pair(F2 a, F2 b)
{
    F2 r = {a.x * b.x, a.y + b.y};
    return r;
}

F8 abi_simd_wide(F8 a)
{
    F8 r;
    int i;
    for (i = 0; i < 8; i++) {
        r.v[i] = a.v[7 - i];
    }
    return r;
}

/* C functions that take, return and call back structs of three and four
   vector types of C. The program test abi_vector_fields_wide calls them
   from Anti with structs of simd structs of 16 bytes. ARM64 passes such a
   struct in three or four vector registers, as a homogeneous vector
   aggregate, and x86_64 passes it in memory, so clang checks both rules
   in antic. */
#include "../binary_stdio.h"
#include <string.h>

#if defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>
typedef float32x4_t f4;
#else
#include <immintrin.h>
typedef __m128 f4;
#endif

typedef struct { f4 a; f4 b; f4 c; } Three;
typedef struct { f4 a; f4 b; f4 c; f4 d; } Four;

static f4 lanes(float a, float b, float c, float d)
{
    float x[4] = {a, b, c, d};
    f4 v;
    memcpy(&v, x, sizeof x);
    return v;
}

/* The lanes of v, each times its place counted from first, added. */
static float weighted(f4 v, float first)
{
    float x[4];
    float sum = 0;
    int i;
    memcpy(x, &v, sizeof x);
    for (i = 0; i < 4; i++) {
        sum += x[i] * (first + (float)i);
    }
    return sum;
}

float vector_fields_wide_three(Three p)
{
    return weighted(p.a, 1) + weighted(p.b, 5) + weighted(p.c, 9);
}

float vector_fields_wide_four(Four p)
{
    return weighted(p.a, 1) + weighted(p.b, 5) + weighted(p.c, 9) +
           weighted(p.d, 13);
}

Four vector_fields_wide_make(float x)
{
    Four r;
    r.a = lanes(x, x + 1, x + 2, x + 3);
    r.b = lanes(10 * x, 20 * x, 30 * x, 40 * x);
    r.c = lanes(-x, -2 * x, -3 * x, -4 * x);
    r.d = lanes(100 * x, 200 * x, 300 * x, 400 * x);
    return r;
}

/* An Anti function takes a struct of four vectors and returns one of
   three. */
float vector_fields_wide_apply(Three (*fn)(Four), float x)
{
    Three t = fn(vector_fields_wide_make(x));
    return weighted(t.a, 1) + weighted(t.b, 5) + weighted(t.c, 9);
}

/* C functions that take, return and call back structs whose fields are
   the vector types of C. The program test abi_vector_fields calls them
   from Anti with structs of simd structs of 16 bytes. ARM64 passes such a
   struct of one or two fields as a homogeneous vector aggregate, in vector
   registers, and x86_64 passes one of one field in one vector register,
   so clang checks both rules in antic. */
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

typedef struct { f4 v; } One;
typedef struct { i4 v; } Ints;
typedef struct { d2 v; } Doubles;
typedef struct { f4 a; f4 b; } Two;

static f4 lanes(float a, float b, float c, float d)
{
    float x[4] = {a, b, c, d};
    f4 v;
    memcpy(&v, x, sizeof x);
    return v;
}

/* The lanes of v, each times its place counted from 1, added. */
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

float vector_fields_one(One p)
{
    return weighted(p.v, 1);
}

Ints vector_fields_ints(int64_t n, Ints p)
{
    int32_t x[4];
    int i;
    memcpy(x, &p.v, sizeof x);
    for (i = 0; i < 4; i++) {
        x[i] += (int32_t)n;
    }
    memcpy(&p.v, x, sizeof x);
    return p;
}

Doubles vector_fields_doubles(Doubles p, double s)
{
    double x[2];
    memcpy(x, &p.v, sizeof x);
    x[0] *= s;
    x[1] *= s;
    memcpy(&p.v, x, sizeof x);
    return p;
}

float vector_fields_two(Two p)
{
    return weighted(p.a, 1) + weighted(p.b, 5);
}

One vector_fields_make(float x)
{
    One r;
    r.v = lanes(x, x + 1, x + 2, x + 3);
    return r;
}

Two vector_fields_make_two(float x)
{
    Two r;
    r.a = lanes(x, x + 1, x + 2, x + 3);
    r.b = lanes(10 * x, 20 * x, 30 * x, 40 * x);
    return r;
}

/* An Anti function takes a struct of two vectors and returns one of one. */
float vector_fields_apply(One (*fn)(Two), float x)
{
    Two t;
    t.a = lanes(x, x + 1, x + 2, x + 3);
    t.b = lanes(10 * x, 15 * x, 20 * x, 25 * x);
    return weighted(fn(t).v, 1);
}

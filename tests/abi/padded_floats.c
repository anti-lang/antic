/* C functions that take and return structs of floats whose _Alignas
   leaves bytes after the fields. The program test abi_padded_floats calls
   them from Anti with the structs that align(N) gives the same layout.
   Such a struct is no homogeneous float aggregate of ARM64, which passes
   one of 16 bytes as two integer registers and a larger one as a pointer
   to a copy, so clang checks the rule in antic. */
#include "../binary_stdio.h"
#include <stdint.h>

typedef struct { _Alignas(16) float a; float b; } A16;
typedef struct { _Alignas(16) double a; } D16;
typedef struct { _Alignas(16) float a; float b; float c; } T16;
typedef struct { _Alignas(32) float a; float b; } A32;

float padded_floats_a16(A16 p)
{
    return p.a + 10 * p.b;
}

double padded_floats_d16(int64_t n, D16 p)
{
    return p.a * 2 + (double)n;
}

float padded_floats_t16(T16 p)
{
    return p.a + 10 * p.b + 100 * p.c;
}

float padded_floats_a32(A32 p)
{
    return p.a + 10 * p.b;
}

A16 padded_floats_make(float x)
{
    A16 r = {x, 2 * x};
    return r;
}

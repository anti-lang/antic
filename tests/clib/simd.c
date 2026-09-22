/* A C program that calls a library whose signatures name simd structs.
   A simd struct of 16 bytes is the vector type of C, which C builds
   with the intrinsics of its architecture. One of another size is the
   struct of its lanes. */
#include "../binary_stdio.h"
#include <stdio.h>
#include <string.h>

#include "simdlib.h"

static Vec4 vector_of(float a, float b, float c, float d)
{
    float lanes[4];
    Vec4 v;

    lanes[0] = a;
    lanes[1] = b;
    lanes[2] = c;
    lanes[3] = d;
    memcpy(&v, lanes, sizeof lanes);
    return v;
}

int main(void)
{
    Vec4 a = vector_of(1.0f, 2.0f, 3.0f, 4.0f);
    Vec4 b = vector_of(0.5f, 8.0f, 0.25f, 16.0f);
    Vec4 mixed = simd_mix(a, 2.0f, b);
    Vec4 greater = simd_greater(a, b);
    int32_t lanes[4] = {1, -2, 3, -4};
    I32x4 ints;
    F2 pair;
    float out[4];

    memcpy(&ints, lanes, sizeof lanes);
    ints = simd_twice(ints);
    memcpy(lanes, &ints, sizeof lanes);
    memcpy(out, &mixed, sizeof out);
    printf("%g %g %g %g\n", (double)out[0], (double)out[1], (double)out[2],
           (double)out[3]);
    memcpy(out, &greater, sizeof out);
    printf("%g %g %g %g\n", (double)out[0], (double)out[1], (double)out[2],
           (double)out[3]);
    printf("%g\n", (double)simd_total(a));
    printf("%d %d %d %d\n", (int)lanes[0], (int)lanes[1], (int)lanes[2],
           (int)lanes[3]);
    pair.x = 3.0f;
    pair.y = 4.0f;
    printf("%g\n", (double)simd_pair(pair));
    return 0;
}

/* A simd loop, the C twin of simd_loop.anti. The dot product of two
   buffers of 4096 floats runs 200000 times in vectors of four lanes, and
   one element changes between the passes. The product and the sum stand
   in two statements, so clang contracts them into no fused multiply-add,
   as the Anti program has none. The lanes add in the order of `sum`: the
   upper half onto the lower half. */

#include "../binary_stdio.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef float f32x4 __attribute__((vector_size(16)));

static f32x4 load(const float *s, int64_t i)
{
    f32x4 v;
    memcpy(&v, s + i, sizeof v);
    return v;
}

static float dot(const float *a, const float *b, int64_t n)
{
    f32x4 acc = {0.0f, 0.0f, 0.0f, 0.0f};
    for (int64_t i = 0; i < n; i += 4) {
        f32x4 p = load(a, i) * load(b, i);
        acc = acc + p;
    }
    float lanes[4];
    memcpy(lanes, &acc, sizeof lanes);
    return (lanes[0] + lanes[2]) + (lanes[1] + lanes[3]);
}

int main(void)
{
    int64_t n = 4096;
    float *a = malloc((size_t)n * sizeof *a);
    float *b = malloc((size_t)n * sizeof *b);
    if (a == NULL || b == NULL) {
        return 1;
    }
    for (int64_t i = 0; i < n; i++) {
        a[i] = (float)(i % 17) * 0.0625f;
        b[i] = (float)(i % 13) * 0.125f;
    }
    double total = 0.0;
    for (int64_t r = 0; r < 200000; r++) {
        total = total + (double)dot(a, b, n);
        int64_t at = r & 4095;
        a[at] = a[at] + 0.5f;
    }
    printf("%.1f\n", total);
    free(a);
    free(b);
    return 0;
}

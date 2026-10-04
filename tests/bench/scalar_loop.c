/* A scalar loop over a slice, the C twin of scalar_loop.anti. One buffer
   of a million integers is summed 1000 times, each pass with its own
   factor. The sums wrap, as they do in Anti, through uint64_t. */

#include "../binary_stdio.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void fill(int64_t *s, int64_t n)
{
    uint64_t x = 88172645463325252u;
    for (int64_t i = 0; i < n; i++) {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        s[i] = (int64_t)(x >> 40);
    }
}

static uint64_t pass(const int64_t *s, int64_t n, uint64_t k)
{
    uint64_t total = 0;
    for (int64_t i = 0; i < n; i++) {
        total += (uint64_t)s[i] * k + (uint64_t)(s[i] >> 3);
    }
    return total;
}

int main(void)
{
    int64_t n = 1048576;
    int64_t *s = malloc((size_t)n * sizeof *s);
    if (s == NULL) {
        return 1;
    }
    fill(s, n);
    uint64_t total = 0;
    for (uint64_t r = 0; r < 1000; r++) {
        total += pass(s, n, r);
    }
    printf("%lld\n", (long long)total);
    free(s);
    return 0;
}

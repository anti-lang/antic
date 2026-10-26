/* The hooks of an object and the trace points of a function, the C twin
   of hooks.anti. Each of 20 million turns allocates a node, calls `step`
   on it and frees it. The pointer goes through a volatile global, so the
   compiler keeps the allocation as the Anti program keeps it. C has no
   hook and no trace point, so the twin is the turn without them. */

#include "../binary_stdio.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

struct node {
    const void *table;
    int64_t v;
};

static struct node *volatile kept;

static int64_t step(struct node *n, int64_t k)
{
    n->v = (n->v + k) & 1023;
    return n->v;
}

int main(void)
{
    uint64_t total = 0;

    for (int64_t i = 0; i < 20000000; i++) {
        struct node *n = malloc(sizeof *n);
        if (n == NULL) {
            return 1;
        }
        n->table = NULL;
        n->v = i & 255;
        kept = n;
        total += (uint64_t)step(kept, i & 7);
        free(n);
    }
    printf("%lld\n", (long long)(int64_t)total);
    return 0;
}

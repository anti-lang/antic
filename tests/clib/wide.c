/* A C program that compares the size and the alignment of the struct the
   header writes for a class with `align(32)` with those Anti gives the
   class, and calls Anti with a function that C stores in a field of it. */
#include "../binary_stdio.h"
#include <stdalign.h>
#include <stdio.h>

#include "wide.h"

static int64_t twice(int64_t n)
{
    return 2 * n;
}

int main(void)
{
    Wide w;

    anti_Wide_init(&w);
    w.n = 21;
    w.step = twice;
    printf("size %d\n", (int)(sizeof(Wide) == (size_t)wide_size()));
    printf("align %d\n", (int)(alignof(Wide) == 32));
    printf("step %d\n", (int)wide_step(&w));
    anti_Wide_destroy(&w);
    return 0;
}

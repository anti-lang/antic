/* A C program that compares the size and the alignment of the struct the
   header writes for a packed struct with `align(16)` with those Anti gives
   it. */
#include "../binary_stdio.h"
#include <stdalign.h>
#include <stdio.h>

#include "tight.h"

int main(void)
{
    printf("size %d\n", (int)(sizeof(Lined) == (size_t)lined_size()));
    printf("align %d\n", (int)(alignof(Lined) == 16));
    return 0;
}

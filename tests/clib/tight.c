/* A C program that compares the size of the struct the header writes for a
   packed class with the size Anti gives it. */
#include "../binary_stdio.h"
#include <stdio.h>

#include "tight.h"

int main(void)
{
    printf("size %d\n", (int)(sizeof(Snug) == (size_t)snug_size()));
    return 0;
}

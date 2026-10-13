/* A C program that calls a function of a library that never returns. The
   header declares it so, and the program ends inside the call. */
#include "../binary_stdio.h"
#include <stdio.h>

#include "never.h"

int main(void)
{
    printf("before\n");
    fflush(stdout);
    leave(0);
}

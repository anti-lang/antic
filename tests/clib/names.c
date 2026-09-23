/* A C program that reads the header of com.example.names. The fields of
   Pair keep one name each, and the float constants divide as floats. A
   wrapper reaches the last of 70 functions of Many through its table. */
#include "../binary_stdio.h"
#include <stdio.h>
#include <stdlib.h>

#include "names.h"

int main(void)
{
    Pair p;
    Many *m = (Many *)malloc(sizeof *m);

    p.sharedqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqa = 1;
    p.sharedqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqb = 2;
    printf("%d %d\n", p.sharedqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqa, p.sharedqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqb);
    printf("%g %g %d\n", 1 / TWO_F32, 1 / TWO_F64,
           (int)sizeof(TWO_F32));
    anti_Many_init(m);
    printf("%d %d\n", anti_Many_step0(m), anti_Many_step69(m));
    anti_Many_delete(m);
    return 0;
}

/* A C program that calls a library whose signatures name tuples. Each
   tuple is a struct of the header, so C builds one and reads the
   elements of one it is given. */
#include "../binary_stdio.h"
#include <stdio.h>

#include "tuples.h"

int main(void)
{
    struct anti_tuple_int_int d = tuples_divmod(17, 5);
    struct anti_tuple_int_int p = {3, 4};
    struct anti_tuple_int_f32 s = tuples_scaled(2, 0.5f);

    printf("%d %d\n", (int)d._0, (int)d._1);
    printf("%d\n", (int)tuples_sum(p));
    printf("%d %.1f\n", (int)s._0, (double)s._1);
    return 0;
}

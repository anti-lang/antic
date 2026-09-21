/* A C program that calls a library whose signatures name tuples. Each
   tuple is a struct of the header, so C builds one and reads the
   elements of one it is given. A function that may fail writes its
   tuple through the one out pointer. */
#include "../binary_stdio.h"
#include <stdio.h>

#include "tuples.h"

int main(void)
{
    struct anti_tuple_int_int d = tuples_divmod(17, 5);
    struct anti_tuple_int_int p = {3, 4};
    struct anti_tuple_int_f32 s = tuples_scaled(2, 0.5f);
    struct anti_tuple_int_int q = {0, 0};
    struct anti_Error *e;

    printf("%d %d\n", (int)d._0, (int)d._1);
    printf("%d\n", (int)tuples_sum(p));
    printf("%d %.1f\n", (int)s._0, (double)s._1);
    e = tuples_divide(23, 4, &q);
    printf("%d %d %d\n", e == NULL, (int)q._0, (int)q._1);
    e = tuples_divide(1, 0, &q);
    printf("%d %d %d\n", e == NULL, (int)q._0, (int)q._1);
    return 0;
}

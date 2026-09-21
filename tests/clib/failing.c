/* A C program that calls the two functions of the library failing that
   may fail, and the `construct` of its class. It takes the path that
   succeeds and the path that reports an error. The error is a pointer C
   passes on and never reads. */
#include "../binary_stdio.h"
#include <stdio.h>

#include "failing.h"

int main(void)
{
    Counter c = {1};
    Gauge g;
    int32_t half = 0;
    struct anti_Error *e;

    e = failing_half(8, &half);
    printf("%d %d\n", e == NULL, half);
    e = failing_half(7, &half);
    printf("%d\n", e == NULL);
    e = failing_step(&c, 3);
    printf("%d %d\n", e == NULL, c.n);
    e = failing_step(&c, 2);
    printf("%d %d\n", e == NULL, c.n);
    printf("%d\n", failing_twice(21));
    /* The helper writes the tables and the defaults, then runs
       `construct` with the arguments. */
    e = anti_Gauge_construct(&g, 5);
    printf("%d %d %d\n", e == NULL, g.limit, g.level);
    e = anti_Gauge_construct(&g, 0);
    printf("%d\n", e == NULL);
    return 0;
}

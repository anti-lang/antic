/* A C program that builds an object of the library created with
   anti_Meter_construct, whose class replaces `created`. The hook runs
   once `construct` succeeded, as it does for `Meter(limit)` in Anti, and
   not at all when `construct` fails. */
#include "../binary_stdio.h"
#include <stdio.h>

#include "created.h"

int main(void)
{
    Meter m;
    struct anti_Error *e;

    e = anti_Meter_construct(&m, 5);
    printf("%d %d %d %d\n", e == NULL, m.limit, m.seen, meter_seen(5));
    e = anti_Meter_construct(&m, 0);
    printf("%d %d %d\n", e == NULL, m.seen, meter_seen(0));
    return 0;
}

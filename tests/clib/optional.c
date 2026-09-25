/* A C program that calls a library whose signatures name optional values.
   Each `?T` is a struct of the header with the value and a flag. C reads
   the flag of one it is given and builds one of its own. */
#include "../binary_stdio.h"
#include <stdbool.h>
#include <stdio.h>

#include "optional.h"

int main(void)
{
    struct anti_opt_int four = optional_even(4, 10);
    struct anti_opt_int five = optional_even(5, 10);
    struct anti_opt_int given = {7, true};
    struct anti_opt_int missing = {0, false};
    struct anti_opt_Spot spot = optional_spot(3);
    struct anti_opt_Spot none = optional_spot(-1);
    struct anti_opt_Spot built = {{2, 5}, true};

    printf("%d %d\n", four.has, (int)four.value);
    printf("%d\n", five.has);
    printf("%d %d\n", (int)optional_or(given, 1), (int)optional_or(missing, 1));
    printf("%d %d %d\n", spot.has, (int)spot.value.x, (int)spot.value.y);
    printf("%d\n", none.has);
    printf("%d %d\n", (int)optional_sum(built), (int)optional_sum(none));
    return 0;
}

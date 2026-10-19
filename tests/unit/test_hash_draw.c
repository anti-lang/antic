/* The number a HashMap and a HashSet draw for the order of their walks,
   and the seed of the hashes, when the system gives no random bytes. The
   minor finding at map.anti:475 of the third audit found the walk order
   at one fixed value then. The number then comes from the two clocks, so
   two draws a moment apart differ.

   hash.c is compiled for this program with anti_rt_entropy named
   refused_entropy, which asks for nothing of the system and gives no
   bytes. */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../binary_stdio.h"
#include "check.h"
#include "hash.h"
#include "platform.h"

int check_failures;

int refused_entropy(void *out, size_t count);

static int asked;

int refused_entropy(void *out, size_t count)
{
    asked++;
    memset(out, 0, count);
    return -1;
}

int main(void)
{
    uint64_t first = anti_rt_hash_draw();
    uint64_t second;

    anti_rt_sleep(2000000);
    second = anti_rt_hash_draw();
    CHECK(asked == 2);
    CHECK(first != 0);
    CHECK(second != 0);
    CHECK(first != second);
    if (check_failures != 0) {
        fprintf(stderr, "%d check(s) failed\n", check_failures);
        return 1;
    }
    return 0;
}

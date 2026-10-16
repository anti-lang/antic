/* The reader of a pattern literal of src/antic/pattern.c: the fewest bytes
   a match or a group can take, which the fit rule of `patch` reads. */
#include <stdint.h>
#include <string.h>

#include "../binary_stdio.h"
#include "check.h"
#include "pattern.h"

static int64_t least_of(const char *pattern, int64_t group)
{
    return pattern_least_bytes(pattern, strlen(pattern), group);
}

/* The count of a sequence stays at the cap of 2^30 however many of its
   parts reach the cap. Two parts at the cap summed to 2^31 in a `long`,
   which overflows where a `long` is 32 bits, as on both Windows targets,
   by S303 of docs/audit/summary.md. PCRE2 refuses these patterns as too
   large, and the reader reads any pattern. */
static void least_cap(void)
{
    const int64_t cap = INT64_C(1) << 30;

    CHECK(least_of("(?:a{65535}){65535}", 0) == cap);
    CHECK(least_of("(?:a{65535}){65535}(?:b{65535}){65535}", 0) == cap);
    CHECK(least_of("((?:a{65535}){65535})((?:b{65535}){65535})c", 0) ==
          cap);
    CHECK(least_of("((?:a{65535}){65535})((?:b{65535}){65535})c", 2) ==
          cap);
    CHECK(least_of("(?:(?:a{65535}){65535}){65535}", 0) == cap);
}

/* Counts below the cap add as they are written. */
static void least_counts(void)
{
    CHECK(least_of("a{3}b", 0) == 4);
    CHECK(least_of("(a|bc)d{2,5}", 0) == 3);
    CHECK(least_of("(a|bc)d{2,5}", 1) == 1);
    CHECK(least_of("x*y?", 0) == 0);
    CHECK(least_of("(?:ab){65535}", 0) == 131070);
}

void test_pattern(void)
{
    least_cap();
    least_counts();
}

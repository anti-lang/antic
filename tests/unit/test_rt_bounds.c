/* Integer bounds of the runtime: the size of a text builder and atomic
   subtraction at the minimum of each width. */
#include <stdint.h>
#include <stdlib.h>

#include "../binary_stdio.h"
#include "atomic.h"
#include "check.h"
#include "std.h"

/* A count that the size of the builder cannot hold is refused, and the
   builder keeps what it holds. */
static void builder_bounds(void)
{
    struct anti_builder b = {0};
    const unsigned char *four = (const unsigned char *)"four";

    anti_rt_builder_append(&b, four, 4);
    CHECK(b.length == 4);
    anti_rt_builder_fill(&b, 0, ' ', INT64_MAX);
    CHECK(b.length == 4);
    anti_rt_builder_fill(&b, 2, ' ', INT64_MAX - 4);
    CHECK(b.length == 4);
    /* The bytes fit and their NUL does not. */
    anti_rt_builder_fill(&b, 4, ' ', INT64_MAX - 4);
    CHECK(b.length == 4);
    anti_rt_builder_append(&b, four, INT64_MAX - 3);
    CHECK(b.length == 4);
    CHECK(b.room != NULL && memcmp(b.room, "four", 5) == 0);

    /* A count that fits still pads. */
    anti_rt_builder_fill(&b, 0, '-', 3);
    CHECK(b.length == 7 && memcmp(b.room, "---four", 8) == 0);
    free(b.room);
}

static void atomic_sub_minimum(void)
{
    int8_t i8 = 5;
    int16_t i16 = 5;
    int32_t i32 = 5;
    int64_t i64 = 5;

    CHECK(anti_rt_atomic_sub(&i8, 1, INT8_MIN) == 5);
    CHECK(i8 == (int8_t)(INT8_MIN + 5));
    CHECK(anti_rt_atomic_sub(&i16, 2, INT16_MIN) == 5);
    CHECK(i16 == (int16_t)(INT16_MIN + 5));
    CHECK(anti_rt_atomic_sub(&i32, 4, INT32_MIN) == 5);
    CHECK(i32 == INT32_MIN + 5);
    CHECK(anti_rt_atomic_sub(&i64, 8, INT64_MIN) == 5);
    CHECK(i64 == INT64_MIN + 5);

    /* An ordinary subtraction wraps as the other operations do. */
    i64 = INT64_MIN;
    CHECK(anti_rt_atomic_sub(&i64, 8, 1) == INT64_MIN);
    CHECK(i64 == INT64_MAX);
    i32 = 7;
    CHECK(anti_rt_atomic_sub(&i32, 4, 9) == 7);
    CHECK(i32 == -2);
}

void test_rt_bounds(void)
{
    builder_bounds();
    atomic_sub_minimum();
}

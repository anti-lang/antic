/* Integer bounds of the runtime: the growth of a block, the places of a
   text builder, atomic subtraction at the minimum of each width, the
   masks and the compare-and-swap of each width and the precision a text
   of the failure routine prints with. */

#include <stdint.h>
#include <stdlib.h>

#include "../binary_stdio.h"
#include "atomic.h"
#include "check.h"
#include "fail.h"
#include "grow.h"
#include "std.h"

/* A block doubles from its first size until it holds what is asked, and
   keeps its bytes as it grows. A size that no int64_t holds is refused,
   and the block and its room stay as they were. */
static void block_growth(void)
{
    int64_t room = 0;
    unsigned char *block = anti_rt_reserve(NULL, &room, 0, 10, 64);
    unsigned char *same;

    CHECK(block != NULL && room == 64);
    memcpy(block, "kept", 4);
    same = anti_rt_reserve(block, &room, 4, 60, 64);
    CHECK(same == block && room == 64);
    block = anti_rt_reserve(block, &room, 64, 1, 64);
    CHECK(block != NULL && room == 128 && memcmp(block, "kept", 4) == 0);
    block = anti_rt_reserve(block, &room, 128, 300, 64);
    CHECK(block != NULL && room == 512 && memcmp(block, "kept", 4) == 0);
    same = anti_rt_reserve(block, &room, 1, INT64_MAX, 64);
    CHECK(same == NULL && room == 512);
    same = anti_rt_reserve(block, &room, INT64_MAX, 1, 64);
    CHECK(same == NULL && room == 512);
    CHECK(memcmp(block, "kept", 4) == 0);
    free(block);
}

/* A count that the size of the builder cannot hold ends the program as
   memory that runs out does, which tests/runners/run_rt_oom.cmake checks. A pad
   before the bytes moves them and keeps their NUL. */
static void builder_bounds(void)
{
    struct anti_builder b = {0};
    const unsigned char *four = (const unsigned char *)"four";

    anti_rt_builder_append(&b, four, 4);
    CHECK(b.length == 4);
    CHECK(b.room != NULL && memcmp(b.room, "four", 5) == 0);
    anti_rt_builder_fill(&b, 0, '-', 3);
    CHECK(b.length == 7 && memcmp(b.room, "---four", 8) == 0);
    /* A place past the bytes writes nothing. */
    anti_rt_builder_fill(&b, 8, '-', 3);
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

/* `and` and `or` give the value before and keep the bits of the width
   alone, and a compare-and-swap of one byte swaps on the expected value
   and on it alone. C calls them, and an Anti program writes the LLVM
   instruction instead. */
static void atomic_masks(void)
{
    int8_t i8 = 0x0F;
    int16_t i16 = 0x0F0F;
    int32_t i32 = 0x0F0F0F0F;
    int64_t i64 = 0x0F0F0F0F0F0F0F0F;

    CHECK(anti_rt_atomic_and(&i8, 1, 0x3C) == 0x0F && i8 == 0x0C);
    CHECK(anti_rt_atomic_or(&i8, 1, 0x30) == 0x0C && i8 == 0x3C);
    CHECK(anti_rt_atomic_and(&i16, 2, 0x3C3C) == 0x0F0F && i16 == 0x0C0C);
    CHECK(anti_rt_atomic_or(&i16, 2, 0x3030) == 0x0C0C && i16 == 0x3C3C);
    CHECK(anti_rt_atomic_and(&i32, 4, 0x3C3C3C3C) == 0x0F0F0F0F &&
          i32 == 0x0C0C0C0C);
    CHECK(anti_rt_atomic_or(&i32, 4, 0x30303030) == 0x0C0C0C0C &&
          i32 == 0x3C3C3C3C);
    CHECK(anti_rt_atomic_and(&i64, 8, 0x3C3C3C3C3C3C3C3C) ==
              0x0F0F0F0F0F0F0F0F &&
          i64 == 0x0C0C0C0C0C0C0C0C);
    CHECK(anti_rt_atomic_or(&i64, 8, 0x3030303030303030) ==
              0x0C0C0C0C0C0C0C0C &&
          i64 == 0x3C3C3C3C3C3C3C3C);
    i8 = 7;
    CHECK(anti_rt_atomic_compare_swap(&i8, 1, 6, 9) == 0 && i8 == 7);
    CHECK(anti_rt_atomic_compare_swap(&i8, 1, 7, 9) == 1 && i8 == 9);
}

/* The precision of `%.*s` for a text of an Anti length: the length, or
   INT_MAX for one that no int holds, which once kept the low 32 bits
   alone. */
static void print_width(void)
{
    CHECK(anti_rt_print_width(0) == 0);
    CHECK(anti_rt_print_width(5) == 5);
    CHECK(anti_rt_print_width(INT32_MAX) == INT32_MAX);
    CHECK(anti_rt_print_width((int64_t)INT32_MAX + 1) == INT32_MAX);
    CHECK(anti_rt_print_width(((int64_t)1 << 32) + 3) == INT32_MAX);
    CHECK(anti_rt_print_width(INT64_MAX) == INT32_MAX);
    CHECK(anti_rt_print_width(-1) == 0);
}

void test_rt_bounds(void)
{
    block_growth();
    builder_bounds();
    atomic_sub_minimum();
    atomic_masks();
    print_width();
}

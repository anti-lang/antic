/* A C program that reads the header of com.example.names. The fields of
   Pair keep one name each, and the float constants divide as floats. A
   wrapper reaches the last of 70 functions of Many through its table.
   A wrapper of SubMaker reads the slot after two statics of one name, the
   wrappers of Named keep apart from the helpers, two tuples that hold
   tuples or functions have a struct each, and the fields of Holder are
   declared before it. */
#include "../binary_stdio.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "names.h"

static int32_t add_one(int32_t x)
{
    return x + 1;
}

static int32_t add(int32_t x, int32_t y)
{
    return x + y;
}

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

    SubMaker *sub = (SubMaker *)malloc(sizeof *sub);
    anti_SubMaker_init(sub);
    printf("%d %d %d %d\n", Maker_make(), SubMaker_make(5),
           anti_SubMaker_first(sub), anti_SubMaker_second(sub));
    anti_SubMaker_delete(sub);

    Named *named = (Named *)malloc(sizeof *named);
    anti_Named_construct(named, 10);
    printf("%d %d %d %d %d %d\n", anti_Named_init_(named),
           anti_Named_vtable_(named), anti_Named_as_Asked_(named),
           anti_Named_Asked_vtable_(named), anti_Named_init__(named),
           anti_Asked_answer(anti_Named_as_Asked(named)));
    anti_Named_construct_(named, 20);
    printf("%d\n", Named_init(named));
    anti_Named_delete(named);

    Holder *holder = (Holder *)malloc(sizeof *holder);
    anti_Holder_init(holder);
    holder->pair._1 = 3;
    holder->maybe.value = 4;
    holder->maybe.has = true;
    holder->later.v = 5;
    struct anti_tuple_i32_i32_i32_i32 four = {1, 2, 3, 4};
    After after = {6};
    printf("%d %d %d %d %d\n", holder->pair._1, holder->maybe.value,
           holder->later.v, anti_Holder_sum(holder, four),
           anti_Holder_after(holder, after));
    anti_Holder_delete(holder);

    struct anti_tuple_tuple_i32_i32_end_i32_i32 two = {{1, 2}, 3, 4};
    struct anti_tuple_tuple_i32_i32_i32_end_i32 three = {{1, 2, 3}, 6};
    struct anti_tuple_fn_i32_to_i32_end_i32 one_fn = {add_one, 7};
    struct anti_tuple_fn_i32_i32_to_i32_end_i32 two_fn = {add, 8};
    struct anti_tuple_IntBox_i32 boxed = {{9}, 1};
    printf("%d %d %d %d %d\n", nest_two(two), nest_three(three),
           apply_one(one_fn), apply_two(two_fn), unbox(boxed));

    printf("%g %g %d %d %d\n", UP, (double)DOWN, isnan(NOT_NUMBER) != 0,
           LEAST == INT64_MIN, MOST == UINT64_MAX);
    return 0;
}

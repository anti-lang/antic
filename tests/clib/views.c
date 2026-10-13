/* A C program that reads the forms of a header that neither forms.c nor
   another program of this directory holds: an enum as a parameter and a
   result, an optional result, a struct and a union that name their own
   type, a field whose name is a macro of <stdint.h>, a vector of 8 and of
   32 bytes, a lent parameter, a function that may fail as a parameter,
   an interface reached through the base of a class, a protected field,
   a packed type of a class and a class under its lock. */
#include "../binary_stdio.h"
#include <stdio.h>
#include <string.h>

#include "views.h"

static int calls = 0;

static void count(void)
{
    calls += 1;
}

static struct anti_Error *twice(int32_t n, int32_t *result, void *context)
{
    (void)context;
    *result = 2 * n;
    return NULL;
}

int main(void)
{
    float pair[2] = {1.5f, 2.5f};
    float eight[8] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    F32x2 two;
    F32x8 wide;
    Limits limits;
    Link link;
    Either either;
    Box box;
    LongTag tag;
    Tally tally;
    struct anti_opt_i32 even = half(10);
    struct anti_opt_i32 odd = half(7);

    printf("%d %d\n", (int)louder(Level_Quiet), (int)Level_Loud);
    printf("%d %d %d\n", (int)even.has, (int)even.value, (int)odd.has);
    memcpy(&two, pair, sizeof pair);
    memcpy(&wide, eight, sizeof eight);
    printf("%d %d\n", (int)first(two), (int)wide_sum(wide));
    limits.SIZE_MAX_ = 40;
    limits.UINT8_MAX_ = 2;
    printf("%d\n", (int)limit_of(limits));
    link.value = 9;
    link.next = &link;
    link.call = count;
    link.next->call();
    printf("%d %d\n", (int)value_of(&link), calls);
    either.whole = 11;
    printf("%d\n", (int)whole_of(either));
    printf("%d\n", (int)attempt(twice, NULL, 21));
    anti_Box_init(&box);
    printf("%d %d %d %d\n", (int)sizeof(Box_Header), (int)Box_size(&box),
           (int)box.depth, (int)box.width);
    anti_Box_destroy(&box);
    anti_LongTag_init(&tag);
    printf("%d %d %d\n", (int)anti_Named_length(anti_LongTag_as_Named(&tag)),
           (int)tag.extra, (int)LongTag_code_of(&tag, NULL));
    anti_LongTag_destroy(&tag);
    anti_Tally_init(&tally);
    Tally_add(&tally, 4);
    Tally_add(&tally, 5);
    printf("%d\n", (int)Tally_sum(&tally));
    anti_Tally_destroy(&tally);
    return 0;
}

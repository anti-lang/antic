/* A C program that reads the forms of a header that no other library of
   this directory holds: a constant of bool, a text with a control
   character, a vector type per kind of lane, a union, a tuple with
   pointers and an array, and a parameter that is a function with its
   context. */
#include "../binary_stdio.h"
#include <stdio.h>
#include <string.h>

#include "forms.h"

static int64_t add(int64_t x, void *context)
{
    const int64_t *offset = context;

    return x + *offset;
}

static void count(void *context)
{
    int *calls = context;

    *calls += 1;
}

int main(void)
{
    int8_t bytes[16] = {1};
    int16_t shorts[8] = {2};
    uint16_t words[8] = {3};
    uint32_t wides[4] = {4};
    int64_t longs[2] = {5};
    uint64_t larges[2] = {6};
    double reals[2] = {7.0};
    I8x16 a;
    I16x8 b;
    U16x8 c;
    U32x4 d;
    I64x2 e;
    U64x2 f;
    F64x2 g;
    Word word;
    int64_t n = 5;
    int64_t offset = 3;
    int calls = 0;
    struct anti_tuple_ptr_int_optr_int_a2_int t;

    printf("%d %d %d %d %d\n", (int)ENABLED, (int)DISABLED, (int)BELL[4],
           (int)BELL[8], (int)strlen(BELL));
    memcpy(&a, bytes, sizeof bytes);
    memcpy(&b, shorts, sizeof shorts);
    memcpy(&c, words, sizeof words);
    memcpy(&d, wides, sizeof wides);
    memcpy(&e, longs, sizeof longs);
    memcpy(&f, larges, sizeof larges);
    memcpy(&g, reals, sizeof reals);
    printf("%d\n", (int)sums(a, b, c, d, e, f, g));
    word.whole = 7;
    printf("%d\n", (int)whole(word));
    t = parts(&n, NULL);
    printf("%d %d %d %d\n", (int)*t._0, (int)(t._1 == NULL), (int)t._2[0],
           (int)t._2[1]);
    printf("%d\n", (int)apply(add, &offset, 4));
    run(count, &calls);
    run(count, &calls);
    printf("%d\n", calls);
    return 0;
}

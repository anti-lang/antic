/* A C program that passes callbacks to com.example.callbacks: one that
   takes a struct, a tuple and an enum, and one that takes a function of
   the library with its context. Each gets back the context C passed. */
#include "../binary_stdio.h"
#include <stdio.h>

#include "callbacks.h"

static int32_t weigh(Point p, struct anti_tuple_i32_i32 t, int32_t c,
                     void *context)
{
    const int32_t *green = context;
    return p.x * 1000 + p.y * 100 + t._0 * 10 + t._1 +
           (c == Color_Green ? *green : 0);
}

static int32_t twice(int32_t (*f)(int32_t, void *), void *f_context,
                     void *context)
{
    const int32_t *start = context;
    return f(f(*start, f_context), f_context);
}

int main(void)
{
    int32_t green = 5;
    int32_t start = 40;
    printf("apply %d\n", (int)apply(weigh, &green));
    printf("nest %d\n", (int)nest(twice, &start));
    return 0;
}

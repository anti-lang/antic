/* callbacks.h, the C interface of com.example.callbacks, written by antic.
   Do not edit. A failure that Anti cannot report calls abort(). */
#ifndef CALLBACKS_H
#define CALLBACKS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#define ANTI_ALIGNAS(n) alignas(n)
extern "C" {
#else
#define ANTI_ALIGNAS(n) _Alignas(n)
#endif

/** A point that the callback of `apply` takes. */
typedef struct Point {
    int32_t x;
    int32_t y;
} Point;

/** A color that the callback of `apply` takes. */
enum Color {
    Color_Red = 0,
    Color_Green = 1
};

/* The tuple (i32, i32). */
struct anti_tuple_i32_i32 {
    int32_t _0;
    int32_t _1;
};

/** Apply f to a point, a pair and a color. */
int32_t apply(int32_t (*f)(Point, struct anti_tuple_i32_i32, int32_t, void *), void *f_context);
/** Call g with a function of the library that adds one. */
int32_t nest(int32_t (*g)(int32_t (*)(int32_t, void *), void *, void *), void *g_context);

#ifdef __cplusplus
}
#endif

#endif

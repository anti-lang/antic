/* tuples.h, the C interface of com.example.tuples, written by antic.
   Do not edit. A failure that Anti cannot report calls abort(). */
#ifndef TUPLES_H
#define TUPLES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#define ANTI_ALIGNAS(n) alignas(n)
extern "C" {
#else
#define ANTI_ALIGNAS(n) _Alignas(n)
#endif

/* The tuple (int, int). */
struct anti_tuple_int_int {
    int64_t _0;
    int64_t _1;
};

/* The tuple (int, f32). */
struct anti_tuple_int_f32 {
    int64_t _0;
    float _1;
};

/** Both answers of a division. */
struct anti_tuple_int_int tuples_divmod(int64_t a, int64_t b);
/** The sum of the elements of a pair. */
int64_t tuples_sum(struct anti_tuple_int_int p);
/** A pair of two types. */
struct anti_tuple_int_f32 tuples_scaled(int64_t n, float k);

#ifdef __cplusplus
}
#endif

#endif

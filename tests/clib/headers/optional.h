/* optional.h, the C interface of com.example.optional, written by antic.
   Do not edit. A failure that Anti cannot report calls abort(). */
#ifndef OPTIONAL_H
#define OPTIONAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#define ANTI_ALIGNAS(n) alignas(n)
extern "C" {
#else
#define ANTI_ALIGNAS(n) _Alignas(n)
#endif

/** A point of the plane. */
typedef struct Spot {
    int32_t x;
    int32_t y;
} Spot;

/* The optional value ?int. */
struct anti_opt_int {
    int64_t value;
    bool has;
};

/* The optional value ?Spot. */
struct anti_opt_Spot {
    Spot value;
    bool has;
};

/** The index of `want` below `limit` when it is even, or `none`. */
struct anti_opt_int optional_even(int64_t want, int64_t limit);
/** The value `n` holds, or `fallback` where it holds none. */
int64_t optional_or(struct anti_opt_int n, int64_t fallback);
/** A spot on the diagonal, or `none` for a negative place. */
struct anti_opt_Spot optional_spot(int32_t at);
/** The sum of a spot's coordinates, or -1 where there is no spot. */
int32_t optional_sum(struct anti_opt_Spot s);

#ifdef __cplusplus
}
#endif

#endif

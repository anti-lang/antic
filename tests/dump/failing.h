/* failing.h, the C interface of com.example.failing, written by antic.
   Do not edit. A failure that Anti cannot report calls abort(). */
#ifndef FAILING_H
#define FAILING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#define ANTI_ALIGNAS(n) alignas(n)
extern "C" {
#else
#define ANTI_ALIGNAS(n) _Alignas(n)
#endif

/* An Anti error. A function that may fail returns a pointer to one,
   or NULL on success. C passes it on and never reads its layout. */
struct anti_Error;

/** A count that must stay positive. */
typedef struct Counter {
    int32_t n;
} Counter;

/** Halve n, or fail when it is odd. */
/* May fail: NULL on success, an error otherwise. */
struct anti_Error *failing_half(int32_t n, int32_t * /* non-null */ out);
/** Add one to the counter, or fail when it would pass the limit. */
/* May fail: NULL on success, an error otherwise. */
struct anti_Error *failing_step(Counter * /* non-null */ c, int32_t limit);
/** Twice n, which never fails. */
int32_t failing_twice(int32_t n);

#ifdef __cplusplus
}
#endif

#endif

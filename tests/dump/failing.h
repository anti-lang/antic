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

/* The root of every class chain, and the record at entry 0 of
   every table. A C program reads the layout and never builds one. */
typedef struct anti_descriptor anti_descriptor;
typedef struct anti_Object {
    const void *vtable;
} anti_Object;

/* Run the destruct chain of the object, free what it owns and free it. */
void anti_rt_delete(void *object, const anti_descriptor *type);
void anti_rt_destroy(void *object, const anti_descriptor *type);
void *anti_rt_dup(void *object, const anti_descriptor *type);

/** A count that must stay positive. */
typedef struct Counter {
    int32_t n;
} Counter;

typedef struct Gauge Gauge;
typedef struct Gauge_vtable {
    const void *descriptor;
    /* The seven functions of anti.rt.Object. They take and give Anti
       values, so C reads their slots and does not call them. */
    void *type_name;
    void *to_text;
    void *equals;
    void *hash;
    void *serialize;
    void *destruct;
    void *copy;
} Gauge_vtable;

/** A gauge whose limit must be positive. */
struct Gauge {
    anti_Object base;
    int32_t limit;
    int32_t level;
};

extern const anti_descriptor anti_Gauge_descriptor;
extern const Gauge_vtable anti_Gauge_vtable;
void anti_Gauge_init(Gauge *self);
/** Take the limit, or fail when it is not positive. */
/* Prepares self as anti_Gauge_init does, then runs construct. */
/* May fail: NULL on success, an error otherwise. */
struct anti_Error *anti_Gauge_construct(Gauge *self, int32_t limit);
static inline void anti_Gauge_delete(Gauge *self)
{
    anti_rt_delete(self, &anti_Gauge_descriptor);
}
static inline void anti_Gauge_destroy(Gauge *self)
{
    anti_rt_destroy(self, &anti_Gauge_descriptor);
}
static inline Gauge *anti_Gauge_dup(Gauge *self)
{
    return (Gauge *)anti_rt_dup(self, &anti_Gauge_descriptor);
}



/** Halve n, or fail when it is odd. */
/* May fail: NULL on success, an error otherwise. */
struct anti_Error *failing_half(int32_t n, int32_t * /* non-null */ out);
/** Add one to the counter, or fail when it would pass the limit. */
/* May fail: NULL on success, an error otherwise. */
struct anti_Error *failing_step(Counter * /* non-null */ c, int32_t limit);
/** f of n, or the error f gives. C passes a function of the ABI form. */
/* May fail: NULL on success, an error otherwise. */
struct anti_Error *failing_apply(struct anti_Error *(*f)(int32_t, int32_t * /* non-null */), int32_t n, int32_t * /* non-null */ out);
/** Twice n, which never fails. */
int32_t failing_twice(int32_t n);

#ifdef __cplusplus
}
#endif

#endif

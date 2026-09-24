/* ledger.h, the C interface of com.example.ledger, written by antic.
   Do not edit. A failure that Anti cannot report calls abort(). */
#ifndef LEDGER_H
#define LEDGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#define ANTI_ALIGNAS(n) alignas(n)
extern "C" {
#else
#define ANTI_ALIGNAS(n) _Alignas(n)
#endif

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

typedef struct Ledger Ledger;
typedef struct Ledger_vtable {
    const void *descriptor;
    /* The seven functions of anti.lang.Object. They take and give Anti
       values, so C reads their slots and does not call them. */
    void *type_name;
    void *to_text;
    void *equals;
    void *hash;
    void *serialize;
    void *destruct;
    void *copy;
    void *created;
    void *destroyed;
    void *copied;
    void *dispatched;
    void *joined;
    void *enter;
    void *leave;
    void *failed;
    void *changed;
    void (*add)(Ledger *self, int64_t n);
    int64_t (*total)(Ledger *self);
} Ledger_vtable;

/** A sum that threads add to. */
struct Ledger {
    anti_Object base;
    int64_t sum;   /* private */
    struct {
#if defined(_WIN32)
        void *word;
#else
        uint32_t word;
#endif
        int64_t owner;
        int64_t depth;
    } anti_lock;   /* the lock of the object */
};

extern const anti_descriptor anti_Ledger_descriptor;
extern const Ledger_vtable anti_Ledger_vtable;
void anti_Ledger_init(Ledger *self);
static inline void anti_Ledger_delete(Ledger *self)
{
    anti_rt_delete(self, &anti_Ledger_descriptor);
}
static inline void anti_Ledger_destroy(Ledger *self)
{
    anti_rt_destroy(self, &anti_Ledger_descriptor);
}
static inline Ledger *anti_Ledger_dup(Ledger *self)
{
    return (Ledger *)anti_rt_dup(self, &anti_Ledger_descriptor);
}

/** Add n to the sum. */
/* Runs under the lock of its object. */
void Ledger_add(Ledger *self, int64_t n);
/** The sum so far. */
/* Runs under the lock of its object. */
int64_t Ledger_total(Ledger *self);

static inline void anti_Ledger_add(Ledger *self, int64_t n)
{
     ((const Ledger_vtable *)self->base.vtable)->add(self, n);
}
static inline int64_t anti_Ledger_total(Ledger *self)
{
    return ((const Ledger_vtable *)self->base.vtable)->total(self);
}

#ifdef __cplusplus
}
#endif

#endif

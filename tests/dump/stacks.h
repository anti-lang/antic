/* stacks.h, the C interface of com.example.stacks, written by antic.
   Do not edit. A failure that Anti cannot report calls abort(). */
#ifndef STACKS_H
#define STACKS_H

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

typedef struct Ints Ints;
typedef struct Ints_vtable {
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
    void (*push)(Ints *self, int32_t value);
    int32_t (*pop)(Ints *self);
    int32_t (*size)(Ints *self);
} Ints_vtable;

/** The stack of `c_int` that C uses. */
struct Ints {
    anti_Object base;
    int32_t items[8];
    int32_t count;
};

extern const anti_descriptor anti_Ints_descriptor;
extern const Ints_vtable anti_Ints_vtable;
void anti_Ints_init(Ints *self);
static inline void anti_Ints_delete(Ints *self)
{
    anti_rt_delete(self, &anti_Ints_descriptor);
}
static inline void anti_Ints_destroy(Ints *self)
{
    anti_rt_destroy(self, &anti_Ints_descriptor);
}
static inline Ints *anti_Ints_dup(Ints *self)
{
    return (Ints *)anti_rt_dup(self, &anti_Ints_descriptor);
}

/** Put a value on top, unless the stack is full. */
void Ints_push(Ints *self, int32_t value);
/** Take the value on top. */
int32_t Ints_pop(Ints *self);
/** The number of values. */
int32_t Ints_size(Ints *self);

static inline void anti_Ints_push(Ints *self, int32_t value)
{
     ((const Ints_vtable *)self->base.vtable)->push(self, value);
}
static inline int32_t anti_Ints_pop(Ints *self)
{
    return ((const Ints_vtable *)self->base.vtable)->pop(self);
}
static inline int32_t anti_Ints_size(Ints *self)
{
    return ((const Ints_vtable *)self->base.vtable)->size(self);
}

/** The sum of the values of a stack, which C passes by pointer. */
int32_t total(Ints * /* non-null */ s);

#ifdef __cplusplus
}
#endif

#endif

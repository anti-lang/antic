/* nested.h, the C interface of com.example.nested, written by antic.
   Do not edit. A failure that Anti cannot report calls abort(). */
#ifndef NESTED_H
#define NESTED_H

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

typedef struct PeopleList PeopleList;
typedef struct PeopleList_Node PeopleList_Node;
typedef struct PeopleList_Counter_Step PeopleList_Counter_Step;
typedef struct PeopleList_Counter PeopleList_Counter;

/* PeopleList.Node, private to PeopleList. */
struct PeopleList_Node {
    int32_t age;
    struct PeopleList_Node *next;
};

enum PeopleList_State {
    PeopleList_State_Empty = 0,
    PeopleList_State_Filled = 1
};

/* PeopleList.Counter.Step, private to PeopleList. */
struct PeopleList_Counter_Step {
    int32_t by;
};

/* PeopleList.Counter, private to PeopleList. */
struct PeopleList_Counter {
    anti_Object base;
    int32_t total;
    PeopleList_Counter_Step step;   /* private */
};

typedef struct PeopleList_vtable {
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
    void (*add)(PeopleList *self, int32_t age);
    void (*clear)(PeopleList *self);
} PeopleList_vtable;

struct PeopleList {
    anti_Object base;
    PeopleList_Node *head;   /* private */
    uint8_t state;   /* private */
    PeopleList_Counter count;   /* private */
    int32_t size;
};

extern const anti_descriptor anti_PeopleList_descriptor;
extern const PeopleList_vtable anti_PeopleList_vtable;
void anti_PeopleList_init(PeopleList *self);
static inline void anti_PeopleList_delete(PeopleList *self)
{
    anti_rt_delete(self, &anti_PeopleList_descriptor);
}
static inline void anti_PeopleList_destroy(PeopleList *self)
{
    anti_rt_destroy(self, &anti_PeopleList_descriptor);
}
static inline PeopleList *anti_PeopleList_dup(PeopleList *self)
{
    return (PeopleList *)anti_rt_dup(self, &anti_PeopleList_descriptor);
}

/** Put a person of the given age in front. */
void PeopleList_add(PeopleList *self, int32_t age);
/** Free every node. */
void PeopleList_clear(PeopleList *self);

static inline void anti_PeopleList_add(PeopleList *self, int32_t age)
{
     ((const PeopleList_vtable *)self->base.vtable)->add(self, age);
}
static inline void anti_PeopleList_clear(PeopleList *self)
{
     ((const PeopleList_vtable *)self->base.vtable)->clear(self);
}

#ifdef __cplusplus
}
#endif

#endif

/* handlers.h, the C interface of com.example.handlers, written by antic.
   Do not edit. A failure that Anti cannot report calls abort(). */
#ifndef HANDLERS_H
#define HANDLERS_H

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

/* Free the snapshot of an `own fn` field, which the object frees
   with itself. NULL frees nothing. */
void anti_rt_snapshot_free(void *snapshot);

typedef struct Button Button;
typedef struct Button_vtable {
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
    void (*listen)(Button *self, int32_t base);
} Button_vtable;

/** A button that owns what a click runs. */
struct Button {
    anti_Object base;
    /** What a click runs, with the snapshot it reads. */
    struct {
        void (*code)(int32_t, void *);
        void *snapshot;
    } handler;   /* own */
};

extern const anti_descriptor anti_Button_descriptor;
extern const Button_vtable anti_Button_vtable;
void anti_Button_init(Button *self);
static inline void anti_Button_delete(Button *self)
{
    anti_rt_delete(self, &anti_Button_descriptor);
}
static inline void anti_Button_destroy(Button *self)
{
    anti_rt_destroy(self, &anti_Button_descriptor);
}
static inline Button *anti_Button_dup(Button *self)
{
    return (Button *)anti_rt_dup(self, &anti_Button_descriptor);
}

/** Keep a handler that prints `base` plus what it is given. */
void Button_listen(Button *self, int32_t base);

static inline void anti_Button_listen(Button *self, int32_t base)
{
     ((const Button_vtable *)self->base.vtable)->listen(self, base);
}

/** A new button that runs nothing on a click. */
Button * /* non-null */ make_button(void);
/** The count of the snapshots alive. */
int64_t snapshots(void);

#ifdef __cplusplus
}
#endif

#endif

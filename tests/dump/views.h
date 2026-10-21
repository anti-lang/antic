/* views.h, the C interface of com.example.views, written by antic.
   Do not edit. A failure that Anti cannot report calls abort(). */
#ifndef VIEWS_H
#define VIEWS_H

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

/**
 * The levels of a log.
 *
 * The second paragraph follows an empty line.
 */
enum Level {
    Level_Quiet = 0,
    Level_Loud = 1
};

/** Two lanes of a float, eight bytes together. */
typedef struct F32x2 {
    ANTI_ALIGNAS(8) float a;
    float b;
} F32x2;

/** Eight lanes of a float, thirty-two bytes together. */
typedef struct F32x8 {
    ANTI_ALIGNAS(16) float a;
    float b;
    float c;
    float d;
    float e;
    float f;
    float g;
    float h;
} F32x8;

/** A struct whose fields have the names of macros of `<stdint.h>`. */
typedef struct Limits {
    int32_t SIZE_MAX_;
    uint8_t UINT8_MAX_;
} Limits;

/** A link of a chain, which names its own type. */
typedef struct Link {
    int32_t value;
    struct Link *next;
    void (*call)(void);
} Link;

/** A union that names its own type. */
typedef union Either {
    uint64_t whole;
    union Either *other;
} Either;

typedef struct Box Box;
typedef struct Box_Header Box_Header;

/* Box.Header, private to Box. */
#pragma pack(push, 1)
struct Box_Header {
    uint8_t tag;
    uint32_t size;
};
#pragma pack(pop)

typedef struct Box_vtable {
    const void *descriptor;
    /* The seven functions and the nine hooks of anti.lang.Object. They
       take and give Anti values, so C reads their slots and does not call
       them. */
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
    uint32_t (*size)(Box *self);
} Box_vtable;

/** A class with a protected field and types of its own. */
struct Box {
    anti_Object base;
    Box_Header head;   /* private */
    int32_t depth;   /* protected */
    int32_t width;
};

extern const anti_descriptor anti_Box_descriptor;
extern const Box_vtable anti_Box_vtable;
void anti_Box_init(Box *self);
static inline void anti_Box_delete(Box *self)
{
    anti_rt_delete(self, &anti_Box_descriptor);
}
static inline void anti_Box_destroy(Box *self)
{
    anti_rt_destroy(self, &anti_Box_descriptor);
}
static inline Box *anti_Box_dup(Box *self)
{
    return (Box *)anti_rt_dup(self, &anti_Box_descriptor);
}

/** The size the header names. */
uint32_t Box_size(Box *self);

static inline uint32_t anti_Box_size(Box *self)
{
    return ((const Box_vtable *)self->base.vtable)->size(self);
}

typedef struct Named Named;
typedef struct Named_vtable {
    const void *descriptor;
    /* The seven functions and the nine hooks of anti.lang.Object. They
       take and give Anti values, so C reads their slots and does not call
       them. */
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
    int32_t (*length)(Named *self);
} Named_vtable;

/** What has a name. */
struct Named {
    anti_Object base;
};

extern const anti_descriptor anti_Named_descriptor;
/* Named is abstract: no table and no init, because it has no complete value. */
static inline void anti_Named_delete(Named *self)
{
    anti_rt_delete(self, &anti_Named_descriptor);
}
static inline void anti_Named_destroy(Named *self)
{
    anti_rt_destroy(self, &anti_Named_descriptor);
}
static inline Named *anti_Named_dup(Named *self)
{
    return (Named *)anti_rt_dup(self, &anti_Named_descriptor);
}


/** The length of the name. */
static inline int32_t anti_Named_length(Named *self)
{
    return ((const Named_vtable *)self->base.vtable)->length(self);
}

typedef struct Tag Tag;
typedef struct Tag_vtable {
    const void *descriptor;
    /* The seven functions and the nine hooks of anti.lang.Object. They
       take and give Anti values, so C reads their slots and does not call
       them. */
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
    int32_t (*length)(Tag *self);
} Tag_vtable;

/** A class that has a name. */
struct Tag {
    anti_Object base;
    Named named;   /* private */
    int32_t id;
};

extern const anti_descriptor anti_Tag_descriptor;
extern const Tag_vtable anti_Tag_vtable;
void anti_Tag_init(Tag *self);
static inline void anti_Tag_delete(Tag *self)
{
    anti_rt_delete(self, &anti_Tag_descriptor);
}
static inline void anti_Tag_destroy(Tag *self)
{
    anti_rt_destroy(self, &anti_Tag_descriptor);
}
static inline Tag *anti_Tag_dup(Tag *self)
{
    return (Tag *)anti_rt_dup(self, &anti_Tag_descriptor);
}

extern const Named_vtable anti_Tag_Named_vtable;
static inline Named *anti_Tag_as_Named(Tag *self)
{
    return &self->named;
}
int32_t Tag_length(Tag *self);

static inline int32_t anti_Tag_length(Tag *self)
{
    return ((const Tag_vtable *)self->base.vtable)->length(self);
}

typedef struct LongTag LongTag;
typedef struct LongTag_vtable {
    const void *descriptor;
    /* The seven functions and the nine hooks of anti.lang.Object. They
       take and give Anti values, so C reads their slots and does not call
       them. */
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
    int32_t (*length)(LongTag *self);
    int32_t (*code_of)(LongTag *self, struct anti_Error *e);
} LongTag_vtable;

/** A class that takes its name from its base. */
struct LongTag {
    Tag base;
    int32_t extra;
};

extern const anti_descriptor anti_LongTag_descriptor;
extern const LongTag_vtable anti_LongTag_vtable;
void anti_LongTag_init(LongTag *self);
static inline void anti_LongTag_delete(LongTag *self)
{
    anti_rt_delete(self, &anti_LongTag_descriptor);
}
static inline void anti_LongTag_destroy(LongTag *self)
{
    anti_rt_destroy(self, &anti_LongTag_descriptor);
}
static inline LongTag *anti_LongTag_dup(LongTag *self)
{
    return (LongTag *)anti_rt_dup(self, &anti_LongTag_descriptor);
}

extern const Named_vtable anti_LongTag_Named_vtable;
static inline Named *anti_LongTag_as_Named(LongTag *self)
{
    return &self->base.named;
}
/** The code of an error, or zero for none. */
int32_t LongTag_code_of(LongTag *self, struct anti_Error *e);

static inline int32_t anti_LongTag_length(LongTag *self)
{
    return ((const LongTag_vtable *)self->base.base.vtable)->length(self);
}
static inline int32_t anti_LongTag_code_of(LongTag *self, struct anti_Error *e)
{
    return ((const LongTag_vtable *)self->base.base.vtable)->code_of(self, e);
}

typedef struct Tally Tally;
typedef struct Tally_vtable {
    const void *descriptor;
    /* The seven functions and the nine hooks of anti.lang.Object. They
       take and give Anti values, so C reads their slots and does not call
       them. */
    void *type_name;
    void *to_text;
    void *equals;
    void *hash;
    void *serialize;
    void (*destruct)(Tally *self);
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
    void (*construct)(Tally *self);
    void (*add)(Tally *self, int32_t n);
    int32_t (*sum)(Tally *self);
} Tally_vtable;

/** A class whose functions run under its lock. */
struct Tally {
    anti_Object base;
    int32_t total;   /* private */
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

extern const anti_descriptor anti_Tally_descriptor;
extern const Tally_vtable anti_Tally_vtable;
void anti_Tally_init(Tally *self);
static inline void anti_Tally_delete(Tally *self)
{
    anti_rt_delete(self, &anti_Tally_descriptor);
}
static inline void anti_Tally_destroy(Tally *self)
{
    anti_rt_destroy(self, &anti_Tally_descriptor);
}
static inline Tally *anti_Tally_dup(Tally *self)
{
    return (Tally *)anti_rt_dup(self, &anti_Tally_descriptor);
}

/** Nothing to release. */
void Tally_destruct(Tally *self);
/** Start at zero. */
void Tally_construct(Tally *self);
/** Add to the total. */
/* Runs under the lock of its object. */
void Tally_add(Tally *self, int32_t n);
/** The total. */
/* Runs under the lock of its object. */
int32_t Tally_sum(Tally *self);

static inline void anti_Tally_destruct(Tally *self)
{
     ((const Tally_vtable *)self->base.vtable)->destruct(self);
}
static inline void anti_Tally_construct_(Tally *self)
{
     ((const Tally_vtable *)self->base.vtable)->construct(self);
}
static inline void anti_Tally_add(Tally *self, int32_t n)
{
     ((const Tally_vtable *)self->base.vtable)->add(self, n);
}
static inline int32_t anti_Tally_sum(Tally *self)
{
    return ((const Tally_vtable *)self->base.vtable)->sum(self);
}

/* The optional value ?i32. */
struct anti_opt_i32 {
    int32_t value;
    bool has;
};

/** The level after a level. */
uint8_t louder(uint8_t l);
/** A value that may be none. */
struct anti_opt_i32 half(int32_t n);
/** The value of a link that the caller keeps. */
int32_t value_of(/* lent */ Link * /* non-null */ l);
/** The first lane of a vector. */
float first(F32x2 v);
/** The sum of the lanes of a wide vector. */
float wide_sum(F32x8 v);
/** The larger of the two limits. */
int32_t limit_of(Limits l);
/** The result of a function that may fail, or zero. */
int32_t attempt(struct anti_Error *(*f)(int32_t, int32_t * /* non-null */, void *), void *f_context, int32_t n);
/** The whole of a union. */
uint64_t whole_of(Either e);

#ifdef __cplusplus
}
#endif

#endif

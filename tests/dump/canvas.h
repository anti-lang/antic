/* canvas.h, the C interface of com.example.canvas, written by antic.
   Do not edit. A failure that Anti cannot report calls abort(). */
#ifndef CANVAS_H
#define CANVAS_H

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

typedef struct Ink Ink;
typedef struct Ink_vtable {
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
    int32_t (*colour)(Ink *self);
} Ink_vtable;

/** What a shape can be drawn with. */
struct Ink {
    anti_Object base;
};

extern const anti_descriptor anti_Ink_descriptor;
/* Ink is abstract: no table and no init, because it has no complete value. */
static inline void anti_Ink_delete(Ink *self)
{
    anti_rt_delete(self, &anti_Ink_descriptor);
}
static inline void anti_Ink_destroy(Ink *self)
{
    anti_rt_destroy(self, &anti_Ink_descriptor);
}
static inline Ink *anti_Ink_dup(Ink *self)
{
    return (Ink *)anti_rt_dup(self, &anti_Ink_descriptor);
}


/** The colour the shape draws with. */
static inline int32_t anti_Ink_colour(Ink *self)
{
    return ((const Ink_vtable *)self->base.vtable)->colour(self);
}

typedef struct Shape Shape;
typedef struct Shape_vtable {
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
    int32_t (*area)(Shape *self);
    void (*move)(Shape *self, int32_t dx, int32_t dy);
} Shape_vtable;

/** A shape on the canvas. */
struct Shape {
    anti_Object base;
    int32_t x;   /* private */
    int32_t y;   /* private */
};

extern const anti_descriptor anti_Shape_descriptor;
extern const Shape_vtable anti_Shape_vtable;
void anti_Shape_init(Shape *self);
static inline void anti_Shape_delete(Shape *self)
{
    anti_rt_delete(self, &anti_Shape_descriptor);
}
static inline void anti_Shape_destroy(Shape *self)
{
    anti_rt_destroy(self, &anti_Shape_descriptor);
}
static inline Shape *anti_Shape_dup(Shape *self)
{
    return (Shape *)anti_rt_dup(self, &anti_Shape_descriptor);
}

/** The area of the shape, which each class below decides. */
int32_t Shape_area(Shape *self);
/** Move the shape by dx and dy. */
void Shape_move(Shape *self, int32_t dx, int32_t dy);

static inline int32_t anti_Shape_area(Shape *self)
{
    return ((const Shape_vtable *)self->base.vtable)->area(self);
}
static inline void anti_Shape_move(Shape *self, int32_t dx, int32_t dy)
{
     ((const Shape_vtable *)self->base.vtable)->move(self, dx, dy);
}

typedef struct Square Square;
typedef struct Square_vtable {
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
    int32_t (*area)(Square *self);
    void (*move)(Square *self, int32_t dx, int32_t dy);
    int32_t (*colour)(Square *self);
} Square_vtable;

/** A square of one side. */
struct Square {
    Shape base;
    Ink ink;   /* private */
    int32_t side;   /* private */
};

extern const anti_descriptor anti_Square_descriptor;
extern const Square_vtable anti_Square_vtable;
void anti_Square_init(Square *self);
static inline void anti_Square_delete(Square *self)
{
    anti_rt_delete(self, &anti_Square_descriptor);
}
static inline void anti_Square_destroy(Square *self)
{
    anti_rt_destroy(self, &anti_Square_descriptor);
}
static inline Square *anti_Square_dup(Square *self)
{
    return (Square *)anti_rt_dup(self, &anti_Square_descriptor);
}

extern const Ink_vtable anti_Square_Ink_vtable;
static inline Ink *anti_Square_as_Ink(Square *self)
{
    return &self->ink;
}
int32_t Square_area(Square *self);
/** Move the shape by dx and dy. */
void Shape_move(Shape *self, int32_t dx, int32_t dy);
int32_t Square_colour(Square *self);

static inline int32_t anti_Square_area(Square *self)
{
    return ((const Square_vtable *)self->base.base.vtable)->area(self);
}
static inline void anti_Square_move(Square *self, int32_t dx, int32_t dy)
{
     ((const Square_vtable *)self->base.base.vtable)->move(self, dx, dy);
}
static inline int32_t anti_Square_colour(Square *self)
{
    return ((const Square_vtable *)self->base.base.vtable)->colour(self);
}

typedef struct Circle Circle;
typedef struct Circle_vtable {
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
    int32_t (*area)(Circle *self);
    void (*move)(Circle *self, int32_t dx, int32_t dy);
} Circle_vtable;

/** A circle of a positive radius. */
struct Circle {
    Shape base;
    int32_t radius;   /* private */
};

extern const anti_descriptor anti_Circle_descriptor;
extern const Circle_vtable anti_Circle_vtable;
void anti_Circle_init(Circle *self);
/** Take the radius, or fail when it is not positive. */
/* Prepares self as anti_Circle_init does, then runs construct. */
/* May fail: NULL on success, an error otherwise. */
struct anti_Error *anti_Circle_construct(Circle *self, int32_t radius);
static inline void anti_Circle_delete(Circle *self)
{
    anti_rt_delete(self, &anti_Circle_descriptor);
}
static inline void anti_Circle_destroy(Circle *self)
{
    anti_rt_destroy(self, &anti_Circle_descriptor);
}
static inline Circle *anti_Circle_dup(Circle *self)
{
    return (Circle *)anti_rt_dup(self, &anti_Circle_descriptor);
}

int32_t Circle_area(Circle *self);
/** Move the shape by dx and dy. */
void Shape_move(Shape *self, int32_t dx, int32_t dy);

static inline int32_t anti_Circle_area(Circle *self)
{
    return ((const Circle_vtable *)self->base.base.vtable)->area(self);
}
static inline void anti_Circle_move(Circle *self, int32_t dx, int32_t dy)
{
     ((const Circle_vtable *)self->base.base.vtable)->move(self, dx, dy);
}

typedef struct Tint Tint;
typedef struct Tint_vtable {
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
    int32_t (*colour)(Tint *self, int32_t k);
} Tint_vtable;

/** A second ink, which takes a shade. */
struct Tint {
    anti_Object base;
};

extern const anti_descriptor anti_Tint_descriptor;
/* Tint is abstract: no table and no init, because it has no complete value. */
static inline void anti_Tint_delete(Tint *self)
{
    anti_rt_delete(self, &anti_Tint_descriptor);
}
static inline void anti_Tint_destroy(Tint *self)
{
    anti_rt_destroy(self, &anti_Tint_descriptor);
}
static inline Tint *anti_Tint_dup(Tint *self)
{
    return (Tint *)anti_rt_dup(self, &anti_Tint_descriptor);
}


/** The colour at the shade k. */
static inline int32_t anti_Tint_colour(Tint *self, int32_t k)
{
    return ((const Tint_vtable *)self->base.vtable)->colour(self, k);
}

typedef struct Stamp Stamp;
typedef struct Stamp_vtable {
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
} Stamp_vtable;

/** A stamp, which fills `colour` once for each ink. */
struct Stamp {
    anti_Object base;
    Ink ink;   /* private */
    Tint tint;   /* private */
    int32_t shade;   /* private */
};

extern const anti_descriptor anti_Stamp_descriptor;
extern const Stamp_vtable anti_Stamp_vtable;
void anti_Stamp_init(Stamp *self);
static inline void anti_Stamp_delete(Stamp *self)
{
    anti_rt_delete(self, &anti_Stamp_descriptor);
}
static inline void anti_Stamp_destroy(Stamp *self)
{
    anti_rt_destroy(self, &anti_Stamp_descriptor);
}
static inline Stamp *anti_Stamp_dup(Stamp *self)
{
    return (Stamp *)anti_rt_dup(self, &anti_Stamp_descriptor);
}

extern const Ink_vtable anti_Stamp_Ink_vtable;
static inline Ink *anti_Stamp_as_Ink(Stamp *self)
{
    return &self->ink;
}
extern const Tint_vtable anti_Stamp_Tint_vtable;
static inline Tint *anti_Stamp_as_Tint(Stamp *self)
{
    return &self->tint;
}


typedef struct Tile Tile;
typedef struct Tile_vtable {
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
    int32_t (*area)(Tile *self);
    void (*move)(Tile *self, int32_t dx, int32_t dy);
} Tile_vtable;

/** A tile, whose body of `area` names the base. */
struct Tile {
    Shape base;
    int32_t side;   /* private */
};

extern const anti_descriptor anti_Tile_descriptor;
extern const Tile_vtable anti_Tile_vtable;
void anti_Tile_init(Tile *self);
static inline void anti_Tile_delete(Tile *self)
{
    anti_rt_delete(self, &anti_Tile_descriptor);
}
static inline void anti_Tile_destroy(Tile *self)
{
    anti_rt_destroy(self, &anti_Tile_descriptor);
}
static inline Tile *anti_Tile_dup(Tile *self)
{
    return (Tile *)anti_rt_dup(self, &anti_Tile_descriptor);
}

int32_t Tile_Shape_area(Tile *self);
/** Move the shape by dx and dy. */
void Shape_move(Shape *self, int32_t dx, int32_t dy);

static inline int32_t anti_Tile_area(Tile *self)
{
    return ((const Tile_vtable *)self->base.base.vtable)->area(self);
}
static inline void anti_Tile_move(Tile *self, int32_t dx, int32_t dy)
{
     ((const Tile_vtable *)self->base.base.vtable)->move(self, dx, dy);
}

#ifdef __cplusplus
}
#endif

#endif

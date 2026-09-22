/* variants.h, the C interface of com.example.variants, written by antic.
   Do not edit. A failure that Anti cannot report calls abort(). */
#ifndef VARIANTS_H
#define VARIANTS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#define ANTI_ALIGNAS(n) alignas(n)
extern "C" {
#else
#define ANTI_ALIGNAS(n) _Alignas(n)
#endif

/* The tags of Shape. */
enum Shape_tag {
    /** A circle of radius `r`. */
    Shape_Circle = 0,
    Shape_Rect = 1,
    Shape_Empty = 2
};

/** A shape of the plane. */
typedef struct Shape {
    uint8_t tag;
    union {
        struct {
            float r;
        } Circle;
        struct {
            float w;
            float h;
        } Rect;
    } u;
} Shape;

/** A shape and its count, held by value. */
typedef struct Tile {
    Shape shape;
    int32_t count;
} Tile;

/* The tags of Mark. */
enum Mark_tag {
    Mark_At = 0,
    Mark_Off = 1
};

/** A mark on a line, packed. */
#pragma pack(push, 1)
typedef struct Mark {
    uint8_t tag;
    union {
        struct {
            int32_t x;
        } At;
    } u;
} Mark;
#pragma pack(pop)

/* The tags of Wide. */
enum Wide_tag {
    Wide_One = 0,
    Wide_Two = 1
};

/** A variant on a boundary of sixteen bytes. */
typedef struct Wide {
    ANTI_ALIGNAS(16) uint8_t tag;
    union {
        struct {
            uint8_t a;
        } One;
    } u;
} Wide;

/** The area of a shape. */
float shape_area(Shape s);
/** A circle of radius `r`. */
Shape shape_circle(float r);
/** The shape scaled by `k`, counted twice. */
Tile shape_tile(Shape s, float k);

#ifdef __cplusplus
}
#endif

#endif

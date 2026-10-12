/* The ABI probe of the types area of docs/notes/test-matrix.md, in C.
   probe_types.anti prints the same for the same types, as probe.anti
   does for probe.c, whose design note holds for this file as well.

   A packed struct holds a field of each width at an odd offset. An
   aligned struct stands inside a packed one, and that one inside a plain
   one. A union is packed, and one is aligned. Shape and Dense are what
   the header writes for a variant: the tag and the union of the cases,
   plain and packed. OptInt, OptByte, Pair and Slice are the structs of
   `?int`, `?u8`, `(u8, int)` and `[]int`. */
#include "../binary_stdio.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#pragma pack(push, 1)
struct OddFields
{
    uint8_t tag;
    uint64_t wide;
    uint16_t mid;
    double real;
    float half;
    void *p;
    int32_t xs[3];
};
#pragma pack(pop)

struct In
{
    _Alignas(8) uint8_t a;
    uint16_t b;
};

#pragma pack(push, 1)
struct Out
{
    uint8_t lead;
    struct In in;
    uint32_t tail;
};

union Bytes
{
    uint8_t a;
    uint32_t b;
    uint8_t c[5];
};
#pragma pack(pop)

struct Norm
{
    uint8_t lead;
    struct Out out;
    struct In in;
};

union Sixteen
{
    _Alignas(16) uint8_t a;
    uint32_t b;
};

struct Shape
{
    uint8_t tag;
    union
    {
        struct
        {
            int64_t r;
        } Circle;
        struct
        {
            int32_t w;
            int32_t h;
            uint8_t on;
        } Rect;
    } u;
};

#pragma pack(push, 1)
struct Dense
{
    uint8_t tag;
    union
    {
        struct
        {
            uint64_t x;
        } A;
        struct
        {
            uint16_t y;
            uint32_t z;
        } B;
    } u;
};
#pragma pack(pop)

enum Plain
{
    Plain_A,
    Plain_B
};

struct Rows
{
    uint8_t lead;
    int16_t g[2][3];
    uint8_t tail;
};

struct OptInt
{
    int64_t value;
    bool has;
};

struct OptByte
{
    uint8_t value;
    bool has;
};

struct Pair
{
    uint8_t first;
    int64_t second;
};

struct Slice
{
    int64_t *ptr;
    int64_t len;
};

static void dump(const char *name, const void *value, size_t size)
{
    const unsigned char *p = value;
    size_t i;

    printf("%s", name);
    for (i = 0; i < size; i++) {
        printf(" %02x", p[i]);
    }
    printf("\n");
}

#define TYPE(T, name)                                                     \
    printf("%s size %d align %d\n", name, (int)sizeof(T),                  \
           (int)_Alignof(T))

#define FIELD(T, name, f)                                                 \
    do {                                                                  \
        T v;                                                              \
        memset(&v, 0, sizeof v);                                          \
        v.f = 1;                                                          \
        dump(name "." #f, &v, sizeof v);                                  \
    } while (0)

int main(void)
{
    TYPE(struct OddFields, "OddFields");
    FIELD(struct OddFields, "OddFields", tag);
    FIELD(struct OddFields, "OddFields", wide);
    FIELD(struct OddFields, "OddFields", mid);
    FIELD(struct OddFields, "OddFields", real);
    FIELD(struct OddFields, "OddFields", half);
    FIELD(struct OddFields, "OddFields", xs[2]);
    TYPE(struct In, "In");
    FIELD(struct In, "In", a);
    FIELD(struct In, "In", b);
    TYPE(struct Out, "Out");
    FIELD(struct Out, "Out", lead);
    FIELD(struct Out, "Out", in.b);
    FIELD(struct Out, "Out", tail);
    TYPE(union Bytes, "Bytes");
    FIELD(union Bytes, "Bytes", b);
    FIELD(union Bytes, "Bytes", c[4]);
    TYPE(struct Norm, "Norm");
    FIELD(struct Norm, "Norm", lead);
    FIELD(struct Norm, "Norm", out.tail);
    FIELD(struct Norm, "Norm", in.a);
    TYPE(union Sixteen, "Sixteen");
    FIELD(union Sixteen, "Sixteen", b);
    TYPE(struct Rows, "Rows");
    FIELD(struct Rows, "Rows", lead);
    FIELD(struct Rows, "Rows", g[1][2]);
    FIELD(struct Rows, "Rows", tail);
    TYPE(struct Shape, "Shape");
    TYPE(struct Dense, "Dense");
    TYPE(enum Plain, "Plain");
    TYPE(struct OptInt, "OptInt");
    TYPE(struct OptByte, "OptByte");
    TYPE(struct Pair, "Pair");
    TYPE(struct Slice, "Slice");
    {
        /* A case of each variant, by its tag and the bytes of its fields.
           The bytes between them are padding and are not printed. */
        struct Shape s;
        struct Dense d;

        memset(&s, 0, sizeof s);
        s.tag = 1;
        s.u.Rect.w = 0x01020304;
        s.u.Rect.h = 0x05060708;
        s.u.Rect.on = 9;
        dump("Shape.Rect.tag", &s, 1);
        dump("Shape.Rect.u", (const char *)&s + offsetof(struct Shape, u), 9);
        memset(&d, 0, sizeof d);
        d.tag = 1;
        d.u.B.y = 7;
        d.u.B.z = 9;
        dump("Dense.B.tag", &d, 1);
        dump("Dense.B.u", (const char *)&d + offsetof(struct Dense, u), 6);
    }
    return 0;
}

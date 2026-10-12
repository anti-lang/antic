/* The ABI probe in C. For every struct and union it prints the size and
   the alignment, and for every field the offset and the value of that
   field. probe.anti prints the same for the same types, so any difference
   in layout shows as a difference in the output.

   DESIGN: a field is printed as the bytes of an object cleared first and
   then given that one field. The image carries the offset and the value
   together, and neither language needs an offsetof. Both clear the
   object, so every byte of the image is defined. A case that cannot clear
   the object prints the value of each field instead. C leaves the padding
   of a struct unspecified once a value is stored in it (C11 6.2.6.1p6). */
#include "../binary_stdio.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

struct Wide
{
    int8_t a;
    long b;
    wchar_t c;
    int16_t d;
};

union Mixed
{
    int8_t a;
    int32_t b;
    double c;
    int64_t d;
};

struct Flags
{
    uint32_t visible : 1;
    uint32_t layer : 4;
    int8_t level : 3;
    uint16_t pad : 9;
};

struct Spread
{
    int8_t a;
    uint32_t b : 5;
    int64_t c : 40;
    uint8_t d : 7;
};

#pragma pack(push, 1)
struct Tight
{
    int8_t a;
    int32_t b;
    int16_t c;
};

struct TightBits
{
    uint8_t a : 3;
    uint32_t b : 7;
};
#pragma pack(pop)

struct Aligned
{
    _Alignas(16) int8_t a;
    int32_t b;
};

union BitUnion
{
    uint32_t a : 3;
    uint8_t b : 7;
};

struct Breaks
{
    uint8_t a;
    uint32_t : 0;
    uint8_t b : 3;
    uint64_t : 0;
    uint8_t c : 5;
};

/* The types of the area types of docs/notes/test-matrix.md. A packed
   struct holds a field of each width at an odd offset. An aligned struct
   stands inside a packed one, and that one inside a plain one. A union is
   packed, and one is aligned. Shape and Dense are what the header writes
   for a variant: the tag and the union of the cases, plain and packed.
   OptInt, OptByte, Pair and Slice are the structs of `?int`, `?u8`,
   `(u8, int)` and `[]int`. */
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
    TYPE(struct Wide, "Wide");
    FIELD(struct Wide, "Wide", a);
    FIELD(struct Wide, "Wide", b);
    FIELD(struct Wide, "Wide", c);
    FIELD(struct Wide, "Wide", d);
    TYPE(union Mixed, "Mixed");
    FIELD(union Mixed, "Mixed", a);
    FIELD(union Mixed, "Mixed", b);
    FIELD(union Mixed, "Mixed", c);
    FIELD(union Mixed, "Mixed", d);
    TYPE(struct Flags, "Flags");
    FIELD(struct Flags, "Flags", visible);
    FIELD(struct Flags, "Flags", layer);
    FIELD(struct Flags, "Flags", level);
    FIELD(struct Flags, "Flags", pad);
    TYPE(struct Spread, "Spread");
    FIELD(struct Spread, "Spread", a);
    FIELD(struct Spread, "Spread", b);
    FIELD(struct Spread, "Spread", c);
    FIELD(struct Spread, "Spread", d);
    TYPE(struct Tight, "Tight");
    FIELD(struct Tight, "Tight", a);
    FIELD(struct Tight, "Tight", b);
    FIELD(struct Tight, "Tight", c);
    TYPE(struct TightBits, "TightBits");
    FIELD(struct TightBits, "TightBits", a);
    FIELD(struct TightBits, "TightBits", b);
    TYPE(struct Aligned, "Aligned");
    FIELD(struct Aligned, "Aligned", a);
    FIELD(struct Aligned, "Aligned", b);
    TYPE(union BitUnion, "BitUnion");
    FIELD(union BitUnion, "BitUnion", a);
    FIELD(union BitUnion, "BitUnion", b);
    TYPE(struct Breaks, "Breaks");
    FIELD(struct Breaks, "Breaks", a);
    FIELD(struct Breaks, "Breaks", b);
    FIELD(struct Breaks, "Breaks", c);
    {
        /* A constant copied into an object of the type. The padding of
           the copy is unspecified, so the fields are read back. */
        static const struct Breaks k = { .a = 1, .b = 2, .c = 3 };
        struct Breaks v = k;

        printf("Breaks.const a %d b %d c %d\n", (int)v.a, (int)v.b,
               (int)v.c);
    }
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

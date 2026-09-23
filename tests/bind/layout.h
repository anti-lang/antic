/* The header that the test anti_bind_clang binds. It holds one of each
   form that anti bind writes. There are unions, bitfields with a unit
   break, a packed and an aligned struct, enums and function pointers.
   There are the fixed C types, macro constants, and static and C99 inline
   functions. */
#ifndef LAYOUT_H
#define LAYOUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LAYOUT_ANSWER 42
#define LAYOUT_HALF (LAYOUT_ANSWER / 2)
#define LAYOUT_MASK (1u << 31)
#define LAYOUT_BIG 0x1FFFFFFFFull
#define LAYOUT_SCALE 0.5f
#define LAYOUT_RATIO (1.0 / 3.0)
#define LAYOUT_NAME "lay" "out"
#define LAYOUT_TWICE(x) ((x) * 2)
#define LAYOUT_DEFAULT_COLOR LAYOUT_GREEN

typedef enum
{
    LAYOUT_RED = 1,
    LAYOUT_GREEN = LAYOUT_RED + 1,
    LAYOUT_BLUE = -3
} LayoutColor;

enum LayoutMode
{
    LAYOUT_READ,
    LAYOUT_WRITE,
    LAYOUT_APPEND = 8,
    LAYOUT_CREATE
};

enum LayoutWide
{
    LAYOUT_SMALL = 0,
    LAYOUT_LARGE = 0xFFFFFFFFu
};

enum
{
    LAYOUT_LOOSE_A = 3,
    LAYOUT_LOOSE_B = 4
};

#pragma pack(push, 1)
typedef struct LayoutTight
{
    char a;
    int b;
    short c;
} LayoutTight;
#pragma pack(pop)

struct __attribute__((packed)) LayoutPacked
{
    char a;
    int64_t b;
};

struct LayoutAligned
{
    int x;
} __attribute__((aligned(16)));

struct LayoutFirst
{
    _Alignas(8) char a;
    int b;
};

struct LayoutBits
{
    unsigned a : 3;
    int : 0;
    signed char b : 2;
    uint64_t c : 40;
    uint8_t d : 7;
};

union LayoutValue
{
    int i;
    float f;
    double d;
    int64_t n;
};

typedef void (*LayoutLogger)(int level, const char *text, ...);
typedef int (*LayoutCompare)(const void *a, const void *b);

struct LayoutNode
{
    struct LayoutNode *next;
    LayoutCompare compare;
    LayoutLogger logger;
    float m[4];
    const char *name;
    volatile int ticks;
    LayoutColor color;
    enum LayoutMode mode;
    union LayoutValue value;
    struct LayoutBits bits;
    size_t size;
    wchar_t wide;
    long l;
    unsigned long ul;
    bool on;
};

struct LayoutHolder
{
    int kind;
    union
    {
        int i;
        float f;
    } as;
};

struct LayoutOpaque;

struct LayoutOpaque *layout_open(const char *path);
void layout_log(int level, const char *format, ...);
size_t layout_count(const uint8_t *data, int64_t n, bool flag);
int layout_sort(void *items, size_t n, LayoutCompare compare);
void layout_fill(int values[8]);
long double layout_precise(void);

static inline int layout_twice(int x)
{
    return 2 * x;
}

static inline struct LayoutHolder layout_hold(int kind, float f)
{
    struct LayoutHolder h;
    h.kind = kind;
    h.as.f = f;
    return h;
}

inline float layout_half(float x)
{
    return x / 2.0f;
}

#endif

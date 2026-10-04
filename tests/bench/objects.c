/* A method-heavy object loop, the C twin of objects.anti. 4096 shapes of
   two kinds take two calls through a table of function pointers on each
   of 20000 passes, and a counter collects the areas through two direct
   calls. */

#include "../binary_stdio.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

struct shape;

struct shape_table {
    int64_t (*area)(const struct shape *s);
    void (*grow)(struct shape *s, int64_t by);
};

struct shape {
    const struct shape_table *table;
};

struct square {
    struct shape base;
    int64_t side;
};

struct rect {
    struct shape base;
    int64_t w;
    int64_t h;
};

static int64_t square_area(const struct shape *s)
{
    const struct square *q = (const struct square *)s;
    return q->side * q->side;
}

static void square_grow(struct shape *s, int64_t by)
{
    struct square *q = (struct square *)s;
    q->side = (q->side + by) & 1023;
}

static int64_t rect_area(const struct shape *s)
{
    const struct rect *r = (const struct rect *)s;
    return r->w * r->h;
}

static void rect_grow(struct shape *s, int64_t by)
{
    struct rect *r = (struct rect *)s;
    r->w = (r->w + by) & 1023;
    r->h = (r->h + 2 * by) & 511;
}

static const struct shape_table square_table = {square_area, square_grow};
static const struct shape_table rect_table = {rect_area, rect_grow};

struct counter {
    uint64_t n;
};

static void counter_add(struct counter *c, int64_t v)
{
    c->n += (uint64_t)v;
}

static int64_t counter_get(const struct counter *c)
{
    return (int64_t)c->n;
}

int main(void)
{
    int64_t n = 4096;
    struct shape **shapes = malloc((size_t)n * sizeof *shapes);
    if (shapes == NULL) {
        return 1;
    }
    for (int64_t i = 0; i < n; i++) {
        if (i % 3 == 0) {
            struct square *q = malloc(sizeof *q);
            if (q == NULL) {
                return 1;
            }
            q->base.table = &square_table;
            q->side = i & 255;
            shapes[i] = &q->base;
        } else {
            struct rect *r = malloc(sizeof *r);
            if (r == NULL) {
                return 1;
            }
            r->base.table = &rect_table;
            r->w = i & 127;
            r->h = i & 63;
            shapes[i] = &r->base;
        }
    }
    struct counter c = {0};
    for (int64_t r = 0; r < 20000; r++) {
        for (int64_t i = 0; i < n; i++) {
            struct shape *s = shapes[i];
            s->table->grow(s, r & 7);
            counter_add(&c, s->table->area(s));
        }
    }
    printf("%lld\n", (long long)counter_get(&c));
    for (int64_t i = 0; i < n; i++) {
        free(shapes[i]);
    }
    free(shapes);
    return 0;
}

#include "antl_io.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* DESIGN: the primitives of the library file, which antl.c and
   antl_tree.c both read and write through. Each check of a length or a
   count stands here once, so the section of the generics keeps every
   check the tables have. */

/* Writing */

void antl_put_u8(struct writer *w, uint8_t v)
{
    text_append_bytes(w->out, &v, 1);
}

void antl_put_u32(struct writer *w, uint32_t v)
{
    uint8_t b[4];
    int i;

    for (i = 0; i < 4; i++) {
        b[i] = (uint8_t)(v >> (8 * i));
    }
    text_append_bytes(w->out, b, sizeof b);
}

void antl_put_count(struct writer *w, size_t n)
{
    if (n > UINT32_MAX) {
        w->failed = true;
        n = 0;
    }
    antl_put_u32(w, (uint32_t)n);
}

void antl_put_u64(struct writer *w, uint64_t v)
{
    uint8_t b[8];
    int i;

    for (i = 0; i < 8; i++) {
        b[i] = (uint8_t)(v >> (8 * i));
    }
    text_append_bytes(w->out, b, sizeof b);
}

void antl_put_bytes(struct writer *w, const char *s, size_t length)
{
    antl_put_count(w, length);
    text_append_bytes(w->out, s, length);
}

/* Reading */

void antl_fail(struct reader *r, const char *format, ...)
{
    va_list args;
    int n;

    if (r->failed) {
        return;
    }
    va_start(args, format);
    n = vsnprintf(r->error, r->error_size, format, args);
    va_end(args);
    /* A message cut to fit ends in three dots. */
    if (n >= 0 && (size_t)n >= r->error_size && r->error_size >= 4) {
        memcpy(r->error + r->error_size - 4, "...", 4);
    }
    r->failed = true;
}

void antl_damaged(struct reader *r)
{
    antl_fail(r, "is damaged at byte %zu", r->pos);
}

bool antl_take(struct reader *r, size_t n)
{
    if (r->failed || n > r->size - r->pos) {
        if (!r->failed) {
            r->pos = r->size;
            antl_damaged(r);
        }
        return false;
    }
    return true;
}

static uint64_t get_uint(struct reader *r, int bytes)
{
    uint64_t v = 0;
    int i;

    if (!antl_take(r, (size_t)bytes)) {
        return 0;
    }
    for (i = 0; i < bytes; i++) {
        v |= (uint64_t)r->data[r->pos + (size_t)i] << (8 * i);
    }
    r->pos += (size_t)bytes;
    return v;
}

uint8_t antl_get_u8(struct reader *r)
{
    return (uint8_t)get_uint(r, 1);
}

uint32_t antl_get_u32(struct reader *r)
{
    return (uint32_t)get_uint(r, 4);
}

uint64_t antl_get_u64(struct reader *r)
{
    return get_uint(r, 8);
}

/* DESIGN: a signed number is its two's complement bits. The reader
   rebuilds it by arithmetic, since C leaves the conversion of an
   unsigned value above the largest signed one to the implementation. */
int32_t antl_get_i32(struct reader *r)
{
    uint32_t u = antl_get_u32(r);

    if (u <= INT32_MAX) {
        return (int32_t)u;
    }
    return (int32_t)(u - (uint32_t)INT32_MAX - 1u) + INT32_MIN;
}

int64_t antl_get_i64(struct reader *r)
{
    uint64_t u = antl_get_u64(r);

    if (u <= INT64_MAX) {
        return (int64_t)u;
    }
    return (int64_t)(u - (uint64_t)INT64_MAX - 1u) + INT64_MIN;
}

int antl_get_int(struct reader *r)
{
    uint32_t u = antl_get_u32(r);

    if (u > INT_MAX) {
        antl_damaged(r);
        return 0;
    }
    return (int)u;
}

uint32_t antl_get_count(struct reader *r, size_t min)
{
    uint32_t n = antl_get_u32(r);

    if (!r->failed && n > (r->size - r->pos) / min) {
        antl_damaged(r);
        return 0;
    }
    return n;
}

void *antl_allocate(struct reader *r, size_t count, size_t size)
{
    return types_alloc_array(r->arena, count + 1, size);
}

struct name antl_get_name(struct reader *r)
{
    struct name n = {"", 0};
    uint32_t length = antl_get_count(r, 1);
    char *text;

    /* A name is printed with `%.*s`, whose precision is an int. */
    if (!r->failed && length > INT_MAX) {
        antl_damaged(r);
    }
    if (r->failed || !antl_take(r, length)) {
        return n;
    }
    text = antl_allocate(r, length, 1);
    memcpy(text, r->data + r->pos, length);
    r->pos += length;
    n.text = text;
    n.length = length;
    return n;
}

#include "text.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"

/* Grow to hold extra more bytes plus the terminating NUL. The compiler
   stops on an allocation failure, because it has no way to continue. */
static void reserve(struct text *t, size_t extra)
{
    size_t needed = alloc_sum(alloc_sum(t->length, extra), 1);
    size_t capacity = t->capacity == 0 ? 64 : t->capacity;

    if (needed <= t->capacity) {
        return;
    }
    while (capacity < needed) {
        capacity = alloc_product(capacity, 2);
    }
    t->data = alloc_resize(t->data, capacity, 1);
    t->capacity = capacity;
}

void text_append(struct text *t, const char *s)
{
    size_t n = strlen(s);

    reserve(t, n);
    memcpy(t->data + t->length, s, n + 1);
    t->length += n;
}

void text_append_bytes(struct text *t, const void *bytes, size_t n)
{
    reserve(t, n);
    /* An empty slice may carry a null pointer, and memcpy takes none. */
    if (n > 0) {
        memcpy(t->data + t->length, bytes, n);
    }
    t->length += n;
    t->data[t->length] = '\0';
}

void text_appendf(struct text *t, const char *format, ...)
{
    va_list args;
    int n;

    va_start(args, format);
    n = vsnprintf(NULL, 0, format, args);
    va_end(args);
    if (n < 0) {
        return;
    }
    reserve(t, (size_t)n);
    va_start(args, format);
    vsnprintf(t->data + t->length, (size_t)n + 1, format, args);
    va_end(args);
    t->length += (size_t)n;
}

const char *text_cstr(const struct text *t)
{
    return t->data == NULL ? "" : t->data;
}

void text_free(struct text *t)
{
    free(t->data);
    t->data = NULL;
    t->length = 0;
    t->capacity = 0;
}

void text_vformat(char *buffer, size_t size, const char *format,
                  va_list args)
{
    int n;

    if (size == 0) {
        return;
    }
    n = vsnprintf(buffer, size, format, args);
    if (n < 0) {
        buffer[0] = '\0';
    } else if ((size_t)n >= size && size >= 4) {
        memcpy(buffer + size - 4, "...", 4);
    }
}

void text_format(char *buffer, size_t size, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    text_vformat(buffer, size, format, args);
    va_end(args);
}

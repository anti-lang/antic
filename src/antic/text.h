#ifndef ANTIC_TEXT_H
#define ANTIC_TEXT_H

#include <stdarg.h>
#include <stddef.h>

#include "attributes.h"

/* A growable byte buffer that stays NUL-terminated. A zero-initialised
   struct text is empty and valid. */
struct text {
    char *data;
    size_t length;
    size_t capacity;
};

void text_append(struct text *t, const char *s);
void text_append_bytes(struct text *t, const void *bytes, size_t n);
void text_appendf(struct text *t, const char *format, ...)
    ATTRIBUTE_PRINTF(2, 3);
const char *text_cstr(const struct text *t);
void text_free(struct text *t);

/* Format a message into buffer of size bytes, as vsnprintf does. A
   message cut to fit ends in three dots, so a reader sees that the rest
   is missing. */
void text_vformat(char *buffer, size_t size, const char *format,
                  va_list args) ATTRIBUTE_PRINTF(3, 0);
void text_format(char *buffer, size_t size, const char *format, ...)
    ATTRIBUTE_PRINTF(3, 4);

#endif

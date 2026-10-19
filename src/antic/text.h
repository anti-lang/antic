#ifndef ANTIC_TEXT_H
#define ANTIC_TEXT_H

/* The interface of text.c: a byte buffer that grows and stays
   NUL-terminated, and the formatting of a message into a fixed buffer. */

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

/* Append the NUL-terminated s to t. */
void text_append(struct text *t, const char *s);

/* Append the n bytes at bytes to t, a NUL among them. bytes may be NULL
   when n is 0. */
void text_append_bytes(struct text *t, const void *bytes, size_t n);

/* Append the text of format and its arguments, as printf writes it, to t.
   A format that vsnprintf refuses appends nothing. */
void text_appendf(struct text *t, const char *format, ...)
    ATTRIBUTE_PRINTF(2, 3);

/* The bytes of t as a C string, and "" for a text that never grew. */
const char *text_cstr(const struct text *t);

/* Release the bytes of t and leave it empty. */
void text_free(struct text *t);

/* Format a message into buffer of size bytes, as vsnprintf does. A
   message cut to fit ends in three dots, so a reader sees that the rest
   is missing. */
void text_vformat(char *buffer, size_t size, const char *format,
                  va_list args) ATTRIBUTE_PRINTF(3, 0);

/* Format a message into buffer as text_vformat does, with the arguments in
   the call. */
void text_format(char *buffer, size_t size, const char *format, ...)
    ATTRIBUTE_PRINTF(3, 4);

/* The room of a generated name: a prefix and a suffix of 27 bytes
   together, the 20 digits of the largest size_t and the NUL. */
#define TEXT_NAME_SIZE 48

/* Format the name of a generated symbol, or another text antic writes
   into its output, into buffer of size bytes, as snprintf does. A text
   that does not fit stops antic, since a name cut to fit could equal
   another, and a cut type or line would enter the output as it stands. */
void text_format_name(char *buffer, size_t size, const char *format, ...)
    ATTRIBUTE_PRINTF(3, 4);

#endif

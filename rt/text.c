#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "std.h"

struct anti_text anti_rt_text_from_c(const unsigned char *bytes)
{
    struct anti_text text;

    text.ptr = bytes;
    text.len = (int64_t)strlen((const char *)bytes);
    return text;
}

struct anti_text anti_rt_text_slice(const unsigned char *bytes, int64_t len)
{
    struct anti_text text;

    text.ptr = bytes;
    text.len = len;
    return text;
}

/* Make room for count more bytes and the NUL after them. Returns false
   when the memory runs out, and the builder keeps what it holds. */
static int reserve(struct anti_builder *b, int64_t count)
{
    int64_t size;
    unsigned char *room;

    if (b->length + count + 1 <= b->capacity) {
        return 1;
    }
    size = b->capacity == 0 ? 32 : b->capacity;
    while (size < b->length + count + 1) {
        size *= 2;
    }
    room = malloc((size_t)size);
    if (room == NULL) {
        return 0;
    }
    if (b->room != NULL) {
        memcpy(room, b->room, (size_t)b->length);
        free(b->room);
    }
    b->room = room;
    b->capacity = size;
    return 1;
}

/* DESIGN: the buffer of `anti.text.Builder` lives here. The default
   `serialize` of the root class writes an object into one, and the
   runtime holds that body. The class is a face over these functions, so
   one buffer serves both sides. A mismatch of the layout breaks the
   builder test at once. */
void anti_rt_builder_append(struct anti_builder *b,
                            const unsigned char *bytes, int64_t len)
{
    if (len <= 0 || !reserve(b, len)) {
        return;
    }
    memcpy(b->room + b->length, bytes, (size_t)len);
    b->length += len;
    b->room[b->length] = 0;
}

void anti_rt_builder_fill(struct anti_builder *b, int64_t at, int64_t byte,
                          int64_t count)
{
    if (count <= 0 || at < 0 || at > b->length || !reserve(b, count)) {
        return;
    }
    memmove(b->room + at + count, b->room + at, (size_t)(b->length - at));
    memset(b->room + at, (int)(byte & 0xFF), (size_t)count);
    b->length += count;
    b->room[b->length] = 0;
}

/* DESIGN: the bytes an `f"..."` gives belong to the program, which frees
   them with `free(s.ptr)`. An empty builder has no memory, so it takes
   one byte for the NUL, and every text it gives can be freed alike. */
struct anti_text anti_rt_builder_take(struct anti_builder *b)
{
    struct anti_text text;
    unsigned char *room = b->room;

    if (room == NULL) {
        room = malloc(1);
        if (room == NULL) {
            text.ptr = (const unsigned char *)"";
            text.len = 0;
            return text;
        }
        room[0] = 0;
    }
    text.ptr = room;
    text.len = b->length;
    b->room = NULL;
    b->length = 0;
    b->capacity = 0;
    return text;
}

/* The fewest digits of value in the form of `%e` that read back as the
   same value, as a float when single is set. Seventeen always do. */
static void shortest_digits(double value, int single, char *out, size_t size)
{
    int digits;

    for (digits = 1; digits < 17; digits++) {
        snprintf(out, size, "%.*e", digits - 1, value);
        if (single ? strtof(out, NULL) == (float)value
                   : strtod(out, NULL) == value) {
            return;
        }
    }
    snprintf(out, size, "%.*e", 16, value);
}

/* DESIGN: a float without a precision is written with the fewest digits
   that read back as the same value. It stands in positional form when
   the exponent of its first digit lies from -4 to 15, and in the form
   `d.ddde+XX` otherwise or when exponent is set. The C library finds the
   digits and the code below lays them out, so every target writes the
   same text. */
static void put_shortest(struct anti_builder *b, double value, int exponent,
                         int single)
{
    char scientific[40];
    char digits[24];
    char out[64];
    const char *p;
    int count = 0;
    int power;
    int n = 0;
    int i;

    shortest_digits(value, single, scientific, sizeof scientific);
    p = scientific;
    if (*p == '-') {
        out[n++] = '-';
        p++;
    }
    for (; *p != 'e' && *p != '\0' && count < 20; p++) {
        if (*p != '.') {
            digits[count++] = *p;
        }
    }
    power = *p == 'e' ? atoi(p + 1) : 0;
    while (count > 1 && digits[count - 1] == '0') {
        count--;
    }
    if (!exponent && power >= -4 && power < 16) {
        if (power < 0) {
            out[n++] = '0';
            out[n++] = '.';
            for (i = -1; i > power; i--) {
                out[n++] = '0';
            }
            for (i = 0; i < count; i++) {
                out[n++] = digits[i];
            }
        } else {
            for (i = 0; i <= power; i++) {
                out[n++] = i < count ? digits[i] : '0';
            }
            if (count > power + 1) {
                out[n++] = '.';
                for (i = power + 1; i < count; i++) {
                    out[n++] = digits[i];
                }
            }
        }
    } else {
        out[n++] = digits[0];
        if (count > 1) {
            out[n++] = '.';
            for (i = 1; i < count; i++) {
                out[n++] = digits[i];
            }
        }
        n += snprintf(out + n, sizeof out - (size_t)n, "e%c%02d",
                      power < 0 ? '-' : '+', power < 0 ? -power : power);
    }
    anti_rt_builder_append(b, (const unsigned char *)out, n);
}

/* value with precision digits after the point, in the form of `%e` or
   of `%f`. */
static int print_fixed(char *out, size_t size, double value, int precision,
                       int exponent)
{
    return exponent ? snprintf(out, size, "%.*e", precision, value)
                    : snprintf(out, size, "%.*f", precision, value);
}

/* C writes at least two digits of an exponent, and the C library of old
   Windows three. The text keeps two at least, as C asks, so that every
   target writes the same. Returns the new length. */
static int two_digit_exponent(char *text, int length)
{
    char *e = memchr(text, 'e', (size_t)length);
    char *end = text + length;
    char *digits;

    if (e == NULL || end - e < 3) {
        return length;
    }
    digits = e + 2;
    while (end - digits > 2 && digits[0] == '0') {
        memmove(digits, digits + 1, (size_t)(end - digits - 1));
        end--;
    }
    return (int)(end - text);
}

void anti_rt_builder_float(struct anti_builder *b, double value,
                           int64_t precision, int64_t exponent, int64_t single)
{
    char small[64];
    char *text = small;
    int length;

    if (isnan(value)) {
        anti_rt_builder_append(b, (const unsigned char *)"nan", 3);
        return;
    }
    if (isinf(value)) {
        anti_rt_builder_append(b, (const unsigned char *)(value < 0 ? "-inf"
                                                                    : "inf"),
                               value < 0 ? 4 : 3);
        return;
    }
    if (precision < 0) {
        put_shortest(b, value, exponent != 0, single != 0);
        return;
    }
    if (precision > 100000000) {
        precision = 100000000;
    }
    length = print_fixed(small, sizeof small, value, (int)precision,
                         exponent != 0);
    if (length < 0) {
        return;
    }
    if ((size_t)length >= sizeof small) {
        text = malloc((size_t)length + 1);
        if (text == NULL) {
            return;
        }
        print_fixed(text, (size_t)length + 1, value, (int)precision,
                    exponent != 0);
    }
    if (exponent != 0) {
        length = two_digit_exponent(text, length);
    }
    anti_rt_builder_append(b, (const unsigned char *)text, length);
    if (text != small) {
        free(text);
    }
}

struct anti_text anti_rt_builder_text(const struct anti_builder *b)
{
    struct anti_text text;

    text.ptr = b->room == NULL ? (const unsigned char *)"" : b->room;
    text.len = b->length;
    return text;
}

void anti_rt_builder_clear(struct anti_builder *b)
{
    b->length = 0;
    if (b->room != NULL) {
        b->room[0] = 0;
    }
}

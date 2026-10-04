/* A string builder, the C twin of builder.anti. One buffer collects 50000
   entries of a word, a number and a comma, and is cleared and filled again
   200 times. The buffer keeps one byte past its length for a NUL and
   doubles from 32 bytes, as the runtime's does. A number is written as
   append_int of anti.text writes it: the digits gathered from the end of
   an array, then appended one byte at a time. */

#include "../binary_stdio.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct builder {
    unsigned char *room;
    int64_t length;
    int64_t capacity;
};

static void reserve(struct builder *b, int64_t count)
{
    int64_t need = b->length + count + 1;
    if (need <= b->capacity) {
        return;
    }
    int64_t capacity = b->capacity < 32 ? 32 : b->capacity;
    while (capacity < need) {
        capacity *= 2;
    }
    unsigned char *room = realloc(b->room, (size_t)capacity);
    if (room == NULL) {
        exit(1);
    }
    b->room = room;
    b->capacity = capacity;
}

static void append(struct builder *b, const char *bytes, int64_t len)
{
    reserve(b, len);
    memcpy(b->room + b->length, bytes, (size_t)len);
    b->length += len;
    b->room[b->length] = 0;
}

static void append_byte(struct builder *b, unsigned char c)
{
    append(b, (const char *)&c, 1);
}

static void put_digits(struct builder *b, uint64_t value)
{
    unsigned char digits[64];
    int at = 64;
    uint64_t n = value;
    do {
        at -= 1;
        digits[at] = (unsigned char)('0' + n % 10);
        n /= 10;
    } while (n != 0);
    while (at < 64) {
        append_byte(b, digits[at]);
        at += 1;
    }
}

static void append_int(struct builder *b, int64_t value)
{
    if (value < 0) {
        append_byte(b, '-');
        put_digits(b, (uint64_t)(-(value + 1)) + 1);
    } else {
        put_digits(b, (uint64_t)value);
    }
}

int main(void)
{
    struct builder b = {NULL, 0, 0};
    uint64_t total = 0;
    for (int64_t r = 0; r < 200; r++) {
        b.length = 0;
        if (b.room != NULL) {
            b.room[0] = 0;
        }
        for (int64_t i = 0; i < 50000; i++) {
            append(&b, "item ", 5);
            append_int(&b, i * 7 - r * 1000);
            append_byte(&b, ',');
        }
        total += (uint64_t)b.length;
    }
    printf("%lld\n", (long long)total);
    free(b.room);
    return 0;
}

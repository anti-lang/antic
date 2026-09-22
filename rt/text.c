#include <math.h>
#include <stdbool.h>
#include <stdint.h>
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

/* DESIGN: the digits of a float come from its exact value and never from
   the C library. printf rounds a tie by the rules of its own version. The
   C runtime of Windows before 10 2004 writes 0.25 at one digit as 0.3,
   and every other one writes 0.2. A double is m * 2^e with an integer m,
   so its decimal form is finite. The code below finds every digit of it
   with integers. Rounding reads the exact remainder, and a tie goes to
   the even digit, as IEEE 754 asks. */

/* The most digits an exact value below takes: m * 5^1075 with m below
   2^54, the half-way point above the smallest double. */
#define EXACT_DIGITS 780
#define BIG_LIMBS ((EXACT_DIGITS + 8) / 9)
#define BILLION 1000000000u

/* A non-negative integer in base 10^9, the lowest limb first. */
struct big {
    uint32_t limb[BIG_LIMBS];
    int count;
};

/* A value as the digits of d after a decimal point, times 10^point. The
   first digit is never 0 and the last one never 0. No digits at all is
   zero. */
struct decimal {
    char d[EXACT_DIGITS];
    int64_t count;
    int64_t point;
};

/* The sign, the integer m and the power e of a double, or of the float
   it holds when single is set. boundary is set when m is the smallest of
   its binade, so the value below lies half as far as the one above. */
struct parts {
    bool negative;
    bool boundary;
    uint64_t m;
    int e;
};

static struct parts parts_of(double value, bool single)
{
    struct parts p;

    if (single) {
        float f = (float)value;
        uint32_t bits;
        uint32_t power;
        uint32_t fraction;
        memcpy(&bits, &f, sizeof bits);
        power = (bits >> 23) & 0xFF;
        fraction = bits & 0x7FFFFF;
        p.negative = (bits >> 31) != 0;
        p.m = power == 0 ? fraction : fraction | 0x800000;
        p.e = power == 0 ? -149 : (int)power - 150;
        p.boundary = fraction == 0 && power > 1;
    } else {
        uint64_t bits;
        uint64_t power;
        uint64_t fraction;
        memcpy(&bits, &value, sizeof bits);
        power = (bits >> 52) & 0x7FF;
        fraction = bits & 0xFFFFFFFFFFFFFull;
        p.negative = (bits >> 63) != 0;
        p.m = power == 0 ? fraction : fraction | 0x10000000000000ull;
        p.e = power == 0 ? -1074 : (int)power - 1075;
        p.boundary = fraction == 0 && power > 1;
    }
    return p;
}

static void big_multiply(struct big *b, uint32_t k)
{
    uint64_t carry = 0;
    int i;

    for (i = 0; i < b->count; i++) {
        uint64_t x = (uint64_t)b->limb[i] * k + carry;
        b->limb[i] = (uint32_t)(x % BILLION);
        carry = x / BILLION;
    }
    while (carry != 0 && b->count < BIG_LIMBS) {
        b->limb[b->count++] = (uint32_t)(carry % BILLION);
        carry /= BILLION;
    }
}

/* The digits of m * 2^e. A negative e multiplies by 5^-e instead and
   moves the point e places to the left. */
static void exact(uint64_t m, int e, struct decimal *out)
{
    struct big b;
    uint32_t top;
    char high[9];
    int64_t n = 0;
    int left;
    int i;

    b.count = 0;
    for (; m != 0; m /= BILLION) {
        b.limb[b.count++] = (uint32_t)(m % BILLION);
    }
    for (left = e; left >= 29; left -= 29) {
        big_multiply(&b, 1u << 29);
    }
    if (left > 0) {
        big_multiply(&b, 1u << left);
    }
    /* 5^13 is the largest power of 5 below 2^32. */
    for (left = -e; left >= 13; left -= 13) {
        big_multiply(&b, 1220703125u);
    }
    if (left > 0) {
        uint32_t five = 1;
        for (; left > 0; left--) {
            five *= 5;
        }
        big_multiply(&b, five);
    }
    out->count = 0;
    out->point = 0;
    if (b.count == 0) {
        return;
    }
    left = 0;
    for (top = b.limb[b.count - 1]; top != 0; top /= 10) {
        high[left++] = (char)('0' + top % 10);
    }
    while (left > 0) {
        out->d[n++] = high[--left];
    }
    for (i = b.count - 2; i >= 0; i--) {
        uint32_t v = b.limb[i];
        int k;
        for (k = 8; k >= 0; k--) {
            out->d[n + k] = (char)('0' + v % 10);
            v /= 10;
        }
        n += 9;
    }
    out->point = n + (e < 0 ? e : 0);
    while (n > 0 && out->d[n - 1] == '0') {
        n--;
    }
    out->count = n;
}

/* Keep the first keep digits of v and round the rest away, a tie to the
   even digit. Below one digit the value rounds to zero or to one unit of
   the place before its first digit. */
static void round_to(struct decimal *v, int64_t keep)
{
    int64_t i;
    bool up;

    if (v->count == 0 || keep >= v->count) {
        return;
    }
    if (keep < 0) {
        v->count = 0;
        return;
    }
    if (v->d[keep] != '5') {
        up = v->d[keep] > '5';
    } else {
        /* The digits of v end in no 0, so a 5 that is not the last digit
           has more after it and lies above the tie. */
        up = keep + 1 < v->count ||
             (keep > 0 && (v->d[keep - 1] - '0') % 2 == 1);
    }
    v->count = keep;
    if (up) {
        i = keep - 1;
        while (i >= 0 && v->d[i] == '9') {
            i--;
        }
        if (i < 0) {
            v->d[0] = '1';
            v->count = 1;
            v->point++;
        } else {
            v->d[i]++;
            v->count = i + 1;
        }
    }
    while (v->count > 0 && v->d[v->count - 1] == '0') {
        v->count--;
    }
}

/* Compare two values that are not zero, as -1, 0 or 1. */
static int compare(const struct decimal *a, const struct decimal *b)
{
    int64_t n = a->count > b->count ? a->count : b->count;
    int64_t i;

    if (a->point != b->point) {
        return a->point < b->point ? -1 : 1;
    }
    for (i = 0; i < n; i++) {
        char x = i < a->count ? a->d[i] : '0';
        char y = i < b->count ? b->d[i] : '0';
        if (x != y) {
            return x < y ? -1 : 1;
        }
    }
    return 0;
}

/* The fewest digits of p that read back as the same value. They are the
   first count that rounds into the interval between the half-way points
   to the neighbours. A reader rounds a tie to the even value, so the ends
   belong to the interval when m is even. Seventeen always do. */
static void shortest(const struct parts *p, struct decimal *out)
{
    struct decimal value;
    struct decimal low;
    struct decimal high;
    bool even = p->m % 2 == 0;
    int64_t digits;

    exact(p->m, p->e, &value);
    exact(2 * p->m + 1, p->e - 1, &high);
    if (p->boundary) {
        exact(4 * p->m - 1, p->e - 2, &low);
    } else {
        exact(2 * p->m - 1, p->e - 1, &low);
    }
    for (digits = 1; digits < 17; digits++) {
        int below;
        int above;
        *out = value;
        round_to(out, digits);
        below = compare(out, &low);
        above = compare(out, &high);
        if ((below > 0 || (even && below == 0)) &&
            (above < 0 || (even && above == 0))) {
            return;
        }
    }
    *out = value;
    round_to(out, 17);
}

/* Bytes on their way to a builder, handed over a block at a time. */
struct sink {
    struct anti_builder *b;
    unsigned char room[128];
    int64_t used;
};

static void flush(struct sink *s)
{
    anti_rt_builder_append(s->b, s->room, s->used);
    s->used = 0;
}

static void put(struct sink *s, char c)
{
    if (s->used == (int64_t)sizeof s->room) {
        flush(s);
    }
    s->room[s->used++] = (unsigned char)c;
}

/* The digit of v at index i, which is 0 outside its digits. */
static char digit_at(const struct decimal *v, int64_t i)
{
    return i >= 0 && i < v->count ? v->d[i] : '0';
}

/* `e`, the sign of power and at least two of its digits, as C writes
   them. */
static void put_power(struct sink *s, int64_t power)
{
    char digits[24];
    int n = 0;
    int64_t size = power < 0 ? -power : power;

    put(s, 'e');
    put(s, power < 0 ? '-' : '+');
    for (; size != 0 || n < 2; size /= 10) {
        digits[n++] = (char)('0' + size % 10);
    }
    while (n > 0) {
        put(s, digits[--n]);
    }
}

/* v in the form of `%.*f`, precision digits after the point. */
static void put_fixed(struct sink *s, const struct decimal *v,
                      int64_t precision)
{
    int64_t i;

    if (v->count == 0 || v->point <= 0) {
        put(s, '0');
    } else {
        for (i = 0; i < v->point; i++) {
            put(s, digit_at(v, i));
        }
    }
    if (precision > 0) {
        put(s, '.');
        for (i = 0; i < precision; i++) {
            put(s, v->count == 0 ? '0' : digit_at(v, v->point + i));
        }
    }
}

/* v in the form of `%.*e`, precision digits after the point. */
static void put_scientific(struct sink *s, const struct decimal *v,
                           int64_t precision)
{
    int64_t i;

    put(s, v->count == 0 ? '0' : v->d[0]);
    if (precision > 0) {
        put(s, '.');
        for (i = 1; i <= precision; i++) {
            put(s, digit_at(v, i));
        }
    }
    put_power(s, v->count == 0 ? 0 : v->point - 1);
}

/* DESIGN: a float without a precision is written with the fewest digits
   that read back as the same value. It stands in positional form when
   the exponent of its first digit lies from -4 to 15, and in the form
   `d.ddde+XX` otherwise or when exponent is set. */
static void put_shortest(struct sink *s, const struct decimal *v,
                         bool exponent)
{
    int64_t power = v->count == 0 ? 0 : v->point - 1;
    int64_t places = v->count - v->point;

    if (!exponent && power >= -4 && power < 16) {
        put_fixed(s, v, places > 0 ? places : 0);
    } else {
        put_scientific(s, v, v->count > 1 ? v->count - 1 : 0);
    }
}

void anti_rt_builder_float(struct anti_builder *b, double value,
                           int64_t precision, int64_t exponent, int64_t single)
{
    struct sink s;
    struct decimal v;
    struct parts p;

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
    s.b = b;
    s.used = 0;
    p = parts_of(value, precision < 0 && single != 0);
    if (p.negative) {
        put(&s, '-');
    }
    if (precision < 0) {
        if (p.m == 0) {
            v.count = 0;
            v.point = 0;
        } else {
            shortest(&p, &v);
        }
        put_shortest(&s, &v, exponent != 0);
    } else {
        if (precision > 100000000) {
            precision = 100000000;
        }
        exact(p.m, p.e, &v);
        if (exponent != 0) {
            round_to(&v, precision + 1);
            put_scientific(&s, &v, precision);
        } else {
            round_to(&v, v.point + precision);
            put_fixed(&s, &v, precision);
        }
    }
    flush(&s);
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

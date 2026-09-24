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

int anti_rt_same_bytes(const unsigned char *a, int64_t a_length,
                       const unsigned char *b, int64_t b_length)
{
    return a_length == b_length &&
           (a_length == 0 || memcmp(a, b, (size_t)a_length) == 0);
}

/* Make room for count more bytes and the NUL after them. Returns false
   when the memory runs out or no size holds them, and the builder keeps
   what it holds. */
static int reserve(struct anti_builder *b, int64_t count)
{
    int64_t need;
    int64_t size;
    unsigned char *room;

    if (count > INT64_MAX - 1 - b->length) {
        return 0;
    }
    need = b->length + count + 1;
    if (need <= b->capacity) {
        return 1;
    }
    size = b->capacity == 0 ? 32 : b->capacity;
    while (size < need) {
        size = size > INT64_MAX / 2 ? need : size * 2;
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

/* DESIGN: a float is read from its text by exact arithmetic on its
   digits as well, and never by strtod. Halving and doubling the decimal
   bring its value into [1/2, 1) and count the powers of two. Doubling it by
   the bits of the mantissa then leaves the mantissa as the whole part.
   The digits after the point round it, a tie to the even value. The
   width is a parameter, so an f32 and an f16 round once, from the text,
   and never twice through an f64. Each step is exact except for digits
   past EXACT_DIGITS, which are dropped and remembered. A half-way point
   between two doubles has at most 769 digits. Every value on the way
   therefore stays at or above the image of each such point below the
   value read. A drop never moves a value across one, and a remembered
   drop turns a tie into a value above it. */

/* The bits that one halving or doubling moves at most, so that a digit
   and the carry above it fit in 64 bits. */
#define SHIFT_LIMIT 27

static void trim(struct decimal *v)
{
    while (v->count > 0 && v->d[v->count - 1] == '0') {
        v->count--;
    }
}

/* v times 2^k, for k up to SHIFT_LIMIT. The lowest digits past
   EXACT_DIGITS are dropped. */
static void double_by(struct decimal *v, int k, bool *dropped)
{
    char out[EXACT_DIGITS + 10];
    int64_t n = (int64_t)sizeof out;
    int64_t count;
    uint64_t carry = 0;
    int64_t i;

    for (i = v->count - 1; i >= 0; i--) {
        uint64_t x = ((uint64_t)(v->d[i] - '0') << k) + carry;
        out[--n] = (char)('0' + x % 10);
        carry = x / 10;
    }
    for (; carry != 0; carry /= 10) {
        out[--n] = (char)('0' + carry % 10);
    }
    count = (int64_t)sizeof out - n;
    v->point += count - v->count;
    for (i = EXACT_DIGITS; i < count; i++) {
        if (out[n + i] != '0') {
            *dropped = true;
        }
    }
    if (count > EXACT_DIGITS) {
        count = EXACT_DIGITS;
    }
    memcpy(v->d, out + n, (size_t)count);
    v->count = count;
    trim(v);
}

/* v divided by 2^k, for k up to SHIFT_LIMIT, by long division from the
   first digit. The digits past EXACT_DIGITS are dropped. */
static void halve_by(struct decimal *v, int k, bool *dropped)
{
    uint64_t mask = ((uint64_t)1 << k) - 1;
    uint64_t r = 0;
    int64_t read = 0;
    int64_t write = 0;

    if (v->count == 0) {
        return;
    }
    /* Read until the first digit of the quotient is not 0. Past the last
       digit the dividend goes on in zeros. */
    while ((r >> k) == 0) {
        r = r * 10 + (read < v->count ? (uint64_t)(v->d[read] - '0') : 0);
        read++;
    }
    v->point -= read - 1;
    for (; read < v->count; read++) {
        v->d[write++] = (char)('0' + (r >> k));
        r = (r & mask) * 10 + (uint64_t)(v->d[read] - '0');
    }
    for (; r != 0; r = (r & mask) * 10) {
        if (write < EXACT_DIGITS) {
            v->d[write++] = (char)('0' + (r >> k));
        } else if ((r >> k) != 0) {
            *dropped = true;
        }
    }
    v->count = write;
    trim(v);
}

/* The whole part of v, below 2^63, rounded by the digits after its
   point. A tie goes to the even value unless digits were dropped, which
   put the value above the tie. */
static uint64_t rounded_whole(const struct decimal *v, bool dropped)
{
    uint64_t n = 0;
    int64_t i;
    bool up;

    for (i = 0; i < v->point; i++) {
        n = n * 10 + (uint64_t)(digit_at(v, i) - '0');
    }
    if (v->point < 0 || v->point >= v->count) {
        return n;
    }
    if (v->d[v->point] == '5' && v->point + 1 == v->count) {
        up = dropped || n % 2 == 1;
    } else {
        up = v->d[v->point] >= '5';
    }
    return up ? n + 1 : n;
}

int anti_rt_read_float(const unsigned char *bytes, int64_t length,
                       int mantissa, int exponent, uint64_t *bits)
{
    /* Entry n is the largest power of 2 below 10^n. It is the most that
       one step moves a value with n digits before its point. */
    static const int below_ten[] = {1, 3, 6, 9, 13, 16, 19, 23, 26};
    int bias = (1 << (exponent - 1)) - 1;
    uint64_t sign = 0;
    uint64_t infinite;
    uint64_t whole;
    struct decimal v;
    bool dropped = false;
    bool digits = false;
    bool after_point = false;
    int64_t scale = 0;
    int64_t power = 0;
    int64_t i = 0;

    infinite = (((uint64_t)1 << exponent) - 1) << mantissa;
    if (i < length && (bytes[i] == '+' || bytes[i] == '-')) {
        if (bytes[i] == '-') {
            sign = (uint64_t)1 << (mantissa + exponent);
        }
        i++;
    }
    v.count = 0;
    v.point = 0;
    for (; i < length; i++) {
        unsigned char c = bytes[i];
        if (c == '.' && !after_point) {
            after_point = true;
            continue;
        }
        if (c < '0' || c > '9') {
            break;
        }
        digits = true;
        if (c == '0' && v.count == 0) {
            v.point -= after_point ? 1 : 0;
            continue;
        }
        v.point += after_point ? 0 : 1;
        if (v.count < EXACT_DIGITS) {
            v.d[v.count++] = (char)c;
        } else if (c != '0') {
            dropped = true;
        }
    }
    if (!digits) {
        return 0;
    }
    if (i < length && (bytes[i] == 'e' || bytes[i] == 'E')) {
        bool minus = false;
        bool any = false;
        i++;
        if (i < length && (bytes[i] == '+' || bytes[i] == '-')) {
            minus = bytes[i] == '-';
            i++;
        }
        for (; i < length && bytes[i] >= '0' && bytes[i] <= '9'; i++) {
            any = true;
            /* Past 100000 the value is zero or infinite whatever
               follows. */
            if (scale < 100000) {
                scale = scale * 10 + (bytes[i] - '0');
            }
        }
        if (!any) {
            return 0;
        }
        v.point += minus ? -scale : scale;
    }
    if (i != length) {
        return 0;
    }
    trim(&v);
    /* Below 10^-330 every width rounds to zero, and from 10^310 on each
       one is infinite. */
    if (v.count == 0 || v.point < -330) {
        *bits = sign;
        return 1;
    }
    if (v.point > 310) {
        *bits = sign | infinite;
        return 1;
    }
    while (v.point > 0) {
        int k = v.point < 9 ? below_ten[v.point] : SHIFT_LIMIT;
        halve_by(&v, k, &dropped);
        power += k;
    }
    while (v.point < 0 || (v.point == 0 && v.d[0] < '5')) {
        int k = -v.point < 9 ? below_ten[-v.point] : SHIFT_LIMIT;
        double_by(&v, k, &dropped);
        power -= k;
    }
    /* v lies in [1/2, 1), so the first bit of the value is worth
       2^(power - 1). Below the smallest normal exponent the value is
       halved until it stands at that exponent with a first bit of 0. */
    power--;
    if (power < 1 - bias) {
        int64_t n;
        for (n = 1 - bias - power; n > 0; n -= SHIFT_LIMIT) {
            halve_by(&v, n < SHIFT_LIMIT ? (int)n : SHIFT_LIMIT, &dropped);
        }
        power = 1 - bias;
    }
    if (power > bias) {
        *bits = sign | infinite;
        return 1;
    }
    for (i = mantissa + 1; i > 0; i -= SHIFT_LIMIT) {
        double_by(&v, i < SHIFT_LIMIT ? (int)i : SHIFT_LIMIT, &dropped);
    }
    whole = rounded_whole(&v, dropped);
    /* Rounding up can carry into a new first bit. */
    if (whole == (uint64_t)2 << mantissa) {
        whole >>= 1;
        power++;
        if (power > bias) {
            *bits = sign | infinite;
            return 1;
        }
    }
    /* Without its first bit the value is subnormal, and the exponent
       field is 0. */
    if ((whole & ((uint64_t)1 << mantissa)) == 0) {
        *bits = sign | whole;
    } else {
        *bits = sign | ((uint64_t)(power + bias) << mantissa) |
                (whole & (((uint64_t)1 << mantissa) - 1));
    }
    return 1;
}

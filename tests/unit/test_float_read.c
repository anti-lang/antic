/* rt/text.c holds the reader of the text of a float, and
   Object.deserialize reads a float field with it. A text rounds to the
   nearest float of the width it is read at, a tie to the even one. Every
   text the writer of rt/text.c gives reads back as the same bits. The
   exact half-way point between two neighbours reads as the even one of
   them. Moved by one unit of its last digit, it reads as the neighbour it
   moved towards. */

#include "../binary_stdio.h"
#include "check.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "f16.h"
#include "std.h"

/* Digits after the point that hold every digit of the exact value of a
   double, of a float and of a half. A half-way point takes one more. */
#define DOUBLE_PLACES 1075
#define SINGLE_PLACES 150
#define HALF_PLACES 25

/* splitmix64 with a fixed seed, so a failure repeats. */
static uint64_t next_random(uint64_t *state)
{
    uint64_t z = (*state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

static double double_of(uint64_t b)
{
    double v;
    memcpy(&v, &b, sizeof v);
    return v;
}

static float single_of(uint32_t b)
{
    float v;
    memcpy(&v, &b, sizeof v);
    return v;
}

/* The bits text reads as, at mantissa and exponent bits, or all ones
   when the reader refuses it. */
static uint64_t read_as(const char *text, int mantissa, int exponent)
{
    uint64_t bits = 0;

    if (!anti_rt_read_float((const unsigned char *)text,
                            (int64_t)strlen(text), mantissa, exponent,
                            &bits)) {
        return UINT64_MAX;
    }
    return bits;
}

/* What the writer gives for v, as a C string the caller frees. */
static char *written(double v, int64_t precision, int64_t exponent,
                     int64_t single)
{
    struct anti_builder b = {0};
    struct anti_text t;
    char *s;

    anti_rt_builder_float(&b, v, precision, exponent, single);
    t = anti_rt_builder_text(&b);
    s = malloc((size_t)t.len + 1);
    memcpy(s, t.ptr, (size_t)t.len);
    s[t.len] = '\0';
    free(b.room);
    return s;
}

/* The exact decimal of the half-way point between two non-negative
   texts that each hold places digits after the point. */
static char *halfway(const char *a, const char *b, size_t places)
{
    size_t ia = strcspn(a, ".");
    size_t ib = strcspn(b, ".");
    size_t whole = (ia > ib ? ia : ib) + 1;
    size_t count = whole + places + 1;
    unsigned char *sum = calloc(count, 1);
    char *out = malloc(count + 2);
    unsigned carry = 0;
    unsigned rest = 0;
    size_t i;
    size_t n = 0;
    size_t start;

    for (i = 0; i < ia; i++) {
        sum[whole - ia + i] += (unsigned char)(a[i] - '0');
    }
    for (i = 0; i < ib; i++) {
        sum[whole - ib + i] += (unsigned char)(b[i] - '0');
    }
    for (i = 0; i < places; i++) {
        sum[whole + i] = (unsigned char)(sum[whole + i] + (a[ia + 1 + i] - '0') +
                                         (b[ib + 1 + i] - '0'));
    }
    for (i = count; i-- > 0;) {
        unsigned x = sum[i] + carry;
        sum[i] = (unsigned char)(x % 10);
        carry = x / 10;
    }
    for (i = 0; i < count; i++) {
        unsigned x = rest * 10 + sum[i];
        sum[i] = (unsigned char)(x / 2);
        rest = x % 2;
    }
    for (start = 0; start + 1 < whole && sum[start] == 0; start++) {
    }
    for (i = start; i < whole; i++) {
        out[n++] = (char)('0' + sum[i]);
    }
    out[n++] = '.';
    for (i = whole; i < count; i++) {
        out[n++] = (char)('0' + sum[i]);
    }
    out[n] = '\0';
    free(sum);
    return out;
}

/* text moved down by one unit of its last digit. text is above zero. */
static char *below(const char *text)
{
    size_t n = strlen(text);
    char *out = malloc(n + 1);
    size_t i = n;

    memcpy(out, text, n + 1);
    while (i-- > 0) {
        if (out[i] == '.') {
            continue;
        }
        if (out[i] != '0') {
            out[i]--;
            break;
        }
        out[i] = '9';
    }
    return out;
}

/* text moved up by a unit of a digit past its last one. */
static char *above(const char *text)
{
    size_t n = strlen(text);
    char *out = malloc(n + 2);

    memcpy(out, text, n);
    out[n] = '1';
    out[n + 1] = '\0';
    return out;
}

/* The cases that decide a rounding, as the value Python's Fraction
   rounds each to. */
static void hard_cases(void)
{
    CHECK(read_as("9007199254740993", 52, 11) == 0x4340000000000000ull);
    CHECK(read_as("9007199254740993.000000000000000000000000001", 52, 11) ==
          0x4340000000000001ull);
    CHECK(read_as("9007199254740992.99999999999999999999", 52, 11) ==
          0x4340000000000000ull);
    CHECK(read_as("2.4703282292062327e-324", 52, 11) == 0);
    CHECK(read_as("2.4703282292062328e-324", 52, 11) == 1);
    CHECK(read_as("4.9406564584124654e-324", 52, 11) == 1);
    CHECK(read_as("1.7976931348623157e308", 52, 11) == 0x7FEFFFFFFFFFFFFFull);
    CHECK(read_as("1.7976931348623158e308", 52, 11) == 0x7FEFFFFFFFFFFFFFull);
    CHECK(read_as("1.7976931348623159e308", 52, 11) == 0x7FF0000000000000ull);
    CHECK(read_as("1.00000000000000011102230246251565404236316680908203125",
                  52, 11) == 0x3FF0000000000000ull);
    CHECK(read_as("1.000000000000000111022302462515654042363166809082031251",
                  52, 11) == 0x3FF0000000000001ull);
    CHECK(read_as("1e23", 52, 11) == 0x44B52D02C7E14AF6ull);
    CHECK(read_as("2.2250738585072011e-308", 52, 11) == 0x000FFFFFFFFFFFFFull);
    CHECK(read_as("2.2250738585072012e-308", 52, 11) == 0x0010000000000000ull);
    CHECK(read_as("0.1", 52, 11) == 0x3FB999999999999Aull);
    CHECK(read_as("-0", 52, 11) == 0x8000000000000000ull);
    CHECK(read_as("-0.0e5", 52, 11) == 0x8000000000000000ull);
    CHECK(read_as("1e-400", 52, 11) == 0);
    CHECK(read_as("1e400", 52, 11) == 0x7FF0000000000000ull);
    CHECK(read_as("-1e99999999999999999999", 52, 11) == 0xFFF0000000000000ull);
    CHECK(read_as("1e-99999999999999999999", 52, 11) == 0);
    CHECK(read_as("0.000000000000000000000000000000000000000000001e45", 52,
                  11) == 0x3FF0000000000000ull);
    CHECK(read_as("123456789012345678901234567890e-10", 52, 11) ==
          0x43E56A95319D63E1ull);
    CHECK(read_as(".5", 52, 11) == 0x3FE0000000000000ull);
    CHECK(read_as("5.", 52, 11) == 0x4014000000000000ull);
    CHECK(read_as("+1.5E+2", 52, 11) == 0x4062C00000000000ull);
    CHECK(read_as("-2.5e-3", 52, 11) == 0xBF647AE147AE147Bull);

    /* Through an f64, the first text of each width below meets the tie
       between two neighbours and rounds up. */
    CHECK(read_as("1.00000017881393432617187499", 23, 8) == 0x3F800001);
    CHECK(read_as("1.000000178813934326171875", 23, 8) == 0x3F800002);
    CHECK(read_as("3.4028235e38", 23, 8) == 0x7F7FFFFF);
    CHECK(read_as("3.4028236e38", 23, 8) == 0x7F800000);
    CHECK(read_as("1.4e-45", 23, 8) == 1);
    CHECK(read_as("7e-46", 23, 8) == 0);
    CHECK(read_as("7.1e-46", 23, 8) == 1);
    CHECK(read_as("0.1", 23, 8) == 0x3DCCCCCD);
    CHECK(read_as("16777217", 23, 8) == 0x4B800000);

    CHECK(read_as("1.00146484374999999", 10, 5) == 0x3C01);
    CHECK(read_as("1.00146484375", 10, 5) == 0x3C02);
    CHECK(read_as("65504", 10, 5) == 0x7BFF);
    CHECK(read_as("65519.99", 10, 5) == 0x7BFF);
    CHECK(read_as("65520", 10, 5) == 0x7C00);
    CHECK(read_as("5.96e-8", 10, 5) == 1);
    CHECK(read_as("2.98e-8", 10, 5) == 0);
    CHECK(read_as("2.99e-8", 10, 5) == 1);
    CHECK(read_as("0.1", 10, 5) == 0x2E66);
}

/* A text that is not a number is refused whole. */
static void refusals(void)
{
    static const char *const bad[] = {
        "", "-", "+", ".", "-.", "e5", ".e5", "1e", "1e+", "1e-", "1..2",
        "1.2.3", "--1", "+-1", "1e5.5", "1e5e5", "0x10", " 1", "1 ", "inf",
        "nan", "1f",
    };
    size_t i;

    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        uint64_t bits = 7;
        CHECK(!anti_rt_read_float((const unsigned char *)bad[i],
                                  (int64_t)strlen(bad[i]), 52, 11, &bits));
        CHECK(bits == 7);
    }
}

/* Every text the writer gives for a double reads back as its bits. */
static void doubles_come_back(void)
{
    uint64_t state = 1;
    int wrong = 0;
    int i;

    for (i = 0; i < 5000; i++) {
        uint64_t bits = next_random(&state);
        char *shortest;
        char *science;
        char *seventeen;
        if (((bits >> 52) & 0x7FF) == 0x7FF) {
            continue;
        }
        shortest = written(double_of(bits), -1, 0, 0);
        science = written(double_of(bits), -1, 1, 0);
        seventeen = written(double_of(bits), 16, 1, 0);
        wrong += read_as(shortest, 52, 11) != bits;
        wrong += read_as(science, 52, 11) != bits;
        wrong += read_as(seventeen, 52, 11) != bits;
        free(shortest);
        free(science);
        free(seventeen);
    }
    CHECK(wrong == 0);
}

/* The same for a float, written with the digits of a float. */
static void singles_come_back(void)
{
    uint64_t state = 2;
    int wrong = 0;
    int i;

    for (i = 0; i < 20000; i++) {
        uint32_t bits = (uint32_t)next_random(&state);
        char *shortest;
        if (((bits >> 23) & 0xFF) == 0xFF) {
            continue;
        }
        shortest = written((double)single_of(bits), -1, 0, 1);
        wrong += read_as(shortest, 23, 8) != bits;
        free(shortest);
    }
    CHECK(wrong == 0);
}

/* One pair of neighbours lo < hi and the bits of each, whose digits end
   within places after the point. The half-way point reads as the even
   one, and a unit below it or above it as lo or hi. */
static int neighbours(double lo, double hi, uint64_t lo_bits,
                      uint64_t hi_bits, int mantissa, int exponent,
                      int64_t places)
{
    char *a = written(lo, places, 0, 0);
    char *b = written(hi, places, 0, 0);
    char *middle = halfway(a, b, (size_t)places);
    char *down = below(middle);
    char *up = above(middle);
    char *negative = malloc(strlen(middle) + 2);
    uint64_t sign = (uint64_t)1 << (mantissa + exponent);
    uint64_t even = lo_bits % 2 == 0 ? lo_bits : hi_bits;
    int wrong = 0;

    negative[0] = '-';
    memcpy(negative + 1, middle, strlen(middle) + 1);
    wrong += read_as(middle, mantissa, exponent) != even;
    wrong += read_as(negative, mantissa, exponent) != (even | sign);
    wrong += read_as(down, mantissa, exponent) != lo_bits;
    wrong += read_as(up, mantissa, exponent) != hi_bits;
    free(a);
    free(b);
    free(middle);
    free(down);
    free(up);
    free(negative);
    return wrong;
}

static void halfway_points(void)
{
    uint64_t state = 3;
    int wrong = 0;
    uint32_t h;
    int i;

    /* Doubles from every binade, the smallest and the largest pairs
       among them. The largest double has no neighbour above. */
    wrong += neighbours(0.0, double_of(1), 0, 1, 52, 11, DOUBLE_PLACES);
    wrong += neighbours(double_of(0x7FEFFFFFFFFFFFFEull),
                        double_of(0x7FEFFFFFFFFFFFFFull),
                        0x7FEFFFFFFFFFFFFEull, 0x7FEFFFFFFFFFFFFFull, 52, 11,
                        DOUBLE_PLACES);
    for (i = 0; i < 300; i++) {
        uint64_t bits = next_random(&state) & 0x7FFFFFFFFFFFFFFFull;
        if (bits >= 0x7FEFFFFFFFFFFFFFull) {
            continue;
        }
        wrong += neighbours(double_of(bits), double_of(bits + 1), bits,
                            bits + 1, 52, 11, DOUBLE_PLACES);
    }

    /* Floats, whose half-way points a double holds exactly. */
    for (i = 0; i < 5000; i++) {
        uint32_t bits = (uint32_t)next_random(&state) & 0x7FFFFFFFu;
        if (bits >= 0x7F7FFFFFu) {
            continue;
        }
        wrong += neighbours((double)single_of(bits),
                            (double)single_of(bits + 1), bits, bits + 1, 23,
                            8, SINGLE_PLACES);
    }

    /* Every pair of finite halves. */
    for (h = 0; h < 0x7BFF; h++) {
        wrong += neighbours((double)anti_rt_f16_to_f32(h),
                            (double)anti_rt_f16_to_f32(h + 1), h, h + 1, 10,
                            5, HALF_PLACES);
    }
    CHECK(wrong == 0);
}

/* Every finite half comes back from the text of its f32, which is what
   serialize writes for an f16. */
static void halves_come_back(void)
{
    int wrong = 0;
    uint32_t h;

    for (h = 0; h <= 0xFFFF; h++) {
        char *text;
        if (((h >> 10) & 0x1F) == 0x1F) {
            continue;
        }
        text = written((double)anti_rt_f16_to_f32(h), -1, 0, 1);
        wrong += read_as(text, 10, 5) != h;
        free(text);
    }
    CHECK(wrong == 0);
}

void test_float_read(void)
{
    hard_cases();
    refusals();
    doubles_come_back();
    singles_come_back();
    halfway_points();
    halves_come_back();
}

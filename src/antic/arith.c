#include "arith.h"

#include "text.h"

#include <stdlib.h>

/* The value of the low n bits of v, extended by is_signed. */
static uint64_t extend(uint64_t v, int n, bool is_signed)
{
    uint64_t mask;
    uint64_t sign;

    if (n == 64) {
        return v;
    }
    mask = ((uint64_t)1 << n) - 1;
    sign = (uint64_t)1 << (n - 1);
    v &= mask;
    return is_signed ? (v ^ sign) - sign : v;
}

/* The 128-bit product of a and b as hi and lo, unsigned. C has no wider
   integer on every host, so the four products of the 32-bit halves make
   it. */
static void multiply(uint64_t a, uint64_t b, uint64_t *hi, uint64_t *lo)
{
    uint64_t a0 = a & 0xffffffff;
    uint64_t a1 = a >> 32;
    uint64_t b0 = b & 0xffffffff;
    uint64_t b1 = b >> 32;
    uint64_t p00 = a0 * b0;
    uint64_t p01 = a0 * b1;
    uint64_t p10 = a1 * b0;
    uint64_t middle = (p00 >> 32) + (p01 & 0xffffffff) + (p10 & 0xffffffff);

    *lo = (middle << 32) | (p00 & 0xffffffff);
    *hi = a1 * b1 + (p01 >> 32) + (p10 >> 32) + (middle >> 32);
}

/* DESIGN: below 64 bits both operands extend to 64 and their product is
   exact there, since two values of 32 bits multiply into 64. The upper
   half is then a shift, arithmetic for a signed product. At 64 bits the
   signed upper half is the unsigned one less each operand that the
   other's sign bit counts as 2^64. */
uint64_t arith_mul_high(uint64_t a, uint64_t b, int n, bool is_signed)
{
    uint64_t hi;
    uint64_t lo;

    if (n < 64) {
        uint64_t product = extend(a, n, is_signed) * extend(b, n, is_signed);
        uint64_t high = is_signed && (product >> 63) != 0
                            ? ~(~product >> n)
                            : product >> n;
        return extend(high, n, is_signed);
    }
    multiply(a, b, &hi, &lo);
    if (is_signed) {
        hi -= (a >> 63) != 0 ? b : 0;
        hi -= (b >> 63) != 0 ? a : 0;
    }
    return hi;
}

/* The largest and the smallest value of n bits, as their bits in 64. */
static uint64_t largest(int n, bool is_signed)
{
    int k = is_signed ? n - 1 : n;

    return k == 64 ? UINT64_MAX : ((uint64_t)1 << k) - 1;
}

static uint64_t smallest(int n, bool is_signed)
{
    return is_signed ? ~largest(n, true) : 0;
}

/* Whether the exact result of a op b leaves the range of n bits, and in
   which direction. 1 is above the largest value, -1 below the smallest
   and 0 inside. Below 64 bits the operation is exact in 64, unsigned as well.
   Two values of 32 bits multiply into 64. */
static int direction(char op, uint64_t a, uint64_t b, int n, bool is_signed)
{
    uint64_t hi;
    uint64_t lo;

    if (n < 64 && !is_signed) {
        a = extend(a, n, false);
        b = extend(b, n, false);
        if (op == '-') {
            return a < b ? -1 : 0;
        }
        return (op == '+' ? a + b : a * b) > largest(n, false) ? 1 : 0;
    }
    if (n < 64) {
        int64_t x = (int64_t)extend(a, n, true);
        int64_t y = (int64_t)extend(b, n, true);
        int64_t r = op == '+' ? x + y : op == '-' ? x - y : x * y;
        int64_t top = (int64_t)largest(n, true);
        return r > top ? 1 : r < -top - 1 ? -1 : 0;
    }
    if (!is_signed) {
        if (op == '+') {
            return a + b < a ? 1 : 0;
        }
        if (op == '-') {
            return a < b ? -1 : 0;
        }
        multiply(a, b, &hi, &lo);
        return hi != 0 ? 1 : 0;
    }
    if (op == '+' || op == '-') {
        uint64_t r = op == '+' ? a + b : a - b;
        uint64_t sign_b = op == '+' ? b : ~b;
        if (((a ^ r) & (sign_b ^ r)) >> 63 == 0) {
            return 0;
        }
        return (a >> 63) != 0 ? -1 : 1;
    }
    multiply(a, b, &hi, &lo);
    hi -= (a >> 63) != 0 ? b : 0;
    hi -= (b >> 63) != 0 ? a : 0;
    if (hi == ((lo >> 63) != 0 ? UINT64_MAX : 0)) {
        return 0;
    }
    return ((a ^ b) >> 63) != 0 ? -1 : 1;
}

uint64_t arith_saturate(char op, uint64_t a, uint64_t b, int n,
                        bool is_signed)
{
    int d = direction(op, a, b, n, is_signed);

    if (d > 0) {
        return largest(n, is_signed);
    }
    if (d < 0) {
        return smallest(n, is_signed);
    }
    a = op == '+' ? a + b : op == '-' ? a - b : a * b;
    return extend(a, n, is_signed);
}

double arith_float_literal(const char *bytes, size_t length, bool single)
{
    struct text digits = {0};
    double value;

    text_append_bytes(&digits, bytes, length);
    value = single ? (double)strtof(text_cstr(&digits), NULL)
                   : strtod(text_cstr(&digits), NULL);
    text_free(&digits);
    return value;
}

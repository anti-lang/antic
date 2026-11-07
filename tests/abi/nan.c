/* C functions for the program test abi_nan, by Q1 of "Answers of 2026-11-06,
   fourth set": C reads the bits of every float the program hands it. The
   program passes a NaN as an argument, among the variadic arguments and in a
   struct by value, and C calls a function the program exports and a callback
   the program passes, each of which returns a NaN. */
#include "../binary_stdio.h"
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

struct nan_pair {
    float x;
    float y;
};

struct nan_wide {
    double x;
    double y;
};

unsigned nan_bits32(float v)
{
    uint32_t bits;
    memcpy(&bits, &v, sizeof bits);
    return bits;
}

unsigned long long nan_bits64(double v)
{
    uint64_t bits;
    memcpy(&bits, &v, sizeof bits);
    return bits;
}

unsigned nan_pair_y(struct nan_pair p)
{
    return nan_bits32(p.y);
}

unsigned long long nan_wide_y(struct nan_wide p)
{
    return nan_bits64(p.y);
}

/* The double after count, as a variadic argument of C. */
unsigned long long nan_variadic(int count, ...)
{
    va_list list;
    double v;

    va_start(list, count);
    v = va_arg(list, double);
    va_end(list);
    return count == 1 ? nan_bits64(v) : 0;
}

float anti_nan_make(float z, int kind);
double anti_nan_make64(double z, int kind);

unsigned nan_exported(float z, int kind)
{
    return nan_bits32(anti_nan_make(z, kind));
}

unsigned long long nan_exported64(double z, int kind)
{
    return nan_bits64(anti_nan_make64(z, kind));
}

unsigned nan_callback(float (*f)(float, int), float z, int kind)
{
    return nan_bits32(f(z, kind));
}

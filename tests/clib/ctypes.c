/* A C program that calls the library ctypes with every C type Anti names.
   The header gives `long`, `unsigned long` and `wchar_t` for `c_long`,
   `c_ulong` and `c_wchar`, and the names of <stdint.h> for the others. The
   program compares each with the type of C, passes the limits of each
   type and prints one line per way a type crosses. */
#include "../binary_stdio.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <wchar.h>

#include "ctypes.h"

static long twice_long(long v)
{
    return v * 2;
}

static wchar_t twice_wchar(wchar_t v)
{
    return (wchar_t)(v * 2);
}

static double twice_double(double v)
{
    return v * 2.0;
}

int main(void)
{
    CtypesAll all = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13.5f, 14.25};
    CtypesAll next = ctypes_all(all);
    CtypesAll out = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0};
    CtypesWord word;
    static const size_t sizes[] = {
        sizeof(signed char), sizeof(unsigned char), sizeof(short),
        sizeof(unsigned short), sizeof(int), sizeof(unsigned int),
        sizeof(long), sizeof(unsigned long), sizeof(long long),
        sizeof(unsigned long long), sizeof(size_t), sizeof(wchar_t),
        sizeof(float), sizeof(double)};
    int which;

    printf("header %d %d %d\n", _Generic(ctypes_long(0), long: 1, default: 0),
           _Generic(ctypes_ulong(0), unsigned long: 1, default: 0),
           _Generic(ctypes_wchar(0), wchar_t: 1, default: 0));
    printf("sizes");
    for (which = 0; which < 14; which++) {
        printf(" %d", ctypes_size_of(which) == (int)sizes[which]);
    }
    printf("\n");
    printf("passed %d %d %d %d %d %u %ld %lu %lld %llu %llu %ld %.2f %.2f\n",
           ctypes_char(41), ctypes_uchar(41), ctypes_short(41),
           ctypes_ushort(41), (int)ctypes_int(41), (unsigned)ctypes_uint(41),
           ctypes_long(41), ctypes_ulong(41), (long long)ctypes_longlong(41),
           (unsigned long long)ctypes_ulonglong(41),
           (unsigned long long)ctypes_size(41), (long)ctypes_wchar(41),
           (double)ctypes_float(1.25f), ctypes_double(1.25));
    printf("at the end %d %d %d %d %d %d %d %d %d %d %d %d\n",
           ctypes_char(SCHAR_MAX - 1) == SCHAR_MAX,
           ctypes_uchar(UCHAR_MAX - 1) == UCHAR_MAX,
           ctypes_short(SHRT_MAX - 1) == SHRT_MAX,
           ctypes_ushort(USHRT_MAX - 1) == USHRT_MAX,
           ctypes_int(INT_MAX - 1) == INT_MAX,
           ctypes_uint(UINT_MAX - 1) == UINT_MAX,
           ctypes_long(LONG_MAX - 1) == LONG_MAX,
           ctypes_ulong(ULONG_MAX - 1) == ULONG_MAX,
           ctypes_longlong(LLONG_MAX - 1) == LLONG_MAX,
           ctypes_ulonglong(ULLONG_MAX - 1) == ULLONG_MAX,
           ctypes_size(SIZE_MAX - 1) == SIZE_MAX,
           ctypes_wchar(WCHAR_MAX - 1) == WCHAR_MAX);
    printf("least %d %d %d %d\n", ctypes_long(LONG_MIN) == LONG_MIN + 1,
           ctypes_wchar(WCHAR_MIN) == WCHAR_MIN + 1,
           ctypes_wchar_negative(WCHAR_MIN) == (WCHAR_MIN < 0),
           ctypes_wchar_negative(WCHAR_MAX) == 0);
    printf("fourteen parameters %.2f\n",
           ctypes_sum(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13.5f, 14.25));
    printf("struct %d %d %d %d %d %d %u %ld %lu %lld %llu %llu %ld %.2f %.2f\n",
           ctypes_all_size() == (int)sizeof(CtypesAll), next.a, next.b,
           next.c, next.d, (int)next.e, (unsigned)next.f, next.g, next.h,
           (long long)next.i, (unsigned long long)next.j,
           (unsigned long long)next.k, (long)next.l, (double)next.m, next.n);
    word.whole = 0x41;
    printf("union %d\n", ctypes_first_byte(word));
    ctypes_out(&out.a, &out.b, &out.c, &out.d, &out.e, &out.f, &out.g,
               &out.h, &out.i, &out.j, &out.k, &out.l, &out.m, &out.n);
    printf("pointers %d %d %d %d %d %u %ld %lu %lld %llu %llu %ld %.2f %.2f\n",
           out.a, out.b, out.c, out.d, (int)out.e, (unsigned)out.f, out.g,
           out.h, (long long)out.i, (unsigned long long)out.j,
           (unsigned long long)out.k, (long)out.l, (double)out.m, out.n);
    printf("function pointers %ld %ld %.2f\n",
           ctypes_call_long(twice_long, 20),
           (long)ctypes_call_wchar(twice_wchar, 30),
           ctypes_call_double(twice_double, 1.5));
    printf("constants %d %ld %lu %llu %ld %d %.2f\n", (int)CTYPES_INT,
           CTYPES_LONG, CTYPES_ULONG, (unsigned long long)CTYPES_SIZE,
           (long)CTYPES_WCHAR, CTYPES_UCHAR, CTYPES_DOUBLE);
    return 0;
}

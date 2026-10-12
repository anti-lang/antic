/* C functions for the program test abi_ctypes: one set per C type that Anti
   names, `c_char` to `c_wchar` with `c_float` and `c_double`. The program
   passes each type to C and takes it back, alone, among fourteen arguments,
   in a struct by value, through a pointer and through a callback, and C
   calls the functions the program exports with each type. Where the width
   or the sign of a type follows the target, the C side gives the limits of
   the type, so the output is the same on every target. */
#include "../binary_stdio.h"
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

/* Anti holds `c_char` as an integer of 8 bits with a sign on every target,
   so the functions of it take a signed char. */
signed char ct_char(signed char v)
{
    return (signed char)(v + 1);
}

signed char ct_char_max(void)
{
    return SCHAR_MAX;
}

signed char ct_call_char(signed char (*f)(signed char), signed char v)
{
    return f(v);
}

unsigned char ct_uchar(unsigned char v)
{
    return (unsigned char)(v + 1);
}

unsigned char ct_uchar_max(void)
{
    return UCHAR_MAX;
}

unsigned char ct_call_uchar(unsigned char (*f)(unsigned char), unsigned char v)
{
    return f(v);
}

short ct_short(short v)
{
    return (short)(v + 1);
}

short ct_short_max(void)
{
    return SHRT_MAX;
}

short ct_short_min(void)
{
    return SHRT_MIN;
}

short ct_call_short(short (*f)(short), short v)
{
    return f(v);
}

unsigned short ct_ushort(unsigned short v)
{
    return (unsigned short)(v + 1);
}

unsigned short ct_ushort_max(void)
{
    return USHRT_MAX;
}

unsigned short ct_call_ushort(unsigned short (*f)(unsigned short), unsigned short v)
{
    return f(v);
}

int ct_int(int v)
{
    return (int)(v + 1);
}

int ct_int_max(void)
{
    return INT_MAX;
}

int ct_int_min(void)
{
    return INT_MIN;
}

int ct_call_int(int (*f)(int), int v)
{
    return f(v);
}

unsigned int ct_uint(unsigned int v)
{
    return (unsigned int)(v + 1);
}

unsigned int ct_uint_max(void)
{
    return UINT_MAX;
}

unsigned int ct_call_uint(unsigned int (*f)(unsigned int), unsigned int v)
{
    return f(v);
}

long ct_long(long v)
{
    return (long)(v + 1);
}

long ct_long_max(void)
{
    return LONG_MAX;
}

long ct_long_min(void)
{
    return LONG_MIN;
}

long ct_call_long(long (*f)(long), long v)
{
    return f(v);
}

unsigned long ct_ulong(unsigned long v)
{
    return (unsigned long)(v + 1);
}

unsigned long ct_ulong_max(void)
{
    return ULONG_MAX;
}

unsigned long ct_call_ulong(unsigned long (*f)(unsigned long), unsigned long v)
{
    return f(v);
}

long long ct_longlong(long long v)
{
    return (long long)(v + 1);
}

long long ct_longlong_max(void)
{
    return LLONG_MAX;
}

long long ct_longlong_min(void)
{
    return LLONG_MIN;
}

long long ct_call_longlong(long long (*f)(long long), long long v)
{
    return f(v);
}

unsigned long long ct_ulonglong(unsigned long long v)
{
    return (unsigned long long)(v + 1);
}

unsigned long long ct_ulonglong_max(void)
{
    return ULLONG_MAX;
}

unsigned long long ct_call_ulonglong(unsigned long long (*f)(unsigned long long), unsigned long long v)
{
    return f(v);
}

size_t ct_size(size_t v)
{
    return (size_t)(v + 1);
}

size_t ct_size_max(void)
{
    return SIZE_MAX;
}

size_t ct_call_size(size_t (*f)(size_t), size_t v)
{
    return f(v);
}

wchar_t ct_wchar(wchar_t v)
{
    return (wchar_t)(v + 1);
}

wchar_t ct_wchar_max(void)
{
    return WCHAR_MAX;
}

wchar_t ct_wchar_min(void)
{
    return WCHAR_MIN;
}

wchar_t ct_call_wchar(wchar_t (*f)(wchar_t), wchar_t v)
{
    return f(v);
}

float ct_float(float v)
{
    return v * 2.0f;
}

double ct_double(double v)
{
    return v * 2.0;
}

float ct_call_float(float (*f)(float), float v)
{
    return f(v);
}

double ct_call_double(double (*f)(double), double v)
{
    return f(v);
}

/* The size of each type in the order of the program, so a width that
   differs from the one antic holds shows as a 0 in its line. */
int ct_size_of(int which)
{
    static const size_t sizes[] = {
        sizeof(signed char), sizeof(unsigned char), sizeof(short),
        sizeof(unsigned short), sizeof(int), sizeof(unsigned int),
        sizeof(long), sizeof(unsigned long), sizeof(long long),
        sizeof(unsigned long long), sizeof(size_t), sizeof(wchar_t),
        sizeof(float), sizeof(double)};

    return (int)sizes[which];
}

/* Whether the wchar_t of the target is signed. */
int ct_wchar_signed(void)
{
    return WCHAR_MIN < 0;
}

/* Fourteen arguments, one of each type, which pass in registers and on
   the stack. */
double ct_sum(signed char a, unsigned char b, short c, unsigned short d, int e,
              unsigned int f, long g, unsigned long h, long long i,
              unsigned long long j, size_t k, wchar_t l, float m, double n)
{
    return (double)a + (double)b + (double)c + (double)d + (double)e +
           (double)f + (double)g + (double)h + (double)i + (double)j +
           (double)k + (double)l + (double)m + n;
}

struct CtAll {
    signed char a;
    unsigned char b;
    short c;
    unsigned short d;
    int e;
    unsigned int f;
    long g;
    unsigned long h;
    long long i;
    unsigned long long j;
    size_t k;
    wchar_t l;
    float m;
    double n;
};

int ct_all_size(void)
{
    return (int)sizeof(struct CtAll);
}

struct CtAll ct_all(struct CtAll v)
{
    struct CtAll r = {
        (signed char)(v.a + 1),          (unsigned char)(v.b + 1),
        (short)(v.c + 1),          (unsigned short)(v.d + 1),
        v.e + 1,                   v.f + 1,
        v.g + 1,                   v.h + 1,
        v.i + 1,                   v.j + 1,
        v.k + 1,                   (wchar_t)(v.l + 1),
        v.m + 1.0f,                v.n + 1.0};

    return r;
}

void ct_out(signed char *a, unsigned char *b, short *c, unsigned short *d, int *e,
            unsigned int *f, long *g, unsigned long *h, long long *i,
            unsigned long long *j, size_t *k, wchar_t *l, float *m, double *n)
{
    *a = 1;
    *b = 2;
    *c = 3;
    *d = 4;
    *e = 5;
    *f = 6;
    *g = 7;
    *h = 8;
    *i = 9;
    *j = 10;
    *k = 11;
    *l = 12;
    *m = 13.0f;
    *n = 14.0;
}

/* The variadic arguments Anti passes: `i` an int of 32 bits, `l` one of
   64, `d` a double and `p` a pointer to an int. */
double ct_variadic(const char *format, ...)
{
    va_list args;
    double sum = 0.0;

    va_start(args, format);
    for (; *format != 0; format++) {
        if (*format == 'i') {
            sum += (double)va_arg(args, int);
        } else if (*format == 'l') {
            sum += (double)va_arg(args, long long);
        } else if (*format == 'd') {
            sum += va_arg(args, double);
        } else {
            sum += (double)*va_arg(args, int *);
        }
    }
    va_end(args);
    return sum;
}

signed char anti_ct_char(signed char v);
unsigned char anti_ct_uchar(unsigned char v);
short anti_ct_short(short v);
unsigned short anti_ct_ushort(unsigned short v);
int anti_ct_int(int v);
unsigned int anti_ct_uint(unsigned int v);
long anti_ct_long(long v);
unsigned long anti_ct_ulong(unsigned long v);
long long anti_ct_longlong(long long v);
unsigned long long anti_ct_ulonglong(unsigned long long v);
size_t anti_ct_size(size_t v);
wchar_t anti_ct_wchar(wchar_t v);
float anti_ct_float(float v);
double anti_ct_double(double v);

/* Call every function the program exports, each of which gives back its
   argument less one. A bit of the result stands for a type that came
   back right, in the order of the program, so 0x3fff is all fourteen. */
int ct_check_exports(void)
{
    int right = 0;

    right |= (anti_ct_char(SCHAR_MAX) == (signed char)(SCHAR_MAX - 1)) << 0;
    right |= (anti_ct_uchar(UCHAR_MAX) == (unsigned char)(UCHAR_MAX - 1)) << 1;
    right |= (anti_ct_short(SHRT_MAX) == (short)(SHRT_MAX - 1)) << 2;
    right |= (anti_ct_ushort(USHRT_MAX) == (unsigned short)(USHRT_MAX - 1)) << 3;
    right |= (anti_ct_int(INT_MAX) == (int)(INT_MAX - 1)) << 4;
    right |= (anti_ct_uint(UINT_MAX) == (unsigned int)(UINT_MAX - 1)) << 5;
    right |= (anti_ct_long(LONG_MAX) == (long)(LONG_MAX - 1)) << 6;
    right |= (anti_ct_ulong(ULONG_MAX) == (unsigned long)(ULONG_MAX - 1)) << 7;
    right |= (anti_ct_longlong(LLONG_MAX) == (long long)(LLONG_MAX - 1)) << 8;
    right |= (anti_ct_ulonglong(ULLONG_MAX) == (unsigned long long)(ULLONG_MAX - 1)) << 9;
    right |= (anti_ct_size(SIZE_MAX) == (size_t)(SIZE_MAX - 1)) << 10;
    right |= (anti_ct_wchar(WCHAR_MAX) == (wchar_t)(WCHAR_MAX - 1)) << 11;
    right |= (anti_ct_float(2.5f) == 1.5f) << 12;
    right |= (anti_ct_double(2.5) == 1.5) << 13;
    return right;
}

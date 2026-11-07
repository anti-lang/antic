/* The exact functions of anti.math: the square root, the four roundings,
   the absolute value, the sign and the least and the greatest of two. */

#include "libm.h"

/* DESIGN: each of these is exact in IEEE, so every correct way to compute
   it gives the same bits, and the instruction of a target gives the
   result of musl. The elementwise built-ins of clang write the LLVM
   intrinsic, which never sets errno and so never becomes a call. A
   release build inlines the runtime, so each is one instruction there.
   x86-64-v1 has no instruction of rounding, SSE4.1 comes with v2, and LLVM
   then calls floorf of the C library of the system. The four roundings
   call the libm of musl at that level instead, so no call of anti.math
   reaches the libm of a system. */
#if defined(__aarch64__) || defined(__SSE4_1__)
#define ANTI_RT_ROUNDS 1
#else
#define ANTI_RT_ROUNDS 0
#endif

float anti_rt_sqrtf(float x)
{
    return __builtin_elementwise_sqrt(x);
}

double anti_rt_sqrt(double x)
{
    return __builtin_elementwise_sqrt(x);
}

float anti_rt_floorf(float x)
{
#if ANTI_RT_ROUNDS
    return __builtin_elementwise_floor(x);
#else
    return anti_rt_libm_floorf(x);
#endif
}

double anti_rt_floor(double x)
{
#if ANTI_RT_ROUNDS
    return __builtin_elementwise_floor(x);
#else
    return anti_rt_libm_floor(x);
#endif
}

float anti_rt_ceilf(float x)
{
#if ANTI_RT_ROUNDS
    return __builtin_elementwise_ceil(x);
#else
    return anti_rt_libm_ceilf(x);
#endif
}

double anti_rt_ceil(double x)
{
#if ANTI_RT_ROUNDS
    return __builtin_elementwise_ceil(x);
#else
    return anti_rt_libm_ceil(x);
#endif
}

float anti_rt_truncf(float x)
{
#if ANTI_RT_ROUNDS
    return __builtin_elementwise_trunc(x);
#else
    return anti_rt_libm_truncf(x);
#endif
}

double anti_rt_trunc(double x)
{
#if ANTI_RT_ROUNDS
    return __builtin_elementwise_trunc(x);
#else
    return anti_rt_libm_trunc(x);
#endif
}

float anti_rt_roundf(float x)
{
#if ANTI_RT_ROUNDS
    return __builtin_elementwise_round(x);
#else
    return anti_rt_libm_roundf(x);
#endif
}

double anti_rt_round(double x)
{
#if ANTI_RT_ROUNDS
    return __builtin_elementwise_round(x);
#else
    return anti_rt_libm_round(x);
#endif
}

float anti_rt_fabsf(float x)
{
    return __builtin_elementwise_abs(x);
}

double anti_rt_fabs(double x)
{
    return __builtin_elementwise_abs(x);
}

/* DESIGN: Q1 of "Answers of 2026-11-06, fourth set". The sign of a NaN
   follows the processor, so a NaN gives its sign as the canonical NaN
   does, the sign of 0x7fc00000, on every target. */
float anti_rt_copysignf(float x, float y)
{
    return __builtin_elementwise_copysign(x, __builtin_isnan(y) ? 1.0f : y);
}

double anti_rt_copysign(double x, double y)
{
    return __builtin_elementwise_copysign(x, __builtin_isnan(y) ? 1.0 : y);
}

/* DESIGN: minimumnum and maximumnum of LLVM are minimumNumber and
   maximumNumber of IEEE 754-2019, which fmin and fmax of musl compute: a
   NaN gives the other operand, and -0 stands below +0. Every target and
   level writes them without a call. */
float anti_rt_fminf(float x, float y)
{
    return __builtin_elementwise_minimumnum(x, y);
}

double anti_rt_fmin(double x, double y)
{
    return __builtin_elementwise_minimumnum(x, y);
}

float anti_rt_fmaxf(float x, float y)
{
    return __builtin_elementwise_maximumnum(x, y);
}

double anti_rt_fmax(double x, double y)
{
    return __builtin_elementwise_maximumnum(x, y);
}

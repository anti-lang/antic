#ifndef ANTI_RT_LIBM_H
#define ANTI_RT_LIBM_H

/* The exact functions of anti.math, and the four roundings of the libm
   of musl that they call at a level without the instruction. */

/* src/native/libm.cmake compiles the libm of musl into the runtime of
   every target under the names of src/native/libm-names.h, the prefix
   anti_rt_libm_ before the name of C. anti.math declares each function
   it calls with `extern fn`, and libm.c calls the four roundings. The
   exact functions of libm.c are one instruction where the target has
   it. */

/* floorf of musl, the largest whole number not above x. */
float anti_rt_libm_floorf(float x);

/* floor of musl. */
double anti_rt_libm_floor(double x);

/* ceilf of musl, the smallest whole number not below x. */
float anti_rt_libm_ceilf(float x);

/* ceil of musl. */
double anti_rt_libm_ceil(double x);

/* truncf of musl, x without its fraction. */
float anti_rt_libm_truncf(float x);

/* trunc of musl. */
double anti_rt_libm_trunc(double x);

/* roundf of musl, the nearest whole number, a half away from 0. */
float anti_rt_libm_roundf(float x);

/* round of musl. */
double anti_rt_libm_round(double x);

/* The square root of x, correctly rounded. */
float anti_rt_sqrtf(float x);

/* The square root of x, correctly rounded. */
double anti_rt_sqrt(double x);

/* The largest whole number not above x. */
float anti_rt_floorf(float x);

/* The largest whole number not above x. */
double anti_rt_floor(double x);

/* The smallest whole number not below x. */
float anti_rt_ceilf(float x);

/* The smallest whole number not below x. */
double anti_rt_ceil(double x);

/* x without its fraction. */
float anti_rt_truncf(float x);

/* x without its fraction. */
double anti_rt_trunc(double x);

/* The nearest whole number to x, a half away from 0. */
float anti_rt_roundf(float x);

/* The nearest whole number to x, a half away from 0. */
double anti_rt_round(double x);

/* x without its sign. */
float anti_rt_fabsf(float x);

/* x without its sign. */
double anti_rt_fabs(double x);

/* x with the sign of y. */
float anti_rt_copysignf(float x, float y);

/* x with the sign of y. */
double anti_rt_copysign(double x, double y);

/* The smaller of x and y, with -0 below +0 and a NaN giving the other. */
float anti_rt_fminf(float x, float y);

/* The smaller of x and y, with -0 below +0 and a NaN giving the other. */
double anti_rt_fmin(double x, double y);

/* The larger of x and y, with +0 above -0 and a NaN giving the other. */
float anti_rt_fmaxf(float x, float y);

/* The larger of x and y, with +0 above -0 and a NaN giving the other. */
double anti_rt_fmax(double x, double y);

#endif

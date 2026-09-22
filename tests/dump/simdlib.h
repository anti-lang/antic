/* simdlib.h, the C interface of com.example.simdlib, written by antic.
   Do not edit. A failure that Anti cannot report calls abort(). */
#ifndef SIMDLIB_H
#define SIMDLIB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#define ANTI_ALIGNAS(n) alignas(n)
extern "C" {
#else
#define ANTI_ALIGNAS(n) _Alignas(n)
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>
#else
#include <immintrin.h>
#endif

/** Four floats, the vector type of C. */
#if defined(__aarch64__) || defined(_M_ARM64)
typedef float32x4_t Vec4;
#else
typedef __m128 Vec4;
#endif

/** Four 32-bit integers. */
#if defined(__aarch64__) || defined(_M_ARM64)
typedef int32x4_t I32x4;
#else
typedef __m128i I32x4;
#endif

/** Two floats, which pass as the struct of their lanes. */
typedef struct F2 {
    ANTI_ALIGNAS(8) float x;
    float y;
} F2;

/** Every lane of a times k plus b. */
Vec4 simd_mix(Vec4 a, float k, Vec4 b);
/** The sum of the lanes. */
float simd_total(Vec4 a);
/** The lanes of a where they are greater than the lanes of b. */
Vec4 simd_greater(Vec4 a, Vec4 b);
/** Every lane doubled. */
I32x4 simd_twice(I32x4 v);
/** The product of the lanes of a pair. */
float simd_pair(F2 p);

#ifdef __cplusplus
}
#endif

#endif

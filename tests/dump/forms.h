/* forms.h, the C interface of com.example.forms, written by antic.
   Do not edit. A failure that Anti cannot report calls abort(). */
#ifndef FORMS_H
#define FORMS_H

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

/** Sixteen lanes of one byte, signed. */
#if defined(__aarch64__) || defined(_M_ARM64)
typedef int8x16_t I8x16;
#else
typedef __m128i I8x16;
#endif

/** Eight lanes of sixteen bits, signed. */
#if defined(__aarch64__) || defined(_M_ARM64)
typedef int16x8_t I16x8;
#else
typedef __m128i I16x8;
#endif

/** Eight lanes of sixteen bits, unsigned. */
#if defined(__aarch64__) || defined(_M_ARM64)
typedef uint16x8_t U16x8;
#else
typedef __m128i U16x8;
#endif

/** Four lanes of thirty-two bits, unsigned. */
#if defined(__aarch64__) || defined(_M_ARM64)
typedef uint32x4_t U32x4;
#else
typedef __m128i U32x4;
#endif

/** Two lanes of sixty-four bits, signed. */
#if defined(__aarch64__) || defined(_M_ARM64)
typedef int64x2_t I64x2;
#else
typedef __m128i I64x2;
#endif

/** Two lanes of sixty-four bits, unsigned. */
#if defined(__aarch64__) || defined(_M_ARM64)
typedef uint64x2_t U64x2;
#else
typedef __m128i U64x2;
#endif

/** Two lanes of a double. */
#if defined(__aarch64__) || defined(_M_ARM64)
typedef float64x2_t F64x2;
#else
typedef __m128d F64x2;
#endif

/** A struct that is a union. */
typedef union Word {
    uint32_t whole;
    float real;
} Word;

/* The tuple (*int, ?*int, [2]int). */
struct anti_tuple_ptr_int_optr_int_a2_int {
    int64_t * /* non-null */ _0;
    int64_t *_1;
    int64_t _2[2];
};

/** A flag that is set. */
#define ENABLED true
/** A flag that is clear. */
#define DISABLED false
/** A text with a bell and a tab in it. */
static const char BELL[] = "ring\007tab\011here";

/** A pointer, a pointer that may be none and an array in one tuple. */
struct anti_tuple_ptr_int_optr_int_a2_int parts(int64_t * /* non-null */ p, int64_t *q);
/** A function that takes a function and calls it. */
int64_t apply(int64_t (*f)(int64_t, void *), void *f_context, int64_t x);
/** A function that takes a function of no parameter. */
void run(void (*f)(void *), void *f_context);
/** The sums of the vectors, lane by lane. */
int64_t sums(I8x16 a, I16x8 b, U16x8 c, U32x4 d, I64x2 e, U64x2 f, F64x2 g);
/** The whole of a word. */
uint32_t whole(Word w);

#ifdef __cplusplus
}
#endif

#endif

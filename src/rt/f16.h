/* The conversions between f16 and f32, shared by antic and the runtime.

   DESIGN: one definition read from both sides. antic folds a constant
   `as f16` with it, and the runtime converts with it where the processor
   has no instruction, at x86-64-v1 and v2. A constant and a value that a
   program computes therefore round alike. The rounding is to the nearest
   half with ties to even, which is what fcvt and vcvtps2ph do in the
   default mode. A NaN keeps its sign and the top of its payload and
   becomes quiet, as it does in both instructions. */
#ifndef ANTI_F16_H
#define ANTI_F16_H

#include <stdint.h>
#include <string.h>

/* The f32 of the half h. Every half has one exactly. */
static inline float anti_rt_f16_widen(uint16_t h)
{
    uint32_t sign = (uint32_t)(h & 0x8000) << 16;
    uint32_t exponent = (h >> 10) & 0x1F;
    uint32_t mantissa = h & 0x3FF;
    uint32_t bits;
    float value;

    if (exponent == 0x1F) {
        bits = sign | 0x7F800000 | (mantissa << 13) |
               (mantissa != 0 ? 0x400000 : 0);
    } else if (exponent != 0) {
        bits = sign | ((exponent + 112) << 23) | (mantissa << 13);
    } else {
        /* A subnormal half is a count of 2^-24, which an f32 holds as
           a normal number. */
        value = (float)mantissa * 5.9604644775390625e-8f;
        memcpy(&bits, &value, sizeof bits);
        bits |= sign;
    }
    memcpy(&value, &bits, sizeof value);
    return value;
}

/* The half nearest to f, ties to even. */
static inline uint16_t anti_rt_f16_narrow(float f)
{
    uint32_t bits;
    uint32_t sign;
    uint32_t exponent;
    uint32_t mantissa;
    uint32_t half;
    uint32_t shift;
    uint32_t rest;
    uint32_t middle;
    int32_t e;

    memcpy(&bits, &f, sizeof bits);
    sign = (bits >> 16) & 0x8000;
    exponent = (bits >> 23) & 0xFF;
    mantissa = bits & 0x7FFFFF;
    if (exponent == 0xFF) {
        return (uint16_t)(sign | 0x7C00 |
                          (mantissa != 0 ? 0x200 | (mantissa >> 13) : 0));
    }
    e = (int32_t)exponent - 112;
    if (e >= 31) {
        return (uint16_t)(sign | 0x7C00);
    }
    if (e <= 0) {
        /* Below the smallest normal half the result counts 2^-24. A
           value below half of 2^-24 rounds to zero. */
        if (e < -10) {
            return (uint16_t)sign;
        }
        mantissa |= 0x800000;
        shift = (uint32_t)(14 - e);
    } else {
        shift = 13;
    }
    half = (e <= 0 ? 0 : (uint32_t)e << 10) | (mantissa >> shift);
    rest = mantissa & ((1u << shift) - 1);
    middle = 1u << (shift - 1);
    /* A carry out of the mantissa raises the exponent, which is the right
       result, up to infinity past the largest half. */
    if (rest > middle || (rest == middle && (half & 1) != 0)) {
        half++;
    }
    return (uint16_t)(sign | half);
}

/* The routines a program calls at x86-64-v1 and v2. A half travels in
   the low sixteen bits of a 32-bit integer register. The routines ignore
   the high bits they take and clear the ones they give. */
float anti_rt_f16_to_f32(uint32_t h);
uint32_t anti_rt_f32_to_f16(float f);

#endif

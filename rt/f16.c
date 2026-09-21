#include "f16.h"

/* DESIGN: x86-64-v1 and v2 have no F16C. A program built for them
   converts an f16 with these two calls, where v3 and ARM64 write one
   instruction. They are the functions antic folds constants with. */
float anti_rt_f16_to_f32(uint32_t h)
{
    return anti_f16_widen((uint16_t)h);
}

uint32_t anti_rt_f32_to_f16(float f)
{
    return anti_f16_narrow(f);
}

/* The processor check that runs once at start. antic writes the level a
   program was built for into the object that defines main, and start.c
   passes it here before it calls main.

   DESIGN: the check refuses only a machine that is known to be below the
   level. Where a platform has no query for a feature the level needs, the
   level passes. macOS and Linux answer for every level of the table.
   Windows on ARM64 answers up to the ARMv8.2 dot products and has no
   query above them, so armv8.5 passes there. The platform layer reads
   the machine, and this file holds the levels. */
#include "cpu_level.h"
#include "platform.h"
#include "std.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The build compiles one runtime per target and level and names the level
   here. A build that leaves it out would ship a runtime that checks the
   wrong processor, so it stops instead. */
#ifndef ANTI_CPU_LEVEL_ID
#error "define ANTI_CPU_LEVEL_ID, the anti_rt_cpu_level of this runtime"
#endif

/* One row per level. message is the start-up message of a machine that
   cannot run a program of the level. */
struct row {
    int32_t level;
    const char *message;
};

/* DESIGN: a message names hardware a reader recognises. The x86 lines say
   the instruction set and the first year a machine had it, as
   docs/anti-language-additions.md gives for v3. The ARM64 lines name
   machines, because nobody buys an ARM64 machine by its extensions.

   The lowest level of an architecture never refuses. The machine is at
   least that: an x86_64 machine runs v1 and an ARM64 machine runs
   armv8.0. Those two messages are unreachable and are written for the
   table's shape. */
static const struct row rows[] = {
    {ANTI_CPU_X86_64_V1, "this program needs an x86-64 processor"},
    {ANTI_CPU_X86_64_V2,
     "this program needs a processor with SSE4.2 (x86-64-v2, 2009 or later)"},
    {ANTI_CPU_X86_64_V3,
     "this program needs a processor with AVX2 (x86-64-v3, 2013 or later)"},
    {ANTI_CPU_ARMV8_0, "this program needs an ARMv8-A processor"},
    {ANTI_CPU_ARMV8_2,
     "this program needs an ARMv8.2 processor (Raspberry Pi 5, Apple "
     "Silicon, or later)"},
    {ANTI_CPU_ARMV8_5, "this program needs an Apple Silicon Mac"},
};

static const struct row *row_of(int32_t level)
{
    size_t i;

    for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        if (rows[i].level == level) {
            return &rows[i];
        }
    }
    return NULL;
}

const char *anti_rt_cpu_level_message(int32_t level)
{
    const struct row *r = row_of(level);

    return r == NULL ? "" : r->message;
}

#if defined(ANTI_RT_X86_64)

static int has_v2(void)
{
    uint32_t one[4];
    uint32_t extended[4];

    anti_rt_cpuid(1, 0, one);
    /* SSE3, SSSE3, CMPXCHG16B, SSE4.1, SSE4.2 and POPCNT of leaf 1. */
    if ((one[2] & (1u << 0 | 1u << 9 | 1u << 13 | 1u << 19 | 1u << 20 |
                   1u << 23)) !=
        (1u << 0 | 1u << 9 | 1u << 13 | 1u << 19 | 1u << 20 | 1u << 23)) {
        return 0;
    }
    anti_rt_cpuid(0x80000001u, 0, extended);
    /* LAHF and SAHF in long mode. */
    return (extended[2] & 1u) != 0;
}

static int has_v3(void)
{
    uint32_t one[4];
    uint32_t seven[4];
    uint32_t extended[4];

    anti_rt_cpuid(1, 0, one);
    /* FMA, MOVBE, OSXSAVE, AVX and F16C of leaf 1. */
    if ((one[2] & (1u << 12 | 1u << 22 | 1u << 27 | 1u << 28 | 1u << 29)) !=
        (1u << 12 | 1u << 22 | 1u << 27 | 1u << 28 | 1u << 29)) {
        return 0;
    }
    /* The operating system saves the SSE and the AVX state. */
    if ((anti_rt_xcr0() & 6u) != 6u) {
        return 0;
    }
    anti_rt_cpuid(7, 0, seven);
    /* BMI1, AVX2 and BMI2 of leaf 7. */
    if ((seven[1] & (1u << 3 | 1u << 5 | 1u << 8)) !=
        (1u << 3 | 1u << 5 | 1u << 8)) {
        return 0;
    }
    anti_rt_cpuid(0x80000001u, 0, extended);
    /* LZCNT, which CPUID calls ABM. */
    return (extended[2] & 1u << 5) != 0;
}

static int32_t machine_level(void)
{
    if (has_v3()) {
        return ANTI_CPU_X86_64_V3;
    }
    return has_v2() ? ANTI_CPU_X86_64_V2 : ANTI_CPU_X86_64_V1;
}

#elif defined(ANTI_RT_ARM64)

static int32_t machine_level(void)
{
    return anti_rt_arm64_level();
}

#else

static int32_t machine_level(void)
{
    return ANTI_CPU_NONE;
}

#endif

int32_t anti_rt_cpu_level(void)
{
#if defined(ANTI_DEV_CPU)
    /* A test on this machine sees the refusal of a lower one. The
       variable is read through the platform layer, as every read of the
       environment in the runtime is, since the C runtime of Windows
       deprecates getenv. */
    char *simulated = NULL;
    int32_t level = ANTI_CPU_NONE;
    size_t i;

    if (anti_rt_getenv("ANTI_CPU_LEVEL", &simulated) == 0 &&
        simulated != NULL) {
        for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
            const char *name = anti_rt_cpu_level_name(rows[i].level);
            if (strcmp(simulated, name) == 0) {
                level = rows[i].level;
            }
        }
        free(simulated);
        if (level != ANTI_CPU_NONE) {
            return level;
        }
    }
#endif
    return machine_level();
}

int32_t anti_rt_cpu_missing(int32_t needed)
{
    int32_t have = anti_rt_cpu_level();

    if (have == ANTI_CPU_NONE || row_of(needed) == NULL) {
        return 0;
    }
    return have < needed ? needed : 0;
}

void anti_rt_cpu_check(void)
{
    int32_t missing = anti_rt_cpu_missing(ANTI_CPU_LEVEL_ID);

    if (missing == 0) {
        return;
    }
    anti_rt_fail_exit(70, "anti: %s", anti_rt_cpu_level_message(missing));
}

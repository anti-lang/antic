/* The processor check that runs once at start. antic writes the level a
   program was built for into the object that defines main, and start.c
   passes it here before it calls main.

   DESIGN: the check refuses only a machine that is known to be below the
   level. Where a platform has no query for a feature the level needs, the
   level passes. macOS and Linux answer for every level of the table.
   Windows on ARM64 answers up to the ARMv8.2 dot products and has no
   query above them, so armv8.5 passes there. */
#include "cpu_level.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__x86_64__) || defined(_M_X64)
#define ANTI_CPU_X86_64 1
#elif defined(__aarch64__) || defined(_M_ARM64)
#define ANTI_CPU_ARM64 1
#endif

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#if defined(ANTI_CPU_ARM64)
#if defined(_WIN32)
#include <windows.h>
/* The values of IsProcessorFeaturePresent, which an older SDK may not
   declare. */
#ifndef PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE
#define PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE 34
#endif
#ifndef PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE
#define PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE 43
#endif
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#else
#include <sys/auxv.h>
/* The AT_HWCAP bits of the features the levels need. A musl header of the
   sysroot declares none of them. */
#define ANTI_HWCAP_ATOMICS (1UL << 8)
#define ANTI_HWCAP_FPHP (1UL << 9)
#define ANTI_HWCAP_ASIMDHP (1UL << 10)
#define ANTI_HWCAP_SB (1UL << 29)
#define ANTI_HWCAP2_FRINT (1UL << 8)
#ifndef AT_HWCAP2
#define AT_HWCAP2 26
#endif
#endif
#endif

/* The build compiles one runtime per target and level and names the level
   here. A build that leaves it out would ship a runtime that checks the
   wrong processor, so it stops instead. */
#ifndef ANTI_CPU_LEVEL_ID
#error "define ANTI_CPU_LEVEL_ID, the anti_cpu_level of this runtime"
#endif

/* One row per level. message is the start-up message of a machine that
   cannot run a program of the level. */
struct row {
    int32_t level;
    const char *name;
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
    {ANTI_CPU_X86_64_V1, "v1", "this program needs an x86-64 processor"},
    {ANTI_CPU_X86_64_V2, "v2",
     "this program needs a processor with SSE4.2 (x86-64-v2, 2009 or later)"},
    {ANTI_CPU_X86_64_V3, "v3",
     "this program needs a processor with AVX2 (x86-64-v3, 2013 or later)"},
    {ANTI_CPU_ARMV8_0, "armv8.0", "this program needs an ARMv8-A processor"},
    {ANTI_CPU_ARMV8_2, "armv8.2",
     "this program needs an ARMv8.2 processor (Raspberry Pi 5, Apple "
     "Silicon, or later)"},
    {ANTI_CPU_ARMV8_5, "armv8.5", "this program needs an Apple Silicon Mac"},
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

const char *anti_cpu_level_name(int32_t level)
{
    const struct row *r = row_of(level);

    return r == NULL ? "" : r->name;
}

const char *anti_cpu_level_message(int32_t level)
{
    const struct row *r = row_of(level);

    return r == NULL ? "" : r->message;
}

#if defined(ANTI_CPU_X86_64)

static void cpuid_count(uint32_t leaf, uint32_t sub, uint32_t out[4])
{
#if defined(_MSC_VER)
    int regs[4];

    __cpuidex(regs, (int)leaf, (int)sub);
    out[0] = (uint32_t)regs[0];
    out[1] = (uint32_t)regs[1];
    out[2] = (uint32_t)regs[2];
    out[3] = (uint32_t)regs[3];
#else
    __asm__ volatile("cpuid"
                     : "=a"(out[0]), "=b"(out[1]), "=c"(out[2]), "=d"(out[3])
                     : "a"(leaf), "c"(sub));
#endif
}

/* The extended control register that says whether the operating system
   saves the SSE and AVX state. AVX without it faults. */
static uint64_t control_register(void)
{
#if defined(_MSC_VER)
    return _xgetbv(0);
#else
    uint32_t low;
    uint32_t high;

    /* xgetbv by its bytes, so that the assembler needs no xsave option. */
    __asm__ volatile(".byte 0x0f, 0x01, 0xd0"
                     : "=a"(low), "=d"(high)
                     : "c"(0));
    return (uint64_t)high << 32 | low;
#endif
}

static int has_v2(void)
{
    uint32_t one[4];
    uint32_t extended[4];

    cpuid_count(1, 0, one);
    /* SSE3, SSSE3, CMPXCHG16B, SSE4.1, SSE4.2 and POPCNT of leaf 1. */
    if ((one[2] & (1u << 0 | 1u << 9 | 1u << 13 | 1u << 19 | 1u << 20 |
                   1u << 23)) !=
        (1u << 0 | 1u << 9 | 1u << 13 | 1u << 19 | 1u << 20 | 1u << 23)) {
        return 0;
    }
    cpuid_count(0x80000001u, 0, extended);
    /* LAHF and SAHF in long mode. */
    return (extended[2] & 1u) != 0;
}

static int has_v3(void)
{
    uint32_t one[4];
    uint32_t seven[4];
    uint32_t extended[4];

    cpuid_count(1, 0, one);
    /* FMA, MOVBE, OSXSAVE, AVX and F16C of leaf 1. */
    if ((one[2] & (1u << 12 | 1u << 22 | 1u << 27 | 1u << 28 | 1u << 29)) !=
        (1u << 12 | 1u << 22 | 1u << 27 | 1u << 28 | 1u << 29)) {
        return 0;
    }
    /* The operating system saves the SSE and the AVX state. */
    if ((control_register() & 6u) != 6u) {
        return 0;
    }
    cpuid_count(7, 0, seven);
    /* BMI1, AVX2 and BMI2 of leaf 7. */
    if ((seven[1] & (1u << 3 | 1u << 5 | 1u << 8)) !=
        (1u << 3 | 1u << 5 | 1u << 8)) {
        return 0;
    }
    cpuid_count(0x80000001u, 0, extended);
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

#elif defined(ANTI_CPU_ARM64)

#if defined(_WIN32)

static int32_t machine_level(void)
{
    /* DESIGN: Windows answers for the ARMv8.1 atomics and the ARMv8.2 dot
       products and has no query above them. Where a feature has no flag
       the level is assumed, because every Windows-on-ARM machine sold is
       armv8.2 or later. A machine with both flags therefore reports
       armv8.5 as well. */
    if (IsProcessorFeaturePresent(
            PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE) &&
        IsProcessorFeaturePresent(PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE)) {
        return ANTI_CPU_ARMV8_5;
    }
    return ANTI_CPU_ARMV8_0;
}

#elif defined(__APPLE__)

static int feature(const char *name)
{
    int32_t value = 0;
    size_t size = sizeof value;

    if (sysctlbyname(name, &value, &size, NULL, 0) != 0) {
        return 0;
    }
    return value != 0;
}

static int32_t machine_level(void)
{
    if (feature("hw.optional.arm.FEAT_SB") &&
        feature("hw.optional.arm.FEAT_FRINTTS")) {
        return ANTI_CPU_ARMV8_5;
    }
    if (feature("hw.optional.arm.FEAT_LSE") &&
        feature("hw.optional.arm.FEAT_FP16")) {
        return ANTI_CPU_ARMV8_2;
    }
    return ANTI_CPU_ARMV8_0;
}

#else

static int32_t machine_level(void)
{
    unsigned long one = getauxval(AT_HWCAP);
    unsigned long two = getauxval(AT_HWCAP2);

    if ((one & ANTI_HWCAP_SB) != 0 && (two & ANTI_HWCAP2_FRINT) != 0) {
        return ANTI_CPU_ARMV8_5;
    }
    if ((one & ANTI_HWCAP_ATOMICS) != 0 && (one & ANTI_HWCAP_FPHP) != 0 &&
        (one & ANTI_HWCAP_ASIMDHP) != 0) {
        return ANTI_CPU_ARMV8_2;
    }
    return ANTI_CPU_ARMV8_0;
}

#endif

#else

static int32_t machine_level(void)
{
    return ANTI_CPU_NONE;
}

#endif

int32_t anti_cpu_level(void)
{
#if defined(ANTI_DEV_CPU)
    /* A test on this machine sees the refusal of a lower one. */
    const char *simulated = getenv("ANTI_CPU_LEVEL");
    size_t i;

    if (simulated != NULL) {
        for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
            if (strcmp(simulated, rows[i].name) == 0) {
                return rows[i].level;
            }
        }
    }
#endif
    return machine_level();
}

int32_t anti_cpu_missing(int32_t needed)
{
    int32_t have = anti_cpu_level();

    if (have == ANTI_CPU_NONE || row_of(needed) == NULL) {
        return 0;
    }
    return have < needed ? needed : 0;
}

int32_t anti_cpu_built(void)
{
    return ANTI_CPU_LEVEL_ID;
}

void anti_cpu_check(void)
{
    int32_t missing = anti_cpu_missing(ANTI_CPU_LEVEL_ID);

    if (missing == 0) {
        return;
    }
    fprintf(stderr, "anti: %s\n", anti_cpu_level_message(missing));
    exit(70);
}

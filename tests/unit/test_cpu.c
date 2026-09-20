/* The processor levels: the table antic reads, the names --cpu takes and
   the start-up check of the runtime. The check is compiled here with
   ANTI_DEV_CPU. The environment variable ANTI_CPU_LEVEL then stands in
   for the processor, and this machine sees the refusal of a lower one.
   The runtime of the archive is compiled without it. */

/* setenv and unsetenv are POSIX, outside the C11 library. The headers of
   Apple declare them anyway, and musl and glibc hide them under -std=c11,
   so the Mac compiled this file and the Linux VM did not. */
#define _POSIX_C_SOURCE 200809L

#include "../binary_stdio.h"
#include "check.h"
#include <stdlib.h>

#include "cpu.h"

static void set_level(const char *name)
{
#if defined(_WIN32)
    _putenv_s("ANTI_CPU_LEVEL", name);
#else
    setenv("ANTI_CPU_LEVEL", name, 1);
#endif
}

static void clear_level(void)
{
#if defined(_WIN32)
    _putenv_s("ANTI_CPU_LEVEL", "");
#else
    unsetenv("ANTI_CPU_LEVEL");
#endif
}

/* docs/anti-language-additions.md settles the defaults under "CPU levels". */
static void defaults(void)
{
    CHECK(cpu_default(TARGET_LINUX_X86_64) == CPU_V3);
    CHECK(cpu_default(TARGET_MACOS_X86_64) == CPU_V3);
    CHECK(cpu_default(TARGET_WINDOWS_X86_64) == CPU_V3);
    CHECK(cpu_default(TARGET_MACOS_ARM64) == CPU_ARMV8_5);
    CHECK(cpu_default(TARGET_LINUX_ARM64) == CPU_ARMV8_0);
    CHECK(cpu_default(TARGET_WINDOWS_ARM64) == CPU_ARMV8_2);
}

/* A level belongs to one architecture, and --cpu takes the names of the
   target's architecture alone. */
static void names(void)
{
    enum cpu_level level = CPU_LEVEL_COUNT;

    CHECK(cpu_from_name("v1", TARGET_LINUX_X86_64, &level) &&
          level == CPU_V1);
    CHECK(cpu_from_name("v2", TARGET_WINDOWS_X86_64, &level) &&
          level == CPU_V2);
    CHECK(cpu_from_name("v3", TARGET_MACOS_X86_64, &level) && level == CPU_V3);
    CHECK(cpu_from_name("armv8.0", TARGET_MACOS_ARM64, &level) &&
          level == CPU_ARMV8_0);
    CHECK(cpu_from_name("armv8.5", TARGET_LINUX_ARM64, &level) &&
          level == CPU_ARMV8_5);
    CHECK(!cpu_from_name("v3", TARGET_LINUX_ARM64, &level));
    CHECK(!cpu_from_name("armv8.2", TARGET_LINUX_X86_64, &level));
    CHECK(!cpu_from_name("v4", TARGET_LINUX_X86_64, &level));
    CHECK_STR(cpu_name(CPU_V3), "v3");
    CHECK_STR(cpu_name(CPU_ARMV8_2), "armv8.2");
}

/* The features the back end asks for. x86-64-v3 writes the VEX forms, and
   armv8.2 and above have the atomics in one instruction. */
static void features(void)
{
    CHECK(!cpu_has(CPU_V1, CPU_AVX));
    CHECK(!cpu_has(CPU_V2, CPU_AVX));
    CHECK(cpu_has(CPU_V2, CPU_SSE4));
    CHECK(cpu_has(CPU_V3, CPU_AVX));
    CHECK(cpu_has(CPU_V3, CPU_SSE4));
    CHECK(!cpu_has(CPU_ARMV8_0, CPU_LSE));
    CHECK(cpu_has(CPU_ARMV8_2, CPU_LSE));
    CHECK(cpu_has(CPU_ARMV8_5, CPU_LSE));
    CHECK(cpu_has(CPU_ARMV8_2, CPU_FP16));
    CHECK(cpu_has(CPU_ARMV8_2, CPU_DOTPROD));
    /* The cap on the size of a simd struct, which nothing reads yet. */
    CHECK(CPU_VECTOR_BYTE_CAP == 256);
}

/* The ids the runtime reads, which rise with the level inside one
   architecture. */
static void ids(void)
{
    CHECK(cpu_id(CPU_V1) < cpu_id(CPU_V2));
    CHECK(cpu_id(CPU_V2) < cpu_id(CPU_V3));
    CHECK(cpu_id(CPU_ARMV8_0) < cpu_id(CPU_ARMV8_2));
    CHECK(cpu_id(CPU_ARMV8_2) < cpu_id(CPU_ARMV8_5));
    CHECK(cpu_id(CPU_V3) == ANTI_CPU_X86_64_V3);
    CHECK(cpu_id(CPU_ARMV8_5) == ANTI_CPU_ARMV8_5);
    CHECK_STR(anti_cpu_level_name(cpu_id(CPU_V3)), cpu_name(CPU_V3));
    CHECK_STR(anti_cpu_level_name(cpu_id(CPU_ARMV8_2)),
              cpu_name(CPU_ARMV8_2));
    CHECK_STR(anti_cpu_level_name(0), "");
}

/* The start-up check refuses a machine below the level of the program and
   names what the machine is missing. The message is the one the section
   gives. */
static void refusal(void)
{
    set_level("v1");
    CHECK(anti_cpu_level() == ANTI_CPU_X86_64_V1);
    CHECK(anti_cpu_missing(ANTI_CPU_X86_64_V3) == ANTI_CPU_X86_64_V3);
    CHECK(anti_cpu_missing(ANTI_CPU_X86_64_V2) == ANTI_CPU_X86_64_V2);
    CHECK(anti_cpu_missing(ANTI_CPU_X86_64_V1) == 0);
    CHECK_STR(anti_cpu_level_message(ANTI_CPU_X86_64_V3),
              "this program needs a processor with AVX2 "
              "(x86-64-v3, 2013 or later)");
    CHECK_STR(anti_cpu_level_message(ANTI_CPU_ARMV8_5),
              "this program needs an Apple Silicon Mac");
    CHECK_STR(anti_cpu_level_message(ANTI_CPU_ARMV8_2),
              "this program needs an ARMv8.2 processor "
              "(Raspberry Pi 5, Apple Silicon, or later)");

    set_level("v3");
    CHECK(anti_cpu_missing(ANTI_CPU_X86_64_V3) == 0);
    CHECK(anti_cpu_missing(ANTI_CPU_X86_64_V1) == 0);

    /* The lowest level of an architecture never refuses: every machine of
       it is at least that. */
    set_level("armv8.0");
    CHECK(anti_cpu_missing(ANTI_CPU_ARMV8_2) == ANTI_CPU_ARMV8_2);
    CHECK(anti_cpu_missing(ANTI_CPU_ARMV8_5) == ANTI_CPU_ARMV8_5);
    CHECK(anti_cpu_missing(ANTI_CPU_ARMV8_0) == 0);

    set_level("armv8.5");
    CHECK(anti_cpu_missing(ANTI_CPU_ARMV8_5) == 0);
    CHECK(anti_cpu_missing(ANTI_CPU_ARMV8_2) == 0);

    /* A value that names no level asks for nothing. */
    CHECK(anti_cpu_missing(0) == 0);
    clear_level();
}

void test_cpu(void)
{
    defaults();
    names();
    features();
    ids();
    refusal();
}

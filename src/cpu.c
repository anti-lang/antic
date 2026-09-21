#include "cpu.h"

#include <string.h>

/* DESIGN: one row per level, in the order of enum cpu_level. clang_arch
   builds the runtime archive's native libraries for the level.
   attributes let llvm-mc assemble the instructions the level adds. The
   x86_64 assembler of LLVM takes every instruction of every level, so
   those rows carry no attributes.

   tools/cpu-levels holds the same table for the build, and the test
   cpu_levels_pin compares the two. */
static const struct {
    const char *name;
    enum target_arch arch;
    int32_t id;
    unsigned features;
    const char *clang_arch;
    const char *attributes;
} levels[CPU_LEVEL_COUNT] = {
    [CPU_V1] = {"v1", ARCH_X86_64, ANTI_CPU_X86_64_V1, 0, "x86-64", ""},
    [CPU_V2] = {"v2", ARCH_X86_64, ANTI_CPU_X86_64_V2, CPU_SSE4, "x86-64-v2",
                ""},
    [CPU_V3] = {"v3", ARCH_X86_64, ANTI_CPU_X86_64_V3,
                CPU_SSE4 | CPU_AVX | CPU_F16C, "x86-64-v3", ""},
    [CPU_ARMV8_0] = {"armv8.0", ARCH_ARM64, ANTI_CPU_ARMV8_0, 0, "armv8-a",
                     ""},
    [CPU_ARMV8_2] = {"armv8.2", ARCH_ARM64, ANTI_CPU_ARMV8_2,
                     CPU_LSE | CPU_FP16 | CPU_DOTPROD, "armv8.2-a",
                     "+lse,+fullfp16,+dotprod"},
    [CPU_ARMV8_5] = {"armv8.5", ARCH_ARM64, ANTI_CPU_ARMV8_5,
                     CPU_LSE | CPU_FP16 | CPU_DOTPROD, "armv8.5-a",
                     "+lse,+fullfp16,+dotprod"},
};

/* DESIGN: the x86_64 baseline of a release build is x86-64-v3, and the
   ARM64 baseline is per operating system. Every Apple Silicon Mac is an
   M1 or later. Every Windows-on-ARM machine sold is a Snapdragon 8cx or
   later. linux-arm64 carries the Pi 4 and older boards. */
static const enum cpu_level defaults[TARGET_COUNT] = {
    [TARGET_LINUX_X86_64] = CPU_V3,
    [TARGET_LINUX_ARM64] = CPU_ARMV8_0,
    [TARGET_MACOS_X86_64] = CPU_V3,
    [TARGET_MACOS_ARM64] = CPU_ARMV8_5,
    [TARGET_WINDOWS_X86_64] = CPU_V3,
    [TARGET_WINDOWS_ARM64] = CPU_ARMV8_2,
};

const char *cpu_name(enum cpu_level level)
{
    return levels[level].name;
}

enum target_arch cpu_arch(enum cpu_level level)
{
    return levels[level].arch;
}

int32_t cpu_id(enum cpu_level level)
{
    return levels[level].id;
}

bool cpu_has(enum cpu_level level, unsigned feature)
{
    return (levels[level].features & feature) != 0;
}

const char *cpu_clang_arch(enum cpu_level level)
{
    return levels[level].clang_arch;
}

const char *cpu_attributes(enum cpu_level level)
{
    return levels[level].attributes;
}

enum cpu_level cpu_default(enum target t)
{
    return defaults[t];
}

bool cpu_from_name(const char *name, enum target t, enum cpu_level *level)
{
    int i;

    for (i = 0; i < CPU_LEVEL_COUNT; i++) {
        if (levels[i].arch == target_info(t)->arch &&
            strcmp(name, levels[i].name) == 0) {
            *level = (enum cpu_level)i;
            return true;
        }
    }
    return false;
}

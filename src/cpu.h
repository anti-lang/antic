#ifndef ANTIC_CPU_H
#define ANTIC_CPU_H

#include <stdbool.h>
#include <stdint.h>

/* The level ids are the ones the runtime reads, so they have one
   definition and this header takes it. */
#include "../rt/cpu_level.h"
#include "target.h"

/* A processor level is a code-generation setting and not a target. The
   six targets stay six, and each has a default level that --cpu
   overrides. docs/anti-language-additions.md settles the defaults under
   "CPU levels". */
enum cpu_level {
    CPU_V1,
    CPU_V2,
    CPU_V3,
    CPU_ARMV8_0,
    CPU_ARMV8_2,
    CPU_ARMV8_5,
    CPU_LEVEL_COUNT
};

/* What a level gives the back end and the runtime archive. */
enum cpu_feature {
    CPU_AVX = 1u << 0,      /* VEX forms of the scalar float instructions */
    CPU_SSE4 = 1u << 1,     /* SSE4.1, SSE4.2 and POPCNT */
    CPU_LSE = 1u << 2,      /* an atomic operation in one instruction */
    CPU_FP16 = 1u << 3,     /* half-precision conversion */
    CPU_DOTPROD = 1u << 4   /* the dot products */
};

/* The widest vector register of any level antic knows, and the cap on the
   size of a simd struct. The section names both in the level table. */
enum { CPU_VECTOR_REGISTER_BYTES = 32, CPU_SIMD_STRUCT_CAP = 256 };

const char *cpu_name(enum cpu_level level);
enum target_arch cpu_arch(enum cpu_level level);
int32_t cpu_id(enum cpu_level level);
bool cpu_has(enum cpu_level level, unsigned feature);

/* The -march= value of clang, which builds the native libraries of the
   runtime archive for a level. */
const char *cpu_clang_arch(enum cpu_level level);

/* The -mattr= value of llvm-mc, empty where the assembler needs none. */
const char *cpu_attributes(enum cpu_level level);

/* The widest vector register of the level, in bytes. */
unsigned cpu_vector_bytes(enum cpu_level level);

/* The default level of a target: x86-64-v3 on both x86_64 targets, and
   per operating system on ARM64. */
enum cpu_level cpu_default(enum target t);

/* Store the level named name when it belongs to the architecture of
   target t. Returns false for a name that no level of that architecture
   carries. */
bool cpu_from_name(const char *name, enum target t, enum cpu_level *level);

#endif

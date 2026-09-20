/* The processor levels, shared by antic and the runtime.

   DESIGN: the ids below are one definition read from both sides. antic
   writes the level of a program into the object that defines main, as the
   32-bit global anti_cpu_required. rt/start.c compares it against the
   machine before it calls main. src/cpu.h includes this header rather
   than repeating the numbers, so the two never drift.

   A level is a code-generation setting and not a target. The ids of one
   architecture rise with the level, so a comparison decides whether a
   machine is high enough. Ids of different architectures never meet,
   because a program built for one never starts on the other. */
#ifndef ANTI_CPU_LEVEL_H
#define ANTI_CPU_LEVEL_H

#include <stdint.h>

enum anti_cpu_level {
    ANTI_CPU_NONE = 0,
    ANTI_CPU_X86_64_V1 = 1,
    ANTI_CPU_X86_64_V2 = 2,
    ANTI_CPU_X86_64_V3 = 3,
    ANTI_CPU_ARMV8_0 = 16,
    ANTI_CPU_ARMV8_2 = 18,
    ANTI_CPU_ARMV8_5 = 21
};

/* The name of a level, "v3" or "armv8.2", and "" for a value that names
   none. */
const char *anti_cpu_level_name(int32_t level);

/* What a machine that cannot run a program of this level is missing, as
   the text of the start-up message: "AVX2 (x86-64-v3, 2013 or later)".
   Returns "" for a value that names no level. */
const char *anti_cpu_level_needs(int32_t level);

/* The highest level this machine runs, for the architecture the runtime
   was compiled for. ANTI_CPU_NONE on an architecture with no level table.

   With ANTI_DEV_CPU the environment variable ANTI_CPU_LEVEL replaces the
   answer. A test then sees the refusal of a lower machine on the machine
   it runs on. A released runtime is compiled without it and reads the
   processor alone. */
int32_t anti_cpu_level(void);

/* Zero when this machine runs a program built for needed. Otherwise the
   level it is missing, for anti_cpu_level_needs. */
int32_t anti_cpu_missing(int32_t needed);

/* Exit with the message of the section when this machine is too low for
   needed, and return otherwise. rt/start.c calls it before main. */
void anti_cpu_check(int32_t needed);

#endif

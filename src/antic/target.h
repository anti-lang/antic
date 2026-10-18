#ifndef ANTIC_TARGET_H
#define ANTIC_TARGET_H

/* The interface of target.c: the six targets, their object formats and
   calling conventions, and the symbols of functions and of C names. */

#include <stdbool.h>
#include <stddef.h>

#include "../rt/plugin_index.h"
#include "text.h"

/* The minimum macOS version of both macOS targets. */
enum { MACOS_MIN_MAJOR = 11, MACOS_MIN_MINOR = 0 };

/* The index file that `antic --lib shared --no-runtime` writes beside a
   plugin is ANTI_PLUGIN_INDEX of src/rt/plugin_index.h, which the runtime
   and anti read as well. */

enum target {
    TARGET_LINUX_X86_64,
    TARGET_LINUX_ARM64,
    TARGET_MACOS_X86_64,
    TARGET_MACOS_ARM64,
    TARGET_WINDOWS_X86_64,
    TARGET_WINDOWS_ARM64,
    TARGET_COUNT
};

enum target_os { OS_LINUX, OS_MACOS, OS_WINDOWS };
enum target_arch { ARCH_X86_64, ARCH_ARM64 };
enum object_format { FORMAT_ELF, FORMAT_MACHO, FORMAT_COFF };

/* The calling convention of a target. macOS on x86_64 follows System V,
   and the three ARM64 conventions differ in details of AAPCS64. */
enum convention {
    CONVENTION_SYSV,
    CONVENTION_WINDOWS_X64,
    CONVENTION_AAPCS64,
    CONVENTION_APPLE_ARM64,
    CONVENTION_WINDOWS_ARM64
};

/* What the rest of antic needs to know about a target. */
struct target_info {
    enum target_os os;
    enum target_arch arch;
    enum object_format format;
    enum convention convention;
    const char *triple;             /* completed by llvm_triple */
    const char *object_suffix;
    const char *executable_suffix;
};

/* The name of t, which is also the directory of t in the runtime archive. */
const char *target_name(enum target t);

/* What the rest of antic needs to know about t. */
const struct target_info *target_info(enum target t);

/* The name of an object format for a message: ELF, Mach-O or COFF. */
const char *target_format_name(enum object_format format);

/* The name of a calling convention for a message. */
const char *target_convention_name(enum convention convention);

/* Store the target antic runs on. Returns false on a host that is not
   one of the six targets, and stores TARGET_COUNT. The platform layer,
   platform.c, defines it, since the answer is the host's. */
bool target_host(enum target *t);

/* Store the target named name. Returns false for a name that is none of
   the six targets, and leaves *t alone. */
bool target_from_name(const char *name, enum target *t);

/* Append the symbol of function name in module to out. t is one of the
   six targets. */
void target_mangle(struct text *out, enum target t, const char *module,
                   const char *name);

/* Append the symbol of the C function name as a C compiler for target t
   writes it. Mach-O adds a leading _, and ELF and COFF keep the name. */
void target_c_symbol(struct text *out, enum target t, const char *name);

#endif

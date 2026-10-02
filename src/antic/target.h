#ifndef ANTIC_TARGET_H
#define ANTIC_TARGET_H

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
    const char *triple;             /* the -triple option of llvm-mc */
    const char *object_suffix;
    const char *executable_suffix;
};

const char *target_name(enum target t);
const struct target_info *target_info(enum target t);
const char *target_format_name(enum object_format format);
const char *target_convention_name(enum convention convention);

/* Store the target antic runs on. Returns false on a host that is not
   one of the six targets, and stores TARGET_COUNT. The platform layer,
   platform.c, defines it, since the answer is the host's. */
bool target_host(enum target *t);
bool target_from_name(const char *name, enum target *t);

/* Append the symbol of function name in module to out. t is one of the
   six targets. */
void target_mangle(struct text *out, enum target t, const char *module,
                   const char *name);

/* Append the symbol of the C function name as a C compiler for target t
   writes it. Mach-O adds a leading _, and ELF and COFF keep the name. */
void target_c_symbol(struct text *out, enum target t, const char *name);

/* Append the label of block number block in the function with symbol
   function. The prefix keeps the label out of the symbol table. */
void target_block_label(struct text *out, enum target t, const char *function,
                        size_t block);

#endif

#ifndef ANTIC_MEMCHECK_H
#define ANTIC_MEMCHECK_H

/* The interface of memcheck.c: the checks of AddressSanitizer that
   --memory-checks puts before every load and store. */

#include <stdbool.h>

#include "ir.h"
#include "layout.h"
#include "target.h"

/* DESIGN: --memory-checks puts a check of AddressSanitizer before every
   load and store of the program. The check is a call of __asan_loadN or
   __asan_storeN with the address and the number of bytes. The runtime of
   AddressSanitizer then reports a use after free, a double free and an
   access outside a heap block. Its leak check reports the blocks still
   allocated at exit. A call is the same on every target, where an inline
   test of the shadow memory needs code for each, as the entry on the
   checks of AddressSanitizer in docs/decisions.md says. memcheck_function
   puts the checks in after layout_resolve lays the types out, so the IR
   above it keeps no size. A load, a store, a copy and each simd operation
   on memory take a check of the bytes they reach. */
#define MEMCHECK_LOAD "__asan_loadN"
#define MEMCHECK_STORE "__asan_storeN"
/* The hook that AddressSanitizer reads its default options from, and the
   options a program of --memory-checks gives it. macOS reports the leaks
   only when asked, and ends a program after a report with SIGABRT,
   which writes a crash report. The program exits with status 1
   instead, as on Linux. A report walks the stack by the frame pointers,
   since antic writes no CFI on ELF and the unwinder of a fatal report
   would stop at the first Anti frame. A malloc that AddressSanitizer
   cannot serve gives null, as the malloc of C does, so `alloc(T, n)`
   gives none where it gives none without the checks. */
#define MEMCHECK_OPTIONS_HOOK "__asan_default_options"
#define MEMCHECK_OPTIONS \
    "detect_leaks=1:abort_on_error=0:fast_unwind_on_fatal=1:" \
    "allocator_may_return_null=1"
/* The function of src/rt/platform.h that marks a block of the runtime as kept
   until exit, and the call of the leak checker that it makes in a
   program of --memory-checks. */
#define MEMCHECK_KEPT "anti_rt_memory_kept"
#define MEMCHECK_IGNORE "__lsan_ignore_object"

/* Whether the runtime archive holds the runtime of AddressSanitizer for
   t. compiler-rt builds none for Windows on ARM64. */
bool memcheck_available(enum target t);

/* Declare the two check functions in m and mark m for the checks. Where
   the runtime of t checks for leaks, the module that links, the one that
   defines main, also gets the options hook, which turns the leak check
   on, and the function that marks a block of the runtime as kept. The
   driver calls it after the optimizer, whose removal of unused code would
   drop the two declarations that no call names yet. */
void memcheck_declare(struct ir_module *m, const char *module, bool links,
                      enum target t);

/* Put a check before every load and store of f. l holds the layouts of
   the target, which give each access its size. */
void memcheck_function(const struct ir_module *m, struct ir_function *f,
                       struct layouts *l);

#endif

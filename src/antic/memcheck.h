#ifndef ANTIC_MEMCHECK_H
#define ANTIC_MEMCHECK_H

#include <stdbool.h>

#include "ir.h"
#include "layout.h"
#include "target.h"

/* DESIGN: --memory-checks puts a check of AddressSanitizer before every
   load and store of the program. The check is a call of __asan_loadN or
   __asan_storeN with the address and the number of bytes. The runtime of
   AddressSanitizer then reports a use after free, a double free and an
   access outside a heap block. Its leak check reports the blocks still
   allocated at exit. A call keeps the check out of the two targets,
   since the register allocator already treats one. An inline test of the
   shadow memory would need both. The checks go in after the back end
   lays the types out, so the IR above it keeps no size. By then the
   back end has made plain loads and stores of the bitfields and of the
   lanes of simd structs. */
#define MEMCHECK_LOAD "__asan_loadN"
#define MEMCHECK_STORE "__asan_storeN"
/* The hook that AddressSanitizer reads its default options from, and the
   options a program of --memory-checks gives it. macOS reports the leaks
   only when asked, and ends a program after a report with SIGABRT,
   which writes a crash report. The program exits with status 1
   instead, as on Linux. */
#define MEMCHECK_OPTIONS_HOOK "__asan_default_options"
#define MEMCHECK_OPTIONS "detect_leaks=1:abort_on_error=0"
/* The function of src/rt/rt.h that marks a block of the runtime as kept
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
   on, and the function that marks a block of the runtime as kept. Runs
   before selection, which gives each function of m its entry. */
void memcheck_declare(struct ir_module *m, const char *module, bool links,
                      enum target t);

/* Put a check before every load and store of f. l holds the layouts of
   the target, which give each access its size. */
void memcheck_function(const struct ir_module *m, struct ir_function *f,
                       struct layouts *l);

#endif

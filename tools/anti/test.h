#ifndef ANTI_TEST_H
#define ANTI_TEST_H

#include <stdbool.h>
#include <stddef.h>

/* Compile each module of sources with its `tests` and `fixtures` blocks,
   write a runner that calls every test of the module, link it and run it.
   release builds and runs the whole program instead of one object per
   module, with the checks and the assertions off. work holds the library
   files, the objects and the runner. runtime names the runtime archive,
   and llvm_mc the assembler, either of them NULL for the default.
   roots are the -I search roots, which also give each module its
   path. Returns the exit status of anti. */
int test_run(const char *const *sources, size_t source_count,
             const char *const *roots, size_t root_count, const char *work,
             const char *runtime, const char *llvm_mc, bool release);

#endif

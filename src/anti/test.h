#ifndef ANTI_TEST_H
#define ANTI_TEST_H

/* The interface of test.c: `anti test`. */

#include <stdbool.h>
#include <stddef.h>

#include "build.h"

/* One run of `anti test`. sources are the modules to test, and roots
   are the -I search roots, which also give each module its path. work
   holds the library files, the objects and the runner. package is the
   package name of the manifest, which every compile carries, or NULL
   outside a project. runtime names the runtime archive, NULL for the
   default. release builds and runs the whole program instead of one
   object per module, with the checks and the assertions off.
   memory_checks passes --memory-checks to every compile and runs each
   runner through Anti's symbolizer. inject holds the `Interface=Provider`
   entries of the manifest, which every compile of the run passes to
   antic.

   project marks a run over a project without -I, after the dev build of
   its modules. The library files of the run then stand in lib/ of the
   work directory, which the run empties first. libraries holds the
   library files of the dependency graph, which every compile of the run
   takes. tested_only leaves out of the run a module that declares no
   test, which a run over every module of the project asks for. tested
   is what the build of the project recorded, and NULL outside a
   project. The run then caches its own files in obj/ of the build of its
   mode and takes the objects of the modules it does not test from
   build_object. */
struct test_request {
    const char *const *sources;
    size_t source_count;
    const char *const *roots;
    size_t root_count;
    const char *work;
    const char *package;
    const char *runtime;
    bool release;
    bool memory_checks;
    const char *const *inject;
    size_t inject_count;
    bool project;
    const char *const *libraries;
    size_t library_count;
    bool tested_only;
    const struct build_tested *tested;
};

/* Compile each module of r with its `tests` and `fixtures` blocks, write
   a runner that calls every test of the module, link it and run it.
   Returns the exit status of anti. */
int test_run(const struct test_request *r);

#endif

#ifndef ANTI_BUILD_H
#define ANTI_BUILD_H

/* The interface of build.c: `anti build` and `anti run`. */

#include <stdbool.h>
#include <stddef.h>

#include "cpu.h"
#include "target.h"
#include "text.h"

/* What a test run of a project takes from the build of its modules: the
   package name and the version that every compile carries, the source
   directory, the directory of the library files of the modules and the
   library files of the dependency graph, which stand under no root.
   obj_dir is the directory of the objects of the build of the mode, which
   the test run caches its own files in as well. The rest is what
   build_object compiles a dev object with, as the build compiles it:
   the runtime archive, the target and its level, -g and --memory-checks,
   the injections of `[inject]` and the library files every object
   compile of the build names. */
struct build_tested {
    struct text package;
    struct text version;
    struct text src;
    struct text lib_dir;
    struct text obj_dir;
    struct text *libraries;
    size_t library_count;
    struct text runtime;
    enum target target;
    enum cpu_level cpu;
    bool debug;
    bool memory_checks;
    struct text *inject;
    size_t inject_count;
    struct text *compiled;
    size_t compiled_count;
};

/* Free what a build wrote into t, and leave t empty. */
void build_tested_free(struct build_tested *t);

/* Write the dev object of the library file library into obj/ of the
   build t records, unless the cache holds it, and its path into out.
   By Q2 of "Answers of 2026-11-06, fourth set" in docs/decisions.md a
   dev run of `anti test` takes the object of a module it does not test
   from there, which is the object `anti build` writes. Returns false
   when the compile fails. */
bool build_object(const struct build_tested *t, const char *library,
                  struct text *out);

/* What `anti build` and `anti run` were asked for. A field that names
   nothing is NULL, and the manifest or the host decides in its place. */
struct build_request {
    const char *root;           /* the directory that holds the manifest */
    const char *target;         /* a target name, `all`, or NULL */
    const char *cpu;            /* a processor level, or NULL */
    const char *runtime;        /* the runtime archive, or NULL */
    const char *llvm_ar;
    bool release;
    bool offline;
    bool strip_docs;
    bool memory_checks;         /* --memory-checks, passed to antic */
    /* --profile-generate and --profile-use <file>, passed to the antic
       call of a release program. */
    bool profile_generate;
    const char *profile_use;
    /* --lto full|thin|none, the word passed to the antic call of a
       release program, or NULL for the default of antic. */
    const char *lto;
    /* --lib static and --lib shared build a library for C rather than a
       program. Without one a project with `main` gives an executable and
       a project without it gives the library files of its modules. */
    enum { BUILD_PROGRAM, BUILD_LIB_STATIC, BUILD_LIB_SHARED } lib;
    bool bundle_runtime;
    bool soname;
    bool run;                   /* `anti run`, which runs what it built */
    /* DESIGN: `anti license --project` is a build that also prints the
       notice of the project, and with `--notice` one that writes
       NOTICE.txt beside whatever it built. A build alone writes the file
       beside a program and a shared library. The build is what knows
       whether the program links musl, so no second command decides
       it. */
    enum { BUILD_NOTICE_BUILD, BUILD_NOTICE_WRITE, BUILD_NOTICE_PRINT } notice;
    /* DESIGN: Q4 of "Answers of 2026-11-06, second set" in
       docs/decisions.md. `anti test` of a project writes or reuses the
       library file of every module of the project, as a build of its mode
       does, and links nothing. The build then fills tested. NULL for every
       other build. */
    struct build_tested *tested;
};

/* Build the project and return the exit status of anti. */
int build_run(const struct build_request *r);

#endif

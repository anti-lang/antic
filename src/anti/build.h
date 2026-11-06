#ifndef ANTI_BUILD_H
#define ANTI_BUILD_H

/* The interface of build.c: `anti build` and `anti run`. */

#include <stdbool.h>
#include <stddef.h>

#include "text.h"

/* What a test run of a project takes from the dev build of its modules:
   the package name that every compile carries, the source directory, the
   directory of the library files of the modules and the library files of
   the dependency graph, which stand under no root. */
struct build_tested {
    struct text package;
    struct text src;
    struct text lib_dir;
    struct text *libraries;
    size_t library_count;
};

/* Free what a build wrote into t, and leave t empty. */
void build_tested_free(struct build_tested *t);

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
       library file of every module of the project, as a dev build does,
       and links nothing. The build then fills tested. NULL for every
       other build. */
    struct build_tested *tested;
};

/* Build the project and return the exit status of anti. */
int build_run(const struct build_request *r);

#endif

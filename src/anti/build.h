#ifndef ANTI_BUILD_H
#define ANTI_BUILD_H

#include <stdbool.h>

/* What `anti build` and `anti run` were asked for. A field that names
   nothing is NULL, and the manifest or the host decides in its place. */
struct build_request {
    const char *root;           /* the directory that holds the manifest */
    const char *target;         /* a target name, `all`, or NULL */
    const char *cpu;            /* a processor level, or NULL */
    const char *runtime;        /* the runtime archive, or NULL */
    const char *llvm_mc;
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
};

/* Build the project and return the exit status of anti. */
int build_run(const struct build_request *r);

#endif

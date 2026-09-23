#ifndef ANTI_BUILD_H
#define ANTI_BUILD_H

#include <stdbool.h>
#include <stddef.h>

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
    /* --lib static and --lib shared build a library for C rather than a
       program. Without one a project with `main` gives an executable and
       a project without it gives the library files of its modules. */
    enum { BUILD_PROGRAM, BUILD_LIB_STATIC, BUILD_LIB_SHARED } lib;
    bool bundle_runtime;
    bool soname;
    bool run;                   /* `anti run`, which runs what it built */
};

/* Build the project and return the exit status of anti. */
int build_run(const struct build_request *r);

/* `anti new <name>`: a project of the default layout, with a starter
   manifest and one module that prints and returns. name is the package
   name, a module path of at least two segments, and the directory takes
   its last segment. */
int build_new(const char *name);

#endif

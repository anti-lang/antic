#ifndef ANTIC_DRIVER_H
#define ANTIC_DRIVER_H

#include <stdbool.h>

#include "cpu.h"
#include "linker.h"
#include "target.h"

struct options {
    const char *input;          /* The .anti source file. */
    const char *output;         /* NULL: input without .anti. */
    const char *llvm_mc;        /* NULL: llvm-mc from PATH. */
    const char *runtime;        /* Holds lib/<target>/. */
    const char **libraries;     /* The .antl files. */
    size_t library_count;
    const char **objects;       /* Object files and archives to link. */
    size_t object_count;
    const char **roots;         /* -I, the search roots of module paths. */
    size_t root_count;
    bool internal;              /* --anti-internal, allow anti. paths. */
    bool strip_docs;            /* --strip-docs, no doc text with -c. */
    bool doc_warnings;          /* --doc-warnings, as anti check passes. */
    /* DESIGN: --front-end stops after semantic analysis and writes
       nothing, which is the one build with no artifact. `anti check`
       passes it, once per target. The module is checked as a library and
       not as a program, because a check of one file cannot know which
       module of a project fills an abstract class. */
    bool front_end;
    bool dev;                   /* --dev, one module into its own object. */
    bool no_reflect;            /* --no-reflect, no field list in a
                                   class descriptor. */
    /* DESIGN: -g writes the source positions of the program and keeps the
       debug sections of the link. anti build passes it in dev mode, and
       release mode never does. */
    bool debug;
    /* DESIGN: --tests keeps the `tests` and `fixtures` blocks and is what
       `anti test` passes. Every other build drops them after parsing, so
       no later pass sees them and no object or `.antl` carries them. */
    bool tests;
    /* DESIGN: dev mode keeps assertions and release mode drops them,
       because dev compiles one module and release the whole program.
       --asserts and --no-asserts decide instead of the mode. */
    enum { ASSERTS_MODE, ASSERTS_ON, ASSERTS_OFF } asserts;
    /* The dev-mode checks follow the mode the same way, under their own
       pair of options. --checks and --no-checks decide instead of it. */
    enum { CHECKS_MODE, CHECKS_ON, CHECKS_OFF } checks;
    /* DESIGN: the contextual `trace` follows the mode as `assert` does,
       and --trace and --no-trace decide instead of it. --trace <pattern>
       instruments a package or a class that did not ask, in any mode,
       and --trace writes adds the `changed` hook after a write.
       --no-hooks drops every hook site, the five always-on ones as
       well. */
    enum { TRACE_MODE, TRACE_ON, TRACE_OFF } trace;
    bool trace_writes;          /* --trace writes */
    bool no_hooks;              /* --no-hooks */
    const char **trace_patterns;    /* --trace <pattern> */
    size_t trace_pattern_count;
    /* DESIGN: --inject Interface=Provider names the provider of an
       injectable interface, once per interface. `anti build` passes the
       `[inject]` table of the manifest and `anti test` the
       `[inject.test]` table over it. The link refuses an interface the
       program injects and the table does not name. */
    const char **inject;            /* --inject Interface=Provider */
    size_t inject_count;
    enum { LIB_NONE, LIB_STATIC, LIB_SHARED } lib; /* --lib static|shared */
    bool bundle_runtime;        /* --bundle-runtime, with --lib static. */
    bool soname;                /* --soname, with --lib shared. */
    /* DESIGN: --no-runtime, with --lib shared, writes a plugin. It links
       no runtime and no standard library. The host it is loaded into
       holds both, and the loader resolves every symbol of the plugin
       against the host at load. The library then carries the table of
       what it provides and nothing of the runtime. */
    bool no_runtime;
    /* DESIGN: --closed builds a program without the dynamic exports of
       its runtime symbols and its descriptors. It then loads no plugin,
       because a plugin resolves those symbols against its host. */
    bool closed;
    const char *llvm_ar;        /* NULL: llvm-ar from PATH. */
    const char **frameworks;    /* --framework, macOS frameworks of Apple's
                                   SDK. */
    size_t framework_count;
    enum linker linker;         /* --linker lld|platform, lld by default. */
    const char *package_name;   /* --package-name, of the header. */
    const char *package_version;
    const char **dependencies;  /* --dependency <name>,<constraint>,<url> */
    size_t dependency_count;
    const char *license;        /* --license, an SPDX identifier. */
    const char *license_text;   /* --license-text, a file. */
    const char **attribution;   /* --attribution, one line each. */
    size_t attribution_count;
    bool library;               /* -c, write a library file. */
    bool assembly_only;         /* -S, stop after assembly. */
    bool dump_tokens;           /* --dump-tokens. */
    bool dump_ast;              /* --dump-ast. */
    bool dump_types;            /* --dump-types. */
    bool dump_ir;               /* --dump-ir. */
    bool dump_opt;              /* --dump-opt. */
    bool dump_select;           /* --dump-select. */
    bool dump_alloc;            /* --dump-alloc. */
    enum target target;
    /* DESIGN: the processor level is a code-generation setting, not a
       target. --cpu sets it and the target's default stands otherwise. */
    enum cpu_level cpu;
};

/* Compile options->input and return the process exit status for antic. */
int driver_run(const struct options *options);
struct arena;

/* The library files that options->libraries needs: those files, every
   module each of them imports, and every module below those. The paths
   go into the memory pool, and `paths` points at them. A dev build
   compiles one module into its own object, so `anti test` reads the list
   to write an object of every module the runner links. Returns false
   when a file cannot be read. */
bool driver_libraries(const struct options *options, struct arena *arena,
                      const char ***paths, size_t *count);


#endif

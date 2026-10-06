#ifndef ANTIC_LINKER_H
#define ANTIC_LINKER_H

#include <stdbool.h>
#include <stddef.h>

#include "antic.h"
#include "cpu.h"
#include "target.h"
#include "text.h"

/* The files of a link and the facts about the host that the command line
   needs. */
struct link_inputs {
    const char *object;
    const char *executable;
    const char *runtime;        /* the directory of the runtime archive */
    const char *sdk_path;       /* macOS: xcrun or frameworks */
    const char *sdk_version;    /* macOS: its version */
    const char *crt_dir;        /* Linux: the directory that holds Scrt1.o */
    const char *const *extra;   /* object files and archives to link */
    size_t extra_count;
    enum linker linker;
    enum cpu_level cpu;         /* the level of the runtime to link */
    const char *sysroot;        /* lld: <runtime>/sysroot/<target> */
    const char *lld_dir;        /* lld: its directory, or NULL for PATH */
    const char *const *frameworks; /* macOS: -framework */
    size_t framework_count;
    /* DESIGN: -g keeps the debug sections. Without it every link strips
       them, because the only source of them would be the C library of the
       target. No line of Anti would be in them. */
    bool debug;
    /* DESIGN: a program that can host a plugin exports the names in its
       symbol table, so the loader binds the plugin against them. */
    bool exports;
    const char *const *linux_libraries; /* Linux: -l of the glibc mode */
    size_t linux_library_count;
    /* DESIGN: a Linux program links dynamically against glibc when a
       module it imports names a library with `link linux`, when it can
       host a plugin or under --memory-checks. Every other Linux program
       links statically against musl. A shared library of such a module
       links against the glibc sysroot and the glibc runtime. */
    bool glibc;
    /* Windows: the .def file of the names a program that hosts a plugin
       exports, and the import library the link writes beside it. */
    const char *def_file;
    const char *import_library;
    /* --memory-checks: link the runtime of AddressSanitizer that the
       runtime archive holds beside the native libraries. */
    bool memory_checks;
    /* macOS with --memory-checks: the absolute directory of that
       runtime, where the program finds it at run time. */
    const char *rpath;
    /* --lto: object is bitcode, and the link takes the runtime as
       bitcode of the mode. */
    enum lto lto;
    /* --profile-generate: link the profile runtime that the runtime
       archive holds beside the native libraries. */
    bool profile_generate;
};

/* DESIGN: a macOS link of --lto with -g keeps the objects that the LTO of
   ld64.lld writes in the directory <executable>LINK_LTO_OBJECTS_SUFFIX.
   Mach-O leaves the debug information in the objects, and the program
   names them by path. */
#define LINK_LTO_OBJECTS_SUFFIX ".lto"

/* The word of --lto that names mode, which names its directory of the
   runtime as bitcode as well. */
const char *link_lto_name(enum lto mode);

/* Set *mode to the mode that name names, and answer whether one does. */
bool link_lto_from_name(const char *name, enum lto *mode);

/* The suffixes of the object files and archives that antic passes to the
   linker. */
#define LINK_OBJECT_SUFFIX ".o"
#define LINK_ARCHIVE_SUFFIX ".a"
#define LINK_COFF_OBJECT_SUFFIX ".obj"
#define LINK_COFF_ARCHIVE_SUFFIX ".lib"

enum { LINK_MAX_STRINGS = 16 };

/* A linker command line. The arguments that it builds live in strings,
   and argv holds the fixed arguments and every extra input. */
struct link_command {
    const char **argv;
    size_t argc;
    size_t capacity;
    struct text strings[LINK_MAX_STRINGS];
    size_t string_count;
};

/* Build the command line that links in->object for target t. argv ends
   with NULL. */
void link_command(struct link_command *c, enum target t,
                  const struct link_inputs *in);
void link_command_free(struct link_command *c);

/* The version facts of a shared library, NULL when absent. The .def file
   names the exports of a DLL. */
struct shared_options {
    const char *def_file;
    const char *major;          /* --soname: the compatibility version */
    const char *version;        /* --soname: the full version */
    /* macOS: the file of the exported symbols. Only the export
       functions and anti_licenses stand in it, so a shared library for
       C shows the documented surface and no name of the runtime. */
    const char *exported_file;
    /* DESIGN: --no-runtime writes a plugin. It links no runtime and no
       standard library, and every name it needs is resolved against the
       host that loads it. */
    bool plugin;
};

/* Build the command line of a shared library of in->object at
   in->executable. */
void link_shared_command(struct link_command *c, enum target t,
                         const struct link_inputs *in,
                         const struct shared_options *s);

/* Build the llvm-ar command line that writes archive from the members. */
void link_archive_command(struct link_command *c, enum target t,
                          const char *llvm_ar, const char *archive,
                          const char *const *members, size_t count);

/* Build the command line that joins objects into one relocatable object
   for a bundled runtime, with the linker of in. */
void link_relocatable_command(struct link_command *c, enum target t,
                              const struct link_inputs *in, const char *output,
                              const char *const *objects, size_t count);

/* Append the command line that links a C program main.c with the static
   library, as antic prints it. A bundled runtime needs no runtime library. */
void link_line(struct text *out, enum target t, const char *library,
               const char *runtime, enum cpu_level cpu, bool bundled);

/* Whether path names an object file or an archive for the linker. */
bool link_is_input(const char *path);

/* Append the path of the runtime library of target t at level cpu below
   runtime. */
void link_runtime_library(struct text *out, const char *runtime, enum target t,
                          enum cpu_level cpu);

/* Append the path of the runtime as bitcode of mode for target t at level
   cpu below runtime. */
void link_runtime_bitcode(struct text *out, const char *runtime, enum target t,
                          enum cpu_level cpu, enum lto mode);

/* The native library of `anti.regex`, PCRE2 as src/native/ names it, and
   the runtime library of its glue, which src/native/pcre2.cmake builds
   beside it. */
#define NATIVE_PCRE2 "pcre2-8"
#define NATIVE_REGEX_GLUE "anti_rt_regex"

/* DESIGN: the runtime archive carries the runtime of AddressSanitizer
   of the pinned clang in lib/<target>/, under the names compiler-rt
   gives it. macOS links the dynamic library and finds it at run time
   through an rpath to that directory. Linux links the static archives
   into a program of the glibc mode, whose symbols the list of .syms
   exports, and libunwind.a gives the runtime _Unwind_Backtrace and
   _Unwind_GetIP, which glibc does not. Windows links the import library
   and the thunk, and the DLL stands beside the program. CMakeLists.txt
   copies the same names. */
#define MEMCHECK_MACOS_DYLIB "libclang_rt.asan_osx_dynamic.dylib"
#define MEMCHECK_LINUX_ARCHIVE "libclang_rt.asan.a"
#define MEMCHECK_LINUX_STATIC "libclang_rt.asan_static.a"
#define MEMCHECK_LINUX_SYMS "libclang_rt.asan.a.syms"
#define MEMCHECK_LINUX_UNWIND "libunwind.a"
#define MEMCHECK_WINDOWS_LIB "clang_rt.asan_dynamic.lib"
#define MEMCHECK_WINDOWS_THUNK "clang_rt.asan_dynamic_runtime_thunk.lib"
#define MEMCHECK_WINDOWS_DLL "clang_rt.asan_dynamic.dll"

/* DESIGN: the runtime archive carries the profile runtime of compiler-rt
   of the pinned clang in lib/<target>/, under the names compiler-rt gives
   it, one file per target for every level. The text of an instrumented
   program on Mach-O and COFF refers to PROFILE_RUNTIME_HOOK itself, which
   draws in the code that writes the profile at exit. On ELF it does not,
   so the link names the hook as undefined, as clang's driver does.
   CMakeLists.txt copies the same names. */
#define PROFILE_RUNTIME_ELF "libclang_rt.profile.a"
#define PROFILE_RUNTIME_MACOS "libclang_rt.profile_osx.a"
#define PROFILE_RUNTIME_WINDOWS "clang_rt.profile.lib"
#define PROFILE_RUNTIME_HOOK "__llvm_profile_runtime"

/* Append the path of the profile runtime of target t below runtime. With
   glibc it lies in the directory of the glibc mode. */
void link_profile_runtime(struct text *out, const char *runtime,
                          enum target t, bool glibc);

/* Append the path of the file name of the runtime of AddressSanitizer
   for target t below runtime. With glibc it lies in the directory of
   the glibc mode. */
void link_memcheck_file(struct text *out, const char *runtime, enum target t,
                        bool glibc, const char *name);

/* Append the path of the native library name of target t below runtime,
   which stays at the default level of the target. */
void link_native_library(struct text *out, const char *runtime, enum target t,
                         const char *name);

/* Append the directory name of target t below the runtime's lib/ and
   sysroot/, with LINUX_GLIBC_SUFFIX in the glibc mode. */
void link_target_dir(struct text *out, enum target t, bool glibc);

/* The flavour of lld that links for target t, a program of
   <runtime>/RUNTIME_BIN_DIR/ without the suffix of the host. */
const char *link_lld_flavour(enum target t);

/* Append the path, relative to the sysroot of target t, of the file whose
   presence shows that the sysroot is complete. With glibc it is the
   sysroot of the glibc mode. */
void link_sysroot_marker(struct text *out, enum target t, bool glibc);

/* The directories that may hold the glibc start files for a Linux target,
   in the order of search, ending with NULL. */
const char *const *link_crt_dirs(enum target t);

#endif

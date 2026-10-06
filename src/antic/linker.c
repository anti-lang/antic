#include "linker.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "ir.h"

static bool ends_with(const char *s, const char *suffix)
{
    size_t n = strlen(s);
    size_t m = strlen(suffix);

    return n >= m && strcmp(s + n - m, suffix) == 0;
}

bool link_is_input(const char *path)
{
    return ends_with(path, LINK_OBJECT_SUFFIX) ||
           ends_with(path, LINK_ARCHIVE_SUFFIX) ||
           ends_with(path, LINK_COFF_OBJECT_SUFFIX) ||
           ends_with(path, LINK_COFF_ARCHIVE_SUFFIX);
}

/* The object of the program, then the extra inputs. */
static void add_inputs(struct link_command *c, const struct link_inputs *in);

static void add(struct link_command *c, const char *arg)
{
    c->argv = alloc_grow(c->argv, &c->capacity, c->argc + 1, sizeof *c->argv);
    c->argv[c->argc++] = arg;
    c->argv[c->argc] = NULL;
}

/* The string of the next argument that the command builds. Add it with
   add once it is complete, because appending moves its bytes. A caller
   holds the pointer while it builds, so the array cannot move, and
   LINK_MAX_STRINGS bounds the strings of the longest command. */
static struct text *next(struct link_command *c)
{
    if (c->string_count >= LINK_MAX_STRINGS) {
        /* Unreachable while LINK_MAX_STRINGS covers every command. */
        fputs("antic: a link command needs more than LINK_MAX_STRINGS "
              "strings\n", stderr);
        abort();
    }
    return &c->strings[c->string_count++];
}

void link_target_dir(struct text *out, enum target t, bool glibc)
{
    text_appendf(out, "%s%s", target_name(t), glibc ? LINUX_GLIBC_SUFFIX : "");
}

void link_native_library(struct text *out, const char *runtime, enum target t,
                         const char *name)
{
    text_appendf(out, "%s/%s/", runtime, RUNTIME_LIB_DIR);
    link_target_dir(out, t, false);
    text_appendf(out, target_info(t)->format == FORMAT_COFF ? "/%s.lib"
                                                            : "/lib%s.a",
                 name);
}

void link_memcheck_file(struct text *out, const char *runtime, enum target t,
                        bool glibc, const char *name)
{
    text_appendf(out, "%s/%s/", runtime, RUNTIME_LIB_DIR);
    link_target_dir(out, t, glibc);
    text_appendf(out, "/%s", name);
}

void link_profile_runtime(struct text *out, const char *runtime,
                          enum target t, bool glibc)
{
    static const char *const names[] = {
        [OS_LINUX] = PROFILE_RUNTIME_ELF,
        [OS_MACOS] = PROFILE_RUNTIME_MACOS,
        [OS_WINDOWS] = PROFILE_RUNTIME_WINDOWS,
    };

    text_appendf(out, "%s/%s/", runtime, RUNTIME_LIB_DIR);
    link_target_dir(out, t, glibc);
    text_appendf(out, "/%s", names[target_info(t)->os]);
}

/* The words of --lto by mode, LTO_NONE having none. */
static const char *const lto_names[] = {NULL, "full", "thin"};

const char *link_lto_name(enum lto mode)
{
    return lto_names[mode];
}

bool link_lto_from_name(const char *name, enum lto *mode)
{
    size_t i;

    for (i = 1; i < sizeof lto_names / sizeof lto_names[0]; i++) {
        if (strcmp(name, lto_names[i]) == 0) {
            *mode = (enum lto)i;
            return true;
        }
    }
    return false;
}

/* The runtime library of t at level cpu, of the glibc mode with glibc,
   and the runtime as bitcode of mode lto unless it is LTO_NONE. */
static void runtime_library(struct text *out, const char *runtime,
                            enum target t, enum cpu_level cpu, bool glibc,
                            enum lto lto)
{
    /* DESIGN: MSVC names a static library name.lib, and the other
       toolchains libname.a. The level names the directory, because the
       archive holds one runtime per level of the target. */
    text_appendf(out, "%s/%s/", runtime, RUNTIME_LIB_DIR);
    link_target_dir(out, t, glibc);
    text_appendf(out, "/%s/", cpu_name(cpu));
    if (lto != LTO_NONE) {
        text_appendf(out, "%s/%s/", RUNTIME_BITCODE_DIR, link_lto_name(lto));
    }
    text_append(out, target_info(t)->format == FORMAT_COFF ? "anti_rt.lib"
                                                           : "libanti_rt.a");
}

void link_runtime_library(struct text *out, const char *runtime, enum target t,
                          enum cpu_level cpu)
{
    runtime_library(out, runtime, t, cpu, false, LTO_NONE);
}

void link_runtime_bitcode(struct text *out, const char *runtime, enum target t,
                          enum cpu_level cpu, enum lto mode)
{
    runtime_library(out, runtime, t, cpu, false, mode);
}

const char *const *link_crt_dirs(enum target t)
{
    /* DESIGN: Debian and Ubuntu install glibc in the multiarch directory,
       other distributions in /usr/lib64 or /usr/lib. */
    static const char *const x86_64[] = {
        "/usr/lib/x86_64-linux-gnu", "/usr/lib64", "/usr/lib", NULL
    };
    static const char *const arm64[] = {
        "/usr/lib/aarch64-linux-gnu", "/usr/lib64", "/usr/lib", NULL
    };
    return target_info(t)->arch == ARCH_ARM64 ? arm64 : x86_64;
}

/* The flavour of lld and the platform linker of each system. */
static const char *const lld_flavours[] = {
    [OS_LINUX] = "ld.lld", [OS_MACOS] = "ld64.lld", [OS_WINDOWS] = "lld-link",
};
static const char *const platform_linkers[] = {
    [OS_LINUX] = "ld", [OS_MACOS] = "ld", [OS_WINDOWS] = "link.exe",
};

const char *link_lld_flavour(enum target t)
{
    return lld_flavours[target_info(t)->os];
}

/* DESIGN: the files of a sysroot that the links name, and the file whose
   presence shows that a sysroot is complete, which the driver reads
   before a link. Both come from these names alone. The builtins are the
   last file tools/get-sysroot.cmake writes into a glibc sysroot. */
#define SYSROOT_LIB "usr/lib"
#define SYSROOT_BUILTINS "libclang_rt.builtins.a"
#define SYSROOT_MUSL_LIBC "libc.a"
#define SYSROOT_WINDOWS_CRT "crt/lib"
#define WINDOWS_MSVCRT "msvcrt.lib"
#define WINDOWS_STATIC_CRT "libcmt.lib"

/* The directory of the processor in the library directories of a Windows
   sysroot. */
static const char *windows_arch(enum target t)
{
    return target_info(t)->arch == ARCH_ARM64 ? "aarch64" : "x86_64";
}

void link_sysroot_marker(struct text *out, enum target t, bool glibc)
{
    switch (target_info(t)->os) {
    case OS_LINUX:
        text_appendf(out, "%s/%s", SYSROOT_LIB,
                     glibc ? SYSROOT_BUILTINS : SYSROOT_MUSL_LIBC);
        break;
    case OS_MACOS:
        text_append(out, SYSROOT_SDK_VERSION);
        break;
    case OS_WINDOWS:
        text_appendf(out, "%s/%s/%s", SYSROOT_WINDOWS_CRT, windows_arch(t),
                     WINDOWS_MSVCRT);
        break;
    }
}

/* The program of the linker of target t: the flavour of lld in its
   directory, or the platform linker. */
static const char *program(struct link_command *c, const struct link_inputs *in,
                           enum target t)
{
    enum target_os os = target_info(t)->os;
    struct text *path;

    if (in->linker == LINKER_PLATFORM) {
        return platform_linkers[os];
    }
    if (in->lld_dir == NULL) {
        return lld_flavours[os];
    }
    path = next(c);
    text_appendf(path, "%s/%s", in->lld_dir, lld_flavours[os]);
    return text_cstr(path);
}

/* The start of a Mach-O link: -arch, the versions and libSystem's root.
   ld64 of Apple takes the SDK that xcrun names. ld64.lld takes the stubs
   of the sysroot, or Apple's SDK for a program that names a framework. */
static void macos_start(struct link_command *c, enum target t,
                        const struct link_inputs *in, bool dylib)
{
    const char *linker = program(c, in, t);
    struct text *version = next(c);

    text_appendf(version, "%d.%d", MACOS_MIN_MAJOR, MACOS_MIN_MINOR);
    add(c, linker);
    if (dylib) {
        add(c, "-dylib");
    }
    if (!in->debug) {
        add(c, "-S");
    }
    add(c, "-arch");
    add(c, target_info(t)->arch == ARCH_ARM64 ? "arm64" : "x86_64");
    add(c, "-platform_version");
    add(c, "macos");
    add(c, text_cstr(version));
    add(c, in->sdk_version);
    add(c, "-syslibroot");
    add(c, in->linker == LINKER_LLD && in->framework_count == 0 ? in->sysroot
                                                               : in->sdk_path);
}

/* The frameworks of a macOS link, which follow libSystem. */
static void macos_frameworks(struct link_command *c,
                             const struct link_inputs *in)
{
    size_t i;

    for (i = 0; i < in->framework_count; i++) {
        add(c, "-framework");
        add(c, in->frameworks[i]);
    }
}

/* The runtime of AddressSanitizer of a macOS link, and its directory as
   an rpath, where the program or the library finds it at run time. */
static void macos_memcheck(struct link_command *c, enum target t,
                           const struct link_inputs *in)
{
    struct text *dylib;

    if (!in->memory_checks) {
        return;
    }
    dylib = next(c);
    link_memcheck_file(dylib, in->runtime, t, false, MEMCHECK_MACOS_DYLIB);
    add(c, text_cstr(dylib));
    add(c, "-rpath");
    add(c, in->rpath);
}

/* The profile runtime of --profile-generate, which follows the runtime
   of Anti. */
static void profile_runtime(struct link_command *c, enum target t,
                            const struct link_inputs *in, bool glibc)
{
    struct text *file;

    if (!in->profile_generate) {
        return;
    }
    file = next(c);
    link_profile_runtime(file, in->runtime, t, glibc);
    add(c, text_cstr(file));
}

/* An ELF link of --profile-generate names the hook of the profile
   runtime as undefined, which draws in the code that writes the
   profile. */
static void profile_hook(struct link_command *c, const struct link_inputs *in)
{
    if (in->profile_generate) {
        add(c, "-u");
        add(c, PROFILE_RUNTIME_HOOK);
    }
}

/* DESIGN: the LTO of lld runs at the level of opt in release mode, -O2,
   whatever default lld has. */
static void lto_level(struct link_command *c, enum target t,
                      const struct link_inputs *in)
{
    if (in->lto != LTO_NONE) {
        add(c, target_info(t)->os == OS_WINDOWS ? "/opt:lldlto=2"
                                                : "--lto-O2");
    }
}

static void macos(struct link_command *c, enum target t,
                  const struct link_inputs *in)
{
    struct text *library = next(c);

    runtime_library(library, in->runtime, t, in->cpu, false, in->lto);
    macos_start(c, t, in, false);
    add(c, "-o");
    add(c, in->executable);
    lto_level(c, t, in);
    if (in->lto != LTO_NONE && in->debug) {
        struct text *objects = next(c);
        text_appendf(objects, "%s%s", in->executable, LINK_LTO_OBJECTS_SUFFIX);
        add(c, "-object_path_lto");
        add(c, text_cstr(objects));
    }
    /* DESIGN: a plugin is bound against the host at load, so the host
       keeps every name its own objects define in its export table. */
    if (in->exports) {
        add(c, "-export_dynamic");
    }
    add_inputs(c, in);
    add(c, text_cstr(library));
    profile_runtime(c, t, in, false);
    macos_memcheck(c, t, in);
    add(c, "-lSystem");
    macos_frameworks(c, in);
}

/* The archives of AddressSanitizer in a Linux program, whole, as clang
   links them, before the objects of the program. The list of .syms keeps
   the names the runtime intercepts in the dynamic symbol table. */
static void linux_memcheck(struct link_command *c, enum target t,
                           const struct link_inputs *in)
{
    struct text *files[2];
    struct text *list;
    size_t i;

    if (!in->memory_checks) {
        return;
    }
    files[0] = next(c);
    files[1] = next(c);
    list = next(c);
    link_memcheck_file(files[0], in->runtime, t, true, MEMCHECK_LINUX_STATIC);
    link_memcheck_file(files[1], in->runtime, t, true, MEMCHECK_LINUX_ARCHIVE);
    text_append(list, "--dynamic-list=");
    link_memcheck_file(list, in->runtime, t, true, MEMCHECK_LINUX_SYMS);
    for (i = 0; i < 2; i++) {
        add(c, "--whole-archive");
        add(c, text_cstr(files[i]));
        add(c, "--no-whole-archive");
    }
    add(c, text_cstr(list));
}

/* The unwinder and the libraries of glibc that the runtime of
   AddressSanitizer calls. */
static void linux_memcheck_libraries(struct link_command *c, enum target t,
                                     const struct link_inputs *in)
{
    struct text *unwind;

    if (in->memory_checks) {
        unwind = next(c);
        link_memcheck_file(unwind, in->runtime, t, true, MEMCHECK_LINUX_UNWIND);
        add(c, text_cstr(unwind));
        add(c, "-lpthread");
        add(c, "-lrt");
        add(c, "-ldl");
        add(c, "-lresolv");
    }
}

/* The dynamic linker of a Linux program linked against glibc. */
static const char *glibc_interpreter(enum target t)
{
    return target_info(t)->arch == ARCH_ARM64 ? "/lib/ld-linux-aarch64.so.1"
                                              : "/lib64/ld-linux-x86-64.so.2";
}

/* The -l arguments of the `link linux` libraries. */
static void linux_libraries(struct link_command *c,
                            const struct link_inputs *in)
{
    size_t i;

    for (i = 0; i < in->linux_library_count; i++) {
        add(c, "-l");
        add(c, in->linux_libraries[i]);
    }
}

/* The directory of the libraries of glibc in its sysroot. */
static const char *glibc_triple(enum target t)
{
    return target_info(t)->arch == ARCH_ARM64 ? "aarch64-linux-gnu"
                                              : "x86_64-linux-gnu";
}

/* The libraries of the glibc mode after the runtime: the directories of
   the sysroot, the libraries of `link linux`, those of AddressSanitizer
   for an executable, libm, libc and the builtins. A shared library
   leaves the runtime of AddressSanitizer to the program that loads it. */
static void glibc_libraries(struct link_command *c, enum target t,
                            const struct link_inputs *in, bool executable)
{
    struct text *search = next(c);
    struct text *shared = next(c);
    struct text *builtins = next(c);

    text_appendf(search, "-L%s/%s/%s", in->sysroot, SYSROOT_LIB,
                 glibc_triple(t));
    text_appendf(shared, "-L%s/lib/%s", in->sysroot, glibc_triple(t));
    text_appendf(builtins, "%s/%s/%s", in->sysroot, SYSROOT_LIB,
                 SYSROOT_BUILTINS);
    add(c, text_cstr(search));
    add(c, text_cstr(shared));
    linux_libraries(c, in);
    if (executable) {
        linux_memcheck_libraries(c, t, in);
    }
    add(c, "-lm");
    add(c, "-lc");
    add(c, text_cstr(builtins));
}

/* DESIGN: the glibc mode links a position-independent executable against
   glibc 2.35 of the sysroot, with its dynamic linker. --sysroot makes
   the absolute paths of the linker script libc.so name files of the
   sysroot. The libraries of `link linux` come before libm and libc. Every
   program of the mode takes libm, because musl carries it in libc.a and
   a program of the static mode reaches it there. A program that can host
   a plugin exports its names with --export-dynamic. */
static void linux_glibc(struct link_command *c, enum target t,
                        const struct link_inputs *in)
{
    static const char *const before[] = {"Scrt1.o", "crti.o"};
    const char *triple = glibc_triple(t);
    const char *linker = program(c, in, t);
    struct text *library = next(c);
    struct text *sysroot = next(c);
    struct text *interpreter = next(c);
    struct text *crtn = next(c);
    size_t i;

    runtime_library(library, in->runtime, t, in->cpu, true, in->lto);
    text_appendf(sysroot, "--sysroot=%s", in->sysroot);
    text_appendf(interpreter, "--dynamic-linker=%s", glibc_interpreter(t));
    text_appendf(crtn, "%s/%s/%s/crtn.o", in->sysroot, SYSROOT_LIB, triple);
    add(c, linker);
    add(c, text_cstr(sysroot));
    add(c, "-pie");
    add(c, text_cstr(interpreter));
    if (in->exports) {
        add(c, "--export-dynamic");
    }
    if (!in->debug) {
        add(c, "--strip-debug");
    }
    lto_level(c, t, in);
    profile_hook(c, in);
    add(c, "-o");
    add(c, in->executable);
    for (i = 0; i < 2; i++) {
        struct text *file = next(c);
        text_appendf(file, "%s/%s/%s/%s", in->sysroot, SYSROOT_LIB, triple,
                     before[i]);
        add(c, text_cstr(file));
    }
    linux_memcheck(c, t, in);
    add_inputs(c, in);
    add(c, text_cstr(library));
    profile_runtime(c, t, in, true);
    glibc_libraries(c, t, in, true);
    add(c, text_cstr(crtn));
}

/* DESIGN: ld.lld links a Linux program statically against the musl of
   the sysroot. The executable is position-independent and has no dynamic
   linker, so it runs on any Linux kernel. It drops the debug sections,
   which come from the musl of Alpine and weigh more than the program.
   antic writes no debug information of its own, so nothing of the
   program is lost, and the symbol table stays. */
static void linux_lld(struct link_command *c, enum target t,
                      const struct link_inputs *in)
{
    static const char *const before[] = {"rcrt1.o", "crti.o"};
    static const char *const after[] = {SYSROOT_MUSL_LIBC, SYSROOT_BUILTINS,
                                        "crtn.o"};
    const char *linker = program(c, in, t);
    struct text *library = next(c);
    size_t i;

    runtime_library(library, in->runtime, t, in->cpu, false, in->lto);
    add(c, linker);
    add(c, "-static");
    add(c, "-pie");
    add(c, "--no-dynamic-linker");
    if (!in->debug) {
        add(c, "--strip-debug");
    }
    lto_level(c, t, in);
    profile_hook(c, in);
    add(c, "-o");
    add(c, in->executable);
    for (i = 0; i < 2; i++) {
        struct text *file = next(c);
        text_appendf(file, "%s/%s/%s", in->sysroot, SYSROOT_LIB, before[i]);
        add(c, text_cstr(file));
    }
    add_inputs(c, in);
    add(c, text_cstr(library));
    profile_runtime(c, t, in, false);
    for (i = 0; i < 3; i++) {
        struct text *file = next(c);
        text_appendf(file, "%s/%s/%s", in->sysroot, SYSROOT_LIB, after[i]);
        add(c, text_cstr(file));
    }
}

/* DESIGN: every Windows link passes /DEBUG, with -g and without it.
   lld-link writes the debug information of a program to a PDB rather than
   into the executable, so the flag adds no section to what ships. What it
   adds is the CodeView record of the debug directory. That record holds
   the GUID of the PDB and its file name. That GUID is the Windows form of the
   build id. It is the one thing that ties a shipped program to the
   symbols of its own build. Those symbols are what the archive of a
   release carries. /PDBALTPATH:%_PDB% keeps the record to the file name,
   so no path of the machine that linked the program goes out with it.
   /pdbsourcepath:. is the Windows form of the rule that nothing shipped
   holds a build path. lld-link makes each relative file name of a PDB
   absolute against the directory of the link. With the flag, a line table
   names its source by the path under the search root. link.exe knows no
   such flag and says so with warning LNK4044, so it goes to lld-link
   alone.

   The objects of the Microsoft C runtime name PDBs that no machine here
   holds, and lld-link warns once per object when it cannot read one.
   /ignore:4099 drops that warning, which says nothing about the program
   being linked. */
static void windows_debug(struct link_command *c, const struct link_inputs *in)
{
    add(c, "/DEBUG");
    add(c, "/PDBALTPATH:%_PDB%");
    if (in->linker == LINKER_LLD) {
        add(c, "/pdbsourcepath:.");
    }
    add(c, "/ignore:4099");
}

/* The PDB takes the name of the output with its suffix replaced, as the
   linker names it by default, and stands beside it. */
void link_pdb_path(struct text *out, const char *executable)
{
    const char *slash = strrchr(executable, '/');
    const char *dot = strrchr(slash != NULL ? slash : executable, '.');

    text_appendf(out, "%.*s.pdb",
                 (int)(dot != NULL ? (size_t)(dot - executable)
                                   : strlen(executable)),
                 executable);
}

/* The output and the PDB of a Windows link. */
static void windows_output(struct link_command *c, const struct link_inputs *in)
{
    struct text *output = next(c);
    struct text *pdb = next(c);

    text_appendf(output, "/OUT:%s", in->executable);
    text_append(pdb, "/PDB:");
    link_pdb_path(pdb, in->executable);
    add(c, text_cstr(output));
    add(c, text_cstr(pdb));
}

/* The library directories of lld-link: the CRT and the SDK that xwin
   writes into the sysroot. Without a sysroot lld-link reads LIB, as
   link.exe does. */
static void windows_libpaths(struct link_command *c, enum target t,
                             const struct link_inputs *in)
{
    static const char *const dirs[] = {SYSROOT_WINDOWS_CRT, "sdk/lib/um",
                                       "sdk/lib/ucrt"};
    const char *arch = windows_arch(t);
    size_t i;

    if (in->linker != LINKER_LLD || in->sysroot == NULL) {
        return;
    }
    for (i = 0; i < 3; i++) {
        struct text *dir = next(c);
        text_appendf(dir, "/LIBPATH:%s/%s/%s", in->sysroot, dirs[i], arch);
        add(c, text_cstr(dir));
    }
}

/* GNU ld: a position-independent executable with the glibc start
   files, crti.o first and crtn.o last, and the C library. */
static void linux_ld(struct link_command *c, enum target t,
                     const struct link_inputs *in)
{
    struct text *interpreter = next(c);
    struct text *start = next(c);
    struct text *crti = next(c);
    struct text *library = next(c);
    struct text *search = next(c);
    struct text *crtn = next(c);

    text_appendf(interpreter, "--dynamic-linker=%s", glibc_interpreter(t));
    text_appendf(start, "%s/Scrt1.o", in->crt_dir);
    text_appendf(crti, "%s/crti.o", in->crt_dir);
    link_runtime_library(library, in->runtime, t, in->cpu);
    text_appendf(search, "-L%s", in->crt_dir);
    text_appendf(crtn, "%s/crtn.o", in->crt_dir);
    add(c, program(c, in, t));
    add(c, "-pie");
    if (in->exports) {
        add(c, "--export-dynamic");
    }
    if (!in->debug) {
        add(c, "--strip-debug");
    }
    add(c, text_cstr(interpreter));
    add(c, "-o");
    add(c, in->executable);
    add(c, text_cstr(start));
    add(c, text_cstr(crti));
    linux_memcheck(c, t, in);
    add_inputs(c, in);
    add(c, text_cstr(library));
    add(c, text_cstr(search));
    linux_libraries(c, in);
    linux_memcheck_libraries(c, t, in);
    add(c, "-lc");
    add(c, text_cstr(crtn));
}

/* The runtime of AddressSanitizer of a Windows link. The thunk links
   whole, and the handler of structured exceptions stays, as clang links
   them for a program or a DLL of the DLL C runtime. */
static void windows_memcheck(struct link_command *c, enum target t,
                             const struct link_inputs *in)
{
    struct text *dll;
    struct text *thunk;

    if (!in->memory_checks) {
        return;
    }
    dll = next(c);
    thunk = next(c);
    link_memcheck_file(dll, in->runtime, t, false, MEMCHECK_WINDOWS_LIB);
    text_append(thunk, "/WHOLEARCHIVE:");
    link_memcheck_file(thunk, in->runtime, t, false, MEMCHECK_WINDOWS_THUNK);
    add(c, text_cstr(dll));
    add(c, "/INCLUDE:__asan_seh_interceptor");
    add(c, text_cstr(thunk));
}

/* DESIGN: link.exe or lld-link with the C runtime of the machine. The
   Universal CRT is a part of Windows since Windows 10, so ucrt.lib links
   against what the machine already holds. Only vcruntime, the support
   code of the compiler, comes in statically, because that one ships with
   Visual Studio rather than with Windows. The program then needs no
   redistributable, and it weighs 23,040 bytes instead of 91,136. */
static void windows(struct link_command *c, enum target t,
                    const struct link_inputs *in)
{
    const char *linker = program(c, in, t);
    struct text *library = next(c);

    runtime_library(library, in->runtime, t, in->cpu, false, in->lto);
    add(c, linker);
    add(c, "/NOLOGO");
    windows_debug(c, in);
    add(c, "/SUBSYSTEM:CONSOLE");
    add(c, target_info(t)->arch == ARCH_ARM64 ? "/MACHINE:ARM64"
                                              : "/MACHINE:X64");
    lto_level(c, t, in);
    windows_output(c, in);
    /* DESIGN: a program that can host a plugin exports the names of the
       .def file. The link writes the import library its plugins link
       against. A plugin then imports the runtime, the descriptors and
       the functions from the executable by name. */
    if (in->exports && in->def_file != NULL) {
        struct text *def = next(c);
        struct text *implib = next(c);
        text_appendf(def, "/DEF:%s", in->def_file);
        text_appendf(implib, "/IMPLIB:%s", in->import_library);
        add(c, text_cstr(def));
        add(c, text_cstr(implib));
    }
    windows_libpaths(c, t, in);
    windows_memcheck(c, t, in);
    add_inputs(c, in);
    add(c, text_cstr(library));
    /* DESIGN: compiler-rt builds the profile runtime of Windows against
       the static C runtime, and its objects name libcmt.lib. The program
       takes the C runtime of the machine, as above, so the static one
       stays out and the image holds one C library. */
    if (in->profile_generate) {
        profile_runtime(c, t, in, false);
        add(c, "/NODEFAULTLIB:" WINDOWS_STATIC_CRT);
    }
    add(c, WINDOWS_MSVCRT);
    add(c, "libvcruntime.lib");
    add(c, "ucrt.lib");
    /* DESIGN: printf and its family are inline in the headers of the
       UCRT, and ucrt.lib exports none of them. A program that calls
       one through `extern fn` needs the definitions of the older
       form, which this library holds. */
    add(c, "legacy_stdio_definitions.lib");
}

static void add_inputs(struct link_command *c, const struct link_inputs *in)
{
    size_t i;

    add(c, in->object);
    for (i = 0; i < in->extra_count; i++) {
        add(c, in->extra[i]);
    }
}

void link_command(struct link_command *c, enum target t,
                  const struct link_inputs *in)
{
    memset(c, 0, sizeof *c);
    switch (target_info(t)->os) {
    case OS_MACOS: macos(c, t, in); break;
    case OS_LINUX:
        if (in->linker == LINKER_LLD && in->glibc) {
            linux_glibc(c, t, in);
        } else if (in->linker == LINKER_LLD) {
            linux_lld(c, t, in);
        } else {
            linux_ld(c, t, in);
        }
        break;
    case OS_WINDOWS: windows(c, t, in); break;
    }
}

void link_command_free(struct link_command *c)
{
    size_t i;

    for (i = 0; i < c->string_count; i++) {
        text_free(&c->strings[i]);
    }
    free((void *)c->argv);
}

static struct link_command *start(struct link_command *c)
{
    memset(c, 0, sizeof *c);
    return c;
}

/* The link of a shared library on macOS. library is the runtime archive
   of a library for C. */
static void macos_shared(struct link_command *c, enum target t,
                         const struct link_inputs *in,
                         const struct shared_options *s,
                         const struct text *library)
{
    struct text *install = next(c);
    struct text *compat = next(c);
    const char *base = strrchr(in->executable, '/');

    text_appendf(install, "@rpath/%s",
                 base != NULL ? base + 1 : in->executable);
    text_appendf(compat, "%s.0.0", s->major != NULL ? s->major : "");
    macos_start(c, t, in, true);
    add(c, "-o");
    add(c, in->executable);
    if (s->major != NULL) {
        add(c, "-install_name");
        add(c, text_cstr(install));
        add(c, "-compatibility_version");
        add(c, text_cstr(compat));
        add(c, "-current_version");
        add(c, s->version);
    }
    /* A plugin names what the host defines, so the link leaves
       every such name to the loader. */
    if (s->plugin) {
        add(c, "-undefined");
        add(c, "dynamic_lookup");
    }
    /* A shared library for C shows its export functions and
       anti_licenses, and no name of the runtime. */
    if (s->exported_file != NULL) {
        add(c, "-exported_symbols_list");
        add(c, s->exported_file);
    }
    add_inputs(c, in);
    if (!s->plugin) {
        add(c, text_cstr(library));
    }
    macos_memcheck(c, t, in);
    add(c, "-lSystem");
    macos_frameworks(c, in);
}

/* The link of a shared library on Linux, against the glibc sysroot when
   glibc holds. library is the runtime archive of a library for C. */
static void linux_shared(struct link_command *c, enum target t,
                         const struct link_inputs *in,
                         const struct shared_options *s,
                         const struct text *library, bool glibc)
{
    const char *linker = program(c, in, t);
    const char *base = strrchr(in->executable, '/');

    add(c, linker);
    if (glibc) {
        struct text *sysroot = next(c);
        text_appendf(sysroot, "--sysroot=%s", in->sysroot);
        add(c, text_cstr(sysroot));
    }
    add(c, "-shared");
    /* The runtime comes from an archive, and a shared library for C
       shows its export functions alone. */
    if (!s->plugin) {
        add(c, "--exclude-libs");
        add(c, "ALL");
    }
    if (!in->debug) {
        add(c, "--strip-debug");
    }
    add(c, "-o");
    add(c, in->executable);
    if (s->major != NULL) {
        add(c, "-soname");
        add(c, base != NULL ? base + 1 : in->executable);
    }
    add_inputs(c, in);
    if (!s->plugin) {
        add(c, text_cstr(library));
    }
    /* The platform linker searches the C library beside the start
       files. crt_dir is set for it alone, and lld links no C
       library outside the glibc mode. */
    if (glibc) {
        glibc_libraries(c, t, in, false);
    } else if (in->linker == LINKER_PLATFORM) {
        struct text *search = next(c);
        text_appendf(search, "-L%s", in->crt_dir);
        add(c, text_cstr(search));
        linux_libraries(c, in);
        add(c, "-lc");
    }
}

/* The link of a DLL on Windows. library is the runtime archive of a
   library for C. */
static void windows_shared(struct link_command *c, enum target t,
                           const struct link_inputs *in,
                           const struct shared_options *s,
                           const struct text *library)
{
    const char *linker = program(c, in, t);
    struct text *def = next(c);

    text_appendf(def, "/DEF:%s", s->def_file != NULL ? s->def_file : "");
    add(c, linker);
    add(c, "/NOLOGO");
    windows_debug(c, in);
    add(c, "/DLL");
    /* DESIGN: a plugin links no C runtime startup, so it has no entry
       point. The host's runtime has started before the load, and
       the loader registers what the plugin carries. */
    if (s->plugin) {
        add(c, "/NOENTRY");
    }
    add(c, target_info(t)->arch == ARCH_ARM64 ? "/MACHINE:ARM64"
                                              : "/MACHINE:X64");
    windows_output(c, in);
    add(c, text_cstr(def));
    windows_libpaths(c, t, in);
    windows_memcheck(c, t, in);
    /* The inputs of a plugin hold the import library of its host,
       which names every symbol of the runtime and the program. */
    add_inputs(c, in);
    if (!s->plugin) {
        add(c, text_cstr(library));
        add(c, WINDOWS_MSVCRT);
    }
    add(c, "libvcruntime.lib");
    add(c, "ucrt.lib");
    add(c, "legacy_stdio_definitions.lib");
}

void link_shared_command(struct link_command *c, enum target t,
                         const struct link_inputs *in,
                         const struct shared_options *s)
{
    /* DESIGN: a Linux library of the glibc mode links against the glibc
       sysroot, the glibc runtime and the libraries of `link linux`, as a
       program of the mode does. */
    bool glibc = target_info(t)->os == OS_LINUX &&
                 in->linker == LINKER_LLD && in->glibc;
    struct text *library;

    start(c);
    library = next(c);
    if (!s->plugin) {
        runtime_library(library, in->runtime, t, in->cpu, glibc, LTO_NONE);
    }
    switch (target_info(t)->os) {
    case OS_MACOS: macos_shared(c, t, in, s, library); break;
    case OS_LINUX: linux_shared(c, t, in, s, library, glibc); break;
    case OS_WINDOWS: windows_shared(c, t, in, s, library); break;
    }
}

void link_archive_command(struct link_command *c, enum target t,
                          const char *llvm_ar, const char *archive,
                          const char *const *members, size_t count)
{
    static const char *const formats[] = {
        [FORMAT_ELF] = "--format=gnu",
        [FORMAT_MACHO] = "--format=darwin",
        [FORMAT_COFF] = "--format=coff",
    };
    size_t i;

    start(c);
    add(c, llvm_ar);
    add(c, formats[target_info(t)->format]);
    add(c, "rcs");
    add(c, archive);
    for (i = 0; i < count; i++) {
        add(c, members[i]);
    }
}

/* DESIGN: ld64 turns hidden symbols into local ones in a relocatable
   object unless -keep_private_externs is given. The runtime symbols stay
   global, so two bundled runtimes in one program are a duplicate. ld64.lld
   writes no relocatable object, so Mach-O always joins with ld64. */
void link_relocatable_command(struct link_command *c, enum target t,
                              const struct link_inputs *in, const char *output,
                              const char *const *objects, size_t count)
{
    size_t i;

    start(c);
    add(c, target_info(t)->os == OS_MACOS ? "ld"
                                          : program(c, in, t));
    add(c, "-r");
    if (target_info(t)->os == OS_MACOS) {
        add(c, "-keep_private_externs");
        add(c, "-arch");
        add(c, target_info(t)->arch == ARCH_ARM64 ? "arm64" : "x86_64");
    }
    add(c, "-o");
    add(c, output);
    for (i = 0; i < count; i++) {
        add(c, objects[i]);
    }
}

void link_line(struct text *out, enum target t, const char *library,
               const char *runtime, enum cpu_level cpu, bool bundled)
{
    bool windows = target_info(t)->os == OS_WINDOWS;

    text_appendf(out, "%s main.c %s", windows ? "cl" : "cc", library);
    if (!bundled) {
        text_append(out, " ");
        link_runtime_library(out, runtime, t, cpu);
    }
    if (target_info(t)->os == OS_LINUX) {
        text_append(out, " -lpthread -lm");
    }
}


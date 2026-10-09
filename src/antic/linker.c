#include "linker.h"
#include "llvm_run.h"

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

/* The words of --lto by mode. */
static const char *const lto_names[] = {"none", "full", "thin"};

const char *link_lto_name(enum lto mode)
{
    return lto_names[mode];
}

bool link_lto_from_name(const char *name, enum lto *mode)
{
    size_t i;

    for (i = 0; i < sizeof lto_names / sizeof lto_names[0]; i++) {
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
/* The library directory of a Windows sysroot, which holds the import
   libraries that tools/get-sysroot.cmake writes from the .def files of
   mingw-w64 and the builtins of the pinned clang, and the libraries every
   lld-link command names from it. tools/windows-compile.cmake spells the
   same names for the links of the build. */
#define SYSROOT_WINDOWS_LIB "lib"
#define WINDOWS_BUILTINS "clang_rt.builtins.lib"
#define WINDOWS_C_LIBRARY "ucrtbase.lib"
#define WINDOWS_NT_LIBRARY "ntdll.lib"
#define WINDOWS_KERNEL_LIBRARY "kernel32.lib"
/* The libraries of link.exe, the platform linker, which links with the C
   runtime of Visual Studio as the entry under "Linking" in
   docs/decisions.md gives the command, and the static one that the
   profile runtime names and the program leaves out. */
static const char *const platform_windows_libraries[] = {
    "msvcrt.lib", "libvcruntime.lib", "ucrt.lib",
    "legacy_stdio_definitions.lib",
};
#define WINDOWS_STATIC_CRT "libcmt.lib"

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
        text_appendf(out, "%s/%s", SYSROOT_WINDOWS_LIB, WINDOWS_C_LIBRARY);
        break;
    }
}

/* The program of the linker of target t: the flavour of lld in bin/ of
   the runtime archive, or the platform linker. */
static const char *program(struct link_command *c, const struct link_inputs *in,
                           enum target t)
{
    enum target_os os = target_info(t)->os;
    struct text *path;

    if (in->linker == LINKER_PLATFORM) {
        return platform_linkers[os];
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

/* DESIGN: every link of a program or of a shared library drops the
   sections nothing reaches: -dead_strip for ld64.lld and Apple's ld,
   --gc-sections for ld.lld and GNU ld, /OPT:REF for lld-link and
   link.exe. Eddie decided on 2026-10-06 that an executable carries no
   code it never uses. Relinked with -dead_strip on macos-arm64 that day,
   hello world went from 134,496 to 91,216 bytes, 32.1 percent less,
   tests/bench/builder.anti 17.8, objects.anti 18.3, map_work.anti 10.2
   and tests/bench/ablate/mixed_work.anti 4.3, each with its old output.
   The runtime archive and llc put each function and datum of an ELF or
   COFF object in a section of its own for this, and Mach-O has it
   already.

   The roots each linker keeps by itself: the entry, the constructors of
   llvm.global_ctors and the runtime, what llvm.used names, and every
   name a program that hosts plugins exports, or a shared library shows.
   llvm.used holds the package header, the notice anti_licenses and the
   options hook of --memory-checks, which are reached by name alone. The
   tables the runtime reads, such as anti_rt_slots and
   anti_rt_injectable, are reached by a direct reference from the code
   that reads them, and anti_rt_provides and anti_rt_imports are exports
   of a plugin.

   Both Windows linkers turn /OPT:REF off under the /DEBUG every link
   passes, so it is named, and named alone it turns on the folding of
   all identical code as well. That gives two functions or two constants
   one address, which a program may compare.

   DESIGN: every link of lld folds identical code in the safe form,
   --icf=safe for ld.lld and ld64.lld and /OPT:SAFEICF for lld-link.
   It folds only the code and data that the address-significance table
   of every object leaves out, so each address a program compares stays
   distinct. llc writes the table with -addrsig, see llvm_run.c, the
   object runtime of macOS takes -faddrsig, see CMakeLists.txt, and the
   LTO of lld writes it for the code it generates. An object without a
   table folds nothing. Eddie decided on 2026-10-08 that every target
   folds, since the safe form costs no speed and he wants no code in a
   program that it does not need. link.exe has no safe form and keeps
   /OPT:NOICF, and GNU ld and Apple's ld fold nothing. The measurement
   stands in the entry on folding under "Scope and toolchain" in
   docs/decisions.md. distinct_addresses_<target> checks the
   guarantee. */
static void drop_unused(struct link_command *c, enum target t,
                        const struct link_inputs *in)
{
    bool lld = in->linker == LINKER_LLD;

    switch (target_info(t)->os) {
    case OS_MACOS:
        add(c, "-dead_strip");
        if (lld) {
            add(c, "--icf=safe");
        }
        break;
    case OS_LINUX:
        add(c, "--gc-sections");
        if (lld) {
            add(c, "--icf=safe");
        }
        break;
    case OS_WINDOWS:
        add(c, "/OPT:REF");
        add(c, lld ? "/OPT:SAFEICF" : "/OPT:NOICF");
        break;
    }
}

/* DESIGN: the LTO of lld runs at the level and the inline threshold of
   opt in release mode, O3 and 225, whatever default lld has. run_lto of
   llvm_run.c gives the measurement. */
static void lto_level(struct link_command *c, enum target t,
                      const struct link_inputs *in)
{
    if (in->lto != LTO_NONE && target_info(t)->os == OS_WINDOWS) {
        add(c, "/opt:lldlto=3");
        add(c, "/mllvm:" LLVM_INLINE_THRESHOLD);
    } else if (in->lto != LTO_NONE) {
        add(c, "--lto-O3");
        add(c, "-mllvm");
        add(c, LLVM_INLINE_THRESHOLD);
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
    drop_unused(c, t, in);
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
    drop_unused(c, t, in);
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
    struct text *allocator = next(c);
    size_t i;

    runtime_library(library, in->runtime, t, in->cpu, false, in->lto);
    add(c, linker);
    add(c, "-static");
    add(c, "-pie");
    add(c, "--no-dynamic-linker");
    if (!in->debug) {
        add(c, "--strip-debug");
    }
    drop_unused(c, t, in);
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
    /* The allocator comes before libc.a, so malloc and the functions
       beside it resolve to it for the program, the runtime and musl. */
    text_appendf(allocator, "%s/%s/%s/%s/%s", in->runtime, RUNTIME_LIB_DIR,
                 target_name(t), cpu_name(in->cpu), MUSL_ALLOCATOR);
    add(c, text_cstr(allocator));
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

   The objects of the Microsoft C runtime, which the platform linker
   takes, name PDBs that no machine here holds, and a linker warns once
   per object when it cannot read one. /ignore:4099 drops that warning,
   which says nothing about the program being linked. lld-link links
   nothing of Microsoft, so it needs no such flag. */
static void windows_debug(struct link_command *c, const struct link_inputs *in)
{
    add(c, "/DEBUG");
    add(c, "/PDBALTPATH:%_PDB%");
    if (in->linker == LINKER_LLD) {
        add(c, "/pdbsourcepath:.");
    } else {
        add(c, "/ignore:4099");
    }
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

/* The import library of a DLL takes the name of the DLL with its suffix
   replaced, as lld-link names it by default outside its mingw mode, and
   stands beside it. */
static void link_import_library_path(struct text *out, const char *library)
{
    const char *slash = strrchr(library, '/');
    const char *dot = strrchr(slash != NULL ? slash : library, '.');

    text_appendf(out, "%.*s" LINK_COFF_ARCHIVE_SUFFIX,
                 (int)(dot != NULL ? (size_t)(dot - library)
                                   : strlen(library)),
                 library);
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

/* The start of an lld-link command: the program, no banner, and nothing
   of the machine.
   DESIGN: lld-link reads the library directories of LIB, as link.exe
   does, unless /lldignoreenv is given. On a Windows host it also finds
   the Visual Studio and the Windows SDK the machine installed, through
   the setup configuration and the registry, and adds their library
   directories even under /lldignoreenv. The Windows VM showed ucrt.lib
   come from Windows Kits with LIB unset. A library the sysroot lacks
   would then come from the machine without a word, and the program would
   depend on what the package never held. /lldmingw ends the detection:
   in that mode lld-link looks for no Visual Studio and takes the
   library directories of the command line alone. The mode is the one
   the objects of the gnu triple need as well, since their unwind data
   stands in .pdata$f and .xdata$f sections that only the mode ties to
   the function f, as GNU ld does, and /OPT:REF drops them otherwise.
   The runtimes of compiler-rt name msvcrt.lib, libcmt.lib, oldnames.lib
   and uuid.lib in their directives, the C runtime and the SDK of
   Microsoft, which no sysroot of ours holds, so /NODEFAULTLIB drops
   those four by name. A
   directive that names any other library stays, as the one of a C
   object that names user32.lib does, and lld-link finds it in lib/ of
   the sysroot or reports it missing. So every lld-link command takes the
   sysroot alone, and a missing library is missing. Eddie decided the
   rule on 2026-09-27 under "Binary distribution" in docs/decisions.md.
   The platform linker, link.exe, keeps its own rules. */
static const char *const dropped_default_libraries[] = {
    "msvcrt.lib", "libcmt.lib", "oldnames.lib", "uuid.lib",
};

static void lld_link_start(struct link_command *c, const char *linker,
                           const struct link_inputs *in)
{
    size_t i;

    add(c, linker);
    add(c, "/NOLOGO");
    if (in->linker != LINKER_LLD) {
        return;
    }
    add(c, "/lldmingw");
    add(c, "/lldignoreenv");
    for (i = 0; i < sizeof dropped_default_libraries /
                        sizeof dropped_default_libraries[0]; i++) {
        struct text *dropped = next(c);
        text_appendf(dropped, "/NODEFAULTLIB:%s", dropped_default_libraries[i]);
        add(c, text_cstr(dropped));
    }
}

/* The library directory of lld-link: lib/ of the sysroot. The platform
   linker takes its own. */
static void windows_libpath(struct link_command *c,
                            const struct link_inputs *in)
{
    struct text *dir;

    if (in->linker != LINKER_LLD) {
        return;
    }
    dir = next(c);
    text_appendf(dir, "/LIBPATH:%s/%s", in->sysroot, SYSROOT_WINDOWS_LIB);
    add(c, text_cstr(dir));
}

/* DESIGN: the libraries of every Windows link, after its inputs. lld-link
   takes the builtins of the pinned clang, which hold the helpers the code
   calls for what the processor has no instruction for, and the import
   libraries of ucrtbase.dll, the C library of every Windows since 10, of
   ntdll.dll and of kernel32.dll, as decision 4 of
   docs/work-order-distribution.md names them, and nothing of Microsoft.
   The entry point, the stack probe and the printf family, which the
   static libraries of Microsoft gave, stand in the runtime, in
   src/rt/platform_windows.c. A plugin takes them from the import library
   of its host among its inputs. The platform linker takes the C runtime
   of Visual Studio, with the static one left out of a program of
   --profile-generate, whose runtime names it. */
static void windows_libraries(struct link_command *c,
                              const struct link_inputs *in)
{
    size_t i;

    if (in->linker == LINKER_LLD) {
        add(c, WINDOWS_BUILTINS);
        add(c, WINDOWS_C_LIBRARY);
        add(c, WINDOWS_NT_LIBRARY);
        add(c, WINDOWS_KERNEL_LIBRARY);
        return;
    }
    if (in->profile_generate) {
        add(c, "/NODEFAULTLIB:" WINDOWS_STATIC_CRT);
    }
    for (i = 0; i < sizeof platform_windows_libraries /
                        sizeof platform_windows_libraries[0]; i++) {
        add(c, platform_windows_libraries[i]);
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
    drop_unused(c, t, in);
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

/* DESIGN: lld-link with ucrtbase.dll, the C library every Windows holds
   since Windows 10, and the runtime of Anti, which brings the entry point
   and the rest of what the static libraries of Microsoft gave. The
   program then links nothing of Microsoft and needs no redistributable.
   The platform linker links the C runtime of Visual Studio instead, the
   user's own C world. */
static void windows(struct link_command *c, enum target t,
                    const struct link_inputs *in)
{
    const char *linker = program(c, in, t);
    struct text *library = next(c);

    runtime_library(library, in->runtime, t, in->cpu, false, in->lto);
    lld_link_start(c, linker, in);
    windows_debug(c, in);
    drop_unused(c, t, in);
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
    windows_libpath(c, in);
    windows_memcheck(c, t, in);
    add_inputs(c, in);
    add(c, text_cstr(library));
    /* DESIGN: compiler-rt builds the profile runtime of Windows against
       the static C runtime, and its objects name libcmt.lib, which the
       start of the command drops. What the runtime takes of that
       library, the check of the stack cookie and atexit among it, the
       runtime of Anti defines. The image holds one C library. */
    profile_runtime(c, t, in, false);
    windows_libraries(c, in);
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
    drop_unused(c, t, in);
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
    drop_unused(c, t, in);
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
    lld_link_start(c, linker, in);
    windows_debug(c, in);
    drop_unused(c, t, in);
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
    /* DESIGN: lld-link writes the import library of a DLL in its mingw
       mode only when asked, so a library for C asks for it by the name
       lld-link gives it otherwise, beside the DLL. A plugin has none,
       since nothing links against a plugin. */
    if (!s->plugin && in->linker == LINKER_LLD) {
        struct text *implib = next(c);
        text_append(implib, "/IMPLIB:");
        link_import_library_path(implib, in->executable);
        add(c, text_cstr(implib));
    }
    windows_libpath(c, in);
    windows_memcheck(c, t, in);
    /* The inputs of a plugin hold the import library of its host,
       which names every symbol of the runtime and the program. */
    add_inputs(c, in);
    if (!s->plugin) {
        add(c, text_cstr(library));
    }
    if (s->plugin && in->linker != LINKER_LLD) {
        /* link.exe: a plugin links no msvcrt.lib, the start of a program,
           and the rest of the C runtime of Visual Studio. */
        size_t i;
        for (i = 1; i < sizeof platform_windows_libraries /
                            sizeof platform_windows_libraries[0]; i++) {
            add(c, platform_windows_libraries[i]);
        }
        return;
    }
    windows_libraries(c, in);
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


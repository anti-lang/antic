#include "driver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "antl.h"
#include "applesdk.h"
#include "coff.h"
#include "diagnostic.h"
#include "emit.h"
#include "ir.h"
#include "lexer.h"
#include "linker.h"
#include "lower.h"
#include "header.h"
#include "modpath.h"
#include "notice.h"
#include "optimize.h"
#include "whole.h"
#include "regalloc.h"
#include "select.h"
#include "sha256.h"
#include "parser.h"
#include "sema.h"
#include "selfpath.h"
#include "types.h"
#include "process.h"
#include "text.h"
#include "warnings.h"

#define ASSEMBLY_SUFFIX ".s"
#define PACKAGE_SUFFIX ".package"
#define DEF_SUFFIX ".def"
#define EXPORTED_SUFFIX ".exported"

/* DESIGN: a bundled runtime holds every member of the runtime library
   except two. The object of src/rt/start.c has the C main of executables. The
   object of src/rt/license.c reads the notice, which a static library has
   none of. The bundle takes the stub object of the runtime archive
   instead, and anti.license.text then returns an empty text. */
#define RUNTIME_START_MEMBER "start."
#define RUNTIME_LICENSE_MEMBER "license."

/* What a compilation writes beside the assembly. A library for C has a
   name, a header and a package header copy. A linked binary has the
   licence notice and the names of its export fns. */
struct extras {
    struct text name;
    struct text header;
    struct text package;
    struct text notice;
    struct text exports;
    /* The program can host a plugin, so the link exports its symbols.
       The compilation decides it and the link reads it. */
    bool hosts_plugins;
    /* The `provides` lines of a plugin, one per line as
       `<interface>\t<class>`, which the index file carries. */
    struct text provides;
    /* The names a Windows program that can host a plugin defines, as
       emit_names writes them, which its .def file exports. */
    struct text host_names;
    /* The program holds `anti.regex`, so the link adds PCRE2. */
    bool regex;
};

static void extras_free(struct extras *e)
{
    text_free(&e->name);
    text_free(&e->header);
    text_free(&e->package);
    text_free(&e->notice);
    text_free(&e->exports);
    text_free(&e->provides);
    text_free(&e->host_names);
}


static bool ends_with(const char *s, const char *suffix)
{
    size_t n = strlen(s);
    size_t m = strlen(suffix);
    return n >= m && strcmp(s + n - m, suffix) == 0;
}

/* Read the whole of a binary file. */
static bool read_bytes(const char *path, struct text *out)
{
    FILE *f = fopen(path, "rb");
    char buffer[4096];
    size_t n;
    bool ok;

    if (f == NULL) {
        fprintf(stderr, "antic: cannot open %s\n", path);
        return false;
    }
    while ((n = fread(buffer, 1, sizeof buffer, f)) > 0) {
        text_append_bytes(out, buffer, n);
    }
    ok = !ferror(f);
    if (!ok) {
        fprintf(stderr, "antic: cannot read %s\n", path);
    }
    fclose(f);
    return ok;
}

/* Read the whole file. A source file holding a NUL byte is rejected,
   because the text buffer ends at the first NUL. One above
   LEX_SOURCE_MAX is rejected as well, since the lexer refuses it. */
static bool read_source(const char *path, struct text *out)
{
    FILE *f = fopen(path, "rb");
    char buffer[4096];
    size_t n;
    size_t total = 0;
    bool ok = true;

    if (f == NULL) {
        fprintf(stderr, "antic: cannot open %s\n", path);
        return false;
    }
    while ((n = fread(buffer, 1, sizeof buffer - 1, f)) > 0) {
        if (n > LEX_SOURCE_MAX - total) {
            fprintf(stderr, "antic: %s is larger than 64 MiB\n", path);
            ok = false;
            break;
        }
        total += n;
        buffer[n] = '\0';
        if (strlen(buffer) != n) {
            fprintf(stderr, "antic: %s contains a NUL byte\n", path);
            ok = false;
            break;
        }
        text_append(out, buffer);
    }
    if (ferror(f)) {
        fprintf(stderr, "antic: cannot read %s\n", path);
        ok = false;
    }
    fclose(f);
    return ok;
}

static bool write_file(const char *path, const struct text *content)
{
    FILE *f = fopen(path, "wb");
    bool ok;

    if (f == NULL) {
        fprintf(stderr, "antic: cannot create %s\n", path);
        return false;
    }
    ok = fwrite(text_cstr(content), 1, content->length, f) == content->length;
    ok = fclose(f) == 0 && ok;
    if (!ok) {
        fprintf(stderr, "antic: cannot write %s\n", path);
    }
    return ok;
}

/* The module path of the input, from its path under the search roots.
   DESIGN: antic -c refuses a library under the anti root without
   --anti-internal. It warns on a path of one segment, which is for a
   program's own files. No compilation may define the runtime's module. */
/* DESIGN: the two blocks are compiled only by `anti test`. Their functions
   are ordinary module functions by now, each carrying its own block, so
   dropping them here is one pass over the item list. Every pass
   after this one reads a module that never held them. No object, no
   `.antl` and no header can carry one. */
static void drop_test_blocks(struct module *tree, bool keep)
{
    size_t kept = 0;
    size_t i;
    size_t j;

    /* DESIGN: under `--tests` the two blocks stay and their functions are
       public, so the runner module that `anti test` writes calls them by
       name. Nothing else compiles them, so the name reaches no other
       build. */
    if (keep) {
        for (i = 0; i < tree->item_count; i++) {
            struct item *it = tree->items[i];
            if (it->block != BLOCK_NONE) {
                it->pub = true;
                it->vis = VIS_PUB;
            }
        }
        return;
    }
    for (i = 0; i < tree->item_count; i++) {
        if (tree->items[i]->block == BLOCK_NONE) {
            tree->items[kept++] = tree->items[i];
        }
    }
    tree->item_count = kept;
    /* A clause of a dropped block goes with it, so it does not read as
       one that silences nothing. */
    for (i = j = 0; i < tree->clause_count; i++) {
        if (tree->clauses[i].block == BLOCK_NONE) {
            tree->clauses[j++] = tree->clauses[i];
        }
    }
    tree->clause_count = j;
}

static bool module_name(const struct options *o, struct text *out,
                        struct diagnostics *diags)
{
    char error[200];

    if (!module_path_of_source(o->input, o->roots, o->root_count, out, error,
                               sizeof error)) {
        fprintf(stderr, "antic: %s\n", error);
        return false;
    }
    if (strcmp(text_cstr(out), RUNTIME_MODULE) == 0) {
        fprintf(stderr, "antic: %s: the module path `%s` is the runtime's\n",
                o->input, RUNTIME_MODULE);
        return false;
    }
    if (o->library && !o->front_end && !o->internal &&
        module_path_reserved(text_cstr(out))) {
        fprintf(stderr, "antic: %s: the module path `%s` is reserved for the "
                        "language's own libraries\n",
                o->input, text_cstr(out));
        return false;
    }
    /* The warning stands at the top of the file, where a clause of the
       whole file covers it. */
    if (o->library && !o->front_end && diags != NULL &&
        module_path_segments(text_cstr(out)) == 1) {
        diagnostics_warn(diags, NAME_SINGLE_SEGMENT_PATH, 1, 1,
                         "the module path `%s` has one segment, which is "
                         "for a program's own files", text_cstr(out));
    }
    return true;
}

/* Run xcrun and keep its output without the final newline. */
static bool xcrun(const char *query, struct text *out)
{
    const char *argv[] = {"xcrun", "--sdk", "macosx", query, NULL};

    if (process_capture(argv, out) != 0) {
        fprintf(stderr, "antic: xcrun %s failed\n", query);
        return false;
    }
    while (out->length > 0 && out->data[out->length - 1] == '\n') {
        out->data[--out->length] = '\0';
    }
    return true;
}

/* The directory of the glibc start files for a Linux target. */
static bool find_crt_dir(enum target t, struct text *out)
{
    const char *const *dirs = link_crt_dirs(t);
    size_t i;

    for (i = 0; dirs[i] != NULL; i++) {
        struct text path = {0};
        FILE *f;
        text_appendf(&path, "%s/Scrt1.o", dirs[i]);
        f = fopen(text_cstr(&path), "rb");
        text_free(&path);
        if (f != NULL) {
            fclose(f);
            text_append(out, dirs[i]);
            return true;
        }
    }
    fprintf(stderr, "antic: cannot find Scrt1.o of the C library in %s, %s "
                    "or %s\n", dirs[0], dirs[1], dirs[2]);
    return false;
}

/* Whether the build writes a plugin: a shared library that links no
   runtime and is bound against the host that loads it. */
static bool is_plugin(const struct options *o)
{
    return o->lib == LIB_SHARED && o->no_runtime;
}

/* Whether the command ends with a link, rather than a dump, a library
   file or an assembly file. */
static bool links(const struct options *o)
{
    return !o->assembly_only && !o->dump_tokens && !o->dump_ast &&
           !o->dump_types && !o->dump_ir && !o->dump_opt && !o->dump_select &&
           !o->dump_alloc && !o->library && !o->front_end &&
           o->lib == LIB_NONE;
}

/* DESIGN: lld links for every target from any host, with the sysroot of
   the runtime archive. The platform linker links only for the operating
   system it runs on, where the C library of the target is installed. */
static bool can_link(const struct options *o)
{
    enum target host;

    if (o->linker == LINKER_LLD) {
        return true;
    }
    if (!target_host(&host) ||
        target_info(host)->os != target_info(o->target)->os) {
        fprintf(stderr, "antic: linking for %s with the platform linker needs "
                        "a host with the same operating system. -S writes "
                        "the assembly without linking.\n",
                target_name(o->target));
        return false;
    }
    return true;
}

/* The texts behind the facts of a link. */
struct link_facts {
    struct text sdk_path;
    struct text sdk_version;
    struct text crt_dir;
    struct text sysroot;
    struct text lld_dir;
};

static void link_facts_free(struct link_facts *f)
{
    text_free(&f->sdk_path);
    text_free(&f->sdk_version);
    text_free(&f->crt_dir);
    text_free(&f->sysroot);
    text_free(&f->lld_dir);
}

static bool file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");

    if (f != NULL) {
        fclose(f);
    }
    return f != NULL;
}

/* DESIGN: a program that names a framework links against Apple's SDK,
   which Anti never fetches. sdk/ of the sysroot holds its stubs, from
   tools/get-sysroot.cmake with APPLE_SDK or from anti sdk import. A Mac
   without them takes the SDK of its Command Line Tools. */
/* Set f->sdk_path and f->sdk_version to Apple's SDK, which a program that
   names a framework links against, or explain where it comes from. */
static bool apple_sdk(const struct options *o, struct link_facts *f)
{
    struct text marker = {0};
    struct text version = {0};
    bool found;
    size_t i;

    text_appendf(&f->sdk_path, "%s/%s", text_cstr(&f->sysroot),
                 SYSROOT_APPLE_SDK_DIR);
    text_appendf(&marker, "%s/%s", text_cstr(&f->sdk_path), SYSROOT_SDK_VERSION);
    found = file_exists(text_cstr(&marker)) &&
            read_bytes(text_cstr(&marker), &version);
    text_free(&marker);
    if (found) {
        f->sdk_version.length = 0;
        text_appendf(&f->sdk_version, "%s", text_cstr(&version));
        text_free(&version);
        return true;
    }
    text_free(&version);
    f->sdk_path.length = 0;
    if (apple_clt_sdk(&f->sdk_path, &version)) {
        f->sdk_version.length = 0;
        text_appendf(&f->sdk_version, "%s", text_cstr(&version));
        text_free(&version);
        return true;
    }
    text_free(&version);
    text_appendf(&f->sdk_path, "%s/%s", text_cstr(&f->sysroot),
                 SYSROOT_APPLE_SDK_DIR);
    fprintf(stderr, "antic: the frameworks");
    for (i = 0; i < o->framework_count; i++) {
        fprintf(stderr, "%s %s", i > 0 ? "," : "", o->frameworks[i]);
    }
    fprintf(stderr, " link for %s against Apple's SDK, which %s lacks. A Mac "
                    "keeps the SDK in " APPLE_CLT_SDKS ". Pass a copy of it "
                    "as APPLE_SDK to tools/get-sysroot.cmake, or run anti sdk "
                    "export on that Mac and anti sdk import here.\n",
            target_name(o->target), text_cstr(&f->sdk_path));
    return false;
}

/* The facts of a link for the target. lld takes the sysroot and the lld
   programs of the runtime archive, and the SDK version of a macOS
   sysroot. The platform linker takes the macOS SDK from xcrun and the
   glibc start files of the host. */
static bool link_facts(const struct options *o, struct link_inputs *in,
                       struct link_facts *f)
{
    enum target t = o->target;
    enum target_os os = target_info(t)->os;
    static const char *const flavours[] = {
        [OS_LINUX] = "ld.lld", [OS_MACOS] = "ld64.lld",
        [OS_WINDOWS] = "lld-link",
    };
    struct text marker = {0};
    enum target host;
    bool present;

    memset(f, 0, sizeof *f);
    in->linker = o->linker;
    in->cpu = o->cpu;
    in->debug = o->debug;
    if (o->linker == LINKER_PLATFORM) {
        if (os == OS_MACOS) {
            if (!xcrun("--show-sdk-path", &f->sdk_path) ||
                !xcrun("--show-sdk-version", &f->sdk_version)) {
                return false;
            }
            in->sdk_path = text_cstr(&f->sdk_path);
            in->sdk_version = text_cstr(&f->sdk_version);
        } else if (os == OS_LINUX) {
            if (!find_crt_dir(t, &f->crt_dir)) {
                return false;
            }
            in->crt_dir = text_cstr(&f->crt_dir);
        }
        return true;
    }
    text_appendf(&f->lld_dir, "%s/%s", o->runtime, RUNTIME_BIN_DIR);
    /* The flavour of lld is a program of the host, which ends in .exe on
       Windows. The command line may leave the suffix out. */
    text_appendf(&marker, "%s/%s%s", text_cstr(&f->lld_dir), flavours[os],
                 target_host(&host) ? target_info(host)->executable_suffix : "");
    in->lld_dir = file_exists(text_cstr(&marker)) ? text_cstr(&f->lld_dir)
                                                  : NULL;
    text_free(&marker);
    text_appendf(&f->sysroot, "%s/%s/", o->runtime, RUNTIME_SYSROOT_DIR);
    link_target_dir(&f->sysroot, t, in->glibc);
    /* The builtins are the last file tools/get-sysroot.cmake writes into
       a glibc sysroot. */
    text_appendf(&marker, "%s/%s", text_cstr(&f->sysroot),
                 os == OS_LINUX && in->glibc ? "usr/lib/libclang_rt.builtins.a"
                 : os == OS_LINUX            ? "usr/lib/libc.a"
                 : os == OS_MACOS ? SYSROOT_SDK_VERSION
                 : target_info(t)->arch == ARCH_ARM64
                     ? "crt/lib/aarch64/msvcrt.lib"
                     : "crt/lib/x86_64/msvcrt.lib");
    present = file_exists(text_cstr(&marker)) &&
              (os != OS_MACOS || read_bytes(text_cstr(&marker), &f->sdk_version));
    text_free(&marker);
    /* DESIGN: without a Windows sysroot lld-link reads the library
       directories of LIB, which an MSVC environment sets. */
    if (!present && os != OS_WINDOWS) {
        fprintf(stderr, "antic: linking for %s with lld needs the sysroot "
                        "%s, which tools/get-sysroot.cmake installs\n",
                target_name(t), text_cstr(&f->sysroot));
        return false;
    }
    in->sysroot = present ? text_cstr(&f->sysroot) : NULL;
    if (os == OS_MACOS && o->framework_count > 0) {
        if (!apple_sdk(o, f)) {
            return false;
        }
        in->sdk_path = text_cstr(&f->sdk_path);
    }
    while (f->sdk_version.length > 0 &&
           (f->sdk_version.data[f->sdk_version.length - 1] == '\n' ||
            f->sdk_version.data[f->sdk_version.length - 1] == '\r')) {
        f->sdk_version.data[--f->sdk_version.length] = '\0';
    }
    in->sdk_version = text_cstr(&f->sdk_version);
    return true;
}

/* DESIGN: a Windows link runs in the directory of its output. The object
   files, the PDB and the output are named relative to that directory, and
   so is a runtime archive given by a relative path. lld-link and link.exe
   record the path of every object and the whole command line in the PDB.
   The PDB goes into the symbols archive of a release, and a path of the
   build in it would ship. An absolute runtime archive, such as
   the one of an install, stays as given, and so do its linker and its
   sysroot. */
struct windows_link {
    struct arena arena;
    struct text directory;      /* where the link runs, empty for here */
    struct text base;           /* the absolute form of that directory */
};

static const char *windows_keep(struct windows_link *w, const struct text *t)
{
    char *copy = arena_alloc(&w->arena, t->length + 1);

    memcpy(copy, text_cstr(t), t->length + 1);
    return copy;
}

/* path as the link names it from its directory. A relative path, and any
   path when relative is set, is relative to that directory unless it
   stands on another root. Any other path stays as given. */
static const char *windows_path(struct windows_link *w, const char *path,
                                bool relative)
{
    struct text absolute = {0};
    struct text from = {0};
    const char *result = path;

    if (path == NULL || (!relative && path_is_absolute(path))) {
        return path;
    }
    if (absolute_path(path, &absolute)) {
        result = link_relative(&from, text_cstr(&absolute), text_cstr(&w->base))
                     ? windows_keep(w, &from)
                     : windows_keep(w, &absolute);
    }
    text_free(&absolute);
    text_free(&from);
    return result;
}

/* Name the paths of in, and the .def file of a DLL, from the directory of
   in->executable, where the link runs. */
static bool windows_link_paths(struct windows_link *w, struct link_inputs *in,
                               const char **def_file)
{
    const char *exe = in->executable;
    const char *cut = NULL;
    const char **extra;
    const char *p;
    size_t i;

    for (p = exe; *p != '\0'; p++) {
#if defined(_WIN32)
        if (*p == '\\') {
            cut = p;
        }
#endif
        if (*p == '/') {
            cut = p;
        }
    }
    if (cut != NULL) {
        text_appendf(&w->directory, "%.*s", cut == exe ? 1 : (int)(cut - exe),
                     exe);
    }
    if (!absolute_path(cut != NULL ? text_cstr(&w->directory) : ".", &w->base)) {
        fprintf(stderr, "antic: cannot find the directory of %s\n", exe);
        return false;
    }
    if (cut != NULL) {
        w->directory.length = 0;
        text_append(&w->directory, text_cstr(&w->base));
#if defined(_WIN32)
        for (i = 0; i < w->directory.length; i++) {
            if (w->directory.data[i] == '/') {
                w->directory.data[i] = '\\';
            }
        }
#endif
    }
    in->object = windows_path(w, in->object, true);
    in->executable = windows_path(w, in->executable, true);
    if (def_file != NULL) {
        *def_file = windows_path(w, *def_file, true);
    }
    extra = arena_alloc(&w->arena, (in->extra_count + 1) * sizeof *extra);
    for (i = 0; i < in->extra_count; i++) {
        extra[i] = windows_path(w, in->extra[i], true);
    }
    in->extra = extra;
    in->runtime = windows_path(w, in->runtime, false);
    in->sysroot = windows_path(w, in->sysroot, false);
    in->lld_dir = windows_path(w, in->lld_dir, false);
    return true;
}

static void windows_link_free(struct windows_link *w)
{
    arena_free(&w->arena);
    text_free(&w->directory);
    text_free(&w->base);
}

/* Run the command of a link, a Windows link in the directory it names. */
static bool run_link(const struct windows_link *w, const struct link_command *c)
{
    const char *directory = w->directory.length > 0 ? text_cstr(&w->directory)
                                                    : NULL;

    if (process_run_in(directory, c->argv) != 0) {
        fprintf(stderr, "antic: the linker failed\n");
        return false;
    }
    return true;
}

/* DESIGN: a Windows program that can host a plugin exports the names of
   its own object and of the runtime it links. A .def file beside it
   lists them. The link writes the import library <program>.lib, which
   its plugins link against. A plugin then names the executable in its
   imports, and Windows binds it to the program that loads it. */
static bool host_exports(const struct options *o, const char *object,
                         const char *executable, const struct text *names,
                         struct text *def, struct text *implib)
{
    const char *suffix = target_info(o->target)->executable_suffix;
    const char *slash = strrchr(executable, '/');
    const char *file = slash != NULL ? slash + 1 : executable;
    size_t n = strlen(executable);
    struct text library = {0};
    struct text bytes = {0};
    struct text program = {0};
    struct text all = {0};
    struct text content = {0};
    const char *p;
    bool ok;

    if (n >= strlen(suffix) && strcmp(executable + n - strlen(suffix),
                                      suffix) == 0) {
        n -= strlen(suffix);
    }
    text_appendf(def, "%.*s%s", (int)n, executable, DEF_SUFFIX);
    text_appendf(implib, "%.*s%s", (int)n, executable,
                 LINK_COFF_ARCHIVE_SUFFIX);
    link_runtime_library(&library, o->runtime, o->target, o->cpu);
    text_append(&all, text_cstr(names));
    ok = read_bytes(text_cstr(&library), &bytes) &&
         read_bytes(object, &program);
    if (ok && !coff_archive_exports((const unsigned char *)bytes.data,
                                    bytes.length,
                                    (const unsigned char *)program.data,
                                    program.length, &all)) {
        fprintf(stderr, "antic: cannot read the symbols of %s and %s\n",
                text_cstr(&library), object);
        ok = false;
    }
    text_appendf(&content, "NAME %s\nEXPORTS\n", file);
    for (p = text_cstr(&all); ok && *p != '\0';) {
        size_t line = strcspn(p, "\n");
        text_appendf(&content, "    %.*s\n", (int)line, p);
        p += line + (p[line] == '\n');
    }
    ok = ok && write_file(text_cstr(def), &content);
    text_free(&library);
    text_free(&bytes);
    text_free(&program);
    text_free(&all);
    text_free(&content);
    return ok;
}

/* DESIGN: a program that holds `anti.regex` links the glue of the patterns
   and PCRE2 from lib/<target>/ of the runtime archive after its own
   objects. Both are built for the default level of the target alone. A
   program below that level is refused with the level of each, in the
   words of "CPU levels" in docs/anti-language-additions.md. out holds the
   objects of the command line and the two libraries. */
static bool native_inputs(const struct options *o, const struct extras *extras,
                          struct text *glue, struct text *pcre2,
                          const char ***out, size_t *count)
{
    enum cpu_level level = cpu_default(o->target);
    const char **list;

    *out = o->objects;
    *count = o->object_count;
    if (!extras->regex) {
        return true;
    }
    if (cpu_arch(o->cpu) == cpu_arch(level) && o->cpu < level) {
        fprintf(stderr, "antic: " REGEX_MODULE " is built for %s%s, this "
                "program targets %s\n",
                target_info(o->target)->arch == ARCH_X86_64 ? "x86-64-" : "",
                cpu_name(level), cpu_name(o->cpu));
        return false;
    }
    link_native_library(glue, o->runtime, o->target, NATIVE_REGEX_GLUE);
    link_native_library(pcre2, o->runtime, o->target, NATIVE_PCRE2);
    list = malloc((o->object_count + 2) * sizeof *list);
    if (list == NULL) {
        fputs("antic: out of memory\n", stderr);
        return false;
    }
    if (o->object_count > 0) {
        memcpy(list, o->objects, o->object_count * sizeof *list);
    }
    list[o->object_count] = text_cstr(glue);
    list[o->object_count + 1] = text_cstr(pcre2);
    *out = list;
    *count = o->object_count + 2;
    return true;
}

static bool link_program(const struct options *o, const char *object,
                         const char *executable, const struct extras *extras)
{
    struct text glue = {0};
    struct text pcre2 = {0};
    const char **extra;
    size_t extra_count;
    struct link_inputs in;
    struct link_command command;
    struct link_facts facts;
    struct windows_link w;
    struct text def = {0};
    struct text implib = {0};
    enum target_os os = target_info(o->target)->os;
    bool ok = true;

    if (!native_inputs(o, extras, &glue, &pcre2, &extra, &extra_count)) {
        text_free(&glue);
        text_free(&pcre2);
        return false;
    }
    memset(&in, 0, sizeof in);
    in.object = object;
    in.executable = executable;
    in.runtime = o->runtime;
    in.extra = extra;
    in.extra_count = extra_count;
    in.frameworks = o->frameworks;
    in.framework_count = o->framework_count;
    in.linux_libraries = o->linux_libraries;
    in.linux_library_count = o->linux_library_count;
    in.exports = extras->hosts_plugins;
    in.glibc = os == OS_LINUX &&
               (o->linux_library_count > 0 || extras->hosts_plugins);
    memset(&w, 0, sizeof w);
    memset(&facts, 0, sizeof facts);
    if (in.exports && os == OS_WINDOWS) {
        ok = host_exports(o, object, executable, &extras->host_names, &def,
                          &implib);
        in.def_file = text_cstr(&def);
        in.import_library = text_cstr(&implib);
    }
    ok = ok && link_facts(o, &in, &facts) &&
         (os != OS_WINDOWS || windows_link_paths(&w, &in, &in.def_file));
    if (ok && in.import_library != NULL) {
        in.import_library = windows_path(&w, in.import_library, true);
    }
    if (ok) {
        link_command(&command, o->target, &in);
        ok = run_link(&w, &command);
        link_command_free(&command);
    }
    windows_link_free(&w);
    link_facts_free(&facts);
    text_free(&def);
    text_free(&implib);
    if (extra != o->objects) {
        free((void *)extra);
    }
    text_free(&glue);
    text_free(&pcre2);
    return ok;
}

static void print_diagnostics(const char *input,
                              const struct diagnostics *diags)
{
    size_t i;

    for (i = 0; i < diags->count; i++) {
        const struct diagnostic *d = &diags->items[i];
        fprintf(stderr, "%s:%d:%d: %s: %s", input, d->line, d->column,
                d->warning ? "warning" : "error", d->message);
        if (d->name != NAME_NONE) {
            fprintf(stderr, " [%s]", warnings_name(d->name));
        }
        fputc('\n', stderr);
    }
}

/* Print the diagnostics of the compilation and count them for the caller
   that asked, which is `anti check`. */
static void report_diagnostics(const struct options *o,
                               const struct diagnostics *diags)
{
    size_t i;

    print_diagnostics(o->input, diags);
    if (o->counts == NULL) {
        return;
    }
    for (i = 0; i < diags->count; i++) {
        if (diags->items[i].promoted) {
            o->counts->warnings++;
        } else if (!diags->items[i].warning) {
            o->counts->errors++;
        } else if (diags->items[i].doc) {
            o->counts->doc_warnings++;
        } else {
            o->counts->warnings++;
        }
    }
}

/* Whether the build writes a program in release mode. */
static bool release_build(const struct options *o)
{
    return !o->dev && !o->library && !o->front_end;
}

/* Whether the checker sees the whole program, which it needs to find an
   abstract class that no class fills. */
static bool whole_program_check(const struct options *o)
{
    return release_build(o) && o->lib == LIB_NONE;
}

/* The checks that ran in this compilation. A check that belongs to
   another build leaves its `allow` alone, since that build decides
   whether the clause silences something. */
static warnings_ran checks_ran(const struct options *o)
{
    warnings_ran ran = WARNINGS_ALL_RAN;

    if (!o->doc_warnings) {
        ran &= ~(WARNINGS_BIT(NAME_DOC_MARKUP) |
                 WARNINGS_BIT(NAME_DOC_UNRESOLVED) |
                 WARNINGS_BIT(NAME_DOC_NOTE_ONLY) |
                 WARNINGS_BIT(NAME_DOC_DROPPED));
    }
    if (!o->doc_warnings || !o->warn_undocumented) {
        ran &= ~WARNINGS_BIT(NAME_UNDOCUMENTED);
    }
    if (!whole_program_check(o)) {
        ran &= ~WARNINGS_BIT(NAME_UNFILLED_ABSTRACT);
    }
    if (!o->library || o->front_end) {
        ran &= ~WARNINGS_BIT(NAME_SINGLE_SEGMENT_PATH);
    }
    return ran;
}

/* Apply the clauses of the module, turn the warnings into errors where
   the build accepts none, and report. complete says the checker ran to
   its end, so a clause that silenced nothing is known. Returns false
   when an error stands. */
static bool settle_diagnostics(const struct options *o,
                               const struct module *tree,
                               struct diagnostics *diags, bool complete)
{
    size_t i;

    warnings_apply(tree, diags, checks_ran(o), complete);
    if (o->warnings_as_errors || release_build(o)) {
        warnings_promote(diags);
    }
    report_diagnostics(o, diags);
    for (i = 0; i < diags->count; i++) {
        if (!diags->items[i].warning) {
            return false;
        }
    }
    return true;
}

/* Print one token per line: its position, its group and its source text.
   A doc comment spans lines, so it prints its first line. Chapter 1 shows
   this output for the function scale. */
static void dump_tokens(const char *source, const struct token_list *tokens)
{
    size_t i;

    for (i = 0; i + 1 < tokens->count; i++) {
        const struct token *t = &tokens->items[i];
        const char *end = memchr(source + t->offset, '\n', t->length);
        size_t length = end != NULL ? (size_t)(end - (source + t->offset))
                                    : t->length;
        char position[32];

        snprintf(position, sizeof position, "%d:%d", t->line, t->column);
        printf("%-5s %-8s %.*s\n", position, token_category(t->kind),
               (int)length, source + t->offset);
    }
}

/* The options of lowering that the command line sets. */
static unsigned lower_options(const struct options *o)
{
    unsigned flags = (o->no_reflect ? LOWER_NO_REFLECT : 0u) |
                     (o->dev ? LOWER_DEV : 0u) |
                     (o->no_hooks ? LOWER_NO_HOOKS : 0u) |
                     (o->trace_writes ? LOWER_TRACE_WRITES : 0u);

    /* `trace` follows the mode, as `assert` does, and `--trace` and
       `--no-trace` decide instead of it. */
    if (o->trace == TRACE_ON || (o->trace == TRACE_MODE && o->dev)) {
        flags |= LOWER_TRACE;
    }
    return flags;
}

/* The path a failed assertion or a failed dev-mode check names: the
   input under the first search root that holds it. */
static const char *recorded_file(const struct options *o)
{
    return module_file_of_source(o->input, o->roots, o->root_count);
}

/* Lower the module into ir and verify the result. file is the path that
   an assertion and a check record, which is not the path a message
   names. */
static bool lower_checked(const char *input, const char *file,
                          struct module *tree, const char *module,
                          struct ir_module *ir, struct diagnostics *diags,
                          unsigned options, const char *const *patterns,
                          size_t pattern_count, const char *version)
{
    struct text errors = {0};
    bool ok = false;

    tree->file = file;
    if (!lower_module(tree, module, ir, diags, options, patterns,
                      pattern_count, version)) {
        print_diagnostics(input, diags);
    } else if (!ir_verify(ir, &errors)) {
        fprintf(stderr, "antic: internal error, the IR of %s fails "
                        "verification\n%s", input, text_cstr(&errors));
    } else {
        ok = true;
    }
    text_free(&errors);
    return ok;
}

/* Run the passes over the whole program and print what they report,
   one line each. Returns false after an error. */
static bool whole_checked(const char *input, struct ir_module *program,
                          const char *module, bool release, bool reflect,
                          bool bundled, bool library, bool dev,
                          const struct options *o)
{
    struct whole_options options;
    struct text errors = {0};
    const char *line;
    bool ok;

    memset(&options, 0, sizeof options);
    options.entry = module;
    options.release = release;
    options.reflect = reflect;
    options.bundled = bundled;
    options.library = library;
    options.dev = dev;
    options.plugin = is_plugin(o);
    options.closed = o->closed;
    options.inject = o->inject;
    options.inject_count = o->inject_count;
    ok = whole_program(program, &options, &errors);
    for (line = text_cstr(&errors); *line != '\0';) {
        const char *end = strchr(line, '\n');
        size_t length = end != NULL ? (size_t)(end - line) : strlen(line);
        fprintf(stderr, "%s: error: %.*s\n", input, (int)length, line);
        line += length + (end != NULL ? 1 : 0);
    }
    text_free(&errors);
    return ok;
}

static bool has_main(const struct ir_module *program, const char *module);

/* Lower the module after the loaded libraries and print the whole
   program, after the optimizer passes when the flag asks for them. That
   program has been through the passes over the whole program where the
   build runs them. Returns 2, the status of a finished dump, on success. */
static int dump_ir(const char *input, const char *file, struct module *tree,
                   const char *module, struct ir_module *program,
                   bool optimize, bool release, struct diagnostics *diags,
                   unsigned options, const char *const *patterns,
                   size_t pattern_count, const struct options *o)
{
    bool no_reflect = (options & LOWER_NO_REFLECT) != 0;
    struct text out = {0};
    struct text errors = {0};

    if (!lower_checked(input, file, tree, module, program, diags, options,
                       patterns, pattern_count, o->package_version)) {
        return 1;
    }
    if (optimize) {
        if ((release || has_main(program, module)) &&
            !whole_checked(input, program, module, release, !no_reflect,
                           false, false, !release, o)) {
            return 1;
        }
        ir_optimize(program, module);
        if (!ir_verify(program, &errors)) {
            fprintf(stderr, "antic: internal error, the optimized IR of %s "
                            "fails verification\n%s", input,
                    text_cstr(&errors));
            text_free(&errors);
            return 1;
        }
    }
    ir_print(&out, program);
    fputs(text_cstr(&out), stdout);
    text_free(&out);
    return 2;
}

/* Whether a function of module name stands in the program. */
static bool holds_module(const struct ir_module *program, const char *name)
{
    size_t i;

    for (i = 0; i < program->function_count; i++) {
        const char *module = program->functions[i]->module;
        if (module != NULL && strcmp(module, name) == 0) {
            return true;
        }
    }
    return false;
}

/* Whether the program has a function main in the main module. */
static bool has_main(const struct ir_module *program, const char *module)
{
    size_t i;

    for (i = 0; i < program->function_count; i++) {
        const struct ir_function *f = program->functions[i];
        if (!f->is_extern && f->module != NULL &&
            strcmp(f->module, module) == 0 && strcmp(f->name, "main") == 0) {
            return true;
        }
    }
    return false;
}

/* Feed the bytes of the file at path to the digest. A file that cannot
   be opened adds nothing, and the link that needs it reports it. A file
   that opens and then fails to read returns false, since a digest of part
   of it would name other code. */
static bool digest_file(struct anti_sha256 *s, const char *path)
{
    FILE *f = fopen(path, "rb");
    unsigned char bytes[4096];
    size_t n;
    bool ok;

    if (f == NULL) {
        return true;
    }
    while ((n = fread(bytes, 1, sizeof bytes, f)) > 0) {
        anti_rt_sha256_update(s, bytes, n);
    }
    ok = !ferror(f);
    fclose(f);
    return ok;
}

/* DESIGN: the build id is the SHA-256 of the code that a link takes from
   antic. That is the assembly of the module that links, every object the
   command line adds and the runtime library. The notice is left out,
   because it holds the id. The paths are left out as well, so two links
   of one program in two places carry one id. What `-g` added is left out
   too, so a `-g` link and a plain link of one program carry one id.
   `anti symbols` then matches a trace of the one to the archive of the
   other. */
static bool build_id(const struct options *o, const struct text *assembly,
                     const struct debug_spans *spans, char hex[65],
                     char *error, size_t size)
{
    struct text library = {0};
    struct anti_sha256 s;
    size_t at = 0;
    size_t i;
    bool ok = true;

    anti_rt_sha256_init(&s);
    for (i = 0; spans != NULL && i < spans->count; i++) {
        if (spans->items[i].start > at) {
            anti_rt_sha256_update(&s, assembly->data + at,
                                  spans->items[i].start - at);
        }
        at = spans->items[i].end > at ? spans->items[i].end : at;
    }
    if (at < assembly->length) {
        anti_rt_sha256_update(&s, assembly->data + at, assembly->length - at);
    }
    for (i = 0; ok && i < o->object_count; i++) {
        if (!digest_file(&s, o->objects[i])) {
            snprintf(error, size, "cannot read %s", o->objects[i]);
            ok = false;
        }
    }
    if (ok && o->runtime != NULL) {
        link_runtime_library(&library, o->runtime, o->target, o->cpu);
        if (!digest_file(&s, text_cstr(&library))) {
            snprintf(error, size, "cannot read %s", text_cstr(&library));
            ok = false;
        }
    }
    anti_rt_sha256_hex(&s, hex);
    text_free(&library);
    return ok;
}

/* The notice with the line `build <id>` after its begin marker, where a
   tool that reads the marker finds it. */
static void identified_notice(struct text *out, const struct text *notice,
                              const char *id)
{
    size_t begin = sizeof NOTICE_BEGIN - 1;

    if (notice->length < begin ||
        memcmp(notice->data, NOTICE_BEGIN, begin) != 0) {
        text_append_bytes(out, notice->data, notice->length);
        return;
    }
    text_append(out, NOTICE_BEGIN);
    text_appendf(out, "build %s\n", id);
    text_append_bytes(out, notice->data + begin, notice->length - begin);
}

/* Lower the program, run the optimizer passes and run the back end for
   the target: instruction selection, register allocation and emission.
   The dumps print the machine code instead, before allocation for
   --dump-select. Returns 0 with the assembly, 2 after a dump and 1 after
   an error. */
static int back_end(const struct options *o, struct module *tree,
                    const char *module, struct ir_module *program,
                    struct diagnostics *diags, struct text *assembly,
                    struct extras *extras)
{
    struct mach_function **functions;
    struct text out = {0};
    struct debug_spans spans = {0};
    char error[200];
    bool dump = o->dump_select || o->dump_alloc;
    bool ok;
    int status = 1;
    size_t i;

    if (tree != NULL &&
        !lower_checked(o->input, recorded_file(o), tree, module, program,
                       diags, lower_options(o), o->trace_patterns,
                       o->trace_pattern_count, o->package_version)) {
        return 1;
    }
    /* DESIGN: the passes over the whole program run where the program is
       whole. That is every build in release mode. In dev mode it is the
       module that links, which has main, and a library for C. A dev
       object of any other module never links. */
    if ((!o->dev || o->lib != LIB_NONE || has_main(program, module)) &&
        !whole_checked(o->input, program, module, !o->dev,
                       !o->no_reflect, o->bundle_runtime,
                       o->lib != LIB_NONE, o->dev, o)) {
        return 1;
    }
    /* A program that injects an interface or loads a library exports
       its names. Both this and the `provides` lines of a plugin are
       read here, because the optimizer drops the class records that
       carry them. */
    extras->hosts_plugins = !o->closed && o->lib == LIB_NONE && !o->library &&
                            whole_hosts_plugins(program);
    extras->regex = holds_module(program, REGEX_MODULE);
    for (i = 0; is_plugin(o) && i < program->class_count; i++) {
        const struct ir_class *c = program->classes[i];
        size_t j;
        for (j = 0; j < c->provides_count; j++) {
            text_appendf(&extras->provides, "%s\t%s.%s\n",
                         c->provides[j].interface, c->module, c->name);
        }
    }
    /* The build that compiles the program decides, so an assertion of a
       library file follows this build and not the one that wrote it. */
    if (o->asserts == ASSERTS_OFF ||
        (o->asserts == ASSERTS_MODE && !o->dev)) {
        ir_drop_failures(program, IR_FAIL_ASSERT);
    }
    if (o->checks == CHECKS_OFF || (o->checks == CHECKS_MODE && !o->dev)) {
        ir_drop_failures(program, IR_FAIL_CHECK);
    }
    /* A plugin holds the code of its own module alone. Every other
       module of the program it was checked against belongs to the host,
       which defines it. */
    if (o->dev || is_plugin(o)) {
        ir_optimize_module(program, module);
    } else {
        ir_optimize(program, module);
    }
    if (!dump && !o->assembly_only && !o->dev && o->lib == LIB_NONE &&
        !has_main(program, module)) {
        fprintf(stderr, "antic: %s: the program has no function `main`\n",
                o->input);
        return 1;
    }
    program->plugin = is_plugin(o);
    functions = calloc(program->function_count + 1, sizeof *functions);
    if (functions == NULL) {
        fputs("antic: out of memory\n", stderr);
        exit(70);
    }
    ok = select_module(o->target, o->cpu, program, functions, error,
                       sizeof error);
    for (i = 0; ok && !(o->dump_select && !o->dump_alloc) &&
                i < program->function_count;
         i++) {
        if (functions[i] != NULL) {
            ok = regalloc_function(o->target, functions[i], error,
                                   sizeof error);
        }
    }
    if (ok && dump) {
        for (i = 0; i < program->function_count; i++) {
            if (functions[i] != NULL) {
                mach_print(&out, target_desc(o->target), o->cpu, program,
                           functions[i]);
            }
        }
        fputs(text_cstr(&out), stdout);
        status = 2;
    } else if (ok) {
        ok = o->dev || is_plugin(o)
                 ? emit_module(assembly, o->target, o->cpu, program, functions,
                               module, extras->hosts_plugins, o->debug, &spans,
                               error, sizeof error)
                 : emit_program(assembly, o->target, o->cpu, program,
                                functions, module, extras->hosts_plugins,
                                o->debug, &spans, error, sizeof error);
        if (ok && o->dev && !has_main(program, module)) {
            status = 3;
        }
        /* The host has run the runtime's start already, so a plugin
           brings no constructor of its own. */
        if (ok && o->lib == LIB_SHARED && !is_plugin(o)) {
            emit_constructor(assembly, o->target, "anti_rt_init");
        }
        if (ok && extras->notice.length > 0 && status != 3 &&
            !o->assembly_only && o->lib != LIB_STATIC) {
            struct text notice = {0};
            char id[65];
            ok = build_id(o, assembly, &spans, id, error, sizeof error);
            if (ok) {
                identified_notice(&notice, &extras->notice, id);
                emit_licenses(assembly, o->target, notice.data,
                              notice.length);
                text_free(&notice);
            }
        }
        if (ok && extras->hosts_plugins &&
            target_info(o->target)->format == FORMAT_COFF) {
            emit_names(&extras->host_names, o->target, program, functions);
        }
        for (i = 0; ok && i < program->function_count; i++) {
            const struct ir_function *f = program->functions[i];
            if (f->exported && !f->is_extern) {
                text_appendf(&extras->exports, "%s\n", f->name);
            }
        }
        status = !ok ? 1 : status == 3 ? 3 : 0;
    }
    if (!ok) {
        fprintf(stderr, "antic: %s\n", error);
    }
    for (i = 0; i < program->function_count; i++) {
        if (functions[i] != NULL) {
            mach_function_free(functions[i]);
            free(functions[i]);
        }
    }
    free(functions);
    text_free(&out);
    debug_spans_free(&spans);
    return status;
}

/* The package header from the options. A field without an option stays
   NULL, and sema_interface fills it in. --license-text names a file whose
   content goes into the header. */
static bool package_header(const struct options *o, struct arena *arena,
                           struct package *out)
{
    struct package_dependency *deps =
        arena_alloc(arena, (o->dependency_count + 1) * sizeof *deps);
    size_t i;

    memset(out, 0, sizeof *out);
    out->name = o->package_name;
    out->version = o->package_version;
    out->license = o->license;
    for (i = 0; i < o->dependency_count; i++) {
        const char *spec = o->dependencies[i];
        const char *first = strchr(spec, ',');
        const char *second = first != NULL ? strchr(first + 1, ',') : NULL;
        char *copy;
        if (second == NULL) {
            fprintf(stderr, "antic: --dependency %s is not "
                            "<name>,<constraint>,<url>\n", spec);
            return false;
        }
        copy = arena_alloc(arena, strlen(spec) + 1);
        memcpy(copy, spec, strlen(spec));
        copy[first - spec] = '\0';
        copy[second - spec] = '\0';
        deps[i].name = copy;
        deps[i].constraint = copy + (first - spec) + 1;
        deps[i].url = copy + (second - spec) + 1;
    }
    out->dependencies = deps;
    out->dependency_count = o->dependency_count;
    out->attribution = o->attribution;
    out->attribution_count = o->attribution_count;
    if (o->license_text != NULL) {
        struct text text = {0};
        char *copy;
        if (!read_source(o->license_text, &text)) {
            return false;
        }
        copy = arena_alloc(arena, text.length + 1);
        memcpy(copy, text_cstr(&text), text.length);
        out->license_text = copy;
        text_free(&text);
    }
    return true;
}

static bool own_interface(const struct options *o, struct module *tree,
                          const char *module, struct arena *arena,
                          struct interface *out);

/* Write the library file of the module, by default beside the source.
   Returns 2, the status of a finished command without linking. */
static int write_library(const struct options *o, struct module *tree,
                         const char *module, struct arena *arena,
                         struct diagnostics *diags)
{
    struct ir_module ir;
    struct interface iface;
    struct text bytes = {0};
    struct text path = {0};
    int status = 1;

    ir_module_init(&ir, arena, module);
    if (lower_checked(o->input, recorded_file(o), tree, module, &ir, diags,
                      lower_options(o), o->trace_patterns,
                      o->trace_pattern_count, o->package_version) &&
        own_interface(o, tree, module, arena, &iface)) {
        if (!antl_write(&bytes, &iface, &ir, o->strip_docs)) {
            fprintf(stderr, "antic: %s is too large for a library file\n",
                    o->input);
            goto done;
        }
        if (o->output != NULL) {
            text_append(&path, o->output);
        } else {
            text_appendf(&path, "%.*s%s",
                         (int)(strlen(o->input) - strlen(SOURCE_SUFFIX)),
                         o->input, ANTL_SUFFIX);
        }
        status = write_file(text_cstr(&path), &bytes) ? 2 : 1;
    }
done:
    text_free(&bytes);
    text_free(&path);
    ir_module_free(&ir);
    return status;
}

struct paths {
    const char **items;
    size_t count;
    size_t capacity;
};

static void add_path(struct paths *p, const char *path)
{
    if (p->count == p->capacity) {
        p->capacity = p->capacity == 0 ? 16 : p->capacity * 2;
        p->items = realloc((void *)p->items, p->capacity * sizeof *p->items);
        if (p->items == NULL) {
            fputs("antic: out of memory\n", stderr);
            exit(70);
        }
    }
    p->items[p->count++] = path;
}

/* The library file of module path module under the first search root that
   has one, in the memory pool, or NULL. */
static const char *search_roots(const struct options *o, const char *module,
                                struct arena *arena)
{
    struct text std_root = {0};
    size_t i;

    /* DESIGN: the standard library in std/ of the runtime archive is the
       last search root, so a program imports anti.io without -I. */
    if (o->runtime != NULL) {
        text_appendf(&std_root, "%s/%s", o->runtime, RUNTIME_STD_DIR);
    }
    for (i = 0; i < o->root_count + (o->runtime != NULL); i++) {
        struct text path = {0};
        const char *p;
        FILE *f;
        char *copy;
        text_appendf(&path, "%s/", i < o->root_count ? o->roots[i]
                                                     : text_cstr(&std_root));
        for (p = module; *p != '\0'; p++) {
            text_appendf(&path, "%c", *p == '.' ? '/' : *p);
        }
        text_append(&path, ANTL_SUFFIX);
        f = fopen(text_cstr(&path), "rb");
        if (f != NULL) {
            fclose(f);
            copy = arena_alloc(arena, path.length + 1);
            memcpy(copy, text_cstr(&path), path.length + 1);
            text_free(&path);
            text_free(&std_root);
            return copy;
        }
        text_free(&path);
    }
    text_free(&std_root);
    return NULL;
}

static bool contains(const struct paths *p, const char *s)
{
    size_t i;

    for (i = 0; i < p->count; i++) {
        if (strcmp(p->items[i], s) == 0) {
            return true;
        }
    }
    return false;
}

/* The library files of the compilation. They are the files on the command
   line and, for every import that none of them holds, the file under the
   search roots. The imports of a found file are looked up too. */
static bool find_libraries(const struct options *o, const struct module *tree,
                           struct arena *arena, struct paths *out)
{
    struct paths modules = {0};
    struct paths wanted = {0};
    char error[200];
    size_t read = 0;
    size_t i;
    bool ok = true;
    bool grew = true;

    for (i = 0; i < o->library_count; i++) {
        add_path(out, o->libraries[i]);
    }
    for (i = 0; i < tree->import_count; i++) {
        char *name = arena_alloc(arena, tree->imports[i].module.length + 1);
        memcpy(name, tree->imports[i].module.text,
               tree->imports[i].module.length);
        add_path(&wanted, name);
    }
    while (ok && grew) {
        for (; ok && read < out->count; read++) {
            struct text bytes = {0};
            struct interface header;
            ok = read_bytes(out->items[read], &bytes);
            if (ok && !antl_header((const uint8_t *)bytes.data, bytes.length,
                                   arena, &header, error, sizeof error)) {
                fprintf(stderr, "antic: %s %s\n", out->items[read], error);
                ok = false;
            }
            text_free(&bytes);
            if (ok) {
                add_path(&modules, header.module);
                for (i = 0; i < header.import_count; i++) {
                    add_path(&wanted, header.imports[i]);
                }
            }
        }
        grew = false;
        for (i = 0; ok && i < wanted.count; i++) {
            const char *found;
            if (contains(&modules, wanted.items[i])) {
                continue;
            }
            found = search_roots(o, wanted.items[i], arena);
            if (found != NULL && !contains(out, found)) {
                add_path(out, found);
                grew = true;
            }
        }
    }
    free((void *)modules.items);
    free((void *)wanted.items);
    return ok;
}

bool driver_libraries(const struct options *options, struct arena *arena,
                      const char ***paths, size_t *count)
{
    struct module empty = {0};
    struct paths found = {0};
    const char **list;
    size_t n;
    size_t i;

    if (!find_libraries(options, &empty, arena, &found)) {
        free((void *)found.items);
        return false;
    }
    n = found.count;
    list = arena_alloc(arena, (n + 1) * sizeof *list);
    for (i = 0; i < n; i++) {
        list[i] = found.items[i];
    }
    free((void *)found.items);
    *paths = list;
    *count = n;
    return true;
}

/* The names of the `link framework` lines, or with linux of the `link
   linux` lines, of the library files, each once. */
static bool link_names(const char *const *paths, size_t count,
                       struct arena *arena, const char ***names,
                       size_t *name_count, bool linux)
{
    const char **list = NULL;
    size_t n = 0;
    size_t room = 0;
    size_t i;
    size_t j;
    size_t k;
    char error[200];

    for (i = 0; i < count; i++) {
        struct text bytes = {0};
        struct interface header;
        if (!read_bytes(paths[i], &bytes) ||
            !antl_header((const uint8_t *)bytes.data, bytes.length, arena,
                         &header, error, sizeof error)) {
            if (bytes.length > 0) {
                fprintf(stderr, "antic: %s %s\n", paths[i], error);
            }
            text_free(&bytes);
            free((void *)list);
            return false;
        }
        text_free(&bytes);
        for (j = 0; j < (linux ? header.linux_library_count
                               : header.framework_count);
             j++) {
            const char *name = linux ? header.linux_libraries[j]
                                     : header.frameworks[j];
            for (k = 0; k < n; k++) {
                if (strcmp(list[k], name) == 0) {
                    break;
                }
            }
            if (k < n) {
                continue;
            }
            if (n == room) {
                const char **grown;
                room = room == 0 ? 8 : room * 2;
                grown = realloc((void *)list, room * sizeof *list);
                if (grown == NULL) {
                    fputs("antic: out of memory\n", stderr);
                    exit(70);
                }
                list = grown;
            }
            list[n++] = name;
        }
    }
    *names = arena_alloc(arena, (n + 1) * sizeof **names);
    if (n > 0) {
        memcpy((void *)*names, (void *)list, n * sizeof *list);
    }
    *name_count = n;
    free((void *)list);
    return true;
}

bool driver_frameworks(const char *const *paths, size_t count,
                       struct arena *arena, const char ***names,
                       size_t *name_count)
{
    return link_names(paths, count, arena, names, name_count, false);
}

bool driver_linux_libraries(const char *const *paths, size_t count,
                            struct arena *arena, const char ***names,
                            size_t *name_count)
{
    return link_names(paths, count, arena, names, name_count, true);
}

/* Read the library files and load each after the libraries it imports,
   whatever the order on the command line. The interfaces go to out in
   load order. */
static bool load_libraries(const struct paths *paths, const char *module,
                           struct arena *arena, struct types *types,
                           struct ir_module *program,
                           const struct interface **out)
{
    size_t n = paths->count;
    struct text *files = calloc(n + 1, sizeof *files);
    struct interface *headers = calloc(n + 1, sizeof *headers);
    bool *loaded = calloc(n + 1, sizeof *loaded);
    size_t count = 0;
    size_t i;
    size_t j;
    size_t k;
    char error[200];
    bool ok = files != NULL && headers != NULL && loaded != NULL;

    if (!ok) {
        fputs("antic: out of memory\n", stderr);
        exit(70);
    }
    for (i = 0; i < n && ok; i++) {
        ok = read_bytes(paths->items[i], &files[i]);
        if (ok && !antl_header((const uint8_t *)files[i].data,
                               files[i].length, arena, &headers[i], error,
                               sizeof error)) {
            fprintf(stderr, "antic: %s %s\n", paths->items[i], error);
            ok = false;
        }
    }
    for (i = 0; i < n && ok; i++) {
        if (strcmp(headers[i].module, module) == 0) {
            fprintf(stderr, "antic: %s holds module `%s`, which this command "
                            "compiles\n", paths->items[i], module);
            ok = false;
        }
        for (j = 0; j < i && ok; j++) {
            if (strcmp(headers[i].module, headers[j].module) == 0) {
                fprintf(stderr, "antic: %s and %s both hold module `%s`\n",
                        paths->items[j], paths->items[i], headers[i].module);
                ok = false;
            }
        }
    }
    while (ok && count < n) {
        size_t before = count;
        for (i = 0; i < n && ok; i++) {
            bool ready = !loaded[i];
            for (j = 0; j < headers[i].import_count && ready; j++) {
                bool found = false;
                for (k = 0; k < count && !found; k++) {
                    found = strcmp(out[k]->module, headers[i].imports[j]) == 0;
                }
                ready = found;
            }
            if (!ready) {
                continue;
            }
            out[count] = antl_read((const uint8_t *)files[i].data,
                                   files[i].length, out, count, types, arena,
                                   program, error, sizeof error);
            if (out[count] == NULL) {
                fprintf(stderr, "antic: %s %s\n", paths->items[i], error);
                ok = false;
            } else {
                loaded[i] = true;
                count++;
            }
        }
        if (ok && count == before) {
            /* Report the first import that no file provides, or else the
               cycle that blocks the remaining files. */
            for (i = 0; i < n && ok; i++) {
                for (j = 0; j < headers[i].import_count && !loaded[i]; j++) {
                    bool provided = false;
                    if (strcmp(headers[i].imports[j], module) == 0) {
                        fprintf(stderr, "antic: %s depends on `%s`, so the "
                                        "import forms a cycle\n",
                                paths->items[i], module);
                        ok = false;
                        break;
                    }
                    for (k = 0; k < n && !provided; k++) {
                        provided = strcmp(headers[k].module,
                                          headers[i].imports[j]) == 0;
                    }
                    if (!provided) {
                        fprintf(stderr, "antic: %s needs module `%s`\n",
                                paths->items[i], headers[i].imports[j]);
                        ok = false;
                        break;
                    }
                }
            }
            if (ok) {
                fputs("antic: the library files import each other in a "
                      "cycle\n", stderr);
                ok = false;
            }
        }
    }
    for (i = 0; i < n; i++) {
        text_free(&files[i]);
    }
    free(files);
    free(headers);
    free(loaded);
    return ok;
}

static bool defines_main(const struct module *tree)
{
    size_t i;

    for (i = 0; i < tree->item_count; i++) {
        const struct item *it = tree->items[i];
        if (it->kind == ITEM_FN && it->name.length == 4 &&
            memcmp(it->name.text, "main", 4) == 0) {
            return true;
        }
    }
    return false;
}

/* The name of a library for C: the file name of -o without lib and its
   suffix, or the last segment of the module path. */
static void library_name(const struct options *o, const char *module,
                         struct text *out)
{
    const char *base;
    const char *dot;
    size_t n;

    if (o->output == NULL) {
        text_append(out, module_path_last(module));
        return;
    }
    base = strrchr(o->output, '/');
    base = base != NULL ? base + 1 : o->output;
    if (target_info(o->target)->format != FORMAT_COFF &&
        strncmp(base, "lib", 3) == 0) {
        base += 3;
    }
    dot = strchr(base, '.');
    n = dot != NULL ? (size_t)(dot - base) : strlen(base);
    text_appendf(out, "%.*s", (int)n, base);
}

/* The interface of the compiled module with the package header of the
   options, as a library file of the module holds it. */
static bool own_interface(const struct options *o, struct module *tree,
                          const char *module, struct arena *arena,
                          struct interface *out)
{
    struct package package;

    if (!package_header(o, arena, &package)) {
        return false;
    }
    sema_interface(tree, module, arena, out);
    if (package.name != NULL) {
        out->package.name = package.name;
    }
    if (package.version != NULL) {
        out->package.version = package.version;
    }
    out->package.dependencies = package.dependencies;
    out->package.dependency_count = package.dependency_count;
    if (package.license != NULL) {
        out->package.license = package.license;
    }
    if (package.license_text != NULL) {
        out->package.license_text = package.license_text;
    }
    out->package.attribution = package.attribution;
    out->package.attribution_count = package.attribution_count;
    return true;
}

/* The licence notice of a linked binary: the runtime, every package of
   the loaded libraries once, and the package of the compiled module. */
static bool build_notice(const struct options *o, const struct interface *own,
                         const struct interface *const *libraries,
                         size_t count, struct arena *arena, struct text *out)
{
    const struct package **list = calloc(count + 3, sizeof *list);
    struct package runtime;
    struct text path = {0};
    struct text text = {0};
    size_t n = 0;
    size_t i;
    size_t j;

    if (list == NULL) {
        fputs("antic: out of memory\n", stderr);
        exit(70);
    }
    memset(&runtime, 0, sizeof runtime);
    runtime.name = RUNTIME_MODULE;
    runtime.version = ANTIC_VERSION;
    runtime.license = "0BSD";
    runtime.license_text = "";
    text_appendf(&path, "%s/licenses/anti_rt.txt",
                 o->runtime != NULL ? o->runtime : ".");
    if (o->runtime != NULL && read_source(text_cstr(&path), &text)) {
        char *copy = arena_alloc(arena, text.length + 1);
        memcpy(copy, text_cstr(&text), text.length);
        runtime.license_text = copy;
    }
    list[n++] = &runtime;
    /* DESIGN: the package of the compiled module is always the last
       `package` line, even where a library of the same package came
       first. `anti symbols` reads the version of a binary there. */
    for (i = 0; i < count; i++) {
        if (own->package.name != NULL &&
            strcmp(own->package.name, libraries[i]->package.name) == 0) {
            continue;
        }
        for (j = 0; j < n; j++) {
            if (strcmp(list[j]->name, libraries[i]->package.name) == 0) {
                break;
            }
        }
        if (j == n) {
            list[n++] = &libraries[i]->package;
        }
    }
    list[n++] = &own->package;
    notice_text(out, list, n);
    free((void *)list);
    text_free(&path);
    text_free(&text);
    return true;
}

static int compile(const struct options *o, struct text *source,
                   struct text *module, struct text *assembly,
                   struct text *base, struct extras *extras)
{
    struct arena arena = {0};
    struct diagnostics diags = {0};
    struct token_list tokens = {0};
    struct module *tree = NULL;
    struct types types;
    struct ir_module program;
    const struct interface **libraries = NULL;
    struct paths paths = {0};
    int status = 1;

    ir_module_init(&program, &arena, "");

    if (!read_source(o->input, source)) {
        return 1;
    }
    if (!lex(text_cstr(source), source->length, &arena, &diags, &tokens)) {
        report_diagnostics(o, &diags);
        goto done;
    }
    if (o->dump_tokens) {
        dump_tokens(text_cstr(source), &tokens);
        status = 2;
        goto done;
    }
    if (!parse(text_cstr(source), &tokens, &arena, &diags, &tree)) {
        report_diagnostics(o, &diags);
        goto done;
    }
    drop_test_blocks(tree, o->tests);
    if (o->dump_ast) {
        struct text dump = {0};
        ast_dump(&dump, tree);
        fputs(text_cstr(&dump), stdout);
        text_free(&dump);
        status = 2;
        goto done;
    }
    if (!module_name(o, module, &diags)) {
        goto done;
    }
    types_init(&types, &arena);
    if (!find_libraries(o, tree, &arena, &paths)) {
        goto done;
    }
    libraries = malloc((paths.count + 1) * sizeof *libraries);
    if (libraries == NULL) {
        fputs("antic: out of memory\n", stderr);
        exit(70);
    }
    if (!load_libraries(&paths, text_cstr(module), &arena, &types, &program,
                        libraries)) {
        goto done;
    }
    if (!sema_check(tree, text_cstr(module), o->package_name, libraries,
                    paths.count, &types, &arena, &diags,
                    whole_program_check(o))) {
        settle_diagnostics(o, tree, &diags, false);
        goto done;
    }
    if (o->doc_warnings) {
        sema_doc_warnings(tree, text_cstr(module), libraries, paths.count,
                          &types, o->warn_undocumented, &diags);
    }
    if (!settle_diagnostics(o, tree, &diags, true)) {
        goto done;
    }
    diags.count = 0;
    if (o->dump_types) {
        struct text dump = {0};
        ast_dump_typed(&dump, tree);
        fputs(text_cstr(&dump), stdout);
        text_free(&dump);
        status = 2;
        goto done;
    }
    if (o->library) {
        status = write_library(o, tree, text_cstr(module), &arena, &diags);
        goto done;
    }
    /* The front end ends here, before the first pass that writes a file of
       the program. Status 2 is the status of a command a dump finished. */
    if (o->front_end) {
        status = 2;
        goto done;
    }
    if (o->dump_ir || o->dump_opt) {
        status = dump_ir(o->input, recorded_file(o), tree, text_cstr(module),
                         &program, o->dump_opt, !o->dev, &diags,
                         lower_options(o), o->trace_patterns,
                         o->trace_pattern_count, o);
        goto done;
    }
    if (o->lib != LIB_NONE && defines_main(tree)) {
        fprintf(stderr, "antic: %s: a library for C has no function `main`\n",
                o->input);
        goto done;
    }
    if (o->lib != LIB_NONE || links(o)) {
        struct interface own;
        const struct interface **all =
            malloc((paths.count + 2) * sizeof *all);
        if (all == NULL) {
            fputs("antic: out of memory\n", stderr);
            exit(70);
        }
        if (!own_interface(o, tree, text_cstr(module), &arena, &own) ||
            !build_notice(o, &own, libraries, paths.count, &arena,
                          &extras->notice)) {
            free((void *)all);
            goto done;
        }
        if (o->lib != LIB_NONE) {
            memcpy((void *)all, (void *)libraries,
                   paths.count * sizeof *all);
            all[paths.count] = &own;
            library_name(o, text_cstr(module), &extras->name);
            header_write(&extras->header, text_cstr(&extras->name), all,
                         paths.count + 1, o->bundle_runtime);
            if (!antl_write_header(&extras->package, &own)) {
                fprintf(stderr, "antic: %s is too large for a library file\n",
                        o->input);
                free((void *)all);
                goto done;
            }
        }
        free((void *)all);
    }
    status = back_end(o, tree, text_cstr(module), &program, &diags, assembly,
                      extras);
    if (status != 0 && status != 3) {
        goto done;
    }
    if (o->lib != LIB_NONE && !o->assembly_only) {
        const char *slash = strrchr(o->output != NULL ? o->output : o->input,
                                    '/');
        const char *from = o->output != NULL ? o->output : o->input;
        if (slash != NULL) {
            text_appendf(base, "%.*s/", (int)(slash - from), from);
        }
        text_append(base, text_cstr(&extras->name));
    } else if (o->output != NULL) {
        text_append(base, o->output);
    } else {
        text_appendf(base, "%.*s",
                     (int)(strlen(o->input) - strlen(SOURCE_SUFFIX)),
                     o->input);
    }

done:
    ir_module_free(&program);
    free((void *)libraries);
    free((void *)paths.items);
    token_list_free(&tokens);
    diagnostics_free(&diags);
    arena_free(&arena);
    return status;
}

/* DESIGN: `anti doc` builds user docs from the public interface of one
   module and nothing else. The interface of a library file and the
   interface of its source are then the same structure. The command
   renders that one structure, so the doc-equivalence test of
   docs/tooling.md measures the library file and not the renderer. Dev
   docs need the private items and the `//#` notes, which live in the
   syntax tree alone. tree is filled for a source input and left NULL for
   a library file. */
const struct interface *driver_interface(const struct options *o,
                                         struct arena *arena,
                                         struct types *types,
                                         struct ir_module *program,
                                         struct module **tree)
{
    struct diagnostics diags = {0};
    struct token_list tokens = {0};
    struct module *parsed = NULL;
    struct interface *iface = NULL;
    const struct interface **libraries = NULL;
    struct paths paths = {0};
    struct text source = {0};
    struct text module = {0};
    char *kept;
    size_t length = strlen(o->input);
    bool library_file = length > strlen(ANTL_SUFFIX) &&
                        strcmp(o->input + length - strlen(ANTL_SUFFIX),
                               ANTL_SUFFIX) == 0;
    char error[200];
    size_t i;

    *tree = NULL;
    types_init(types, arena);
    if (library_file) {
        struct options with_input = *o;
        struct module empty;
        struct interface header;
        memset(&empty, 0, sizeof empty);
        with_input.libraries =
            malloc((o->library_count + 1) * sizeof *with_input.libraries);
        if (with_input.libraries == NULL) {
            fputs("antic: out of memory\n", stderr);
            exit(70);
        }
        with_input.libraries[0] = o->input;
        memcpy((void *)(with_input.libraries + 1), (void *)o->libraries,
               o->library_count * sizeof *o->libraries);
        with_input.library_count = o->library_count + 1;
        if (!read_bytes(o->input, &source) ||
            !antl_header((const uint8_t *)source.data, source.length, arena,
                         &header, error, sizeof error)) {
            if (source.length > 0) {
                fprintf(stderr, "antic: %s %s\n", o->input, error);
            }
            free((void *)with_input.libraries);
            goto done;
        }
        text_append(&module, header.module);
        if (!find_libraries(&with_input, &empty, arena, &paths)) {
            free((void *)with_input.libraries);
            goto done;
        }
        free((void *)with_input.libraries);
        libraries = malloc((paths.count + 1) * sizeof *libraries);
        if (libraries == NULL) {
            fputs("antic: out of memory\n", stderr);
            exit(70);
        }
        if (!load_libraries(&paths, "", arena, types, program, libraries)) {
            goto done;
        }
        for (i = 0; i < paths.count; i++) {
            if (strcmp(libraries[i]->module, text_cstr(&module)) == 0) {
                iface = (struct interface *)libraries[i];
                break;
            }
        }
        if (iface == NULL) {
            fprintf(stderr, "antic: %s holds no module `%s`\n", o->input,
                    text_cstr(&module));
        }
        goto done;
    }
    if (!read_source(o->input, &source)) {
        goto done;
    }
    /* The names of the interface point into the source, which outlives
       the call in the memory pool and not in this buffer. */
    kept = arena_alloc(arena, source.length + 1);
    memcpy(kept, text_cstr(&source), source.length + 1);
    if (!lex(kept, source.length, arena, &diags, &tokens) ||
        !parse(kept, &tokens, arena, &diags, &parsed)) {
        report_diagnostics(o, &diags);
        goto done;
    }
    drop_test_blocks(parsed, false);
    if (!module_name(o, &module, NULL)) {
        goto done;
    }
    if (!find_libraries(o, parsed, arena, &paths)) {
        goto done;
    }
    libraries = malloc((paths.count + 1) * sizeof *libraries);
    if (libraries == NULL) {
        fputs("antic: out of memory\n", stderr);
        exit(70);
    }
    if (!load_libraries(&paths, text_cstr(&module), arena, types, program,
                        libraries)) {
        goto done;
    }
    if (!sema_check(parsed, text_cstr(&module), o->package_name, libraries,
                    paths.count, types, arena, &diags, false)) {
        warnings_apply(parsed, &diags, 0, false);
        report_diagnostics(o, &diags);
        goto done;
    }
    warnings_apply(parsed, &diags, 0, false);
    report_diagnostics(o, &diags);
    iface = arena_alloc(arena, sizeof *iface);
    sema_interface(parsed, text_cstr(&module), arena, iface);
    *tree = parsed;

done:
    free((void *)libraries);
    free((void *)paths.items);
    token_list_free(&tokens);
    diagnostics_free(&diags);
    text_free(&source);
    text_free(&module);
    return iface;
}

bool driver_library_header(const struct options *o, struct text *out)
{
    struct arena arena = {0};
    struct types types;
    struct ir_module program;
    struct options with_input = *o;
    struct module empty;
    struct interface header;
    struct paths paths = {0};
    struct text source = {0};
    struct text name = {0};
    struct diagnostics diags = {0};
    const struct interface **libraries = NULL;
    const struct interface *own = NULL;
    char error[200];
    size_t kept = 0;
    size_t i;
    bool ok = false;

    memset(&empty, 0, sizeof empty);
    types_init(&types, &arena);
    ir_module_init(&program, &arena, "");
    with_input.libraries =
        malloc((o->library_count + 1) * sizeof *with_input.libraries);
    if (with_input.libraries == NULL) {
        fputs("antic: out of memory\n", stderr);
        exit(70);
    }
    with_input.libraries[0] = o->input;
    memcpy((void *)(with_input.libraries + 1), (void *)o->libraries,
           o->library_count * sizeof *o->libraries);
    with_input.library_count = o->library_count + 1;
    if (!read_bytes(o->input, &source) ||
        !antl_header((const uint8_t *)source.data, source.length, &arena,
                     &header, error, sizeof error)) {
        if (source.length > 0) {
            fprintf(stderr, "antic: %s %s\n", o->input, error);
        }
        goto done;
    }
    if (!find_libraries(&with_input, &empty, &arena, &paths)) {
        goto done;
    }
    libraries = malloc((paths.count + 1) * sizeof *libraries);
    if (libraries == NULL) {
        fputs("antic: out of memory\n", stderr);
        exit(70);
    }
    if (!load_libraries(&paths, "", &arena, &types, &program, libraries)) {
        goto done;
    }
    /* The checker declares the functions of anti.lang.Object, which the
       table of an export class writes. An empty module checks nothing
       else. */
    if (!sema_check(&empty, "", NULL, libraries, paths.count, &types,
                    &arena, &diags, false)) {
        report_diagnostics(o, &diags);
        goto done;
    }
    /* The module of the file goes last, where the compiled module stands
       when antic writes the header of --lib, so both write one order. */
    for (i = 0; i < paths.count; i++) {
        if (strcmp(libraries[i]->module, header.module) == 0) {
            own = libraries[i];
        } else {
            libraries[kept++] = libraries[i];
        }
    }
    if (own == NULL) {
        fprintf(stderr, "antic: %s holds no module `%s`\n", o->input,
                header.module);
        goto done;
    }
    libraries[kept] = own;
    library_name(o, header.module, &name);
    header_write(out, text_cstr(&name), libraries, kept + 1, false);
    ok = true;

done:
    free((void *)with_input.libraries);
    free((void *)libraries);
    free((void *)paths.items);
    text_free(&source);
    text_free(&name);
    diagnostics_free(&diags);
    ir_module_free(&program);
    arena_free(&arena);
    return ok;
}

/* DESIGN: dev mode also compiles a library file of the dependency graph
   into its own object. The IR in the file is the module, and the other
   library files declare what it calls. A main module comes from source,
   so this object never links. */
static int compile_library_file(const struct options *o,
                                struct text *assembly, struct text *base,
                                struct extras *extras)
{
    struct arena arena = {0};
    struct diagnostics diags = {0};
    struct types types;
    struct ir_module program;
    struct module empty;
    struct options with_input = *o;
    struct interface header;
    struct text bytes = {0};
    struct text verify_errors = {0};
    const struct interface **libraries = NULL;
    struct paths paths = {0};
    char error[200];
    int status = 1;

    memset(&empty, 0, sizeof empty);
    ir_module_init(&program, &arena, "");
    types_init(&types, &arena);
    with_input.libraries =
        malloc((o->library_count + 1) * sizeof *with_input.libraries);
    if (with_input.libraries == NULL) {
        fputs("antic: out of memory\n", stderr);
        exit(70);
    }
    with_input.libraries[0] = o->input;
    memcpy((void *)(with_input.libraries + 1), (void *)o->libraries,
           o->library_count * sizeof *o->libraries);
    with_input.library_count = o->library_count + 1;
    if (!read_bytes(o->input, &bytes)) {
        goto done;
    }
    if (!antl_header((const uint8_t *)bytes.data, bytes.length, &arena,
                     &header, error, sizeof error)) {
        fprintf(stderr, "antic: %s %s\n", o->input, error);
        goto done;
    }
    if (!find_libraries(&with_input, &empty, &arena, &paths)) {
        goto done;
    }
    libraries = malloc((paths.count + 1) * sizeof *libraries);
    if (libraries == NULL) {
        fputs("antic: out of memory\n", stderr);
        exit(70);
    }
    if (!load_libraries(&paths, "", &arena, &types, &program, libraries)) {
        goto done;
    }
    /* No module is lowered here, so lower_checked does not verify the
       program. The passes and the optimizer rely on the verifier, and a
       library file may come from anywhere. */
    if (!ir_verify(&program, &verify_errors)) {
        fprintf(stderr, "antic: the IR of the library files fails "
                        "verification\n%s", text_cstr(&verify_errors));
        goto done;
    }
    status = back_end(o, NULL, header.module, &program, &diags, assembly,
                      extras);
    if (status == 0 || status == 3) {
        status = 3;
        if (o->output != NULL) {
            text_append(base, o->output);
        } else {
            text_appendf(base, "%.*s",
                         (int)(strlen(o->input) - strlen(ANTL_SUFFIX)),
                         o->input);
        }
    }

done:
    ir_module_free(&program);
    free((void *)libraries);
    free((void *)paths.items);
    free((void *)with_input.libraries);
    text_free(&bytes);
    text_free(&verify_errors);
    diagnostics_free(&diags);
    arena_free(&arena);
    return status;
}

/* The tool of the runtime archive, or its bare name for the search path.
   DESIGN: an installed antic finds llvm-mc and llvm-ar beside itself in
   bin/, the rule that already holds for the lld programs. The text holds
   the path, so it lives until the caller frees it. */
static const char *archive_tool(const struct options *o, const char *name,
                                struct text *path)
{
    enum target host;

    if (o->runtime == NULL) {
        return name;
    }

    if (!target_host(&host)) {
        return name;
    }
    text_appendf(path, "%s/%s/%s%s", o->runtime, RUNTIME_BIN_DIR, name,
                 target_info(host)->executable_suffix);
    if (file_exists(text_cstr(path))) {
        return text_cstr(path);
    }
    return name;
}

/* Run llvm-mc on the assembly file for the target. */
static bool assemble(const struct options *o, const char *assembly,
                     const char *object)
{
    struct text triple = {0};
    struct text attributes = {0};
    struct text found = {0};
    const char *argv[] = {NULL, NULL, "-filetype=obj", "-o", object, assembly,
                          NULL, NULL};
    int run;

    argv[0] = o->llvm_mc != NULL ? o->llvm_mc
                                 : archive_tool(o, "llvm-mc", &found);
    text_appendf(&triple, "-triple=%s", target_info(o->target)->triple);
    argv[1] = text_cstr(&triple);
    /* The assembler of the level, so that it takes the instructions the
       level adds. The x86_64 assembler needs no attributes. */
    if (cpu_attributes(o->cpu)[0] != '\0') {
        text_appendf(&attributes, "-mattr=%s", cpu_attributes(o->cpu));
        argv[6] = argv[5];
        argv[5] = text_cstr(&attributes);
        argv[7] = NULL;
    }
    run = process_run(argv);
    text_free(&attributes);
    text_free(&triple);
    text_free(&found);
    if (run != 0) {
        fprintf(stderr, "antic: llvm-mc failed\n");
        return false;
    }
    return true;
}

static bool run_command(const struct link_command *c, const char *what)
{
    if (process_run(c->argv) != 0) {
        fprintf(stderr, "antic: %s failed\n", what);
        return false;
    }
    return true;
}

/* Join the COFF objects into one object at output, which src/antic/coff.c does
   because no linker of COFF writes a relocatable object. */
static bool join_coff(const struct paths *objects, const char *output)
{
    struct coff_input *inputs = calloc(objects->count, sizeof *inputs);
    struct text *bytes = calloc(objects->count, sizeof *bytes);
    struct text joined = {0};
    struct text error = {0};
    bool ok = inputs != NULL && bytes != NULL;
    size_t i;

    for (i = 0; ok && i < objects->count; i++) {
        ok = read_bytes(objects->items[i], &bytes[i]);
        inputs[i].name = objects->items[i];
        inputs[i].data = (const unsigned char *)bytes[i].data;
        inputs[i].size = bytes[i].length;
    }
    if (ok && !coff_join(inputs, objects->count, &joined, &error)) {
        fprintf(stderr, "antic: joining the runtime into the library: %s\n",
                text_cstr(&error));
        ok = false;
    }
    ok = ok && write_file(output, &joined);
    for (i = 0; bytes != NULL && i < objects->count; i++) {
        text_free(&bytes[i]);
    }
    free(inputs);
    free(bytes);
    text_free(&joined);
    text_free(&error);
    return ok;
}

/* The members of a static library besides the package header. They are
   the compiled object, or with a bundled runtime one relocatable object
   of it and the runtime's members. The paths are in the memory pool. */
static bool bundle(const struct options *o, const char *object,
                   const char *base, struct arena *arena,
                   struct paths *members)
{
    const char *llvm_ar = o->llvm_ar != NULL ? o->llvm_ar : "llvm-ar";
    bool coff = target_info(o->target)->format == FORMAT_COFF;
    struct text library = {0};
    struct text listing = {0};
    struct text dir = {0};
    struct text output = {0};
    struct text joined = {0};
    struct paths objects = {0};
    const char *p;
    bool ok;

    if (!o->bundle_runtime) {
        add_path(members, object);
        return true;
    }
    link_runtime_library(&library, o->runtime, o->target, o->cpu);
    text_appendf(&dir, "%s.rt", base);
    text_appendf(&output, "--output=%s", text_cstr(&dir));
    {
        const char *argv[] = {llvm_ar, "t", text_cstr(&library), NULL};
        ok = process_capture(argv, &listing) == 0;
    }
    add_path(&objects, object);
    {
        struct text stub = {0};
        char *path;
        text_appendf(&stub, "%s/%s/%s/%s/%s%s", o->runtime, RUNTIME_LIB_DIR,
                     target_name(o->target), cpu_name(o->cpu),
                     RUNTIME_LICENSE_STUB,
                     target_info(o->target)->object_suffix);
        path = arena_alloc(arena, stub.length + 1);
        memcpy(path, text_cstr(&stub), stub.length + 1);
        add_path(&objects, path);
        text_free(&stub);
    }
    for (p = text_cstr(&listing); ok && *p != '\0';) {
        size_t n = strcspn(p, "\r\n");
        if (n > 0 &&
            strncmp(p, RUNTIME_START_MEMBER, strlen(RUNTIME_START_MEMBER)) != 0 &&
            strncmp(p, RUNTIME_LICENSE_MEMBER,
                    strlen(RUNTIME_LICENSE_MEMBER)) != 0) {
            struct text member = {0};
            char *path = arena_alloc(arena, dir.length + n + 2);
            const char *argv[] = {llvm_ar, "x", NULL, NULL, NULL, NULL};
            text_appendf(&member, "%.*s", (int)n, p);
            snprintf(path, dir.length + n + 2, "%s/%.*s", text_cstr(&dir),
                     (int)n, p);
            argv[2] = text_cstr(&output);
            argv[3] = text_cstr(&library);
            argv[4] = text_cstr(&member);
            ok = process_run(argv) == 0;
            add_path(&objects, path);
            text_free(&member);
        }
        p += n;
        p += strspn(p, "\r\n");
    }
    if (ok) {
        char *copy;
        text_appendf(&joined, "%s.bundled%s", base,
                     target_info(o->target)->object_suffix);
        if (coff) {
            ok = join_coff(&objects, text_cstr(&joined));
        } else {
            struct link_command c;
            struct link_inputs in;
            struct link_facts facts;
            memset(&in, 0, sizeof in);
            ok = link_facts(o, &in, &facts);
            if (ok) {
                relocatable_command(&c, o->target, &in, text_cstr(&joined),
                                    objects.items, objects.count);
                ok = run_command(&c, "joining the runtime into the library");
                link_command_free(&c);
            }
            link_facts_free(&facts);
        }
        copy = arena_alloc(arena, joined.length + 1);
        memcpy(copy, text_cstr(&joined), joined.length + 1);
        add_path(members, copy);
    }
    if (!ok) {
        fprintf(stderr, "antic: cannot bundle %s\n", text_cstr(&library));
    }
    free((void *)objects.items);
    text_free(&library);
    text_free(&listing);
    text_free(&dir);
    text_free(&output);
    text_free(&joined);
    return ok;
}

/* Assemble the object of the package header copy at <path>.o, and make
   path that object. */
static bool assemble_package(const struct options *o, const struct text *bytes,
                             struct text *path)
{
    struct text source = {0};
    struct text asm_path = {0};
    struct text obj_path = {0};
    bool ok;

    emit_package(&source, o->target, bytes->data, bytes->length);
    text_appendf(&asm_path, "%s%s", text_cstr(path), ASSEMBLY_SUFFIX);
    text_appendf(&obj_path, "%s%s", text_cstr(path),
                 target_info(o->target)->object_suffix);
    ok = write_file(text_cstr(&asm_path), &source) &&
         assemble(o, text_cstr(&asm_path), text_cstr(&obj_path));
    text_free(path);
    text_append(path, text_cstr(&obj_path));
    text_free(&source);
    text_free(&asm_path);
    text_free(&obj_path);
    return ok;
}

/* DESIGN: `anti-plugins.toml` beside a plugin lists every library of
   the directory. Each entry holds the interfaces, the runtime version
   it was built against and the digest of its bytes. Discovery reads it
   and opens no library to find out what is inside one. antic writes the
   file, because `anti build` is not built, and it keeps the entries of
   the other libraries as they stand. */
static void index_entry(struct text *out, const char *name, const char *digest,
                        const struct text *provides)
{
    const char *line = text_cstr(provides);
    bool first = true;

    text_appendf(out, "[[library]]\npath = '%s'\nruntime = '%s'\n"
                 "digest = '%s'\ninterfaces = [", name, ANTIC_VERSION,
                 digest);
    while (*line != '\0') {
        size_t n = strcspn(line, "\t");
        text_appendf(out, "%s'%.*s'", first ? "" : ", ", (int)n, line);
        first = false;
        line += strcspn(line, "\n");
        line += *line == '\n';
    }
    text_append(out, "]\n");
}

/* Whether the entry of length bytes at block names the library `name`. */
static bool index_names(const char *block, size_t length, const char *name)
{
    struct text wanted = {0};
    const char *found;
    bool named;

    text_appendf(&wanted, "\npath = '%s'\n", name);
    found = strstr(block, text_cstr(&wanted));
    named = found != NULL && (size_t)(found - block) < length;
    text_free(&wanted);
    return named;
}

/* The entries of the index that name another library, as they stand. A
   file that is no index of this form is replaced. */
static void index_others(struct text *out, const char *path, const char *name)
{
    struct text file = {0};
    const char *block;
    FILE *f = fopen(path, "rb");

    if (f == NULL) {
        return;
    }
    fclose(f);
    if (!read_source(path, &file)) {
        text_free(&file);
        return;
    }
    block = strstr(text_cstr(&file), "[[library]]\n");
    while (block != NULL) {
        const char *next = strstr(block + 11, "[[library]]\n");
        size_t n = next != NULL ? (size_t)(next - block) : strlen(block);
        if (!index_names(block, n, name)) {
            text_append_bytes(out, block, n);
        }
        block = next;
    }
    text_free(&file);
}

static bool write_plugin_index(const char *dir, const char *name,
                               const char *library,
                               const struct text *provides)
{
    struct text path = {0};
    struct text content = {0};
    char digest[65];
    bool ok;

    text_appendf(&path, "%s%s", dir, PLUGIN_INDEX);
    ok = sha256_file(library, digest);
    if (ok) {
        index_others(&content, text_cstr(&path), name);
        index_entry(&content, name, digest, provides);
        ok = write_file(text_cstr(&path), &content);
    } else {
        fprintf(stderr, "antic: cannot read %s\n", library);
    }
    text_free(&path);
    text_free(&content);
    return ok;
}

/* Write a library for C from object: its header, and the static archive
   with the copy of the package header, or the shared library. A static
   library prints the line that links a C program with it. */
static bool build_c_library(const struct options *o, const char *object,
                            const char *base, const struct extras *extras)
{
    const struct target_info *info = target_info(o->target);
    const char *name = text_cstr(&extras->name);
    const char *slash = strrchr(base, '/');
    struct text dir = {0};
    struct text path = {0};
    struct text header = {0};
    struct text major = {0};
    bool ok;

    if (slash != NULL) {
        text_appendf(&dir, "%.*s/", (int)(slash - base), base);
    }
    if (o->output != NULL) {
        text_append(&path, o->output);
    } else if (o->lib == LIB_STATIC) {
        text_appendf(&path, info->format == FORMAT_COFF ? "%s%s.lib"
                                                        : "%slib%s.a",
                     text_cstr(&dir), name);
    } else {
        text_appendf(&path,
                     info->os == OS_WINDOWS ? "%s%s.dll"
                     : info->os == OS_MACOS ? "%slib%s.dylib"
                                            : "%slib%s.so",
                     text_cstr(&dir), name);
    }
    text_appendf(&header, "%s%s%s", text_cstr(&dir), name, HEADER_SUFFIX);
    /* A plugin is loaded by an Anti host and never by C, so it carries
       no header of its own. */
    ok = is_plugin(o) || write_file(text_cstr(&header), &extras->header);
    if (ok && o->lib == LIB_STATIC) {
        struct arena arena = {0};
        struct text package = {0};
        struct paths members = {0};
        struct link_command c;
        text_appendf(&package, "%s%s%s", text_cstr(&dir), name, PACKAGE_SUFFIX);
        ok = assemble_package(o, &extras->package, &package) &&
             bundle(o, object, base, &arena, &members);
        if (ok) {
            add_path(&members, text_cstr(&package));
            remove(text_cstr(&path));
            archive_command(&c, o->target,
                            o->llvm_ar != NULL ? o->llvm_ar : "llvm-ar",
                            text_cstr(&path), members.items, members.count);
            ok = run_command(&c, "llvm-ar");
            link_command_free(&c);
        }
        if (ok) {
            struct text line = {0};
            link_line(&line, o->target, text_cstr(&path), o->runtime, o->cpu,
                      o->bundle_runtime);
            printf("%s\n", text_cstr(&line));
            text_free(&line);
        }
        free((void *)members.items);
        arena_free(&arena);
        text_free(&package);
    } else if (ok) {
        struct link_inputs in;
        struct link_command c;
        struct shared_options s = {NULL, NULL, NULL, NULL, is_plugin(o)};
        struct link_facts facts;
        struct windows_link w;
        struct text def = {0};
        struct text exported = {0};
        struct text versioned = {0};
        struct text glue = {0};
        struct text pcre2 = {0};
        const char **extra = o->objects;
        size_t extra_count = o->object_count;
        /* A library for C links PCRE2 as a program does. A plugin links
           no runtime, and the glue it calls is the host's. */
        if (!s.plugin) {
            ok = native_inputs(o, extras, &glue, &pcre2, &extra,
                               &extra_count);
        }
        memset(&in, 0, sizeof in);
        in.object = object;
        in.runtime = o->runtime;
        in.extra = extra;
        in.extra_count = extra_count;
        in.executable = text_cstr(&path);
        if (o->soname) {
            const char *version = o->package_version;
            if (version == NULL) {
                fputs("antic: --soname needs --package-version\n", stderr);
                ok = false;
            } else {
                text_appendf(&major, "%.*s", (int)strcspn(version, "."),
                             version);
                s.major = text_cstr(&major);
                s.version = version;
            }
        }
        if (ok && info->os == OS_LINUX && o->soname) {
            text_appendf(&versioned, "%s.%s", text_cstr(&path),
                         text_cstr(&major));
            in.executable = text_cstr(&versioned);
        }
        /* DESIGN: the exported surface of a shared library for C is
           the export functions and anti_licenses. Windows takes it as
           a .def file and macOS as an -exported_symbols_list. Linux
           takes --exclude-libs, because the runtime is an archive. */
        if (ok && info->os == OS_MACOS && !s.plugin) {
            struct text content = {0};
            const char *p = text_cstr(&extras->exports);
            text_appendf(&exported, "%s%s%s", text_cstr(&dir), name,
                         EXPORTED_SUFFIX);
            while (*p != '\0') {
                size_t n = strcspn(p, "\n");
                text_appendf(&content, "_%.*s\n", (int)n, p);
                p += n + (p[n] == '\n');
            }
            text_append(&content, "_anti_licenses\n");
            ok = write_file(text_cstr(&exported), &content);
            s.exported_file = text_cstr(&exported);
            text_free(&content);
        }
        /* DESIGN: a Windows plugin exports its table and the list of the
           places the loader fills with the addresses of the host. */
        if (ok && info->os == OS_WINDOWS && s.plugin) {
            struct text content = {0};
            text_appendf(&def, "%s%s%s", text_cstr(&dir), name, DEF_SUFFIX);
            text_appendf(&content, "LIBRARY %s\nEXPORTS\n"
                         "    anti_rt_provides DATA\n"
                         "    anti_rt_imports DATA\n", name);
            ok = write_file(text_cstr(&def), &content);
            s.def_file = text_cstr(&def);
            text_free(&content);
        }
        if (ok && info->os == OS_WINDOWS && !s.plugin) {
            struct text content = {0};
            const char *p = text_cstr(&extras->exports);
            text_appendf(&def, "%s%s%s", text_cstr(&dir), name, DEF_SUFFIX);
            text_appendf(&content, "LIBRARY %s\nEXPORTS\n", name);
            while (*p != '\0') {
                size_t n = strcspn(p, "\n");
                text_appendf(&content, "    %.*s\n", (int)n, p);
                p += n + (p[n] == '\n');
            }
            text_append(&content, "    anti_licenses DATA\n");
            ok = write_file(text_cstr(&def), &content);
            s.def_file = text_cstr(&def);
            text_free(&content);
        }
        memset(&facts, 0, sizeof facts);
        memset(&w, 0, sizeof w);
        ok = ok && link_facts(o, &in, &facts) &&
             (info->os != OS_WINDOWS || windows_link_paths(&w, &in, &s.def_file));
        if (ok) {
            link_shared_command(&c, o->target, &in, &s);
            ok = run_link(&w, &c);
            link_command_free(&c);
        }
        windows_link_free(&w);
        /* DESIGN: with --soname a Linux library is lib<name>.so.<major>,
           and lib<name>.so links to it for the C compiler. */
        if (ok && versioned.length > 0) {
            const char *link_name = strrchr(text_cstr(&versioned), '/');
            const char *argv[] = {"ln", "-sf", NULL, NULL, NULL};
            argv[2] = link_name != NULL ? link_name + 1 : text_cstr(&versioned);
            argv[3] = text_cstr(&path);
            ok = process_run(argv) == 0;
        }
        if (ok && s.plugin) {
            const char *file = strrchr(text_cstr(&path), '/');
            ok = write_plugin_index(text_cstr(&dir),
                                    file != NULL ? file + 1 : text_cstr(&path),
                                    text_cstr(&path), &extras->provides);
        }
        link_facts_free(&facts);
        text_free(&def);
        text_free(&exported);
        text_free(&versioned);
        if (extra != o->objects) {
            free((void *)extra);
        }
        text_free(&glue);
        text_free(&pcre2);
    }
    text_free(&dir);
    text_free(&path);
    text_free(&header);
    text_free(&major);
    return ok;
}

int driver_run(const struct options *o)
{
    struct text source = {0};
    struct text module = {0};
    struct text assembly = {0};
    struct text base = {0};
    struct text asm_path = {0};
    struct text obj_path = {0};
    struct extras extras;
    bool object_only = false;
    int status = 1;

    /* DESIGN: a caller that builds its own options sets the level, because
       the zero value of enum cpu_level names v1 rather than the target's
       default. A level of the other architecture is that mistake, and it
       would link a runtime directory that does not exist. */
    if (cpu_arch(o->cpu) != target_info(o->target)->arch) {
        fprintf(stderr, "antic: %s is no processor level of %s\n",
                cpu_name(o->cpu), target_name(o->target));
        return 2;
    }

    memset(&extras, 0, sizeof extras);

    if (!ends_with(o->input, SOURCE_SUFFIX) &&
        !(o->dev && ends_with(o->input, ANTL_SUFFIX))) {
        fprintf(stderr, "antic: %s: expected a %s file\n", o->input,
                SOURCE_SUFFIX);
        return 2;
    }
    if (links(o) && !o->dev && o->runtime == NULL) {
        fprintf(stderr, "antic: linking needs --runtime <dir>, the directory "
                        "that holds %s/<target>/<level>/\n",
                RUNTIME_LIB_DIR);
        return 2;
    }
    if (links(o) && !o->dev && !can_link(o)) {
        return 2;
    }
    if (o->lib != LIB_NONE && !o->assembly_only &&
        (o->runtime == NULL ||
         ((o->lib == LIB_SHARED ||
           (o->bundle_runtime &&
            target_info(o->target)->format != FORMAT_COFF)) &&
          !can_link(o)))) {
        if (o->runtime == NULL) {
            fprintf(stderr, "antic: --lib needs --runtime <dir>, the directory "
                            "that holds %s/<target>/<level>/\n",
                    RUNTIME_LIB_DIR);
        }
        return 2;
    }
    switch (ends_with(o->input, ANTL_SUFFIX)
                ? compile_library_file(o, &assembly, &base, &extras)
                : compile(o, &source, &module, &assembly, &base, &extras)) {
    case 0:
        break;
    case 2: /* a dump or a library file finished the command */
        status = 0;
        goto done;
    case 3: /* dev mode: a module without main stops at its object */
        object_only = true;
        break;
    default:
        goto done;
    }
    if (o->dev && !object_only && !o->assembly_only &&
        (o->runtime == NULL || !can_link(o))) {
        if (o->runtime == NULL) {
            fprintf(stderr, "antic: linking needs --runtime <dir>, the "
                            "directory that holds %s/<target>/\n",
                    RUNTIME_LIB_DIR);
        }
        goto done;
    }
    if (o->assembly_only) {
        if (o->output == NULL) {
            text_append(&base, ASSEMBLY_SUFFIX);
        }
        status = write_file(text_cstr(&base), &assembly) ? 0 : 1;
        goto done;
    }

    /* DESIGN: the assembly and object files stay beside the executable,
       so that a reader can open them. */
    text_appendf(&asm_path, "%s%s", text_cstr(&base), ASSEMBLY_SUFFIX);
    text_appendf(&obj_path, "%s%s", text_cstr(&base),
                 target_info(o->target)->object_suffix);
    if (!write_file(text_cstr(&asm_path), &assembly)) {
        goto done;
    }
    if (!assemble(o, text_cstr(&asm_path), text_cstr(&obj_path))) {
        goto done;
    }
    if (object_only) {
        status = 0;
        goto done;
    }
    if (o->lib != LIB_NONE) {
        status = build_c_library(o, text_cstr(&obj_path), text_cstr(&base),
                                 &extras)
                     ? 0
                     : 1;
        goto done;
    }
    if (o->output == NULL) {
        text_append(&base, target_info(o->target)->executable_suffix);
    }
    if (link_program(o, text_cstr(&obj_path), text_cstr(&base), &extras)) {
        status = 0;
    }

done:
    extras_free(&extras);
    text_free(&source);
    text_free(&module);
    text_free(&assembly);
    text_free(&base);
    text_free(&asm_path);
    text_free(&obj_path);
    return status;
}

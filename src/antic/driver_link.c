/* The link of a program: the facts of the target, Apple's SDK and the
   glibc start files, the inputs every link of a module takes, the paths
   of a Windows link, the exports of a host of plugins, the native
   libraries of the runtime archive and the command that links. */

#include "driver_parts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "applesdk.h"
#include "coff.h"
#include "platform.h"
#include "types.h"

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
        f = platform_open(text_cstr(&path), false);
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

void driver_link_facts_free(struct link_facts *f)
{
    text_free(&f->sdk_path);
    text_free(&f->sdk_version);
    text_free(&f->crt_dir);
    text_free(&f->sysroot);
    text_free(&f->lld_dir);
    text_free(&f->rpath);
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
    found = driver_file_exists(text_cstr(&marker)) &&
            driver_read_bytes(text_cstr(&marker), &version);
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
   glibc start files of the host. in->glibc and in->frameworks are set. */
static bool link_facts(const struct options *o, struct link_inputs *in,
                       struct link_facts *f)
{
    enum target t = o->target;
    enum target_os os = target_info(t)->os;
    struct text marker = {0};
    enum target host;
    bool present;

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
    text_appendf(&marker, "%s/%s%s", text_cstr(&f->lld_dir),
                 link_lld_flavour(t),
                 target_host(&host) ? target_info(host)->executable_suffix : "");
    in->lld_dir = driver_file_exists(text_cstr(&marker))
                      ? text_cstr(&f->lld_dir)
                      : NULL;
    text_free(&marker);
    text_appendf(&f->sysroot, "%s/%s/", o->runtime, RUNTIME_SYSROOT_DIR);
    link_target_dir(&f->sysroot, t, in->glibc);
    text_appendf(&marker, "%s/", text_cstr(&f->sysroot));
    link_sysroot_marker(&marker, t, in->glibc);
    present = driver_file_exists(text_cstr(&marker)) &&
              (os != OS_MACOS ||
               driver_read_bytes(text_cstr(&marker), &f->sdk_version));
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
    if (os == OS_MACOS && in->framework_count > 0) {
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

/* DESIGN: every link of a module takes its inputs from
   driver_link_inputs_of: a program, a library for C, a plugin and the
   join of a bundled runtime alike. The frameworks, the libraries of
   `link linux`, the glibc mode and the memory checks of the module then
   reach each of them. A shared library once filled its own and lost all
   four. */
/* The inputs of a link of object into executable, with the extra inputs
   of the command line. The caller frees f with driver_link_facts_free,
   also when this fails. */
bool driver_link_inputs_of(const struct options *o, const struct extras *extras,
                           const char *object, const char *executable,
                           struct link_inputs *in, struct link_facts *f)
{
    enum target_os os = target_info(o->target)->os;
    bool ok = true;

    memset(in, 0, sizeof *in);
    memset(f, 0, sizeof *f);
    in->object = object;
    in->executable = executable;
    in->runtime = o->runtime;
    in->extra = o->objects;
    in->extra_count = o->object_count;
    in->frameworks = o->frameworks;
    in->framework_count = o->framework_count;
    in->linux_libraries = o->linux_libraries;
    in->linux_library_count = o->linux_library_count;
    in->exports = extras->hosts_plugins;
    /* DESIGN: the runtime of AddressSanitizer is built against glibc,
       so a Linux link of --memory-checks takes the glibc mode. */
    in->glibc = os == OS_LINUX &&
                (o->linux_library_count > 0 || extras->hosts_plugins ||
                 o->memory_checks);
    in->memory_checks = o->memory_checks;
    if (o->memory_checks && os == OS_MACOS) {
        struct text dir = {0};
        text_appendf(&dir, "%s/%s/", o->runtime, RUNTIME_LIB_DIR);
        link_target_dir(&dir, o->target, false);
        ok = absolute_path(text_cstr(&dir), &f->rpath);
        text_free(&dir);
        in->rpath = text_cstr(&f->rpath);
    }
    return ok && link_facts(o, in, f);
}

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
        result = path_relative(&from, text_cstr(&absolute), text_cstr(&w->base))
                     ? windows_keep(w, &from)
                     : windows_keep(w, &absolute);
    }
    text_free(&absolute);
    text_free(&from);
    return result;
}

/* Name the paths of in, and the .def file of a DLL, from the directory of
   in->executable, where the link runs. */
bool driver_windows_link_paths(struct windows_link *w, struct link_inputs *in,
                               const char **def_file)
{
    const char *exe = in->executable;
    const char *cut = platform_last_separator(exe);
    const char **extra;
    size_t i;

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
        for (i = 0; i < w->directory.length; i++) {
            if (w->directory.data[i] == '/') {
                w->directory.data[i] = platform_separator();
            }
        }
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

void driver_windows_link_free(struct windows_link *w)
{
    arena_free(&w->arena);
    text_free(&w->directory);
    text_free(&w->base);
}

/* Run the command of a link, a Windows link in the directory it names. */
bool driver_run_link(const struct windows_link *w, const struct link_command *c)
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
    ok = driver_read_bytes(text_cstr(&library), &bytes) &&
         driver_read_bytes(object, &program);
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
    ok = ok && driver_write_file(text_cstr(def), &content);
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
   words of "CPU levels" in docs/anti-language-additions.md. *out is a new
   list of the objects of the command line, and of the two libraries when
   the program holds `anti.regex`. The caller frees it with free, whether
   the call succeeded or not. */
bool driver_native_inputs(const struct options *o, const struct extras *extras,
                          struct text *glue, struct text *pcre2,
                          const char ***out, size_t *count)
{
    enum cpu_level level = cpu_default(o->target);
    const char **list = alloc_zeroed(o->object_count + 2, sizeof *list);

    /* memcpy takes no null pointer, even for no bytes. */
    if (o->object_count > 0) {
        memcpy(list, o->objects, o->object_count * sizeof *list);
    }
    *out = list;
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
    list[o->object_count] = text_cstr(glue);
    list[o->object_count + 1] = text_cstr(pcre2);
    *count = o->object_count + 2;
    return true;
}

/* DESIGN: a Windows program of --memory-checks loads the DLL of
   AddressSanitizer. The link copies it from the runtime archive to the
   directory of the program, where the loader looks first. */
static bool copy_memcheck_dll(const struct options *o, const char *executable)
{
    struct text from = {0};
    struct text to = {0};
    struct text bytes = {0};
    const char *slash = platform_last_separator(executable);
    bool ok;

    link_memcheck_file(&from, o->runtime, o->target, false,
                       MEMCHECK_WINDOWS_DLL);
    if (slash != NULL) {
        text_append_bytes(&to, executable, (size_t)(slash - executable) + 1);
    }
    text_append(&to, MEMCHECK_WINDOWS_DLL);
    ok = driver_read_bytes(text_cstr(&from), &bytes) &&
         driver_write_file(text_cstr(&to), &bytes);
    text_free(&from);
    text_free(&to);
    text_free(&bytes);
    return ok;
}

bool driver_link_program(const struct options *o, const char *object,
                         const char *executable, const struct extras *extras)
{
    struct text glue = {0};
    struct text pcre2 = {0};
    const char **extra = NULL;
    size_t extra_count;
    struct link_inputs in;
    struct link_command command;
    struct link_facts facts;
    struct windows_link w;
    struct text def = {0};
    struct text implib = {0};
    enum target_os os = target_info(o->target)->os;
    bool ok;

    if (!driver_native_inputs(o, extras, &glue, &pcre2, &extra, &extra_count)) {
        free(extra);
        text_free(&glue);
        text_free(&pcre2);
        return false;
    }
    memset(&w, 0, sizeof w);
    ok = driver_link_inputs_of(o, extras, object, executable, &in, &facts);
    in.extra = extra;
    in.extra_count = extra_count;
    if (ok && in.exports && os == OS_WINDOWS) {
        ok = host_exports(o, object, executable, &extras->host_names, &def,
                          &implib);
        in.def_file = text_cstr(&def);
        in.import_library = text_cstr(&implib);
    }
    ok = ok &&
         (os != OS_WINDOWS || driver_windows_link_paths(&w, &in, &in.def_file));
    if (ok && in.import_library != NULL) {
        in.import_library = windows_path(&w, in.import_library, true);
    }
    if (ok) {
        link_command(&command, o->target, &in);
        ok = driver_run_link(&w, &command);
        link_command_free(&command);
    }
    if (ok && o->memory_checks && os == OS_WINDOWS) {
        ok = copy_memcheck_dll(o, executable);
    }
    driver_windows_link_free(&w);
    driver_link_facts_free(&facts);
    text_free(&def);
    text_free(&implib);
    free(extra);
    text_free(&glue);
    text_free(&pcre2);
    return ok;
}

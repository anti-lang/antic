/* `anti build` and `anti run`.

   DESIGN: the tool reads the manifest, resolves the dependency graph and
   calls the compiler once per module in dev mode and once for the whole
   program in release mode. It calls driver_run in this process, as
   `anti test` and `anti check` do, rather than starting antic again.
   Everything the compiler writes lands under `build/`, and the tool
   copies the deliverable into `dist/`. The split of docs/tooling.md then
   holds: `build/` is disposable and `dist/` is what a user runs. */

#include "build.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "antic.h"
#include "arena.h"
#include "cpu.h"
#include "deps.h"
#include "driver.h"
#include "files.h"
#include "licensing.h"
#include "manifest.h"
#include "memreport.h"
#include "modpath.h"
#include "platform.h"
#include "repo.h"
#include "syms.h"
#include "sha256.h"
#include "symmap.h"
#include "target.h"
#include "text.h"
#include "units.h"
#include "userdirs.h"
#include "zip.h"

/* The key of a cached output stands beside it, under its name and this
   suffix. */
#define BUILD_KEY_SUFFIX ".key"

struct path_context;
static int build_project(const struct build_request *r,
                         const struct path_context *outer);

/* A growing list of strings that the option lists point into. */
struct strings {
    const char **items;
    size_t count;
    size_t capacity;
};

static void strings_add(struct strings *list, const char *item)
{
    if (list->count == list->capacity) {
        list->items = files_grow(list->items, &list->capacity,
                                 sizeof *list->items);
    }
    list->items[list->count++] = item;
}

static void strings_free(struct strings *list)
{
    free((void *)list->items);
    memset(list, 0, sizeof *list);
}

/* Everything one build carries: the manifest, the graph, the modules and
   the directories of the target it is building. */
struct build {
    const struct build_request *r;
    int depth;
    struct manifest m;
    struct dep_graph graph;
    struct unit *units;
    size_t unit_count;
    size_t *order;
    struct text src;
    struct text runtime;
    struct text license;            /* the licence file of the manifest */
    struct files_list sources;       /* the source files under src */
    const char *roots[1];           /* the search root of the module paths */
    struct text *specs;             /* one `--dependency` entry per package */
    size_t spec_count;
    struct strings spec_list;
    struct strings attribution;
    /* The directories and the file name of the target being built. */
    struct text build_dir;
    struct text dist_dir;
    struct text lib_dir;
    struct text obj_dir;
    struct text name;
    /* The frameworks and the Linux libraries of every library file the
       program reaches, in framework_arena. */
    struct arena framework_arena;
    struct unit_links links;
    /* Whether the link of the program of this target took the C library
       of musl, which the driver writes through links_musl of its
       options. */
    bool links_musl;
};

/* What decides one output of the cache beside its input. The addendum
   names the version of the compiler and the target. The level joins them
   because it decides the instructions, and debug because it decides the
   lines. strict is --warnings-as-errors, which decides whether a warning
   stops the file, and checked is --memory-checks, which decides whether
   the object carries the checks. */
struct cache_flags {
    enum target target;
    enum cpu_level cpu;
    bool debug;
    bool strict;
    bool checked;
};

/* The cache key of one output: the digest of input, the version of the
   compiler and the flags f. */
static void cache_key(const char *input, const struct cache_flags *f,
                      struct text *out)
{
    struct anti_sha256 digest;
    struct text bytes = {0};
    char hex[65];

    anti_rt_sha256_init(&digest);
    if (files_read(input, &bytes)) {
        anti_rt_sha256_update(&digest, bytes.data, bytes.length);
    }
    anti_rt_sha256_hex(&digest, hex);
    text_appendf(out, "%s %s %s %s %s%s%s\n", hex, ANTIC_VERSION,
                 target_name(f->target), cpu_name(f->cpu),
                 f->debug ? "g" : "no-g", f->strict ? " strict" : "",
                 f->checked ? " memory-checks" : "");
    text_free(&bytes);
}

/* Whether the output stands and the key beside it is the one this build
   would write. */
static bool cached(const char *output, const struct text *key)
{
    struct text path = {0};
    struct text stored = {0};
    bool same;

    text_appendf(&path, "%s%s", output, BUILD_KEY_SUFFIX);
    same = files_exists(output) && files_read(text_cstr(&path), &stored) &&
           stored.length == key->length &&
           memcmp(stored.data, key->data, key->length) == 0;
    text_free(&path);
    text_free(&stored);
    return same;
}

/* Forget the key of an output, so a compile that fails halfway leaves no
   key that the next build would trust. */
static void drop_key(const char *output)
{
    struct text path = {0};

    text_appendf(&path, "%s%s", output, BUILD_KEY_SUFFIX);
    platform_remove(text_cstr(&path));
    text_free(&path);
}

static bool write_key(const char *output, const struct text *key)
{
    struct text path = {0};
    bool ok;

    text_appendf(&path, "%s%s", output, BUILD_KEY_SUFFIX);
    ok = files_write(text_cstr(&path), key);
    text_free(&path);
    return ok;
}

/* Append to out the options of o that decide an output beside its input
   and the flags of the cache: the package with its version, its licence,
   the text of its licence file, its dependencies and its attribution, the
   injections and --strip-docs. The text of the licence file goes in by its
   digest. Returns false when that file cannot be read.

   DESIGN: defect 1 of docs/audit/anti-tool.md. The key held the input,
   the compiler, the target, the level and three marks, so a build with
   --strip-docs took the cached library file with its doc text, and a new
   version in anti.toml left the library files and the objects as they
   were. "Versions" in docs/decisions.md puts that version in every class
   descriptor. Every option a compile takes from the manifest is in the
   key, and the frameworks and the Linux libraries, which the link alone
   reads, are not. */
static bool options_key(const struct options *o, struct text *out)
{
    size_t i;

    text_appendf(out, "package %s %s\n",
                 o->package_name != NULL ? o->package_name : "",
                 o->package_version != NULL ? o->package_version : "");
    if (o->license != NULL) {
        text_appendf(out, "license %s\n", o->license);
    }
    if (o->license_text != NULL) {
        char hex[65];
        if (!sha256_file(o->license_text, hex)) {
            fprintf(stderr, "anti: cannot read %s\n", o->license_text);
            return false;
        }
        text_appendf(out, "license-text %s\n", hex);
    }
    for (i = 0; i < o->dependency_count; i++) {
        text_appendf(out, "dependency %s\n", o->dependencies[i]);
    }
    for (i = 0; i < o->attribution_count; i++) {
        text_appendf(out, "attribution %s\n", o->attribution[i]);
    }
    for (i = 0; i < o->inject_count; i++) {
        text_appendf(out, "inject %s\n", o->inject[i]);
    }
    if (o->strip_docs) {
        text_append(out, "strip-docs\n");
    }
    return true;
}

/* Whether the objects carry the lines of -g. --memory-checks turns them
   on in release mode too, as it does for antic, since its reports name
   the lines. */
static bool build_debug(const struct build *b)
{
    return !b->r->release || b->r->memory_checks;
}

/* The options every call of one target shares, over the options of a
   project compile that unit_options gives. */
static void base_options(struct build *b, struct options *o,
                         enum target t, enum cpu_level cpu)
{
    unit_options(o, text_cstr(&b->m.name), text_cstr(&b->runtime), b->roots,
                 1, t, cpu);
    o->llvm_ar = b->r->llvm_ar;
    o->inject = b->m.inject.entries;
    o->inject_count = b->m.inject.count;
    /* The link writes the version into the notice and into the
       descriptor of every class of the module it compiles. A library
       file of the project carries the same version. */
    if (b->m.version.length > 0) {
        o->package_version = text_cstr(&b->m.version);
    }
    /* DESIGN: dev mode carries the line of every statement and release
       mode does not, which is the rule of docs/tooling.md. A release
       build of --memory-checks carries them for its reports. */
    o->debug = build_debug(b);
    o->memory_checks = b->r->memory_checks;
    unit_links_apply(&b->links, o);
}

/* The package header that every library file of this project carries.
   The name and the version stand in the options of every call. Every
   call that links takes it as well: the notice of a program and of a
   shared library names the package of the project with its licence, its
   text and its attributions, and a static library carries a copy of the
   header. A link without it wrote the name and the version alone. */
static void header_options(const struct build *b, struct options *o)
{
    if (b->m.license.length > 0) {
        o->license = text_cstr(&b->m.license);
    }
    if (b->license.length > 0) {
        o->license_text = text_cstr(&b->license);
    }
    o->dependencies = b->spec_list.items;
    o->dependency_count = b->spec_list.count;
    o->attribution = b->attribution.items;
    o->attribution_count = b->attribution.count;
}

/* The library files of the dependency graph, which every call of the
   build passes beside the ones of the project. */
static void graph_libraries(const struct build *b, struct strings *out)
{
    size_t i;
    size_t j;

    for (i = 0; i < b->graph.count; i++) {
        for (j = 0; j < b->graph.packages[i].module_count; j++) {
            strings_add(out, text_cstr(&b->graph.packages[i].modules[j].file));
        }
    }
}

/* Append to out the digest of every library file that the library file
   library reaches through its imports, and of every library file of the
   dependency graph, apart from library itself. Returns false when one
   cannot be read.

   DESIGN: decision 15 of "Decisions of 2026-10-11", by defect 10 of
   docs/audit/anti-tool.md. The key of a library file was over its own
   source, so a module whose import changed its interface was not checked
   again, and its object kept the layout of a struct that had grown. The
   search finds a module of the project under lib/ of the build and one
   of the standard library in the runtime archive. A library file of the
   graph stands under no root its module path names, so every one of them
   is in the key. A new version of a dependency then builds every module
   again, which the lock file makes rare. */
static bool imports_key(const struct build *b, enum target t,
                        enum cpu_level cpu, const char *library,
                        struct text *out)
{
    struct arena arena = {0};
    struct options search;
    struct strings seed = {0};
    const char *roots[1];
    const char **closure = NULL;    /* the memory pool holds the list */
    size_t count = 0;
    size_t i;
    bool ok;

    strings_add(&seed, library);
    graph_libraries(b, &seed);
    roots[0] = text_cstr(&b->lib_dir);
    unit_options(&search, text_cstr(&b->m.name), text_cstr(&b->runtime), roots,
                 1, t, cpu);
    search.libraries = seed.items;
    search.library_count = seed.count;
    ok = driver_libraries(&search, &arena, &closure, &count);
    for (i = 0; ok && i < count; i++) {
        char hex[65];
        if (strcmp(closure[i], library) == 0) {
            continue;
        }
        ok = sha256_file(closure[i], hex);
        if (!ok) {
            fprintf(stderr, "anti: cannot read %s\n", closure[i]);
        } else {
            text_appendf(out, "import %s %s\n", closure[i], hex);
        }
    }
    strings_free(&seed);
    arena_free(&arena);
    return ok;
}

/* Whether the file at path is a library file of this compiler, asked
   without a message. A library file of an older format is no cache. */
static bool library_readable(const char *path)
{
    struct arena arena = {0};
    struct text bytes = {0};
    struct package package;
    const char *module;
    char error[200];
    bool ok = files_read(path, &bytes) &&
              antic_library_header((const uint8_t *)bytes.data, bytes.length,
                                   &arena, &package, &module, error,
                                   sizeof error);

    text_free(&bytes);
    arena_free(&arena);
    return ok;
}

/* Write the library file of one module, unless the cache holds it. */
static bool module_library(struct build *b, const struct unit *u,
                           enum target t, enum cpu_level cpu,
                           const struct strings *libraries,
                           struct text *output)
{
    struct options o;
    struct cache_flags flags = {0};
    struct text base = {0};
    struct text key = {0};
    bool ok = false;

    if (!unit_file(text_cstr(&b->lib_dir), text_cstr(&u->path), ANTL_SUFFIX,
                   output)) {
        goto done;
    }
    base_options(b, &o, t, cpu);
    header_options(b, &o);
    o.input = u->source;
    o.output = text_cstr(output);
    o.library = true;
    o.warnings_as_errors = b->r->release;
    o.strip_docs = b->r->strip_docs;
    o.libraries = libraries->items;
    o.library_count = libraries->count;
    /* DESIGN: a release build accepts no warning, so it checks every
       module of the project with --warnings-as-errors. A library file
       that a dev build wrote was checked without it, and the key keeps
       the two apart. */
    flags.target = t;
    flags.cpu = cpu;
    flags.strict = b->r->release;
    cache_key(u->source, &flags, &base);
    if (!options_key(&o, &base)) {
        goto done;
    }
    /* The imports of the library file that stands are those of its
       source, which the digest of the source holds. A new library file
       takes its key from the imports it names after the compile. */
    text_append(&key, text_cstr(&base));
    if (library_readable(text_cstr(output)) &&
        imports_key(b, t, cpu, text_cstr(output), &key) &&
        cached(text_cstr(output), &key)) {
        ok = true;
        goto done;
    }
    drop_key(text_cstr(output));
    if (driver_run(&o) != 0) {
        goto done;
    }
    key.length = 0;
    text_append(&key, text_cstr(&base));
    ok = imports_key(b, t, cpu, text_cstr(output), &key) &&
         write_key(text_cstr(output), &key);
done:
    text_free(&base);
    text_free(&key);
    return ok;
}

/* Write the object of one library file, unless the cache holds it. A
   module without `main` stops at its object, which antic reports as
   status 3. */
static bool module_object(struct build *b, const char *library,
                          const char *module, enum target t,
                          enum cpu_level cpu, const struct strings *libraries,
                          struct text *out)
{
    struct options o;
    struct cache_flags flags = {0};
    struct strings rest = {0};
    struct text base = {0};
    struct text key = {0};
    size_t i;
    int status;
    bool ok = false;

    if (!unit_file(text_cstr(&b->obj_dir), module, "", &base)) {
        goto done;
    }
    text_appendf(out, "%s%s", text_cstr(&base), target_info(t)->object_suffix);
    /* The input is one of the library files of the build, and antic
       refuses a module that stands twice on its command line. */
    for (i = 0; i < libraries->count; i++) {
        if (strcmp(libraries->items[i], library) != 0) {
            strings_add(&rest, libraries->items[i]);
        }
    }
    base_options(b, &o, t, cpu);
    o.input = library;
    o.output = text_cstr(&base);
    o.dev = true;
    o.libraries = rest.items;
    o.library_count = rest.count;
    flags.target = t;
    flags.cpu = cpu;
    flags.debug = build_debug(b);
    flags.checked = b->r->memory_checks;
    cache_key(library, &flags, &key);
    if (!options_key(&o, &key) ||
        !imports_key(b, t, cpu, library, &key)) {
        goto done;
    }
    if (cached(text_cstr(out), &key)) {
        ok = true;
        goto done;
    }
    drop_key(text_cstr(out));
    status = driver_run(&o);
    ok = (status == 0 || status == 3) && write_key(text_cstr(out), &key);
done:
    strings_free(&rest);
    text_free(&base);
    text_free(&key);
    return ok;
}

/* The module path a library file names, which gives the object of a dev
   build its place under `obj/`. */
static bool library_module(const char *file, struct text *out)
{
    struct arena arena = {0};
    struct package package;
    const char *module;
    bool ok = deps_library_header(file, &arena, &package, &module);

    if (ok) {
        text_append(out, module);
    }
    arena_free(&arena);
    return ok;
}

/* Copy the file from into the directory to, under the name it carries. */
static bool copy_into(const char *from, const char *to, const char *name)
{
    struct text path = {0};
    bool ok;

    text_appendf(&path, "%s/%s", to, name);
    ok = files_make_dirs(to) && files_copy_program(from, text_cstr(&path));
    if (!ok) {
        fprintf(stderr, "anti: cannot write %s\n", text_cstr(&path));
    }
    text_free(&path);
    return ok;
}

/* Whether the library of the build is lib<name>.so.<major> with the
   link lib<name>.so to it, as antic writes it for --soname on Linux. */
static bool soname_link(const struct build *b, enum target t)
{
    return b->r->soname && b->r->lib == BUILD_LIB_SHARED &&
           target_info(t)->os == OS_LINUX && b->m.version.length > 0;
}

/* DESIGN: dist/ of a Linux library of --soname holds the file its soname
   names and the link beside it, as the build directory does, so a C
   program linked against dist/ finds what the loader asks for, by T099.
   The major version is the one antic takes from --package-version. */
static bool deliver_soname(const struct build *b, const char *deliverable)
{
    const char *version = text_cstr(&b->m.version);
    struct text versioned = {0};
    struct text from = {0};
    struct text link = {0};
    bool ok;

    text_appendf(&versioned, "%s.%.*s", deliverable,
                 (int)strcspn(version, "."), version);
    text_appendf(&from, "%s/%s", text_cstr(&b->build_dir),
                 text_cstr(&versioned));
    text_appendf(&link, "%s/%s", text_cstr(&b->dist_dir), deliverable);
    ok = copy_into(text_cstr(&from), text_cstr(&b->dist_dir),
                   text_cstr(&versioned));
    if (ok && !platform_symlink(text_cstr(&versioned), text_cstr(&link))) {
        fprintf(stderr, "anti: cannot write %s\n", text_cstr(&link));
        ok = false;
    }
    text_free(&versioned);
    text_free(&from);
    text_free(&link);
    return ok;
}

/* The file beside a binary in dist/ that holds the text of its notice. */
#define NOTICE_FILE "NOTICE.txt"

/* The package of the project itself, as the header of its library files
   carries it: the fields of the manifest and the text of its licence
   file. text holds the bytes of that file. */
static bool own_package(const struct build *b, struct text *text,
                        struct package *out)
{
    memset(out, 0, sizeof *out);
    out->name = text_cstr(&b->m.name);
    out->version = b->m.version.length > 0 ? text_cstr(&b->m.version)
                                           : PACKAGE_VERSION_DEFAULT;
    out->license = text_cstr(&b->m.license);
    out->attribution = b->attribution.items;
    out->attribution_count = b->attribution.count;
    if (b->license.length > 0 &&
        !files_read_reported(text_cstr(&b->license), text)) {
        return false;
    }
    out->license_text = text_cstr(text);
    return true;
}

/* [provisional] DESIGN: the notice of a project comes from the project
   and not from the binary. It names the runtime, musl and mimalloc where
   the link of the program took them, every package of anti.lock, the
   package of every bundled module the project imports and the package of
   the project, in the lines of the notice of a binary, with the upstream
   source of each component of the runtime archive after its text. The
   bundled modules are the library files that the modules of the project
   and of the lock reach and that belong to neither, which
   driver_libraries finds under std/ of the runtime archive. A package of
   the lock that no module imports is named all the same, since the lock
   is what the project declares. `anti build` writes the notice as
   NOTICE.txt beside a program or a shared library, `anti license
   --project --notice` beside whatever it built, and `anti license
   --project` prints it. files[i] is the library file of unit i. */
static bool deliver_notice(struct build *b, enum target t, enum cpu_level cpu,
                           const struct text *files, bool beside)
{
    struct arena arena = {0};
    struct licensing_project project = {0};
    struct options search;
    struct strings seed = {0};
    struct strings libraries = {0};
    struct package own;
    struct text own_text = {0};
    struct text notice = {0};
    struct text path = {0};
    const char **closure = NULL;    /* the memory pool holds the list */
    size_t count = 0;
    size_t i;
    size_t j;
    bool ok;

    if (!beside && b->r->notice == BUILD_NOTICE_BUILD) {
        return true;
    }
    for (i = 0; i < b->unit_count; i++) {
        strings_add(&seed, text_cstr(&files[i]));
    }
    graph_libraries(b, &seed);
    base_options(b, &search, t, cpu);
    search.libraries = seed.items;
    search.library_count = seed.count;
    ok = driver_libraries(&search, &arena, &closure, &count) &&
         own_package(b, &own_text, &own);
    graph_libraries(b, &libraries);
    for (i = 0; ok && i < count; i++) {
        for (j = 0; j < seed.count; j++) {
            if (strcmp(closure[i], seed.items[j]) == 0) {
                break;
            }
        }
        if (j == seed.count) {
            strings_add(&libraries, closure[i]);
        }
    }
    project.runtime = text_cstr(&b->runtime);
    project.musl = b->links_musl;
    project.libraries = libraries.items;
    project.library_count = libraries.count;
    project.own = &own;
    ok = ok && licensing_project(&project, &notice);
    if (ok && (beside || b->r->notice == BUILD_NOTICE_WRITE)) {
        text_appendf(&path, "%s/%s", text_cstr(&b->dist_dir), NOTICE_FILE);
        ok = files_write(text_cstr(&path), &notice);
    }
    if (ok && b->r->notice == BUILD_NOTICE_PRINT && b->depth == 0) {
        fputs(text_cstr(&notice), stdout);
    }
    strings_free(&seed);
    strings_free(&libraries);
    text_free(&own_text);
    text_free(&notice);
    text_free(&path);
    arena_free(&arena);
    return ok;
}

/* The module of the project that holds `main`, or the count of the
   modules when none does, which is a library project. */
static size_t linking_module(const struct build *b)
{
    size_t i;

    for (i = 0; i < b->unit_count; i++) {
        if (b->units[i].has_main) {
            return i;
        }
    }
    return b->unit_count;
}

/* Whether the project holds the module of path. */
static bool has_module(const struct build *b, const char *path)
{
    size_t i;

    for (i = 0; i < b->unit_count; i++) {
        if (strcmp(text_cstr(&b->units[i].path), path) == 0) {
            return true;
        }
    }
    return false;
}

/* The module whose path is the package name, which a library for C is
   built from. Without one the last module of the order stands. */
static size_t root_module(const struct build *b)
{
    size_t i;

    for (i = 0; i < b->unit_count; i++) {
        if (strcmp(text_cstr(&b->units[i].path), text_cstr(&b->m.name)) == 0) {
            return i;
        }
    }
    return b->unit_count > 0 ? b->order[b->unit_count - 1] : 0;
}

/* Dev mode: one object per module, cached by digest, and one link that
   takes them all. */
static bool build_dev(struct build *b, enum target t, enum cpu_level cpu,
                      const struct text *files, size_t main_at,
                      const struct strings *libraries)
{
    struct arena arena = {0};
    struct options o;
    struct options search;
    struct strings seed = {0};
    struct strings objects = {0};
    struct text *paths = NULL;
    const char **closure = NULL;    /* the memory pool holds the list */
    size_t count = 0;
    size_t at = 0;
    size_t i;
    size_t j;
    bool ok = false;

    /* DESIGN: a dev build compiles one module into its own object, so
       the link needs an object of every module it reaches. That is the
       modules of this project, the modules of the dependency graph and
       the modules of the standard library below them. driver_libraries
       walks the imports of the seed and gives all three. */
    for (i = 0; i < b->unit_count; i++) {
        strings_add(&seed, text_cstr(&files[i]));
    }
    for (i = 0; i < b->graph.count; i++) {
        for (j = 0; j < b->graph.packages[i].module_count; j++) {
            strings_add(&seed, text_cstr(&b->graph.packages[i].modules[j].file));
        }
    }
    base_options(b, &search, t, cpu);
    search.input = NULL;
    search.libraries = seed.items;
    search.library_count = seed.count;
    if (!driver_libraries(&search, &arena, &closure, &count)) {
        goto done;
    }
    paths = files_array(count + 1, sizeof *paths);
    for (i = 0; i < count; i++) {
        struct text module = {0};
        bool written;
        if (strcmp(closure[i], text_cstr(&files[main_at])) == 0) {
            continue;
        }
        if (!library_module(closure[i], &module)) {
            text_free(&module);
            goto done;
        }
        written = module_object(b, closure[i], text_cstr(&module), t, cpu,
                                libraries, &paths[at]);
        text_free(&module);
        if (!written) {
            goto done;
        }
        strings_add(&objects, text_cstr(&paths[at]));
        at++;
    }
    /* DESIGN: antic stops a dev build of a library file at its object,
       so the module that links is compiled from its source. It is the
       one module of a dev build that every build compiles again. The
       link it carries is the relink that every build runs. */
    base_options(b, &o, t, cpu);
    header_options(b, &o);
    o.input = b->units[main_at].source;
    o.output = text_cstr(&b->name);
    o.dev = true;
    o.libraries = libraries->items;
    o.library_count = libraries->count;
    o.objects = objects.items;
    o.object_count = objects.count;
    o.links_musl = &b->links_musl;
    ok = driver_run(&o) == 0;
done:
    for (i = 0; paths != NULL && i < count; i++) {
        text_free(&paths[i]);
    }
    free(paths);
    strings_free(&seed);
    strings_free(&objects);
    arena_free(&arena);
    return ok;
}

/* The --lto of the request, whose word the command line checked, on the
   options of a release program. */
static void release_lto(const struct build *b, struct options *o)
{
    if (b->r->lto != NULL) {
        o->lto_given = link_lto_from_name(b->r->lto, &o->lto);
    }
}

/* Release mode: one call with every module, so the optimizer sees the
   whole program. */
static bool build_release(struct build *b, enum target t, enum cpu_level cpu,
                          size_t main_at, const struct strings *libraries)
{
    struct options o;

    base_options(b, &o, t, cpu);
    header_options(b, &o);
    o.input = b->units[main_at].source;
    o.output = text_cstr(&b->name);
    o.libraries = libraries->items;
    o.library_count = libraries->count;
    o.profile_generate = b->r->profile_generate;
    o.profile_use = b->r->profile_use;
    release_lto(b, &o);
    o.links_musl = &b->links_musl;
    return driver_run(&o) == 0;
}

/* The name of the deliverable without the suffix of an executable, which
   the entries of a symbols archive carry. */
static void archive_stem(enum target t, const char *name, struct text *out)
{
    text_append(out, name);
    files_cut_suffix(out, target_info(t)->executable_suffix);
}

/* DESIGN: a release binary carries no symbol data, so the build writes
   the archive that names it. It holds the same link with the debug
   sections kept and the map of that program. All three carry the build
   id of the binary, because the digest leaves what `-g` added out. That
   id is what ties a frame of a trace to this archive. A Windows link
   writes the symbols to a PDB. The archive holds the PDB of the release
   link, whose GUID the shipped program carries. Eddie decided this. The
   debug link of Windows is named <stem>.debug.exe, so its own PDB is
   <stem>.debug.pdb and leaves the one of the release link alone. Named
   <stem>.debug, it wrote <stem>.pdb over it. */
static bool build_symbols(struct build *b, enum target t, enum cpu_level cpu,
                          size_t main_at, const struct strings *libraries,
                          const char *deliverable)
{
    struct options o;
    struct zip_entry entries[3];
    struct text stem = {0};
    struct text debug_path = {0};
    struct text debug_name = {0};
    struct text map_path = {0};
    struct text map_name = {0};
    struct text pdb_path = {0};
    struct text pdb_name = {0};
    struct text archive = {0};
    struct text id = {0};
    size_t count = 0;
    bool ok = false;

    memset(entries, 0, sizeof entries);
    archive_stem(t, deliverable, &stem);
    text_appendf(&debug_name, "%s.debug", text_cstr(&stem));
    text_appendf(&map_name, "%s.map", text_cstr(&stem));
    text_appendf(&debug_path, "%s/%s%s", text_cstr(&b->build_dir),
                 text_cstr(&debug_name),
                 target_info(t)->format == FORMAT_COFF ? ".exe" : "");
    text_appendf(&map_path, "%s/%s", text_cstr(&b->build_dir),
                 text_cstr(&map_name));
    text_appendf(&archive, "%s/%s%s", text_cstr(&b->dist_dir),
                 text_cstr(&stem), SYMS_ARCHIVE_SUFFIX);
    base_options(b, &o, t, cpu);
    header_options(b, &o);
    o.input = b->units[main_at].source;
    o.output = text_cstr(&debug_path);
    o.libraries = libraries->items;
    o.library_count = libraries->count;
    o.debug = true;
    o.profile_generate = b->r->profile_generate;
    o.profile_use = b->r->profile_use;
    release_lto(b, &o);
    if (driver_run(&o) != 0) {
        goto done;
    }
    if (!symmap_build_id(text_cstr(&b->name), &id) ||
        !symmap_write(text_cstr(&debug_path), t, text_cstr(&id),
                      text_cstr(&map_path))) {
        goto done;
    }
    entries[count].name = text_cstr(&debug_name);
    entries[count].file = text_cstr(&debug_path);
    entries[count].executable = true;
    count++;
    if (target_info(t)->format == FORMAT_COFF) {
        /* The PDB of the release link, beside the program in the build
           directory. Every Windows link writes one. */
        text_appendf(&pdb_name, "%s.pdb", text_cstr(&stem));
        link_pdb_path(&pdb_path, text_cstr(&b->name));
        if (!files_exists(text_cstr(&pdb_path))) {
            fprintf(stderr, "anti: the link of %s wrote no %s\n",
                    text_cstr(&b->name), text_cstr(&pdb_path));
            goto done;
        }
        entries[count].name = text_cstr(&pdb_name);
        entries[count].file = text_cstr(&pdb_path);
        entries[count].executable = false;
        count++;
    }
    entries[count].name = text_cstr(&map_name);
    entries[count].file = text_cstr(&map_path);
    entries[count].executable = false;
    count++;
    ok = zip_write(text_cstr(&archive), entries, count);
done:
    text_free(&stem);
    text_free(&debug_path);
    text_free(&debug_name);
    text_free(&map_path);
    text_free(&map_name);
    text_free(&pdb_path);
    text_free(&pdb_name);
    text_free(&archive);
    text_free(&id);
    return ok;
}

/* The file name of a library for C on a target. */
static void library_name(enum target t, bool shared, const char *name,
                         struct text *out)
{
    const struct target_info *info = target_info(t);

    if (!shared) {
        text_appendf(out, info->format == FORMAT_COFF ? "%s.lib" : "lib%s.a",
                     name);
        return;
    }
    text_appendf(out,
                 info->os == OS_WINDOWS  ? "%s.dll"
                 : info->os == OS_MACOS  ? "lib%s.dylib"
                                         : "lib%s.so",
                 name);
}

/* `--lib static` and `--lib shared`: the whole program as a library for
   C, with the header beside it. */
static bool build_c_library(struct build *b, enum target t, enum cpu_level cpu,
                            const struct strings *libraries)
{
    struct options o;
    const char *base = modpath_last(text_cstr(&b->m.name));
    bool ok;

    base_options(b, &o, t, cpu);
    header_options(b, &o);
    o.input = b->units[root_module(b)].source;
    o.output = text_cstr(&b->name);
    o.libraries = libraries->items;
    o.library_count = libraries->count;
    o.lib = b->r->lib == BUILD_LIB_STATIC ? LIB_STATIC : LIB_SHARED;
    o.bundle_runtime = b->r->bundle_runtime;
    o.soname = b->r->soname;
    ok = driver_run(&o) == 0;
    if (ok) {
        struct text file = {0};
        struct text name = {0};
        text_appendf(&name, "%s%s", base, HEADER_SUFFIX);
        text_appendf(&file, "%s/%s", text_cstr(&b->build_dir),
                     text_cstr(&name));
        ok = copy_into(text_cstr(&file), text_cstr(&b->dist_dir),
                       text_cstr(&name));
        text_free(&file);
        text_free(&name);
    }
    /* A C program links against a DLL through its import library, which
       the link writes beside the DLL under the name a static library of
       Windows takes, so dist/ holds it beside the DLL and the header. */
    if (ok && o.lib == LIB_SHARED && target_info(t)->os == OS_WINDOWS) {
        struct text file = {0};
        struct text name = {0};
        library_name(t, false, base, &name);
        text_appendf(&file, "%s/%s", text_cstr(&b->build_dir),
                     text_cstr(&name));
        ok = copy_into(text_cstr(&file), text_cstr(&b->dist_dir),
                       text_cstr(&name));
        text_free(&file);
        text_free(&name);
    }
    return ok;
}

/* The links of every library file the program reaches, which the DESIGN
   comment of struct unit_links explains. */
static bool link_frameworks(struct build *b, enum target t,
                            enum cpu_level cpu, const struct text *files,
                            const struct strings *shared)
{
    struct options search;
    struct strings seed = {0};
    size_t i;
    bool ok;

    for (i = 0; i < shared->count; i++) {
        strings_add(&seed, shared->items[i]);
    }
    for (i = 0; i < b->unit_count; i++) {
        strings_add(&seed, text_cstr(&files[i]));
    }
    memset(&b->links, 0, sizeof b->links);
    base_options(b, &search, t, cpu);
    search.libraries = seed.items;
    search.library_count = seed.count;
    ok = unit_links_read(&search, &b->framework_arena, &b->links);
    strings_free(&seed);
    return ok;
}

/* A project without `main` is a library project, whose build writes the
   library file of each module, files[i] of unit i, into `dist/`. */
static bool write_library_files(const struct build *b,
                                const struct text *files)
{
    size_t i;

    for (i = 0; i < b->unit_count; i++) {
        struct text out = {0};
        bool written =
            unit_file(text_cstr(&b->dist_dir), text_cstr(&b->units[i].path),
                      ANTL_SUFFIX, &out) &&
            files_copy(text_cstr(&files[i]), text_cstr(&out));
        if (!written) {
            fprintf(stderr, "anti: cannot write %s\n", text_cstr(&out));
        }
        text_free(&out);
        if (!written) {
            return false;
        }
    }
    return true;
}

/* Build one target in one mode. */
static bool build_target(struct build *b, enum target t, enum cpu_level cpu)
{
    const char *mode = b->r->release ? "release" : "dev";
    struct strings shared = {0};
    struct strings libraries = {0};
    struct text *files = NULL;
    struct text deliverable = {0};
    size_t main_at;
    size_t skip;
    size_t i;
    size_t j;
    bool ok = false;

    b->build_dir.length = 0;
    b->dist_dir.length = 0;
    b->lib_dir.length = 0;
    b->obj_dir.length = 0;
    b->name.length = 0;
    b->links_musl = false;
    text_appendf(&b->build_dir, "%s/%s/%s/%s", b->r->root,
                 text_cstr(&b->m.build), target_name(t), mode);
    text_appendf(&b->dist_dir, "%s/%s/%s/%s", b->r->root,
                 text_cstr(&b->m.dist), target_name(t), mode);
    text_appendf(&b->lib_dir, "%s/lib", text_cstr(&b->build_dir));
    text_appendf(&b->obj_dir, "%s/obj", text_cstr(&b->build_dir));
    if (!files_make_dirs_or_say(text_cstr(&b->build_dir)) ||
        !files_make_dirs_or_say(text_cstr(&b->dist_dir))) {
        goto done;
    }
    graph_libraries(b, &shared);
    files = files_array(b->unit_count + 1, sizeof *files);
    /* Every library file first, in the order of the imports, so a module
       that imports another of the project finds it. */
    for (i = 0; i < b->unit_count; i++) {
        libraries.count = 0;
        for (j = 0; j < shared.count; j++) {
            strings_add(&libraries, shared.items[j]);
        }
        for (j = 0; j < i; j++) {
            strings_add(&libraries, text_cstr(&files[b->order[j]]));
        }
        if (!module_library(b, &b->units[b->order[i]], t, cpu, &libraries,
                            &files[b->order[i]])) {
            goto done;
        }
    }
    main_at = linking_module(b);
    if (b->r->lib == BUILD_PROGRAM && main_at == b->unit_count) {
        ok = write_library_files(b, files) &&
             deliver_notice(b, t, cpu, files, false);
        goto done;
    }
    /* The link reads the module that carries `main` as its input, so its
       own library file stays off the list. A library for C is built from
       the root module and needs every other one. */
    skip = b->r->lib == BUILD_PROGRAM ? main_at : root_module(b);
    libraries.count = 0;
    for (j = 0; j < shared.count; j++) {
        strings_add(&libraries, shared.items[j]);
    }
    for (j = 0; j < b->unit_count; j++) {
        if (b->order[j] != skip) {
            strings_add(&libraries, text_cstr(&files[b->order[j]]));
        }
    }
    if (b->r->lib != BUILD_PROGRAM) {
        library_name(t, b->r->lib == BUILD_LIB_SHARED,
                     modpath_last(text_cstr(&b->m.name)), &deliverable);
    } else {
        text_appendf(&deliverable, "%s%s",
                     modpath_last(text_cstr(&b->m.name)),
                     target_info(t)->executable_suffix);
    }
    text_appendf(&b->name, "%s/%s", text_cstr(&b->build_dir),
                 text_cstr(&deliverable));
    if (!link_frameworks(b, t, cpu, files, &shared)) {
        goto done;
    }
    if (b->r->lib != BUILD_PROGRAM) {
        ok = build_c_library(b, t, cpu, &libraries);
    } else if (b->r->release) {
        ok = build_release(b, t, cpu, main_at, &libraries);
    } else {
        ok = build_dev(b, t, cpu, files, main_at, &libraries);
    }
    if (ok && soname_link(b, t)) {
        ok = deliver_soname(b, text_cstr(&deliverable));
    } else if (ok) {
        ok = copy_into(text_cstr(&b->name), text_cstr(&b->dist_dir),
                       text_cstr(&deliverable));
    }
    if (ok) {
        ok = deliver_notice(b, t, cpu, files, b->r->lib != BUILD_LIB_STATIC);
    }
    /* DESIGN: a Windows program of --memory-checks loads the DLL of
       AddressSanitizer from its own directory. The link wrote it beside
       the program in the build directory, and dist/ takes it as well, so
       `anti run` starts a program that finds it. */
    if (ok && b->r->memory_checks && b->r->lib == BUILD_PROGRAM &&
        driver_memcheck_companion(t) != NULL) {
        struct text companion = {0};
        text_appendf(&companion, "%s/%s", text_cstr(&b->build_dir),
                     driver_memcheck_companion(t));
        ok = copy_into(text_cstr(&companion), text_cstr(&b->dist_dir),
                       driver_memcheck_companion(t));
        text_free(&companion);
    }
    if (ok && b->r->release && b->r->lib == BUILD_PROGRAM) {
        ok = build_symbols(b, t, cpu, main_at, &libraries,
                           text_cstr(&deliverable));
    }
done:
    for (i = 0; files != NULL && i < b->unit_count; i++) {
        text_free(&files[i]);
    }
    free(files);
    text_free(&deliverable);
    strings_free(&shared);
    strings_free(&libraries);
    return ok;
}

/* The context a path dependency is built with: the request of the
   project that names it, that project and the chain of the projects whose
   builds wait for it, outer first.

   DESIGN: a path dependency may be a project, whose build is a build of
   its own. R165 of "Rules of 2026-10-13": a project that stands in the
   chain already closes a cycle, which is refused at any depth with the
   packages of the cycle named. The chain compares the resolved
   directories, so two spellings of one directory are one project. */
struct path_context {
    const struct build_request *r;
    const struct path_context *outer;
    struct text directory;
    const char *name;
    int depth;
};

/* Report the cycle that the project in directory closes, when the chain
   of c holds it. */
static bool path_cycle(const struct path_context *c, const char *directory)
{
    const struct path_context *at;
    const struct path_context *first = NULL;
    struct text resolved = {0};
    struct text line = {0};
    const char **names;
    size_t count = 0;
    size_t i;

    if (!absolute_path(directory, &resolved)) {
        text_free(&resolved);
        return false;
    }
    for (at = c; at != NULL && first == NULL; at = at->outer) {
        count++;
        if (strcmp(text_cstr(&at->directory), text_cstr(&resolved)) == 0) {
            first = at;
        }
    }
    text_free(&resolved);
    if (first == NULL) {
        return false;
    }
    names = files_array(count + 1, sizeof *names);
    i = count;
    for (at = c; i > 0; at = at->outer) {
        names[--i] = at->name;
    }
    for (i = 0; i < count; i++) {
        text_appendf(&line, "%s -> ", names[i]);
    }
    fprintf(stderr, "anti: the path dependencies form a cycle: %s%s\n",
            text_cstr(&line), first->name);
    text_free(&line);
    free((void *)names);
    return true;
}

/* Where the library files of a path dependency stand. A directory that
   holds a manifest is a project, which is built for the host first. */
static bool path_dependency(void *context, const char *directory,
                            struct text *out)
{
    struct path_context *c = context;
    struct build_request sub;
    struct manifest m;
    struct text manifest_path = {0};
    enum target host;
    bool ok = false;

    text_appendf(&manifest_path, "%s/%s", directory, MANIFEST_FILE);
    if (!files_exists(text_cstr(&manifest_path))) {
        text_append(out, directory);
        text_free(&manifest_path);
        return true;
    }
    if (path_cycle(c, directory)) {
        text_free(&manifest_path);
        return false;
    }
    if (!target_host(&host) || !manifest_read(text_cstr(&manifest_path), false,
                                              &m)) {
        text_free(&manifest_path);
        return false;
    }
    sub = *c->r;
    sub.root = directory;
    /* The dependency is built for the host, whose dist directory is read
       below. Without a target its build took the default targets of its
       own manifest, and a manifest that names another target wrote its
       library files where nothing reads them. */
    sub.target = target_name(host);
    /* The level of the request is one of the targets of the request, and
       the dependency takes the default level of the host. The level of
       an x86_64 target refused the build of an ARM64 host, by T144. */
    sub.cpu = NULL;
    sub.lib = BUILD_PROGRAM;
    sub.run = false;
    sub.notice = BUILD_NOTICE_BUILD;
    if (build_project(&sub, c) == 0) {
        text_appendf(out, "%s/%s/%s/%s", directory, text_cstr(&m.dist),
                     target_name(host), c->r->release ? "release" : "dev");
        ok = true;
    }
    manifest_free(&m);
    text_free(&manifest_path);
    return ok;
}

/* The targets of this build: `--target`, or the lists of `[targets]`, or
   the host. */
static bool build_targets(const struct build *b, enum target *list,
                          size_t *count)
{
    const struct text *names = NULL;
    size_t named = 0;
    size_t i;

    *count = 0;
    if (b->r->target != NULL && strcmp(b->r->target, "all") != 0) {
        if (!target_from_name(b->r->target, &list[0])) {
            fprintf(stderr, "anti: %s is no target of Anti\n", b->r->target);
            return false;
        }
        *count = 1;
        return true;
    }
    if (b->r->target != NULL) {
        names = b->m.all_targets;
        named = b->m.all_target_count;
        if (named == 0) {
            for (i = 0; i < TARGET_COUNT; i++) {
                list[i] = (enum target)i;
            }
            *count = TARGET_COUNT;
            return true;
        }
    } else {
        names = b->m.default_targets;
        named = b->m.default_target_count;
        if (named == 0) {
            if (!target_host(&list[0])) {
                fputs("anti: this host is no target of Anti, so the build "
                      "needs --target\n", stderr);
                return false;
            }
            *count = 1;
            return true;
        }
    }
    if (named > TARGET_COUNT) {
        fprintf(stderr, "anti: %s: [targets] names more than the six targets "
                        "of Anti\n", MANIFEST_FILE);
        return false;
    }
    for (i = 0; i < named; i++) {
        if (!target_from_name(text_cstr(&names[i]), &list[i])) {
            fprintf(stderr, "anti: %s: [targets] names %s, which is no target "
                            "of Anti\n", MANIFEST_FILE, text_cstr(&names[i]));
            return false;
        }
    }
    *count = named;
    return true;
}

/* Read the modules of the project under its source directory. */
static bool read_units(struct build *b)
{
    size_t i;

    b->roots[0] = text_cstr(&b->src);
    if (!files_list_tree(text_cstr(&b->src), SOURCE_SUFFIX, &b->sources)) {
        return false;
    }
    if (b->sources.count == 0) {
        fprintf(stderr, "anti: %s holds no %s file, so this project has no "
                        "module\n", text_cstr(&b->src), SOURCE_SUFFIX);
        return false;
    }
    b->units = files_array(b->sources.count, sizeof *b->units);
    b->order = files_array(b->sources.count, sizeof *b->order);
    b->unit_count = b->sources.count;
    for (i = 0; i < b->sources.count; i++) {
        if (!unit_read(text_cstr(&b->sources.items[i]), b->roots, 1, NULL,
                       &b->units[i]) ||
            !b->units[i].parsed) {
            return false;
        }
    }
    return unit_order(b->units, b->unit_count, b->order);
}

static void build_free(struct build *b)
{
    size_t i;

    for (i = 0; i < b->unit_count; i++) {
        unit_free(&b->units[i]);
    }
    free(b->units);
    files_list_free(&b->sources);
    free(b->order);
    for (i = 0; i < b->spec_count; i++) {
        text_free(&b->specs[i]);
    }
    free(b->specs);
    strings_free(&b->spec_list);
    arena_free(&b->framework_arena);
    strings_free(&b->attribution);
    deps_free(&b->graph);
    manifest_free(&b->m);
    text_free(&b->src);
    text_free(&b->runtime);
    text_free(&b->license);
    text_free(&b->build_dir);
    text_free(&b->dist_dir);
    text_free(&b->lib_dir);
    text_free(&b->obj_dir);
    text_free(&b->name);
}

/* `anti run`: run the program the build of b wrote for the host, and
   give its status. */
static int run_built(const struct build *b)
{
    const struct build_request *r = b->r;
    struct text program = {0};
    enum target host;
    const char *argv[2];
    int status;

    if (!target_host(&host)) {
        fputs("anti: this host is no target of Anti\n", stderr);
        return 1;
    }
    text_appendf(&program, "%s/%s/%s/%s/%s%s", r->root,
                 text_cstr(&b->m.dist), target_name(host),
                 r->release ? "release" : "dev",
                 modpath_last(text_cstr(&b->m.name)),
                 target_info(host)->executable_suffix);
    if (!files_exists(text_cstr(&program))) {
        fprintf(stderr, "anti: %s was not built, so there is nothing "
                        "to run\n", text_cstr(&program));
        status = 1;
    } else {
        argv[0] = text_cstr(&program);
        argv[1] = NULL;
        status = r->memory_checks ? memreport_run(argv) : process_run(argv);
    }
    text_free(&program);
    return status;
}

static int build_project(const struct build_request *r,
                         const struct path_context *outer)
{
    struct build b;
    struct path_context context;
    struct text manifest_path = {0};
    enum target targets[TARGET_COUNT];
    enum cpu_level cpus[TARGET_COUNT];
    size_t target_count = 0;
    size_t i;
    int status = 1;

    memset(&b, 0, sizeof b);
    memset(&context, 0, sizeof context);
    b.r = r;
    b.depth = outer != NULL ? outer->depth + 1 : 0;
    text_appendf(&manifest_path, "%s/%s", r->root, MANIFEST_FILE);
    if (!manifest_read(text_cstr(&manifest_path), false, &b.m)) {
        goto done;
    }
    /* `antic` of the manifest is the minimum compiler version. A version
       of another form satisfies nothing, as one of an index does, and
       the comparison read `v0.3` as 0.0.0, which every compiler passed. */
    if (b.m.antic.length > 0 && !repo_version_valid(text_cstr(&b.m.antic))) {
        fprintf(stderr, "anti: %s: [package] antic `%s` is no version of one "
                        "to three parts of digits, as `%s`\n",
                text_cstr(&manifest_path), text_cstr(&b.m.antic),
                ANTIC_VERSION);
        goto done;
    }
    if (b.m.antic.length > 0 &&
        deps_version_compare(ANTIC_VERSION, text_cstr(&b.m.antic)) < 0) {
        fprintf(stderr, "anti: %s asks for antic %s and this one is %s\n",
                text_cstr(&manifest_path), text_cstr(&b.m.antic),
                ANTIC_VERSION);
        goto done;
    }
    if (r->runtime != NULL) {
        text_append(&b.runtime, r->runtime);
    } else if (!runtime_archive(&b.runtime)) {
        fputs("anti: the system does not say where the runtime archive is, so "
              "the build needs --runtime\n", stderr);
        goto done;
    }
    if (b.m.license_text.length > 0) {
        text_appendf(&b.license, "%s/%s", r->root,
                     text_cstr(&b.m.license_text));
    }
    if (!build_targets(&b, targets, &target_count)) {
        goto done;
    }
    /* R158 of "Rules of 2026-10-13": every target takes the level before
       the dependencies resolve and any target builds, so a refusal leaves
       no part of the build. */
    for (i = 0; i < target_count; i++) {
        cpus[i] = cpu_default(targets[i]);
        if (r->cpu != NULL && !cpu_from_name(r->cpu, targets[i], &cpus[i])) {
            fprintf(stderr, "anti: %s is no processor level of %s\n", r->cpu,
                    target_name(targets[i]));
            goto done;
        }
    }
    context.r = r;
    context.outer = outer;
    context.name = text_cstr(&b.m.name);
    context.depth = b.depth;
    if (!absolute_path(r->root, &context.directory)) {
        text_append(&context.directory, r->root);
    }
    if (!deps_resolve(&b.m, r->root, r->offline, path_dependency, &context,
                      &b.graph)) {
        goto done;
    }
    b.specs = files_array(b.graph.count + 1, sizeof *b.specs);
    for (i = 0; i < b.graph.count; i++) {
        const struct dep_package *p = &b.graph.packages[i];
        text_appendf(&b.specs[i], "%s,%s,%s", text_cstr(&p->name),
                     text_cstr(&p->version), text_cstr(&p->repo));
        strings_add(&b.spec_list, text_cstr(&b.specs[i]));
    }
    b.spec_count = b.graph.count;
    for (i = 0; i < b.m.attribution_count; i++) {
        strings_add(&b.attribution, text_cstr(&b.m.attribution[i]));
    }
    text_appendf(&b.src, "%s/%s", r->root, text_cstr(&b.m.src));
    if (!read_units(&b)) {
        goto done;
    }
    /* R168 of "Rules of 2026-10-13": a library project names its root
       module by its package name, and one without it builds from the
       last module of the order, which a user may not expect. */
    if (linking_module(&b) == b.unit_count &&
        !has_module(&b, text_cstr(&b.m.name))) {
        fprintf(stderr, "anti: %s: warning: the library project %s has no "
                        "module %s\n", text_cstr(&manifest_path),
                text_cstr(&b.m.name), text_cstr(&b.m.name));
    }
    /* R160 of "Rules of 2026-10-13": run of a library project is a wrong
       use of the command, refused before anything builds. */
    if (r->run && linking_module(&b) == b.unit_count) {
        fprintf(stderr, "anti: %s is a library project, which has no `main` "
                        "to run\n", text_cstr(&b.m.name));
        status = 2;
        goto done;
    }
    for (i = 0; i < target_count; i++) {
        if (!build_target(&b, targets[i], cpus[i])) {
            goto done;
        }
    }
    status = r->run ? run_built(&b) : 0;
done:
    text_free(&context.directory);
    build_free(&b);
    text_free(&manifest_path);
    return status;
}

int build_run(const struct build_request *r)
{
    return build_project(r, NULL);
}

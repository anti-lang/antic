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
#include "cache.h"
#include "cpu.h"
#include "deps.h"
#include "digest.h"
#include "driver.h"
#include "files.h"
#include "licensing.h"
#include "manifest.h"
#include "memreport.h"
#include "modpath.h"
#include "platform.h"
#include "say.h"
#include "syms.h"
#include "sha256.h"
#include "symmap.h"
#include "target.h"
#include "text.h"
#include "units.h"
#include "userdirs.h"
#include "zip.h"

/* What the link of a program took beside its inputs, which the notice
   names, stands beside a cached program under its name and this suffix. */
#define BUILD_LINKS_SUFFIX ".links"

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
    /* The file name of the program without the suffix of its target, and
       empty in a library project. */
    struct text program;
    /* The frameworks and the Linux libraries of every library file the
       program reaches, in framework_arena. */
    struct arena framework_arena;
    struct unit_links links;
    /* Whether the link of the program of this target took the C library
       of musl, which the driver writes through links_musl of its
       options. */
    bool links_musl;
    /* The native libraries of the runtime archive that the same link
       took, DRIVER_NATIVE_* bits, which it writes through links_natives. */
    unsigned natives;
    /* The key of the program of this target, and whether the program of
       an earlier build had it, so this build links nothing. */
    struct text program_key;
    bool program_reused;
};

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
            say_anti("cannot read %s\n", o->license_text);
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

   DESIGN: decision 15 of "Decisions of 2026-10-11", which cache_imports
   holds. The search finds a module of the project under lib/ of the
   build and one of the standard library in the runtime archive. A
   library file of the graph stands under no root its module path names,
   so every one of them is in the key. A new version of a dependency then
   builds every module again, which the lock file makes rare. */
static bool imports_key(const struct build *b, enum target t,
                        enum cpu_level cpu, const char *library,
                        struct text *out)
{
    struct options search;
    struct strings graph = {0};
    const char *roots[1];
    bool ok;

    graph_libraries(b, &graph);
    roots[0] = text_cstr(&b->lib_dir);
    unit_options(&search, text_cstr(&b->m.name), text_cstr(&b->runtime), roots,
                 1, t, cpu);
    search.libraries = graph.items;
    search.library_count = graph.count;
    ok = cache_imports(&search, library, out);
    strings_free(&graph);
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
        cache_holds(text_cstr(output), &key)) {
        ok = true;
        goto done;
    }
    cache_drop(text_cstr(output));
    if (driver_run(&o) != 0) {
        goto done;
    }
    key.length = 0;
    text_append(&key, text_cstr(&base));
    ok = imports_key(b, t, cpu, text_cstr(output), &key) &&
         cache_store(text_cstr(output), &key);
done:
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
    bool ok = unit_library_header(file, &arena, &package, &module);

    if (ok) {
        text_append(out, module);
    }
    arena_free(&arena);
    return ok;
}

/* What the dev object of one library file is compiled with: the options
   of a project compile, the library files every such compile names and
   the roots of the walk of its key. A build fills it from itself, and a
   test run from what the dev build of its project recorded, so both
   write and reuse the same object under the same key. */
struct object_context {
    const char *package;
    const char *version;
    const char *runtime;
    const char *src;
    const char *lib_dir;
    const char *obj_dir;
    const char *llvm_ar;
    enum target target;
    enum cpu_level cpu;
    bool debug;
    bool checked;
    const char *const *inject;
    size_t inject_count;
    const char *const *graph;       /* the library files of the graph */
    size_t graph_count;
    const char *const *libraries;   /* the library files of every compile */
    size_t library_count;
};

/* Write the object of one library file into obj/ of c, unless the cache
   holds it, and its path into out. A module without `main` stops at its
   object, which antic reports as status 3. */
static bool object_compile(const struct object_context *c, const char *library,
                           struct text *out)
{
    struct options o;
    struct options search;
    struct cache_flags flags = {0};
    struct strings rest = {0};
    struct text module = {0};
    struct text base = {0};
    struct text key = {0};
    size_t i;
    int status;
    bool ok = false;

    if (!library_module(library, &module) ||
        !unit_file(c->obj_dir, text_cstr(&module), "", &base)) {
        goto done;
    }
    text_appendf(out, "%s%s", text_cstr(&base),
                 target_info(c->target)->object_suffix);
    /* The input is one of the library files of the build, and antic
       refuses a module that stands twice on its command line. */
    for (i = 0; i < c->library_count; i++) {
        if (strcmp(c->libraries[i], library) != 0) {
            strings_add(&rest, c->libraries[i]);
        }
    }
    unit_options(&o, c->package, c->runtime, &c->src, 1, c->target, c->cpu);
    o.llvm_ar = c->llvm_ar;
    o.inject = c->inject;
    o.inject_count = c->inject_count;
    o.package_version = c->version;
    o.debug = c->debug;
    o.memory_checks = c->checked;
    o.input = library;
    o.output = text_cstr(&base);
    o.dev = true;
    o.libraries = rest.items;
    o.library_count = rest.count;
    flags.target = c->target;
    flags.cpu = c->cpu;
    flags.debug = c->debug;
    flags.checked = c->checked;
    cache_key(library, &flags, &key);
    unit_options(&search, c->package, c->runtime, &c->lib_dir, 1, c->target,
                 c->cpu);
    search.libraries = c->graph;
    search.library_count = c->graph_count;
    if (!options_key(&o, &key) || !cache_imports(&search, library, &key)) {
        goto done;
    }
    if (cache_holds(text_cstr(out), &key)) {
        ok = true;
        goto done;
    }
    cache_drop(text_cstr(out));
    status = driver_run(&o);
    ok = (status == 0 || status == DRIVER_OBJECT_ONLY) &&
         cache_store(text_cstr(out), &key);
done:
    strings_free(&rest);
    text_free(&module);
    text_free(&base);
    text_free(&key);
    return ok;
}

/* Write the object of one library file of the build b, unless the cache
   holds it. libraries are the library files every compile names, and
   graph those of the dependency graph. */
static bool module_object(struct build *b, const char *library, enum target t,
                          enum cpu_level cpu, const struct strings *libraries,
                          const struct strings *graph, struct text *out)
{
    struct object_context c;

    memset(&c, 0, sizeof c);
    c.package = text_cstr(&b->m.name);
    c.version = b->m.version.length > 0 ? text_cstr(&b->m.version) : NULL;
    c.runtime = text_cstr(&b->runtime);
    c.src = b->roots[0];
    c.lib_dir = text_cstr(&b->lib_dir);
    c.obj_dir = text_cstr(&b->obj_dir);
    c.llvm_ar = b->r->llvm_ar;
    c.target = t;
    c.cpu = cpu;
    c.debug = build_debug(b);
    c.checked = b->r->memory_checks;
    c.inject = b->m.inject.entries;
    c.inject_count = b->m.inject.count;
    c.graph = graph->items;
    c.graph_count = graph->count;
    c.libraries = libraries->items;
    c.library_count = libraries->count;
    return object_compile(&c, library, out);
}

/* Copy the file from into the directory to, under the name it carries. */
static bool copy_into(const char *from, const char *to, const char *name)
{
    struct text path = {0};
    bool ok;

    text_appendf(&path, "%s/%s", to, name);
    ok = files_make_dirs(to) && files_copy_program(from, text_cstr(&path));
    if (!ok) {
        say_anti("cannot write %s\n", text_cstr(&path));
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
        say_anti("cannot write %s\n", text_cstr(&link));
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
   the link of the program took them, the native libraries of the runtime
   archive that the link took, by Q2 of "Answers of 2026-11-01" in
   docs/decisions.md, every package of anti.lock, the
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
    project.natives = b->natives;
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

/* [provisional] DESIGN: a project whose manifest names no package is
   the package of its root module: the module that holds main, or the
   root module of a library project, which is the last of the order. Its
   path is the package name that every compile carries, so the modules of
   the project share their internal items. Q3 and Q6 of "Answers of
   2026-11-01" in docs/decisions.md make [package] and its `name`
   optional. The program takes its name from name_program. */
static void name_project(struct build *b)
{
    size_t at = linking_module(b);

    if (at == b->unit_count) {
        at = root_module(b);
    }
    text_append(&b->m.name, text_cstr(&b->units[at].path));
}

/* DESIGN: Q6 of "Answers of 2026-11-01" in docs/decisions.md names the
   program. `program` of [package] names it, a `name` of the manifest
   gives its last segment, and without either the program takes the name
   of the project directory, the one that holds anti.toml. unnamed says
   the manifest named no package.
   [provisional] A library project has no program, so one that names
   `program` is refused. A directory name that is no file name on every
   target, a blank or a name of a device of Windows, is refused and asks
   for `program`. On POSIX the directory is the one realpath gives, after
   its links. */
static bool name_program(struct build *b, const char *manifest_path,
                         bool unnamed)
{
    const char *program = text_cstr(&b->m.program);
    struct text directory = {0};
    const char *last;
    bool ok = true;

    if (linking_module(b) == b->unit_count) {
        if (b->m.program.length > 0) {
            say_anti("%s: [package] names the program `%s`, and the "
                     "project holds no `main`\n", manifest_path, program);
            return false;
        }
        return true;
    }
    if (b->m.program.length > 0) {
        text_append(&b->program, program);
        return true;
    }
    if (!unnamed) {
        text_append(&b->program, modpath_last(text_cstr(&b->m.name)));
        return true;
    }
    if (!platform_absolute_path(b->r->root, &directory)) {
        text_append(&directory, b->r->root);
    }
    while (directory.length > 1 &&
           directory.data[directory.length - 1] == '/') {
        directory.length--;
        directory.data[directory.length] = '\0';
    }
    last = strrchr(text_cstr(&directory), '/');
    last = last != NULL ? last + 1 : text_cstr(&directory);
    if (manifest_file_name_valid(last)) {
        text_append(&b->program, last);
    } else {
        say_anti("the project directory `%s` is no file name on every "
                 "target, and `program` of [package] names the program\n",
                 last);
        ok = false;
    }
    text_free(&directory);
    return ok;
}

/* The facts of the link of the program of b that the notice reads,
   beside the cached program. */
static void links_path(const struct build *b, struct text *out)
{
    text_appendf(out, "%s%s", text_cstr(&b->name), BUILD_LINKS_SUFFIX);
}

/* Read what the link of the cached program took into b. Returns false
   when the file is missing or malformed, and the program is then linked
   again. */
static bool links_read(struct build *b)
{
    struct text path = {0};
    struct text text = {0};
    unsigned musl = 0;
    unsigned natives = 0;
    bool ok;

    links_path(b, &path);
    ok = files_read(text_cstr(&path), &text) &&
         sscanf(text_cstr(&text), "musl %u natives %u", &musl, &natives) == 2;
    if (ok) {
        b->links_musl = musl != 0;
        b->natives = natives;
    }
    text_free(&path);
    text_free(&text);
    return ok;
}

/* Whether the file of the symbols of a release program stands, at the
   path that build_symbols writes it to. */
static bool symbols_stand(const struct build *b, enum target t);

static void program_name(const struct build *b, enum target t,
                         struct text *out);

/* Append to out the path of the twin of the release program of b on t
   in the build directory, <stem>.debug, with .exe on Windows. */
static void debug_twin(const struct build *b, enum target t,
                       struct text *out);

/* Append to out the path of the DWARF file of the dSYM of the twin of the
   release program of b on t, and give true, when the program is a
   stripped Mach-O one. Give false with out untouched otherwise. */
static bool dsym_dwarf(const struct build *b, enum target t,
                       struct text *out);

/* DESIGN: Q2 of "Answers of 2026-11-06, fourth set" in docs/decisions.md.
   The key of a program holds what its link reads: the key of the source
   of the module with `main`, the options of the manifest, the digest of
   every library file that module reaches, the key of every object of a
   dev link, the digest of every archive of the runtime archive the link
   may take and the options of the link. When the program that stands
   has the same key, and the facts of its link and, in release mode, its
   symbols stand beside it, the build links nothing. Sets b->program_key and
   b->program_reused. library is the library file of the module with
   `main`. Returns false when an input of the key cannot be read. */
static bool program_reuse(struct build *b, enum target t, enum cpu_level cpu,
                          const struct options *o, const char *library)
{
    struct cache_flags flags = {0};
    size_t i;

    b->program_key.length = 0;
    b->program_reused = false;
    flags.target = t;
    flags.cpu = cpu;
    flags.debug = o->debug;
    flags.checked = o->memory_checks;
    cache_key(o->input, &flags, &b->program_key);
    text_appendf(&b->program_key, "program %s\n",
                 b->r->release ? "release" : "dev");
    if (!options_key(o, &b->program_key) ||
        !imports_key(b, t, cpu, library, &b->program_key)) {
        return false;
    }
    for (i = 0; i < o->object_count; i++) {
        if (!cache_key_of(o->objects[i], &b->program_key)) {
            return false;
        }
    }
    if (!cache_runtime(o->runtime, t, cpu, &b->program_key)) {
        return false;
    }
    for (i = 0; i < o->framework_count; i++) {
        text_appendf(&b->program_key, "framework %s\n", o->frameworks[i]);
    }
    for (i = 0; i < o->linux_library_count; i++) {
        text_appendf(&b->program_key, "linux %s\n", o->linux_libraries[i]);
    }
    if (o->lto_given) {
        text_appendf(&b->program_key, "lto %s\n", link_lto_name(o->lto));
    }
    if (o->profile_generate) {
        text_append(&b->program_key, "profile-generate\n");
    }
    if (o->profile_use != NULL &&
        !cache_file("profile-use", o->profile_use, &b->program_key)) {
        return false;
    }
    b->program_reused = cache_holds(text_cstr(&b->name), &b->program_key) &&
                        links_read(b) &&
                        (!b->r->release || symbols_stand(b, t));
    if (!b->program_reused) {
        cache_drop(text_cstr(&b->name));
    }
    return true;
}

/* Write the facts of the link and the key of the program b linked, once
   everything the key stands for is written. */
static bool program_store(const struct build *b)
{
    struct text path = {0};
    struct text text = {0};
    bool ok;

    links_path(b, &path);
    text_appendf(&text, "musl %u natives %u\n", b->links_musl ? 1u : 0u,
                 b->natives);
    ok = files_write(text_cstr(&path), &text) &&
         cache_store(text_cstr(&b->name), &b->program_key);
    text_free(&path);
    text_free(&text);
    return ok;
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
    struct strings graph = {0};
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
    graph_libraries(b, &graph);
    paths = files_array(count + 1, sizeof *paths);
    for (i = 0; i < count; i++) {
        if (strcmp(closure[i], text_cstr(&files[main_at])) == 0) {
            continue;
        }
        if (!module_object(b, closure[i], t, cpu, libraries, &graph,
                           &paths[at])) {
            goto done;
        }
        strings_add(&objects, text_cstr(&paths[at]));
        at++;
    }
    /* DESIGN: antic stops a dev build of a library file at its object,
       so the module that links is compiled from its source, together
       with the link. By Q2 of "Answers of 2026-11-06, fourth set" the
       build links nothing when the key of the program is the one of the
       program that stands, which names the key of every object. */
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
    o.links_natives = &b->natives;
    if (!program_reuse(b, t, cpu, &o, text_cstr(&files[main_at]))) {
        goto done;
    }
    ok = b->program_reused ||
         (driver_run(&o) == 0 && program_store(b));
done:
    for (i = 0; paths != NULL && i < count; i++) {
        text_free(&paths[i]);
    }
    free(paths);
    strings_free(&seed);
    strings_free(&objects);
    strings_free(&graph);
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
   whole program.
   DESIGN: the call links with -g, and that one link is both the twin of
   the symbols archive and, with its debug information stripped, the
   program, by Q1 of "Answers of 2026-11-07, fourth set". The program and
   its archive then hold the same code at every address, which T211 of
   docs/audit/tests-findings.md showed a second link with -g does not
   give. driver_strip_debug says what each format strips. */
static bool build_release(struct build *b, enum target t, enum cpu_level cpu,
                          size_t main_at, const struct strings *libraries,
                          const char *library)
{
    struct options o;
    struct text twin = {0};
    bool lines;
    bool ok;

    base_options(b, &o, t, cpu);
    /* A program of --memory-checks keeps its lines for its reports, by
       the rule of base_options, so nothing strips it. */
    lines = o.debug;
    header_options(b, &o);
    o.input = b->units[main_at].source;
    o.output = text_cstr(&b->name);
    o.libraries = libraries->items;
    o.library_count = libraries->count;
    o.debug = true;
    /* The program that is stripped names no directory of this build, by
       Q2 of "Answers of 2026-11-07, sixth set". */
    if (!lines) {
        o.debug_map_root = text_cstr(&b->build_dir);
    }
    o.profile_generate = b->r->profile_generate;
    o.profile_use = b->r->profile_use;
    release_lto(b, &o);
    o.links_musl = &b->links_musl;
    o.links_natives = &b->natives;
    /* The key is stored after the symbols archive, which the link
       writes. */
    if (!program_reuse(b, t, cpu, &o, library)) {
        return false;
    }
    if (b->program_reused) {
        return true;
    }
    debug_twin(b, t, &twin);
    ok = driver_run(&o) == 0;
    if (ok && !files_copy_program(text_cstr(&b->name), text_cstr(&twin))) {
        say_anti("cannot write %s\n", text_cstr(&twin));
        ok = false;
    }
    ok = ok && (lines || driver_strip_debug(&o, text_cstr(&twin),
                                            text_cstr(&b->name)));
    text_free(&twin);
    return ok;
}

/* The name of the deliverable without the suffix of an executable, which
   the entries of a symbols archive carry. */
static void archive_stem(enum target t, const char *name, struct text *out)
{
    text_append(out, name);
    files_cut_suffix(out, target_info(t)->executable_suffix);
}

static void debug_twin(const struct build *b, enum target t,
                       struct text *out)
{
    struct text deliverable = {0};
    struct text stem = {0};

    program_name(b, t, &deliverable);
    archive_stem(t, text_cstr(&deliverable), &stem);
    text_appendf(out, "%s/%s" SYMS_DEBUG_SUFFIX "%s", text_cstr(&b->build_dir),
                 text_cstr(&stem),
                 target_info(t)->format == FORMAT_COFF ? ".exe" : "");
    text_free(&deliverable);
    text_free(&stem);
}

static bool dsym_dwarf(const struct build *b, enum target t,
                       struct text *out)
{
    struct text twin = {0};

    if (target_info(t)->format != FORMAT_MACHO || build_debug(b)) {
        return false;
    }
    debug_twin(b, t, &twin);
    driver_dsym_dwarf(text_cstr(&twin), out);
    text_free(&twin);
    return true;
}

/* DESIGN: a release binary carries no symbol data, so the build writes
   the archive that names it. It holds the same link with the debug
   sections kept and the map of that program. All three carry the build
   id of the binary, because the digest leaves what `-g` added out. That
   id is what ties a frame of a trace to this archive. A Windows link
   writes the symbols to a PDB. The archive holds the PDB of the release
   link, whose GUID the shipped program carries. Eddie decided this. By
   Q1 of "Answers of 2026-11-07, fourth set" the link with its debug
   sections kept is the link of the program itself, which build_release
   keeps as <stem>.debug before it strips the program. A Windows program
   is that link as it stands, so its twin is a copy of it, named
   <stem>.debug.exe. */
static bool build_symbols(struct build *b, enum target t,
                          const char *deliverable)
{
    struct zip_entry entries[4];
    struct text stem = {0};
    struct text debug_path = {0};
    struct text dwarf_path = {0};
    struct text dwarf_name = {0};
    bool dwarf;
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
    text_appendf(&debug_name, "%s" SYMS_DEBUG_SUFFIX, text_cstr(&stem));
    text_appendf(&map_name, "%s" SYMS_MAP_SUFFIX, text_cstr(&stem));
    debug_twin(b, t, &debug_path);
    dwarf = dsym_dwarf(b, t, &dwarf_path);
    text_appendf(&map_path, "%s/%s", text_cstr(&b->build_dir),
                 text_cstr(&map_name));
    text_appendf(&archive, "%s/%s%s", text_cstr(&b->dist_dir),
                 text_cstr(&stem), SYMS_ARCHIVE_SUFFIX);
    /* A program of an earlier build has its symbols beside it, which
       symbols_stand checked. */
    if (!b->program_reused &&
        (!symmap_build_id(text_cstr(&b->name), &id) ||
         !symmap_write(text_cstr(&debug_path),
                       dwarf ? text_cstr(&dwarf_path) : NULL, t,
                       text_cstr(&id), text_cstr(&map_path)))) {
        goto done;
    }
    entries[count].name = text_cstr(&debug_name);
    entries[count].file = text_cstr(&debug_path);
    entries[count].executable = true;
    count++;
    if (target_info(t)->format == FORMAT_COFF) {
        /* The PDB of the release link, beside the program in the build
           directory. Every Windows link writes one. */
        text_appendf(&pdb_name, "%s" SYMS_PDB_SUFFIX, text_cstr(&stem));
        link_pdb_path(&pdb_path, text_cstr(&b->name));
        if (!files_exists(text_cstr(&pdb_path))) {
            say_anti("the link of %s wrote no %s\n",
                     text_cstr(&b->name), text_cstr(&pdb_path));
            goto done;
        }
        entries[count].name = text_cstr(&pdb_name);
        entries[count].file = text_cstr(&pdb_path);
        entries[count].executable = false;
        count++;
    }
    /* The DWARF of the dSYM of a macOS program, by Q1 of "Answers of
       2026-11-07, sixth set", so a frame resolves without the LTO object
       that the debug map names. */
    if (dwarf) {
        text_appendf(&dwarf_name, "%s" SYMS_DWARF_SUFFIX, text_cstr(&stem));
        entries[count].name = text_cstr(&dwarf_name);
        entries[count].file = text_cstr(&dwarf_path);
        entries[count].executable = false;
        count++;
    }
    entries[count].name = text_cstr(&map_name);
    entries[count].file = text_cstr(&map_path);
    entries[count].executable = false;
    count++;
    ok = zip_write(text_cstr(&archive), entries, count) &&
         (b->program_reused || program_store(b));
done:
    text_free(&stem);
    text_free(&debug_path);
    text_free(&dwarf_path);
    text_free(&dwarf_name);
    text_free(&debug_name);
    text_free(&map_path);
    text_free(&map_name);
    text_free(&pdb_path);
    text_free(&pdb_name);
    text_free(&archive);
    text_free(&id);
    return ok;
}

static bool symbols_stand(const struct build *b, enum target t)
{
    struct text deliverable = {0};
    struct text stem = {0};
    struct text path = {0};
    bool ok;

    program_name(b, t, &deliverable);
    archive_stem(t, text_cstr(&deliverable), &stem);
    debug_twin(b, t, &path);
    ok = files_exists(text_cstr(&path));
    path.length = 0;
    text_appendf(&path, "%s/%s" SYMS_MAP_SUFFIX, text_cstr(&b->build_dir),
                 text_cstr(&stem));
    ok = ok && files_exists(text_cstr(&path));
    path.length = 0;
    if (ok && dsym_dwarf(b, t, &path)) {
        ok = files_exists(text_cstr(&path));
    }
    text_free(&deliverable);
    text_free(&stem);
    text_free(&path);
    return ok;
}

/* Append <root>/<dir>/<target>/<mode> to out: the directory of
   docs/tooling.md that dir, the build or the dist directory of the
   manifest, holds for one target in the dev or the release mode. */
static void target_dir(const char *root, const struct text *dir,
                       enum target t, bool release, struct text *out)
{
    text_appendf(out, "%s/%s/%s/%s", root, text_cstr(dir), target_name(t),
                 release ? "release" : "dev");
}

/* The file name of the program of the build b on a target. */
static void program_name(const struct build *b, enum target t,
                         struct text *out)
{
    text_appendf(out, "%s%s", text_cstr(&b->program),
                 target_info(t)->executable_suffix);
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
            say_anti("cannot write %s\n", text_cstr(&out));
        }
        text_free(&out);
        if (!written) {
            return false;
        }
    }
    return true;
}

/* Copy the count strings of items into a new array of texts. */
static struct text *texts_of(const char *const *items, size_t count)
{
    struct text *out = files_array(count + 1, sizeof *out);
    size_t i;

    for (i = 0; i < count; i++) {
        text_append(&out[i], items[i]);
    }
    return out;
}

static void texts_free(struct text *items, size_t count)
{
    size_t i;

    for (i = 0; items != NULL && i < count; i++) {
        text_free(&items[i]);
    }
    free(items);
}

/* Write what a test run takes from this build of target t at level cpu
   into its request: the package name, the source directory, the library
   directory and the library files of the dependency graph in shared, and
   what build_object compiles with. files[i] is the library file of unit
   i, and every object compile names all of them but the one of the
   module with `main`, as build_dev names them. */
static void record_tested(const struct build *b, enum target t,
                          enum cpu_level cpu, const struct strings *shared,
                          const struct text *files)
{
    struct build_tested *tested = b->r->tested;
    struct strings compiled = {0};
    size_t main_at = linking_module(b);
    size_t i;

    build_tested_free(tested);
    text_append(&tested->package, text_cstr(&b->m.name));
    text_append(&tested->version, text_cstr(&b->m.version));
    text_append(&tested->src, text_cstr(&b->src));
    text_append(&tested->lib_dir, text_cstr(&b->lib_dir));
    text_append(&tested->obj_dir, text_cstr(&b->obj_dir));
    tested->libraries = texts_of(shared->items, shared->count);
    tested->library_count = shared->count;
    text_append(&tested->runtime, text_cstr(&b->runtime));
    tested->target = t;
    tested->cpu = cpu;
    tested->debug = build_debug(b);
    tested->memory_checks = b->r->memory_checks;
    tested->inject = texts_of(b->m.inject.entries, b->m.inject.count);
    tested->inject_count = b->m.inject.count;
    for (i = 0; i < shared->count; i++) {
        strings_add(&compiled, shared->items[i]);
    }
    for (i = 0; i < b->unit_count; i++) {
        if (b->order[i] != main_at) {
            strings_add(&compiled, text_cstr(&files[b->order[i]]));
        }
    }
    tested->compiled = texts_of(compiled.items, compiled.count);
    tested->compiled_count = compiled.count;
    strings_free(&compiled);
}

void build_tested_free(struct build_tested *t)
{
    text_free(&t->package);
    text_free(&t->version);
    text_free(&t->src);
    text_free(&t->lib_dir);
    text_free(&t->obj_dir);
    texts_free(t->libraries, t->library_count);
    text_free(&t->runtime);
    texts_free(t->inject, t->inject_count);
    texts_free(t->compiled, t->compiled_count);
    t->libraries = NULL;
    t->library_count = 0;
    t->inject = NULL;
    t->inject_count = 0;
    t->compiled = NULL;
    t->compiled_count = 0;
}

/* The C strings of count texts, in an array the caller frees. */
static const char **cstrs_of(const struct text *items, size_t count)
{
    const char **out = files_array(count + 1, sizeof *out);
    size_t i;

    for (i = 0; i < count; i++) {
        out[i] = text_cstr(&items[i]);
    }
    return out;
}

bool build_object(const struct build_tested *t, const char *library,
                  struct text *out)
{
    struct object_context c;
    const char *src = text_cstr(&t->src);
    const char **graph = cstrs_of(t->libraries, t->library_count);
    const char **inject = cstrs_of(t->inject, t->inject_count);
    const char **compiled = cstrs_of(t->compiled, t->compiled_count);
    bool ok;

    memset(&c, 0, sizeof c);
    c.package = text_cstr(&t->package);
    c.version = t->version.length > 0 ? text_cstr(&t->version) : NULL;
    c.runtime = text_cstr(&t->runtime);
    c.src = src;
    c.lib_dir = text_cstr(&t->lib_dir);
    c.obj_dir = text_cstr(&t->obj_dir);
    c.target = t->target;
    c.cpu = t->cpu;
    c.debug = t->debug;
    c.checked = t->memory_checks;
    c.inject = inject;
    c.inject_count = t->inject_count;
    c.graph = graph;
    c.graph_count = t->library_count;
    c.libraries = compiled;
    c.library_count = t->compiled_count;
    ok = object_compile(&c, library, out);
    free((void *)graph);
    free((void *)inject);
    free((void *)compiled);
    return ok;
}

/* Build one target in one mode. */
static bool build_target(struct build *b, enum target t, enum cpu_level cpu)
{
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
    b->natives = 0;
    b->program_reused = false;
    target_dir(b->r->root, &b->m.build, t, b->r->release, &b->build_dir);
    target_dir(b->r->root, &b->m.dist, t, b->r->release, &b->dist_dir);
    text_appendf(&b->lib_dir, "%s/lib", text_cstr(&b->build_dir));
    text_appendf(&b->obj_dir, "%s/obj", text_cstr(&b->build_dir));
    /* A test run delivers nothing, so it makes no directory in dist/. */
    if (!files_make_dirs_or_say(text_cstr(&b->build_dir)) ||
        (b->r->tested == NULL &&
         !files_make_dirs_or_say(text_cstr(&b->dist_dir)))) {
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
    if (b->r->tested != NULL) {
        record_tested(b, t, cpu, &shared, files);
        ok = true;
        goto done;
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
        program_name(b, t, &deliverable);
    }
    text_appendf(&b->name, "%s/%s", text_cstr(&b->build_dir),
                 text_cstr(&deliverable));
    if (!link_frameworks(b, t, cpu, files, &shared)) {
        goto done;
    }
    if (b->r->lib != BUILD_PROGRAM) {
        ok = build_c_library(b, t, cpu, &libraries);
    } else if (b->r->release) {
        ok = build_release(b, t, cpu, main_at, &libraries,
                           text_cstr(&files[main_at]));
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
        ok = build_symbols(b, t, text_cstr(&deliverable));
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

    if (!platform_absolute_path(directory, &resolved)) {
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
    say_anti("the path dependencies form a cycle: %s%s\n",
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

    /* A failed manifest_read leaves m empty, so done frees it on every
       path. */
    memset(&m, 0, sizeof m);
    text_appendf(&manifest_path, "%s/%s", directory, MANIFEST_FILE);
    if (!files_exists(text_cstr(&manifest_path))) {
        text_append(out, directory);
        ok = true;
        goto done;
    }
    if (path_cycle(c, directory) || !target_host(&host) ||
        !manifest_read(text_cstr(&manifest_path), false, &m)) {
        goto done;
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
    /* A path dependency is built whole, since its dist directory holds
       the library files that the test run reads. */
    sub.tested = NULL;
    if (build_project(&sub, c) == 0) {
        target_dir(directory, &m.dist, host, c->r->release, out);
        ok = true;
    }
done:
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
            say_anti("%s is no target of Anti\n", b->r->target);
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
                say_anti("this host is no target of Anti, so the build "
                         "needs --target\n");
                return false;
            }
            *count = 1;
            return true;
        }
    }
    if (named > TARGET_COUNT) {
        say_anti("%s: [targets] names more than the six targets "
                 "of Anti\n", MANIFEST_FILE);
        return false;
    }
    for (i = 0; i < named; i++) {
        if (!target_from_name(text_cstr(&names[i]), &list[i])) {
            say_anti("%s: [targets] names %s, which is no target "
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
        say_anti("%s holds no %s file, so this project has no "
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
    text_free(&b->program);
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
    text_free(&b->program_key);
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
        say_anti("this host is no target of Anti\n");
        return 1;
    }
    target_dir(r->root, &b->m.dist, host, r->release, &program);
    text_append(&program, "/");
    program_name(b, host, &program);
    if (!files_exists(text_cstr(&program))) {
        say_anti("%s was not built, so there is nothing "
                 "to run\n", text_cstr(&program));
        status = 1;
    } else {
        argv[0] = text_cstr(&program);
        argv[1] = NULL;
        status = r->memory_checks ? memreport_run(argv) : platform_run(argv);
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
    bool unnamed;
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
    if (b.m.antic.length > 0 && !manifest_version_valid(text_cstr(&b.m.antic))) {
        say_anti("%s: [package] antic `%s` is no version of one "
                 "to three parts of digits, as `%s`\n",
                 text_cstr(&manifest_path), text_cstr(&b.m.antic),
                 ANTIC_VERSION);
        goto done;
    }
    if (b.m.antic.length > 0 &&
        deps_version_compare(ANTIC_VERSION, text_cstr(&b.m.antic)) < 0) {
        say_anti("%s asks for antic %s and this one is %s\n",
                 text_cstr(&manifest_path), text_cstr(&b.m.antic),
                 ANTIC_VERSION);
        goto done;
    }
    if (r->runtime != NULL) {
        text_append(&b.runtime, r->runtime);
    } else if (!user_runtime_archive(&b.runtime)) {
        say_anti("the system does not say where the runtime archive is, so "
                 "the build needs --runtime\n");
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
            say_anti("%s is no processor level of %s\n", r->cpu,
                     target_name(targets[i]));
            goto done;
        }
    }
    context.r = r;
    context.outer = outer;
    context.name = text_cstr(&b.m.name);
    context.depth = b.depth;
    if (!platform_absolute_path(r->root, &context.directory)) {
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
    unnamed = b.m.name.length == 0;
    if (unnamed) {
        name_project(&b);
    }
    if (!name_program(&b, text_cstr(&manifest_path), unnamed)) {
        goto done;
    }
    /* R168 of "Rules of 2026-10-13": a library project names its root
       module by its package name, and one without it builds from the
       last module of the order, which a user may not expect. */
    if (linking_module(&b) == b.unit_count &&
        !has_module(&b, text_cstr(&b.m.name))) {
        say_anti("%s: warning: the library project %s has no "
                 "module %s\n", text_cstr(&manifest_path),
                 text_cstr(&b.m.name), text_cstr(&b.m.name));
    }
    /* R160 of "Rules of 2026-10-13": run of a library project is a wrong
       use of the command, refused before anything builds. */
    if (r->run && linking_module(&b) == b.unit_count) {
        say_anti("%s is a library project, which has no `main` "
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

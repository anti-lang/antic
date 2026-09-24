/* `anti build`, `anti run` and `anti new`.

   DESIGN: the tool reads the manifest, resolves the dependency graph and
   calls the compiler once per module in dev mode and once for the whole
   program in release mode. It calls driver_run in this process, as
   `anti test` and `anti check` do, rather than starting antic again.
   Everything the compiler writes lands under `build/`, and the tool
   copies the deliverable into `dist/`. The split of docs/tooling.md then
   holds: `build/` is disposable and `dist/` is what a user runs. */
#include "build.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "antl.h"
#include "arena.h"
#include "cpu.h"
#include "deps.h"
#include "driver.h"
#include "files.h"
#include "header.h"
#include "manifest.h"
#include "modpath.h"
#include "process.h"
#include "sha256.h"
#include "symmap.h"
#include "target.h"
#include "text.h"
#include "units.h"
#include "userdirs.h"
#include "zip.h"

/* A path dependency may be a project, whose build is a build of its own.
   The chain stops here, so a cycle of path dependencies reports rather
   than running out of stack. */
enum { BUILD_DEPTH = 8 };

/* The key of a cached output stands beside it, under its name and this
   suffix. */
#define BUILD_KEY_SUFFIX ".key"

static int build_project(const struct build_request *r, int depth);

static void die_out_of_memory(void)
{
    fputs("anti: out of memory\n", stderr);
    exit(70);
}

/* A growing list of strings that the option lists point into. */
struct strings {
    const char **items;
    size_t count;
    size_t capacity;
};

static void strings_add(struct strings *list, const char *item)
{
    if (list->count == list->capacity) {
        list->capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        list->items = realloc(list->items,
                              list->capacity * sizeof *list->items);
        if (list->items == NULL) {
            die_out_of_memory();
        }
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
    /* The frameworks of the `link framework` lines of every library file
       the program reaches, which the link passes as --framework. */
    struct arena framework_arena;
    const char **frameworks;
    size_t framework_count;
};

/* The cache key of one output: the digest of its input, the version of
   the compiler, the target and the processor level. The addendum names
   the first three. The level joins them because it decides the
   instructions, and the debug flag because it decides the lines. */
static void cache_key(const char *input, enum target t, enum cpu_level cpu,
                      bool debug, struct text *out)
{
    struct sha256 digest;
    struct text bytes = {0};
    char hex[65];

    sha256_init(&digest);
    if (files_read(input, &bytes)) {
        sha256_update(&digest, bytes.data, bytes.length);
    }
    sha256_hex(&digest, hex);
    text_appendf(out, "%s %s %s %s %s\n", hex, ANTIC_VERSION, target_name(t),
                 cpu_name(cpu), debug ? "g" : "no-g");
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
    remove(text_cstr(&path));
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

/* The options every call of one target shares. */
static void base_options(struct build *b, struct options *o,
                         enum target t, enum cpu_level cpu)
{
    memset(o, 0, sizeof *o);
    o->target = t;
    o->cpu = cpu;
    o->runtime = text_cstr(&b->runtime);
    o->llvm_mc = b->r->llvm_mc;
    o->llvm_ar = b->r->llvm_ar;
    o->inject = b->m.inject.entries;
    o->inject_count = b->m.inject.count;
    /* The link writes the version into the notice and into the
       descriptor of every class of the module it compiles. A library
       file of the project carries the same version. */
    if (b->m.version.length > 0) {
        o->package_version = text_cstr(&b->m.version);
    }
    o->roots = b->roots;
    o->root_count = 1;
    /* DESIGN: dev mode carries the line of every statement and release
       mode never does, which is the rule of docs/tooling.md. */
    o->debug = !b->r->release;
    o->frameworks = b->frameworks;
    o->framework_count = b->framework_count;
}

/* The package header that every library file of this project carries. */
static void header_options(const struct build *b, struct options *o)
{
    o->package_name = text_cstr(&b->m.name);
    if (b->m.version.length > 0) {
        o->package_version = text_cstr(&b->m.version);
    }
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

/* Write the library file of one module, unless the cache holds it. */
static bool module_library(struct build *b, const struct unit *u,
                           enum target t, enum cpu_level cpu,
                           const struct strings *libraries,
                           struct text *output)
{
    struct options o;
    struct text key = {0};
    bool ok = false;

    if (!unit_file(text_cstr(&b->lib_dir), text_cstr(&u->path), ANTL_SUFFIX,
                   output)) {
        goto done;
    }
    cache_key(u->source, t, cpu, false, &key);
    if (cached(text_cstr(output), &key)) {
        ok = true;
        goto done;
    }
    base_options(b, &o, t, cpu);
    header_options(b, &o);
    o.input = u->source;
    o.output = text_cstr(output);
    o.library = true;
    o.strip_docs = b->r->strip_docs;
    o.libraries = libraries->items;
    o.library_count = libraries->count;
    drop_key(text_cstr(output));
    ok = driver_run(&o) == 0 && write_key(text_cstr(output), &key);
done:
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
    cache_key(library, t, cpu, !b->r->release, &key);
    if (cached(text_cstr(out), &key)) {
        ok = true;
        goto done;
    }
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
    struct interface iface;
    struct text bytes = {0};
    char error[256];
    bool ok = false;

    if (!files_read(file, &bytes)) {
        fprintf(stderr, "anti: cannot read %s\n", file);
        goto done;
    }
    memset(&iface, 0, sizeof iface);
    if (!antl_header((const uint8_t *)bytes.data, bytes.length, &arena, &iface,
                     error, sizeof error)) {
        fprintf(stderr, "anti: %s: %s\n", file, error);
        goto done;
    }
    text_append(out, iface.module);
    ok = true;
done:
    text_free(&bytes);
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

/* The module whose path is the package name, which a library for C is
   built from. Without one the first module of the order stands. */
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
    paths = calloc(count + 1, sizeof *paths);
    if (paths == NULL) {
        die_out_of_memory();
    }
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
    o.input = b->units[main_at].source;
    o.output = text_cstr(&b->name);
    o.dev = true;
    o.libraries = libraries->items;
    o.library_count = libraries->count;
    o.objects = objects.items;
    o.object_count = objects.count;
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

/* Release mode: one call with every module, so the optimizer sees the
   whole program. */
static bool build_release(struct build *b, enum target t, enum cpu_level cpu,
                          size_t main_at, const struct strings *libraries)
{
    struct options o;

    base_options(b, &o, t, cpu);
    o.input = b->units[main_at].source;
    o.output = text_cstr(&b->name);
    o.libraries = libraries->items;
    o.library_count = libraries->count;
    return driver_run(&o) == 0;
}

/* The name of the deliverable without the suffix of an executable, which
   the entries of a symbols archive carry. */
static void archive_stem(enum target t, const char *name, struct text *out)
{
    const char *suffix = target_info(t)->executable_suffix;
    size_t length = strlen(suffix);

    text_append(out, name);
    if (length > 0 && out->length >= length &&
        strcmp(out->data + out->length - length, suffix) == 0) {
        out->length -= length;
        out->data[out->length] = '\0';
    }
}

/* DESIGN: a release binary carries no symbol data, so the build writes
   the archive that names it. It holds the same link with the debug
   sections kept and the map of that program. All three carry the build
   id of the binary, because the digest leaves what `-g` added out. That
   id is what ties a frame of a trace to this archive. A Windows link
   writes the symbols to a PDB, which goes in as well. */
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
    text_appendf(&debug_path, "%s/%s", text_cstr(&b->build_dir),
                 text_cstr(&debug_name));
    text_appendf(&map_path, "%s/%s", text_cstr(&b->build_dir),
                 text_cstr(&map_name));
    text_appendf(&archive, "%s/%s-symbols.zip", text_cstr(&b->dist_dir),
                 text_cstr(&stem));
    base_options(b, &o, t, cpu);
    o.input = b->units[main_at].source;
    o.output = text_cstr(&debug_path);
    o.libraries = libraries->items;
    o.library_count = libraries->count;
    o.debug = true;
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
        text_appendf(&pdb_name, "%s.pdb", text_cstr(&stem));
        text_appendf(&pdb_path, "%s.pdb", text_cstr(&debug_path));
        if (files_exists(text_cstr(&pdb_path))) {
            entries[count].name = text_cstr(&pdb_name);
            entries[count].file = text_cstr(&pdb_path);
            entries[count].executable = false;
            count++;
        }
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
    struct text header = {0};
    const char *base = module_path_last(text_cstr(&b->m.name));
    bool ok;

    base_options(b, &o, t, cpu);
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
    text_free(&header);
    return ok;
}

/* DESIGN: a binding names the frameworks of Apple's SDK it needs with
   `link framework`, and its library file records them. The build reads
   them from every library file the program reaches. It passes them to
   antic as --framework, so a program never names a framework itself. */
static bool link_frameworks(struct build *b, enum target t,
                            enum cpu_level cpu, const struct text *files,
                            const struct strings *shared)
{
    struct options search;
    struct strings seed = {0};
    const char **closure = NULL;
    size_t count = 0;
    size_t i;
    bool ok;

    for (i = 0; i < shared->count; i++) {
        strings_add(&seed, shared->items[i]);
    }
    for (i = 0; i < b->unit_count; i++) {
        strings_add(&seed, text_cstr(&files[i]));
    }
    b->frameworks = NULL;
    b->framework_count = 0;
    base_options(b, &search, t, cpu);
    search.libraries = seed.items;
    search.library_count = seed.count;
    ok = driver_libraries(&search, &b->framework_arena, &closure, &count) &&
         driver_frameworks(closure, count, &b->framework_arena,
                           &b->frameworks, &b->framework_count);
    strings_free(&seed);
    return ok;
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
    text_appendf(&b->build_dir, "%s/%s/%s/%s", b->r->root,
                 text_cstr(&b->m.build), target_name(t), mode);
    text_appendf(&b->dist_dir, "%s/%s/%s/%s", b->r->root,
                 text_cstr(&b->m.dist), target_name(t), mode);
    text_appendf(&b->lib_dir, "%s/lib", text_cstr(&b->build_dir));
    text_appendf(&b->obj_dir, "%s/obj", text_cstr(&b->build_dir));
    if (!files_make_dirs(text_cstr(&b->build_dir)) ||
        !files_make_dirs(text_cstr(&b->dist_dir))) {
        goto done;
    }
    graph_libraries(b, &shared);
    files = calloc(b->unit_count + 1, sizeof *files);
    if (files == NULL) {
        die_out_of_memory();
    }
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
        /* A project without `main` is a library project, whose build
           writes the library file of each module into `dist/`. */
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
                goto done;
            }
        }
        ok = true;
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
                     module_path_last(text_cstr(&b->m.name)), &deliverable);
    } else {
        text_appendf(&deliverable, "%s%s",
                     module_path_last(text_cstr(&b->m.name)),
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
    if (ok) {
        ok = copy_into(text_cstr(&b->name), text_cstr(&b->dist_dir),
                       text_cstr(&deliverable));
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

/* The context a path dependency is built with, which is the request of
   the project that names it. */
struct path_context {
    const struct build_request *r;
    int depth;
};

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
    if (c->depth + 1 >= BUILD_DEPTH) {
        fprintf(stderr, "anti: %s: the path dependencies of this project are "
                        "more than %d deep, so one of them is a cycle\n",
                directory, BUILD_DEPTH);
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
    sub.target = NULL;
    sub.lib = BUILD_PROGRAM;
    sub.run = false;
    if (build_project(&sub, c->depth + 1) == 0) {
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
    b->units = calloc(b->sources.count, sizeof *b->units);
    b->order = calloc(b->sources.count, sizeof *b->order);
    if (b->units == NULL || b->order == NULL) {
        die_out_of_memory();
    }
    b->unit_count = b->sources.count;
    for (i = 0; i < b->sources.count; i++) {
        if (!unit_read(text_cstr(&b->sources.items[i]), b->roots, 1, NULL,
                       &b->units[i]) ||
            !b->units[i].parsed) {
            return false;
        }
    }
    unit_order(b->units, b->unit_count, b->order);
    return true;
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

static int build_project(const struct build_request *r, int depth)
{
    struct build b;
    struct path_context context;
    struct text manifest_path = {0};
    enum target targets[TARGET_COUNT];
    size_t target_count = 0;
    size_t i;
    int status = 1;

    memset(&b, 0, sizeof b);
    b.r = r;
    b.depth = depth;
    text_appendf(&manifest_path, "%s/%s", r->root, MANIFEST_FILE);
    if (!manifest_read(text_cstr(&manifest_path), false, &b.m)) {
        goto done;
    }
    /* `antic` of the manifest is the minimum compiler version. */
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
    context.r = r;
    context.depth = depth;
    if (!deps_resolve(&b.m, r->root, r->offline, path_dependency, &context,
                      &b.graph)) {
        goto done;
    }
    b.specs = calloc(b.graph.count + 1, sizeof *b.specs);
    if (b.specs == NULL) {
        die_out_of_memory();
    }
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
    if (!build_targets(&b, targets, &target_count)) {
        goto done;
    }
    for (i = 0; i < target_count; i++) {
        enum cpu_level cpu = cpu_default(targets[i]);
        if (r->cpu != NULL && !cpu_from_name(r->cpu, targets[i], &cpu)) {
            fprintf(stderr, "anti: %s is no processor level of %s\n", r->cpu,
                    target_name(targets[i]));
            goto done;
        }
        if (!build_target(&b, targets[i], cpu)) {
            goto done;
        }
    }
    status = 0;
    if (r->run) {
        struct text program = {0};
        enum target host;
        const char *argv[2];
        if (!target_host(&host)) {
            fputs("anti: this host is no target of Anti\n", stderr);
            status = 1;
        } else {
            text_appendf(&program, "%s/%s/%s/%s/%s%s", r->root,
                         text_cstr(&b.m.dist), target_name(host),
                         r->release ? "release" : "dev",
                         module_path_last(text_cstr(&b.m.name)),
                         target_info(host)->executable_suffix);
            if (!files_exists(text_cstr(&program))) {
                fprintf(stderr, "anti: %s was not built, so there is nothing "
                                "to run\n", text_cstr(&program));
                status = 1;
            } else {
                argv[0] = text_cstr(&program);
                argv[1] = NULL;
                status = process_run(argv);
            }
        }
        text_free(&program);
    }
done:
    build_free(&b);
    text_free(&manifest_path);
    return status;
}

int build_run(const struct build_request *r)
{
    return build_project(r, 0);
}

/* The starter module of `anti new`, which prints and returns. */
static void starter_module(const char *name, struct text *out)
{
    text_appendf(out, "//! The program of the package `%s`.\n\n", name);
    text_append(out, "import anti.io;\n\n");
    text_append(out, "fn main() -> int\n{\n");
    text_append(out, "\tio.println(\"hello\");\n");
    text_append(out, "\treturn 0;\n}\n");
}

int build_new(const char *name)
{
    struct text manifest = {0};
    struct text source = {0};
    struct text path = {0};
    struct text file = {0};
    struct text directory = {0};
    const char *last = module_path_last(name);
    int status = 1;

    if (module_path_segments(name) < 2) {
        fprintf(stderr, "anti: `%s` is one segment, and a package name is a "
                        "module path of at least two, as com.example.%s\n",
                name, name);
        goto done;
    }
    text_append(&directory, last);
    if (files_exists(text_cstr(&directory))) {
        fprintf(stderr, "anti: %s is there already\n", text_cstr(&directory));
        goto done;
    }
    text_appendf(&manifest, "[package]\nname = \"%s\"\nversion = \"0.1.0\"\n",
                 name);
    starter_module(name, &source);
    /* The default layout: the manifest, `src/` with the one module whose
       path is the package name, and `test/`. */
    text_appendf(&path, "%s/src", text_cstr(&directory));
    text_appendf(&file, "%s/%s", text_cstr(&directory), MANIFEST_FILE);
    if (!files_make_dirs(text_cstr(&directory)) ||
        !files_write(text_cstr(&file), &manifest)) {
        goto done;
    }
    file.length = 0;
    if (!unit_file(text_cstr(&path), name, SOURCE_SUFFIX, &file) ||
        !files_write(text_cstr(&file), &source)) {
        goto done;
    }
    path.length = 0;
    text_appendf(&path, "%s/test", text_cstr(&directory));
    if (!files_make_dirs(text_cstr(&path))) {
        goto done;
    }
    printf("anti: %s holds the project %s\n", text_cstr(&directory), name);
    status = 0;
done:
    text_free(&manifest);
    text_free(&source);
    text_free(&path);
    text_free(&file);
    text_free(&directory);
    return status;
}

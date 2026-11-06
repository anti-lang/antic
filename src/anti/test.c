/* `anti test`: the runner of the `tests` and `fixtures` blocks.

   DESIGN: the tool reads each module with the compiler's own lexer and
   parser. The test names then come from the syntax tree that antic
   compiles, never from a second grammar. It calls driver_run in this
   process, as the sdk commands call their own code, rather than starting
   antic again. */

#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "cache.h"
#include "cpu.h"
#include "driver.h"
#include "files.h"
#include "memreport.h"
#include "modpath.h"
#include "platform.h"
#include "say.h"
#include "target.h"
#include "text.h"
#include "units.h"

/* The directory of the runners under the work directory, which the module
   path anti.test.<module> of a runner names. */
#define TEST_RUNNER_DIR "anti/test"

/* The directory of the library files of a run over a project, under the
   work directory. */
#define TEST_LIBRARY_DIR "lib"

/* The suffixes of the files a run over a project caches in obj/ of the
   build of its mode, after the module path as directories. No module
   path names such a file, since every dot of a path is a directory. */
#define TEST_CACHED_LIBRARY ".tests" ANTL_SUFFIX
#define TEST_CACHED_OBJECT ".tests"
#define TEST_CACHED_RUNNER ".runner"

/* One module of the run and the object a dev build writes for it. */
struct test_unit {
    struct unit unit;
    struct text object;         /* the .o of a dev build */
};

/* The flags of the key of a file of the run. */
static struct cache_flags test_flags(const struct options *base, bool tests,
                                     bool debug)
{
    struct cache_flags f = {0};

    f.target = base->target;
    f.cpu = base->cpu;
    f.debug = debug;
    f.checked = base->memory_checks;
    f.tests = tests;
    return f;
}

/* Append to key the options of base that decide a file of the run beside
   its input and its flags: the package with its version and the
   injections, which `[inject.test]` lays over `[inject]`. */
static void test_options_key(const struct options *base, struct text *key)
{
    size_t i;

    text_appendf(key, "package %s %s\n",
                 base->package_name != NULL ? base->package_name : "",
                 base->package_version != NULL ? base->package_version : "");
    for (i = 0; i < base->inject_count; i++) {
        text_appendf(key, "inject %s\n", base->inject[i]);
    }
}

/* The path of the file of module u with suffix in obj/ of the build that
   r->tested records, into out. */
static bool cached_path(const struct test_request *r, const struct test_unit *u,
                        const char *suffix, struct text *out)
{
    return unit_file(text_cstr(&r->tested->obj_dir), text_cstr(&u->unit.path),
                     suffix, out);
}

/* Read one module of the run. A module the parser refuses has been
   reported, and fails the run. */
static bool read_unit(const char *source, const char *const *roots,
                      size_t root_count, const char *work,
                      struct test_unit *out)
{
    return unit_read(source, roots, root_count, work, &out->unit) &&
           out->unit.parsed;
}

/* The options every call of this run shares, over the options of a
   project compile that unit_options gives. */
static bool base_options(struct options *o, const char *package,
                         const char *runtime, const char *const *roots,
                         size_t root_count, bool memory_checks)
{
    enum target target;
    enum cpu_level cpu;

    /* A test run builds for the host at the host target's default
       processor level, as a build without --cpu does. */
    if (!unit_host(&target, &cpu)) {
        return false;
    }
    unit_options(o, package, runtime, roots, root_count, target, cpu);
    o->tests = true;
    /* DESIGN: a test names the insides of its own module. `anti test`
       runs the standard library's tests too, so the run allows the
       `anti.` module paths that a reader's program may not write. */
    o->internal = true;
    /* --memory-checks carries the lines of -g, which its reports name,
       as it does for antic. */
    o->memory_checks = memory_checks;
    o->debug = memory_checks;
    return true;
}

/* Write the library file of a module, which the runner imports. A run
   over a project takes it from obj/ of the build when the key beside it
   holds, and copies it into the library directory of the run. */
static bool compile_library(const struct test_request *r,
                            const struct test_unit *u,
                            const struct options *base)
{
    struct options o = *base;
    struct cache_flags flags = test_flags(base, true, false);
    struct text cached = {0};
    struct text start = {0};
    struct text key = {0};
    bool ok = false;

    o.input = u->unit.source;
    o.output = text_cstr(&u->unit.library);
    o.library = true;
    if (r->tested == NULL) {
        return driver_run(&o) == 0;
    }
    if (!cached_path(r, u, TEST_CACHED_LIBRARY, &cached)) {
        goto done;
    }
    o.output = text_cstr(&cached);
    cache_key(u->unit.source, &flags, &start);
    test_options_key(base, &start);
    /* The imports of the library file that stands are those of its
       source, which the digest of the source holds, as in build.c. */
    text_append(&key, text_cstr(&start));
    if (!(files_exists(text_cstr(&cached)) &&
          cache_imports(base, text_cstr(&cached), &key) &&
          cache_holds(text_cstr(&cached), &key))) {
        cache_drop(text_cstr(&cached));
        if (driver_run(&o) != 0) {
            goto done;
        }
        key.length = 0;
        text_append(&key, text_cstr(&start));
        if (!cache_imports(base, text_cstr(&cached), &key) ||
            !cache_store(text_cstr(&cached), &key)) {
            goto done;
        }
    }
    ok = files_copy(text_cstr(&cached), text_cstr(&u->unit.library));
    if (!ok) {
        say_anti("cannot write %s\n", text_cstr(&u->unit.library));
    }
done:
    text_free(&cached);
    text_free(&start);
    text_free(&key);
    return ok;
}

/* Write the object of a module, which a dev build links. A run over a
   project writes it into obj/ of the build, unless the key beside it
   holds. */
static bool compile_object(const struct test_request *r, struct test_unit *u,
                           const struct options *base)
{
    struct options o = *base;
    struct cache_flags flags = test_flags(base, true, base->debug);
    struct text base_path = {0};
    struct text key = {0};
    int status;
    bool ok = false;

    /* DESIGN: antic takes -o as the base of a dev build and adds the
       suffix of each file it writes. The suffix of an object is the
       target's, `.obj` on Windows and `.o` everywhere else. */
    if (r->tested == NULL) {
        text_appendf(&base_path, "%s/%s", r->work, text_cstr(&u->unit.path));
    } else if (!cached_path(r, u, TEST_CACHED_OBJECT, &base_path)) {
        goto done;
    }
    text_appendf(&u->object, "%s%s", text_cstr(&base_path),
                 target_info(base->target)->object_suffix);
    o.input = u->unit.source;
    o.output = text_cstr(&base_path);
    o.dev = true;
    if (r->tested != NULL) {
        cache_key(u->unit.source, &flags, &key);
        test_options_key(base, &key);
        if (!cache_imports(base, text_cstr(&u->unit.library), &key)) {
            goto done;
        }
        if (cache_holds(text_cstr(&u->object), &key)) {
            ok = true;
            goto done;
        }
        cache_drop(text_cstr(&u->object));
    }
    /* A module without `main` stops at its object. */
    status = driver_run(&o);
    ok = (status == 0 || status == DRIVER_OBJECT_ONLY) &&
         (r->tested == NULL || cache_store(text_cstr(&u->object), &key));
done:
    text_free(&base_path);
    text_free(&key);
    return ok;
}

/* The objects of the modules a unit imports, for a dev link. */
struct imports {
    struct text *items;
    size_t count;
};

static void imports_free(struct imports *list)
{
    size_t i;

    for (i = 0; i < list->count; i++) {
        text_free(&list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
}

/* The library file of u and the library files of the dependency graph
   of base, which seed a walk of the imports. The graph stands under no
   root, so a walk from u alone would not reach it. The caller frees the
   array. */
static const char **unit_seed(const struct test_unit *u,
                              const struct options *base)
{
    const char **seed = files_array(base->library_count + 1, sizeof *seed);
    size_t i;

    seed[0] = text_cstr(&u->unit.library);
    for (i = 0; i < base->library_count; i++) {
        seed[i + 1] = base->libraries[i];
    }
    return seed;
}

/* DESIGN: a dev build compiles one module into its own object and links
   the objects of the modules it imports. The run therefore needs an
   object of every library file the module under test needs, its own
   excepted. Those objects go on the link line of the runner. That is
   what lets a module of the standard library carry a `tests` block. A
   run over a project takes them from the build, and any other run writes
   them into the work directory. The release run takes the whole program
   from the library files and needs none of this. */
static bool compile_imports(const struct test_request *r,
                            const struct test_unit *units, size_t unit_count,
                            const struct test_unit *u,
                            const struct options *base, const char *flat,
                            struct imports *out)
{
    const char *work = r->work;
    struct arena arena = {0};
    struct options search = *base;
    struct text directory = {0};
    const char **seed = unit_seed(u, base);
    const char **rest = files_array(base->library_count + 1, sizeof *rest);
    const char **paths = NULL;
    size_t count = 0;
    size_t i;
    bool ok = true;

    search.libraries = seed;
    search.library_count = base->library_count + 1;
    search.input = NULL;
    if (!driver_libraries(&search, &arena, &paths, &count)) {
        ok = false;
        goto done;
    }
    text_appendf(&directory, "%s/" TEST_RUNNER_DIR "/imports/%s", work, flat);
    if (!files_make_dirs_or_say(text_cstr(&directory))) {
        ok = false;
        goto done;
    }
    out->items = files_array(count + 1, sizeof *out->items);
    for (i = 0; ok && i < count; i++) {
        struct options one = *base;
        struct text base_path = {0};
        struct text object = {0};
        size_t kept = 0;
        size_t j;
        int status;
        if (strcmp(paths[i], seed[0]) == 0) {
            continue;
        }
        /* DESIGN: Q2 of "Answers of 2026-11-06, fourth set" in
           docs/decisions.md. An import has no `tests` block to keep, so
           its object is the one a dev build of `anti build` writes from
           the same library file, and a run over a project takes it from
           there. A module of the run is the exception: the import names
           the library file of the run, whose object the run wrote with
           --tests already. */
        for (j = 0; j < unit_count; j++) {
            if (strcmp(paths[i], text_cstr(&units[j].unit.library)) == 0) {
                break;
            }
        }
        if (j < unit_count) {
            text_append(&object, text_cstr(&units[j].object));
            out->items[out->count++] = object;
            continue;
        }
        if (r->tested != NULL) {
            ok = build_object(r->tested, paths[i], &object);
            if (ok) {
                out->items[out->count++] = object;
            } else {
                text_free(&object);
            }
            continue;
        }
        /* antic refuses a module that stands twice on its command line,
           so a library file of the graph leaves the list when it is the
           input, as module_object of build.c leaves it. */
        for (j = 0; j < base->library_count; j++) {
            if (strcmp(base->libraries[j], paths[i]) != 0) {
                rest[kept++] = base->libraries[j];
            }
        }
        text_appendf(&base_path, "%s/%zu", text_cstr(&directory), i);
        text_appendf(&object, "%s%s", text_cstr(&base_path),
                     target_info(base->target)->object_suffix);
        one.input = paths[i];
        one.output = text_cstr(&base_path);
        one.dev = true;
        one.libraries = rest;
        one.library_count = kept;
        /* A module without `main` stops at its object. */
        status = driver_run(&one);
        ok = status == 0 || status == DRIVER_OBJECT_ONLY;
        if (ok) {
            out->items[out->count++] = object;
        } else {
            text_free(&object);
        }
        text_free(&base_path);
    }
    if (!ok) {
        imports_free(out);
    }

done:
    text_free(&directory);
    arena_free(&arena);
    free((void *)seed);
    free((void *)rest);
    return ok;
}

/* DESIGN: the runner is Anti source. It imports the module under test,
   names each test to the runtime before it calls it, and prints the line
   of a test that returned. A failed assertion prints the name and its
   position from the runtime, then ends the process. The last line of a
   run that stopped names the test that stopped it. */
static void write_runner(const struct test_unit *u, struct text *out)
{
    const char *last = modpath_last(text_cstr(&u->unit.path));
    size_t i;

    text_append(out, "//! The runner `anti test` wrote for one module.\n");
    text_appendf(out, "import %s;\n\n", text_cstr(&u->unit.path));
    text_append(out, "extern fn printf(format: ?*byte, ...) -> c_int;\n");
    text_append(out, "extern fn anti_rt_test_running(name: ?*byte, "
                     "length: int);\n\n");
    text_append(out, "fn main() -> int\n{\n");
    for (i = 0; i < u->unit.test_count; i++) {
        struct text name = {0};
        text_appendf(&name, "%s.%s", text_cstr(&u->unit.path),
                     u->unit.tests[i]);
        text_appendf(out, "\tanti_rt_test_running(\"%s\".ptr, %zu);\n",
                     text_cstr(&name), name.length);
        if (u->unit.setup) {
            text_appendf(out, "\t%s.setup();\n", last);
        }
        text_appendf(out, "\t%s.%s();\n", last, u->unit.tests[i]);
        if (u->unit.teardown) {
            text_appendf(out, "\t%s.teardown();\n", last);
        }
        text_appendf(out, "\tprintf(\"ok %%.*s\\n\".ptr, %zu as c_int, "
                          "\"%s\".ptr);\n",
                     name.length, text_cstr(&name));
        text_free(&name);
    }
    text_append(out, "\tanti_rt_test_running(none, 0);\n");
    text_append(out, "\treturn 0;\n}\n");
}

/* The frameworks and the Linux libraries that the library files below
   the module name. The link of the runner passes them as --framework
   and --linux-lib, as `anti build` does. */
static bool runner_frameworks(const struct test_unit *u,
                              const struct options *base,
                              struct arena *arena, struct options *o)
{
    struct options search = *base;
    struct unit_links links;
    const char **seed = unit_seed(u, base);
    bool ok;

    search.libraries = seed;
    search.library_count = base->library_count + 1;
    search.input = NULL;
    ok = unit_links_read(&search, arena, &links);
    if (ok) {
        unit_links_apply(&links, o);
    }
    free((void *)seed);
    return ok;
}

/* The key of the runner of u that the compile with the options o writes,
   into key: the key of its source, the options of the run, the digest of
   the library file of u and of every library file it reaches, the key of
   every object of a dev link, the runtime and the links. */
static bool runner_key(const struct test_unit *u, const struct options *base,
                       const struct options *o, bool release,
                       struct text *key)
{
    struct cache_flags flags = test_flags(base, false, o->debug);
    size_t i;

    cache_key(o->input, &flags, key);
    text_appendf(key, "runner %s\n", release ? "release" : "dev");
    test_options_key(base, key);
    if (!cache_file("library", text_cstr(&u->unit.library), key) ||
        !cache_imports(base, text_cstr(&u->unit.library), key)) {
        return false;
    }
    for (i = 0; i < o->object_count; i++) {
        if (!cache_key_of(o->objects[i], key)) {
            return false;
        }
    }
    cache_runtime(o->runtime, o->target, o->cpu, key);
    for (i = 0; i < o->framework_count; i++) {
        text_appendf(key, "framework %s\n", o->frameworks[i]);
    }
    for (i = 0; i < o->linux_library_count; i++) {
        text_appendf(key, "linux %s\n", o->linux_libraries[i]);
    }
    return true;
}

/* Build the runner of one module and run it. units are the modules of the
   run. A run over a project links the runner into obj/ of the build,
   unless the key beside the runner that stands holds. */
static bool run_unit(const struct test_request *r,
                     const struct test_unit *units, size_t unit_count,
                     const struct test_unit *u, const struct options *base)
{
    struct options o = *base;
    struct text source = {0};
    struct text path = {0};
    struct text program = {0};
    struct text directory = {0};
    struct text flat = {0};
    struct text key = {0};
    struct imports imports = {0};
    struct arena framework_arena = {0};
    const char **objects = NULL;
    const char *argv[2];
    bool linked = false;
    bool ok = false;
    size_t i;

    /* DESIGN: the runner lives at TEST_RUNNER_DIR under the work directory.
       Its own module path is `anti.test.<module>`, never a name the
       module under test could take. Each dot of that module becomes an
       `_`, since a dot in the file name is a segment of its own. */
    write_runner(u, &source);
    text_appendf(&directory, "%s/%s", r->work, TEST_RUNNER_DIR);
    if (!files_make_dirs_or_say(text_cstr(&directory))) {
        goto done;
    }
    unit_flat_path(text_cstr(&u->unit.path), &flat);
    text_appendf(&path, "%s/%s%s", text_cstr(&directory), text_cstr(&flat),
                 SOURCE_SUFFIX);
    if (r->tested == NULL) {
        text_appendf(&program, "%s/%s.runner", r->work, text_cstr(&flat));
    } else if (!cached_path(r, u, TEST_CACHED_RUNNER, &program)) {
        goto done;
    }
    if (!files_write(text_cstr(&path), &source)) {
        goto done;
    }
    o.input = text_cstr(&path);
    o.output = text_cstr(&program);
    if (r->release) {
        /* DESIGN: release mode runs the tests with the checks and the
           assertions off, which is the specification's second run. The
           whole program comes from the library file of the module. */
        o.asserts = ASSERTS_OFF;
        o.checks = CHECKS_OFF;
    } else {
        o.dev = true;
        if (!compile_imports(r, units, unit_count, u, base,
                             text_cstr(&flat), &imports)) {
            goto done;
        }
        objects = files_array(imports.count + 1, sizeof *objects);
        objects[0] = text_cstr(&u->object);
        for (i = 0; i < imports.count; i++) {
            objects[i + 1] = text_cstr(&imports.items[i]);
        }
        o.objects = objects;
        o.object_count = imports.count + 1;
    }
    if (!runner_frameworks(u, base, &framework_arena, &o)) {
        goto done;
    }
    if (r->tested != NULL) {
        if (!runner_key(u, base, &o, r->release, &key)) {
            goto done;
        }
        linked = cache_holds(text_cstr(&program), &key);
        if (!linked) {
            cache_drop(text_cstr(&program));
        }
    }
    if (!linked) {
        if (driver_run(&o) != 0 ||
            (r->tested != NULL && !cache_store(text_cstr(&program), &key))) {
            goto done;
        }
    }
    argv[0] = text_cstr(&program);
    argv[1] = NULL;
    ok = (o.memory_checks ? memreport_run(argv) : platform_run(argv)) == 0;
done:
    imports_free(&imports);
    arena_free(&framework_arena);
    free((void *)objects);
    text_free(&source);
    text_free(&path);
    text_free(&program);
    text_free(&directory);
    text_free(&flat);
    text_free(&key);
    return ok;
}

int test_run(const struct test_request *r)
{
    struct options base;
    const char **search;
    struct test_unit *units;
    struct text libraries = {0};
    const char *library_dir = r->work;
    size_t count = 0;
    size_t total = 0;
    size_t failed = 0;
    size_t i;
    int status = 1;

    if (r->source_count == 0) {
        say_anti("test takes one or more .anti files\n");
        return 2;
    }
    if (!files_make_dirs_or_say(r->work)) {
        return 1;
    }
    /* [provisional] DESIGN: a run over a project finds the modules it
       leaves out among the library files of the build, which stand
       behind the files of the run. A library file that an earlier run
       wrote of such a module would stand before them, so the files of
       the run stand in a directory of their own, emptied first. */
    if (r->project) {
        text_appendf(&libraries, "%s/" TEST_LIBRARY_DIR, r->work);
        library_dir = text_cstr(&libraries);
        if (!files_remove_tree(library_dir)) {
            say_anti("cannot replace %s\n", library_dir);
            text_free(&libraries);
            return 1;
        }
        if (!files_make_dirs_or_say(library_dir)) {
            text_free(&libraries);
            return 1;
        }
    }
    units = files_array(r->source_count, sizeof *units);
    search = files_array(r->root_count + 1, sizeof *search);
    /* The work directory holds the library files this run wrote, and the
       user's roots hold the sources. Both are search roots of every call,
       the work directory first, so a module of the run wins. */
    search[0] = library_dir;
    for (i = 0; i < r->root_count; i++) {
        search[i + 1] = r->roots[i];
    }
    if (!base_options(&base, r->package, r->runtime, search,
                      r->root_count + 1, r->memory_checks)) {
        status = 2;
        goto done;
    }
    /* DESIGN: `[inject.test]` of the manifest lies over `[inject]`, so a
       test run takes the fake of an interface where the manifest names
       one. Every compile of the run carries the same table. */
    base.inject = r->inject;
    base.inject_count = r->inject_count;
    /* The library files of the graph stand under no root, so every
       compile of the run names them, as every compile of `anti build`
       does. */
    base.libraries = r->libraries;
    base.library_count = r->library_count;
    /* The compiles of a run over a project carry the version of the
       manifest, as the build of the project does. */
    if (r->tested != NULL && r->tested->version.length > 0) {
        base.package_version = text_cstr(&r->tested->version);
    }
    for (i = 0; i < r->source_count; i++) {
        bool read = read_unit(r->sources[i], r->roots, r->root_count,
                              library_dir, &units[count]);
        if (!read) {
            count++;
            goto done;
        }
        /* A module without a test has nothing to run, and the library
           file of the build stands for it. */
        if (r->tested_only && units[count].unit.test_count == 0) {
            unit_free(&units[count].unit);
            continue;
        }
        total += units[count].unit.test_count;
        count++;
    }
    /* Every library file first, so a module that imports another of the
       run finds it however the files were ordered. */
    for (i = 0; i < count; i++) {
        if (!compile_library(r, &units[i], &base) ||
            (!r->release && !compile_object(r, &units[i], &base))) {
            goto done;
        }
    }
    for (i = 0; i < count; i++) {
        if (units[i].unit.test_count == 0) {
            continue;
        }
        if (!run_unit(r, units, count, &units[i], &base)) {
            failed++;
        }
    }
    printf("anti test: %zu tests in %zu modules, %zu module%s failed\n",
           total, count, failed, failed == 1 ? "" : "s");
    status = failed == 0 ? 0 : 1;
done:
    for (i = 0; i < count; i++) {
        unit_free(&units[i].unit);
        text_free(&units[i].object);
    }
    free(units);
    free(search);
    text_free(&libraries);
    return status;
}

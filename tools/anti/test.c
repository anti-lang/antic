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
#include "ast.h"
#include "diagnostic.h"
#include "driver.h"
#include "files.h"
#include "lexer.h"
#include "modpath.h"
#include "parser.h"
#include "process.h"
#include "target.h"
#include "text.h"

/* One module of the run: where it came from, what it is called and the
   tests it holds. */
struct unit {
    const char *source;
    struct text path;           /* the module path, dots and all */
    const char *last;           /* the segment an import declares */
    struct text library;        /* the .antl this run wrote */
    struct text object;         /* the .o of a dev build */
    char **tests;
    size_t test_count;
    bool setup;
    bool teardown;
};

/* The compiler keeps its own copy of this test private, so the tool has
   one too rather than widening a header for four calls. */
static bool named(const struct name *a, const char *text)
{
    size_t n = strlen(text);

    return a->length == n && memcmp(a->text, text, n) == 0;
}

static void die_out_of_memory(void)
{
    fputs("anti: out of memory\n", stderr);
    exit(70);
}

static bool read_file(const char *path, struct text *out)
{
    FILE *f = fopen(path, "rb");
    char buffer[8192];
    size_t n;

    if (f == NULL) {
        fprintf(stderr, "anti: cannot read %s\n", path);
        return false;
    }
    while ((n = fread(buffer, 1, sizeof buffer, f)) > 0) {
        text_append_bytes(out, buffer, n);
    }
    fclose(f);
    return true;
}

static bool write_file(const char *path, const struct text *bytes)
{
    FILE *f = fopen(path, "wb");

    if (f == NULL) {
        fprintf(stderr, "anti: cannot write %s\n", path);
        return false;
    }
    if (bytes->length > 0 &&
        fwrite(bytes->data, 1, bytes->length, f) != bytes->length) {
        fclose(f);
        fprintf(stderr, "anti: cannot write %s\n", path);
        return false;
    }
    fclose(f);
    return true;
}

/* The directory of the library file of a module path: work/a/b for
   a.b.c, which is where find_libraries of the compiler looks. */
static bool library_path(const char *work, const char *module,
                         struct text *out)
{
    struct text directory = {0};
    const char *p;

    text_append(&directory, work);
    text_append(&directory, "/");
    for (p = module; *p != '\0'; p++) {
        if (*p == '.') {
            text_append(&directory, "/");
        } else {
            text_append_bytes(&directory, p, 1);
        }
    }
    text_append(out, text_cstr(&directory));
    text_append(out, ".antl");
    /* The directories above the file, which the compiler does not make. */
    while (directory.length > 0 && directory.data[directory.length - 1] != '/') {
        directory.length--;
    }
    if (directory.length > 0) {
        directory.data[--directory.length] = '\0';
        if (!make_dirs(text_cstr(&directory))) {
            text_free(&directory);
            return false;
        }
    }
    text_free(&directory);
    return true;
}

/* Read one module and collect the names its `tests` block declares and
   whether its `fixtures` block declares setup and teardown. */
static bool read_unit(const char *source, const char *const *roots,
                      size_t root_count, const char *work, struct unit *out)
{
    struct arena arena = {0};
    struct diagnostics diags = {0};
    struct token_list tokens = {0};
    struct module *tree = NULL;
    struct text bytes = {0};
    char message[256];
    bool ok = false;
    size_t i;

    out->source = source;
    if (!read_file(source, &bytes)) {
        goto done;
    }
    if (!module_path_of_source(source, roots, root_count, &out->path,
                               message, sizeof message)) {
        fprintf(stderr, "anti: %s\n", message);
        goto done;
    }
    out->last = module_path_last(text_cstr(&out->path));
    if (!lex(text_cstr(&bytes), bytes.length, &arena, &diags, &tokens) ||
        !parse(text_cstr(&bytes), &tokens, &arena, &diags, &tree)) {
        for (i = 0; i < diags.count; i++) {
            fprintf(stderr, "%s:%d:%d: %s\n", source, diags.items[i].line,
                    diags.items[i].column, diags.items[i].message);
        }
        goto done;
    }
    out->tests = malloc(tree->item_count * sizeof *out->tests + 1);
    if (out->tests == NULL) {
        die_out_of_memory();
    }
    for (i = 0; i < tree->item_count; i++) {
        const struct item *it = tree->items[i];
        char *name;
        if (it->kind != ITEM_FN || it->block == BLOCK_NONE) {
            continue;
        }
        if (it->block == BLOCK_FIXTURES) {
            out->setup = out->setup || named(&it->name, "setup");
            out->teardown = out->teardown || named(&it->name, "teardown");
            continue;
        }
        name = malloc(it->name.length + 1);
        if (name == NULL) {
            die_out_of_memory();
        }
        memcpy(name, it->name.text, it->name.length);
        name[it->name.length] = '\0';
        out->tests[out->test_count++] = name;
    }
    ok = library_path(work, text_cstr(&out->path), &out->library);
done:
    token_list_free(&tokens);
    diagnostics_free(&diags);
    arena_free(&arena);
    text_free(&bytes);
    return ok;
}

/* The options every call of this run shares. */
static void base_options(struct options *o, const char *runtime,
                         const char *llvm_mc, const char **roots,
                         size_t root_count)
{
    memset(o, 0, sizeof *o);
    o->roots = roots;
    o->root_count = root_count;
    o->runtime = runtime;
    o->llvm_mc = llvm_mc;
    if (!target_host(&o->target)) {
        fputs("anti: unknown host target\n", stderr);
        exit(2);
    }
    o->tests = true;
    /* DESIGN: a test names the insides of its own module. `anti test`
       runs the standard library's tests too, so the run allows the
       `anti.` module paths that a reader's program may not write. */
    o->internal = true;
}

/* Write the library file of a module, which the runner imports. */
static bool compile_library(const struct unit *u, const struct options *base)
{
    struct options o = *base;

    o.input = u->source;
    o.output = text_cstr(&u->library);
    o.library = true;
    return driver_run(&o) == 0;
}

/* Write the object of a module, which a dev build links. */
static bool compile_object(struct unit *u, const struct options *base,
                           const char *work)
{
    struct options o = *base;
    struct text base_path = {0};
    int status;

    /* DESIGN: antic takes -o as the base of a dev build and adds the
       suffix of each file it writes. The suffix of an object is the
       target's, `.obj` on Windows and `.o` everywhere else. */
    text_appendf(&base_path, "%s/%s", work, text_cstr(&u->path));
    text_appendf(&u->object, "%s%s", text_cstr(&base_path),
                 target_info(base->target)->object_suffix);
    o.input = u->source;
    o.output = text_cstr(&base_path);
    o.dev = true;
    /* A module without `main` stops at its object, which is status 3. */
    status = driver_run(&o);
    text_free(&base_path);
    return status == 0 || status == 3;
}

/* DESIGN: the runner is Anti source. It imports the module under test,
   names each test to the runtime before it calls it, and prints the line
   of a test that returned. A failed assertion prints the name and its
   position from the runtime, then ends the process. The last line of a
   run that stopped names the test that stopped it. */
static void write_runner(const struct unit *u, struct text *out)
{
    size_t i;

    text_append(out, "//! The runner `anti test` wrote for one module.\n");
    text_appendf(out, "import %s;\n\n", text_cstr(&u->path));
    text_append(out, "extern fn printf(format: ?*byte, ...) -> c_int;\n");
    text_append(out, "extern fn anti_rt_test_running(name: ?*byte, "
                     "length: int);\n\n");
    text_append(out, "fn main() -> int\n{\n");
    for (i = 0; i < u->test_count; i++) {
        struct text name = {0};
        text_appendf(&name, "%s.%s", text_cstr(&u->path), u->tests[i]);
        text_appendf(out, "\tanti_rt_test_running(\"%s\".ptr, %zu);\n",
                     text_cstr(&name), name.length);
        if (u->setup) {
            text_appendf(out, "\t%s.setup();\n", u->last);
        }
        text_appendf(out, "\t%s.%s();\n", u->last, u->tests[i]);
        if (u->teardown) {
            text_appendf(out, "\t%s.teardown();\n", u->last);
        }
        text_appendf(out, "\tprintf(\"ok %%.*s\\n\".ptr, %zu as c_int, "
                          "\"%s\".ptr);\n",
                     name.length, text_cstr(&name));
        text_free(&name);
    }
    text_append(out, "\tanti_rt_test_running(none, 0);\n");
    text_append(out, "\treturn 0;\n}\n");
}

/* Build the runner of one module and run it. */
static bool run_unit(const struct unit *u, const struct options *base,
                     const char *work, bool release)
{
    struct options o = *base;
    struct text source = {0};
    struct text path = {0};
    struct text program = {0};
    struct text directory = {0};
    struct text flat = {0};
    const char *objects[1];
    const char *argv[2];
    bool ok = false;
    size_t i;

    /* DESIGN: the runner lives at anti/test/ under the work directory.
       Its own module path is `anti.test.<module>`, never a name the
       module under test could take. Each dot of that module becomes an
       `_`, since a dot in the file name is a segment of its own. */
    write_runner(u, &source);
    text_appendf(&directory, "%s/anti/test", work);
    if (!make_dirs(text_cstr(&directory))) {
        goto done;
    }
    text_appendf(&flat, "%s", text_cstr(&u->path));
    for (i = 0; i < flat.length; i++) {
        if (flat.data[i] == '.') {
            flat.data[i] = '_';
        }
    }
    text_appendf(&path, "%s/%s.anti", text_cstr(&directory),
                 text_cstr(&flat));
    text_appendf(&program, "%s/%s.runner", work, text_cstr(&flat));
    if (!write_file(text_cstr(&path), &source)) {
        goto done;
    }
    o.input = text_cstr(&path);
    o.output = text_cstr(&program);
    if (release) {
        /* DESIGN: release mode runs the tests with the checks and the
           assertions off, which is the specification's second run. The
           whole program comes from the library file of the module. */
        o.asserts = ASSERTS_OFF;
        o.checks = CHECKS_OFF;
    } else {
        o.dev = true;
        objects[0] = text_cstr(&u->object);
        o.objects = objects;
        o.object_count = 1;
    }
    if (driver_run(&o) != 0) {
        goto done;
    }
    argv[0] = text_cstr(&program);
    argv[1] = NULL;
    ok = process_run(argv) == 0;
done:
    text_free(&source);
    text_free(&path);
    text_free(&program);
    text_free(&directory);
    text_free(&flat);
    return ok;
}

int test_run(const char *const *sources, size_t source_count,
             const char *const *roots, size_t root_count, const char *work,
             const char *runtime, const char *llvm_mc, bool release)
{
    struct options base;
    const char **search;
    struct unit *units;
    size_t total = 0;
    size_t failed = 0;
    size_t i;
    size_t j;

    if (source_count == 0) {
        fputs("anti: test takes one or more .anti files\n", stderr);
        return 2;
    }
    if (!make_dirs(work)) {
        return 1;
    }
    units = calloc(source_count, sizeof *units);
    search = malloc((root_count + 1) * sizeof *search);
    if (units == NULL || search == NULL) {
        die_out_of_memory();
    }
    /* The work directory holds the library files this run wrote, and the
       user's roots hold the sources. Both are search roots of every call,
       the work directory first, so a module of the run wins. */
    search[0] = work;
    for (i = 0; i < root_count; i++) {
        search[i + 1] = roots[i];
    }
    base_options(&base, runtime, llvm_mc, search, root_count + 1);
    for (i = 0; i < source_count; i++) {
        if (!read_unit(sources[i], roots, root_count, work, &units[i])) {
            failed = source_count;
            goto done;
        }
        total += units[i].test_count;
    }
    /* Every library file first, so a module that imports another of the
       run finds it however the files were ordered. */
    for (i = 0; i < source_count; i++) {
        if (!compile_library(&units[i], &base) ||
            (!release && !compile_object(&units[i], &base, work))) {
            failed = source_count;
            goto done;
        }
    }
    for (i = 0; i < source_count; i++) {
        if (units[i].test_count == 0) {
            continue;
        }
        if (!run_unit(&units[i], &base, work, release)) {
            failed++;
        }
    }
    printf("anti test: %zu tests in %zu modules, %zu module%s failed\n",
           total, source_count, failed, failed == 1 ? "" : "s");
done:
    for (i = 0; i < source_count; i++) {
        for (j = 0; j < units[i].test_count; j++) {
            free(units[i].tests[j]);
        }
        free(units[i].tests);
        text_free(&units[i].path);
        text_free(&units[i].library);
        text_free(&units[i].object);
    }
    free(units);
    free(search);
    return failed == 0 ? 0 : 1;
}

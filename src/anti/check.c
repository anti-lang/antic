/* `anti check`: every check that writes no artifact of a program.

   DESIGN: the classes of "Check command" in docs/tooling-addendum.md run
   in order, and the first failing class ends the run. The front-end class
   writes the interface file of each module into the work directory. A
   module that imports another of the project needs it, and that file is
   the only thing the command writes. The doc-warning class reports and
   does not fail, because the specification makes each of its findings a
   warning. A backtick may hold code that is no name, and a `may fail`
   function that never fails is a warning of the checker on purpose. */
#include "check.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "antic.h"
#include "arena.h"
#include "cpu.h"
#include "driver.h"
#include "files.h"
#include "fmt.h"
#include "manifest.h"
#include "modpath.h"
#include "target.h"
#include "text.h"
#include "units.h"

/* The directories of the work tree. The interface files stand in its
   root. The module of a user doc block goes under docs and the module of
   a developer doc block under dev. */
#define CHECK_DOCS_DIR "check/docs"
#define CHECK_DEV_DIR "check/dev"

/* One fenced `anti` block of a doc comment. */
struct doc_block {
    struct text body;
    struct text owner;          /* the item the comment belongs to */
    int line;                   /* the line of the comment */
    bool dev;                   /* a block of a `//#` or `//#!` comment */
};

struct doc_blocks {
    struct doc_block *items;
    size_t count;
    size_t capacity;
};

/* The options every call of the run shares, over the options of a
   project compile that unit_options gives. */
static bool base_options(struct options *o, const char *package,
                         const char *runtime, const char *const *roots,
                         size_t root_count)
{
    enum target target;
    enum cpu_level cpu;

    if (!unit_host(&target, &cpu)) {
        return false;
    }
    unit_options(o, package, runtime, roots, root_count, target, cpu);
    o->front_end = true;
    /* A warning fails the check, as it fails a release build. The doc
       warnings stay a class that fails nothing. */
    o->warnings_as_errors = true;
    return true;
}

/* The front end on every file, and with all_targets once per target. The
   host writes the interface file of each module, which the imports of the
   next module and the doc blocks read. */
static size_t front_end_class(const struct unit *units, size_t count,
                              const size_t *order, const struct options *base,
                              struct diagnostic_counts *counts,
                              bool undocumented, bool all_targets,
                              size_t *targets)
{
    size_t failed = 0;
    size_t i;
    int t;

    *targets = 1;
    for (i = 0; i < count; i++) {
        const struct unit *u = &units[order[i]];
        struct options o = *base;
        if (!u->parsed) {
            failed++;
            continue;
        }
        o.input = u->source;
        o.output = text_cstr(&u->library);
        o.library = true;
        o.doc_warnings = true;
        o.warn_undocumented = undocumented;
        o.counts = counts;
        if (driver_run(&o) != 0) {
            failed++;
        }
    }
    if (!all_targets) {
        return failed;
    }
    for (t = 0; t < TARGET_COUNT; t++) {
        if ((enum target)t == base->target) {
            continue;
        }
        (*targets)++;
        for (i = 0; i < count; i++) {
            const struct unit *u = &units[order[i]];
            struct options o = *base;
            if (!u->parsed) {
                continue;
            }
            o.input = u->source;
            o.target = (enum target)t;
            o.cpu = cpu_default(o.target);
            if (driver_run(&o) != 0) {
                failed++;
            }
        }
    }
    return failed;
}

static void blocks_add(struct doc_blocks *list, struct doc_block *one)
{
    if (list->count == list->capacity) {
        list->items = files_grow(list->items,
                                 &list->capacity, sizeof *list->items);
    }
    list->items[list->count++] = *one;
}

static void blocks_free(struct doc_blocks *list)
{
    size_t i;

    for (i = 0; i < list->count; i++) {
        text_free(&list->items[i].body);
        text_free(&list->items[i].owner);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

/* The bytes of one line of the doc text, without its line end. */
static size_t line_end(const struct antic_doc_comment *doc, size_t from)
{
    size_t end = from;

    while (end < doc->length && doc->text[end] != '\n') {
        end++;
    }
    return end;
}

/* Whether the line is a fence, and what its language tag is. */
static bool fence(const struct antic_doc_comment *doc, size_t from, size_t end,
                  const char **tag, size_t *tag_length)
{
    size_t lead = from;

    while (lead < end && (doc->text[lead] == ' ' || doc->text[lead] == '\t')) {
        lead++;
    }
    if (end - lead < 3 || memcmp(doc->text + lead, "```", 3) != 0) {
        return false;
    }
    *tag = doc->text + lead + 3;
    *tag_length = end - lead - 3;
    while (*tag_length > 0 && (*tag)[*tag_length - 1] == ' ') {
        (*tag_length)--;
    }
    return true;
}

/* Every fenced `anti` block of one doc comment. A block of a developer
   comment compiles inside the module and a block of a user comment as a
   module that imports it. */
static void collect_blocks(const struct antic_doc_comment *doc,
                           struct doc_blocks *out)
{
    size_t start = 0;
    struct doc_block one;
    bool open = false;

    memset(&one, 0, sizeof one);
    while (start <= doc->length) {
        size_t end = line_end(doc, start);
        const char *tag = NULL;
        size_t tag_length = 0;
        if (fence(doc, start, end, &tag, &tag_length)) {
            if (open) {
                blocks_add(out, &one);
                memset(&one, 0, sizeof one);
                open = false;
            } else if (tag_length == 4 && memcmp(tag, "anti", 4) == 0) {
                open = true;
                one.line = doc->line;
                one.dev = doc->dev;
                if (doc->owner != NULL) {
                    text_append(&one.owner, doc->owner);
                } else {
                    text_append(&one.owner, "the module");
                }
            }
            start = end + 1;
            continue;
        }
        if (open) {
            text_append_bytes(&one.body, doc->text + start, end - start);
            text_append(&one.body, "\n");
        }
        start = end + 1;
    }
    if (open) {
        text_free(&one.body);
        text_free(&one.owner);
    }
}

/* A block without `fn main` is wrapped in a function. The user block
   takes `main`, as the specification says. The developer block takes a
   name of its own, because the module it is compiled inside may hold a
   `main` already. */
static void wrap_block(const struct doc_block *one, const char *name,
                       struct text *out)
{
    if (strstr(text_cstr(&one->body), "fn main") != NULL) {
        text_append(out, text_cstr(&one->body));
        return;
    }
    text_appendf(out, "fn %s() -> int\n{\n%s\treturn 0;\n}\n", name,
                 text_cstr(&one->body));
}

/* Compile one block through the front end. The module of a user block
   imports the documented one. The module of a developer block is the
   documented one with the block added, so a private item is in reach. */
static bool compile_block(const struct unit *u, const struct doc_block *one,
                          size_t index, const struct options *base,
                          const char *work)
{
    struct options o = *base;
    struct text path = {0};
    struct text source = {0};
    struct text flat = {0};
    struct text name = {0};
    bool ok = false;

    unit_flat_path(text_cstr(&u->path), &flat);
    if (one->dev) {
        struct text directory = {0};
        struct text original = {0};
        text_appendf(&directory, "%s/%s", work, CHECK_DEV_DIR);
        if (!files_read_reported(u->source, &original) ||
            !unit_file(text_cstr(&directory), text_cstr(&u->path),
                         SOURCE_SUFFIX, &path)) {
            text_free(&directory);
            text_free(&original);
            goto done;
        }
        text_free(&directory);
        text_append(&source, text_cstr(&original));
        text_append(&source, "\n");
        text_free(&original);
        text_appendf(&flat, "_block_%zu", index);
        wrap_block(one, text_cstr(&flat), &source);
    } else {
        struct text directory = {0};
        text_appendf(&directory, "%s/%s", work, CHECK_DOCS_DIR);
        if (!files_make_dirs(text_cstr(&directory))) {
            text_free(&directory);
            goto done;
        }
        text_appendf(&path, "%s/%s_%zu%s", text_cstr(&directory),
                     text_cstr(&flat), index, SOURCE_SUFFIX);
        text_free(&directory);
        /* The import of the documented module is implied. A block that
           writes it itself takes its own line, because two imports of one
           module are a redeclaration. */
        text_appendf(&name, "import %s;", text_cstr(&u->path));
        if (strstr(text_cstr(&one->body), text_cstr(&name)) == NULL) {
            text_appendf(&source, "import %s;\n\n", text_cstr(&u->path));
        }
        wrap_block(one, "main", &source);
    }
    if (!files_write(text_cstr(&path), &source)) {
        goto done;
    }
    o.input = text_cstr(&path);
    ok = driver_run(&o) == 0;
    if (!ok) {
        fprintf(stderr,
                "anti: the `anti` block of `%s` at %s:%d failed, and its "
                "module is %s\n",
                text_cstr(&one->owner), u->source, one->line,
                text_cstr(&path));
    }
done:
    text_free(&path);
    text_free(&source);
    text_free(&flat);
    text_free(&name);
    return ok;
}

/* Every `anti` block of every doc comment of one module. */
static bool unit_blocks(const struct unit *u, const struct options *base,
                        const char *work, size_t *count, size_t *failed)
{
    struct arena arena = {0};
    struct antic_outline outline;
    struct text bytes = {0};
    struct doc_blocks list = {0};
    size_t i;
    bool ok = true;

    if (!files_read_reported(u->source, &bytes)) {
        ok = false;
        goto done;
    }
    if (!antic_outline(u->source, text_cstr(&bytes), bytes.length, false,
                       &arena, &outline)) {
        goto done;
    }
    for (i = 0; i < outline.comment_count; i++) {
        collect_blocks(&outline.comments[i], &list);
    }
    *count += list.count;
    for (i = 0; i < list.count; i++) {
        if (!compile_block(u, &list.items[i], i, base, work)) {
            (*failed)++;
        }
    }
done:
    blocks_free(&list);
    arena_free(&arena);
    text_free(&bytes);
    return ok;
}

/* The line of the first byte the two texts do not share, counted from
   one. Both end with a newline, so a text that runs out is a line that
   differs as well. */
static int first_difference(const struct text *a, const struct text *b)
{
    const char *left = text_cstr(a);
    const char *right = text_cstr(b);
    size_t shorter = a->length < b->length ? a->length : b->length;
    int line = 1;
    size_t i;

    for (i = 0; i < shorter; i++) {
        if (left[i] != right[i]) {
            return line;
        }
        line += left[i] == '\n';
    }
    return line;
}

/* DESIGN: the class writes the canonical text of each file and compares
   the bytes, which is the test `anti fmt --check` runs. It reported the
   rules against the token stream while `anti fmt` was not built. */
static size_t format_class(const struct unit *units, size_t count)
{
    size_t findings = 0;
    size_t i;

    for (i = 0; i < count; i++) {
        struct text source = {0};
        struct text formed = {0};
        if (!files_read_reported(units[i].source, &source)) {
            findings++;
        } else if (!fmt_source(text_cstr(&source), source.length, &formed)) {
            /* A file the lexer refuses is the front-end class's to
               report, and that class runs before this one. */
            findings++;
            fprintf(stderr, "%s:1:1: error: the file does not lex, so it has "
                            "no canonical form\n", units[i].source);
        } else if (formed.length != source.length ||
                   memcmp(text_cstr(&formed), text_cstr(&source),
                          formed.length) != 0) {
            findings++;
            fprintf(stderr, "%s:%d:1: error: the line is not the one "
                            "`anti fmt` writes\n", units[i].source,
                    first_difference(&source, &formed));
        }
        text_free(&source);
        text_free(&formed);
    }
    return findings;
}

/* The sources of the project: every `.anti` file under the source and the
   test directory the manifest names. */
static bool project_sources(const char *src, const char *test,
                            struct files_list *out)
{
    bool ok = files_list_tree(src, SOURCE_SUFFIX, out) &&
              files_list_tree(test, SOURCE_SUFFIX, out);

    if (out->count == 0) {
        fprintf(stderr, "anti: no %s file under %s or %s\n", SOURCE_SUFFIX,
                src, test);
        ok = false;
    }
    return ok;
}

int check_run(const struct check_request *r)
{
    struct diagnostic_counts counts;
    struct files_list found = {0};
    struct options base;
    struct options blocks;
    const char **search = NULL;
    const char **block_search = NULL;
    struct text dev_root = {0};
    struct text package = {0};
    struct text src = {0};
    struct text test = {0};
    struct unit *units = NULL;
    const char **path_roots = NULL;
    size_t path_root_count;
    size_t *order = NULL;
    size_t targets = 1;
    size_t failed;
    size_t count = 0;
    size_t blocks_count = 0;
    size_t blocks_failed = 0;
    size_t i;
    int status = 0;

    memset(&counts, 0, sizeof counts);
    /* DESIGN: the package name of the manifest rides on every call. The
       modules of one package share an `internal` item, and the interface
       files this run writes carry the name. The source and the test
       directory are search roots of the run, because the directories
       under them mirror the module paths. */
    if (!manifest_layout_read(MANIFEST_FILE, &src, &test, &package) ||
        (r->source_count == 0 &&
         !project_sources(text_cstr(&src), text_cstr(&test), &found)) ||
        !files_make_dirs(r->work)) {
        status = 1;
        goto done;
    }
    count = r->source_count > 0 ? r->source_count : found.count;
    units = files_array(count + 1, sizeof *units);
    order = files_array(count + 1, sizeof *order);
    path_roots = files_array(r->root_count + 2, sizeof *path_roots);
    search = files_array(r->root_count + 3, sizeof *search);
    block_search = files_array(r->root_count + 4, sizeof *block_search);
    path_root_count = 0;
    for (i = 0; i < r->root_count; i++) {
        path_roots[path_root_count++] = r->roots[i];
    }
    path_roots[path_root_count++] = text_cstr(&src);
    path_roots[path_root_count++] = text_cstr(&test);
    /* The work directory holds the interface files of this run, and the
       roots hold the sources. All of them are search roots, the work
       directory first, so a module of the run wins. */
    search[0] = r->work;
    for (i = 0; i < path_root_count; i++) {
        search[i + 1] = path_roots[i];
    }
    text_appendf(&dev_root, "%s/%s", r->work, CHECK_DEV_DIR);
    block_search[0] = text_cstr(&dev_root);
    for (i = 0; i <= path_root_count; i++) {
        block_search[i + 1] = search[i];
    }
    if (!base_options(&base, package.length > 0 ? text_cstr(&package) : NULL,
                      r->runtime, search, path_root_count + 1)) {
        status = 2;
        goto done;
    }
    blocks = base;
    blocks.roots = block_search;
    blocks.root_count = path_root_count + 2;
    for (i = 0; i < count; i++) {
        const char *source =
            r->source_count > 0 ? r->sources[i] : text_cstr(&found.items[i]);
        unit_read(source, path_roots, path_root_count, r->work, &units[i]);
    }
    unit_order(units, count, order);

    failed = front_end_class(units, count, order, &base, &counts,
                             r->undocumented, r->all_targets, &targets);
    printf("anti check: front end: %zu file%s, %zu target%s, %zu warning%s",
           count, count == 1 ? "" : "s", targets, targets == 1 ? "" : "s",
           counts.warnings, counts.warnings == 1 ? "" : "s");
    if (failed > 0) {
        printf(", %zu failed\n", failed);
        status = 1;
        goto done;
    }
    printf("\n");

    for (i = 0; i < count; i++) {
        if (!unit_blocks(&units[order[i]], &blocks, r->work, &blocks_count,
                         &blocks_failed)) {
            blocks_failed++;
        }
    }
    printf("anti check: doc blocks: %zu block%s", blocks_count,
           blocks_count == 1 ? "" : "s");
    if (blocks_failed > 0) {
        printf(", %zu failed\n", blocks_failed);
        status = 1;
        goto done;
    }
    printf("\n");

    /* The doc warnings of the front-end pass, which --doc-warnings
       produced. The class reports and does not fail. */
    printf("anti check: doc warnings: %zu\n", counts.doc_warnings);

    failed = format_class(units, count);
    printf("anti check: formatting: %zu file%s", count, count == 1 ? "" : "s");
    if (failed > 0) {
        printf(", %zu finding%s\n", failed, failed == 1 ? "" : "s");
        status = 1;
        goto done;
    }
    printf("\n");

    /* DESIGN: no class of its own checks the patterns. The front end
       compiles every pattern literal with PCRE2, as antic does in every
       build, so a malformed one fails the first class. */

done:
    for (i = 0; i < count; i++) {
        unit_free(&units[i]);
    }
    free(units);
    free(order);
    free((void *)search);
    free((void *)block_search);
    free((void *)path_roots);
    text_free(&dev_root);
    text_free(&package);
    text_free(&src);
    text_free(&test);
    files_list_free(&found);
    return status;
}

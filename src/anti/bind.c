/* anti bind: bindings of C libraries, and the C header of a library
   file. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bind.h"
#include "bindmodel.h"
#include "driver.h"
#include "files.h"
#include "fmt.h"
#include "header.h"
#include "modpath.h"
#include "text.h"

/* DESIGN: the header comes from the same driver call that `antic --lib`
   writes its header with, over the interfaces of the library file and
   its imports. The two cannot drift, and the test anti_bind_header
   compares their bytes. The file takes the name of the last segment of
   the module, as the library of --lib does without -o. */
int bind_header(const char *library, const char *out_dir,
                const char *runtime, const char **roots, size_t root_count)
{
    struct options o;
    struct text header = {0};
    struct text path = {0};
    const char *base;
    const char *dot;
    int status = 1;

    memset(&o, 0, sizeof o);
    o.input = library;
    o.roots = roots;
    o.root_count = root_count;
    o.runtime = runtime;
    if (!target_host(&o.target)) {
        fputs("anti: unknown host target\n", stderr);
        return 2;
    }
    o.cpu = cpu_default(o.target);
    if (!files_make_dirs(out_dir) || !driver_library_header(&o, &header)) {
        goto done;
    }
    base = strrchr(library, '/');
    base = base != NULL ? base + 1 : library;
    dot = strrchr(base, '.');
    text_appendf(&path, "%s/%.*s%s", out_dir,
                 (int)(dot != NULL ? (size_t)(dot - base) : strlen(base)),
                 base, HEADER_SUFFIX);
    if (files_write(text_cstr(&path), &header)) {
        status = 0;
    }

done:
    text_free(&header);
    text_free(&path);
    return status;
}

/* DESIGN: the module of a binding is anti.<name>, where the name is the
   file name without its extension, and without `_api` for a description
   of rlparser. raylib_api.json gives anti.raylib and miniaudio.h gives
   anti.miniaudio, the two modules docs/tooling-addendum.md names. */
static void default_module(const char *input, struct text *out)
{
    const char *base = files_base_name(input);
    const char *dot = strrchr(base, '.');
    size_t n = dot != NULL ? (size_t)(dot - base) : strlen(base);

    if (n > 4 && strncmp(base + n - 4, "_api", 4) == 0) {
        n -= 4;
    }
    text_appendf(out, "anti.%.*s", (int)n, base);
}

static bool write_named(const char *dir, const char *name,
                        const struct text *bytes)
{
    struct text path = {0};
    bool ok;

    text_appendf(&path, "%s/%s", dir, name);
    ok = files_write(text_cstr(&path), bytes);
    text_free(&path);
    return ok;
}

/* DESIGN: a binding is Anti in the canonical form of `anti fmt`, so a
   binding committed into a tree that `anti fmt --check` guards stands as
   the generator wrote it. The formatter takes the writer's text, which
   keeps the writer free of the rules of line width. */
static bool write_formatted(const char *dir, const char *name,
                            const struct text *bytes)
{
    struct text formatted = {0};
    bool ok;

    if (!fmt_source(bytes->data != NULL ? bytes->data : "", bytes->length,
                    &formatted)) {
        fprintf(stderr, "anti: bind wrote %s, and it does not lex\n", name);
        text_free(&formatted);
        return false;
    }
    ok = write_named(dir, name, &formatted);
    text_free(&formatted);
    return ok;
}

int bind_run(const struct bind_request *q)
{
    struct bind_module b;
    struct text module = {0};
    struct text bytes = {0};
    struct text out = {0};
    struct text name = {0};
    size_t i;
    size_t bound = 0;
    int status = 1;

    memset(&b, 0, sizeof b);
    if (q->module != NULL) {
        text_append(&module, q->module);
    } else {
        default_module(q->input, &module);
    }
    b.module = text_cstr(&module);
    b.library = module_path_last(b.module);
    b.source = files_base_name(q->input);
    b.defines = q->defines;
    b.define_count = q->define_count;
    if (q->clang) {
        struct bind_clang_request c;
        b.header = b.source;
        memset(&c, 0, sizeof c);
        c.header = q->input;
        c.runtime = q->runtime;
        c.target = q->target;
        c.includes = q->includes;
        c.include_count = q->include_count;
        c.defines = q->defines;
        c.define_count = q->define_count;
        if (!bind_read_clang(&b, &c)) {
            goto done;
        }
    } else {
        struct text header = {0};
        text_appendf(&header, "%s.h", b.library);
        b.header = bind_strdup(&b, text_cstr(&header));
        text_free(&header);
        if (!files_read_reported(q->input, &bytes) ||
            !bind_read_api(&b, (const unsigned char *)bytes.data,
                           bytes.length)) {
            goto done;
        }
    }
    bind_settle(&b);
    if (!files_make_dirs(q->out_dir)) {
        goto done;
    }
    bind_write_module(&b, &out);
    text_appendf(&name, "%s.anti", b.library);
    if (!write_formatted(q->out_dir, text_cstr(&name), &out)) {
        goto done;
    }
    if (bind_needs_shim(&b)) {
        out.length = 0;
        name.length = 0;
        bind_write_shim(&b, &out);
        text_appendf(&name, "shim_%s.c", b.library);
        if (!write_named(q->out_dir, text_cstr(&name), &out)) {
            goto done;
        }
    }
    if (q->probe) {
        out.length = 0;
        name.length = 0;
        bind_write_probe_c(&b, &out);
        text_appendf(&name, "probe_%s.c", b.library);
        if (!write_named(q->out_dir, text_cstr(&name), &out)) {
            goto done;
        }
        out.length = 0;
        name.length = 0;
        bind_write_probe_anti(&b, &out);
        text_appendf(&name, "probe_%s.anti", b.library);
        if (!write_formatted(q->out_dir, text_cstr(&name), &out)) {
            goto done;
        }
    }
    for (i = 0; i < b.functions.count; i++) {
        const struct bind_function *f = b.functions.items[i];
        bound += f->bound ? 1 : 0;
    }
    printf("bind: %s from %s, %zu functions%s, %zu warning%s\n", b.module,
           b.source, bound, bind_needs_shim(&b) ? " and a shim" : "",
           b.warnings, b.warnings == 1 ? "" : "s");
    status = 0;

done:
    free(b.records.items);
    free(b.enums.items);
    free(b.functions.items);
    free(b.consts.items);
    arena_free(&b.arena);
    text_free(&module);
    text_free(&bytes);
    text_free(&out);
    text_free(&name);
    return status;
}

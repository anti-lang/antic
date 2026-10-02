/* The modules of a run over the sources of a project, in the order a
   compile takes them, and the options every compile of the run starts
   from. `anti check`, `anti doc`, `anti build` and `anti test` need the
   module path of each source, the modules it imports and the interface
   file the run writes for it. The list has one definition. */
#include "units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "antic.h"
#include "arena.h"
#include "files.h"
#include "modpath.h"
#include "text.h"

void unit_options(struct options *o, const char *package, const char *runtime,
                  const char **roots, size_t root_count, enum target target,
                  enum cpu_level cpu)
{
    memset(o, 0, sizeof *o);
    o->package_name = package;
    o->runtime = runtime;
    o->roots = roots;
    o->root_count = root_count;
    o->target = target;
    o->cpu = cpu;
}

bool unit_host(enum target *target, enum cpu_level *cpu)
{
    if (!target_host(target)) {
        fputs("anti: unknown host target\n", stderr);
        return false;
    }
    *cpu = cpu_default(*target);
    return true;
}

void unit_flat_path(const char *module, struct text *out)
{
    const char *p;

    for (p = module; *p != '\0'; p++) {
        text_appendf(out, "%c", *p == '.' ? '_' : *p);
    }
}

bool unit_file(const char *dir, const char *module,
                        const char *suffix, struct text *out)
{
    struct text directory = {0};
    const char *p;
    bool ok;

    text_appendf(out, "%s/", dir);
    for (p = module; *p != '\0'; p++) {
        text_appendf(out, "%c", *p == '.' ? '/' : *p);
    }
    text_append(out, suffix);
    text_append(&directory, text_cstr(out));
    while (directory.length > 0 &&
           directory.data[directory.length - 1] != '/') {
        directory.length--;
    }
    if (directory.length > 0) {
        directory.data[--directory.length] = '\0';
    }
    ok = directory.length == 0 || files_make_dirs(text_cstr(&directory));
    text_free(&directory);
    return ok;
}

bool unit_read(const char *source, const char *const *roots,
                      size_t root_count, const char *work, struct unit *out)
{
    struct arena arena = {0};
    struct antic_outline outline;
    struct text bytes = {0};
    char message[256];
    size_t i;
    bool ok = false;

    memset(out, 0, sizeof *out);
    out->source = source;
    if (!files_read_reported(source, &bytes)) {
        goto done;
    }
    if (!modpath_of_source(source, roots, root_count, &out->path, message,
                               sizeof message)) {
        fprintf(stderr, "anti: %s\n", message);
        goto done;
    }
    if (work != NULL && !unit_file(work, text_cstr(&out->path), ANTL_SUFFIX,
                                   &out->library)) {
        goto done;
    }
    ok = true;
    if (!antic_outline(source, text_cstr(&bytes), bytes.length, true, &arena,
                       &outline)) {
        goto done;
    }
    out->parsed = true;
    out->has_main = outline.has_main;
    out->setup = outline.setup;
    out->teardown = outline.teardown;
    out->tests = files_array(outline.test_count + 1, sizeof *out->tests);
    for (i = 0; i < outline.test_count; i++) {
        size_t n = strlen(outline.tests[i]);
        char *name = files_array(n + 1, 1);
        memcpy(name, outline.tests[i], n);
        out->tests[out->test_count++] = name;
    }
    out->imports = files_array(outline.import_count + 1, sizeof *out->imports);
    for (i = 0; i < outline.import_count; i++) {
        text_append(&out->imports[i], outline.imports[i]);
    }
    out->import_count = outline.import_count;
done:
    arena_free(&arena);
    text_free(&bytes);
    return ok;
}

void unit_free(struct unit *u)
{
    size_t i;

    for (i = 0; i < u->import_count; i++) {
        text_free(&u->imports[i]);
    }
    free(u->imports);
    for (i = 0; i < u->test_count; i++) {
        free(u->tests[i]);
    }
    free(u->tests);
    text_free(&u->path);
    text_free(&u->library);
}

void unit_order(const struct unit *units, size_t count, size_t *order)
{
    bool *done = files_array(count + 1, sizeof *done);
    size_t placed = 0;
    size_t i;
    size_t j;
    size_t k;
    bool grew = true;

    while (grew && placed < count) {
        grew = false;
        for (i = 0; i < count; i++) {
            bool ready = true;
            if (done[i]) {
                continue;
            }
            for (j = 0; ready && j < units[i].import_count; j++) {
                for (k = 0; k < count; k++) {
                    if (!done[k] && k != i &&
                        strcmp(text_cstr(&units[k].path),
                               text_cstr(&units[i].imports[j])) == 0) {
                        ready = false;
                        break;
                    }
                }
            }
            if (ready) {
                order[placed++] = i;
                done[i] = true;
                grew = true;
            }
        }
    }
    for (i = 0; i < count; i++) {
        if (!done[i]) {
            order[placed++] = i;
        }
    }
    free(done);
}

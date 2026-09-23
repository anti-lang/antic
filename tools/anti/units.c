/* The modules of a run over the sources of a project, in the order a
   compile takes them. `anti check` and `anti doc` both need the module
   path of each source, the modules it imports and the interface file the
   run writes for it. The list has one definition. */
#include "units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "antl.h"
#include "arena.h"
#include "ast.h"
#include "diagnostic.h"
#include "files.h"
#include "lexer.h"
#include "modpath.h"
#include "parser.h"
#include "text.h"

static void out_of_memory(void)
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
    ok = directory.length == 0 || make_dirs(text_cstr(&directory));
    text_free(&directory);
    return ok;
}

bool unit_read(const char *source, const char *const *roots,
                      size_t root_count, const char *work, struct unit *out)
{
    struct arena arena = {0};
    struct diagnostics diags = {0};
    struct token_list tokens = {0};
    struct module *tree = NULL;
    struct text bytes = {0};
    char message[256];
    size_t i;
    bool ok = false;

    memset(out, 0, sizeof *out);
    out->source = source;
    if (!read_file(source, &bytes)) {
        goto done;
    }
    if (!module_path_of_source(source, roots, root_count, &out->path, message,
                               sizeof message)) {
        fprintf(stderr, "anti: %s\n", message);
        goto done;
    }
    if (!unit_file(work, text_cstr(&out->path), ANTL_SUFFIX,
                     &out->library)) {
        goto done;
    }
    ok = true;
    if (!lex(text_cstr(&bytes), bytes.length, &arena, &diags, &tokens) ||
        !parse(text_cstr(&bytes), &tokens, &arena, &diags, &tree)) {
        for (i = 0; i < diags.count; i++) {
            fprintf(stderr, "%s:%d:%d: error: %s\n", source,
                    diags.items[i].line, diags.items[i].column,
                    diags.items[i].message);
        }
        goto done;
    }
    out->parsed = true;
    out->imports = calloc(tree->import_count + 1, sizeof *out->imports);
    if (out->imports == NULL) {
        out_of_memory();
    }
    for (i = 0; i < tree->import_count; i++) {
        text_append_bytes(&out->imports[i], tree->imports[i].module.text,
                          tree->imports[i].module.length);
    }
    out->import_count = tree->import_count;
done:
    token_list_free(&tokens);
    diagnostics_free(&diags);
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
    text_free(&u->path);
    text_free(&u->library);
}

void unit_order(const struct unit *units, size_t count, size_t *order)
{
    bool *done = calloc(count + 1, sizeof *done);
    size_t placed = 0;
    size_t i;
    size_t j;
    size_t k;
    bool grew = true;

    if (done == NULL) {
        out_of_memory();
    }
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

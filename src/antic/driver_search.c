/* The library files of a compilation: the files of the command line, the
   files of the imports under the search roots, the `link framework` and
   `link linux` lines they carry, and the load of each after the libraries
   it imports. */

#include "driver_parts.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "antl.h"
#include "ir.h"
#include "platform.h"
#include "sema.h"
#include "types.h"

void driver_add_path(struct paths *p, const char *path)
{
    p->items = alloc_grow(p->items, &p->capacity, p->count, sizeof *p->items);
    p->items[p->count++] = path;
}

/* The library file of module path module under the first search root that
   has one, in the memory pool, or NULL. */
static const char *search_roots(const struct options *o, const char *module,
                                struct arena *arena)
{
    struct text std_root = {0};
    size_t i;

    /* DESIGN: the standard library in std/ of the runtime archive is the
       last search root, so a program imports anti.io without -I. */
    if (o->runtime != NULL) {
        text_appendf(&std_root, "%s/%s", o->runtime, RUNTIME_STD_DIR);
    }
    for (i = 0; i < o->root_count + (o->runtime != NULL); i++) {
        struct text path = {0};
        const char *p;
        FILE *f;
        char *copy;
        text_appendf(&path, "%s/", i < o->root_count ? o->roots[i]
                                                     : text_cstr(&std_root));
        for (p = module; *p != '\0'; p++) {
            text_appendf(&path, "%c", *p == '.' ? '/' : *p);
        }
        text_append(&path, ANTL_SUFFIX);
        f = platform_open(text_cstr(&path), false);
        if (f != NULL) {
            fclose(f);
            copy = arena_alloc(arena, path.length + 1);
            memcpy(copy, text_cstr(&path), path.length + 1);
            text_free(&path);
            text_free(&std_root);
            return copy;
        }
        text_free(&path);
    }
    text_free(&std_root);
    return NULL;
}

static bool contains(const struct paths *p, const char *s)
{
    size_t i;

    for (i = 0; i < p->count; i++) {
        if (strcmp(p->items[i], s) == 0) {
            return true;
        }
    }
    return false;
}

/* The library files of the compilation. They are the files on the command
   line and, for every import that none of them holds, the file under the
   search roots. The imports of a found file are looked up too. */
/* The library files of o with the input in front, for a run whose input
   is itself a library file. The caller frees the list with free. */
const char **driver_libraries_with_input(const struct options *o)
{
    const char **list = alloc_zeroed(o->library_count + 1, sizeof *list);

    list[0] = o->input;
    /* memcpy takes no null pointer, even for no bytes, and o->libraries
       is null when no library was named. */
    if (o->library_count > 0) {
        memcpy(list + 1, o->libraries, o->library_count * sizeof *list);
    }
    return list;
}

bool driver_find_libraries(const struct options *o, const struct module *tree,
                           struct arena *arena, struct paths *out)
{
    struct paths modules = {0};
    struct paths wanted = {0};
    char error[200];
    size_t read = 0;
    size_t i;
    bool ok = true;
    bool grew = true;

    for (i = 0; i < o->library_count; i++) {
        driver_add_path(out, o->libraries[i]);
    }
    for (i = 0; i < tree->import_count; i++) {
        char *name = arena_alloc(arena, tree->imports[i].module.length + 1);
        memcpy(name, tree->imports[i].module.text,
               tree->imports[i].module.length);
        driver_add_path(&wanted, name);
    }
    while (ok && grew) {
        for (; ok && read < out->count; read++) {
            struct text bytes = {0};
            struct interface header;
            ok = driver_read_bytes(out->items[read], &bytes);
            if (ok && !antl_header((const uint8_t *)bytes.data, bytes.length,
                                   arena, &header, error, sizeof error)) {
                fprintf(stderr, "antic: %s %s\n", out->items[read], error);
                ok = false;
            }
            text_free(&bytes);
            if (ok) {
                driver_add_path(&modules, header.module);
                for (i = 0; i < header.import_count; i++) {
                    driver_add_path(&wanted, header.imports[i]);
                }
            }
        }
        grew = false;
        for (i = 0; ok && i < wanted.count; i++) {
            const char *found;
            if (contains(&modules, wanted.items[i])) {
                continue;
            }
            found = search_roots(o, wanted.items[i], arena);
            if (found != NULL && !contains(out, found)) {
                driver_add_path(out, found);
                grew = true;
            }
        }
    }
    free((void *)modules.items);
    free((void *)wanted.items);
    return ok;
}

bool driver_libraries(const struct options *options, struct arena *arena,
                      const char ***paths, size_t *count)
{
    struct module empty = {0};
    struct paths found = {0};
    const char **list;
    size_t n;
    size_t i;

    if (!driver_find_libraries(options, &empty, arena, &found)) {
        free((void *)found.items);
        return false;
    }
    n = found.count;
    list = arena_alloc(arena, (n + 1) * sizeof *list);
    for (i = 0; i < n; i++) {
        list[i] = found.items[i];
    }
    free((void *)found.items);
    *paths = list;
    *count = n;
    return true;
}

/* The names of the `link framework` lines, or with linux of the `link
   linux` lines, of the library files, each once. */
static bool link_names(const char *const *paths, size_t count,
                       struct arena *arena, const char *const **names,
                       size_t *name_count, bool linux)
{
    const char **list = NULL;
    const char **copy;
    size_t n = 0;
    size_t room = 0;
    size_t i;
    size_t j;
    size_t k;
    char error[200];

    for (i = 0; i < count; i++) {
        struct text bytes = {0};
        struct interface header;
        if (!driver_read_bytes(paths[i], &bytes) ||
            !antl_header((const uint8_t *)bytes.data, bytes.length, arena,
                         &header, error, sizeof error)) {
            if (bytes.length > 0) {
                fprintf(stderr, "antic: %s %s\n", paths[i], error);
            }
            text_free(&bytes);
            free((void *)list);
            return false;
        }
        text_free(&bytes);
        for (j = 0; j < (linux ? header.linux_library_count
                               : header.framework_count);
             j++) {
            const char *name = linux ? header.linux_libraries[j]
                                     : header.frameworks[j];
            for (k = 0; k < n; k++) {
                if (strcmp(list[k], name) == 0) {
                    break;
                }
            }
            if (k < n) {
                continue;
            }
            list = alloc_grow(list, &room, n, sizeof *list);
            list[n++] = name;
        }
    }
    copy = arena_alloc(arena, (n + 1) * sizeof *copy);
    if (n > 0) {
        memcpy(copy, list, n * sizeof *list);
    }
    *names = copy;
    *name_count = n;
    free((void *)list);
    return true;
}

bool driver_frameworks(const char *const *paths, size_t count,
                       struct arena *arena, const char *const **names,
                       size_t *name_count)
{
    return link_names(paths, count, arena, names, name_count, false);
}

bool driver_linux_libraries(const char *const *paths, size_t count,
                            struct arena *arena, const char *const **names,
                            size_t *name_count)
{
    return link_names(paths, count, arena, names, name_count, true);
}

/* Read the library files and load each after the libraries it imports,
   whatever the order on the command line. The interfaces go to out in
   load order. */
bool driver_load_libraries(const struct paths *paths, const char *module,
                           struct arena *arena, struct types *types,
                           struct ir_module *program,
                           const struct interface **out)
{
    size_t n = paths->count;
    struct text *files = alloc_zeroed(n + 1, sizeof *files);
    struct interface *headers = alloc_zeroed(n + 1, sizeof *headers);
    bool *loaded = alloc_zeroed(n + 1, sizeof *loaded);
    size_t count = 0;
    size_t i;
    size_t j;
    size_t k;
    char error[200];
    bool ok = true;

    for (i = 0; i < n && ok; i++) {
        ok = driver_read_bytes(paths->items[i], &files[i]);
        if (ok && !antl_header((const uint8_t *)files[i].data,
                               files[i].length, arena, &headers[i], error,
                               sizeof error)) {
            fprintf(stderr, "antic: %s %s\n", paths->items[i], error);
            ok = false;
        }
    }
    for (i = 0; i < n && ok; i++) {
        if (strcmp(headers[i].module, module) == 0) {
            fprintf(stderr, "antic: %s holds module `%s`, which this command "
                            "compiles\n", paths->items[i], module);
            ok = false;
        }
        for (j = 0; j < i && ok; j++) {
            if (strcmp(headers[i].module, headers[j].module) == 0) {
                fprintf(stderr, "antic: %s and %s both hold module `%s`\n",
                        paths->items[j], paths->items[i], headers[i].module);
                ok = false;
            }
        }
    }
    while (ok && count < n) {
        size_t before = count;
        for (i = 0; i < n && ok; i++) {
            bool ready = !loaded[i];
            for (j = 0; j < headers[i].import_count && ready; j++) {
                bool found = false;
                for (k = 0; k < count && !found; k++) {
                    found = strcmp(out[k]->module, headers[i].imports[j]) == 0;
                }
                ready = found;
            }
            if (!ready) {
                continue;
            }
            out[count] = antl_read((const uint8_t *)files[i].data,
                                   files[i].length, out, count, types, arena,
                                   program, error, sizeof error);
            if (out[count] == NULL) {
                fprintf(stderr, "antic: %s %s\n", paths->items[i], error);
                ok = false;
            } else {
                loaded[i] = true;
                count++;
            }
        }
        if (ok && count == before) {
            /* Report the first import that no file provides, or else the
               cycle that blocks the remaining files. */
            for (i = 0; i < n && ok; i++) {
                for (j = 0; j < headers[i].import_count && !loaded[i]; j++) {
                    bool provided = false;
                    if (strcmp(headers[i].imports[j], module) == 0) {
                        fprintf(stderr, "antic: %s depends on `%s`, so the "
                                        "import forms a cycle\n",
                                paths->items[i], module);
                        ok = false;
                        break;
                    }
                    for (k = 0; k < n && !provided; k++) {
                        provided = strcmp(headers[k].module,
                                          headers[i].imports[j]) == 0;
                    }
                    if (!provided) {
                        fprintf(stderr, "antic: %s needs module `%s`\n",
                                paths->items[i], headers[i].imports[j]);
                        ok = false;
                        break;
                    }
                }
            }
            if (ok) {
                fputs("antic: the library files import each other in a "
                      "cycle\n", stderr);
                ok = false;
            }
        }
    }
    for (i = 0; i < n; i++) {
        text_free(&files[i]);
    }
    free(files);
    free(headers);
    free(loaded);
    return ok;
}

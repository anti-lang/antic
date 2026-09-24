/* The text map of a symbols archive.

   DESIGN: the readers of src/rt/symbols.c hold every format this needs, and
   `anti.lang.StackTrace.symbolize` reads a running program with the same
   ones. The map is therefore what a trace of that program would name,
   written out once at the build. A Windows program carries no symbol
   table, because lld-link writes the symbols to the PDB that the archive
   holds beside the map. */
#include "symmap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "files.h"
#include "symbols.h"

/* One function of the map. */
struct entry {
    struct text name;
    struct text file;
    uint64_t vaddr;
    uint64_t size;
    int64_t line;
};

struct map {
    struct entry *items;
    size_t count;
    size_t capacity;
};

static bool add_function(void *context, const char *name, uint64_t vaddr,
                         uint64_t size)
{
    struct map *m = context;
    struct entry *e;

    if (m->count == m->capacity) {
        m->items = files_grow(m->items, &m->capacity, sizeof *m->items);
    }
    e = &m->items[m->count++];
    memset(e, 0, sizeof *e);
    text_append(&e->name, name);
    e->vaddr = vaddr;
    e->size = size;
    return false;
}

static int by_address(const void *a, const void *b)
{
    const struct entry *x = a;
    const struct entry *y = b;

    if (x->vaddr != y->vaddr) {
        return x->vaddr < y->vaddr ? -1 : 1;
    }
    return strcmp(text_cstr(&x->name), text_cstr(&y->name));
}

/* The file and the line of every function of an ELF program, which its
   own line table holds. */
static void elf_lines(struct map *m, const struct text *bytes)
{
    size_t i;

    for (i = 0; i < m->count; i++) {
        struct anti_found found;
        memset(&found, 0, sizeof found);
        if (anti_rt_elf_line((const uint8_t *)bytes->data, bytes->length,
                             m->items[i].vaddr, &found) &&
            found.file != NULL) {
            text_append_bytes(&m->items[i].file, found.file,
                              found.file_length);
            m->items[i].line = found.line;
        }
    }
}

/* The file and the line of every function of a Mach-O program. The link
   keeps the debug information in the objects and names them in its
   debug map, so each object is read and relocated once. */
static void macho_lines(struct map *m, const struct anti_macho_table *t)
{
    struct text object_path = {0};
    struct text object = {0};
    size_t i;

    for (i = 0; i < m->count; i++) {
        struct anti_found found;
        const char *path;
        const char *symbol;
        uint64_t start;
        memset(&found, 0, sizeof found);
        if (!anti_rt_macho_debug_map(t, m->items[i].vaddr, &path, &symbol,
                                     &start)) {
            continue;
        }
        if (strcmp(text_cstr(&object_path), path) != 0) {
            object.length = 0;
            object_path.length = 0;
            text_append(&object_path, path);
            if (!files_read_reported(path, &object)) {
                object.length = 0;
                continue;
            }
            anti_rt_macho_relocate((uint8_t *)object.data, object.length);
        }
        if (object.length > 0 &&
            anti_rt_macho_object_line((const uint8_t *)object.data,
                                      object.length, symbol,
                                      m->items[i].vaddr - start, &found) &&
            found.file != NULL) {
            text_append_bytes(&m->items[i].file, found.file,
                              found.file_length);
            m->items[i].line = found.line;
        }
    }
    text_free(&object_path);
    text_free(&object);
}

/* Sort the functions by address. A program the reader finds no
   function in leaves items NULL, which qsort may not take even with a
   count of zero. */
static void sort_map(struct map *m)
{
    if (m->count > 0) {
        qsort(m->items, m->count, sizeof *m->items, by_address);
    }
}

bool symmap_build_id(const char *program, struct text *out)
{
    static const char marker[] = "build ";
    struct text bytes = {0};
    size_t i;
    bool found = false;

    if (!files_read_reported(program, &bytes)) {
        return false;
    }
    /* The notice of every program holds the line `build <64 digits>`,
       which src/rt/license.c writes and `--anti.inspect` prints. */
    for (i = 0; !found && i + sizeof marker + 64 <= bytes.length; i++) {
        size_t j;
        if (memcmp(bytes.data + i, marker, sizeof marker - 1) != 0) {
            continue;
        }
        for (j = 0; j < 64; j++) {
            char c = bytes.data[i + sizeof marker - 1 + j];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
                break;
            }
        }
        if (j == 64) {
            text_append_bytes(out, bytes.data + i + sizeof marker - 1, 64);
            found = true;
        }
    }
    text_free(&bytes);
    if (!found) {
        fprintf(stderr, "anti: %s carries no build id\n", program);
    }
    return found;
}

bool symmap_write(const char *program, enum target t, const char *id,
                  const char *path)
{
    const struct target_info *info = target_info(t);
    struct map m = {0};
    struct text bytes = {0};
    struct text out = {0};
    struct anti_macho_table table;
    size_t i;
    bool ok;

    if (!files_read_reported(program, &bytes)) {
        return false;
    }
    if (info->format == FORMAT_ELF) {
        anti_rt_elf_functions((const uint8_t *)bytes.data, bytes.length,
                              add_function, &m);
        sort_map(&m);
        elf_lines(&m, &bytes);
    } else if (info->format == FORMAT_MACHO &&
               anti_rt_macho_table((const uint8_t *)bytes.data, bytes.length,
                                   false, 0, &table)) {
        anti_rt_macho_functions(&table, add_function, &m);
        sort_map(&m);
        macho_lines(&m, &table);
    }
    text_append(&out, "# The map of a symbols archive of Anti.\n");
    text_appendf(&out, "# build %s\n", id);
    text_appendf(&out, "# target %s\n", target_name(t));
    if (info->format == FORMAT_COFF) {
        text_append(&out, "# A Windows program carries no symbol table. The "
                          "PDB of this archive\n# holds the functions, the "
                          "files and the lines of this build.\n");
    }
    for (i = 0; i < m.count; i++) {
        const struct entry *e = &m.items[i];
        uint64_t end = e->size > 0 ? e->vaddr + e->size
                       : i + 1 < m.count ? m.items[i + 1].vaddr
                                         : e->vaddr;
        text_appendf(&out, "%016llx-%016llx %s", (unsigned long long)e->vaddr,
                     (unsigned long long)end, text_cstr(&e->name));
        if (e->file.length > 0) {
            text_appendf(&out, " %s:%lld", text_cstr(&e->file),
                         (long long)e->line);
        }
        text_append(&out, "\n");
    }
    ok = files_write(path, &out);
    for (i = 0; i < m.count; i++) {
        text_free(&m.items[i].name);
        text_free(&m.items[i].file);
    }
    free(m.items);
    text_free(&bytes);
    text_free(&out);
    return ok;
}

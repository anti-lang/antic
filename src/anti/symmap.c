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

#include "antic.h"
#include "files.h"
#include "license.h"
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
    size_t length = strlen(name);
    char *readable;
    size_t unescaped;

    if (m->count == m->capacity) {
        m->items = files_grow(m->items, &m->capacity, sizeof *m->items);
    }
    e = &m->items[m->count++];
    memset(e, 0, sizeof *e);
    /* The map names a function as a trace of the program would, with
       the escapes of its symbol read back. */
    readable = malloc(length + 1);
    unescaped = readable != NULL
                    ? anti_rt_symbol_unescape(name, length, readable, length)
                    : 0;
    text_append_bytes(&e->name, unescaped > 0 ? readable : name,
                      unescaped > 0 ? unescaped : length);
    free(readable);
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

static const char notice_head[] = ANTI_NOTICE_BEGIN ANTI_NOTICE_BUILD;

/* The begin marker of the notice in bytes, where the line of a build id
   follows it, or NULL when the bytes hold none. */
static const char *find_notice(const struct text *bytes)
{
    size_t marker = sizeof notice_head - 1;
    size_t i;

    for (i = 0; i + marker + ANTI_BUILD_ID_LENGTH < bytes->length; i++) {
        const char *at = bytes->data + i;
        size_t j;
        if (memcmp(at, notice_head, marker) != 0) {
            continue;
        }
        for (j = 0; j < ANTI_BUILD_ID_LENGTH; j++) {
            char c = at[marker + j];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
                break;
            }
        }
        if (j == ANTI_BUILD_ID_LENGTH &&
            at[marker + ANTI_BUILD_ID_LENGTH] == '\n') {
            return at;
        }
    }
    return NULL;
}

bool symmap_license(const struct text *bytes, struct text *out)
{
    static const char end_marker[] = ANTI_NOTICE_END;
    const char *at = find_notice(bytes);
    const char *from;
    const char *end;
    const char *to;

    if (at == NULL) {
        return false;
    }
    from = at + sizeof notice_head - 1 + ANTI_BUILD_ID_LENGTH + 1;
    end = bytes->data + bytes->length;
    for (to = from; to + sizeof end_marker - 1 <= end; to++) {
        if (memcmp(to, end_marker, sizeof end_marker - 1) == 0) {
            text_append_bytes(out, from, (size_t)(to - from));
            return true;
        }
    }
    return false;
}

bool symmap_notice(const struct text *bytes, struct text *id,
                   struct text *version)
{
    size_t marker = sizeof notice_head - 1;
    const char *at = find_notice(bytes);
    const char *end = bytes->data + bytes->length;
    const char *line;

    if (at != NULL) {
        text_append_bytes(id, at + marker, ANTI_BUILD_ID_LENGTH);
        line = at + marker + ANTI_BUILD_ID_LENGTH + 1;
        while (line < end && end - line > 8 &&
               memcmp(line, "package ", 8) == 0) {
            const char *stop = memchr(line, '\n', (size_t)(end - line));
            const char *word;
            const char *after;
            if (stop == NULL) {
                break;
            }
            word = memchr(line + 8, ' ', (size_t)(stop - line - 8));
            after = word != NULL
                        ? memchr(word + 1, ' ', (size_t)(stop - word - 1))
                        : NULL;
            if (word != NULL) {
                version->length = 0;
                text_append_bytes(version, word + 1,
                                  (size_t)((after != NULL ? after : stop) -
                                           word - 1));
            }
            line = stop + 1;
            while (line < end && end - line > 12 &&
                   memcmp(line, "attribution ", 12) == 0) {
                stop = memchr(line, '\n', (size_t)(end - line));
                line = stop != NULL ? stop + 1 : end;
            }
        }
        return true;
    }
    return false;
}

bool symmap_license_of(const char *binary, struct text *out)
{
    struct text bytes = {0};
    bool found;

    if (!files_read_reported(binary, &bytes)) {
        return false;
    }
    found = symmap_license(&bytes, out);
    text_free(&bytes);
    if (!found) {
        fprintf(stderr, "anti: %s carries no licence notice\n", binary);
    }
    return found;
}

/* The line of the record in sources that names the component of name,
   without its newline, with its length in length, or NULL. A line of the
   record is `<name> <version> <url>`, and one that starts with # is a
   comment. */
static const char *source_line(const struct text *sources, const char *name,
                               size_t name_length, size_t *length)
{
    const char *at = sources->data;
    const char *end = at + sources->length;

    while (at < end) {
        const char *stop = memchr(at, '\n', (size_t)(end - at));
        size_t line_length = (size_t)((stop != NULL ? stop : end) - at);
        if (line_length > name_length && at[0] != '#' &&
            at[name_length] == ' ' && memcmp(at, name, name_length) == 0) {
            *length = line_length;
            return at;
        }
        at = stop != NULL ? stop + 1 : end;
    }
    return NULL;
}

bool symmap_source_version(const struct text *sources, const char *name,
                           struct text *out)
{
    size_t name_length = strlen(name);
    size_t length = 0;
    const char *line = source_line(sources, name, name_length, &length);
    const char *version;
    const char *stop;

    if (line == NULL) {
        return false;
    }
    version = line + name_length + 1;
    stop = memchr(version, ' ', length - name_length - 1);
    text_append_bytes(out, version,
                      stop != NULL ? (size_t)(stop - version)
                                   : length - name_length - 1);
    return true;
}

/* Append to out a `source` line for each of the names, the names of a
   `text for` line separated by spaces, that the record in sources
   names. */
static void append_sources(struct text *out, const struct text *sources,
                           const char *names, size_t names_length)
{
    const char *at = names;
    const char *end;

    if (names == NULL) {
        return;
    }
    end = names + names_length;
    while (at < end) {
        const char *space = memchr(at, ' ', (size_t)(end - at));
        const char *stop = space != NULL ? space : end;
        size_t length = 0;
        const char *line =
            source_line(sources, at, (size_t)(stop - at), &length);
        if (line != NULL) {
            text_append(out, "source ");
            text_append_bytes(out, line, length);
            text_append(out, "\n");
        }
        if (space == NULL) {
            break;
        }
        at = space + 1;
    }
}

bool symmap_license_sources(struct text *notice, const char *runtime)
{
    static const char head[] = "text for ";
    struct text path = {0};
    struct text sources = {0};
    struct text out = {0};
    const char *names = NULL;
    size_t names_length = 0;
    bool ok;

    text_appendf(&path, "%s/%s/%s", runtime, RUNTIME_LICENSES_DIR,
                 RUNTIME_SOURCES_FILE);
    ok = files_read_reported(text_cstr(&path), &sources);
    if (ok) {
        const char *at = notice->data;
        const char *end = at + notice->length;
        while (at < end) {
            const char *stop = memchr(at, '\n', (size_t)(end - at));
            const char *next = stop != NULL ? stop + 1 : end;
            size_t line_length = (size_t)((stop != NULL ? stop : end) - at);
            if (line_length > sizeof head - 1 &&
                memcmp(at, head, sizeof head - 1) == 0) {
                append_sources(&out, &sources, names, names_length);
                names = at + sizeof head - 1;
                names_length = line_length - (sizeof head - 1);
            }
            text_append_bytes(&out, at, (size_t)(next - at));
            at = next;
        }
        append_sources(&out, &sources, names, names_length);
        notice->length = 0;
        if (out.length > 0) {
            text_append_bytes(notice, out.data, out.length);
        }
    }
    text_free(&path);
    text_free(&sources);
    text_free(&out);
    return ok;
}

bool symmap_build_id(const char *program, struct text *out)
{
    struct text bytes = {0};
    struct text version = {0};
    bool found;

    if (!files_read_reported(program, &bytes)) {
        return false;
    }
    found = symmap_notice(&bytes, out, &version);
    text_free(&bytes);
    text_free(&version);
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
    text_appendf(&out, "%s%s\n", SYMMAP_BUILD_LINE, id);
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

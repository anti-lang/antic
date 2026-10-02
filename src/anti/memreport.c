/* The run of a program of --memory-checks whose report names its frames
   with Anti's symbolizer. */
#include "memreport.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cursor.h"
#include "files.h"
#include "platform.h"
#include "syms.h"
#include "text.h"

/* The variable AddressSanitizer reads its options from, and the option
   that leaves the frames of a report to Anti's symbolizer. */
#define SANITIZER_OPTIONS "ASAN_OPTIONS"
#define NO_SYMBOLIZE "symbolize=0"

/* One module a report names: its path and processor as `path:arch`, and
   its bytes, none when the read failed. */
struct module {
    struct text key;
    struct text file;
};

/* The modules a report names, each read once. */
struct report {
    struct module *items;
    size_t count;
    size_t room;
};

static uint32_t big_endian(const unsigned char *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 |
           (uint32_t)p[3];
}

/* The processor types of Mach-O that a frame of macOS names after the
   path of its module. */
struct slice_arch {
    const char *name;
    uint32_t cpu;
    uint32_t sub;
};

static const struct slice_arch slice_archs[] = {
    {"arm64", 0x0100000c, 0},
    {"arm64e", 0x0100000c, 2},
    {"x86_64", 0x01000007, 3},
    {"x86_64h", 0x01000007, 8},
};

/* DESIGN: macOS ships its own libraries and the runtime of
   AddressSanitizer as universal files, one Mach-O file per processor
   behind a header of their offsets. The frame names the processor, and
   the reader takes that file alone. A file of one processor stays as it
   is. The header and each entry are big-endian words. Returns false for
   a universal file without the processor. */
static bool thin_slice(struct text *file, const char *arch, size_t length)
{
    const unsigned char *bytes = (const unsigned char *)file->data;
    const struct slice_arch *want = NULL;
    uint32_t count;
    uint32_t i;
    size_t k;

    if (file->length < 8 || big_endian(bytes) != 0xcafebabe) {
        return true;
    }
    for (k = 0; k < sizeof slice_archs / sizeof slice_archs[0]; k++) {
        if (strlen(slice_archs[k].name) == length &&
            memcmp(slice_archs[k].name, arch, length) == 0) {
            want = &slice_archs[k];
        }
    }
    count = big_endian(bytes + 4);
    for (i = 0; want != NULL && i < count && 8 + 20 * (size_t)(i + 1) <=
                                                 file->length;
         i++) {
        const unsigned char *entry = bytes + 8 + 20 * (size_t)i;
        size_t offset = big_endian(entry + 8);
        size_t size = big_endian(entry + 12);
        if (big_endian(entry) != want->cpu ||
            (big_endian(entry + 4) & 0x00ffffff) != want->sub ||
            offset > file->length || size > file->length - offset) {
            continue;
        }
        memmove(file->data, file->data + offset, size);
        file->length = size;
        return true;
    }
    return false;
}

/* The module at the length bytes of path, and in a universal file the
   one of the arch_length bytes of arch. */
static const struct text *report_module(struct report *r, const char *path,
                                        size_t length, const char *arch,
                                        size_t arch_length)
{
    struct module *m;
    size_t i;

    for (i = 0; i < r->count; i++) {
        const struct text *key = &r->items[i].key;
        if (key->length == length + 1 + arch_length &&
            memcmp(key->data, path, length) == 0 &&
            memcmp(key->data + length + 1, arch, arch_length) == 0) {
            return &r->items[i].file;
        }
    }
    if (r->count == r->room) {
        r->items = files_grow(r->items, &r->room, sizeof *r->items);
    }
    m = &r->items[r->count++];
    text_append_bytes(&m->key, path, length);
    if (!files_read(text_cstr(&m->key), &m->file) ||
        !thin_slice(&m->file, arch, arch_length)) {
        m->file.length = 0;
    }
    text_append(&m->key, ":");
    text_append_bytes(&m->key, arch, arch_length);
    return &m->file;
}

/* DESIGN: `anti run` and `anti test` run a program of --memory-checks
   with symbolize=0, so each frame of its report names its module and the
   offset in it: `#3 0x... (/path/prog:arm64+0x100000a10)` on macOS and
   `(/path/prog+0xa10)` on Linux. A frame whose module Anti's symbolizer
   reads gets `in <function> <file>:<line>` in place of the module, the
   form of the runtime's own symbolizer. Every other line comes out as
   it went in. The runtime already printed the instruction before the
   return address, so the offset is looked up as it stands. */
static void report_line(void *context, const char *line, size_t length)
{
    struct report *r = context;
    size_t shown = length;
    const char *stop;
    const char *plus = NULL;
    const char *colon = NULL;
    const char *p;
    struct cursor c;
    struct text function = {0};
    struct text where = {0};
    uint64_t number;
    uint64_t offset;
    const struct text *module;

    while (shown > 0 && (line[shown - 1] == '\n' || line[shown - 1] == '\r')) {
        shown--;
    }
    c.at = line;
    c.end = line + shown;
    cursor_blank(&c);
    if (!cursor_char(&c, '#') || !cursor_number(&c, 10, &number) ||
        !cursor_blank(&c) || !cursor_hex(&c, &number)) {
        fwrite(line, 1, length, stderr);
        return;
    }
    stop = c.at;
    cursor_blank(&c);
    if (!cursor_char(&c, '(') || c.end - c.at < 5 || c.end[-1] != ')') {
        fwrite(line, 1, length, stderr);
        return;
    }
    for (p = c.at; p + 3 <= c.end - 1; p++) {
        if (memcmp(p, "+0x", 3) == 0) {
            plus = p;
        }
    }
    for (p = c.at; plus != NULL && p < plus; p++) {
        if (*p == ':') {
            colon = p;
        } else if (*p == '/') {
            colon = NULL;
        }
    }
    if (plus != NULL) {
        struct cursor o;
        o.at = plus + 1;
        o.end = c.end - 1;
        module = report_module(
            r, c.at, (size_t)((colon != NULL ? colon : plus) - c.at),
            colon != NULL ? colon + 1 : plus,
            colon != NULL ? (size_t)(plus - colon - 1) : 0);
        if (cursor_hex(&o, &offset) && o.at == o.end && module->length > 0 &&
            syms_resolve_binary(module, offset, &function, &where) &&
            function.length > 0) {
            fwrite(line, 1, (size_t)(stop - line), stderr);
            fprintf(stderr, " in %s", text_cstr(&function));
            if (where.length > 0) {
                fprintf(stderr, " %s\n", text_cstr(&where));
            } else {
                fputc(' ', stderr);
                fwrite(c.at - 1, 1, (size_t)(c.end - c.at + 1), stderr);
                fputc('\n', stderr);
            }
            text_free(&function);
            text_free(&where);
            return;
        }
    }
    text_free(&function);
    text_free(&where);
    fwrite(line, 1, length, stderr);
}

/* Windows keeps the symbolizing of AddressSanitizer, as the DESIGN of
   platform_run_symbolized in platform.h says. */
int memreport_run(const char *const argv[])
{
    struct report r = {0};
    struct text value = {0};
    const char *before = platform_getenv(SANITIZER_OPTIONS);
    int status;
    size_t i;

    if (before != NULL && before[0] != '\0') {
        text_appendf(&value, "%s:", before);
    }
    text_append(&value, NO_SYMBOLIZE);
    status = platform_run_symbolized(argv, SANITIZER_OPTIONS,
                                     text_cstr(&value), report_line, &r);
    for (i = 0; i < r.count; i++) {
        text_free(&r.items[i].key);
        text_free(&r.items[i].file);
    }
    free(r.items);
    text_free(&value);
    return status;
}

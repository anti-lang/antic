/* The capture, the modules and the symbols of a stack trace.

   DESIGN: a trace is taken in two steps. The capture walks the frames.
   Per frame it records the module that holds the address, the build id
   of that module and its load base. It reads nothing from a file.
   Symbolising reads the symbol table and the line table. It runs only
   when a program asks for the names, so an error that is handled costs
   the walk alone. The walk and the module of an address are calls of
   the platform layer, and the format of the module chooses the reader
   of its tables here. */
#include "trace.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "grow.h"
#include "license.h"
#include "platform.h"
#include "rt.h"
#include "symbols.h"

static struct anti_text text_of(const char *s)
{
    struct anti_text t;

    t.ptr = (const unsigned char *)(s != NULL ? s : "");
    t.len = s != NULL ? (int64_t)strlen(s) : 0;
    return t;
}

static struct anti_text text_part(const char *s, size_t n)
{
    struct anti_text t;

    t.ptr = (const unsigned char *)(s != NULL ? s : "");
    t.len = s != NULL ? (int64_t)n : 0;
    return t;
}

/* The texts a lookup makes, kept for the life of the program so a frame
   may point at them. Equal texts are kept once. The paths of the modules
   stand in a list of their own, which every capture searches, and the
   names and files that symbolising makes in another. Both are guarded by
   ANTI_RT_LOCK_TRACE_TEXTS. */
struct kept {
    struct kept *next;
    size_t length;
    char bytes[1];
};

static struct kept *kept_names;
static struct kept *kept_paths;

static struct anti_text keep_in(struct kept **list, const char *s, size_t n)
{
    struct kept *k;

    anti_rt_lock_hold(ANTI_RT_LOCK_TRACE_TEXTS);
    for (k = *list; k != NULL; k = k->next) {
        if (k->length == n && memcmp(k->bytes, s, n) == 0) {
            break;
        }
    }
    if (k == NULL) {
        k = malloc(sizeof *k + n);
        if (k != NULL) {
            memcpy(k->bytes, s, n);
            k->bytes[n] = 0;
            k->length = n;
            k->next = *list;
            *list = k;
        }
    }
    anti_rt_lock_release(ANTI_RT_LOCK_TRACE_TEXTS);
    return k != NULL ? text_part(k->bytes, k->length) : text_of(NULL);
}

static struct anti_text keep(const char *s, size_t n)
{
    return keep_in(&kept_names, s, n);
}

/* The name of a function as a person reads it, from the n bytes of its
   symbol at s: `List<int>.push` of the symbol `List$3cint$3e.push`. A
   name without an escape stays where it is when stable says s outlives
   the frame, and is kept otherwise. */
static struct anti_text function_name(const char *s, size_t n, bool stable)
{
    char *readable;
    size_t length;
    struct anti_text t;

    if (s == NULL || memchr(s, '$', n) == NULL) {
        return stable ? text_part(s, n) : keep(s, n);
    }
    readable = malloc(n + 1);
    if (readable == NULL) {
        return stable ? text_part(s, n) : keep(s, n);
    }
    length = anti_rt_symbol_unescape(s, n, readable, n);
    t = length > 0 ? keep(readable, length)
        : stable   ? text_part(s, n)
                   : keep(s, n);
    free(readable);
    return t;
}

/* The build id of a notice, the digits of its line `build <id>` after
   the begin marker, or an empty text. No byte at or past room is read,
   and a notice of this program or of a table in memory passes
   SIZE_MAX. */
static struct anti_text notice_id(const char *notice, size_t room)
{
    static const char head[] = ANTI_NOTICE_BEGIN ANTI_NOTICE_BUILD;
    size_t at = sizeof head - 1;

    if (notice == NULL || room < at || strncmp(notice, head, at) != 0) {
        return text_of(NULL);
    }
    while (at < room && notice[at] != '\n' && notice[at] != 0) {
        at++;
    }
    if (at == room || notice[at] != '\n') {
        return text_of(NULL);
    }
    return text_part(notice + sizeof head - 1, at - (sizeof head - 1));
}

/* DESIGN: the files a symbol lookup reads stay in memory for the life of
   the program. They are the binaries of the modules on Linux and the
   objects of the debug map on macOS, a few of each. A second trace would
   read them again for nothing. ANTI_RT_LOCK_TRACE_FILES guards them and
   the build ids of the Mach-O images below. */
struct loaded {
    char *path;
    uint8_t *bytes;
    size_t size;
};

enum { LOADED_MAX = 32 };

static struct loaded files[LOADED_MAX];
static size_t file_count;

/* The bytes of the file at path, read whole, or NULL. The relocations of
   a Mach-O object are resolved once, as it is read. */
static const struct loaded *load(const char *path, bool object)
{
    const struct loaded *found = NULL;
    size_t i;

    anti_rt_lock_hold(ANTI_RT_LOCK_TRACE_FILES);
    for (i = 0; i < file_count; i++) {
        if (strcmp(files[i].path, path) == 0) {
            found = files[i].bytes != NULL ? &files[i] : NULL;
            anti_rt_lock_release(ANTI_RT_LOCK_TRACE_FILES);
            return found;
        }
    }
    if (file_count < LOADED_MAX) {
        struct loaded *l = &files[file_count];
        int64_t size = 0;
        l->path = malloc(strlen(path) + 1);
        if (l->path != NULL) {
            memcpy(l->path, path, strlen(path) + 1);
            file_count++;
            /* A file that cannot be read whole leaves the entry without
               bytes, so it is not tried again. */
            l->bytes = anti_rt_fs_read(path, &size);
            l->size = l->bytes != NULL ? (size_t)size : 0;
        }
        if (l->bytes != NULL && object) {
            anti_rt_macho_relocate(l->bytes, l->size);
        }
        found = l->path != NULL && l->bytes != NULL ? l : NULL;
    }
    anti_rt_lock_release(ANTI_RT_LOCK_TRACE_FILES);
    return found;
}

/* The distance between where the image lies and where it was linked,
   which is its header against the address of its __TEXT segment. 0 where
   the header names no __TEXT. */
static intptr_t image_slide(const uint8_t *header)
{
    uint64_t vmaddr;

    if (!anti_rt_macho_text(header, SIZE_MAX, &vmaddr)) {
        return 0;
    }
    return (intptr_t)((uintptr_t)header - (uintptr_t)vmaddr);
}

/* The build ids the capture found, by the header of each image, so the
   symbol table of an image is read once. */
struct known_id {
    const uint8_t *header;
    struct anti_text id;
};

enum { KNOWN_MAX = 32 };

static struct known_id known[KNOWN_MAX];
static size_t known_count;

/* The build id of the Mach-O image at header, read by the symbol
   `anti_licenses`, which only an Anti binary has. An image of the system
   cache has none, and its table is not read. */
static struct anti_text macho_id(const uint8_t *header)
{
    struct anti_macho_table t;
    struct anti_text id = text_of(NULL);
    uint32_t flags;
    uint64_t vaddr;
    intptr_t slide;
    size_t i;

    anti_rt_lock_hold(ANTI_RT_LOCK_TRACE_FILES);
    for (i = 0; i < known_count; i++) {
        if (known[i].header == header) {
            id = known[i].id;
            anti_rt_lock_release(ANTI_RT_LOCK_TRACE_FILES);
            return id;
        }
    }
    memcpy(&flags, header + 24, 4);
    slide = image_slide(header);
    if ((flags & 0x80000000u) == 0 &&
        anti_rt_macho_table(header, SIZE_MAX, true, slide, &t) &&
        anti_rt_macho_symbol(&t, "anti_licenses", &vaddr)) {
        id = notice_id((const char *)(uintptr_t)(vaddr + (uint64_t)slide),
                       SIZE_MAX);
    }
    if (known_count < KNOWN_MAX) {
        known[known_count].header = header;
        known[known_count].id = id;
        known_count++;
    }
    anti_rt_lock_release(ANTI_RT_LOCK_TRACE_FILES);
    return id;
}

/* The build id of an ELF module, read by the symbol `anti_licenses` of
   its file, which only an Anti binary has. The file on disk may no
   longer be the one mapped, so the notice is read only inside a loaded
   segment of the module. */
static struct anti_text elf_id(const struct anti_rt_module *m)
{
    const struct loaded *l = load(m->path, false);
    uint64_t vaddr;
    uint64_t room;

    if (l == NULL || !anti_rt_elf_symbol(l->bytes, l->size, "anti_licenses",
                                         &vaddr)) {
        return text_of(NULL);
    }
    room = anti_rt_elf_loaded_room((const uint8_t *)m->headers,
                                   m->header_count, vaddr);
    if (room == 0) {
        return text_of(NULL);
    }
    return notice_id((const char *)(uintptr_t)(m->base + vaddr),
                     room < SIZE_MAX ? (size_t)room : SIZE_MAX);
}

/* The build id of the module, from the notice the platform layer found
   or from the reader of its format. */
static struct anti_text module_id(const struct anti_rt_module *m)
{
    if (m->notice_read) {
        return notice_id(m->notice, SIZE_MAX);
    }
    switch (m->format) {
    case ANTI_RT_IMAGE_MACHO:
        return macho_id((const uint8_t *)(uintptr_t)m->base);
    case ANTI_RT_IMAGE_ELF:
        return elf_id(m);
    case ANTI_RT_IMAGE_PE:
        break;
    }
    return text_of(NULL);
}

void anti_rt_trace_frame(uint64_t address, struct anti_raw_frame *out)
{
    struct anti_rt_module m;

    memset(out, 0, sizeof *out);
    out->address = address;
    out->module = text_of(NULL);
    out->build_id = text_of(NULL);
    /* A return address follows the call, so the module is the one of the
       byte before it, which is the call. */
    if (address == 0 || !anti_rt_module_at(address - 1, &m)) {
        return;
    }
    out->module = keep_in(&kept_paths, m.path, strlen(m.path));
    out->base = m.base;
    out->build_id = module_id(&m);
}

/* The function and the line of address in the Mach-O image at header,
   from its symbol table in memory and the objects of its debug map. */
static void symbolize_macho(const uint8_t *header, uint64_t address,
                            struct anti_frame *out)
{
    struct anti_macho_table t;
    struct anti_found found;
    const char *object;
    const char *symbol;
    uint64_t start;
    uint64_t vaddr;
    intptr_t slide = image_slide(header);

    memset(&found, 0, sizeof found);
    if (!anti_rt_macho_table(header, SIZE_MAX, true, slide, &t)) {
        return;
    }
    vaddr = address - (uint64_t)slide;
    if (anti_rt_macho_function(&t, vaddr, &found)) {
        out->function = function_name(found.function, found.function_length,
                                       true);
    }
    if (anti_rt_macho_debug_map(&t, vaddr, &object, &symbol, &start)) {
        const struct loaded *l = load(object, true);
        if (l != NULL && anti_rt_macho_object_line(l->bytes, l->size, symbol,
                                                   vaddr - start, &found) &&
            found.file != NULL) {
            out->file = text_part(found.file, found.file_length);
            out->line = found.line;
        }
    }
}

/* The function and the line of address in the ELF module, from the
   tables of its file. */
static void symbolize_elf(const struct anti_rt_module *m, uint64_t address,
                          struct anti_frame *out)
{
    struct anti_found found;
    const struct loaded *l = load(m->path, false);
    uint64_t vaddr = address - m->base;

    memset(&found, 0, sizeof found);
    if (l == NULL) {
        return;
    }
    if (anti_rt_elf_function(l->bytes, l->size, vaddr, &found)) {
        out->function = function_name(found.function, found.function_length,
                                       true);
    }
    if (anti_rt_elf_line(l->bytes, l->size, vaddr, &found) &&
        found.file != NULL) {
        out->file = text_part(found.file, found.file_length);
        out->line = found.line;
    }
}

/* The function and the line of address from the debugger library of the
   system, which reads the PDB. A name of C++ is demangled. */
static void symbolize_pe(const struct anti_rt_module *m, uint64_t address,
                         struct anti_frame *out)
{
    struct anti_rt_debug_answer answer;
    char name[ANTI_RT_SYMBOL_ROOM];

    anti_rt_debug_lookup(address, m->base, &answer);
    if (answer.function[0] != '\0') {
        size_t n = strlen(answer.function);
        size_t named =
            anti_rt_coff_demangle(answer.function, n, name, sizeof name);
        out->function = named > 0 ? function_name(name, named, false)
                                  : function_name(answer.function, n, false);
    }
    if (answer.file[0] != '\0') {
        out->file = keep(answer.file, strlen(answer.file));
        out->line = answer.line;
    }
}

void anti_rt_trace_symbolize(const struct anti_raw_frame *frame,
                             struct anti_frame *out)
{
    struct anti_rt_module m;
    uint64_t address = frame->address - 1;

    memset(out, 0, sizeof *out);
    out->address = frame->address;
    out->function = text_of(NULL);
    out->file = text_of(NULL);
    /* A return address follows the call, so the lookup takes the byte
       before it, which is the call and names its line. */
    if (frame->address == 0 || !anti_rt_module_at(address, &m)) {
        return;
    }
    switch (m.format) {
    case ANTI_RT_IMAGE_MACHO:
        symbolize_macho((const uint8_t *)(uintptr_t)m.base, address, out);
        break;
    case ANTI_RT_IMAGE_ELF:
        symbolize_elf(&m, address, out);
        break;
    case ANTI_RT_IMAGE_PE:
        symbolize_pe(&m, address, out);
        break;
    }
}

/* A text that grows, for the text of a trace. It turns NULL when the
   memory runs out. */
struct growing {
    unsigned char *bytes;
    int64_t used;
    int64_t room;
};

/* The room of one formatted line of the text of a trace. */
enum { LINE_ROOM = 128 };

static void add(struct growing *g, const void *bytes, int64_t n)
{
    unsigned char *more;

    if (g->bytes == NULL) {
        return;
    }
    more = anti_rt_reserve(g->bytes, &g->room, g->used + 1, n, 256);
    if (more == NULL) {
        free(g->bytes);
        g->bytes = NULL;
        return;
    }
    g->bytes = more;
    memcpy(g->bytes + g->used, bytes, (size_t)n);
    g->used += n;
    g->bytes[g->used] = 0;
}

/* Add the line snprintf wrote when it gave n, or give the bytes back
   and 0 when it failed or cut the line short. */
static int add_line(struct growing *g, const char *line, int n)
{
    if (n < 0 || (size_t)n >= LINE_ROOM) {
        free(g->bytes);
        g->bytes = NULL;
        return 0;
    }
    add(g, line, n);
    return 1;
}

/* The number of the module of frame i: the count of distinct modules
   that frames before it reach first, or -1 outside every module. */
static int64_t module_number(const struct anti_raw_frame *frames, int64_t i)
{
    int64_t seen = 0;
    int64_t j;
    int64_t k;

    if (frames[i].base == 0) {
        return -1;
    }
    for (j = 0; j <= i; j++) {
        if (frames[j].base == 0) {
            continue;
        }
        for (k = 0; k < j && frames[k].base != frames[j].base; k++) {
        }
        if (k < j) {
            continue;
        }
        if (frames[j].base == frames[i].base) {
            return seen;
        }
        seen++;
    }
    return -1;
}

/* DESIGN: the text of a trace names every module once, in the order the
   frames reach them, as `module <n> <build id> <base> <path>`. Each frame
   follows as its address and `<n>+<offset>` into its module, or `-`
   outside every module. That is the raw form a resolver matches to the
   symbols of a build by the id. */
unsigned char *anti_rt_trace_text(const struct anti_raw_frame *frames,
                                  int64_t count)
{
    struct growing g;
    char line[LINE_ROOM];
    int64_t modules = 0;
    int64_t i;
    int n;

    g.room = 256;
    g.used = 0;
    g.bytes = malloc((size_t)g.room);
    if (g.bytes == NULL) {
        return NULL;
    }
    g.bytes[0] = 0;
    for (i = 0; i < count; i++) {
        const struct anti_raw_frame *f = &frames[i];
        if (module_number(frames, i) != modules) {
            continue;
        }
        n = snprintf(line, sizeof line, "%smodule %" PRId64 " ",
                     g.used > 0 ? "\n" : "", modules++);
        if (!add_line(&g, line, n)) {
            return NULL;
        }
        if (f->build_id.len > 0) {
            add(&g, f->build_id.ptr, f->build_id.len);
        } else {
            add(&g, "-", 1);
        }
        n = snprintf(line, sizeof line, " 0x%016" PRIx64 " ", f->base);
        if (!add_line(&g, line, n)) {
            return NULL;
        }
        add(&g, f->module.ptr, f->module.len);
    }
    for (i = 0; i < count; i++) {
        const struct anti_raw_frame *f = &frames[i];
        int64_t number = module_number(frames, i);
        if (number < 0) {
            n = snprintf(line, sizeof line, "%s0x%016" PRIx64 " -",
                         g.used > 0 ? "\n" : "", f->address);
        } else {
            n = snprintf(line, sizeof line,
                         "%s0x%016" PRIx64 " %" PRId64 "+0x%" PRIx64,
                         g.used > 0 ? "\n" : "", f->address, number,
                         f->address - f->base);
        }
        if (!add_line(&g, line, n)) {
            return NULL;
        }
    }
    return g.bytes;
}

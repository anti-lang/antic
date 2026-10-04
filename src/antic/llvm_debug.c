#include "llvm_debug.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"

bool llvm_debug_wanted(enum target t, bool lines)
{
    return lines || target_info(t)->format == FORMAT_COFF;
}

void debug_spans_free(struct debug_spans *s)
{
    free(s->items);
    memset(s, 0, sizeof *s);
}

void llvm_debug_mark(struct debug_spans *s, size_t start, size_t end)
{
    if (s == NULL || end <= start) {
        return;
    }
    if (s->count > 0 && s->items[s->count - 1].end == start) {
        s->items[s->count - 1].end = end;
        return;
    }
    s->items = alloc_grow(s->items, &s->capacity, s->count, sizeof *s->items);
    s->items[s->count].start = start;
    s->items[s->count].end = end;
    s->count++;
}

/* Append s as a metadata string in quotes. A quote, a backslash and a
   byte outside printable ASCII take the escape \XX. */
static void quoted(struct text *out, const char *s)
{
    text_append(out, "\"");
    for (; *s != '\0'; s++) {
        unsigned char c = (unsigned char)*s;
        if (c < 0x20 || c >= 0x7f || c == '"' || c == '\\') {
            text_appendf(out, "\\%02X", c);
        } else {
            text_append_bytes(out, s, 1);
        }
    }
    text_append(out, "\"");
}

/* The file of the module that the text compiles, which names the
   compile unit. */
static const char *unit_file(const struct llvm_debug *d)
{
    size_t i;

    for (i = 0; i < d->m->function_count; i++) {
        const struct ir_function *f = d->m->functions[i];
        if (!f->is_extern && f->file != IR_NO_INDEX && f->module != NULL &&
            d->module != NULL && strcmp(f->module, d->module) == 0) {
            return d->m->files[f->file];
        }
    }
    return d->m->file_count > 0 ? d->m->files[0] : d->m->name;
}

/* The id of the DIFile of the compile unit: the one of its source file,
   or the one after the files for a text of no source. */
static uint32_t unit_file_id(const struct llvm_debug *d)
{
    const char *name = unit_file(d);
    size_t i;

    for (i = 0; i < d->m->file_count; i++) {
        if (strcmp(d->m->files[i], name) == 0) {
            return d->files + (uint32_t)i;
        }
    }
    return d->files + (uint32_t)d->m->file_count;
}

/* DESIGN: the compile unit is FullDebug, though it describes lines alone.
   Under LineTablesOnly llc writes no DWARF subprogram for a function that
   inlines nothing, so gdb on Linux named a frame by its symbol,
   app.twice$3cint$3e, where "Debug information" of
   docs/work-order-llvm-back-end.md keeps one subprogram per function. The
   nodes stay the same: no variable, type or parameter is described. */
void llvm_debug_init(struct llvm_debug *d, enum target t,
                     const struct ir_module *m, const char *module,
                     bool lines, bool optimized, uint32_t first,
                     struct debug_spans *spans)
{
    size_t i;

    memset(d, 0, sizeof *d);
    d->target = t;
    d->m = m;
    d->module = module;
    d->on = llvm_debug_wanted(t, lines);
    d->lines = lines;
    d->optimized = optimized;
    d->spans = spans;
    if (!d->on) {
        return;
    }
    d->next = first;
    d->flags = d->next;
    d->flag_count = target_info(t)->format == FORMAT_COFF ? 2 : 1;
    d->next += d->flag_count;
    d->unit = d->next++;
    d->files = d->next;
    d->next += (uint32_t)m->file_count + 1;
    d->type = d->next++;
    text_appendf(&d->nodes, "!%" PRIu32 " = distinct !DICompileUnit("
                            "language: DW_LANG_C11, file: !%" PRIu32
                            ", producer: \"antic %s\", isOptimized: %s, "
                            "runtimeVersion: 0, emissionKind: "
                            "FullDebug)\n",
                 d->unit, unit_file_id(d), ANTIC_VERSION,
                 optimized ? "true" : "false");
    /* The id after the files names the file of a unit of no source
       alone. */
    for (i = 0; i <= m->file_count; i++) {
        if (i == m->file_count &&
            unit_file_id(d) != d->files + (uint32_t)i) {
            break;
        }
        text_appendf(&d->nodes, "!%" PRIu32 " = !DIFile(filename: ",
                     d->files + (uint32_t)i);
        quoted(&d->nodes, i < m->file_count ? m->files[i] : unit_file(d));
        text_append(&d->nodes, ", directory: \"\")\n");
    }
    text_appendf(&d->nodes, "!%" PRIu32 " = !DISubroutineType(types: !%"
                            PRIu32 ")\n!%" PRIu32 " = !{null}\n",
                 d->type, d->next, d->next);
    d->next++;
}

void llvm_debug_free(struct llvm_debug *d)
{
    text_free(&d->nodes);
}

/* The name a reader gives f: its module path, a dot and its name, or the
   C name of an export fn and of a function of the runtime. */
static void reader_name(struct text *out, const struct ir_function *f)
{
    if (f->module == NULL || f->exported) {
        target_c_symbol(out, TARGET_LINUX_X86_64, f->name);
    } else {
        text_appendf(out, "%s.%s", f->module, f->name);
    }
}

void llvm_debug_open(struct llvm_debug *d, struct text *out,
                     const struct ir_function *f, bool local)
{
    struct text name = {0};
    size_t start = out->length;
    uint32_t file = f->file != IR_NO_INDEX ? d->files + f->file
                                           : unit_file_id(d);
    /* Without -g the subprogram of COFF names no line, which CodeView
       would write as the line record of the function. */
    uint32_t line = d->lines && f->file != IR_NO_INDEX ? f->decl_line : 0;

    d->f = f;
    d->located = false;
    d->location = 0;
    if (!d->on) {
        return;
    }
    d->subprogram = d->next++;
    text_appendf(out, " !dbg !%" PRIu32, d->subprogram);
    llvm_debug_mark(d->spans, start, out->length);
    reader_name(&name, f);
    text_appendf(&d->nodes, "!%" PRIu32 " = distinct !DISubprogram(name: ",
                 d->subprogram);
    quoted(&d->nodes, text_cstr(&name));
    text_appendf(&d->nodes, ", scope: !%" PRIu32 ", file: !%" PRIu32
                            ", line: %" PRIu32 ", type: !%" PRIu32
                            ", scopeLine: %" PRIu32 ", spFlags: %s"
                            "DISPFlagDefinition%s, unit: !%" PRIu32 ")\n",
                 file, file, line, d->type, line,
                 local ? "DISPFlagLocalToUnit | " : "",
                 d->optimized ? " | DISPFlagOptimized" : "", d->unit);
    text_free(&name);
}

/* The id of the location of line in the function being written. Each
   run of one line takes one node. */
static uint32_t location(struct llvm_debug *d, uint32_t line)
{
    if (d->location != 0 && d->line == line) {
        return d->location;
    }
    d->line = line;
    d->location = d->next++;
    text_appendf(&d->nodes, "!%" PRIu32 " = !DILocation(line: %" PRIu32
                            ", column: 0, scope: !%" PRIu32 ")\n",
                 d->location, line, d->subprogram);
    return d->location;
}

/* Whether the line of the text at p, up to its end, is a call. */
static bool is_call(const char *p, size_t length)
{
    static const char call[] = " call ";
    size_t n = sizeof call - 1;
    size_t i;

    for (i = 0; i + n <= length; i++) {
        if (memcmp(p + i, call, n) == 0) {
            return true;
        }
    }
    return false;
}

/* DESIGN: an instruction of a line takes its location, and one of no
   line takes none and inherits nothing, as the work order gives it. Two
   take the location of line 0 instead. A call does, because the verifier
   asks for a location on a call of a function with a subprogram in a
   function with one. The first instruction of a function does, because
   llc writes the subprogram of a function, and CodeView its symbol
   record, only for a function with a location. */
void llvm_debug_at(struct llvm_debug *d, struct text *body, size_t from,
                   uint32_t line, struct debug_spans *spans)
{
    bool real = d->lines && line != 0 && d->f->file != IR_NO_INDEX;
    char *copy;
    size_t length;
    size_t at = 0;

    if (!d->on || body->length <= from) {
        return;
    }
    length = body->length - from;
    copy = alloc_zeroed(length + 1, 1);
    memcpy(copy, body->data + from, length);
    body->length = from;
    body->data[from] = '\0';
    while (at < length) {
        const char *end = memchr(copy + at, '\n', length - at);
        size_t n = end != NULL ? (size_t)(end - (copy + at)) : length - at;
        bool instruction = n > 2 && copy[at] == ' ' && copy[at + 1] == ' ';
        text_append_bytes(body, copy + at, n);
        if (instruction &&
            (real || !d->located || is_call(copy + at, n))) {
            size_t start = body->length;
            text_appendf(body, ", !dbg !%" PRIu32,
                         location(d, real ? line : 0));
            llvm_debug_mark(spans, start, body->length);
            d->located = true;
        }
        if (end != NULL) {
            text_append(body, "\n");
            n++;
        }
        at += n;
    }
    free(copy);
}

void llvm_debug_place(struct llvm_debug *d, const struct debug_spans *body,
                      size_t base)
{
    size_t i;

    for (i = 0; i < body->count; i++) {
        llvm_debug_mark(d->spans, base + body->items[i].start,
                        base + body->items[i].end);
    }
}

void llvm_debug_flags(struct llvm_debug *d, struct text *out)
{
    size_t start = out->length;
    uint32_t i;

    if (!d->on) {
        return;
    }
    for (i = 0; i < d->flag_count; i++) {
        text_appendf(out, ", !%" PRIu32, d->flags + i);
    }
    llvm_debug_mark(d->spans, start, out->length);
}

/* DESIGN: llc writes DWARF on ELF and Mach-O and CodeView on COFF. The
   flag "CodeView" asks for the second. */
void llvm_debug_finish(struct llvm_debug *d, struct text *out)
{
    size_t start = out->length;

    if (!d->on) {
        return;
    }
    text_appendf(out, "!llvm.dbg.cu = !{!%" PRIu32 "}\n", d->unit);
    text_appendf(out, "!%" PRIu32 " = !{i32 2, !\"Debug Info Version\", "
                      "i32 3}\n",
                 d->flags);
    if (d->flag_count > 1) {
        text_appendf(out, "!%" PRIu32 " = !{i32 2, !\"CodeView\", i32 1}\n",
                     d->flags + 1);
    }
    text_append_bytes(out, d->nodes.data, d->nodes.length);
    llvm_debug_mark(d->spans, start, out->length);
}

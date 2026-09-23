#include "debug.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* DESIGN: `.file` and `.loc` give llvm-mc a line table and nothing else.
   A debugger needs a compile unit as well: without one it finds no source
   file, and `breakpoint set --file --line` resolves to nothing. llvm-mc
   writes a unit of its own only for an assembly file that carries no
   `.file` directive, and then names the assembly file rather than the
   Anti source. So antic writes the unit itself, as the bytes of
   .debug_abbrev and .debug_info, and llvm-mc writes the line table those
   bytes point at. The unit holds one entry, the range of the code and the
   name of the line table. Names of functions come from the symbol table,
   which every build already carries. */

/* The DWARF codes that the compile unit needs. */
enum {
    DW_TAG_compile_unit = 0x11,
    DW_CHILDREN_no = 0x00,
    DW_AT_name = 0x03,
    DW_AT_stmt_list = 0x10,
    DW_AT_low_pc = 0x11,
    DW_AT_high_pc = 0x12,
    DW_AT_language = 0x13,
    DW_AT_producer = 0x25,
    DW_FORM_addr = 0x01,
    DW_FORM_data2 = 0x05,
    DW_FORM_data4 = 0x06,
    DW_FORM_string = 0x08,
    DW_FORM_sec_offset = 0x17,
    /* No DWARF language code names Anti. The code of an assembler is the
       one llvm-mc writes for a file of `.loc` directives, and the
       debuggers are tested against it. A code of its own replaces this
       one the day Anti has one. */
    DW_LANG_assembler = 0x8001,
    /* The version of the unit. llvm-mc writes a line table of version 4
       for an assembly file, and the two agree. */
    DWARF_VERSION = 4,
    DWARF_ADDRESS_SIZE = 8
};

/* The CodeView codes of the symbol records: the kind of the subsection
   of .debug$S that holds them, and the two kinds of record. */
enum {
    CV_SYMBOLS = 0xF1,
    S_LPROC32 = 0x110F,
    S_END = 0x0006
};

/* The label prefix that keeps a label out of the symbol table. */
static const char *local(const struct debug *d)
{
    return target_info(d->target)->format == FORMAT_MACHO ? "L" : ".L";
}

/* DESIGN: on COFF a symbol record in .debug$S names every function, with
   -g and without it. A release writes each function as a static symbol,
   and lld-link carries only external symbols into the PDB. Without the
   record DbgHelp names a frame after the external symbol below it. The
   record reaches the PDB and never the executable. It names the function
   as the symbol tables of ELF and Mach-O do, `module.Class.f`, and an
   export fn by its C name. A debugger and the runtime then read one name
   on every target. */
static bool codeview(const struct debug *d)
{
    return target_info(d->target)->format == FORMAT_COFF;
}

void debug_spans_free(struct debug_spans *s)
{
    free(s->items);
    memset(s, 0, sizeof *s);
}

/* Record the bytes appended to out since start as a range that `-g`
   added. A caller that reads no range records none. */
static void mark(const struct debug *d, const struct text *out, size_t start)
{
    struct debug_spans *s = d->spans;

    if (s == NULL || out->length <= start) {
        return;
    }
    /* The ranges come in the order of the file, so one that follows the
       last without a byte between them joins it. */
    if (s->count > 0 && s->items[s->count - 1].end == start) {
        s->items[s->count - 1].end = out->length;
        return;
    }
    s->items = ir_grow(s->items, &s->capacity, s->count, sizeof *s->items);
    s->items[s->count].start = start;
    s->items[s->count].end = out->length;
    s->count++;
}

void debug_init(struct debug *d, enum target t, const struct ir_module *m,
                const char *module, bool on, struct debug_spans *spans)
{
    memset(d, 0, sizeof *d);
    d->target = t;
    d->m = m;
    d->module = module;
    d->on = on;
    d->file = IR_NO_INDEX;
    d->spans = spans;
}

/* The file of the program's own module, which names the compile unit. */
static const char *unit_file(const struct debug *d)
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

/* Append s as an assembler string in quotes. A path from Windows holds
   `\`, which llvm-mc reads as the start of an escape, and a path may hold
   `"`. A control byte is written as its octal escape. */
static void quoted(struct text *out, const char *s)
{
    text_append(out, "\"");
    for (; *s != '\0'; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') {
            text_appendf(out, "\\%c", c);
        } else if (c < 0x20 || c == 0x7f) {
            text_appendf(out, "\\%03o", c);
        } else {
            text_append_bytes(out, s, 1);
        }
    }
    text_append(out, "\"");
}

void debug_files(struct debug *d, struct text *out)
{
    bool codeview = target_info(d->target)->format == FORMAT_COFF;
    size_t start = out->length;
    size_t i;

    if (!d->on) {
        return;
    }
    for (i = 0; i < d->m->file_count; i++) {
        text_appendf(out, "    %s %zu ", codeview ? ".cv_file" : ".file",
                     i + 1);
        quoted(out, d->m->files[i]);
        text_append(out, "\n");
    }
    text_appendf(out, "%santi_debug_code:\n", local(d));
    mark(d, out, start);
}

void debug_open(struct debug *d, struct text *out,
                const struct ir_function *f)
{
    size_t start = out->length;

    if (!d->on) {
        /* The symbol record takes the length of the function from its
           end label. */
        d->function += codeview(d) ? 1 : 0;
        return;
    }
    d->file = f->file;
    /* The line of the instruction before is the line of another function,
       so the first position of this one is written whatever it is. */
    d->line = 0;
    if (codeview(d)) {
        text_appendf(out, "    .cv_func_id %zu\n", d->function);
        mark(d, out, start);
    }
    d->function++;
    /* The position of the declaration marks its own range, which joins
       the one above it. */
    debug_at(d, out, f->decl_line);
}

void debug_at(struct debug *d, struct text *out, uint32_t line)
{
    size_t start = out->length;

    if (!d->on || line == 0 || line == d->line || d->file == IR_NO_INDEX) {
        return;
    }
    d->line = line;
    if (target_info(d->target)->format == FORMAT_COFF) {
        text_appendf(out, "    .cv_loc %zu %" PRIu32 " %" PRIu32 " 0\n",
                     d->function - 1, d->file + 1, line);
    } else {
        text_appendf(out, "    .loc %" PRIu32 " %" PRIu32 " 0\n", d->file + 1,
                     line);
    }
    mark(d, out, start);
}

void debug_close(struct debug *d, struct text *out)
{
    size_t start = out->length;

    if (!d->on && !codeview(d)) {
        return;
    }
    text_appendf(out, "%santi_debug_fn%zu_end:\n", local(d), d->function - 1);
    /* On COFF the label stands in every build, because the symbol record
       of the function reads it. It is then no range of `-g`. */
    if (!codeview(d)) {
        mark(d, out, start);
    }
}

/* The section that holds part of the debug information. ELF and COFF name
   it with its flags. Mach-O names it with its segment and the attribute
   that marks debug information the linker does not load. */
static void section(struct debug *d, struct text *out, const char *name)
{
    switch (target_info(d->target)->format) {
    case FORMAT_ELF:
        text_appendf(out, "    .section .%s,\"\",@progbits\n", name);
        break;
    case FORMAT_MACHO:
        text_appendf(out, "    .section __DWARF,__%s,regular,debug\n", name);
        break;
    case FORMAT_COFF:
        text_appendf(out, "    .section .%s,\"dr\"\n", name);
        break;
    }
}

/* One attribute of the abbreviation: its code and its form. */
static void attribute(struct text *out, unsigned at, unsigned form,
                      const char *name)
{
    text_appendf(out, "    .byte %u, %u          /* %s */\n", at, form, name);
}

/* The abbreviation of the one entry of the unit. */
static void abbreviations(struct debug *d, struct text *out)
{
    section(d, out, "debug_abbrev");
    text_appendf(out, "%santi_debug_abbrev:\n", local(d));
    text_appendf(out, "    .byte 1              /* the abbreviation */\n"
                      "    .byte %u             /* DW_TAG_compile_unit */\n"
                      "    .byte %u              /* DW_CHILDREN_no */\n",
                 DW_TAG_compile_unit, DW_CHILDREN_no);
    attribute(out, DW_AT_stmt_list, DW_FORM_sec_offset, "DW_AT_stmt_list");
    attribute(out, DW_AT_low_pc, DW_FORM_addr, "DW_AT_low_pc");
    attribute(out, DW_AT_high_pc, DW_FORM_data4, "DW_AT_high_pc");
    attribute(out, DW_AT_name, DW_FORM_string, "DW_AT_name");
    attribute(out, DW_AT_producer, DW_FORM_string, "DW_AT_producer");
    attribute(out, DW_AT_language, DW_FORM_data2, "DW_AT_language");
    text_append(out, "    .byte 0, 0           /* the end of the "
                     "attributes */\n"
                     "    .byte 0              /* the end of the "
                     "abbreviations */\n");
}

/* DESIGN: an offset into another section is a relocation on ELF and COFF.
   There the linker joins the debug sections of every object and moves
   each one. Mach-O keeps the debug information in the object files and
   names them from the executable, so nothing moves. The offset of the one
   unit of the object is then 0. */
static void section_offset(struct debug *d, struct text *out,
                           const char *label, const char *what)
{
    if (target_info(d->target)->format == FORMAT_MACHO) {
        text_appendf(out, "    .long 0              /* %s */\n", what);
    } else {
        text_appendf(out, "    .long %santi_debug_%s    /* %s */\n", local(d),
                     label, what);
    }
}

/* The compile unit: the range of the code, the line table and the name. */
static void compile_unit(struct debug *d, struct text *out)
{
    const char *l = local(d);

    section(d, out, "debug_line");
    text_appendf(out, "%santi_debug_line:\n", l);
    abbreviations(d, out);
    section(d, out, "debug_info");
    text_appendf(out, "    .long %santi_debug_unit_end - "
                      "%santi_debug_unit    /* the length of the unit */\n",
                 l, l);
    text_appendf(out, "%santi_debug_unit:\n", l);
    text_appendf(out, "    .short %u              /* the DWARF version */\n",
                 DWARF_VERSION);
    section_offset(d, out, "abbrev", "the abbreviations");
    text_appendf(out, "    .byte %u              /* the size of an "
                      "address */\n"
                      "    .byte 1              /* the abbreviation of the "
                      "unit */\n",
                 DWARF_ADDRESS_SIZE);
    section_offset(d, out, "line", "DW_AT_stmt_list");
    text_appendf(out, "    .quad %santi_debug_code    /* DW_AT_low_pc */\n", l);
    text_appendf(out, "    .long %santi_debug_code_end - "
                      "%santi_debug_code    /* DW_AT_high_pc */\n",
                 l, l);
    text_append(out, "    .asciz ");
    quoted(out, unit_file(d));
    text_append(out, "\n");
    text_appendf(out, "    .asciz \"antic %s\"\n", ANTIC_VERSION);
    text_appendf(out, "    .short %u          /* DW_AT_language */\n",
                 DW_LANG_assembler);
    text_appendf(out, "%santi_debug_unit_end:\n", l);
}

/* The name that the readers of ELF and Mach-O give a function. That is its
   module path, a dot and its name, or the C name of an export fn. */
static void reader_name(struct text *out, const struct ir_function *f)
{
    if (f->module == NULL || f->exported) {
        mach_function_symbol(out, TARGET_LINUX_X86_64, f);
    } else {
        text_appendf(out, "%s.%s", f->module, f->name);
    }
}

/* The symbol record of each function, in the order the emitter wrote
   them. Each is an S_LPROC32 without a type, closed by an S_END. lld-link
   fills in the parent, the end and the next record. A record ends on a
   multiple of 4, which the PDB asks for. */
static void symbol_records(struct debug *d, struct text *out,
                           struct mach_function *const *functions)
{
    const char *l = local(d);
    size_t id = 0;
    size_t i;

    text_appendf(out, "    .long %u            /* DEBUG_S_SYMBOLS */\n"
                      "    .long %santi_cv_symbols_end - %santi_cv_symbols\n"
                      "%santi_cv_symbols:\n",
                 CV_SYMBOLS, l, l, l);
    for (i = 0; i < d->m->function_count; i++) {
        struct text symbol = {0};
        struct text name = {0};
        const char *s;
        if (functions[i] == NULL) {
            continue;
        }
        mach_function_symbol(&symbol, d->target, d->m->functions[i]);
        reader_name(&name, d->m->functions[i]);
        s = text_cstr(&symbol);
        text_appendf(out, "    .short %santi_cv_fn%zu_end - %santi_cv_fn%zu\n"
                          "%santi_cv_fn%zu:\n", l, id, l, id, l, id);
        text_appendf(out, "    .short %u          /* S_LPROC32 */\n",
                     S_LPROC32);
        text_append(out, "    .long 0, 0, 0        /* the parent, the end, "
                         "the next */\n");
        text_appendf(out, "    .long %santi_debug_fn%zu_end - %s\n", l, id, s);
        text_append(out, "    .long 0, 0           /* the ends of the "
                         "prologue and the epilogue */\n"
                         "    .long 0              /* no type */\n");
        text_appendf(out, "    .secrel32 %s\n    .secidx %s\n", s, s);
        text_appendf(out, "    .byte 0              /* the flags */\n"
                          "    .asciz \"%s\"\n"
                          "    .p2align 2\n"
                          "%santi_cv_fn%zu_end:\n", text_cstr(&name), l, id);
        text_appendf(out, "    .short 2\n"
                          "    .short %u             /* S_END */\n", S_END);
        text_free(&symbol);
        text_free(&name);
        id++;
    }
    text_appendf(out, "%santi_cv_symbols_end:\n", l);
}

/* The CodeView line table of every function, in the order the emitter
   wrote them, with the table of file names and the strings they name. */
static void line_tables(struct debug *d, struct text *out,
                        struct mach_function *const *functions)
{
    size_t id = 0;
    size_t i;

    for (i = 0; i < d->m->function_count; i++) {
        struct text symbol = {0};
        if (functions[i] == NULL) {
            continue;
        }
        mach_function_symbol(&symbol, d->target, d->m->functions[i]);
        text_appendf(out, "    .cv_linetable %zu, %s, %santi_debug_fn%zu_end\n",
                     id, text_cstr(&symbol), local(d), id);
        text_free(&symbol);
        id++;
    }
    text_append(out, "    .cv_filechecksums\n    .cv_stringtable\n");
}

void debug_sections(struct debug *d, struct text *out,
                    struct mach_function *const *functions)
{
    size_t start = out->length;

    if (d->on) {
        text_appendf(out, "%santi_debug_code_end:\n", local(d));
        mark(d, out, start);
    }
    if (codeview(d)) {
        text_append(out, "    .section .debug$S,\"dr\"\n"
                         "    .p2align 2\n"
                         "    .long 4              /* the section starts "
                         "with a 4 */\n");
        symbol_records(d, out, functions);
        if (d->on) {
            start = out->length;
            line_tables(d, out, functions);
            mark(d, out, start);
        }
    } else if (d->on) {
        start = out->length;
        compile_unit(d, out);
        mark(d, out, start);
    }
}

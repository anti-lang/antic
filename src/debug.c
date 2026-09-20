#include "debug.h"

#include <inttypes.h>
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

/* The label prefix that keeps a label out of the symbol table. */
static const char *local(const struct debug *d)
{
    return target_info(d->target)->format == FORMAT_MACHO ? "L" : ".L";
}

void debug_init(struct debug *d, enum target t, const struct ir_module *m,
                const char *module, bool on)
{
    memset(d, 0, sizeof *d);
    d->target = t;
    d->m = m;
    d->module = module;
    d->on = on;
    d->file = IR_NO_INDEX;
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

void debug_files(struct debug *d, struct text *out)
{
    bool codeview = target_info(d->target)->format == FORMAT_COFF;
    size_t i;

    if (!d->on) {
        return;
    }
    for (i = 0; i < d->m->file_count; i++) {
        text_appendf(out, "    %s %zu \"%s\"\n", codeview ? ".cv_file" : ".file",
                     i + 1, d->m->files[i]);
    }
    text_appendf(out, "%santi_debug_code:\n", local(d));
}

void debug_open(struct debug *d, struct text *out,
                const struct ir_function *f)
{
    if (!d->on) {
        return;
    }
    d->file = f->file;
    /* The line of the instruction before is the line of another function,
       so the first position of this one is written whatever it is. */
    d->line = 0;
    if (target_info(d->target)->format == FORMAT_COFF) {
        text_appendf(out, "    .cv_func_id %zu\n", d->function);
    }
    d->function++;
    debug_at(d, out, f->decl_line);
}

void debug_at(struct debug *d, struct text *out, uint32_t line)
{
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
}

void debug_close(struct debug *d, struct text *out)
{
    if (!d->on) {
        return;
    }
    text_appendf(out, "%santi_debug_fn%zu_end:\n", local(d), d->function - 1);
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
    text_appendf(out, "    .asciz \"%s\"\n", unit_file(d));
    text_appendf(out, "    .asciz \"antic %s\"\n", ANTIC_VERSION);
    text_appendf(out, "    .short %u          /* DW_AT_language */\n",
                 DW_LANG_assembler);
    text_appendf(out, "%santi_debug_unit_end:\n", l);
}

/* The CodeView line table of every function, in the order the emitter
   wrote them, with the table of file names and the strings they name. */
static void line_tables(struct debug *d, struct text *out,
                        struct mach_function *const *functions)
{
    size_t id = 0;
    size_t i;

    text_append(out, "    .section .debug$S,\"dr\"\n"
                     "    .p2align 2\n"
                     "    .long 4              /* the section starts "
                     "with a 4 */\n");
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
    if (!d->on) {
        return;
    }
    text_appendf(out, "%santi_debug_code_end:\n", local(d));
    if (target_info(d->target)->format == FORMAT_COFF) {
        line_tables(d, out, functions);
    } else {
        compile_unit(d, out);
    }
}

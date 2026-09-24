#include "emit.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "select.h"

/* DESIGN: one assembly file holds the whole program. Every Anti function
   is a local symbol, and only the runtime entry is global. */

/* DESIGN: in an object of one module, every function and every datum is
   global for the other modules and hidden. A shared library then exports
   only the export fns, and the other modules name the one copy of a
   datum. COFF has no hidden symbols, and the .def file of a DLL names its
   exports. A program that hosts a plugin leaves the hidden mark off. The
   loader then resolves the plugin against it, and the descriptors it
   holds are the ones the plugin reaches. */
static void visibility(struct text *out, enum target t, const char *symbol,
                       bool exported, bool module, bool exports)
{
    if (!exported && !module && !exports) {
        return;
    }
    text_appendf(out, "    .globl %s\n", symbol);
    if (module && !exports && !exported &&
        target_info(t)->format != FORMAT_COFF) {
        text_appendf(out, "    %s %s\n",
                     target_info(t)->format == FORMAT_MACHO ? ".private_extern"
                                                           : ".hidden",
                     symbol);
    }
}

/* Write bytes from to up to to as .byte lines. A line ends after each
   offset that is 15 modulo 16, so the rows of one global line up however
   its addresses split them, and at to. */
static void byte_rows(struct text *out, const unsigned char *bytes,
                      uint64_t from, uint64_t to)
{
    uint64_t k;

    for (k = from; k < to; k++) {
        text_appendf(out, "%s0x%02x",
                     k == from || k % 16 == 0 ? "    .byte " : ", ", bytes[k]);
        if (k % 16 == 15 || k + 1 == to) {
            text_append(out, "\n");
        }
    }
}

/* The Mach-O line of the least macOS version, which every object of the
   target carries. */
static void build_version(struct text *out, enum target t)
{
    if (target_info(t)->format == FORMAT_MACHO) {
        text_appendf(out, "    .build_version macos, %d, %d\n",
                     MACOS_MIN_MAJOR, MACOS_MIN_MINOR);
    }
}

static void emit_function(struct text *out, enum target t, enum cpu_level cpu,
                          const struct ir_module *m,
                          const struct mach_function *f, bool module,
                          bool exports, struct debug *debug)
{
    const struct target_desc *desc = target_desc(t);
    struct text symbol = {0};
    struct names names;
    size_t b;
    size_t i;

    mach_function_symbol(&symbol, t, f->ir);
    names.target = t;
    names.function = text_cstr(&symbol);
    visibility(out, t, text_cstr(&symbol), f->ir->exported, module,
               exports);
    /* An ARM64 instruction is 4 bytes, and a function starts on one. */
    if (target_info(t)->arch == ARCH_ARM64) {
        text_append(out, "    .p2align 2\n");
    }
    text_appendf(out, "%s:\n", text_cstr(&symbol));
    /* A function with Windows unwind data. llvm-mc writes its pdata and
       xdata from the .seh_ lines. */
    if (f->unwind) {
        text_appendf(out, "    .seh_proc %s\n", text_cstr(&symbol));
    }
    debug_open(debug, out, f->ir);
    for (b = 0; b < f->block_count; b++) {
        block_label(out, t, text_cstr(&symbol), b);
        text_append(out, ":\n");
        for (i = 0; i < f->blocks[b].count; i++) {
            debug_at(debug, out, f->blocks[b].insts[i].line);
            text_append(out, "    ");
            desc->print(out, cpu, m, &f->blocks[b].insts[i], &names);
            text_append(out, "\n");
        }
    }
    debug_close(debug, out);
    if (f->unwind) {
        text_append(out, "    .seh_endproc\n");
    }
    text_free(&symbol);
}

/* The runtime reaches main through a second global name, RUNTIME_ENTRY
   of RUNTIME_MODULE, which .set defines. */
static void emit_entry(struct text *out, enum target t,
                       const struct ir_module *m, const char *module)
{
    struct text entry = {0};
    struct text main_symbol = {0};
    size_t i;

    for (i = 0; i < m->function_count; i++) {
        const struct ir_function *f = m->functions[i];
        if (!f->is_extern && f->module != NULL &&
            strcmp(f->module, module) == 0 && strcmp(f->name, "main") == 0) {
            mangle(&entry, t, RUNTIME_MODULE, RUNTIME_ENTRY);
            mangle(&main_symbol, t, module, "main");
            text_appendf(out, "    .globl %s\n", text_cstr(&entry));
            text_appendf(out, "    .set %s, %s\n", text_cstr(&entry),
                         text_cstr(&main_symbol));
        }
    }
    text_free(&entry);
    text_free(&main_symbol);
}

/* DESIGN: global data holds the bytes of literals, which no program writes
   to, so it goes to the read-only data section of each object format. */
static const char *const data_sections[] = {
    [FORMAT_ELF] = ".rodata",
    [FORMAT_MACHO] = "__TEXT,__const",
    [FORMAT_COFF] = ".rdata,\"dr\"",
};

/* DESIGN: data that holds an address is written once when the program
   loads, so a section mapped read-only from the start cannot hold it.
   Every format has a section for data the loader writes and then
   protects, and PE protects .rdata after its base relocations. */
static const char *const reloc_sections[] = {
    [FORMAT_ELF] = ".data.rel.ro",
    [FORMAT_MACHO] = "__DATA,__const",
    [FORMAT_COFF] = ".rdata,\"dr\"",
};

/* DESIGN: a static field is data the program writes while it runs, so it
   goes to the section that stays writable. */
static const char *const mutable_sections[] = {
    [FORMAT_ELF] = ".data",
    [FORMAT_MACHO] = "__DATA,__data",
    [FORMAT_COFF] = ".data",
};

/* Whether the address of relocation r names a function or a datum the
   host of a COFF plugin defines. */
static bool imported(enum target t, const struct ir_module *m,
                     const struct ir_reloc *r)
{
    if (!m->plugin || target_info(t)->format != FORMAT_COFF) {
        return false;
    }
    return r->fn ? m->functions[r->global]->is_extern
                 : m->globals[r->global]->is_extern;
}

/* The symbol an address at offset names, written into out. It is a
   global, or the function of a table entry, which may be an extern of
   the runtime and then carries no module. Returns false when no
   relocation sits at that offset. */
static bool reloc_at(struct text *out, enum target t,
                     const struct ir_module *m, const struct ir_global *g,
                     uint64_t offset)
{
    size_t i;

    for (i = 0; i < g->reloc_count; i++) {
        if (g->relocs[i].offset != offset) {
            continue;
        }
        /* DESIGN: a COFF plugin holds the address of the __imp_ entry of
           a name of its host. The loader replaces it with the address
           that entry holds. anti_rt_imports lists each such place. */
        if (imported(t, m, &g->relocs[i])) {
            text_append(out, "__imp_");
        }
        if (g->relocs[i].fn) {
            mach_function_symbol(out, t, m->functions[g->relocs[i].global]);
        } else if (m->globals[g->relocs[i].global]->exported) {
            c_symbol(out, t, m->globals[g->relocs[i].global]->name);
        } else {
            mangle(out, t, m->globals[g->relocs[i].global]->module,
                   m->globals[g->relocs[i].global]->name);
        }
        return true;
    }
    return false;
}

/* The bytes of one global, with an address written as the symbol it
   names, so that the linker fills in the eight bytes. */
static void emit_global(struct text *out, enum target t,
                        const struct ir_module *m, const struct ir_global *g,
                        bool module, bool exports)
{
    struct text name = {0};
    uint64_t align;
    uint64_t k;
    int log2;
    uint64_t from = 0;

    for (log2 = 0, align = g->align; align > 1; align /= 2) {
        log2++;
    }
    if (log2 > 0) {
        text_appendf(out, "    .p2align %d\n", log2);
    }
    if (g->exported) {
        c_symbol(&name, t, g->name);
    } else {
        mangle(&name, t, g->module, g->name);
    }
    visibility(out, t, text_cstr(&name), g->exported, module, exports);
    text_appendf(out, "%s:\n", text_cstr(&name));
    text_free(&name);
    for (k = 0; k < g->size; k++) {
        struct text symbol = {0};
        if (reloc_at(&symbol, t, m, g, k)) {
            byte_rows(out, g->bytes, from, k);
            text_appendf(out, "    .quad %s\n", text_cstr(&symbol));
            k += 7;
            from = k + 1;
        }
        text_free(&symbol);
    }
    byte_rows(out, g->bytes, from, g->size);
}

/* DESIGN: a global that holds an address goes to the section for data the
   loader writes, and every other global to the read-only section. The two
   are written in one pass each, so each section is named once. */
static void emit_data(struct text *out, enum target t,
                      const struct ir_module *m, bool module, bool exports)
{
    enum object_format format = target_info(t)->format;
    bool relocated = false;
    bool written = false;
    size_t i;

    text_appendf(out, "    .section %s\n", data_sections[format]);
    for (i = 0; i < m->global_count; i++) {
        if (m->globals[i]->is_extern) {
            continue;
        }
        if (m->globals[i]->mutable) {
            written = true;
            continue;
        }
        if (m->globals[i]->reloc_count > 0) {
            relocated = true;
            continue;
        }
        emit_global(out, t, m, m->globals[i], module, exports);
    }
    if (written) {
        text_appendf(out, "    .section %s\n", mutable_sections[format]);
        for (i = 0; i < m->global_count; i++) {
            if (m->globals[i]->mutable && !m->globals[i]->is_extern) {
                emit_global(out, t, m, m->globals[i], module, exports);
            }
        }
    }
    if (!relocated) {
        return;
    }
    text_appendf(out, "    .section %s\n", reloc_sections[format]);
    for (i = 0; i < m->global_count; i++) {
        if (!m->globals[i]->mutable && m->globals[i]->reloc_count > 0 &&
            !m->globals[i]->is_extern) {
            emit_global(out, t, m, m->globals[i], module, exports);
        }
    }
}

/* DESIGN: the table anti_rt_imports of a COFF plugin holds three parts.
   The first is the count of the places that hold the address of an
   __imp_ entry. The second is a word the loader marks once it has
   replaced them, and the third the address of each place. It stands in .data, because the loader writes
   the mark. */
static void emit_imports(struct text *out, enum target t,
                         const struct ir_module *m)
{
    struct text symbol = {0};
    struct text places = {0};
    size_t count = 0;
    size_t i;
    size_t j;

    for (i = 0; i < m->global_count; i++) {
        const struct ir_global *g = m->globals[i];
        struct text name = {0};
        if (g->is_extern) {
            continue;
        }
        if (g->exported) {
            c_symbol(&name, t, g->name);
        } else {
            mangle(&name, t, g->module, g->name);
        }
        for (j = 0; j < g->reloc_count; j++) {
            if (imported(t, m, &g->relocs[j])) {
                text_appendf(&places, "    .quad %s+%" PRIu64 "\n",
                             text_cstr(&name), g->relocs[j].offset);
                count++;
            }
        }
        text_free(&name);
    }
    c_symbol(&symbol, t, "anti_rt_imports");
    text_appendf(out, "    .section %s\n    .p2align 3\n    .globl %s\n%s:\n"
                 "    .quad %zu\n    .quad 0\n%s",
                 mutable_sections[target_info(t)->format], text_cstr(&symbol),
                 text_cstr(&symbol), count, text_cstr(&places));
    text_free(&symbol);
    text_free(&places);
}

/* Whether every address of g lies inside its bytes and apart from the
   others. The pairs cost no more than the writing of g, which looks each
   offset up among the addresses. */
static bool relocations_fit(const struct ir_global *g, char *error,
                            size_t error_size)
{
    size_t j;
    size_t k;

    for (j = 0; j < g->reloc_count; j++) {
        uint64_t at = g->relocs[j].offset;
        if (g->size < 8 || at > g->size - 8) {
            ir_format(error, error_size,
                      "the address at %" PRIu64 " of `%s.%s` ends past its "
                      "%" PRIu64 " bytes", at, g->module, g->name, g->size);
            return false;
        }
        for (k = 0; k < j; k++) {
            uint64_t other = g->relocs[k].offset;
            if ((at > other ? at - other : other - at) < 8) {
                ir_format(error, error_size,
                          "the addresses at %" PRIu64 " and %" PRIu64
                          " of `%s.%s` overlap", at < other ? at : other,
                          at < other ? other : at, g->module, g->name);
                return false;
            }
        }
    }
    return true;
}

/* The sections of the functions that run before main, one entry of an
   address each. */
static const char *const constructor_sections[] = {
    [FORMAT_ELF] = ".init_array,\"aw\"",
    [FORMAT_MACHO] = "__DATA,__mod_init_func,mod_init_funcs",
    [FORMAT_COFF] = ".CRT$XCU,\"dr\"",
};

/* An entry that runs f before main, or when the library loads. */
static void emit_start(struct text *out, enum target t,
                       const struct ir_function *f)
{
    struct text symbol = {0};

    mach_function_symbol(&symbol, t, f);
    text_appendf(out, "    .section %s\n    .p2align 3\n    .quad %s\n",
                 constructor_sections[target_info(t)->format],
                 text_cstr(&symbol));
    text_free(&symbol);
}

static bool emit(struct text *out, enum target t, enum cpu_level cpu,
                 const struct ir_module *m, struct mach_function **functions,
                 const char *module, bool one_module, bool exports,
                 bool debug_info, struct debug_spans *spans, char *error,
                 size_t error_size)
{
    const struct target_info *info = target_info(t);
    struct debug debug;
    size_t i;

    debug_init(&debug, t, m, module, debug_info, spans);

    /* An address takes the eight bytes at its offset, so it lies inside
       the data that holds it, and no two share a byte. A library file
       read from disk is the one source of a global that breaks either. */
    for (i = 0; i < m->global_count; i++) {
        if (!relocations_fit(m->globals[i], error, error_size)) {
            return false;
        }
    }
    build_version(out, t);
    text_append(out, "    .text\n");
    emit_entry(out, t, m, module);
    debug_files(&debug, out);
    for (i = 0; i < m->function_count; i++) {
        if (functions[i] != NULL) {
            emit_function(out, t, cpu, m, functions[i], one_module, exports,
                          &debug);
        }
    }
    /* The debug information ends the text section, because the range of
       the code is the range of the functions written above it. */
    debug_sections(&debug, out, functions);
    if (m->global_count > 0) {
        emit_data(out, t, m, one_module, exports);
    }
    for (i = 0; i < m->function_count; i++) {
        if (functions[i] != NULL && ir_is_patterns_start(m->functions[i])) {
            emit_start(out, t, m->functions[i]);
        }
    }
    if (m->plugin && info->format == FORMAT_COFF) {
        emit_imports(out, t, m);
    }
    /* Without this note GNU ld may mark the stack executable. */
    if (info->format == FORMAT_ELF) {
        text_append(out, "    .section .note.GNU-stack,\"\",@progbits\n");
    }
    return true;
}

bool emit_program(struct text *out, enum target t, enum cpu_level cpu,
                  const struct ir_module *m, struct mach_function **functions,
                  const char *module, bool exports, bool debug_info,
                  struct debug_spans *spans, char *error, size_t error_size)
{
    return emit(out, t, cpu, m, functions, module, false, exports, debug_info,
                spans, error, error_size);
}

bool emit_module(struct text *out, enum target t, enum cpu_level cpu,
                 const struct ir_module *m, struct mach_function **functions,
                 const char *module, bool exports, bool debug_info,
                 struct debug_spans *spans, char *error, size_t error_size)
{
    return emit(out, t, cpu, m, functions, module, true, exports, debug_info,
                spans, error, error_size);
}

void emit_names(struct text *out, enum target t, const struct ir_module *m,
                struct mach_function **functions)
{
    size_t i;

    for (i = 0; i < m->function_count; i++) {
        struct text symbol = {0};
        if (functions[i] == NULL) {
            continue;
        }
        mach_function_symbol(&symbol, t, m->functions[i]);
        text_appendf(out, "%s\n", text_cstr(&symbol));
        text_free(&symbol);
    }
    for (i = 0; i < m->global_count; i++) {
        const struct ir_global *g = m->globals[i];
        struct text name = {0};
        if (g->is_extern) {
            continue;
        }
        if (g->exported) {
            c_symbol(&name, t, g->name);
        } else {
            mangle(&name, t, g->module, g->name);
        }
        text_appendf(out, "%s DATA\n", text_cstr(&name));
        text_free(&name);
    }
}

/* DESIGN: a shared library initialises the runtime in a constructor. The
   loader calls every function whose address lies in .init_array on ELF,
   __mod_init_func on Mach-O and .CRT$XCU on COFF. */
void emit_constructor(struct text *out, enum target t, const char *function)
{
    struct text symbol = {0};

    c_symbol(&symbol, t, function);
    text_appendf(out, "    .section %s\n    .p2align 3\n    .quad %s\n",
                 constructor_sections[target_info(t)->format],
                 text_cstr(&symbol));
    text_free(&symbol);
}

void emit_licenses(struct text *out, enum target t, const char *bytes,
                   size_t length)
{
    struct text symbol = {0};
    unsigned char *text = ir_alloc(length + 1, 1);

    /* The texts end in a NUL, which ir_alloc left there. */
    if (length > 0) {
        memcpy(text, bytes, length);
    }
    c_symbol(&symbol, t, "anti_licenses");
    text_appendf(out, "    .section %s\n    .globl %s\n%s:\n",
                 data_sections[target_info(t)->format], text_cstr(&symbol),
                 text_cstr(&symbol));
    byte_rows(out, text, 0, length + 1);
    free(text);
    text_free(&symbol);
}

/* DESIGN: the copy of the package header in a static library is an object
   file, because Apple's linker refuses an archive member of another kind.
   No symbol names the section, so no link takes the member. */
void emit_package(struct text *out, enum target t, const char *bytes,
                  size_t length)
{
    static const char *const sections[] = {
        [FORMAT_ELF] = ".anti_package,\"\",@progbits",
        [FORMAT_MACHO] = "__DATA,__anti_package",
        [FORMAT_COFF] = ".anti_package,\"dr\"",
    };
    build_version(out, t);
    text_appendf(out, "    .section %s\n", sections[target_info(t)->format]);
    byte_rows(out, (const unsigned char *)bytes, 0, length);
}

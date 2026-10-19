/* The module of LLVM IR text and the small texts beside it. It writes
   the names of the symbols, the globals, the imports of a COFF plugin,
   the alias of main, the declarations, the attributes, the metadata, the
   roots and the constructors, and llvm_emit_module puts the parts in
   order. The texts of a package header, a bundle marker, a notice and the
   names of a .def file stand here as well. */

#include "llvm_emit.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "llvm_debug.h"
#include "llvm_emit_parts.h"
#include "llvm_target.h"
#include "rt_abi.h"
#include "shared.h"
#include "../rt/plugin_index.h"

/* Whether name is an LLVM identifier that needs no quotes. */
static bool plain_name(const char *name)
{
    const char *p;

    if (*name == '\0' || (*name >= '0' && *name <= '9')) {
        return false;
    }
    for (p = name; *p != '\0'; p++) {
        unsigned char ch = (unsigned char)*p;
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') || ch == '-' || ch == '$' ||
              ch == '.' || ch == '_')) {
            return false;
        }
    }
    return true;
}

/* Append name after sigil, in quotes with \XX escapes where it is no
   plain identifier. */
static void llvm_name(struct text *out, char sigil, const char *name)
{
    const char *p;

    text_appendf(out, "%c", sigil);
    if (plain_name(name)) {
        text_append(out, name);
        return;
    }
    text_append(out, "\"");
    for (p = name; *p != '\0'; p++) {
        unsigned char ch = (unsigned char)*p;
        if (ch < 0x20 || ch >= 0x7f || ch == '"' || ch == '\\') {
            text_appendf(out, "\\%02X", ch);
        } else {
            text_appendf(out, "%c", ch);
        }
    }
    text_append(out, "\"");
}

/* DESIGN: a function takes the symbol of target_c_symbol or
   target_mangle, which the runtime, the C headers and the symbolizer
   read. llc applies the mangling of the triple, so the text drops the
   leading _ of Mach-O, which llc writes again. Append symbol, the symbol
   of target t, as the text names it, and free it. */
static void text_symbol(struct text *out, enum target t, struct text *symbol)
{
    const char *name = text_cstr(symbol);

    if (target_info(t)->format == FORMAT_MACHO && name[0] == '_') {
        name++;
    }
    text_append(out, name);
    text_free(symbol);
}

static void function_symbol(struct text *out, enum target t,
                            const struct ir_function *f)
{
    struct text symbol = {0};

    if (f->module == NULL || f->exported) {
        target_c_symbol(&symbol, t, f->name);
    } else {
        target_mangle(&symbol, t, f->module, f->name);
    }
    text_symbol(out, t, &symbol);
}

void llvm_emit_function_name(struct text *out, enum target t,
                             const struct ir_function *f)
{
    struct text symbol = {0};

    function_symbol(&symbol, t, f);
    llvm_name(out, '@', text_cstr(&symbol));
    text_free(&symbol);
}

/* The symbol of a global as the text names it. */
static void global_name(struct text *out, enum target t,
                        const struct ir_global *g)
{
    struct text symbol = {0};

    if (g->exported) {
        target_c_symbol(&symbol, t, g->name);
    } else {
        target_mangle(&symbol, t, g->module, g->name);
    }
    text_symbol(out, t, &symbol);
}

static void global_sigil(struct text *out, char sigil, enum target t,
                         const struct ir_global *g)
{
    struct text name = {0};

    global_name(&name, t, g);
    llvm_name(out, sigil, text_cstr(&name));
    text_free(&name);
}

void llvm_emit_global_symbol(struct text *out, enum target t,
                             const struct ir_global *g)
{
    global_sigil(out, '@', t, g);
}

/* Append c"..." of length bytes, with \XX escapes. */
static void byte_string(struct text *out, const uint8_t *bytes,
                        uint64_t length)
{
    uint64_t k;

    text_append(out, "c\"");
    for (k = 0; k < length; k++) {
        unsigned char ch = bytes[k];
        if (ch < 0x20 || ch >= 0x7f || ch == '"' || ch == '\\') {
            text_appendf(out, "\\%02X", ch);
        } else {
            text_appendf(out, "%c", ch);
        }
    }
    text_append(out, "\"");
}

/* Append the member of the bytes from up to to of g to the types and the
   values of its struct, zeroinitializer for a run of zeros. */
static void byte_member(struct text *types, struct text *values,
                        const struct ir_global *g, uint64_t from, uint64_t to)
{
    bool zero = true;
    uint64_t k;

    for (k = from; k < to; k++) {
        zero = zero && g->bytes[k] == 0;
    }
    text_appendf(types, "%s[%" PRIu64 " x i8]", types->length > 0 ? ", " : "",
                 to - from);
    text_appendf(values, "%s[%" PRIu64 " x i8] ",
                 values->length > 0 ? ", " : "", to - from);
    if (zero) {
        text_append(values, "zeroinitializer");
    } else {
        byte_string(values, g->bytes + from, to - from);
    }
}

static int reloc_order(const void *a, const void *b)
{
    const struct ir_reloc *x = a;
    const struct ir_reloc *y = b;

    return x->offset < y->offset ? -1 : x->offset > y->offset;
}

/* Whether g is the datum of a copy of a generic in an object of one
   module, which every module that uses it defines. */
static bool global_once(const struct emitter *e, const struct ir_global *g)
{
    return e->o->one_module && ir_is_copy_name(g->name);
}

/* Whether the text is a COFF plugin, which reaches the names of its
   host through the __imp_ entries of the import library of the host. */
bool llvm_emit_imports(const struct emitter *e)
{
    return e->m->plugin &&
           target_info(e->o->target)->format == FORMAT_COFF;
}

/* Whether relocation r of a global names a function or a datum of the
   host of a COFF plugin. */
static bool imported(const struct emitter *e, const struct ir_reloc *r)
{
    return llvm_emit_imports(e) &&
           (r->fn ? e->m->functions[r->global]->is_extern
                  : e->m->globals[r->global]->is_extern);
}

/* DESIGN: a COFF plugin holds the address of the __imp_ entry of a name
   of its host where its data holds an address of the host. The loader
   replaces it with the address the entry holds, at each place
   anti_rt_imports lists. Append the entry of r, which global g holds at
   its offset, to values, and record the place. */
static void import_place(struct emitter *e, struct text *values,
                         const struct ir_global *g, const struct ir_reloc *r)
{
    struct text entry = {0};
    struct text line = {0};

    text_append(&entry, "__imp_");
    if (r->fn) {
        function_symbol(&entry, e->o->target, e->m->functions[r->global]);
    } else {
        global_name(&entry, e->o->target, e->m->globals[r->global]);
    }
    llvm_name(values, '@', text_cstr(&entry));
    text_appendf(&line, "\n%s\n", text_cstr(&entry));
    if (strstr(text_cstr(&e->imports), text_cstr(&line)) == NULL) {
        text_appendf(&e->imports, "%s%s\n", e->imports.length == 0 ? "\n" : "",
                     text_cstr(&entry));
    }
    text_append(&e->places, e->place_count++ > 0 ? ", ptr " : "ptr ");
    if (r->offset == 0) {
        llvm_emit_global_symbol(&e->places, e->o->target, g);
    } else {
        text_append(&e->places, "getelementptr (i8, ptr ");
        llvm_emit_global_symbol(&e->places, e->o->target, g);
        text_appendf(&e->places, ", i64 %" PRIu64 ")", r->offset);
    }
    text_free(&entry);
    text_free(&line);
}

/* The declaration of every __imp_ entry the data of a COFF plugin names,
   and the table anti_rt_imports of the places that hold one. The table
   stands in every COFF plugin, since its .def file exports it, and in
   writable data, since the loader marks it done. */
static void import_table(struct emitter *e)
{
    const char *p = text_cstr(&e->imports);

    if (!llvm_emit_imports(e)) {
        return;
    }
    while (*p != '\0') {
        const char *end;
        struct text entry = {0};
        if (*p == '\n') {
            p++;
            continue;
        }
        end = strchr(p, '\n');
        text_append_bytes(&entry, p, (size_t)(end - p));
        llvm_name(e->out, '@', text_cstr(&entry));
        text_append(e->out, " = external global ptr\n");
        text_free(&entry);
        p = end;
    }
    if (e->place_count == 0) {
        text_append(e->out, "@anti_rt_imports = dso_local global <{ i64, "
                            "i64 }> <{ i64 0, i64 0 }>, align 8\n");
        return;
    }
    text_appendf(e->out, "@anti_rt_imports = dso_local global <{ i64, i64, "
                         "[%zu x ptr] }> <{ i64 %zu, i64 0, [%zu x ptr] "
                         "[%s] }>, align 8\n",
                 e->place_count, e->place_count, e->place_count,
                 text_cstr(&e->places));
}

/* Whether module is among the count entries of list. */
static bool listed(const char *const *list, size_t count, const char *module)
{
    size_t i;

    for (i = 0; i < count; i++) {
        if (strcmp(list[i], module) == 0) {
            return true;
        }
    }
    return false;
}

/* The list of the modules a plugin reaches, every module of its
   functions and globals in the order they come, which
   src/rt/plugin_index.h describes. */
static void plugin_modules(struct emitter *e, const struct ir_module *m)
{
    const char **list;
    struct text bytes = {0};
    size_t count = 0;
    size_t i;

    if (!m->plugin) {
        return;
    }
    list = alloc_zeroed(m->function_count + m->global_count + 1, sizeof *list);
    for (i = 0; i < m->function_count + m->global_count; i++) {
        const char *module = i < m->function_count
                                 ? m->functions[i]->module
                                 : m->globals[i - m->function_count]->module;
        if (module != NULL && !listed(list, count, module)) {
            list[count++] = module;
        }
    }
    text_append(&bytes, ANTI_PLUGIN_MODULES);
    for (i = 0; i < count; i++) {
        text_appendf(&bytes, " %s", list[i]);
    }
    text_appendf(e->out, "@%s = dso_local constant [%zu x i8] c\"%s\\00\", "
                         "align 1\n",
                 ANTI_PLUGIN_MODULES_SYMBOL, bytes.length + 1,
                 text_cstr(&bytes));
    text_free(&bytes);
    free(list);
}

/* DESIGN: a global is a packed struct, so its size and its offsets are
   the ones layout_data computed, whatever the data layout string says.
   The section "Globals, sections and constructors" of
   docs/work-order-llvm-back-end.md gives the form. The bytes between two
   addresses are one member of [N x i8], and each address a member of ptr
   that names the global or the function. A global the program writes is
   a global, every other one a constant, which llc puts in the read-only
   section of each format, or in the one the loader writes once where it
   holds an address. The linkage follows the one of a function. A global
   the runtime defines is a declaration of its bytes. */
static void global(struct emitter *e, const struct ir_global *g, bool twin)
{
    enum target t = e->o->target;
    bool coff = target_info(t)->format == FORMAT_COFF;
    uint64_t word = layout_size(e->l, ir_scalar(IR_PTR));
    struct text types = {0};
    struct text values = {0};
    struct ir_reloc *relocs;
    uint64_t from = 0;
    bool zero = true;
    bool writable = false;
    size_t i;

    if (g->is_extern && twin) {
        return;
    }
    llvm_emit_global_symbol(e->out, t, g);
    if (g->is_extern) {
        text_appendf(e->out, " = external %sglobal [%" PRIu64 " x i8], "
                             "align %" PRIu64 "\n",
                     llvm_emit_imports(e) ? "dllimport " : "", g->size,
                     g->align > 0 ? g->align : 1);
        return;
    }
    text_append(e->out, " = ");
    if (!g->exported && !e->o->one_module && !e->o->exports) {
        text_append(e->out, "internal ");
    } else {
        if (global_once(e, g)) {
            text_append(e->out, "weak_odr ");
        }
        if (g->exported) {
            text_append(e->out, "dso_local ");
        } else if (e->o->one_module && !e->o->exports && !coff) {
            text_append(e->out, "hidden ");
        }
    }
    relocs = alloc_zeroed(g->reloc_count + 1, sizeof *relocs);
    if (g->reloc_count > 0) {
        memcpy(relocs, g->relocs, g->reloc_count * sizeof *relocs);
        qsort(relocs, g->reloc_count, sizeof *relocs, reloc_order);
    }
    for (i = 0; i < g->reloc_count; i++) {
        if (relocs[i].offset > from) {
            byte_member(&types, &values, g, from, relocs[i].offset);
        }
        text_appendf(&types, "%sptr", types.length > 0 ? ", " : "");
        text_appendf(&values, "%sptr ", values.length > 0 ? ", " : "");
        if (imported(e, &relocs[i])) {
            import_place(e, &values, g, &relocs[i]);
            writable = true;
        } else if (relocs[i].fn) {
            llvm_emit_function_name(&values, t,
                                    e->m->functions[relocs[i].global]);
        } else {
            llvm_emit_global_symbol(&values, t,
                                    e->m->globals[relocs[i].global]);
        }
        from = relocs[i].offset + word;
    }
    if (from < g->size || g->reloc_count == 0) {
        byte_member(&types, &values, g, from, g->size);
    }
    for (i = 0; i < g->size; i++) {
        zero = zero && g->bytes[i] == 0;
    }
    text_appendf(e->out, "%s <{ %s }> ",
                 g->mutable || writable ? "global" : "constant",
                 text_cstr(&types));
    if (zero && g->reloc_count == 0) {
        text_append(e->out, "zeroinitializer");
    } else {
        text_appendf(e->out, "<{ %s }>", text_cstr(&values));
    }
    if (global_once(e, g) && coff) {
        text_append(e->out, ", comdat");
    }
    text_appendf(e->out, ", align %" PRIu64 "\n", g->align > 0 ? g->align : 1);
    free(relocs);
    text_free(&types);
    text_free(&values);
}

/* DESIGN: the runtime calls main through its entry, the name RUNTIME_ENTRY
   of RUNTIME_MODULE. An alias gives main the second name, which stays global while main
   itself may be internal. */
static void entry(struct emitter *e, const struct ir_module *m)
{
    struct text symbol = {0};
    struct text name = {0};
    size_t i;

    for (i = 0; i < m->function_count; i++) {
        const struct ir_function *f = m->functions[i];
        if (!llvm_emit_is_main(e, f)) {
            continue;
        }
        target_mangle(&symbol, e->o->target, RUNTIME_MODULE, RUNTIME_ENTRY);
        text_symbol(&name, e->o->target, &symbol);
        llvm_name(e->out, '@', text_cstr(&name));
        text_appendf(e->out, " = alias %s, ptr ", text_cstr(&e->entry));
        llvm_emit_function_name(e->out, e->o->target, f);
        text_append(e->out, "\n\n");
        text_free(&name);
        return;
    }
}

static void declaration(struct emitter *e, const struct ir_function *f)
{
    e->f = f;
    text_append(e->out,
                llvm_emit_imports(e) ? "declare dllimport " : "declare ");
    if (llvm_emit_signature(e, f, false, NULL)) {
        llvm_emit_effects(e->out, f->effects, f->guarantees);
        text_append(e->out, f->never_returns ? " noreturn cold #1\n"
                                             : " #1\n");
    }
}

/* DESIGN: every Anti function is nounwind, since Anti has no exceptions
   and fails through a return value. A declaration is a function of
   another module or a function of C, which unwinds through no Anti frame
   either. Windows walks a stack through the unwind tables of each frame,
   and its frames of a page or more call the stack probe. Every other
   target keeps the frame record of a function that calls another, since
   anti_rt_trace_walk and the report of --memory-checks follow the chain of
   records. Eddie decided it on 2026-10-04, see "Attributes and metadata"
   in docs/work-order-llvm-back-end.md. */
static void attributes(struct emitter *e)
{
    const struct target_info *info = target_info(e->o->target);
    bool windows = info->os == OS_WINDOWS;
    const char *tune = llvm_tune_cpu(e->o->target);

    text_appendf(e->out, "attributes #0 = { nounwind%s \"frame-pointer\"="
                         "\"%s\"%s \"target-cpu\"=\"%s\" "
                         "\"target-features\"=\"%s\"",
                 windows ? " uwtable(sync)" : "",
                 windows ? "none" : "non-leaf",
                 windows ? " \"stack-probe-size\"=\"4096\"" : "",
                 llvm_target_cpu(e->o->cpu),
                 llvm_target_features(e->o->cpu));
    if (tune != NULL) {
        text_appendf(e->out, " \"tune-cpu\"=\"%s\"", tune);
    }
    text_append(e->out, " }\n");
    text_append(e->out, "attributes #1 = { nounwind }\n\n");
}

static bool position_independent(enum target t)
{
    return strcmp(llvm_relocation_model(t), "pic") == 0;
}

static bool on_windows(enum target t)
{
    return target_info(t)->os == OS_WINDOWS;
}

/* The module flags after wchar_size, each written for the targets its
   test takes, in the order of their ids. */
static const struct {
    bool (*wanted)(enum target t);
    const char *node;
} module_flags[] = {
    {position_independent, "!{i32 8, !\"PIC Level\", i32 2}"},
    {on_windows, "!{i32 7, !\"uwtable\", i32 2}"},
};

/* The module flags, the identification and the weights of the cold
   branches, in the ids the branches named. */
static void metadata(struct emitter *e, uint32_t flag_count)
{
    uint32_t i;
    uint32_t id = 0;
    size_t k;

    text_append(e->out, "!llvm.module.flags = !{");
    for (i = 0; i < flag_count; i++) {
        text_appendf(e->out, "%s!%" PRIu32, i > 0 ? ", " : "", i);
    }
    llvm_debug_flags(&e->debug, e->out);
    text_appendf(e->out, "}\n!llvm.ident = !{!%" PRIu32 "}\n", flag_count);
    text_appendf(e->out, "!%" PRIu32 " = !{i32 1, !\"wchar_size\", i32 %"
                         PRIu64 "}\n",
                 id++, layout_size(e->l, ir_scalar(IR_CWCHAR)));
    for (k = 0; k < sizeof module_flags / sizeof module_flags[0]; k++) {
        if (module_flags[k].wanted(e->o->target)) {
            text_appendf(e->out, "!%" PRIu32 " = %s\n", id++,
                         module_flags[k].node);
        }
    }
    text_appendf(e->out, "!%" PRIu32 " = !{!\"antic %s\"}\n", id,
                 ANTIC_VERSION);
    if (e->cold_used[COLD_THEN]) {
        text_appendf(e->out, "!%" PRIu32 " = !{!\"branch_weights\", i32 1, "
                             "i32 2000}\n",
                     e->cold_id + COLD_THEN);
    }
    if (e->cold_used[COLD_ELSE]) {
        text_appendf(e->out, "!%" PRIu32 " = !{!\"branch_weights\", "
                             "i32 2000, i32 1}\n",
                     e->cold_id + COLD_ELSE);
    }
    if (e->facts) {
        text_appendf(e->out, "!%" PRIu32 " = !{}\n", e->facts_id);
    }
    for (i = 0; i < e->range_count; i++) {
        const char *type = llvm_emit_value_type(e, e->ranges[i].type);
        text_appendf(e->out, "!%" PRIu32 " = !{%s ", e->range_id + (uint32_t)i,
                     type);
        llvm_emit_int_constant(e->out, e->ranges[i].type, e->ranges[i].low);
        text_appendf(e->out, ", %s ", type);
        llvm_emit_int_constant(e->out, e->ranges[i].type, e->ranges[i].high);
        text_append(e->out, "}\n");
    }
    llvm_emit_tbaa_nodes(e);
    llvm_debug_finish(&e->debug, e->out);
}

/* The number of module flags of the target: the size of wchar_t, the
   level of position-independent code where the relocation model is pic,
   and the unwind tables of Windows. */
/* The count of the module flags of target t, wchar_size and those of
   module_flags it takes. */
static uint32_t flag_count(enum target t)
{
    uint32_t count = 1;
    size_t i;

    for (i = 0; i < sizeof module_flags / sizeof module_flags[0]; i++) {
        count += module_flags[i].wanted(t);
    }
    return count;
}

/* DESIGN: two globals of the IR may name one symbol, as every module
   names the descriptor of the root that the runtime defines. A
   declaration is written once, and not at all for a symbol the text
   defines. Whether the declaration of global i is such a twin. */
static bool declared_elsewhere(const struct ir_module *m,
                               const struct text *symbols, size_t i)
{
    size_t j;

    for (j = 0; j < m->global_count; j++) {
        if (j != i && (j < i || !m->globals[j]->is_extern) &&
            strcmp(text_cstr(&symbols[i]), text_cstr(&symbols[j])) == 0) {
            return true;
        }
    }
    return false;
}

/* The name of the notice anti_licenses in the text. */
static void notice_symbol(struct text *out, enum target t)
{
    struct text symbol = {0};

    target_c_symbol(&symbol, t, "anti_licenses");
    text_symbol(out, t, &symbol);
}

/* DESIGN: main keeps its own frame and its own name in the program, as
   the object link gave it, so a trace, the map of a symbols archive and
   a PDB name the main of the program. Under LTO, which a release build
   links through by default, two things took both away. lld inlined main
   into the C main of the runtime, and the optimizer gives an internal
   function that only an alias names the name of the alias, which is the
   entry of the runtime here. main is noinline, and llvm.compiler.used
   keeps the optimizer from the rename while it leaves the link free to
   drop what it would. The object link never inlined main, since the
   runtime calls it from an object of its own. */
static void main_kept(struct emitter *e, const struct ir_module *m)
{
    size_t i;

    for (i = 0; i < m->function_count; i++) {
        const struct ir_function *f = m->functions[i];
        if (llvm_emit_is_main(e, f)) {
            text_append(e->out, "@llvm.compiler.used = appending global "
                                "[1 x ptr] [ptr ");
            llvm_emit_function_name(e->out, e->o->target, f);
            text_append(e->out, "], section \"llvm.metadata\"\n");
            return;
        }
    }
}

/* DESIGN: the link drops what nothing refers to, so what is reached by
   name alone stands in llvm.used, which makes it a root in every format:
   SHF_GNU_RETAIN on ELF, no_dead_strip on Mach-O and /INCLUDE on COFF.
   Those are the notice anti_licenses, whose licence texts every binary
   ships and in which the trace of src/rt/trace.c and `anti symbols` find
   the build id by the symbol, and the functions marked by_name, such as
   the options hook that the runtime of AddressSanitizer looks up. A
   module holds one llvm.used, so this one names both. See the entry on
   dropped code under "Scope and toolchain" in docs/decisions.md.

   DESIGN: the object of a bundled Mach-O library refers to the marker
   of its package, so a link that takes the object from the archive
   takes the marker member with it. Nothing of the library reads the
   marker, so a private constant holds its address and llvm.used keeps
   the constant. See bundle in driver_library.c. */
static void roots(struct emitter *e, const struct ir_module *m)
{
    struct text entries = {0};
    size_t count = 0;
    size_t i;

    if (e->o->bundle != NULL) {
        llvm_name(e->out, '@', e->o->bundle);
        text_append(e->out, " = external hidden global i8\n"
                            "@anti.bundle = private constant ptr ");
        llvm_name(e->out, '@', e->o->bundle);
        text_append(e->out, "\n");
        text_append(&entries, "ptr @anti.bundle");
        count++;
    }
    if (e->o->notice) {
        struct text symbol = {0};
        notice_symbol(&symbol, e->o->target);
        text_append(&entries, count > 0 ? ", ptr " : "ptr ");
        llvm_name(&entries, '@', text_cstr(&symbol));
        text_free(&symbol);
        count++;
    }
    /* DESIGN: a plugin converts an f16 through the runtime of its host,
       below x86-64-v3, as the host does. A host whose level converts
       through the runtime refers to both conversions, so its link takes
       them and exports them whether it converts one itself or not, by
       T121. */
    if (e->o->exports && llvm_emit_f16_calls(e)) {
        llvm_emit_runtime_function(e, "float", RT_FN_F16_TO_F32, "i32");
        llvm_emit_runtime_function(e, "i32", RT_FN_F32_TO_F16, "float");
        text_appendf(e->out, "@anti.f16 = private constant [2 x ptr] "
                             "[ptr @%s, ptr @%s]\n",
                     rt_name(RT_FN_F16_TO_F32), rt_name(RT_FN_F32_TO_F16));
        text_append(&entries, count > 0 ? ", ptr " : "ptr ");
        text_append(&entries, "@anti.f16");
        count++;
    }
    for (i = 0; i < m->function_count; i++) {
        const struct ir_function *f = m->functions[i];
        if ((f->by_name && !f->is_extern) || llvm_emit_local_export(e, f)) {
            text_append(&entries, count++ > 0 ? ", ptr " : "ptr ");
            llvm_emit_function_name(&entries, e->o->target, f);
        }
    }
    if (count > 0) {
        text_appendf(e->out, "@llvm.used = appending global [%zu x ptr] "
                             "[%s], section \"llvm.metadata\"\n",
                     count, text_cstr(&entries));
    }
    text_free(&entries);
    main_kept(e, m);
}

/* DESIGN: the functions that run before main are the one of each module
   that compiles its patterns and, in a library for C, the constructor of
   the runtime. llvm.global_ctors names them, at the priority of a C
   constructor, and llc writes the section of each format: .init_array,
   __mod_init_func or .CRT$XCU. */
static void constructors(struct emitter *e, const struct ir_module *m)
{
    struct text entries = {0};
    size_t count = 0;
    bool declared = false;
    size_t i;

    for (i = 0; i < m->function_count; i++) {
        const struct ir_function *f = m->functions[i];
        if (ir_is_patterns_start(f)) {
            text_appendf(&entries, "%s{ i32, ptr, ptr } { i32 65535, ptr ",
                         count++ > 0 ? ", " : "");
            llvm_emit_function_name(&entries, e->o->target, f);
            text_append(&entries, ", ptr null }");
        }
        declared = declared ||
                   (e->o->constructor != NULL && f->is_extern &&
                    f->module == NULL &&
                    strcmp(f->name, e->o->constructor) == 0);
    }
    if (e->o->constructor != NULL) {
        struct text symbol = {0};
        struct text symbol_name = {0};
        const char *name;
        target_c_symbol(&symbol, e->o->target, e->o->constructor);
        text_symbol(&symbol_name, e->o->target, &symbol);
        name = text_cstr(&symbol_name);
        text_appendf(&entries, "%s{ i32, ptr, ptr } { i32 65535, ptr ",
                     count++ > 0 ? ", " : "");
        llvm_name(&entries, '@', name);
        text_append(&entries, ", ptr null }");
        if (!declared) {
            text_append(&e->intrinsics, "declare void ");
            llvm_name(&e->intrinsics, '@', name);
            text_append(&e->intrinsics, "() #1\n");
        }
        text_free(&symbol_name);
    }
    if (count > 0) {
        text_appendf(e->out, "@llvm.global_ctors = appending global [%zu x "
                             "{ i32, ptr, ptr }] [%s]\n",
                     count, text_cstr(&entries));
    }
    text_free(&entries);
}

/* The name of the source, the data layout and the triple of t. */
static void head(struct text *out, enum target t, const char *source)
{
    text_appendf(out, "source_filename = \"%s\"\n", source);
    text_appendf(out, "target datalayout = \"%s\"\n", llvm_data_layout(t));
    text_append(out, "target triple = \"");
    llvm_triple(out, t);
    text_append(out, "\"\n\n");
}

/* DESIGN: the package header and the notice keep a section of their own
   per object format. No symbol names the copy of the package header, so
   llvm.used keeps the private constant through every pass. */
void llvm_emit_package(struct text *out, enum target t, const char *bytes,
                       size_t length)
{
    static const char *const sections[] = {
        [FORMAT_ELF] = ".anti_package",
        [FORMAT_MACHO] = "__DATA,__anti_package",
        [FORMAT_COFF] = ".anti_package",
    };

    head(out, t, "package");
    text_appendf(out, "@anti.package = private constant [%zu x i8] ",
                 length);
    byte_string(out, (const uint8_t *)bytes, length);
    text_appendf(out, ", section \"%s\"\n"
                      "@llvm.used = appending global [1 x ptr] "
                      "[ptr @anti.package], section \"llvm.metadata\"\n",
                 sections[target_info(t)->format]);
}

/* DESIGN: both symbols are hidden, as every symbol of the runtime is,
   so neither is an export of a program or a shared library that links
   the bundle. A hidden symbol is still one definition per link, and two
   of a name are a duplicate. */
void llvm_emit_bundle_marker(struct text *out, enum target t,
                             const char *symbol)
{
    head(out, t, "bundle");
    text_appendf(out, "@%s = hidden constant i8 0\n", RUNTIME_BUNDLE_MARKER);
    llvm_name(out, '@', symbol);
    text_append(out, " = hidden constant i8 0\n");
}

void llvm_emit_names(struct text *out, enum target t,
                     const struct ir_module *m)
{
    size_t i;

    for (i = 0; i < m->function_count; i++) {
        if (!m->functions[i]->is_extern) {
            function_symbol(out, t, m->functions[i]);
            text_append(out, "\n");
        }
    }
    for (i = 0; i < m->global_count; i++) {
        if (!m->globals[i]->is_extern) {
            global_name(out, t, m->globals[i]);
            text_append(out, " DATA\n");
        }
    }
}

void llvm_emit_licenses(struct text *out, enum target t, const char *bytes,
                        size_t length)
{
    static const char *const sections[] = {
        [FORMAT_ELF] = ".rodata",
        [FORMAT_MACHO] = "__TEXT,__const",
        [FORMAT_COFF] = ".rdata",
    };
    struct text symbol = {0};
    struct text text = {0};

    notice_symbol(&symbol, t);
    text_append_bytes(&text, bytes, length);
    llvm_name(out, '@', text_cstr(&symbol));
    text_appendf(out, " = dso_local constant [%zu x i8] ", length + 1);
    /* The texts end in a NUL, which text_cstr gives after the bytes. */
    byte_string(out, (const uint8_t *)text_cstr(&text), length + 1);
    text_appendf(out, ", section \"%s\", align 1\n",
                 sections[target_info(t)->format]);
    text_free(&symbol);
    text_free(&text);
}

bool llvm_emit_module(struct text *out, const struct llvm_emit_options *o,
                      const struct ir_module *m, struct layouts *l,
                      char *error, size_t error_size)
{
    struct emitter e;
    struct text declarations = {0};
    struct text data = {0};
    uint32_t flags = flag_count(o->target);
    struct text *symbols;
    bool comdats = false;
    size_t i;

    memset(&e, 0, sizeof e);
    e.o = o;
    e.l = l;
    e.error = error;
    e.error_size = error_size;
    e.cold_id = flags + 1;
    e.facts_id = e.cold_id + COLD_COUNT;
    e.facts = m->table_facts && llvm_emit_names_table(m);
    e.m = m;
    e.range_id = e.facts_id + (e.facts ? 1 : 0);
    llvm_emit_collect_ranges(&e, m);
    e.tbaa_id = e.range_id + (uint32_t)e.range_count;
    llvm_emit_collect_tbaa(&e, m);
    /* The ids of the debug metadata follow every other id, so the text
       without them names the ids of a build without -g. */
    llvm_debug_init(&e.debug, o->target, m, o->module, o->debug,
                    o->optimized, e.tbaa_id + llvm_emit_tbaa_count(&e),
                    o->spans);
    e.keeps_frame = alloc_zeroed(m->function_count + 1, sizeof *e.keeps_frame);
    llvm_emit_frames_kept(m, e.keeps_frame);
    head(out, o->target, o->module);
    for (i = 0; i < m->global_count; i++) {
        const struct ir_global *g = m->globals[i];
        if (!g->is_extern && global_once(&e, g) &&
            target_info(o->target)->format == FORMAT_COFF) {
            global_sigil(out, '$', o->target, g);
            text_append(out, " = comdat any\n");
            comdats = true;
        }
    }
    for (i = 0; i < m->function_count; i++) {
        const struct ir_function *f = m->functions[i];
        if (!f->is_extern && llvm_emit_link_once(&e, f) &&
            target_info(o->target)->format == FORMAT_COFF) {
            struct text symbol = {0};
            function_symbol(&symbol, o->target, f);
            llvm_name(out, '$', text_cstr(&symbol));
            text_append(out, " = comdat any\n");
            text_free(&symbol);
            comdats = true;
        }
    }
    if (comdats) {
        text_append(out, "\n");
    }
    /* The definitions go straight into out, where the spans of the
       debug information they carry stand. */
    for (i = 0; i < m->function_count && !e.failed; i++) {
        const struct ir_function *f = m->functions[i];
        e.out = f->is_extern ? &declarations : out;
        if (f->is_extern) {
            declaration(&e, f);
        } else {
            llvm_emit_definition(&e, f);
        }
    }
    e.out = &data;
    symbols = alloc_zeroed(m->global_count + 1, sizeof *symbols);
    for (i = 0; i < m->global_count; i++) {
        llvm_emit_global_symbol(&symbols[i], o->target, m->globals[i]);
    }
    for (i = 0; i < m->global_count && !e.failed; i++) {
        global(&e, m->globals[i],
               m->globals[i]->is_extern &&
                   declared_elsewhere(m, symbols, i));
    }
    for (i = 0; i < m->global_count; i++) {
        text_free(&symbols[i]);
    }
    free(symbols);
    import_table(&e);
    plugin_modules(&e, m);
    constructors(&e, m);
    roots(&e, m);
    if (!e.failed) {
        e.out = out;
        entry(&e, m);
        if (data.length > 0) {
            text_append_bytes(out, data.data, data.length);
            text_append(out, "\n");
        }
        if (declarations.length > 0 || e.intrinsics.length > 0) {
            text_append_bytes(out, declarations.data, declarations.length);
            text_append_bytes(out, e.intrinsics.data, e.intrinsics.length);
            text_append(out, "\n");
        }
        attributes(&e);
        metadata(&e, flags);
    }
    text_free(&data);
    text_free(&declarations);
    text_free(&e.intrinsics);
    text_free(&e.entry);
    text_free(&e.places);
    text_free(&e.imports);
    free(e.keeps_frame);
    free(e.ranges);
    free(e.tbaa_aggs);
    free(e.tbaa_fields);
    llvm_debug_free(&e.debug);
    return !e.failed;
}

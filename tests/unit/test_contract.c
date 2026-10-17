/* The layouts and the numbers that a module of the standard library and
   the runtime both write. A module declares in Anti what the runtime reads
   or fills in C, so neither side can name the other. These tests read the
   modules of src/std/ as antic compiles them and compare each layout and
   each number with the declaration of src/rt/, so a change on one side
   that the other lacks fails here on the host. */
#include "../../src/antic/platform.h"
#include "../binary_stdio.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "check.h"
#include "pipeline.h"
#include "arena.h"
#include "ast.h"
#include "diagnostic.h"
#include "files.h"
#include "ir.h"
#include "layout.h"
#include "lexer.h"
#include "lower.h"
#include "parser.h"
#include "rt_abi.h"
#include "sema.h"
#include "target.h"
#include "types.h"

#include "object.h"
#include "reflect.h"
#include "regex.h"
#include "trace.h"

/* The modules a test reads, in the order of their imports. */
#define MODULES_MAX 8

/* The modules of the standard library compiled as antic compiles them,
   each against the interfaces of the ones before it. Every module shares
   the memory pool and the types, as in antic. */
struct library {
    struct arena arena;
    struct types types;
    struct diagnostics diags;
    struct token_list tokens[MODULES_MAX];
    struct text sources[MODULES_MAX];
    struct ir_module irs[MODULES_MAX];
    const struct interface *interfaces[MODULES_MAX];
    const char *names[MODULES_MAX];
    size_t count;
};

static void library_open(struct library *lib)
{
    memset(lib, 0, sizeof *lib);
    types_init(&lib->types, &lib->arena);
}

static void library_close(struct library *lib)
{
    size_t i;

    for (i = 0; i < lib->count; i++) {
        ir_module_free(&lib->irs[i]);
        lexer_token_list_free(&lib->tokens[i]);
        text_free(&lib->sources[i]);
    }
    diagnostics_free(&lib->diags);
    arena_free(&lib->arena);
}

/* Compile the module name of src/std/, with probe after its text, and add
   its interface. A probe names a private item of the module as a public
   constant, so the test reads a number the module keeps to itself. */
static bool library_load(struct library *lib, const char *name,
                         const char *probe)
{
    struct text path = {0};
    struct text *source = &lib->sources[lib->count];
    struct ir_module *ir = &lib->irs[lib->count];
    struct interface *iface = arena_alloc(&lib->arena, sizeof *iface);
    struct module *module = NULL;
    const char *c;
    bool ok;

    text_append(&path, ANTIC_SOURCE_DIR "/src/std/");
    for (c = name; *c != '\0'; c++) {
        text_appendf(&path, "%c", *c == '.' ? '/' : *c);
    }
    text_append(&path, ".anti");
    ok = files_read(text_cstr(&path), source);
    if (!ok) {
        fprintf(stderr, "cannot read %s\n", text_cstr(&path));
    }
    text_free(&path);
    if (ok && probe != NULL) {
        text_append(source, probe);
    }
    ir_module_init(ir, &lib->arena, name);
    lib->names[lib->count] = name;
    lib->count++;
    ok = ok &&
         lexer_lex(text_cstr(source), source->length, &lib->arena,
                   &lib->diags, &lib->tokens[lib->count - 1]) &&
         parser_parse(text_cstr(source), &lib->tokens[lib->count - 1],
                      &lib->arena, &lib->diags, &module) &&
         sema_check(module, name, NULL, lib->interfaces, lib->count - 1,
                    &lib->types, &lib->arena, &lib->diags, false);
    if (!ok) {
        check_failures++;
        fprintf(stderr, "%s does not compile: %s\n", name,
                lib->diags.count > 0 ? lib->diags.items[0].message : "");
        return false;
    }
    /* The generics leave the module before lowering, as in the driver. */
    sema_strip_generics(module, &lib->arena);
    lower_module(module, name, ir, 0, NULL, 0, PACKAGE_VERSION_DEFAULT);
    sema_interface(module, name, &lib->arena, iface);
    lib->interfaces[lib->count - 1] = iface;
    return true;
}

/* The item of the interface of module that is called name, or NULL. */
static const struct symbol *library_item(const struct library *lib,
                                         const char *module, const char *name)
{
    size_t i;
    size_t k;

    for (i = 0; i < lib->count; i++) {
        const struct interface *iface = lib->interfaces[i];
        if (iface == NULL || strcmp(lib->names[i], module) != 0) {
            continue;
        }
        for (k = 0; k < iface->item_count; k++) {
            const struct symbol *s = iface->items[k];
            if (s->name.length == strlen(name) &&
                memcmp(s->name.text, name, s->name.length) == 0) {
                return s;
            }
        }
    }
    fprintf(stderr, "%s declares no %s\n", module, name);
    return NULL;
}

/* The value of the integer constant name of module, which must be value. */
static void constant_is(const struct library *lib, const char *module,
                        const char *name, int64_t value)
{
    const struct symbol *s = library_item(lib, module, name);

    if (s == NULL || s->kind != SYMBOL_CONST || s->value == NULL ||
        s->value->kind != CONST_INT) {
        check_failures++;
        fprintf(stderr, "%s.%s is no integer constant\n", module, name);
        return;
    }
    if ((int64_t)s->value->as.integer != value) {
        check_failures++;
        fprintf(stderr, "%s.%s is %lld in the module and %lld in src/rt/\n",
                module, name, (long long)(int64_t)s->value->as.integer,
                (long long)value);
    }
}

/* One name of an enum and the number the runtime gives it. */
struct enum_expect {
    const char *name;
    uint64_t number;
};

/* The enum name of module holds the names of expect, in that order and
   with those numbers, and no other. */
static void enum_is(const struct library *lib, const char *module,
                    const char *name, const struct enum_expect *expect,
                    size_t count)
{
    const struct symbol *s = library_item(lib, module, name);
    const struct type *t = s != NULL ? s->type : NULL;
    size_t i;

    if (t == NULL || t->kind != TYPE_ENUM) {
        check_failures++;
        fprintf(stderr, "%s.%s is no enum\n", module, name);
        return;
    }
    if (t->field_count != count) {
        check_failures++;
        fprintf(stderr, "%s.%s has %zu names, src/rt/ %zu\n", module, name,
                t->field_count, count);
    }
    for (i = 0; i < count && i < t->field_count; i++) {
        const struct struct_field *f = &t->fields[i];
        if (f->name.length != strlen(expect[i].name) ||
            memcmp(f->name.text, expect[i].name, f->name.length) != 0 ||
            f->number != expect[i].number) {
            check_failures++;
            fprintf(stderr, "%s.%s: name %zu is %.*s = %llu, src/rt/ has "
                    "%s = %llu\n", module, name, i, (int)f->name.length,
                    f->name.text, (unsigned long long)f->number,
                    expect[i].name, (unsigned long long)expect[i].number);
        }
    }
}

/* One member of a C struct: its offset and its size. */
struct c_member {
    size_t offset;
    size_t size;
};

#define C_MEMBER(type, member)                                             \
    {offsetof(struct type, member), sizeof(((struct type *)0)->member)}

/* The layout of a C struct: each member in the order the Anti struct
   declares its fields, and the size of the whole. */
struct c_struct {
    const struct c_member *members;
    size_t count;
    size_t size;
};

/* The struct name of module, laid out for the host, has the offsets and
   the size of c. */
static void layout_is(const struct library *lib, const char *module,
                      const char *name, const struct c_struct *c)
{
    struct text full = {0};
    const struct ir_module *ir = NULL;
    struct layouts layouts;
    const struct layout *l;
    char error[256];
    enum target host;
    uint32_t agg;
    size_t i;

    for (i = 0; i < lib->count; i++) {
        if (strcmp(lib->names[i], module) == 0) {
            ir = &lib->irs[i];
        }
    }
    text_appendf(&full, "%s.%s", module, name);
    agg = ir != NULL ? ir_agg_find(ir, text_cstr(&full)) : IR_NO_AGG;
    if (agg == IR_NO_AGG || !target_host(&host)) {
        check_failures++;
        fprintf(stderr, "%s is no struct the module lowers\n",
                text_cstr(&full));
        text_free(&full);
        return;
    }
    if (ir->aggs[agg]->field_count != c->count) {
        check_failures++;
        fprintf(stderr, "%s has %zu fields, src/rt/ %zu members\n",
                text_cstr(&full), ir->aggs[agg]->field_count, c->count);
    }
    CHECK(layout_init(&layouts, host, ir, error, sizeof error));
    l = layout_agg(&layouts, agg);
    if (l->size != c->size) {
        check_failures++;
        fprintf(stderr, "%s: %llu bytes in the module, %zu in src/rt/\n",
                text_cstr(&full), (unsigned long long)l->size, c->size);
    }
    for (i = 0; i < c->count && i < ir->aggs[agg]->field_count; i++) {
        uint64_t bytes = layout_size(&layouts, ir->aggs[agg]->fields[i].type);
        if (l->offsets[i] != c->members[i].offset ||
            bytes != c->members[i].size) {
            check_failures++;
            fprintf(stderr, "%s.%s: offset %llu and %llu bytes in the "
                    "module, %zu and %zu in src/rt/\n", text_cstr(&full),
                    ir->aggs[agg]->fields[i].name,
                    (unsigned long long)l->offsets[i],
                    (unsigned long long)bytes, c->members[i].offset,
                    c->members[i].size);
        }
    }
    layout_free(&layouts);
    text_free(&full);
}

#define C_STRUCT(name, members)                                            \
    {members, sizeof members / sizeof members[0], sizeof(struct name)}

/* DESIGN: anti.lang declares the two frames of a stack trace, and
   src/rt/trace.h fills them as struct anti_raw_frame and struct
   anti_frame. */
static void lang_frames(void)
{
    static const struct c_member raw[] = {
        C_MEMBER(anti_raw_frame, address),
        C_MEMBER(anti_raw_frame, module),
        C_MEMBER(anti_raw_frame, build_id),
        C_MEMBER(anti_raw_frame, base)
    };
    static const struct c_member named[] = {
        C_MEMBER(anti_frame, address),
        C_MEMBER(anti_frame, function),
        C_MEMBER(anti_frame, file),
        C_MEMBER(anti_frame, line)
    };
    static const struct c_struct raw_c = C_STRUCT(anti_raw_frame, raw);
    static const struct c_struct named_c = C_STRUCT(anti_frame, named);
    struct library lib;

    library_open(&lib);
    if (library_load(&lib, "anti.lang", NULL)) {
        layout_is(&lib, "anti.lang", "RawFrame", &raw_c);
        layout_is(&lib, "anti.lang", "Frame", &named_c);
    }
    library_close(&lib);
}

/* The private numbers of anti.regex that the runtime gives. */
static const char regex_probe[] =
    "\npub const PROBE_EXPENSIVE: int = EXPENSIVE;\n"
    "pub const PROBE_MISFIT: int = MISFIT;\n"
    "pub const PROBE_MISSING: int = MISSING;\n";

/* DESIGN: anti.regex holds the walk over the matches and the bytes that
   grow, which src/rt/patterns.c reads and writes as struct anti_cursor
   and struct anti_growing of src/rt/regex.h. The numbers a search and
   `patch` give below zero stand in regex.h as well. */
static void regex_contract(void)
{
    static const struct c_member cursor[] = {
        C_MEMBER(anti_cursor, pattern),
        C_MEMBER(anti_cursor, subject),
        C_MEMBER(anti_cursor, at),
        C_MEMBER(anti_cursor, options),
        C_MEMBER(anti_cursor, last),
        C_MEMBER(anti_cursor, skip),
        C_MEMBER(anti_cursor, left)
    };
    static const struct c_member growing[] = {
        C_MEMBER(anti_growing, bytes),
        C_MEMBER(anti_growing, length),
        C_MEMBER(anti_growing, capacity)
    };
    static const struct c_struct cursor_c = C_STRUCT(anti_cursor, cursor);
    static const struct c_struct growing_c = C_STRUCT(anti_growing, growing);
    struct library lib;

    library_open(&lib);
    if (library_load(&lib, "anti.lang", NULL) &&
        library_load(&lib, "anti.regex", regex_probe)) {
        layout_is(&lib, "anti.regex", "Cursor", &cursor_c);
        layout_is(&lib, "anti.regex", "Growing", &growing_c);
        constant_is(&lib, "anti.regex", "PROBE_EXPENSIVE",
                    ANTI_RT_REGEX_EXPENSIVE);
        constant_is(&lib, "anti.regex", "PROBE_MISFIT",
                    ANTI_RT_REGEX_MISFIT);
        constant_is(&lib, "anti.regex", "PROBE_MISSING",
                    ANTI_RT_REGEX_MISSING);
    }
    library_close(&lib);
}

/* DESIGN: anti.reflect declares the Value that src/rt/reflect.h reads as
   struct anti_value, the kinds of enum anti_value_kind, the codes of
   enum anti_reflect_reason and the type ids of enum anti_type of
   src/rt/object.h. */
static void reflect_contract(void)
{
    static const struct c_member value[] = {
        C_MEMBER(anti_value, kind),
        C_MEMBER(anti_value, data)
    };
    static const struct c_struct value_c = C_STRUCT(anti_value, value);
    static const struct enum_expect kinds[] = {
        {"None", ANTI_VALUE_NONE}, {"Int", ANTI_VALUE_INT},
        {"Uint", ANTI_VALUE_UINT}, {"Float", ANTI_VALUE_FLOAT},
        {"Bool", ANTI_VALUE_BOOL}, {"Char", ANTI_VALUE_CHAR},
        {"Str", ANTI_VALUE_STR}, {"Ptr", ANTI_VALUE_PTR}
    };
    static const struct enum_expect type_ids[] = {
        {"None", ANTI_TYPE_NONE}, {"Bool", ANTI_TYPE_BOOL},
        {"Char", ANTI_TYPE_CHAR}, {"I8", ANTI_TYPE_I8},
        {"I16", ANTI_TYPE_I16}, {"I32", ANTI_TYPE_I32},
        {"I64", ANTI_TYPE_I64}, {"CLong", ANTI_TYPE_CLONG},
        {"U8", ANTI_TYPE_U8}, {"U16", ANTI_TYPE_U16},
        {"U32", ANTI_TYPE_U32}, {"U64", ANTI_TYPE_U64},
        {"CULong", ANTI_TYPE_CULONG}, {"CWchar", ANTI_TYPE_CWCHAR},
        {"F32", ANTI_TYPE_F32}, {"F64", ANTI_TYPE_F64},
        {"Str", ANTI_TYPE_STR}, {"Ptr", ANTI_TYPE_PTR},
        {"Fn", ANTI_TYPE_FN}, {"Slice", ANTI_TYPE_SLICE},
        {"Array", ANTI_TYPE_ARRAY}, {"Struct", ANTI_TYPE_STRUCT},
        {"Union", ANTI_TYPE_UNION}, {"Enum", ANTI_TYPE_ENUM},
        {"Class", ANTI_TYPE_CLASS}, {"F16", ANTI_TYPE_F16},
        {"Variant", ANTI_TYPE_VARIANT}, {"Optional", ANTI_TYPE_OPTIONAL},
        {"Handle", ANTI_TYPE_HANDLE}, {"Regex", ANTI_TYPE_REGEX},
        {"Tuple", ANTI_TYPE_TUPLE}
    };
    struct library lib;

    library_open(&lib);
    if (library_load(&lib, "anti.lang", NULL) &&
        library_load(&lib, "anti.mem", NULL) &&
        library_load(&lib, "anti.text", NULL) &&
        library_load(&lib, "anti.reflect", NULL)) {
        layout_is(&lib, "anti.reflect", "Value", &value_c);
        enum_is(&lib, "anti.reflect", "ValueKind", kinds,
                sizeof kinds / sizeof kinds[0]);
        enum_is(&lib, "anti.reflect", "TypeId", type_ids,
                sizeof type_ids / sizeof type_ids[0]);
        constant_is(&lib, "anti.reflect", "BAD_INDEX",
                    ANTI_REFLECT_BAD_INDEX);
        constant_is(&lib, "anti.reflect", "NOT_CARRIED",
                    ANTI_REFLECT_NOT_CARRIED);
        constant_is(&lib, "anti.reflect", "WRONG_KIND",
                    ANTI_REFLECT_WRONG_KIND);
        constant_is(&lib, "anti.reflect", "WRONG_COUNT",
                    ANTI_REFLECT_WRONG_COUNT);
    }
    library_close(&lib);
}

/* The lowered module calls the C function name, the form in which antic
   finds it in the IR of a program. */
static void declares_extern(const struct library *lib, const char *module,
                            const char *name)
{
    size_t i;
    size_t k;

    for (i = 0; i < lib->count; i++) {
        const struct ir_module *ir = &lib->irs[i];
        if (strcmp(lib->names[i], module) != 0) {
            continue;
        }
        for (k = 0; k < ir->function_count; k++) {
            if (ir->functions[k]->module == NULL &&
                strcmp(ir->functions[k]->name, name) == 0) {
                return;
            }
        }
    }
    check_failures++;
    fprintf(stderr, "%s declares no extern fn %s\n", module, name);
}

/* DESIGN: antic finds four runtime functions by name that a module of
   the standard library declares with `extern fn`, which RT_FUNCTIONS
   leaves out. src/antic/rt_abi.h names each once, and each module
   declares the name antic looks for. */
static void library_names(void)
{
    struct library lib;

    library_open(&lib);
    if (library_load(&lib, "anti.lang", NULL) &&
        library_load(&lib, "anti.mem", NULL) &&
        library_load(&lib, "anti.text", NULL) &&
        library_load(&lib, "anti.reflect", NULL) &&
        library_load(&lib, "anti.plugin", NULL)) {
        declares_extern(&lib, "anti.mem", MEM_ALLOC);
        declares_extern(&lib, "anti.reflect", REFLECT_NEW);
        declares_extern(&lib, "anti.reflect", REFLECT_CALL);
        declares_extern(&lib, "anti.plugin", PLUGIN_LOAD);
    }
    library_close(&lib);
}

void test_contract(void)
{
    lang_frames();
    regex_contract();
    reflect_contract();
    library_names();
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "target.h"
#include "text.h"
#include "types.h"
#include "lower_lowerer.h"

/* DESIGN: anti.lang.Object declares seven public functions whose bodies
   live in the runtime, and nine hooks with empty bodies after them.
   Every class inherits all sixteen, so they take the first entries of
   every table. A class replaces one with a concrete function of the same
   name. The order of the nine is `enum anti_hook` of src/rt/object.h, and
   the unit test records_hook_entries pins the two together. */
static const char *const root_names[] = {
    "type_name", ROOT_TO_TEXT, "equals", "hash", "serialize", "destruct",
    "copy", ROOT_CREATED, ROOT_DESTROYED, ROOT_COPIED, ROOT_DISPATCHED,
    ROOT_JOINED, ROOT_ENTER, ROOT_LEAVE, ROOT_FAILED, ROOT_CHANGED
};

/* The parameters of each, `self` among them. */
static const unsigned char root_params[] = {
    1, 1, 2, 1, 2, 1, 2, 1, 1, 2, 1, 1, 2, 2, 3, 2
};

bool lower_same_name(const struct name *a, const struct name *b)
{
    return a->length == b->length &&
           memcmp(a->text, b->text, a->length) == 0;
}

/* The parameters of the member m, `self` among them. The out pointer of
   a `may fail` function is no parameter of the declaration. */
static size_t member_params(const struct item *m)
{
    return m->symbol != NULL && m->symbol->type != NULL
               ? m->symbol->type->param_count
               : (size_t)m->param_count + (m->has_self ? 1 : 0);
}

static void table_add(struct table *t, struct name name, size_t params,
                      const struct item *m, const char *runtime)
{
    size_t i;

    for (i = 0; i < t->count; i++) {
        if (lower_same_name(&t->entries[i].name, &name) &&
            t->entries[i].params == params) {
            t->entries[i].fn = m;
            return;
        }
    }
    t->entries = ir_grow(t->entries, &t->capacity, t->count,
                         sizeof *t->entries);
    t->entries[t->count].name = name;
    t->entries[t->count].params = params;
    t->entries[t->count].fn = m;
    t->entries[t->count].runtime = runtime;
    t->count++;
}

static void table_of(const struct type *t, struct table *out)
{
    size_t i;

    if (t == NULL) {
        return;
    }
    if (t->kind == TYPE_CLASS) {
        table_of(t->base, out);
    }
    if (t->kind == TYPE_CLASS && t->base == NULL) {
        for (i = 0; i < sizeof root_names / sizeof root_names[0]; i++) {
            struct name name;
            name.text = root_names[i];
            name.length = strlen(root_names[i]);
            table_add(out, name, root_params[i], NULL, root_names[i]);
        }
    }
    /* DESIGN: `concrete fn I::f` fills the table of I alone, and the
       primary table keeps the inherited entry or a plain body. A body
       qualified by a base fills the primary table. It wins over a plain
       body of the same level, so it is added after it. The rule is
       types_body_table's. */
    for (i = 0; i < t->member_count; i++) {
        const struct item *m = t->members[i];
        if (m->kind == ITEM_FN && m->pub &&
            types_body_table(t, m) == BODY_PLAIN) {
            table_add(out, m->name, member_params(m), m, NULL);
        }
    }
    for (i = 0; i < t->member_count; i++) {
        const struct item *m = t->members[i];
        if (m->kind == ITEM_FN && m->pub &&
            types_body_table(t, m) == BODY_BASE) {
            table_add(out, m->name, member_params(m), m, NULL);
        }
    }
}

/* The index of the entry that holds the function `name`, or 0 when the
   class has no such entry. Entry 0 is the descriptor, so a real entry is
   never 0. */
size_t lower_table_index(const struct type *t, const struct name *name,
                         size_t params)
{
    struct table table = {0};
    size_t i;
    size_t found = 0;

    table_of(t, &table);
    for (i = 0; i < table.count; i++) {
        if (lower_same_name(&table.entries[i].name, name) &&
            table.entries[i].params == params) {
            found = i + 1;
            break;
        }
    }
    free(table.entries);
    return found;
}

/* The offset of table entry index. The IR holds no sizes, so the width
   of a pointer stays symbolic. */
struct ir_operand lower_entry_offset(struct lowerer *l, size_t index)
{
    return lower_temp(
        l, ir_binary(l->f, l->b, IR_MUL, IR_I64,
                     ir_int_op(IR_I64, (uint64_t)index),
                     ir_sym_operand(l->m, ir_sym_size_of(l->m,
                                                         ir_scalar(IR_PTR)))));
}

/* DESIGN: the function record of a descriptor names one public function
   of the class and where its entry sits. A program reads the list and
   calls through the table, so reflection needs no name of its own. */
static uint32_t function_agg(struct lowerer *l)
{
    static const char name[] = "anti.rt.Function";
    struct ir_field fields[5];
    uint32_t agg = ir_agg_find(l->m, name);

    if (agg != IR_NO_AGG) {
        return agg;
    }
    memset(fields, 0, sizeof fields);
    fields[0].name = "name";
    fields[0].type = ir_scalar(IR_PTR);
    fields[1].name = "name_length";
    fields[1].type = ir_scalar(IR_I64);
    fields[2].name = "slot";
    fields[2].type = ir_scalar(IR_I64);
    fields[3].name = "param_count";
    fields[3].type = ir_scalar(IR_I64);
    fields[4].name = "signature";
    fields[4].type = ir_scalar(IR_PTR);
    return ir_struct_add(l->m, IR_AGG_STRUCT, name, fields, 5, false, 0);
}

/* The aggregate of a function list of n records. */
static uint32_t functions_agg(struct lowerer *l, size_t n)
{
    uint32_t record = function_agg(l);

    return lower_array_agg(l, "anti.rt.Function", ir_aggregate(record), n);
}

/* The aggregate of one field record of a descriptor. */
static uint32_t field_agg(struct lowerer *l)
{
    static const char name[] = "anti.rt.Field";
    struct ir_field fields[6];
    uint32_t agg = ir_agg_find(l->m, name);

    if (agg != IR_NO_AGG) {
        return agg;
    }
    memset(fields, 0, sizeof fields);
    fields[0].name = "name";
    fields[0].type = ir_scalar(IR_PTR);
    fields[1].name = "name_length";
    fields[1].type = ir_scalar(IR_I64);
    fields[2].name = "offset";
    fields[2].type = ir_scalar(IR_I64);
    fields[3].name = "type";
    fields[3].type = ir_scalar(IR_I64);
    fields[4].name = "owned";
    fields[4].type = ir_scalar(IR_I64);
    fields[5].name = "descriptor";
    fields[5].type = ir_scalar(IR_PTR);
    return ir_struct_add(l->m, IR_AGG_STRUCT, name, fields, 6, false, 0);
}

/* The aggregate of an array of n field records. */
uint32_t lower_fields_agg(struct lowerer *l, size_t n)
{
    uint32_t record = field_agg(l);

    return lower_array_agg(l, "anti.rt.Field", ir_aggregate(record), n);
}

/* The aggregate of a class descriptor. Every class shares it. */
uint32_t lower_descriptor_agg(struct lowerer *l)
{
    static const char name[] = "anti.rt.Descriptor";
    struct ir_field fields[15];
    uint32_t agg = ir_agg_find(l->m, name);

    if (agg != IR_NO_AGG) {
        return agg;
    }
    memset(fields, 0, sizeof fields);
    fields[0].name = "name";
    fields[0].type = ir_scalar(IR_PTR);
    fields[1].name = "name_length";
    fields[1].type = ir_scalar(IR_I64);
    fields[2].name = "parent";
    fields[2].type = ir_scalar(IR_PTR);
    fields[3].name = "size";
    fields[3].type = ir_scalar(IR_I64);
    fields[4].name = "depth";
    fields[4].type = ir_scalar(IR_I64);
    fields[5].name = "ancestors";
    fields[5].type = ir_scalar(IR_PTR);
    fields[6].name = "field_count";
    fields[6].type = ir_scalar(IR_I64);
    fields[7].name = "fields";
    fields[7].type = ir_scalar(IR_PTR);
    fields[8].name = "destruct";
    fields[8].type = ir_scalar(IR_PTR);
    /* DESIGN: the descriptor of an interface table records how far the
       sub-object sits from the start of the object. A pointer to the
       sub-object therefore leads back to the object itself, which is
       what `is`, `as`, `delete` and identity need. The descriptor of a
       class has zero there. */
    fields[9].name = "offset";
    fields[9].type = ir_scalar(IR_I64);
    fields[10].name = "function_count";
    fields[10].type = ir_scalar(IR_I64);
    fields[11].name = "functions";
    fields[11].type = ir_scalar(IR_PTR);
    /* DESIGN: every descriptor carries the version of the package that
       declared the class. An abstract class points at the record of its
       chain and its floor. The loader of a plugin reads both. */
    fields[12].name = "version";
    fields[12].type = ir_scalar(IR_PTR);
    fields[13].name = "version_length";
    fields[13].type = ir_scalar(IR_I64);
    fields[14].name = "versions";
    fields[14].type = ir_scalar(IR_PTR);
    return ir_struct_add(l->m, IR_AGG_STRUCT, name, fields, 15, false, 0);
}

/* The depth of a class in its chain. The root anti.lang.Object is 0. */
uint32_t lower_class_depth(const struct type *t)
{
    uint32_t depth = 0;

    for (; t != NULL && t->kind == TYPE_CLASS && t->base != NULL;
         t = t->base) {
        depth++;
    }
    return depth;
}

/* A global of the module named `<class>.<suffix>`, or NULL when the
   module has none. Every class builds its data once. On NULL,
   *module_out and *name_out receive the module and the name to give the
   global, and the caller frees both with free. */
struct ir_global *lower_class_global(struct lowerer *l, const struct type *t,
                                     const char *suffix, char **module_out,
                                     char **name_out)
{
    struct ir_module *m = l->m;
    char *module = lower_cstr(&t->module);
    struct text name = {0};
    struct ir_global *g;

    /* DESIGN: an export class gives its table and its descriptor the C
       names the generated header declares, so C code reads them. Every
       other class, and every struct, keeps the name `Type.part` of its
       module. */
    if (t->item_exported && t->kind == TYPE_CLASS &&
        (strcmp(suffix, "table") == 0 || strcmp(suffix, "descriptor") == 0)) {
        text_appendf(&name, "anti_%.*s_%s", (int)t->name.length,
                     t->name.text,
                     strcmp(suffix, "table") == 0 ? "vtable" : "descriptor");
    } else {
        type_symbol_name(&name, t);
        text_appendf(&name, ".%s", suffix);
    }
    g = lower_find_global(m, module, text_cstr(&name));
    /* DESIGN: a class's table and descriptor belong to the module that
       declares it, which writes them whether it builds one or not. A
       module that names a class of another refers to them, so a program
       holds one descriptor per class and `is` compares one address. */
    if (g == NULL && strcmp(module, l->module_name) != 0) {
        g = ir_global_add(m, module, text_cstr(&name), NULL, 0, 1);
        g->is_extern = true;
        g->exported = t->item_exported;
    }
    if (g == NULL) {
        *module_out = module;
        *name_out = lower_copy_text(&name);
    } else {
        free(module);
    }
    text_free(&name);
    return g;
}

/* The global `<Struct>.<suffix>` of the module that declares the struct,
   or NULL. On NULL, *module_out and *name_out receive the module and the
   name to give the global, and the caller frees both with free. */
static struct ir_global *struct_global(struct lowerer *l,
                                       const struct type *t,
                                       const char *suffix, char **module_out,
                                       char **name_out)
{
    struct ir_module *m = l->m;
    char *module = lower_cstr(&t->module);
    struct text name = {0};
    struct ir_global *g;

    type_symbol_name(&name, t);
    text_appendf(&name, ".%s", suffix);
    g = lower_find_global(m, module, text_cstr(&name));
    /* DESIGN: a struct's descriptor belongs to the module that declares
       it, as a class's does. A module that names the struct of another
       refers to it. A program then holds one descriptor per struct, and
       two field records of one struct hold one address. */
    if (g == NULL && strcmp(module, l->module_name) != 0) {
        g = ir_global_add(m, module, text_cstr(&name), NULL, 0, 1);
        g->is_extern = true;
    }
    if (g == NULL) {
        *module_out = module;
        *name_out = lower_copy_text(&name);
    } else {
        free(module);
    }
    text_free(&name);
    return g;
}

/* Whether the field list of a descriptor carries a record of the field.
   The base and the table pointer have none, and neither has a
   `transient` field, which no walk of the list reads. */
bool lower_listed_field(const struct struct_field *f)
{
    return f->form != FIELD_BASE && f->form != FIELD_TABLE && !f->transient;
}

/* The count of fields a descriptor lists: the class's own fields, with
   the base and the table pointer left out. Each class lists its own, and
   the parent descriptor holds the rest of the chain. */
size_t lower_own_fields(const struct type *t)
{
    size_t count = 0;
    size_t i;

    for (i = 0; i < t->field_count; i++) {
        if (lower_listed_field(&t->fields[i])) {
            count++;
        }
    }
    return count;
}

/* The type ids of enum anti_type in src/rt/object.h, in its order. The unit
   test records_type_ids pins the two together. */
enum type_id {
    TYPE_ID_NONE, TYPE_ID_BOOL, TYPE_ID_CHAR, TYPE_ID_I8, TYPE_ID_I16,
    TYPE_ID_I32, TYPE_ID_I64, TYPE_ID_CLONG, TYPE_ID_U8, TYPE_ID_U16,
    TYPE_ID_U32, TYPE_ID_U64, TYPE_ID_CULONG, TYPE_ID_CWCHAR, TYPE_ID_F32,
    TYPE_ID_F64, TYPE_ID_STR, TYPE_ID_PTR, TYPE_ID_FN, TYPE_ID_SLICE,
    TYPE_ID_ARRAY, TYPE_ID_STRUCT, TYPE_ID_UNION, TYPE_ID_ENUM,
    TYPE_ID_CLASS, TYPE_ID_F16
};

/* The type id of t alone, without the type it is built on. */
static uint64_t type_id_of(const struct type *t)
{
    static const enum type_id builtins[] = {
        [TYPE_BOOL] = TYPE_ID_BOOL,     [TYPE_CHAR] = TYPE_ID_CHAR,
        [TYPE_I8] = TYPE_ID_I8,         [TYPE_I16] = TYPE_ID_I16,
        [TYPE_I32] = TYPE_ID_I32,       [TYPE_I64] = TYPE_ID_I64,
        [TYPE_CLONG] = TYPE_ID_CLONG,   [TYPE_U8] = TYPE_ID_U8,
        [TYPE_U16] = TYPE_ID_U16,       [TYPE_U32] = TYPE_ID_U32,
        [TYPE_U64] = TYPE_ID_U64,       [TYPE_CULONG] = TYPE_ID_CULONG,
        [TYPE_CWCHAR] = TYPE_ID_CWCHAR, [TYPE_F32] = TYPE_ID_F32,
        [TYPE_F64] = TYPE_ID_F64,       [TYPE_STR] = TYPE_ID_STR,
        [TYPE_F16] = TYPE_ID_F16,
    };

    /* DESIGN: a Mutex and a channel are handles of objects that threads
       share. A Regex is the handle of a compiled pattern, and a Match
       holds slices of a searched text. A walk has nothing to write or copy
       there, so their type id is none, as a variant's is. `serialize`
       and `reflect.get` pass over them. */
    if (types_is_mutex(t) || types_is_chan(t) || types_is_regex(t) ||
        types_is_match(t) || types_is_object_lock(t)) {
        return TYPE_ID_NONE;
    }
    /* An `own fn` field is two words and owns its snapshot. No Value
       carries two words, and a copy would give the snapshot two owners,
       so its type id is none as well. */
    if (t->kind == TYPE_FN && t->context) {
        return TYPE_ID_NONE;
    }
    switch (t->kind) {
    case TYPE_POINTER: return TYPE_ID_PTR;
    case TYPE_FN: return TYPE_ID_FN;
    case TYPE_SLICE: return TYPE_ID_SLICE;
    case TYPE_ARRAY: return TYPE_ID_ARRAY;
    case TYPE_STRUCT: return t->is_union ? TYPE_ID_UNION : TYPE_ID_STRUCT;
    /* A tuple is an anonymous struct, and no module declares it, so it
       has the id of a struct and no descriptor of its own. */
    case TYPE_TUPLE: return TYPE_ID_STRUCT;
    case TYPE_ENUM: return TYPE_ID_ENUM;
    case TYPE_CLASS: return TYPE_ID_CLASS;
    default:
        /* An enum is signed under the ABI of Windows, so the index
           converts before the comparison. */
        return (size_t)t->kind < sizeof builtins / sizeof *builtins
                   ? (uint64_t)builtins[t->kind]
                   : TYPE_ID_NONE;
    }
}

/* DESIGN: a field record names the type of its field and never its
   width, so the record is the same on every target. A pointer, a slice,
   an array and an enum add the id of the type they are built on in the
   byte above their own. An enum there is written as its integer, so a
   walk knows the size of every element. */
static uint64_t type_id(const struct type *t)
{
    const struct type *on = t->kind == TYPE_ENUM ? t->base
                            : t->kind == TYPE_POINTER || t->kind == TYPE_SLICE ||
                                    t->kind == TYPE_ARRAY
                                ? t->element
                                : NULL;

    if (on != NULL && on->kind == TYPE_ENUM) {
        on = on->base;
    }
    return type_id_of(t) | (on != NULL ? type_id_of(on) << 8 : 0);
}

/* The descriptor a field of type t carries. It is the one of its class
   or struct, or of the class or struct that a pointer or a slice
   reaches. Every other type gives NULL, as a union and a Job do. */
static const struct ir_global *field_descriptor(struct lowerer *l,
                                                const struct type *t)
{
    if (t->kind == TYPE_POINTER || t->kind == TYPE_SLICE) {
        t = t->element;
    }
    if (t->kind == TYPE_CLASS) {
        return lower_class_descriptor(l, t);
    }
    return t->kind == TYPE_STRUCT ? lower_struct_descriptor(l, t) : NULL;
}

/* DESIGN: the field list of a class or a struct has one record per
   field it declares. A record holds the offset, the type id and the
   `own` bit. A field of class or struct type carries its descriptor as
   well. So does a pointer or a slice of one. A walk of the list then
   reaches the whole object. A bitfield has type id none, since no offset
   reaches its bits. `--no-reflect` drops the list and keeps the rest of
   the descriptor. */
struct ir_global *lower_class_fields(struct lowerer *l,
                                     const struct type *t)
{
    size_t count = lower_own_fields(t);
    struct ir_const *value;
    struct ir_global *g;
    struct token_text text;
    char *module;
    char *name;
    size_t i;
    size_t n = 0;

    g = t->kind == TYPE_CLASS
            ? lower_class_global(l, t, "fields", &module, &name)
            : struct_global(l, t, "fields", &module, &name);
    if (g != NULL) {
        return g;
    }
    value = ir_const_agg(l->m, ir_aggregate(lower_fields_agg(l, count)), count);
    for (i = 0; i < t->field_count; i++) {
        const struct struct_field *f = &t->fields[i];
        const struct ir_global *descriptor;
        struct ir_const *item;
        if (!lower_listed_field(f)) {
            continue;
        }
        item = ir_const_agg(l->m, ir_aggregate(field_agg(l)), 6);
        text.bytes = f->name.text;
        text.length = f->name.length;
        item->items[0].kind = IR_CONST_ADDR;
        item->items[0].scalar = IR_PTR;
        item->items[0].global = lower_literal_global(l, &text)->index;
        item->items[1].kind = IR_CONST_INT;
        item->items[1].scalar = IR_I64;
        item->items[1].integer = f->name.length;
        item->items[2].kind = IR_CONST_SYM;
        item->items[2].scalar = IR_I64;
        item->items[2].sym = ir_sym_offset_of(l->m, lower_agg_of(l, t),
                                              (uint32_t)i);
        item->items[3].kind = IR_CONST_INT;
        item->items[3].scalar = IR_I64;
        item->items[3].integer = f->bits != 0 ? TYPE_ID_NONE
                                              : type_id(f->type);
        item->items[4].kind = IR_CONST_INT;
        item->items[4].scalar = IR_I64;
        item->items[4].integer = f->owned ? 1 : 0;
        item->items[5].scalar = IR_PTR;
        descriptor = field_descriptor(l, f->type);
        if (descriptor != NULL) {
            item->items[5].kind = IR_CONST_ADDR;
            item->items[5].global = descriptor->index;
        } else {
            item->items[5].kind = IR_CONST_INT;
            item->items[5].integer = 0;
        }
        value->items[n++] = *item;
    }
    g = ir_global_add_value(l->m, module, name, value);
    free(module);
    free(name);
    return g;
}

/* A global the runtime defines. Every module refers to it and none
   writes it out. */
static struct ir_global *runtime_global(struct lowerer *l, const char *name)
{
    struct ir_module *m = l->m;
    struct ir_global *g = lower_find_global(m, NULL, name);

    if (g == NULL) {
        g = ir_global_add(m, NULL, name, NULL, 0, 1);
        g->is_extern = true;
        g->exported = true;
    }
    return g;
}

/* DESIGN: the ancestors of a class are its descriptors from the root
   down to the class itself, one entry per level. The entry at a level is
   the same address in every class below it, so `p is *T` compares the
   entry at T's depth with T's descriptor. */
static struct ir_global *class_ancestors(struct lowerer *l,
                                         const struct type *t)
{
    uint32_t depth = lower_class_depth(t);
    struct ir_const *value;
    struct ir_global *g;
    const struct type *up;
    char *module;
    char *name;
    uint32_t i;

    if (t->base == NULL) {
        return runtime_global(l, RUNTIME_ROOT "ancestors");
    }
    g = lower_class_global(l, t, "ancestors", &module, &name);
    if (g != NULL) {
        return g;
    }
    value = ir_const_agg(l->m, ir_aggregate(lower_table_agg(l, depth + 1)),
                         depth + 1);
    up = t;
    for (i = depth + 1; i-- > 0; up = up->base) {
        value->items[i].kind = IR_CONST_ADDR;
        value->items[i].scalar = IR_PTR;
        value->items[i].global = lower_class_descriptor(l, up)->index;
    }
    g = ir_global_add_value(l->m, module, name, value);
    free(module);
    free(name);
    return g;
}

/* The name a signature gives a type that a reflect.Value carries, or
   NULL for a type it cannot carry. */
static const char *value_type_name(const struct type *t)
{
    static const char *const names[] = {
        [TYPE_VOID] = "void", [TYPE_BOOL] = "bool", [TYPE_CHAR] = "char",
        [TYPE_I8] = "i8",     [TYPE_I16] = "i16",   [TYPE_I32] = "i32",
        [TYPE_I64] = "i64",   [TYPE_U8] = "u8",     [TYPE_U16] = "u16",
        [TYPE_U32] = "u32",   [TYPE_U64] = "u64",   [TYPE_F32] = "f32",
        [TYPE_F64] = "f64",   [TYPE_STR] = "str",
    };

    /* A function with its context is two words, which no Value
       carries, so reflect.call refuses a function that takes one. */
    if (t->kind == TYPE_POINTER ||
        (t->kind == TYPE_FN && !t->bound && !t->context)) {
        return "ptr";
    }
    /* An enum is signed under the ABI of Windows, so the index converts
       before the comparison. */
    return (size_t)t->kind < sizeof names / sizeof *names ? names[t->kind]
                                                          : NULL;
}

/* DESIGN: the signature of a function names its result and then each
   parameter after self, joined by dots, as `i64.i32.str`. The pass over
   the whole program writes one trampoline per such text, so equal
   signatures share one. A signature holding a type that a Value cannot
   carry has no text, and reflect.call refuses the function. */
static bool signature_code(const struct type *fn, struct text *code)
{
    const char *name;
    size_t i;

    if (fn == NULL || fn->kind != TYPE_FN ||
        (name = value_type_name(fn->result)) == NULL) {
        return false;
    }
    text_append(code, name);
    for (i = 1; i < fn->param_count; i++) {
        name = value_type_name(fn->params[i]);
        if (name == NULL || strcmp(name, "void") == 0) {
            return false;
        }
        text_appendf(code, ".%s", name);
    }
    return true;
}

static const struct ir_global *signature_text(struct lowerer *l,
                                              const struct type *fn)
{
    struct token_text text;
    struct text code = {0};
    const struct ir_global *g;

    if (!signature_code(fn, &code)) {
        text_free(&code);
        return NULL;
    }
    text.bytes = text_cstr(&code);
    text.length = code.length;
    g = lower_literal_global(l, &text);
    text_free(&code);
    return g;
}

/* The list of public functions of the chain, in table order. */
static struct ir_global *class_functions(struct lowerer *l,
                                         const struct type *t,
                                         size_t *count_out)
{
    struct table table = {0};
    struct ir_const *value;
    struct ir_global *g;
    struct token_text text;
    char *module;
    char *name;
    size_t i;

    table_of(t, &table);
    *count_out = table.count;
    if (table.count == 0) {
        free(table.entries);
        return NULL;
    }
    g = lower_class_global(l, t, "functions", &module, &name);
    if (g != NULL) {
        free(table.entries);
        return g;
    }
    value = ir_const_agg(l->m, ir_aggregate(functions_agg(l, table.count)),
                         table.count);
    for (i = 0; i < table.count; i++) {
        struct ir_const *item =
            ir_const_agg(l->m, ir_aggregate(function_agg(l)), 5);
        const struct item *fn = table.entries[i].fn;
        const struct ir_global *signature =
            fn != NULL && fn->symbol != NULL
                ? signature_text(l, fn->symbol->type)
                : NULL;
        text.bytes = table.entries[i].name.text;
        text.length = table.entries[i].name.length;
        item->items[0].kind = IR_CONST_ADDR;
        item->items[0].scalar = IR_PTR;
        item->items[0].global = lower_literal_global(l, &text)->index;
        item->items[1].kind = IR_CONST_INT;
        item->items[1].scalar = IR_I64;
        item->items[1].integer = table.entries[i].name.length;
        item->items[2].kind = IR_CONST_INT;
        item->items[2].scalar = IR_I64;
        item->items[2].integer = i + 1;
        item->items[3].kind = IR_CONST_INT;
        item->items[3].scalar = IR_I64;
        item->items[3].integer =
            fn != NULL && fn->symbol != NULL &&
                    fn->symbol->type->kind == TYPE_FN
                ? fn->symbol->type->param_count
                : 1;
        item->items[4].scalar = IR_PTR;
        if (signature != NULL) {
            item->items[4].kind = IR_CONST_ADDR;
            item->items[4].global = signature->index;
        } else {
            item->items[4].kind = IR_CONST_INT;
            item->items[4].integer = 0;
        }
        value->items[i] = *item;
    }
    free(table.entries);
    g = ir_global_add_value(l->m, module, name, value);
    free(module);
    free(name);
    return g;
}

/* The global that holds the version of the package being built, one per
   module. It has a name of its own rather than a number, so that adding
   it moves no literal of the module. */
static const struct ir_global *version_global(struct lowerer *l)
{
    static const char name[] = "package.version";
    struct ir_module *m = l->m;
    size_t length = strlen(l->version);
    struct ir_global *g = lower_find_global(m, l->module_name, name);
    uint8_t *bytes;

    if (g != NULL) {
        return g;
    }
    bytes = arena_alloc(m->arena, length + 1);
    memcpy(bytes, l->version, length + 1);
    return ir_global_add(m, l->module_name, name, bytes, length + 1, 1);
}

/* DESIGN: the structural hash of an abstract class covers the entries of
   its table in order, each with its name and its signature. The hash
   after k entries is the hash of the version whose table held k of them.
   The array of the hashes is therefore the chain of its earlier versions
   with the slots each had. A plugin records the chain of the interface
   it was built against, and the loader compares the two at the length of
   the shorter. An entry whose signature no reflect.Value carries hashes
   its count of parameters instead. That still tells two entries of one
   name apart. */
static uint64_t hash_bytes(uint64_t h, const char *bytes, size_t length)
{
    size_t i;

    for (i = 0; i < length; i++) {
        h = (h ^ (uint8_t)bytes[i]) * 0x100000001b3ULL;
    }
    return h;
}

/* The chain of an abstract class: one hash per prefix of its table, the
   empty prefix first. Entry k is the hash of a table of k entries. */
static struct ir_global *class_chain(struct lowerer *l, const struct type *t,
                                     size_t *count_out)
{
    struct table table = {0};
    struct ir_const *value;
    struct ir_global *g;
    char array[24];
    char *module;
    char *name;
    uint64_t h = 0xcbf29ce484222325ULL;
    size_t i;

    table_of(t, &table);
    *count_out = table.count + 1;
    g = lower_class_global(l, t, "chain", &module, &name);
    if (g != NULL) {
        free(table.entries);
        return g;
    }
    snprintf(array, sizeof array, "[%zu]i64", table.count + 1);
    value = ir_const_agg(
        l->m,
        ir_aggregate(ir_array_add(l->m, array, ir_scalar(IR_I64),
                                  ir_sym_int(l->m, IR_I64, table.count + 1),
                                  NULL)),
        table.count + 1);
    value->items[0].kind = IR_CONST_INT;
    value->items[0].scalar = IR_I64;
    value->items[0].integer = h;
    for (i = 0; i < table.count; i++) {
        const struct item *fn = table.entries[i].fn;
        struct text code = {0};
        char params[24];
        h = hash_bytes(h, table.entries[i].name.text,
                       table.entries[i].name.length);
        h = hash_bytes(h, ":", 1);
        if (fn != NULL && fn->symbol != NULL &&
            signature_code(fn->symbol->type, &code)) {
            h = hash_bytes(h, text_cstr(&code), code.length);
        } else {
            snprintf(params, sizeof params, "%zu", table.entries[i].params);
            h = hash_bytes(h, params, strlen(params));
        }
        text_free(&code);
        h = hash_bytes(h, ";", 1);
        value->items[i + 1].kind = IR_CONST_INT;
        value->items[i + 1].scalar = IR_I64;
        value->items[i + 1].integer = h;
    }
    free(table.entries);
    g = ir_global_add_value(l->m, module, name, value);
    free(module);
    free(name);
    return g;
}

/* The aggregate of the version record of an abstract class. */
static uint32_t versions_agg(struct lowerer *l)
{
    static const char name[] = "anti.rt.Versions";
    struct ir_field fields[4];
    uint32_t agg = ir_agg_find(l->m, name);

    if (agg != IR_NO_AGG) {
        return agg;
    }
    memset(fields, 0, sizeof fields);
    fields[0].name = "chain";
    fields[0].type = ir_scalar(IR_PTR);
    fields[1].name = "chain_length";
    fields[1].type = ir_scalar(IR_I64);
    fields[2].name = "floor";
    fields[2].type = ir_scalar(IR_PTR);
    fields[3].name = "floor_length";
    fields[3].type = ir_scalar(IR_I64);
    return ir_struct_add(l->m, IR_AGG_STRUCT, name, fields, 4, false, 0);
}

/* DESIGN: the version record of an abstract class holds its chain and
   the floor a `compatible` line names. The descriptor of a class that
   is no interface points at none. */
static struct ir_global *class_versions(struct lowerer *l,
                                        const struct type *t)
{
    struct ir_const *value;
    struct ir_global *g;
    const struct ir_global *chain;
    char *module;
    char *name;
    size_t count = 0;

    g = lower_class_global(l, t, "versions", &module, &name);
    if (g != NULL) {
        return g;
    }
    chain = class_chain(l, t, &count);
    value = ir_const_agg(l->m, ir_aggregate(versions_agg(l)), 4);
    value->items[0].kind = IR_CONST_ADDR;
    value->items[0].scalar = IR_PTR;
    value->items[0].global = chain->index;
    value->items[1].kind = IR_CONST_INT;
    value->items[1].scalar = IR_I64;
    value->items[1].integer = count;
    value->items[2].scalar = IR_PTR;
    value->items[3].kind = IR_CONST_INT;
    value->items[3].scalar = IR_I64;
    value->items[3].integer = t->compatible.length;
    if (t->compatible.length > 0) {
        struct token_text floor;
        floor.bytes = t->compatible.text;
        floor.length = t->compatible.length;
        value->items[2].kind = IR_CONST_ADDR;
        value->items[2].global = lower_literal_global(l, &floor)->index;
    } else {
        value->items[2].kind = IR_CONST_INT;
        value->items[2].integer = 0;
    }
    g = ir_global_add_value(l->m, module, name, value);
    free(module);
    free(name);
    return g;
}

/* DESIGN: the descriptor of a class is read-only data at entry 0 of its
   table. It names the class and points at the descriptor of its base. It
   holds the size of the class and its depth in the chain, and points at
   its ancestors. */
struct ir_global *lower_class_descriptor(struct lowerer *l,
                                         const struct type *t)
{
    struct ir_const *value;
    struct ir_global *g;
    struct token_text text;
    char *module;
    char *name;

    /* DESIGN: the root's descriptor belongs to the runtime, so that two
       modules of one program share one and `p is *Object` compares the
       same address everywhere. */
    if (t->base == NULL) {
        return runtime_global(l, RUNTIME_ROOT "descriptor");
    }
    g = lower_class_global(l, t, "descriptor", &module, &name);
    if (g != NULL) {
        return g;
    }
    /* The global is added before its ancestors, so a chain that comes
       back around finds it and does not build it twice. */
    value = ir_const_agg(l->m, ir_aggregate(lower_descriptor_agg(l)), 15);
    g = ir_global_add_value(l->m, module, name, value);
    g->exported = t->item_exported;
    free(module);
    free(name);
    text.bytes = t->name.text;
    text.length = t->name.length;
    value->items[0].kind = IR_CONST_ADDR;
    value->items[0].scalar = IR_PTR;
    value->items[0].global = lower_literal_global(l, &text)->index;
    value->items[1].kind = IR_CONST_INT;
    value->items[1].scalar = IR_I64;
    value->items[1].integer = t->name.length;
    value->items[2].scalar = IR_PTR;
    if (t->base != NULL) {
        value->items[2].kind = IR_CONST_ADDR;
        value->items[2].global = lower_class_descriptor(l, t->base)->index;
    } else {
        value->items[2].kind = IR_CONST_INT;
        value->items[2].integer = 0;
    }
    value->items[3].kind = IR_CONST_SYM;
    value->items[3].scalar = IR_I64;
    value->items[3].sym = ir_sym_size_of(l->m, lower_vtype_of(l, t));
    value->items[4].kind = IR_CONST_INT;
    value->items[4].scalar = IR_I64;
    value->items[4].integer = lower_class_depth(t);
    value->items[5].kind = IR_CONST_ADDR;
    value->items[5].scalar = IR_PTR;
    value->items[5].global = class_ancestors(l, t)->index;
    value->items[6].kind = IR_CONST_INT;
    value->items[6].scalar = IR_I64;
    value->items[6].integer = l->no_reflect ? 0 : lower_own_fields(t);
    value->items[7].scalar = IR_PTR;
    if (l->no_reflect || lower_own_fields(t) == 0) {
        value->items[7].kind = IR_CONST_INT;
        value->items[7].integer = 0;
    } else {
        value->items[7].kind = IR_CONST_ADDR;
        value->items[7].global = lower_class_fields(l, t)->index;
    }
    /* DESIGN: the destruct body the class declares, and not the one it
       inherits, because `delete` runs one body per level of the chain.
       The table holds the last one, which is a different question. */
    value->items[8].scalar = IR_PTR;
    value->items[8].kind = IR_CONST_INT;
    value->items[8].integer = 0;
    {
        static const struct name destruct_name = {"destruct", 8};
        size_t i;
        for (i = 0; i < t->member_count; i++) {
            const struct item *m = t->members[i];
            if (m->kind == ITEM_FN &&
                lower_same_name(&m->name, &destruct_name) &&
                m->body != NULL && m->symbol != NULL) {
                value->items[8].kind = IR_CONST_FUNC;
                value->items[8].global = m->symbol->ir;
            }
        }
    }
    value->items[9].kind = IR_CONST_INT;
    value->items[9].scalar = IR_I64;
    value->items[9].integer = 0;
    /* The function list names every public function of the chain and
       the entry of each. `--no-reflect` drops it with the field list. */
    {
        size_t function_count = 0;
        const struct ir_global *list =
            l->no_reflect ? NULL : class_functions(l, t, &function_count);
        value->items[10].kind = IR_CONST_INT;
        value->items[10].scalar = IR_I64;
        value->items[10].integer = list != NULL ? function_count : 0;
        value->items[11].scalar = IR_PTR;
        if (list != NULL) {
            value->items[11].kind = IR_CONST_ADDR;
            value->items[11].global = list->index;
        } else {
            value->items[11].kind = IR_CONST_INT;
            value->items[11].integer = 0;
        }
    }
    /* The version of the package that declares the class. The module
       that declares it writes the descriptor, and every other module
       refers to the one it wrote. The version of this build is then the
       class's own. */
    value->items[12].kind = IR_CONST_ADDR;
    value->items[12].scalar = IR_PTR;
    value->items[12].global = version_global(l)->index;
    value->items[13].kind = IR_CONST_INT;
    value->items[13].scalar = IR_I64;
    value->items[13].integer = (uint64_t)strlen(l->version);
    value->items[14].scalar = IR_PTR;
    if (t->has_abstract) {
        value->items[14].kind = IR_CONST_ADDR;
        value->items[14].global = class_versions(l, t)->index;
    } else {
        value->items[14].kind = IR_CONST_INT;
        value->items[14].integer = 0;
    }
    return g;
}

/* DESIGN: a struct has a descriptor as well, which the struct itself
   never points at. A field of the struct's type names it, so a walk of a
   class reaches the fields of a struct inside it. It holds the name, the
   size and the field list, and nothing of a chain. The module that
   declares the struct writes it, whether a class there names it or not.
   It cannot know which classes of other modules hold it. A union
   has none, since no walk knows which of its fields holds the value. A
   Job, Flags, a Mutex and a channel have none either, since no module
   declares them. */
struct ir_global *lower_struct_descriptor(struct lowerer *l,
                                          const struct type *t)
{
    struct ir_const *value;
    struct ir_global *g;
    struct token_text text;
    char *module;
    char *name;
    size_t count = lower_own_fields(t);
    size_t k;

    if (t->is_union || types_is_job(t) || types_is_flags(t) ||
        types_is_mutex(t) || types_is_chan(t) || types_is_regex(t) ||
        types_is_match(t) || types_is_object_lock(t)) {
        return NULL;
    }
    g = struct_global(l, t, "descriptor", &module, &name);
    if (g != NULL) {
        return g;
    }
    /* The global is added before the field list, so a struct that
       points at itself finds it. */
    value = ir_const_agg(l->m, ir_aggregate(lower_descriptor_agg(l)), 15);
    g = ir_global_add_value(l->m, module, name, value);
    free(module);
    free(name);
    for (k = 0; k < 15; k++) {
        value->items[k].kind = IR_CONST_INT;
        value->items[k].scalar = l->m->aggs[lower_descriptor_agg(l)]
                                     ->fields[k].type.type;
        value->items[k].integer = 0;
    }
    text.bytes = t->name.text;
    text.length = t->name.length;
    value->items[0].kind = IR_CONST_ADDR;
    value->items[0].global = lower_literal_global(l, &text)->index;
    value->items[1].integer = t->name.length;
    value->items[3].kind = IR_CONST_SYM;
    value->items[3].sym = ir_sym_size_of(l->m, lower_vtype_of(l, t));
    if (!l->no_reflect && count > 0) {
        value->items[6].integer = count;
        value->items[7].kind = IR_CONST_ADDR;
        value->items[7].global = lower_class_fields(l, t)->index;
    }
    value->items[12].kind = IR_CONST_ADDR;
    value->items[12].global = version_global(l)->index;
    value->items[13].integer = (uint64_t)strlen(l->version);
    return g;
}

/* The descriptor that entry 0 of an interface table points at. It is
   the class's own descriptor with the offset of the sub-object. A
   pointer into the middle of an object therefore still names the class
   and still leads back to its start. */
static struct ir_global *interface_descriptor(struct lowerer *l,
                                              const struct type *t,
                                              const struct struct_field *sub)
{
    const struct ir_global *of = lower_class_descriptor(l, t);
    struct ir_const *value;
    struct ir_global *g;
    struct text label = {0};
    char *module;
    char *name;

    text_appendf(&label, "%.*s.descriptor", (int)sub->name.length,
                 sub->name.text);
    g = lower_class_global(l, t, text_cstr(&label), &module, &name);
    text_free(&label);
    if (g != NULL) {
        return g;
    }
    value = ir_const_agg(l->m, ir_aggregate(lower_descriptor_agg(l)), 15);
    memcpy(value->items, l->m->globals[of->index]->value->items,
           15 * sizeof *value->items);
    value->items[9].kind = IR_CONST_SYM;
    value->items[9].scalar = IR_I64;
    value->items[9].sym =
        ir_sym_offset_of(l->m, lower_agg_of(l, sub->home),
                         (uint32_t)(sub - sub->home->fields));
    g = ir_global_add_value(l->m, module, name, value);
    g->exported = t->item_exported;
    free(module);
    free(name);
    return g;
}

/* Whether fn has a body: here, in the runtime, or in the module whose
   library file declared it. A library file carries no bodies, and an
   abstract function has none anywhere. */
bool lower_has_body(const struct item *fn)
{
    return fn->body != NULL || fn->runtime != NULL ||
           (fn->symbol != NULL && fn->symbol->home != NULL &&
            fn->contract != FN_ABSTRACT);
}

/* DESIGN: the compiler writes a teardown and a copy for every complete
   class, `Class.destroy` and `Class.copy` in the module that declares it.
   They fill the destruct and the copy entries of its table, and delete,
   destroy and dup reach them through it. Ownership therefore never reads
   the field list, which --no-reflect drops. A chain that declares `copy`
   keeps that function in the entry, and no copy is written. The
   teardown takes the anti.mem.Allocator that the memory the object owns
   goes back to, which is zero for the C library. */
struct ir_function *lower_class_function(struct lowerer *l,
                                         const struct type *t,
                                         const char *part)
{
    char *module = lower_cstr(&t->module);
    struct ir_function *f;
    struct text name = {0};

    type_symbol_name(&name, t);
    text_appendf(&name, ".%s", part);
    f = lower_find_function(l->m, module, text_cstr(&name));
    if (f == NULL) {
        f = strcmp(module, l->module_name) == 0
                ? ir_function_add(l->m, l->module_name, text_cstr(&name),
                                  IR_VOID, IR_NO_AGG)
                : ir_declare_add(l->m, module, text_cstr(&name), IR_VOID,
                                 IR_NO_AGG);
        ir_param_add(f, IR_PTR, IR_NO_AGG);
        if (strcmp(part, "copy") == 0 || strcmp(part, "destroy") == 0) {
            ir_param_add(f, IR_PTR, IR_NO_AGG);
        }
    }
    text_free(&name);
    free(module);
    return f;
}

/* The function name that level t of a chain declares with a body, or
   NULL. */
const struct item *lower_level_fn(const struct type *t,
                                  const struct name *name)
{
    size_t i;

    for (i = 0; i < t->member_count; i++) {
        const struct item *m = t->members[i];
        if (m->kind == ITEM_FN && m->runtime == NULL && lower_has_body(m) &&
            m->name.length == name->length &&
            memcmp(m->name.text, name->text, name->length) == 0) {
            return m;
        }
    }
    return NULL;
}

static const struct name copy_name = {"copy", 4};

/* The `copy` the chain of t declares nearest to t, or NULL when it keeps
   the one of the root and the compiler writes it. */
const struct item *lower_declared_copy(const struct type *t)
{
    const struct item *m = NULL;

    for (; t != NULL && t->kind == TYPE_CLASS && t->base != NULL && m == NULL;
         t = t->base) {
        m = lower_level_fn(t, &copy_name);
    }
    return m;
}

/* The global that holds the table of t, built once per class. */
struct ir_global *lower_class_table(struct lowerer *l, const struct type *t)
{
    struct ir_module *m = l->m;
    struct table table = {0};
    struct ir_const *value;
    struct ir_global *g;
    char *module;
    char *name;
    size_t i;

    g = lower_class_global(l, t, "table", &module, &name);
    if (g != NULL) {
        return g;
    }
    table_of(t, &table);
    value = ir_const_agg(m, ir_aggregate(lower_table_agg(l, table.count + 1)),
                         table.count + 1);
    g = ir_global_add_value(m, module, name, value);
    g->exported = t->item_exported;
    value->items[0].kind = IR_CONST_ADDR;
    value->items[0].scalar = IR_PTR;
    value->items[0].global = lower_class_descriptor(l, t)->index;
    for (i = 0; i < table.count; i++) {
        static const struct name destruct_name = {"destruct", 8};
        const struct item *fn = table.entries[i].fn;
        const struct ir_function *written =
            lower_same_name(&table.entries[i].name, &destruct_name)
                ? lower_class_function(l, t, "destroy")
            : lower_same_name(&table.entries[i].name, &copy_name) &&
                    lower_declared_copy(t) == NULL
                ? lower_class_function(l, t, "copy")
                : NULL;
        value->items[i + 1].scalar = IR_PTR;
        if (written != NULL) {
            value->items[i + 1].kind = IR_CONST_FUNC;
            value->items[i + 1].global = written->index;
        } else if (fn != NULL && fn->symbol != NULL && lower_has_body(fn)) {
            value->items[i + 1].kind = IR_CONST_FUNC;
            value->items[i + 1].global =
                lower_callee_function(l, fn->symbol)->index;
        } else {
            value->items[i + 1].kind = IR_CONST_INT;
            value->items[i + 1].integer = 0;
        }
    }
    free(table.entries);
    free(module);
    free(name);
    return g;
}

/* The public function `name` of t or of a class above it. */
static const struct item *find_member_fn(const struct type *t,
                                         const struct name *name)
{
    size_t i;

    for (; t != NULL; t = t->kind == TYPE_CLASS ? t->base : NULL) {
        for (i = 0; i < t->member_count; i++) {
            const struct item *m = t->members[i];
            if (m->kind == ITEM_FN && m->pub &&
                lower_same_name(&m->name, name)) {
                return m;
            }
        }
    }
    return NULL;
}

/* DESIGN: an interface table holds thunks. A thunk takes `self` as a
   pointer to the sub-object. It subtracts the offset of that sub-object
   to reach the object, then calls the class's own function. Every
   function is written once. It serves the direct call and the call
   through the interface alike. */
static struct ir_function *interface_thunk(struct lowerer *l,
                                           const struct type *t,
                                           const struct struct_field *sub,
                                           const struct item *fn)
{
    const struct type *sig = fn->symbol->type;
    struct ir_function *outer_f = l->f;
    struct ir_block *outer_b = l->b;
    struct ir_function *target = lower_callee_function(l, fn->symbol);
    struct ir_function *f;
    struct ir_operand *args;
    struct ir_block *entry;
    struct ir_operand back;
    uint32_t value;
    struct text name = {0};
    size_t i;

    type_symbol_name(&name, t);
    text_appendf(&name, ".%.*s.%.*s.thunk", (int)sub->name.length,
                 sub->name.text, (int)fn->name.length, fn->name.text);
    f = lower_find_function(l->m, l->module_name, text_cstr(&name));
    if (f != NULL) {
        text_free(&name);
        return f;
    }
    f = ir_function_add(l->m, l->module_name, text_cstr(&name),
                        lower_ir_type_of(sig->result),
                        lower_result_agg(l, sig->result));
    text_free(&name);
    f->result_agg = lower_result_agg(l, sig->result);
    for (i = 0; i < sig->param_count; i++) {
        lower_add_param(l, f, sig->params[i]);
    }
    entry = ir_block_add(f);
    l->f = f;
    l->b = entry;
    /* The thunk passes on each IR parameter as it came, so a parameter
       of two words passes as two. */
    args = ir_alloc(f->param_count + 1, sizeof *args);
    back = lower_temp(l, ir_binary(f, entry, IR_SUB, IR_I64,
                                   ir_int_op(IR_I64, 0),
                                   lower_field_offset(l, sub->home,
                                                      &sub->name)));
    args[0] = lower_temp(l,
                         ir_ptradd(f, entry, lower_temp(l, f->params[0].temp),
                                   back));
    for (i = 1; i < f->param_count; i++) {
        args[i] = lower_temp(l, f->params[i].temp);
    }
    value = ir_call(f, entry, lower_ir_type_of(sig->result),
                    ir_func_op(target), args, f->param_count);
    free(args);
    if (sig->result->kind == TYPE_VOID) {
        ir_ret(f, entry, IR_VOID, lower_none());
    } else {
        /* An aggregate result travels as the address of its storage,
           which is what the called function already returned. */
        ir_ret(f, entry, f->result == IR_AGG ? IR_PTR : f->result,
               lower_temp(l, value));
    }
    l->f = outer_f;
    l->b = outer_b;
    return f;
}

/* The table of one interface sub-object of t, whose entries are thunks
   into t's own functions. */
struct ir_global *lower_interface_table(struct lowerer *l,
                                        const struct type *t,
                                        const struct struct_field *sub)
{
    struct ir_module *m = l->m;
    struct table table = {0};
    struct ir_const *value;
    struct ir_global *g;
    char *module;
    char *name;
    struct text label = {0};
    size_t i;

    text_appendf(&label, "%.*s.table", (int)sub->name.length,
                 sub->name.text);
    g = lower_class_global(l, t, text_cstr(&label), &module, &name);
    text_free(&label);
    if (g != NULL) {
        return g;
    }
    table_of(sub->type, &table);
    value = ir_const_agg(m, ir_aggregate(lower_table_agg(l, table.count + 1)),
                         table.count + 1);
    g = ir_global_add_value(m, module, name, value);
    g->exported = t->item_exported;
    value->items[0].kind = IR_CONST_ADDR;
    value->items[0].scalar = IR_PTR;
    value->items[0].global = interface_descriptor(l, t, sub)->index;
    for (i = 0; i < table.count; i++) {
        /* The class fills the entry where it declares or inherits the
           name, and the thunk moves the receiver back to the object.
           Where it does not, the interface's own body serves, and that
           body already takes a pointer to the sub-object. */
        const struct item *fn =
            types_interface_member(t, sub->type, &table.entries[i].name);
        bool own = fn != NULL && fn->symbol != NULL && lower_has_body(fn);
        if (!own) {
            fn = find_member_fn(sub->type, &table.entries[i].name);
            own = false;
        }
        value->items[i + 1].scalar = IR_PTR;
        if (fn != NULL && fn->symbol != NULL && lower_has_body(fn)) {
            value->items[i + 1].kind = IR_CONST_FUNC;
            value->items[i + 1].global =
                own ? interface_thunk(l, t, sub, fn)->index
                    : lower_callee_function(l, fn->symbol)->index;
        } else {
            value->items[i + 1].kind = IR_CONST_INT;
            value->items[i + 1].integer = 0;
        }
    }
    free(table.entries);
    free(module);
    free(name);
    return g;
}

/* DESIGN: `T.f` of a body qualified by an interface is a function of
   its own, which the module that names it writes. It moves the object to
   the sub-object, reads the entry of its table and calls that. A class
   below that replaces the body is therefore honoured. The call names the
   interface and the slot, as every call through a table does. */
struct ir_function *lower_reach_thunk(struct lowerer *l,
                                      const struct struct_field *sub,
                                      const struct symbol *sym)
{
    const struct type *sig = sym->type;
    struct ir_function *outer_f = l->f;
    struct ir_block *outer_b = l->b;
    size_t index = lower_table_index(sub->type, &sym->item->name,
                                     sig->param_count);
    struct ir_function *f;
    struct ir_operand *args;
    struct ir_operand table;
    struct ir_operand target;
    struct ir_inst *call;
    uint32_t value;
    struct text name = {0};
    size_t i;

    type_symbol_name(&name, sub->home);
    text_appendf(&name, ".%.*s.%.*s.reach", (int)sub->name.length,
                 sub->name.text, (int)sym->item->name.length,
                 sym->item->name.text);
    f = lower_find_function(l->m, l->module_name, text_cstr(&name));
    if (f != NULL) {
        text_free(&name);
        return f;
    }
    f = ir_function_add(l->m, l->module_name, text_cstr(&name),
                        lower_ir_type_of(sig->result),
                        lower_result_agg(l, sig->result));
    text_free(&name);
    f->result_agg = lower_result_agg(l, sig->result);
    for (i = 0; i < sig->param_count; i++) {
        lower_add_param(l, f, sig->params[i]);
    }
    l->f = f;
    l->b = ir_block_add(f);
    args = ir_alloc(f->param_count + 1, sizeof *args);
    args[0] = lower_offset_address(l, lower_temp(l, f->params[0].temp),
                                   lower_field_offset(l, sub->home,
                                                      &sub->name));
    for (i = 1; i < f->param_count; i++) {
        args[i] = lower_temp(l, f->params[i].temp);
    }
    table = lower_load_table(l, args[0], sub->type);
    target = lower_temp(
        l, ir_load(l->f, l->b, IR_PTR,
                   lower_offset_address(l, table,
                                        lower_entry_offset(l, index))));
    value = ir_call_indirect(l->f, l->b, lower_ir_type_of(sig->result), target,
                             lower_signature(l, sig), args, f->param_count);
    call = &l->b->insts[l->b->count - 1];
    call->c = ir_global_op(lower_class_descriptor(l, sub->type));
    call->field = (uint32_t)index;
    free(args);
    if (sig->result->kind == TYPE_VOID) {
        ir_ret(l->f, l->b, IR_VOID, lower_none());
    } else {
        /* An aggregate result travels as the address of its storage,
           which is what the called function already returned. */
        ir_ret(l->f, l->b, f->result == IR_AGG ? IR_PTR : f->result,
               lower_temp(l, value));
    }
    l->f = outer_f;
    l->b = outer_b;
    return f;
}

/* DESIGN: `construct` runs after a literal has written every field,
   base first down the chain. A base therefore sees its own fields
   before the class below it adds to them. A class without one adds
   nothing. */
void lower_run_construct_bodies(struct lowerer *l, const struct type *t,
                                struct ir_operand dest)
{
    static const struct name construct_name = {"construct", 9};
    size_t i;

    if (t == NULL || t->kind != TYPE_CLASS) {
        return;
    }
    lower_run_construct_bodies(l, t->base, dest);
    for (i = 0; i < t->member_count; i++) {
        const struct item *m = t->members[i];
        if (m->kind != ITEM_FN || !lower_same_name(&m->name, &construct_name) ||
            m->symbol == NULL || !lower_has_body(m) ||
            m->symbol->type->param_count != 1) {
            continue;
        }
        ir_call(l->f, l->b, IR_VOID,
                ir_func_op(lower_callee_function(l, m->symbol)), &dest, 1);
    }
}

/* The bodies of the chain, and then the `created` hook of the object
   the literal built. One object gives one hook, whatever its chain
   declares. */
void lower_run_construct(struct lowerer *l, const struct type *t,
                         struct ir_operand dest)
{
    lower_run_construct_bodies(l, t, dest);
    if (t != NULL && t->kind == TYPE_CLASS) {
        lower_hook_object(l, HOOK_CREATED, dest);
    }
}

/* Store the table pointer of every interface sub-object of t into the
   object at dest. */
void lower_store_interface_tables(struct lowerer *l, const struct type *t,
                                  struct ir_operand dest)
{
    const struct type *up;
    size_t i;

    for (up = t; up != NULL; up = up->kind == TYPE_CLASS ? up->base : NULL) {
        for (i = 0; i < up->field_count; i++) {
            const struct struct_field *field = &up->fields[i];
            struct ir_operand table;
            struct ir_operand at;

            if (field->form != FIELD_IMPL) {
                continue;
            }
            /* One call, one argument that emits code: the order of the
               arguments of a C call is unspecified. */
            table = lower_temp(
                l, ir_addr(l->f, l->b,
                           ir_global_op(lower_interface_table(l, t, field))));

            at = lower_offset_address(l, dest,
                                      lower_field_offset(l, up, &field->name));
            ir_store(l->f, l->b, IR_PTR, table, at);
        }
    }
}

/* Whether name is one of the nine hooks, which stand after the seven
   functions of the root in root_names. */
bool lower_hook_name(const struct name *name)
{
    size_t count = sizeof root_names / sizeof root_names[0];
    size_t i;

    for (i = count - (size_t)HOOK_COUNT; i < count; i++) {
        if (name->length == strlen(root_names[i]) &&
            memcmp(name->text, root_names[i], name->length) == 0) {
            return true;
        }
    }
    return false;
}

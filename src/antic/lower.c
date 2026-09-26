#include "lower.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../rt/f16.h"
#include "arith.h"
#include "sema.h"
#include "target.h"
#include "text.h"
#include "types.h"
#include "lower_lowerer.h"

struct ir_operand lower_none(void)
{
    struct ir_operand o = {IR_NONE, IR_VOID, {0}};
    return o;
}

struct ir_operand lower_temp(const struct lowerer *l, uint32_t t)
{
    return ir_temp_op(l->f, t);
}

/* bool is i8 and char is i32. Signedness moves into the operations. An
   enum is its underlying integer type. */
enum ir_type lower_ir_type_of(const struct type *t)
{
    if (t->kind == TYPE_ENUM) {
        t = t->base;
    }
    if (t->lock_word) {
        return IR_LOCK;
    }
    switch (t->kind) {
    case TYPE_BOOL:
    case TYPE_I8:
    case TYPE_U8:
        return IR_I8;
    case TYPE_I16:
    case TYPE_U16:
    case TYPE_F16:
        return IR_I16;
    case TYPE_CHAR:
    case TYPE_I32:
    case TYPE_U32:
        return IR_I32;
    case TYPE_I64:
    case TYPE_U64:
        return IR_I64;
    case TYPE_CLONG:
    case TYPE_CULONG:
        return IR_CLONG;
    case TYPE_CWCHAR:
        return IR_CWCHAR;
    case TYPE_F32:
        return IR_F32;
    case TYPE_F64:
        return IR_F64;
    case TYPE_POINTER:
    case TYPE_FN:
        return IR_PTR;
    case TYPE_STRUCT:
    case TYPE_CLASS:
    case TYPE_TUPLE:
    case TYPE_VARIANT:
    case TYPE_OPTIONAL:
    case TYPE_ARRAY:
    case TYPE_STR:
    case TYPE_SLICE:
        return IR_AGG;
    default:
        return IR_VOID;
    }
}

/* Whether t is a function type in the form of two words, the code and a
   context. A parameter that does not keep its argument takes it. */
bool lower_is_context(const struct type *t)
{
    return t != NULL && t->kind == TYPE_FN && t->context;
}

/* DESIGN: a function with its context has its code first, which is zero
   for `none`. A test of it reads that word as the test of a pointer reads
   the pointer. */
bool lower_none_in_first_word(const struct type *t)
{
    return lower_is_context(t);
}

/* DESIGN: the test of a `?T` reads its flag byte, which lies after the
   value. The value lies at offset 0, so the address of a `?T` is the
   address of the value it holds. */
struct ir_operand lower_optional_flag(struct lowerer *l, const struct type *t,
                                      struct ir_operand address)
{
    static const struct name has = {OPTIONAL_HAS, sizeof OPTIONAL_HAS - 1};

    return lower_temp(
        l, ir_load(l->f, l->b, IR_I8,
                   lower_offset_address(l, address,
                                        lower_field_offset(l, t, &has))));
}

/* Write flag, 0 or 1, into the `?T` of type t at address. Write it into
   every `?T` its value holds as well, down to the type value. That is
   the type of what was written at offset 0. */
void lower_set_optional(struct lowerer *l, const struct type *t,
                        const struct type *value, struct ir_operand address,
                        int flag)
{
    static const struct name has = {OPTIONAL_HAS, sizeof OPTIONAL_HAS - 1};

    for (; t != NULL && t->kind == TYPE_OPTIONAL && t != value;
         t = t->element) {
        ir_store(l->f, l->b, IR_I8, ir_int_op(IR_I8, flag != 0 ? 1u : 0u),
                 lower_offset_address(l, address,
                                      lower_field_offset(l, t, &has)));
        if (flag == 0) {
            return;
        }
    }
}

/* A str, a slice, a bound function and a function with its context are
   aggregates of two words. */
bool lower_is_aggregate(const struct type *t)
{
    return type_has_fields(t) || t->kind == TYPE_ARRAY ||
           t->kind == TYPE_STR || t->kind == TYPE_SLICE ||
           (t->kind == TYPE_FN && (t->bound || t->context));
}

/* The name of type t, qualified by its module when qualified is set. The
   caller frees it with free. */
static char *name_of_type(const struct type *t, bool qualified)
{
    struct text name = {0};
    char *copy;

    if (qualified) {
        type_name_qualified(&name, t);
    } else {
        type_name(&name, t);
    }
    copy = lower_copy_text(&name);
    text_free(&name);
    return copy;
}

/* The name of the aggregate of every function with its context, which no
   type of a program spells. The caller frees it with free. */
static char *pair_name(void)
{
    static const char name[] = "fn(...)";
    char *copy = ir_alloc(sizeof name, 1);

    memcpy(copy, name, sizeof name);
    return copy;
}

/* The aggregate of t in the type table of the module. A struct lists its
   fields, a str or slice a pointer and a length, and an array its element
   and its length. */
uint32_t lower_agg_of(struct lowerer *l, const struct type *t)
{
    /* DESIGN: every function with its context is one aggregate of two
       pointers, whatever its signature and its marks. A value made in one
       form and read in another then names the same fields. */
    char *name = t->kind == TYPE_FN && !t->bound ? pair_name()
                                                 : name_of_type(t, true);
    uint32_t agg = ir_agg_find(l->m, name);
    struct ir_field *fields;
    size_t count = type_has_fields(t) ? t->field_count : 2;
    size_t i;

    if (agg != IR_NO_AGG) {
        free(name);
        return agg;
    }
    fields = ir_alloc(count + 1, sizeof *fields);
    if (t->kind == TYPE_ARRAY) {
        struct text text = {0};
        struct ir_vtype element = lower_vtype_of(l, t->element);
        uint32_t length = t->length_of != NULL
                              ? lower_sym_of(l, t->length_of)
                              : ir_sym_int(l->m, IR_I64, t->length);
        if (t->length_of != NULL) {
            symbolic_print(&text, t->length_of, false);
        } else {
            text_appendf(&text, "%llu", (unsigned long long)t->length);
        }
        agg = ir_array_add(l->m, name, element, length, text_cstr(&text));
        text_free(&text);
    } else if (type_has_fields(t)) {
        for (i = 0; i < count; i++) {
            char *field = ir_alloc(t->fields[i].name.length + 1, 1);
            memcpy(field, t->fields[i].name.text, t->fields[i].name.length);
            field[t->fields[i].name.length] = '\0';
            fields[i].name = field;
            fields[i].type = lower_vtype_of(l, t->fields[i].type);
            fields[i].bits = t->fields[i].bits;
            fields[i].ext = t->fields[i].bits == 0 ? IR_EXT_NONE
                            : type_is_signed(t->fields[i].type) ? IR_EXT_SIGN
                                                                : IR_EXT_ZERO;
        }
        agg = t->simd ? ir_simd_add(l->m, name, fields, count)
                      : ir_struct_add(l->m,
                                      t->is_union ? IR_AGG_UNION
                                                  : IR_AGG_STRUCT,
                                      name, fields, count, t->packed,
                                      t->align);
        for (i = 0; i < count; i++) {
            free((char *)fields[i].name);
        }
    } else if (t->kind == TYPE_FN) {
        /* A bound function is the object and the entry of its table, and
           a function with its context the code and the context. The plain
           form names the same pair where a conversion builds it. */
        fields[0].name = t->bound ? "object" : "code";
        fields[0].type = ir_scalar(IR_PTR);
        fields[1].name = t->bound ? "entry" : "context";
        fields[1].type = ir_scalar(IR_PTR);
        agg = ir_struct_add(l->m, IR_AGG_STRUCT, name, fields, 2, false, 0);
    } else {
        fields[0].name = "ptr";
        fields[0].type = ir_scalar(IR_PTR);
        fields[1].name = "len";
        fields[1].type = ir_scalar(IR_I64);
        agg = ir_struct_add(l->m, IR_AGG_STRUCT, name, fields, 2, false, 0);
    }
    free(fields);
    free(name);
    return agg;
}

struct ir_vtype lower_vtype_of(struct lowerer *l, const struct type *t)
{
    return lower_is_aggregate(t) ? ir_aggregate(lower_agg_of(l, t))
                           : ir_scalar(lower_ir_type_of(t));
}

/* The size of a value of type t as an operand, symbolic until the back
   end folds it. */
struct ir_operand lower_size_operand(struct lowerer *l, const struct type *t)
{
    return ir_sym_operand(l->m, ir_sym_size_of(l->m, lower_vtype_of(l, t)));
}

/* The narrowest and the widest width of an integer type on the six
   targets. Only a target-sized type has two. */
static int min_bits(enum ir_type type)
{
    switch (type) {
    case IR_I8: return 8;
    case IR_I16:
    case IR_CWCHAR: return 16;
    case IR_I32:
    case IR_LOCK:
    case IR_CLONG: return 32;
    default: return 64;
    }
}

static int max_bits(enum ir_type type)
{
    return type == IR_CLONG || type == IR_LOCK ? 64
           : type == IR_CWCHAR                ? 32
                                              : min_bits(type);
}

/* DESIGN: a conversion between integer types truncates when the target
   type is never wider than the source, and extends otherwise. With
   c_long and c_wchar both cases are one width on some target, and the
   back end turns such a conversion into a copy. */
bool lower_narrows(enum ir_type from, enum ir_type to)
{
    return max_bits(to) <= min_bits(from);
}

/* Every block records the loops it sits inside. The allocator weighs a
   spill by it, because a value in a loop is read again on every pass. */
struct ir_block *lower_new_block(struct lowerer *l)
{
    struct ir_block *b = ir_block_add(l->f);

    b->loop_depth = l->loop_depth;
    return b;
}

/* Names in the tree point into the source and carry a length. Return a
   NUL-terminated copy of name, which the caller frees with free. */
char *lower_cstr(const struct name *name)
{
    char *s = ir_alloc(name->length + 1, 1);

    memcpy(s, name->text, name->length);
    s[name->length] = '\0';
    return s;
}

/* A function of the IR module by its module and name. A NULL module
   names a C function. */
struct ir_function *lower_find_function(const struct ir_module *m,
                                        const char *module, const char *name)
{
    size_t i;

    for (i = 0; i < m->function_count; i++) {
        struct ir_function *f = m->functions[i];
        if (strcmp(f->name, name) == 0 &&
            (module == NULL ? f->module == NULL
                            : f->module != NULL &&
                                  strcmp(f->module, module) == 0)) {
            return f;
        }
    }
    return NULL;
}

/* A global of the IR module by its module and name, or NULL. A NULL
   module names a global the runtime defines. */
struct ir_global *lower_find_global(const struct ir_module *m,
                                    const char *module, const char *name)
{
    size_t i;

    for (i = 0; i < m->global_count; i++) {
        struct ir_global *g = m->globals[i];
        if (strcmp(g->name, name) == 0 &&
            (module == NULL ? g->module == NULL
                            : g->module != NULL &&
                                  strcmp(g->module, module) == 0)) {
            return g;
        }
    }
    return NULL;
}

/* A copy of the text of t, which the caller frees with free. */
char *lower_copy_text(const struct text *t)
{
    char *copy = ir_alloc(t->length + 1, 1);

    memcpy(copy, text_cstr(t), t->length + 1);
    return copy;
}

uint32_t lower_result_agg(struct lowerer *l, const struct type *t)
{
    return lower_is_aggregate(t) ? lower_agg_of(l, t) : IR_NO_AGG;
}

static enum ir_ext param_ext(const struct type *t)
{
    enum ir_type type = lower_ir_type_of(t);

    if (type != IR_I8 && type != IR_I16) {
        return IR_EXT_NONE;
    }
    return type_is_signed(t) ? IR_EXT_SIGN : IR_EXT_ZERO;
}

/* A parameter of 8 or 16 bits records whether it is signed, and an
   aggregate its layout.
   DESIGN: a parameter that does not keep its argument is two parameters
   of the IR, the code and then the context. C passes a callback and its
   `void *` so. Every convention then passes two pointers, where a struct
   of two words would go by reference on Windows. */
void lower_add_param(struct lowerer *l, struct ir_function *f,
                     const struct type *t)
{
    enum ir_type type = lower_ir_type_of(t);

    if (lower_is_context(t)) {
        ir_param_add(f, IR_PTR, IR_NO_AGG);
        ir_param_add(f, IR_PTR, IR_NO_AGG);
        return;
    }
    ir_param_add(f, type, lower_result_agg(l, t));
    f->params[f->param_count - 1].ext = param_ext(t);
}

/* Append to args the operands that pass value to a parameter of type
   param. A function with its context passes the two words its aggregate
   at value holds, and every other value passes as it is. */
void lower_push_argument(struct lowerer *l, struct ir_operand *args,
                         size_t *count, struct ir_operand value,
                         const struct type *param)
{
    if (!lower_is_context(param)) {
        args[(*count)++] = value;
        return;
    }
    args[(*count)++] = lower_temp(l, ir_load(l->f, l->b, IR_PTR, value));
    args[(*count)++] = lower_temp(
        l, ir_load(l->f, l->b, IR_PTR,
                   lower_offset_address(
                       l, value,
                       ir_sym_operand(l->m,
                                      ir_sym_offset_of(
                                          l->m, lower_agg_of(l, param), 1)))));
}

/* The C library functions behind alloc and free. A module that declares
   one of them itself shares the declaration. */
struct ir_function *lower_c_function(struct lowerer *l, const char *name,
                                     enum ir_type result, enum ir_type param)
{
    struct ir_function *f = lower_find_function(l->m, NULL, name);

    if (f == NULL) {
        f = ir_extern_add(l->m, name, result, false);
        ir_param_add(f, param, IR_NO_AGG);
    }
    return f;
}

/* calloc of C, which takes a count and a size. */
struct ir_function *lower_calloc_function(struct lowerer *l)
{
    struct ir_function *f = lower_find_function(l->m, NULL, "calloc");

    if (f == NULL) {
        f = ir_extern_add(l->m, "calloc", IR_PTR, false);
        ir_param_add(f, IR_I64, IR_NO_AGG);
        ir_param_add(f, IR_I64, IR_NO_AGG);
    }
    return f;
}

/* The IR function of a function symbol. An imported function is found by
   its module and name, or declared at its first call. */
struct ir_function *lower_callee_function(struct lowerer *l,
                                          const struct symbol *sym)
{
    const struct type *t = sym->type;
    const char *module;
    char *name;
    struct ir_function *f;
    size_t i;

    /* DESIGN: a function of the root has no source. Its body is a symbol
       of the runtime under the prefix RUNTIME_ROOT, declared with the
       signature the checker gave the function. */
    if (sym->item != NULL && sym->item->runtime != NULL) {
        char symbol[64];
        snprintf(symbol, sizeof symbol, RUNTIME_ROOT "%s",
                 sym->item->runtime);
        f = lower_find_function(l->m, NULL, symbol);
        if (f == NULL) {
            f = ir_extern_add(l->m, symbol, lower_ir_type_of(t->result), false);
            f->result_agg = lower_result_agg(l, t->result);
            for (i = 0; i < t->param_count; i++) {
                lower_add_param(l, f, t->params[i]);
            }
        }
        return f;
    }
    if (sym->home == NULL) {
        return l->m->functions[sym->ir];
    }
    module = sym->kind == SYMBOL_EXTERN_FN ? NULL : sym->home->module;
    name = lower_cstr(&sym->name);
    f = lower_find_function(l->m, module, name);
    if (f == NULL) {
        if (module == NULL) {
            f = ir_extern_add(l->m, name, lower_ir_type_of(t->result),
                              sym->variadic);
            f->result_agg = lower_result_agg(l, t->result);
        } else {
            f = ir_declare_add(l->m, module, name, lower_ir_type_of(t->result),
                               lower_result_agg(l, t->result));
            f->exported = sym->exported;
        }
        for (i = 0; i < t->param_count; i++) {
            lower_add_param(l, f, t->params[i]);
        }
    }
    free(name);
    return f;
}

/* Whether the IR parameters of g from *at on are those of the parameters
   of t, and move *at past them. A parameter with its context is two. */
static bool has_params(struct lowerer *l, const struct ir_function *g,
                       const struct type *t, size_t *at)
{
    size_t i;

    for (i = 0; i < t->param_count; i++) {
        if (lower_is_context(t->params[i])) {
            if (*at + 2 > g->param_count ||
                g->params[*at].type != IR_PTR ||
                g->params[*at + 1].type != IR_PTR) {
                return false;
            }
            *at += 2;
            continue;
        }
        if (*at >= g->param_count ||
            g->params[*at].type != lower_ir_type_of(t->params[i]) ||
            g->params[*at].ext != param_ext(t->params[i]) ||
            g->params[*at].agg != lower_result_agg(l, t->params[i])) {
            return false;
        }
        (*at)++;
    }
    return true;
}

/* Whether declared function g has the parameters and result of t. */
static bool has_signature(struct lowerer *l, const struct ir_function *g,
                          const struct type *t)
{
    size_t at = 0;

    return g->result == lower_ir_type_of(t->result) &&
           g->result_agg == lower_result_agg(l, t->result) &&
           has_params(l, g, t, &at) && at == g->param_count;
}

/* The same with the context pointer after the parameters, which a call
   through a function with its context passes last. */
static bool has_context_signature(struct lowerer *l,
                                  const struct ir_function *g,
                                  const struct type *t)
{
    size_t at = 0;

    return g->result == lower_ir_type_of(t->result) &&
           g->result_agg == lower_result_agg(l, t->result) &&
           has_params(l, g, t, &at) && at + 1 == g->param_count &&
           g->params[at].type == IR_PTR && g->params[at].agg == IR_NO_AGG;
}

/* DESIGN: a call through a function pointer takes its parameters and
   result from a signature: a function fn.N that the module declares and
   never defines. No identifier contains a dot, so no Anti function has
   that name. Equal signatures share one declaration. find_signature
   returns the declaration for which fits holds, which may read t, or
   NULL with the count of the module's signatures in *count. */
static struct ir_function *find_signature(
    struct lowerer *l,
    bool (*fits)(struct lowerer *l, const struct ir_function *g,
                 const struct type *t),
    const struct type *t, size_t *count)
{
    size_t i;

    *count = 0;
    for (i = 0; i < l->m->function_count; i++) {
        struct ir_function *g = l->m->functions[i];
        if (!g->is_extern || g->module == NULL ||
            strcmp(g->module, l->module_name) != 0 ||
            strncmp(g->name, "fn.", 3) != 0) {
            continue;
        }
        if (fits(l, g, t)) {
            return g;
        }
        (*count)++;
    }
    return NULL;
}

/* Declare the signature fn.<count> with no parameters, which the caller
   adds. */
static struct ir_function *declare_signature(struct lowerer *l, size_t count,
                                             enum ir_type result,
                                             uint32_t agg)
{
    struct text name = {0};
    struct ir_function *f;

    text_appendf(&name, "fn.%zu", count);
    f = ir_declare_add(l->m, l->module_name, text_cstr(&name), result, agg);
    text_free(&name);
    return f;
}

const struct ir_function *lower_signature(struct lowerer *l,
                                          const struct type *t)
{
    size_t count;
    struct ir_function *f = find_signature(l, has_signature, t, &count);
    size_t i;

    if (f != NULL) {
        return f;
    }
    f = declare_signature(l, count, lower_ir_type_of(t->result),
                          lower_result_agg(l, t->result));
    for (i = 0; i < t->param_count; i++) {
        lower_add_param(l, f, t->params[i]);
    }
    return f;
}

/* DESIGN: a call through a function with its context passes the context
   after every other argument, the out pointer of `may fail` included. A
   closure takes it there. A named function, whose context is `none`, has
   no parameter there and never reads it. On each of the six conventions
   the caller removes what it pushed. The extra word is then harmless,
   and no call tests the context. */
const struct ir_function *lower_context_signature(struct lowerer *l,
                                                  const struct type *t)
{
    size_t count;
    struct ir_function *f =
        find_signature(l, has_context_signature, t, &count);
    size_t i;

    if (f != NULL) {
        return f;
    }
    f = declare_signature(l, count, lower_ir_type_of(t->result),
                          lower_result_agg(l, t->result));
    for (i = 0; i < t->param_count; i++) {
        lower_add_param(l, f, t->params[i]);
    }
    ir_param_add(f, IR_PTR, IR_NO_AGG);
    return f;
}

static bool fits_fatal(struct lowerer *l, const struct ir_function *g,
                       const struct type *t)
{
    (void)l;
    (void)t;
    return g->param_count == 1 && g->params[0].type == IR_PTR &&
           g->result == IR_VOID;
}

/* The signature of `fatal`, which takes the error and returns nothing.
   `catch fatal` calls it through the table of the error's class. */
const struct ir_function *lower_fatal_signature(struct lowerer *l)
{
    size_t count;
    struct ir_function *f = find_signature(l, fits_fatal, NULL, &count);

    if (f == NULL) {
        f = declare_signature(l, count, IR_VOID, IR_NO_AGG);
        ir_param_add(f, IR_PTR, IR_NO_AGG);
    }
    return f;
}

static bool fits_provider(struct lowerer *l, const struct ir_function *g,
                          const struct type *t)
{
    (void)l;
    (void)t;
    return g->param_count == 0 && g->result == IR_PTR;
}

/* DESIGN: a provider takes no arguments and gives a pointer of its
   interface. The slot of an interface holds one, and a call through the
   slot needs that signature. */
const struct ir_function *lower_provider_signature(struct lowerer *l)
{
    size_t count;
    struct ir_function *f = find_signature(l, fits_provider, NULL, &count);

    return f != NULL ? f : declare_signature(l, count, IR_PTR, IR_NO_AGG);
}

static bool fits_bound(struct lowerer *l, const struct ir_function *g,
                       const struct type *t)
{
    size_t at = 1;
    size_t k;

    (void)l;
    if (g->param_count == 0 || g->params[0].type != IR_PTR ||
        g->result != lower_ir_type_of(t->result)) {
        return false;
    }
    for (k = 0; k < t->param_count; k++) {
        size_t words = lower_is_context(t->params[k]) ? 2 : 1;
        if (at + words > g->param_count ||
            (words == 1 &&
             g->params[at].type != lower_ir_type_of(t->params[k]))) {
            return false;
        }
        at += words;
    }
    return at == g->param_count;
}

/* The signature of a call through a bound function, which takes the
   object as its first parameter and then the declared ones. */
const struct ir_function *lower_bound_signature(struct lowerer *l,
                                                const struct type *t)
{
    size_t count;
    struct ir_function *f = find_signature(l, fits_bound, t, &count);
    size_t i;

    if (f != NULL) {
        return f;
    }
    f = declare_signature(l, count, lower_ir_type_of(t->result),
                          lower_result_agg(l, t->result));
    ir_param_add(f, IR_PTR, IR_NO_AGG);
    for (i = 0; i < t->param_count; i++) {
        lower_add_param(l, f, t->params[i]);
    }
    return f;
}

double lower_float_literal(const struct expr *literal, enum ir_type type)
{
    return arith_float_literal(literal->as.text.bytes,
                               literal->as.text.length, type == IR_F32);
}

struct ir_operand lower_constant(struct lowerer *l,
                                 const struct const_value *v,
                                 enum ir_type type)
{
    switch (v->kind) {
    case CONST_SYMBOLIC:
        return ir_sym_operand(l->m, lower_sym_of(l, v->as.symbolic));
    /* An f16 is its bits. The value is one a half holds exactly, so the
       rounding changes nothing. */
    case CONST_FLOAT:
        if (v->type->kind == TYPE_F16) {
            return ir_int_op(type, anti_rt_f16_narrow((float)v->as.floating));
        }
        return ir_float_op(type, v->as.floating);
    case CONST_BOOL:
        return ir_int_op(type, v->as.boolean);
    case CONST_CHAR:
        return ir_int_op(type, v->as.character);
    case CONST_NULL:
        return ir_int_op(type, 0);
    default:
        return ir_int_op(type, v->as.integer);
    }
}

/* An address offset bytes after address. An offset of 0 is the address
   itself. */
struct ir_operand lower_offset_address(struct lowerer *l,
                                       struct ir_operand address,
                                       struct ir_operand offset)
{
    if (offset.kind == IR_INT && offset.as.integer == 0) {
        return address;
    }
    return lower_temp(l, ir_ptradd(l->f, l->b, address, offset));
}

struct ir_operand lower_zero(void)
{
    return ir_int_op(IR_I64, 0);
}

const struct struct_field *lower_field_of(const struct type *s,
                                          const struct name *name)
{
    size_t i;

    for (i = 0; i < s->field_count; i++) {
        if (s->fields[i].name.length == name->length &&
            memcmp(s->fields[i].name.text, name->text, name->length) == 0) {
            return &s->fields[i];
        }
    }
    return NULL;
}

/* DESIGN: the class of the chain that declares the field `name`. A class
   carries its base as field 0, so every class of the chain starts at the
   same address as the object. The address of an inherited field is
   therefore the object plus the field's offset in the class that
   declares it, with no step per level. */
const struct type *lower_field_owner(const struct type *t,
                                     const struct name *name)
{
    const struct type *s;

    for (s = t; s != NULL; s = s->kind == TYPE_CLASS ? s->base : NULL) {
        if (lower_field_of(s, name) != NULL) {
            return s;
        }
    }
    return t;
}

/* Write a free lock at at. It is the zero word of a Mutex, or the hidden
   lock of a synchronized object that no thread holds. */
void lower_zero_lock(struct lowerer *l, const struct type *t,
                     struct ir_operand at)
{
    size_t i;

    if (types_is_mutex(t)) {
        ir_store(l->f, l->b, IR_LOCK, ir_int_op(IR_LOCK, 0), at);
        return;
    }
    for (i = 0; i < t->field_count; i++) {
        struct ir_operand into = lower_offset_address(
            l, at, lower_field_offset(l, t, &t->fields[i].name));
        if (types_is_mutex(t->fields[i].type)) {
            lower_zero_lock(l, t->fields[i].type, into);
        } else {
            ir_store(l->f, l->b, IR_I64, ir_int_op(IR_I64, 0), into);
        }
    }
}

/* The address of the hidden lock of the synchronized object at object,
   whose class is t or inherits the class that declares the lock. */
struct ir_operand lower_object_lock_address(struct lowerer *l,
                                            const struct type *t,
                                            struct ir_operand object)
{
    static const struct name lock = {HIDDEN_LOCK, sizeof HIDDEN_LOCK - 1};
    const struct type *owner = lower_field_owner(t, &lock);

    return lower_offset_address(l, object,
                                lower_field_offset(l, owner, &lock));
}

bool lower_name_is(const struct name *name, const char *text)
{
    return name->length == strlen(text) &&
           memcmp(name->text, text, name->length) == 0;
}

const struct name lower_len_name = {"len", 3};

const struct name lower_entry_name = {"entry", 5};

/* The offset of a field as an operand. C places the first field and every
   field of a union at offset 0 on every target. Only a later field of a
   struct needs a symbolic offset. The ptr of a str or slice is its first
   field and len its second. A bound function holds its object first and
   its entry second. */
struct ir_operand lower_field_offset(struct lowerer *l, const struct type *s,
                                     const struct name *name)
{
    uint32_t index = type_has_fields(s) ? (uint32_t)(lower_field_of(s, name) -
                                                     s->fields)
                     : lower_name_is(name, "len") || lower_name_is(name,
                                                                   "entry") ? 1
                                                                      : 0;

    if (index == 0 || s->is_union) {
        return lower_zero();
    }
    return ir_sym_operand(l->m,
                          ir_sym_offset_of(l->m, lower_agg_of(l, s), index));
}

/* The offset of element index of an array of element type t: index times
   the size of t. */
struct ir_operand lower_element_offset(struct lowerer *l,
                                       const struct type *t, uint64_t index)
{
    if (index == 0) {
        return lower_zero();
    }
    return lower_temp(l, ir_binary(l->f, l->b, IR_MUL, IR_I64,
                                   ir_int_op(IR_I64, index),
                                   lower_size_operand(l, t)));
}

/* The number that names the next global of the module. */
static size_t globals_of_module(struct lowerer *l)
{
    size_t count = 0;
    size_t i;

    for (i = 0; i < l->m->global_count; i++) {
        if (l->m->globals[i]->module != NULL &&
            strcmp(l->m->globals[i]->module, l->module_name) == 0) {
            count++;
        }
    }
    return count;
}

/* DESIGN: the bytes of a literal and a NUL go into a global of the module,
   named by its index there. No identifier starts with a digit, so no
   function has that name. Literals with equal bytes share the global. */
const struct ir_global *lower_literal_global(struct lowerer *l,
                                             const struct token_text *text)
{
    struct ir_module *m = l->m;
    uint8_t *bytes;
    char name[24];
    size_t i;

    for (i = 0; i < m->global_count; i++) {
        const struct ir_global *g = m->globals[i];
        if (g->module == NULL ||
            strcmp(g->module, l->module_name) != 0 || g->bytes == NULL) {
            continue;
        }
        if (g->size == text->length + 1 && g->bytes[text->length] == 0 &&
            memcmp(g->bytes, text->bytes, text->length) == 0) {
            return g;
        }
    }
    bytes = arena_alloc(m->arena, text->length + 1);
    memcpy(bytes, text->bytes, text->length);
    bytes[text->length] = 0;
    snprintf(name, sizeof name, "%zu", globals_of_module(l));
    return ir_global_add(m, l->module_name, name, bytes, text->length + 1, 1);
}

struct ir_operand lower_literal_address(struct lowerer *l,
                                        const struct token_text *text)
{
    return lower_temp(l,
                      ir_addr(l->f, l->b,
                              ir_global_op(lower_literal_global(l, text))));
}

/* DESIGN: a hook site is one call of the runtime with the object and the
   hook. The runtime holds the handler, the order of the two calls and
   the compare against the root's empty body. The order then stands in
   one place and the compiler writes no branch. `--no-hooks` drops every
   site, the five always-on ones as well. */
void lower_hook_object(struct lowerer *l, enum hook_kind hook,
                       struct ir_operand object)
{
    static const enum ir_type params[] = {IR_PTR, IR_I64};
    struct ir_operand args[2];

    if (!l->hooks || l->b == NULL || object.kind == IR_NONE) {
        return;
    }
    args[0] = object;
    args[1] = ir_int_op(IR_I64, (uint64_t)hook);
    ir_call(l->f, l->b, IR_VOID,
            ir_func_op(lower_rt_function(l, "anti_rt_hook", params, 2)), args,
            2);
}

/* Whether the `--trace` pattern names the module path of the class, a
   package above it, or the class itself. */
static bool pattern_names(const char *pattern, const struct type *t)
{
    size_t length = strlen(pattern);

    if (t->name.length == length &&
        memcmp(t->name.text, pattern, length) == 0) {
        return true;
    }
    if (t->module.length == length &&
        memcmp(t->module.text, pattern, length) == 0) {
        return true;
    }
    return t->module.length > length && t->module.text[length] == '.' &&
           memcmp(t->module.text, pattern, length) == 0;
}

/* DESIGN: a class is instrumented when it asked for the call hooks with
   the contextual `trace` and the build compiles marked code. A `--trace`
   pattern that names its package or itself instruments it as well, which
   reaches code that did not ask. The library file carries the marking,
   so a class of another module answers the same question. */
bool lower_traced_class(const struct lowerer *l, const struct type *t)
{
    size_t i;

    if (!l->hooks || t == NULL || t->kind != TYPE_CLASS) {
        return false;
    }
    if (l->trace_marked && t->traced) {
        return true;
    }
    for (i = 0; i < l->pattern_count; i++) {
        if (pattern_names(l->patterns[i], t)) {
            return true;
        }
    }
    return false;
}

/* The `enter` or the `leave` hook of the function being lowered, which
   names itself. A function that is not instrumented writes none. */
void lower_hook_call(struct lowerer *l, enum hook_kind hook)
{
    static const enum ir_type params[] = {IR_PTR, IR_I64, IR_PTR, IR_I64};
    struct ir_operand args[4];

    if (l->trace_name == NULL || l->b == NULL) {
        return;
    }
    args[0] = l->trace_self;
    args[1] = ir_int_op(IR_I64, (uint64_t)hook);
    args[2] = lower_temp(l, ir_addr(l->f, l->b, ir_global_op(l->trace_name)));
    args[3] = ir_int_op(IR_I64, (uint64_t)l->trace_name_length);
    ir_call(l->f, l->b, IR_VOID,
            ir_func_op(lower_rt_function(l, "anti_rt_hook_call", params, 4)),
            args,
            4);
}

/* The `failed` hook, before the `leave` of an exit that gives an error. */
void lower_hook_failed(struct lowerer *l, struct ir_operand err)
{
    static const enum ir_type params[] = {IR_PTR, IR_PTR, IR_I64, IR_PTR};
    struct ir_operand args[4];

    if (l->trace_name == NULL || l->b == NULL ||
        err.kind == IR_NONE) {
        return;
    }
    args[0] = l->trace_self;
    args[1] = lower_temp(l, ir_addr(l->f, l->b, ir_global_op(l->trace_name)));
    args[2] = ir_int_op(IR_I64, (uint64_t)l->trace_name_length);
    args[3] = err;
    ir_call(l->f, l->b, IR_VOID,
            ir_func_op(lower_rt_function(l, "anti_rt_hook_failed", params, 4)),
            args, 4);
}

/* The `copied` hook, after `dup` made the object at `made` out of the
   one at `from`. */
void lower_hook_copied(struct lowerer *l, struct ir_operand made,
                       struct ir_operand from)
{
    static const enum ir_type params[] = {IR_PTR, IR_PTR};
    struct ir_operand args[2];

    if (!l->hooks || l->b == NULL || made.kind == IR_NONE) {
        return;
    }
    args[0] = made;
    args[1] = from;
    ir_call(l->f, l->b, IR_VOID,
            ir_func_op(lower_rt_function(l, "anti_rt_hook_copied", params, 2)),
            args, 2);
}

/* The text of a failed check: the file, the line and the operation. The
   values the kind names follow it at run time. The back end formats
   nothing, and a build without the checks drops the whole string. */
const struct ir_global *lower_check_text(struct lowerer *l, int line,
                                         const char *operation)
{
    struct token_text text;
    struct text message = {0};
    const struct ir_global *g;

    text_appendf(&message, "%s:%d: %s", l->file, line, operation);
    text.bytes = text_cstr(&message);
    text.length = message.length;
    g = lower_literal_global(l, &text);
    text_free(&message);
    return g;
}

/* The value v of an integer type as the i64 the failure routine takes. */
struct ir_operand lower_widen_operand(struct lowerer *l, struct ir_operand v,
                                      const struct type *t)
{
    if (lower_ir_type_of(t) == IR_I64) {
        return v;
    }
    return lower_temp(l, ir_unary(l->f, l->b,
                                  type_is_signed(t) ? IR_SEXT : IR_ZEXT, IR_I64,
                                  v));
}

/* DESIGN: the values the failure prints are widened inside the failure
   block, which runs only when the check fails. The path a program takes
   pays the test and the branch alone. widen names the type they are
   widened from, or is NULL when they are already i64. An empty b is a
   check that prints one value. */
static void check_call(struct lowerer *l, struct ir_block *fail,
                       struct ir_block *rest, const struct ir_global *text,
                       enum check_kind kind, struct ir_operand a,
                       struct ir_operand b, const struct type *widen)
{
    static const enum ir_type params[] = {IR_PTR, IR_I64, IR_I32, IR_I64,
                                          IR_I64};
    struct ir_operand args[5];

    l->b = fail;
    if (widen != NULL) {
        a = lower_widen_operand(l, a, widen);
        if (b.kind != IR_NONE) {
            b = lower_widen_operand(l, b, widen);
        }
    }
    args[0] = lower_temp(l, ir_addr(l->f, l->b, ir_global_op(text)));
    args[1] = ir_int_op(IR_I64, text->size - 1);
    args[2] = ir_int_op(IR_I32, (uint64_t)kind);
    args[3] = a;
    args[4] = b.kind == IR_NONE ? ir_int_op(IR_I64, 0) : b;
    ir_call(l->f, l->b, IR_VOID,
            ir_func_op(lower_rt_function(l, "anti_rt_check_failed", params, 5)),
            args, 5);
    ir_jump(l->f, l->b, rest);
    l->b = rest;
}

/* DESIGN: a dev-mode check is a branch to a block that calls the runtime
   and falls through to the rest, as an assertion is. The failure block
   carries its own kind, so the build that compiles the program drops the
   checks and the assertions under separate options. cond decides the
   failure when bad is set, and decides the rest otherwise. */
void lower_check_branch(struct lowerer *l, struct ir_operand cond, bool bad,
                        const struct ir_global *text, enum check_kind kind,
                        struct ir_operand a, struct ir_operand b,
                        const struct type *widen)
{
    struct ir_block *fail = lower_new_block(l);
    struct ir_block *rest = lower_new_block(l);

    fail->fail = IR_FAIL_CHECK;
    ir_branch(l->f, l->b, cond, bad ? fail : rest, bad ? rest : fail);
    check_call(l, fail, rest, text, kind, a, b, widen);
}

static enum ir_op overflow_op(enum token_kind op)
{
    return op == TOKEN_PLUS    ? IR_ADD_OV
           : op == TOKEN_MINUS ? IR_SUB_OV
                               : IR_MUL_OV;
}

/* DESIGN: the overflow test is the arithmetic itself. The operation
   gives its result and records whether it left the range, and the branch
   reads that. A dev build pays the branch and not a second add. The
   value of the expression is therefore the operation's result, which
   binary_checks returns. Every other check gives nothing back and is
   emitted before the operation, so a divisor of zero never reaches the
   instruction.

   The checks are overflow on a signed + - or *, a zero divisor of / and
   %, and a shift count outside the width of the type. Unsigned
   arithmetic wraps and is not checked. The width is the size of the type
   in bits, which a target-sized type leaves to the back end. */
struct ir_operand lower_binary_checks(struct lowerer *l, enum token_kind op,
                                      const struct type *t,
                                      struct ir_operand left,
                                      struct ir_operand right, int line)
{
    char operation[64];
    struct ir_operand ok;
    struct ir_operand count;
    struct ir_operand width;

    if (!type_is_integer(t)) {
        return lower_none();
    }
    switch (op) {
    case TOKEN_PLUS:
    case TOKEN_MINUS:
    case TOKEN_STAR: {
        const struct ir_global *text;
        struct ir_block *fail;
        struct ir_block *rest;
        struct ir_operand result;
        if (!type_is_signed(t)) {
            return lower_none();
        }
        snprintf(operation, sizeof operation, "overflow in %s",
                 op == TOKEN_PLUS ? "+" : op == TOKEN_MINUS ? "-" : "*");
        text = lower_check_text(l, line, operation);
        result = lower_temp(l, ir_binary(l->f, l->b, overflow_op(op),
                                         lower_ir_type_of(t), left, right));
        fail = lower_new_block(l);
        rest = lower_new_block(l);
        fail->fail = IR_FAIL_CHECK;
        ir_branch_ov(l->f, l->b, result, fail, rest);
        check_call(l, fail, rest, text, CHECK_OVERFLOW, left, right, t);
        return result;
    }
    case TOKEN_SLASH:
    case TOKEN_PERCENT:
        ok = lower_temp(l, ir_binary(l->f, l->b, IR_NE, IR_I8, right,
                                     ir_int_op(lower_ir_type_of(t), 0)));
        snprintf(operation, sizeof operation, "division by zero in %s",
                 op == TOKEN_SLASH ? "/" : "%");
        lower_check_branch(l, ok, false, lower_check_text(l, line, operation),
                           type_is_signed(t) ? CHECK_LEFT : CHECK_LEFT_U, left,
                           lower_none(), t);
        return lower_none();
    case TOKEN_SHL:
    case TOKEN_SHR:
        count = lower_widen_operand(l, right, t);
        width = lower_temp(l, ir_binary(l->f, l->b, IR_MUL, IR_I64,
                                        lower_size_operand(l, t),
                                        ir_int_op(IR_I64, 8)));
        ok = lower_temp(l, ir_binary(l->f, l->b, IR_ULT, IR_I8, count, width));
        snprintf(operation, sizeof operation,
                 "shift count out of range for %s",
                 op == TOKEN_SHL ? "<<" : ">>");
        lower_check_branch(l, ok, false, lower_check_text(l, line, operation),
                           CHECK_SHIFT, count, width, NULL);
        return lower_none();
    default:
        return lower_none();
    }
}

/* v.f is f's offset after the address of v. A pointer base p.f uses the
   pointer. */
struct ir_operand lower_field_address(struct lowerer *l,
                                      const struct expr *e)
{
    const struct expr *base = e->as.field.base;
    bool pointer = base->type->kind == TYPE_POINTER;
    const struct type *s = pointer ? base->type->element : base->type;
    struct ir_operand address;

    address = pointer ? lower_expr(l, base) : lower_address(l, base);
    return lower_offset_address(l, address,
                                lower_field_offset(l, s, &e->as.field.name));
}

/* The address of element 0: an array starts at its own address, a str or
   a slice at its pointer, and a pointer is the address. The count of
   elements comes with it, which an array takes from its type and a str
   or a slice reads beside the pointer. A raw pointer has none, and
   length is left empty. */
struct ir_operand lower_first_element(struct lowerer *l, const struct expr *e,
                                      struct ir_operand *length)
{
    struct ir_operand address;

    *length = lower_none();
    switch (e->type->kind) {
    case TYPE_ARRAY:
        address = lower_address(l, e);
        *length = e->type->length_of != NULL
                      ? ir_sym_operand(l->m,
                                       lower_sym_of(l, e->type->length_of))
                      : ir_int_op(IR_I64, e->type->length);
        return address;
    case TYPE_STR:
    case TYPE_SLICE:
        address = lower_address(l, e);
        *length = lower_slice_length(l, address, e->type);
        return lower_temp(l, ir_load(l->f, l->b, IR_PTR, address));
    default:
        return lower_expr(l, e);
    }
}

/* DESIGN: one unsigned comparison covers both ends. A negative index is
   a large unsigned value, so it fails the same test as an index past the
   length. The check stays a compare and a branch. */
static void bounds_check(struct lowerer *l, const struct expr *e,
                         struct ir_operand index, struct ir_operand length)
{
    struct ir_operand ok =
        lower_temp(l, ir_binary(l->f, l->b, IR_ULT, IR_I8, index, length));

    lower_check_branch(l, ok, false,
                       lower_check_text(l, e->as.index.index->pos.line,
                                        "index out of bounds"),
                       CHECK_BOUNDS, index, length, NULL);
}

struct ir_operand lower_element_address(struct lowerer *l,
                                        const struct expr *e)
{
    struct ir_operand length;
    struct ir_operand base = lower_first_element(l, e->as.index.base, &length);
    struct ir_operand index = lower_expr(l, e->as.index.index);
    uint32_t offset;

    if (length.kind != IR_NONE) {
        bounds_check(l, e, index, length);
    }
    offset = ir_binary(l->f, l->b, IR_MUL, IR_I64, index,
                       lower_size_operand(l, e->type));
    return lower_temp(l, ir_ptradd(l->f, l->b, base, lower_temp(l, offset)));
}

/* The field that e reads when it is a bitfield, or NULL. owner gets
   the struct, or the class of the chain, that declares it. */
static const struct struct_field *bitfield_of(const struct expr *e,
                                              const struct type **owner)
{
    const struct type *s;
    const struct struct_field *f;

    if (e->kind != EXPR_FIELD || e->symbol != NULL) {
        return NULL;
    }
    s = e->as.field.base->type;
    s = s->kind == TYPE_POINTER ? s->element : s;
    if (s->kind != TYPE_STRUCT && s->kind != TYPE_CLASS) {
        return NULL;
    }
    s = lower_field_owner(s, &e->as.field.name);
    f = lower_field_of(s, &e->as.field.name);
    *owner = s;
    return f != NULL && f->bits != 0 ? f : NULL;
}

static void const_tree(struct lowerer *l, const struct const_value *v,
                       const struct type *t, struct ir_const *out);

/* DESIGN: a static field is a global of the module that declares its
   class, named `Class.field`, with the declared value written into it.
   Every mention of the field reaches the same global. */
struct ir_global *lower_static_global(struct lowerer *l,
                                      const struct symbol *sym)
{
    struct ir_module *m = l->m;
    struct ir_const *value;
    char *name = lower_cstr(&sym->name);
    struct ir_global *g = lower_find_global(m, l->module_name, name);

    if (g == NULL) {
        value = arena_alloc(m->arena, sizeof *value);
        const_tree(l, sym->value, sym->type, value);
        g = ir_global_add_value(m, l->module_name, name, value);
        g->mutable = true;
    }
    free(name);
    return g;
}

bool lower_place(struct lowerer *l, const struct expr *e,
                 struct place *p)
{
    const struct symbol *sym = e->symbol;
    const struct struct_field *bits;
    const struct type *owner = NULL;

    p->bitfield = false;
    p->in_temp = false;
    p->object = lower_none();
    p->owner = NULL;
    p->type = lower_ir_type_of(e->type);
    switch (e->kind) {
    /* A name narrowed from a `?T` reads the value at offset 0 of the
       variable, which lives where its declared type puts it. */
    case EXPR_NAME:
        p->in_temp = !sym->address_taken && !lower_is_aggregate(sym->type) &&
                     !lower_is_aggregate(e->type);
        p->temp = sym->ir;
        p->address = p->in_temp ? lower_none() : lower_temp(l, sym->ir);
        return true;
    case EXPR_UNARY:
        p->address = lower_expr(l, e->as.unary.operand);
        return true;
    case EXPR_INDEX:
        p->address = lower_element_address(l, e);
        return true;
    case EXPR_FIELD:
        /* A static field of a class is a global of the module. */
        if (sym != NULL && sym->kind == SYMBOL_GLOBAL) {
            p->address = lower_temp(
                l, ir_addr(l->f, l->b,
                           ir_global_op(lower_static_global(l, sym))));
            return true;
        }
        bits = bitfield_of(e, &owner);
        if (bits != NULL) {
            const struct expr *base = e->as.field.base;
            /* Every class of a chain starts at the address of the
               object, so the unit is found from there. */
            p->bitfield = true;
            p->agg = lower_agg_of(l, owner);
            p->field = (uint32_t)(bits - owner->fields);
            p->address = base->type->kind == TYPE_POINTER
                             ? lower_expr(l, base)
                             : lower_address(l, base);
            p->object = p->address;
            p->owner = owner;
            return true;
        }
        {
            const struct expr *base = e->as.field.base;
            bool pointer = base->type->kind == TYPE_POINTER;
            const struct type *s = pointer ? base->type->element : base->type;
            struct ir_operand address =
                pointer ? lower_expr(l, base) : lower_address(l, base);
            p->object = address;
            p->owner = s;
            p->address = lower_offset_address(
                l, address, lower_field_offset(l, s, &e->as.field.name));
        }
        return true;
    default:
        /* DESIGN: a value that is no place still has an address once it
           is written somewhere. A literal receiver of an operator is the
           case, and lower_address gives it a slot of its own. */
        p->address = lower_address(l, e);
        return true;
    }
}

/* The scalar constant v as a leaf of a constant tree. */
static void const_scalar(struct lowerer *l, const struct const_value *v,
                         enum ir_type type, struct ir_const *out)
{
    struct ir_operand o = lower_constant(l, v, type);

    out->scalar = type;
    if (o.kind == IR_FLOAT) {
        out->kind = IR_CONST_FLOAT;
        out->floating = o.as.floating;
    } else if (o.kind == IR_SYM) {
        out->kind = IR_CONST_SYM;
        out->sym = o.as.index;
    } else {
        out->kind = IR_CONST_INT;
        out->integer = o.as.integer;
    }
}

/* The constant v of type t as the tree the back end lays out. A field of
   a struct keeps its place in the tree, so an item and a field share an
   index. The back end reads the offset of one from the other. */
static void const_tree(struct lowerer *l, const struct const_value *v,
                       const struct type *t, struct ir_const *out)
{
    struct ir_const *agg;
    uint64_t i;

    if (v->kind == CONST_TEXT) {
        agg = ir_const_agg(l->m, lower_vtype_of(l, t), 2);
        agg->items[0].kind = IR_CONST_ADDR;
        agg->items[0].scalar = IR_PTR;
        agg->items[0].global = lower_literal_global(l, &v->as.text)->index;
        agg->items[1].kind = IR_CONST_INT;
        agg->items[1].scalar = IR_I64;
        agg->items[1].integer = v->as.text.length;
        *out = *agg;
    } else if (type_has_fields(t)) {
        agg = ir_const_agg(l->m, lower_vtype_of(l, t), t->field_count);
        for (i = 0; i < t->field_count; i++) {
            if (type_field_is_unit_break(&t->fields[i])) {
                continue;
            }
            const_tree(l, &v->as.aggregate.items[i], t->fields[i].type,
                       &agg->items[i]);
        }
        *out = *agg;
    } else if (t->kind == TYPE_ARRAY) {
        agg = ir_const_agg(l->m, lower_vtype_of(l, t), v->as.aggregate.count);
        for (i = 0; i < v->as.aggregate.count; i++) {
            const_tree(l, &v->as.aggregate.items[i], t->element,
                       &agg->items[i]);
        }
        *out = *agg;
    } else {
        const_scalar(l, v, lower_ir_type_of(t), out);
    }
}

/* DESIGN: an aggregate constant is read-only data of the module, named by
   its index there as a literal is. Every use reads the same bytes, the
   copy is one memcopy, and a constant that no use reaches costs nothing.
   Two constants of equal value share the data. */
struct ir_operand lower_const_address(struct lowerer *l,
                                      const struct const_value *v,
                                      const struct type *t)
{
    struct ir_module *m = l->m;
    struct ir_const *value = arena_alloc(m->arena, sizeof *value);
    char name[24];
    size_t i;

    const_tree(l, v, t, value);
    for (i = 0; i < m->global_count; i++) {
        const struct ir_global *g = m->globals[i];
        if (g->module != NULL && strcmp(g->module, l->module_name) == 0 &&
            ir_const_equal(g->value, value)) {
            return lower_temp(l, ir_addr(l->f, l->b, ir_global_op(g)));
        }
    }
    snprintf(name, sizeof name, "%zu", globals_of_module(l));
    return lower_temp(
        l, ir_addr(l->f, l->b,
                   ir_global_op(ir_global_add_value(m, l->module_name, name,
                                                    value))));

}

/* The bytes of s as the text of a constant, kept in the module's memory. */
static void text_constant(struct lowerer *l, struct const_value *v,
                          const char *s)
{
    size_t n = strlen(s);
    char *bytes = arena_alloc(l->m->arena, n + 1);

    memcpy(bytes, s, n + 1);
    v->kind = CONST_TEXT;
    v->as.text.bytes = bytes;
    v->as.text.length = n;
}

/* DESIGN: a pattern literal is a global of the module that holds its
   Regex, one per distinct pattern and mode. The global starts empty, and the
   function IR_PATTERNS_START of the module, which runs before main, fills it
   through the runtime. A use of the literal reads the global, so no
   pattern is compiled lazily and no flag guards a first use. */
struct ir_operand lower_pattern(struct lowerer *l, const struct expr *e)
{
    static const uint8_t empty[8] = {0};
    const struct ir_global *text = lower_literal_global(l, &e->as.text);
    struct ir_global *slot;
    char name[32];
    size_t i;

    bool bytes = types_is_byte_regex(e->type);

    for (i = 0; i < l->regex_count; i++) {
        if (l->regex_literals[i].text == text->index &&
            l->regex_literals[i].bytes == bytes) {
            slot = l->m->globals[l->regex_literals[i].slot];
            return lower_temp(l, ir_addr(l->f, l->b, ir_global_op(slot)));
        }
    }
    snprintf(name, sizeof name, "pattern.%zu", l->regex_count);
    /* The one field of a Regex is a pointer, 8 bytes on every target. */
    slot = ir_global_add(l->m, l->module_name, name, empty, sizeof empty, 8);
    slot->mutable = true;
    if (l->regex_count == l->regex_capacity) {
        size_t capacity = l->regex_capacity == 0 ? 4 : 2 * l->regex_capacity;
        struct lower_pattern *grown =
            realloc(l->regex_literals, capacity * sizeof *grown);
        if (grown == NULL) {
            fprintf(stderr, "antic: out of memory\n");
            exit(1);
        }
        l->regex_literals = grown;
        l->regex_capacity = capacity;
    }
    l->regex_literals[l->regex_count].text = text->index;
    l->regex_literals[l->regex_count].length = (int64_t)e->as.text.length;
    l->regex_literals[l->regex_count].slot = slot->index;
    l->regex_literals[l->regex_count].bytes = bytes;
    l->regex_count++;
    return lower_temp(l, ir_addr(l->f, l->b, ir_global_op(slot)));
}

/* The function IR_PATTERNS_START of the module, which compiles each pattern
   literal into its global. The back end makes it a constructor of the
   object, so it runs before main in a program and when a library loads. */
static void patterns_start(struct lowerer *l)
{
    static const enum ir_type params[] = {IR_PTR, IR_I64};
    struct ir_operand args[2];
    struct ir_operand made;
    size_t i;

    if (l->regex_count == 0) {
        return;
    }
    l->f = ir_function_add(l->m, l->module_name, IR_PATTERNS_START, IR_VOID,
                           IR_NO_AGG);
    l->b = ir_block_add(l->f);
    for (i = 0; i < l->regex_count; i++) {
        const struct lower_pattern *p = &l->regex_literals[i];
        args[0] = lower_temp(l, ir_addr(l->f, l->b,
                                        ir_global_op(l->m->globals[p->text])));
        args[1] = ir_int_op(IR_I64, (uint64_t)p->length);
        made = lower_rt_call(l, p->bytes ? REGEX_LITERAL_BYTES : REGEX_LITERAL,
                             IR_PTR, params, args, 2);
        ir_store(l->f, l->b, IR_PTR, made,
                 lower_temp(l, ir_addr(l->f, l->b,
                                       ir_global_op(l->m->globals[p->slot]))));
    }
    ir_ret(l->f, l->b, IR_VOID, lower_none());
    l->f = NULL;
    l->b = NULL;
}

/* DESIGN: `here` is constant data of the module. It holds the path that
   a failed check names, the line and the column, the function around the
   position and the module. Lowering builds it, because the path and the
   function are what lowering records. The fields are found by name, so
   the order `anti.lang` gives them is its own. */
const struct const_value *lower_location_value(struct lowerer *l,
                                               struct pos pos,
                                               const struct type *t)
{
    struct const_value *v = arena_alloc(l->m->arena, sizeof *v);
    struct text function = {0};
    size_t i;

    memset(v, 0, sizeof *v);
    v->kind = CONST_STRUCT;
    v->type = (struct type *)t;
    v->as.aggregate.count = t->field_count;
    v->as.aggregate.items =
        arena_alloc(l->m->arena, ir_product(t->field_count + 1, sizeof *v));
    if (l->f != NULL) {
        text_appendf(&function, "%s.%s", l->module_name, l->f->name);
    }
    for (i = 0; i < t->field_count; i++) {
        struct const_value *item = &v->as.aggregate.items[i];
        const struct name *name = &t->fields[i].name;
        memset(item, 0, sizeof *item);
        item->type = t->fields[i].type;
        item->kind = item->type->kind == TYPE_STR ? CONST_TEXT : CONST_INT;
        if (lower_name_is(name, LANG_LOCATION_FILE)) {
            text_constant(l, item, l->file);
        } else if (lower_name_is(name, LANG_LOCATION_FUNCTION)) {
            text_constant(l, item, text_cstr(&function));
        } else if (lower_name_is(name, LANG_LOCATION_MODULE)) {
            text_constant(l, item, l->module_name);
        } else if (lower_name_is(name, LANG_LOCATION_LINE)) {
            item->as.integer = (uint64_t)pos.line;
        } else if (lower_name_is(name, LANG_LOCATION_COLUMN)) {
            item->as.integer = (uint64_t)pos.column;
        } else if (item->kind == CONST_TEXT) {
            text_constant(l, item, "");
        }
    }
    text_free(&function);
    return v;
}

/* The aggregate of an array of n elements of type element, whose name is
   element_name. Every use of one length shares it, as any two equal
   array types do. The caller builds element first, since that may add an
   aggregate and the length adds a symbol. */
uint32_t lower_array_agg(struct lowerer *l, const char *element_name,
                         struct ir_vtype element, size_t n)
{
    struct text name = {0};
    uint32_t agg;
    uint32_t length;

    text_appendf(&name, "[%zu]%s", n, element_name);
    agg = ir_agg_find(l->m, text_cstr(&name));
    if (agg == IR_NO_AGG) {
        length = ir_sym_int(l->m, IR_I64, (uint64_t)n);
        agg = ir_array_add(l->m, text_cstr(&name), element, length, NULL);
    }
    text_free(&name);
    return agg;
}

/* The aggregate of a table of n entries: an array of n pointers. */
uint32_t lower_table_agg(struct lowerer *l, size_t n)
{
    return lower_array_agg(l, "ptr", ir_scalar(IR_PTR), n);
}

/* The class behind a value of type T or *T, or NULL. */
const struct type *lower_struct_of_expr(const struct expr *e)
{
    const struct type *t = e->type;

    if (t == NULL) {
        return NULL;
    }
    if (t->kind == TYPE_POINTER) {
        t = t->element;
    }
    return t->kind == TYPE_CLASS ? t : NULL;
}

/* Whether a bound function names one body. A `final` function and a
   `final` class have no class below them to replace it. A plain body
   beside one qualified by a base holds no entry of the primary table. */
bool lower_bound_is_direct(const struct expr *e, const struct type *s)
{
    const struct item *m = e->symbol != NULL ? e->symbol->item : NULL;

    return s == NULL || s->is_final ||
           (m != NULL && (m->is_final || !types_holds_entry(s, m)));
}

static void declare_function(struct lowerer *l, const struct item *it)
{
    const struct type *t = it->symbol->type;
    /* A function of a struct body carries the name `T.f`, which its
       symbol holds, so its symbol becomes `module.T.f`. An `operator fn`
       that shares its name carries `eq:T` there as well. */
    char *name = lower_cstr(it->owner != NULL || it->overloaded
                                ? &it->symbol->name
                                : &it->name);
    const char *module =
        it->home_module != NULL ? it->home_module : l->module_name;
    struct ir_function *f;
    size_t i;

    f = it->kind == ITEM_EXTERN_FN ? lower_find_function(l->m, NULL,
                                                         name) : NULL;
    /* A copy of a generic of another module that a library file of the
       program holds already is that one. */
    if (f == NULL && it->home_module != NULL) {
        f = lower_find_function(l->m, module, name);
        if (f != NULL && !f->is_extern) {
            free(name);
            it->symbol->ir = f->index;
            return;
        }
        f = NULL;
    }
    if (f == NULL) {
        f = it->kind == ITEM_FN
                ? ir_function_add(l->m, module, name,
                                  lower_ir_type_of(t->result),
                                  lower_result_agg(l, t->result))
                : ir_extern_add(l->m, name, lower_ir_type_of(t->result),
                                it->variadic);
        f->result_agg = lower_result_agg(l, t->result);
        f->exported = it->exported;
        f->worker = it->worker;
        for (i = 0; i < t->param_count; i++) {
            lower_add_param(l, f, t->params[i]);
        }
    }
    free(name);
    it->symbol->ir = f->index;
}

/* DESIGN: a hook is never instrumented. `enter` and `leave` around a
   body of `enter` would call themselves without end, and a handler's
   nine are hooks as much as a class's own. The name decides, so a
   function of any signature under one of the nine names is left alone. */
/* Whether the function it takes the `enter`, `leave` and `failed` hooks.
   It is a `pub` function of an instrumented class, or a `trace fn` of
   one, and it has an object to hook. */
static bool traced_function(const struct lowerer *l, const struct item *it)
{
    const struct type *owner = it->owner != NULL && it->owner->symbol != NULL
                                   ? it->owner->symbol->type
                                   : NULL;

    if (!it->has_self || owner == NULL || owner->kind != TYPE_CLASS ||
        lower_hook_name(&it->name)) {
        return false;
    }
    if (l->hooks && l->trace_marked && it->trace) {
        return true;
    }
    return it->vis == VIS_PUB && lower_traced_class(l, owner);
}

/* The literal of the full name of the function, `module.Class.f`, which
   the `enter`, `leave` and `failed` hooks take. */
static void take_trace_name(struct lowerer *l)
{
    struct token_text text;
    struct text name = {0};

    text_appendf(&name, "%s.%s", l->f->module, l->f->name);
    text.bytes = text_cstr(&name);
    text.length = name.length;
    l->trace_name = lower_literal_global(l, &text);
    l->trace_name_length = (int64_t)name.length;
    text_free(&name);
}

static void snapshot_entry(struct lowerer *l, const struct item *it,
                           struct ir_block *entry);

/* Whether it is a function of a synchronized class that takes the hidden
   lock of its object. */
static bool synchronized_function(const struct item *it)
{
    return it->owner != NULL && it->owner->symbol != NULL &&
           it->owner->symbol->type->kind == TYPE_CLASS &&
           it->owner->symbol->type->safety == SAFETY_SYNCHRONIZED &&
           it->has_self && it->vis != VIS_PRIVATE &&
           !lower_name_is(&it->name, "construct") &&
           !lower_name_is(&it->name, "destruct");
}

/* The copy of a generic of another module, or an anonymous function of
   one, and so the source its lines name. NULL for any other function. */
static const char *home_file_of(const struct item *it)
{
    while (it->enclosing != NULL) {
        it = it->enclosing;
    }
    return it->home_file;
}

static void lower_function_body(struct lowerer *l, const struct item *it);

static void lower_function(struct lowerer *l, const struct item *it)
{
    const char *home = home_file_of(it);
    const char *file = l->file;
    uint32_t file_index = l->file_index;

    /* A function a library file brought has its body there. */
    if (it->symbol->ir < l->first_function) {
        return;
    }
    if (home != NULL) {
        l->file = home;
        l->file_index = ir_file_add(l->m, home);
        l->m->functions[it->symbol->ir]->file = l->file_index;
    }
    lower_function_body(l, it);
    l->file = file;
    l->file_index = file_index;
}

static void lower_function_body(struct lowerer *l, const struct item *it)
{
    struct defers around;
    struct ir_block *entry;
    size_t first;
    size_t at;
    size_t i;

    l->f = l->m->functions[it->symbol->ir];
    l->f->decl_line = (uint32_t)it->pos.line;
    l->loop = NULL;
    l->defers = NULL;
    l->trace_name = NULL;
    l->failing_error = lower_none();
    l->may_fail = it->may_fail;
    l->result_out = lower_none();
    entry = lower_new_block(l);
    l->b = entry;
    /* DESIGN: `self` is the first IR parameter of a member function
       and has no entry in the declared list. Every declared parameter
       therefore sits one place further along. One that does not keep its
       argument takes two places, which a slot joins into its pair. */
    first = it->has_self ? 1 : 0;
    if (it->self != NULL) {
        it->self->ir = l->f->params[0].temp;
        /* A closure captures `self` by its address. */
        if (it->self->address_taken) {
            it->self->ir = ir_slot(l->f, entry, ir_scalar(IR_PTR));
        }
    }
    at = first;
    for (i = 0; i < it->param_count; i++) {
        struct symbol *sym = it->params[i].symbol;
        sym->ir = l->f->params[at].temp;
        if (lower_is_context(sym->type) ||
            (sym->address_taken && !lower_is_aggregate(sym->type))) {
            sym->ir = ir_slot(l->f, entry, lower_vtype_of(l, sym->type));
        }
        at += lower_is_context(sym->type) ? 2 : 1;
    }
    lower_reserve_slots(l, entry, it->body);
    if (it->self != NULL && it->self->address_taken) {
        ir_store(l->f, entry, IR_PTR, lower_temp(l, l->f->params[0].temp),
                 lower_temp(l, it->self->ir));
    }
    at = first;
    for (i = 0; i < it->param_count; i++) {
        const struct symbol *sym = it->params[i].symbol;
        if (lower_is_context(sym->type)) {
            ir_store(l->f, entry, IR_PTR, lower_temp(l, l->f->params[at].temp),
                     lower_temp(l, sym->ir));
            ir_store(l->f, entry, IR_PTR,
                     lower_temp(l, l->f->params[at + 1].temp),
                     lower_offset_address(
                         l, lower_temp(l, sym->ir),
                         ir_sym_operand(l->m,
                                        ir_sym_offset_of(
                                            l->m, lower_agg_of(l, sym->type),
                                            1))));
            at += 2;
            continue;
        }
        if (sym->address_taken && !lower_is_aggregate(sym->type)) {
            ir_store(l->f, entry, l->f->params[at].type,
                     lower_temp(l, l->f->params[at].temp),
                     lower_temp(l, sym->ir));
        }
        at++;
    }
    /* The out pointer of a `may fail` function with a result follows the
       parameters the declaration wrote, which is the ABI its callers
       already pass. */
    if (it->may_fail && at < l->f->param_count &&
        it->symbol->type->has_out) {
        l->result_out = lower_temp(l, l->f->params[at].temp);
    }
    /* DESIGN: the context of a closure is its last parameter. It holds
       the address of every variable the closure captures, in the order
       of the captures. Each name reaches its variable through the
       address loaded here. */
    if (it->capture_count > 0 && it->snapshot) {
        snapshot_entry(l, it, entry);
    } else if (it->capture_count > 0) {
        struct ir_operand context =
            lower_temp(l, l->f->params[l->f->param_count - 1].temp);
        uint32_t agg = lower_captures_agg(l, it);
        for (i = 0; i < it->capture_count; i++) {
            it->captures[i].symbol->ir = ir_load(
                l->f, entry, IR_PTR,
                lower_offset_address(
                    l, context,
                    i == 0 ? lower_zero()
                           : ir_sym_operand(l->m,
                                            ir_sym_offset_of(l->m, agg,
                                                             (uint32_t)i))));
        }
    }
    l->b = entry;
    /* DESIGN: the `enter` hook stands before the first statement and the
       `leave` hook is an exit action of a scope around the whole body,
       so every `return`, every `fail` and the closing brace run it, and
       it runs after the locals of the body are gone. */
    memset(&around, 0, sizeof around);
    l->defers = &around;
    for (i = 0; i < it->param_count; i++) {
        const struct symbol *sym = it->params[i].symbol;
        if (sym->type->kind == TYPE_FN && sym->type->owned) {
            lower_push_snapshot_action(l, sym);
        } else if (sym->own_param) {
            lower_push_own_action(l, sym);
        }
    }
    if (traced_function(l, it)) {
        take_trace_name(l);
        l->trace_self = lower_temp(l, l->f->params[0].temp);
        lower_hook_call(l, HOOK_ENTER);
        lower_push_leave_action(l);
    }
    /* DESIGN: code outside a synchronized class calls a function of it,
       which takes the hidden lock of its object when it starts. Every
       exit gives it back, as the unlock of `sync` does. A
       private function runs inside the lock of the one that called it,
       and `construct` and `destruct` run where no other thread sees the
       object. */
    if (synchronized_function(it)) {
        lower_hold_lock(l,
                        lower_object_lock_address(
                            l, it->owner->symbol->type,
                            lower_temp(l, l->f->params[0].temp)),
                        true, it->pos.line);
    }
    lower_block(l, it->body);
    if (l->b != NULL) {
        lower_run_defers(l, &around, false);
    }
    l->defers = NULL;
    free(around.items);
    /* Semantic analysis rejects a function with a result that can reach
       its end, so only a function without one gets here. A `may fail`
       function reports success there. */
    if (l->b != NULL) {
        if (it->may_fail) {
            ir_ret(l->f, l->b, IR_PTR, ir_int_op(IR_PTR, 0));
        } else {
            ir_ret(l->f, l->b, IR_VOID, lower_none());
        }
    }
}

/* DESIGN: a snapshot is one record: its size in bytes, then a copy of
   each value it captures in the order of the captures. A snapshot on
   the heap holds the bytes of each `str` after the record, and the `ptr`
   of the `str` holds their offset from the start. It then moves and
   copies as a block of bytes, so freeing it and `dup` need nothing of
   the closure. A snapshot in the frame holds each `str` as it was, since
   the caller waits and the bytes stay. */
uint32_t lower_snapshot_agg(struct lowerer *l, const struct item *it)
{
    struct ir_field *fields = ir_alloc(it->capture_count + 1,
                                       sizeof *fields);
    struct text name = {0};
    uint32_t agg;
    size_t i;

    fields[0].name = "size";
    fields[0].type = ir_scalar(IR_I64);
    for (i = 0; i < it->capture_count; i++) {
        fields[i + 1].name = lower_cstr(&it->captures[i].symbol->name);
        fields[i + 1].type = lower_vtype_of(l, it->captures[i].symbol->type);
    }
    text_appendf(&name, "%s.snapshot", l->m->functions[it->symbol->ir]->name);
    agg = ir_struct_add(l->m, IR_AGG_STRUCT, text_cstr(&name), fields,
                        it->capture_count + 1, false, 0);
    text_free(&name);
    for (i = 0; i < it->capture_count; i++) {
        free((char *)fields[i + 1].name);
    }
    free(fields);
    return agg;
}

/* The entry of a snapshot: each captured name reaches its copy in the
   record that the context points at. A `str` of a snapshot on the heap
   is rebuilt in a slot from its offset. */
static void snapshot_entry(struct lowerer *l, const struct item *it,
                           struct ir_block *entry)
{
    struct ir_operand base =
        lower_temp(l, l->f->params[l->f->param_count - 1].temp);
    uint32_t agg = lower_snapshot_agg(l, it);
    size_t i;

    for (i = 0; i < it->capture_count; i++) {
        struct symbol *sym = it->captures[i].symbol;
        uint32_t at = ir_ptradd(
            l->f, entry, base,
            ir_sym_operand(l->m,
                           ir_sym_offset_of(l->m, agg, (uint32_t)(i + 1))));
        struct ir_operand len_offset;
        struct ir_operand offset;
        uint32_t slot;
        uint32_t ptr;

        if (!it->snapshot_heap || sym->type->kind != TYPE_STR) {
            sym->ir = at;
            continue;
        }
        len_offset = lower_field_offset(l, sym->type, &lower_len_name);
        slot = ir_slot(l->f, entry, lower_vtype_of(l, sym->type));
        offset = lower_temp(l, ir_load(l->f, entry, IR_I64, lower_temp(l, at)));
        ptr = ir_ptradd(l->f, entry, base, offset);
        ir_store(l->f, entry, IR_PTR, lower_temp(l, ptr), lower_temp(l, slot));
        ir_store(l->f, entry, IR_I64,
                 lower_temp(l, ir_load(l->f, entry, IR_I64,
                                       lower_temp(l, ir_ptradd(
                                                         l->f, entry,
                                                         lower_temp(l, at),
                                                         len_offset)))),
                 lower_temp(l, ir_ptradd(l->f, entry, lower_temp(l, slot),
                                         len_offset)));
        sym->ir = slot;
    }
}

/* The aggregate of the context of the closure it: one pointer per
   variable it captures, in the order of the captures. */
uint32_t lower_captures_agg(struct lowerer *l, const struct item *it)
{
    struct ir_field *fields = ir_alloc(it->capture_count, sizeof *fields);
    struct text name = {0};
    uint32_t agg;
    size_t i;

    for (i = 0; i < it->capture_count; i++) {
        fields[i].name = lower_cstr(&it->captures[i].symbol->name);
        fields[i].type = ir_scalar(IR_PTR);
    }
    text_appendf(&name, "%s.context", l->m->functions[it->symbol->ir]->name);
    agg = ir_struct_add(l->m, IR_AGG_STRUCT, text_cstr(&name), fields,
                        it->capture_count, false, 0);
    text_free(&name);
    for (i = 0; i < it->capture_count; i++) {
        free((char *)fields[i].name);
    }
    free(fields);
    return agg;
}

/* The IR function of the anonymous function it. The first expression
   that names it declares it and queues it, and it is lowered after the
   function being lowered now. A `defer` lowers its statement at every
   exit, so one expression may be met more than once. A closure takes its
   context after every other parameter. */
struct ir_function *lower_anonymous_function(struct lowerer *l,
                                             struct item *it)
{
    const struct type *t = it->symbol->type;
    struct text name = {0};
    struct ir_function *f;
    size_t i;

    for (i = 0; i < l->anonymous_count; i++) {
        if (l->anonymous[i] == it) {
            return l->m->functions[it->symbol->ir];
        }
    }
    text_appendf(&name, "%s.%zu", l->f->name, l->anonymous_named++);
    f = ir_function_add(l->m, l->module_name, text_cstr(&name),
                        lower_ir_type_of(t->result),
                        lower_result_agg(l, t->result));
    text_free(&name);
    for (i = 0; i < t->param_count; i++) {
        lower_add_param(l, f, t->params[i]);
    }
    if (it->capture_count > 0) {
        ir_param_add(f, IR_PTR, IR_NO_AGG);
    }
    it->symbol->ir = f->index;
    l->anonymous = ir_grow(l->anonymous, &l->anonymous_capacity,
                           l->anonymous_count, sizeof *l->anonymous);
    l->anonymous[l->anonymous_count++] = it;
    return f;
}

/* DESIGN: a singleton keeps its one instance in an atomic global of its
   own module. `get` reads it, and on the first call it builds an object
   and puts it there with a compare and swap. A second thread that lost
   the race frees what it made and reads the winner. */
static struct ir_global *singleton_instance(struct lowerer *l,
                                            const struct type *t)
{
    struct ir_const *value;
    struct ir_global *g;
    char *module;
    char *name;

    g = lower_class_global(l, t, "instance", &module, &name);
    if (g != NULL) {
        return g;
    }
    value = arena_alloc(l->m->arena, sizeof *value);
    value->kind = IR_CONST_INT;
    value->scalar = IR_PTR;
    value->integer = 0;
    g = ir_global_add_value(l->m, module, name, value);
    g->mutable = true;
    g->exported = t->item_exported;
    free(module);
    free(name);
    return g;
}

static void lower_singleton_get(struct lowerer *l, const struct item *it)
{
    static const enum ir_type load_params[] = {IR_PTR, IR_I64};
    static const enum ir_type swap_params[] = {IR_PTR, IR_I64, IR_I64,
                                               IR_I64};
    const struct type *t = it->owner->symbol->type;
    struct ir_operand address;
    struct ir_operand args[4];
    struct ir_operand made;
    struct ir_operand first;
    struct ir_block *build;
    struct ir_block *lost;
    struct ir_block *done;
    uint32_t result;
    uint32_t width;

    /* A function a library file brought has its body there. */
    if (it->symbol->ir < l->first_function) {
        return;
    }

    l->f = l->m->functions[it->symbol->ir];
    l->b = lower_new_block(l);
    address = lower_temp(l, ir_addr(l->f, l->b,
                                    ir_global_op(singleton_instance(l, t))));
    width = ir_sym_size_of(l->m, ir_scalar(IR_PTR));
    args[0] = address;
    args[1] = ir_sym_operand(l->m, width);
    first = lower_temp(
        l, ir_call(l->f, l->b, IR_PTR,
                   ir_func_op(lower_rt_function(l, "anti_rt_atomic_load",
                                                load_params, 2)),
                   args, 2));
    result = ir_unary(l->f, l->b, IR_COPY, IR_PTR, first);
    build = lower_new_block(l);
    lost = lower_new_block(l);
    done = lower_new_block(l);
    ir_branch(l->f, l->b,
              lower_temp(l, ir_binary(l->f, l->b, IR_EQ, IR_I8, first,
                                      ir_int_op(IR_PTR, 0))),
              build, done);
    l->b = build;
    args[0] = lower_size_operand(l, t);
    made = lower_temp(l, ir_call(l->f, l->b, IR_PTR,
                                 ir_func_op(lower_c_function(l, "malloc",
                                                             IR_PTR,
                                                             IR_I64)),
                                 args, 1));
    ir_store(l->f, l->b, IR_PTR,
             lower_temp(l,
                        ir_addr(l->f, l->b,
                                ir_global_op(lower_class_table(l, t)))),
             made);
    lower_store_interface_tables(l, t, made);
    {
        const struct type *up;
        size_t i;
        for (up = t; up != NULL; up = up->kind == TYPE_CLASS ? up->base
                                                             : NULL) {
            for (i = 0; i < up->field_count; i++) {
                const struct struct_field *field = &up->fields[i];
                if (!lower_has_default(field)) {
                    continue;
                }
                lower_store_default(
                    l, field,
                    lower_offset_address(
                        l, made, lower_field_offset(l, up, &field->name)));
            }
        }
    }
    lower_run_construct(l, t, made);
    ir_assign(l->f, l->b, result, made);
    args[0] = address;
    args[1] = ir_sym_operand(l->m, width);
    args[2] = ir_int_op(IR_PTR, 0);
    args[3] = made;
    ir_branch(l->f, l->b,
              lower_temp(l, ir_call(l->f, l->b, IR_I8,
                                    ir_func_op(lower_rt_function(
                                        l, "anti_rt_atomic_compare_swap",
                                        swap_params, 4)),
                                    args, 4)),
              done, lost);
    l->b = lost;
    ir_call(l->f, l->b, IR_VOID,
            ir_func_op(lower_c_function(l, "free", IR_VOID, IR_PTR)), &made, 1);
    args[0] = address;
    args[1] = ir_sym_operand(l->m, width);
    ir_assign(l->f, l->b, result,
              lower_temp(l, ir_call(l->f, l->b, IR_PTR,
                                    ir_func_op(lower_rt_function(
                                        l, "anti_rt_atomic_load",
                                        load_params, 2)),
                                    args, 2)));
    ir_jump(l->f, l->b, done);
    l->b = done;
    ir_ret(l->f, l->b, IR_PTR, lower_temp(l, result));
}

static void class_init(struct lowerer *l, const struct item *it)
{
    const struct type *t = it->symbol->type;
    const struct type *up;
    struct ir_function *f;
    struct ir_block *entry;
    struct ir_operand self;
    size_t i;

    f = lower_init_function(l, t);
    entry = ir_block_add(f);
    l->f = f;
    l->b = entry;
    self = lower_temp(l, f->params[0].temp);
    ir_store(l->f, l->b, IR_PTR,
             lower_temp(l,
                        ir_addr(l->f, l->b,
                                ir_global_op(lower_class_table(l, t)))),
             self);
    lower_store_interface_tables(l, t, self);
    for (up = t; up != NULL; up = up->kind == TYPE_CLASS ? up->base : NULL) {
        for (i = 0; i < up->field_count; i++) {
            const struct struct_field *field = &up->fields[i];
            if (!lower_has_default(field)) {
                continue;
            }
            lower_store_field_default(l, up, i, self);
        }
    }
    lower_run_construct(l, t, self);
    ir_ret(l->f, l->b, IR_VOID, lower_none());
}

/* DESIGN: an export class whose `construct` takes arguments gives C
   `anti_<Class>_construct(self, args...)`, the counterpart of
   `Class(args)`. It prepares self as the init of the class does, then
   runs `construct` with the arguments and returns what that returns: the
   error of one that may fail, and nothing otherwise. C then needs no
   call of the init first, and cannot forget one. */
static void class_construct(struct lowerer *l, const struct item *it)
{
    static const struct name construct_name = {"construct", 9};
    const struct type *t = it->symbol->type;
    const struct item *m = NULL;
    const struct type *sig;
    struct ir_function *target;
    struct ir_function *f;
    struct ir_operand *args;
    struct ir_operand self;
    uint32_t value;
    struct text name = {0};
    size_t i;

    for (i = 0; i < t->member_count; i++) {
        const struct item *c = t->members[i];
        if (c->kind == ITEM_FN && lower_same_name(&c->name, &construct_name) &&
            c->symbol != NULL && lower_has_body(c) && c->param_count > 0) {
            m = c;
        }
    }
    if (m == NULL) {
        return;
    }
    sig = m->symbol->type;
    target = lower_callee_function(l, m->symbol);
    text_appendf(&name, "anti_%.*s_construct", (int)types_c_name(t).length,
                 types_c_name(t).text);
    f = ir_function_add(l->m, l->module_name, text_cstr(&name),
                        lower_ir_type_of(sig->result), IR_NO_AGG);
    text_free(&name);
    f->exported = true;
    for (i = 0; i < sig->param_count; i++) {
        lower_add_param(l, f, sig->params[i]);
    }
    l->f = f;
    l->b = ir_block_add(f);
    self = lower_temp(l, f->params[0].temp);
    ir_call(l->f, l->b, IR_VOID, ir_func_op(lower_init_function(l, t)), &self,
            1);
    args = ir_alloc(f->param_count, sizeof *args);
    for (i = 0; i < f->param_count; i++) {
        args[i] = lower_temp(l, f->params[i].temp);
    }
    value = ir_call(l->f, l->b, lower_ir_type_of(sig->result),
                    ir_func_op(target),
                    args, f->param_count);
    free(args);
    if (sig->result->kind == TYPE_VOID) {
        ir_ret(l->f, l->b, IR_VOID, lower_none());
    } else {
        ir_ret(l->f, l->b, IR_PTR, lower_temp(l, value));
    }
}

/* Branch to a new block when the class value at p has a table, and give
   the block after it, where both paths meet. A place that `=` fills has a
   zero table when it is an element of `alloc(T, n)` never filled. It
   holds nothing. */
struct ir_block *lower_when_made(struct lowerer *l, struct ir_operand p)
{
    struct ir_block *made = lower_new_block(l);
    struct ir_block *after = lower_new_block(l);
    struct ir_operand table = lower_temp(l, ir_load(l->f, l->b, IR_PTR, p));

    ir_branch(l->f, l->b,
              lower_temp(l, ir_binary(l->f, l->b, IR_NE, IR_I8, table,
                                      ir_int_op(IR_PTR, 0))),
              made, after);
    l->b = made;
    return after;
}

/* A call of the runtime function name with the count arguments of the
   types in params. It gives the result when there is one. */
struct ir_operand lower_rt_call(struct lowerer *l, const char *name,
                                enum ir_type result,
                                const enum ir_type *params,
                                struct ir_operand *args, size_t count)
{
    struct ir_function *f = lower_rt_function_giving(l, name, result, params,
                                                     count);
    uint32_t call = ir_call(l->f, l->b, result, ir_func_op(f), args, count);

    return result == IR_VOID ? lower_none() : lower_temp(l, call);
}

/* The count of elements of the `own` slice whose field is at p. */
struct ir_operand lower_slice_length(struct lowerer *l, struct ir_operand p,
                                     const struct type *slice)
{
    return lower_temp(
        l, ir_load(l->f, l->b, IR_I64,
                   lower_offset_address(
                       l, p, lower_field_offset(l, slice, &lower_len_name))));

}

/* The address of the context word of the function value of type t at
   pair. */
struct ir_operand lower_context_word(struct lowerer *l, const struct type *t,
                                     struct ir_operand pair)
{
    return lower_offset_address(
        l, pair,
        ir_sym_operand(l->m, ir_sym_offset_of(l->m, lower_agg_of(l, t), 1)));
}

/* DESIGN: an `own fn` frees its snapshot with its owner and leaves
   `none` in the context word. A function that captures nothing holds
   `none` there, and the runtime frees nothing for it. The snapshot is
   memory of the C library whatever allocator the owner came from. */
void lower_free_snapshot(struct lowerer *l, const struct type *t,
                         struct ir_operand pair)
{
    static const enum ir_type one[] = {IR_PTR};
    struct ir_operand word = lower_context_word(l, t, pair);
    struct ir_operand snapshot =
        lower_temp(l, ir_load(l->f, l->b, IR_PTR, word));

    lower_rt_call(l, "anti_rt_snapshot_free", IR_VOID, one, &snapshot, 1);
    ir_store(l->f, l->b, IR_PTR, ir_int_op(IR_PTR, 0), word);
}

/* The copy of the `own fn` of type t at from into the pair at into: the
   code, and a copy of the snapshot, byte for byte. */
void lower_dup_snapshot(struct lowerer *l, const struct type *t,
                        struct ir_operand from, struct ir_operand into)
{
    static const enum ir_type one[] = {IR_PTR};
    struct ir_operand snapshot = lower_temp(
        l, ir_load(l->f, l->b, IR_PTR, lower_context_word(l, t, from)));
    struct ir_operand made;

    ir_store(l->f, l->b, IR_PTR,
             lower_temp(l, ir_load(l->f, l->b, IR_PTR, from)), into);
    made = lower_rt_call(l, "anti_rt_snapshot_dup", IR_PTR, one, &snapshot,
                         1);
    ir_store(l->f, l->b, IR_PTR, made, lower_context_word(l, t, into));
}

/* What each_element does to an element. */
enum each { EACH_DESTROY, EACH_COPY, EACH_CLEAR };

/* A loop over the elements of the innermost element type of the array t
   at base, last to first. It tears each down, copies what each owns into
   the element at the same place of into, or clears each. */
static void each_element(struct lowerer *l, const struct type *t,
                         struct ir_operand base, struct ir_operand into,
                         struct ir_operand from, bool made_only,
                         enum each what);

void lower_destroy_owned(struct lowerer *l, const struct type *t,
                         struct ir_operand at, struct ir_operand from,
                         bool made_only)
{
    struct ir_operand args[2];
    size_t i;

    switch (t->kind) {
    case TYPE_CLASS: {
        struct ir_block *after = NULL;
        if (made_only) {
            after = lower_when_made(l, at);
        } else {
            lower_check_table(l, lower_temp(l, ir_load(l->f, l->b, IR_PTR, at)),
                              t);
        }
        args[0] = at;
        args[1] = from;
        ir_call(l->f, l->b, IR_VOID,
                ir_func_op(lower_class_function(l, t, "destroy")), args, 2);
        if (after != NULL) {
            ir_jump(l->f, l->b, after);
            l->b = after;
        }
        return;
    }
    case TYPE_OPTIONAL: {
        struct ir_block *held = lower_new_block(l);
        struct ir_block *after = lower_new_block(l);
        ir_branch(l->f, l->b,
                  lower_temp(l, ir_binary(l->f, l->b, IR_NE, IR_I8,
                                          lower_optional_flag(l, t, at),
                                          ir_int_op(IR_I8, 0))),
                  held, after);
        l->b = held;
        lower_destroy_owned(l, t->element, at, from, made_only);
        ir_jump(l->f, l->b, after);
        l->b = after;
        return;
    }
    case TYPE_ARRAY:
        each_element(l, t, at, lower_none(), from, made_only, EACH_DESTROY);
        return;
    case TYPE_FN:
        lower_free_snapshot(l, t, at);
        return;
    case TYPE_STRUCT:
    case TYPE_TUPLE:
        /* The parts go last to first, as the locals of a block do. */
        for (i = t->field_count; i > 0; i--) {
            const struct struct_field *f = &t->fields[i - 1];
            if ((f->form == FIELD_PLAIN || f->form == FIELD_USE) &&
                sema_needs_teardown(f->type)) {
                lower_destroy_owned(
                    l, f->type,
                    lower_offset_address(l, at,
                                         lower_field_offset(l, t, &f->name)),
                    from, made_only);
            }
        }
        return;
    default:
        return;
    }
}

void lower_clear_owned(struct lowerer *l, const struct type *t,
                       struct ir_operand at)
{
    size_t i;

    switch (t->kind) {
    case TYPE_CLASS:
        ir_store(l->f, l->b, IR_PTR, ir_int_op(IR_PTR, 0), at);
        return;
    case TYPE_OPTIONAL:
        lower_set_optional(l, t, NULL, at, 0);
        return;
    case TYPE_FN:
        if (t->owned) {
            ir_store(l->f, l->b, IR_PTR, ir_int_op(IR_PTR, 0),
                     lower_context_word(l, t, at));
        }
        return;
    case TYPE_ARRAY:
        each_element(l, t, at, lower_none(), lower_none(), false, EACH_CLEAR);
        return;
    case TYPE_STRUCT:
    case TYPE_TUPLE:
        for (i = 0; i < t->field_count; i++) {
            const struct struct_field *f = &t->fields[i];
            if ((f->form == FIELD_PLAIN || f->form == FIELD_USE) &&
                sema_needs_teardown(f->type)) {
                lower_clear_owned(
                    l, f->type,
                    lower_offset_address(l, at,
                                         lower_field_offset(l, t, &f->name)));
            }
        }
        return;
    default:
        return;
    }
}

static struct ir_function *copy_of(struct lowerer *l, const struct type *t);

void lower_copy_owned(struct lowerer *l, const struct type *t,
                      struct ir_operand from, struct ir_operand into)
{
    struct ir_operand args[2];
    size_t i;

    switch (t->kind) {
    case TYPE_CLASS:
        lower_check_table(l, lower_temp(l, ir_load(l->f, l->b, IR_PTR, from)),
                          t);
        args[0] = from;
        args[1] = into;
        ir_call(l->f, l->b, IR_VOID, ir_func_op(copy_of(l, t)), args, 2);
        return;
    case TYPE_OPTIONAL: {
        struct ir_block *held = lower_new_block(l);
        struct ir_block *after = lower_new_block(l);
        ir_branch(l->f, l->b,
                  lower_temp(l, ir_binary(l->f, l->b, IR_NE, IR_I8,
                                          lower_optional_flag(l, t, from),
                                          ir_int_op(IR_I8, 0))),
                  held, after);
        l->b = held;
        lower_copy_owned(l, t->element, from, into);
        ir_jump(l->f, l->b, after);
        l->b = after;
        return;
    }
    case TYPE_ARRAY:
        each_element(l, t, from, into, lower_none(), false, EACH_COPY);
        return;
    case TYPE_FN:
        if (t->owned) {
            lower_dup_snapshot(l, t, from, into);
        }
        return;
    case TYPE_STRUCT:
    case TYPE_TUPLE:
        for (i = 0; i < t->field_count; i++) {
            const struct struct_field *f = &t->fields[i];
            struct ir_operand offset;
            if ((f->form != FIELD_PLAIN && f->form != FIELD_USE) ||
                !lower_copies_parts(f->type)) {
                continue;
            }
            offset = lower_field_offset(l, t, &f->name);
            lower_copy_owned(l, f->type, lower_offset_address(l, from, offset),
                             lower_offset_address(l, into, offset));
        }
        return;
    default:
        return;
    }
}

bool lower_copies_parts(const struct type *t)
{
    size_t i;

    while (t->kind == TYPE_ARRAY || t->kind == TYPE_OPTIONAL) {
        t = t->element;
    }
    if (t->kind == TYPE_CLASS) {
        return true;
    }
    if (t->kind == TYPE_FN) {
        return t->owned;
    }
    if ((t->kind != TYPE_STRUCT && t->kind != TYPE_TUPLE) || t->is_union) {
        return false;
    }
    for (i = 0; i < t->field_count; i++) {
        const struct struct_field *f = &t->fields[i];
        if ((f->form == FIELD_PLAIN || f->form == FIELD_USE) &&
            lower_copies_parts(f->type)) {
            return true;
        }
    }
    return false;
}

static void each_element(struct lowerer *l, const struct type *t,
                         struct ir_operand base, struct ir_operand into,
                         struct ir_operand from, bool made_only,
                         enum each what)
{
    const struct type *element = t;
    struct ir_operand size;
    struct ir_block *test = lower_new_block(l);
    struct ir_block *body = lower_new_block(l);
    struct ir_block *done = lower_new_block(l);
    struct ir_operand offset;
    uint32_t index;

    while (element->kind == TYPE_ARRAY) {
        element = element->element;
    }
    size = lower_size_operand(l, element);
    index = ir_unary(l->f, l->b, IR_COPY, IR_I64, lower_array_count(l, t));
    ir_jump(l->f, l->b, test);
    l->b = test;
    ir_branch(l->f, l->b,
              lower_temp(l, ir_binary(l->f, l->b, IR_SGT, IR_I8,
                                      lower_temp(l, index),
                                      ir_int_op(IR_I64, 0))),
              body, done);
    l->b = body;
    ir_assign(l->f, l->b, index,
              lower_temp(l, ir_binary(l->f, l->b, IR_SUB, IR_I64,
                                      lower_temp(l, index),
                                      ir_int_op(IR_I64, 1))));
    offset = lower_temp(l, ir_binary(l->f, l->b, IR_MUL, IR_I64,
                                     lower_temp(l, index), size));
    if (what == EACH_COPY) {
        struct ir_operand a =
            lower_temp(l, ir_ptradd(l->f, l->b, base, offset));
        struct ir_operand b =
            lower_temp(l, ir_ptradd(l->f, l->b, into, offset));
        lower_copy_owned(l, element, a, b);
    } else if (what == EACH_CLEAR) {
        lower_clear_owned(l, element,
                          lower_temp(l, ir_ptradd(l->f, l->b, base, offset)));
    } else {
        lower_destroy_owned(l, element,
                            lower_temp(l, ir_ptradd(l->f, l->b, base, offset)),
                            from, made_only);
    }
    ir_jump(l->f, l->b, test);
    l->b = done;
}

/* The teardown of the field f of level up, in the object at self. Owned
   memory goes back to the allocator from. */
static void teardown_field(struct lowerer *l, const struct type *up,
                           const struct struct_field *f,
                           struct ir_operand self, struct ir_operand from)
{
    static const enum ir_type two[] = {IR_PTR, IR_PTR};
    static const enum ir_type three[] = {IR_PTR, IR_PTR, IR_PTR};
    static const enum ir_type four[] = {IR_PTR, IR_I64, IR_PTR, IR_PTR};
    const struct type *element = f->type->element;
    struct ir_operand at;
    struct ir_operand v;

    if (f->form != FIELD_PLAIN && f->form != FIELD_USE) {
        return;
    }
    /* A struct, a tuple or an array held inline that owns something is
       torn down part by part with its owner. */
    if (!f->owned &&
        (f->type->kind == TYPE_STRUCT || f->type->kind == TYPE_TUPLE ||
         f->type->kind == TYPE_ARRAY ||
         (f->type->kind == TYPE_OPTIONAL &&
          f->type->element->kind != TYPE_CLASS)) &&
        sema_needs_teardown(f->type)) {
        lower_destroy_owned(l, f->type,
                            lower_offset_address(
                                l, self, lower_field_offset(l, up, &f->name)),
                            from, false);
        return;
    }
    /* A `?T` of a class value runs the teardown of the object it holds
       when its flag is set. */
    if (lower_optional_needs_destruct(f->type)) {
        struct ir_block *held = lower_new_block(l);
        struct ir_block *after = lower_new_block(l);
        struct ir_operand args[2];
        at = lower_offset_address(l, self,
                                  lower_field_offset(l, up, &f->name));
        ir_branch(l->f, l->b,
                  lower_temp(l, ir_binary(l->f, l->b, IR_NE, IR_I8,
                                          lower_optional_flag(l, f->type, at),
                                          ir_int_op(IR_I8, 0))),
                  held, after);
        l->b = held;
        args[0] = at;
        args[1] = from;
        lower_check_table(l, lower_temp(l, ir_load(l->f, l->b, IR_PTR, at)),
                          f->type->element);
        ir_call(l->f, l->b, IR_VOID,
                ir_func_op(lower_class_function(l, f->type->element,
                                                "destroy")),
                args, 2);
        ir_jump(l->f, l->b, after);
        l->b = after;
        return;
    }
    if (!f->owned && !(f->type->kind == TYPE_CLASS &&
                       lower_type_needs_destruct(f->type))) {
        return;
    }
    at = lower_offset_address(l, self, lower_field_offset(l, up, &f->name));
    if (f->type->kind == TYPE_FN) {
        lower_free_snapshot(l, f->type, at);
        return;
    }
    if (!f->owned) {
        struct ir_operand args[2];
        args[0] = at;
        args[1] = from;
        lower_check_table(l, lower_temp(l, ir_load(l->f, l->b, IR_PTR, at)),
                          f->type);
        ir_call(l->f, l->b, IR_VOID,
                ir_func_op(lower_class_function(l, f->type, "destroy")), args,
                2);
        return;
    }
    v = lower_temp(l, ir_load(l->f, l->b, IR_PTR, at));
    if (f->type->kind == TYPE_POINTER && element->kind == TYPE_CLASS) {
        struct ir_operand args[3];
        args[0] = v;
        args[1] = lower_static_descriptor(l, element);
        args[2] = from;
        lower_rt_call(l, "anti_rt_delete_from", IR_VOID, three, args, 3);
    } else {
        struct ir_operand args[4];
        if (f->type->kind == TYPE_SLICE && element->kind == TYPE_CLASS) {
            args[0] = v;
            args[1] = lower_slice_length(l, at, f->type);
            args[2] = lower_static_descriptor(l, element);
            args[3] = from;
            lower_rt_call(l, "anti_rt_destroy_elements", IR_VOID, four, args,
                          4);
        }
        args[0] = from;
        args[1] = v;
        lower_rt_call(l, "anti_rt_give", IR_VOID, two, args, 2);
    }
    ir_store(l->f, l->b, IR_PTR, ir_int_op(IR_PTR, 0), at);
}

/* DESIGN: the teardown runs the destruct body of every level, concrete
   first. It then destroys what each level owns, so a body still reads
   what it owns. An object behind an `own` pointer is deleted, and each
   element of an `own` slice of class values is destroyed before the
   buffer is freed. A class value held inline runs its own teardown. Its
   table is never zero in an object a literal or `construct` made, so a
   zero one traps, as every zero table does. */
static void class_teardown(struct lowerer *l, const struct type *t)
{
    static const struct name destruct_name = {"destruct", 8};
    struct ir_function *f = lower_class_function(l, t, "destroy");
    const struct type *up;
    struct ir_operand self;
    struct ir_operand from;
    size_t i;

    l->f = f;
    l->b = ir_block_add(f);
    self = lower_temp(l, f->params[0].temp);
    from = lower_temp(l, f->params[1].temp);
    lower_hook_object(l, HOOK_DESTROYED, self);
    for (up = t; up->base != NULL; up = up->base) {
        const struct item *m = lower_level_fn(up, &destruct_name);
        if (m != NULL) {
            struct ir_operand arg = self;
            ir_call(l->f, l->b, IR_VOID,
                    ir_func_op(lower_callee_function(l, m->symbol)), &arg, 1);
        }
    }
    for (up = t; up->base != NULL; up = up->base) {
        for (i = 0; i < up->field_count; i++) {
            teardown_field(l, up, &up->fields[i], self, from);
        }
    }
    ir_ret(l->f, l->b, IR_VOID, lower_none());
}

/* The copy of the class value t, the one its chain declares or the one
   the compiler writes. */
static struct ir_function *copy_of(struct lowerer *l, const struct type *t)
{
    const struct item *m = lower_declared_copy(t);

    return m != NULL ? lower_callee_function(l, m->symbol)
                     : lower_class_function(l, t, "copy");
}

/* The copy of the field f of level up, from the object at self into the
   one at to, whose bytes are already the same. */
static void copy_field(struct lowerer *l, const struct type *up,
                       const struct struct_field *f, struct ir_operand self,
                       struct ir_operand to)
{
    static const enum ir_type two[] = {IR_PTR, IR_I64};
    static const enum ir_type four[] = {IR_PTR, IR_PTR, IR_I64, IR_PTR};
    const struct type *element = f->type->element;
    struct ir_operand offset;
    struct ir_operand from;
    struct ir_operand into;
    struct ir_operand v;
    struct ir_operand made;
    struct ir_operand args[4];

    if (f->form != FIELD_PLAIN && f->form != FIELD_USE) {
        return;
    }
    /* A lock of the object is its own, and the copy gets one that is
       free, whatever the lock of the original held. */
    if (types_is_mutex(f->type) || types_is_object_lock(f->type)) {
        offset = lower_field_offset(l, up, &f->name);
        lower_zero_lock(l, f->type, lower_offset_address(l, to, offset));
        return;
    }
    /* A `transient` field is derived state, and the copy derives its
       own. */
    if (f->transient) {
        offset = lower_field_offset(l, up, &f->name);
        into = lower_offset_address(l, to, offset);
        ir_store(l->f, l->b, IR_PTR, ir_int_op(IR_PTR, 0), into);
        return;
    }
    /* A struct, a tuple or an array held inline copies each part that
       a copy reaches, as `dup` of it does. */
    if (!f->owned &&
        (f->type->kind == TYPE_STRUCT || f->type->kind == TYPE_TUPLE ||
         f->type->kind == TYPE_ARRAY ||
         (f->type->kind == TYPE_OPTIONAL &&
          f->type->element->kind != TYPE_CLASS)) &&
        lower_copies_parts(f->type)) {
        offset = lower_field_offset(l, up, &f->name);
        lower_copy_owned(l, f->type, lower_offset_address(l, self, offset),
                         lower_offset_address(l, to, offset));
        return;
    }
    /* A `?T` of a class value copies the object it holds with its own
       copy when its flag is set. */
    if (f->type->kind == TYPE_OPTIONAL &&
        f->type->element->kind == TYPE_CLASS) {
        struct ir_block *held = lower_new_block(l);
        struct ir_block *after = lower_new_block(l);
        offset = lower_field_offset(l, up, &f->name);
        from = lower_offset_address(l, self, offset);
        into = lower_offset_address(l, to, offset);
        ir_branch(l->f, l->b,
                  lower_temp(l, ir_binary(l->f, l->b, IR_NE, IR_I8,
                                          lower_optional_flag(l, f->type,
                                                              from),
                                          ir_int_op(IR_I8, 0))),
                  held, after);
        l->b = held;
        args[0] = from;
        args[1] = into;
        ir_call(l->f, l->b, IR_VOID,
                ir_func_op(copy_of(l, f->type->element)), args, 2);
        ir_jump(l->f, l->b, after);
        l->b = after;
        return;
    }
    if (!f->owned && f->type->kind != TYPE_CLASS) {
        return;
    }
    offset = lower_field_offset(l, up, &f->name);
    from = lower_offset_address(l, self, offset);
    into = lower_offset_address(l, to, offset);
    if (f->type->kind == TYPE_FN) {
        lower_dup_snapshot(l, f->type, from, into);
        return;
    }
    if (!f->owned) {
        lower_check_table(l, lower_temp(l, ir_load(l->f, l->b, IR_PTR, from)),
                          f->type);
        args[0] = from;
        args[1] = into;
        ir_call(l->f, l->b, IR_VOID, ir_func_op(copy_of(l, f->type)), args,
                2);
        return;
    }
    v = lower_temp(l, ir_load(l->f, l->b, IR_PTR, from));
    if (f->type->kind == TYPE_POINTER && element->kind == TYPE_CLASS) {
        made = lower_object_call(l, "anti_rt_dup", v, element);
    } else {
        struct ir_operand count =
            f->type->kind == TYPE_SLICE ? lower_slice_length(l, from, f->type)
                                        : ir_int_op(IR_I64, 1);
        args[0] = v;
        args[1] = lower_temp(l, ir_binary(l->f, l->b, IR_MUL, IR_I64, count,
                                          lower_size_operand(l, element)));
        made = lower_rt_call(l, "anti_rt_copy_buffer", IR_PTR, two, args, 2);
        if (f->type->kind == TYPE_SLICE && element->kind == TYPE_CLASS) {
            args[0] = v;
            args[1] = made;
            args[2] = count;
            args[3] = lower_static_descriptor(l, element);
            lower_rt_call(l, "anti_rt_copy_elements", IR_VOID, four, args, 4);
        }
    }
    ir_store(l->f, l->b, IR_PTR, made, into);
}

/* DESIGN: the copy starts from every byte of the object. It then gives
   each `own` field fresh memory with a copy of its contents. The object
   behind a pointer is copied, and so is each element of a slice of class
   values. A class value held inline copies itself with its own copy.
   Other pointers keep the address. */
static void class_copy(struct lowerer *l, const struct type *t)
{
    struct ir_function *f = lower_class_function(l, t, "copy");
    const struct type *up;
    struct ir_operand self;
    struct ir_operand to;
    size_t i;

    l->f = f;
    l->b = ir_block_add(f);
    self = lower_temp(l, f->params[0].temp);
    to = lower_temp(l, f->params[1].temp);
    ir_memcopy(l->f, l->b, to, self, lower_vtype_of(l, t));
    for (up = t; up->base != NULL; up = up->base) {
        for (i = 0; i < up->field_count; i++) {
            copy_field(l, up, &up->fields[i], self, to);
        }
    }
    ir_ret(l->f, l->b, IR_VOID, lower_none());
}

/* Whether the class declares a `construct` that takes arguments. */
static bool constructs_with_arguments(const struct type *t)
{
    static const struct name construct_name = {"construct", 9};
    size_t i;

    for (i = 0; i < t->member_count; i++) {
        const struct item *m = t->members[i];
        if (m->kind == ITEM_FN && lower_same_name(&m->name, &construct_name) &&
            m->symbol != NULL && m->symbol->type->kind == TYPE_FN &&
            m->symbol->type->param_count > 1) {
            return true;
        }
    }
    return false;
}

/* Whether a field of the chain of t holds a class value inline that no
   default fills. Only a literal that names it then makes the class. */
static bool requires_class_field(const struct type *t)
{
    size_t i;

    for (; t != NULL && t->kind == TYPE_CLASS; t = t->base) {
        for (i = 0; i < t->field_count; i++) {
            const struct struct_field *f = &t->fields[i];
            if ((f->form == FIELD_PLAIN || f->form == FIELD_USE) &&
                f->type->kind == TYPE_CLASS && !lower_has_default(f)) {
                return true;
            }
        }
    }
    return false;
}

/* The record of a class that the module being lowered declares, for the
   passes over the whole program. */
static void class_record(struct lowerer *l, const struct module *module,
                         const struct item *it)
{
    const struct type *t = it->symbol->type;
    struct text symbol = {0};
    char *module_path;
    char *name;
    struct ir_class *c;
    const struct type *up;
    size_t k;

    type_symbol_name(&symbol, t);
    name = lower_copy_text(&symbol);
    text_free(&symbol);
    module_path = lower_cstr(&t->module);
    c = ir_class_add(l->m, module_path, name);
    free(name);
    c->flags = (it->is_abstract ? IR_CLASS_ABSTRACT : 0u) |
               (t->is_final ? IR_CLASS_FINAL : 0u) |
               (it->is_singleton ? IR_CLASS_SINGLETON : 0u) |
               (constructs_with_arguments(t) ? IR_CLASS_ARGS : 0u) |
               (requires_class_field(t) ? IR_CLASS_REQUIRED : 0u);
    c->descriptor = lower_class_descriptor(l, t)->index;
    c->base = lower_class_descriptor(l, t->base)->index;
    c->agg = lower_agg_of(l, t);
    if (!it->is_abstract) {
        struct text init = {0};
        const struct ir_function *f;
        lower_init_name(t, it->exported, &init);
        f = lower_find_function(l->m, module_path, text_cstr(&init));
        text_free(&init);
        c->init = f != NULL ? f->index : IR_NO_INDEX;
        c->table = lower_class_table(l, t)->index;
        for (up = t; up != NULL;
             up = up->kind == TYPE_CLASS ? up->base : NULL) {
            for (k = 0; k < up->field_count; k++) {
                if (up->fields[k].form == FIELD_IMPL) {
                    uint32_t interface =
                        lower_class_descriptor(l, up->fields[k].type)->index;
                    uint32_t table =
                        lower_interface_table(l, t, &up->fields[k])->index;
                    ir_class_subtable(c, interface, table, lower_agg_of(l, up),
                                      (uint32_t)k);
                }
            }
        }
    }
    for (k = 0; k < t->field_count; k++) {
        if (t->fields[k].writable) {
            ir_class_mutable(c, (uint32_t)k);
        }
        /* DESIGN: the record carries the `inject` fields the class
           declares, never the ones it inherits. The record of the class
           above carries those. The pass over the whole program then
           reads one entry per declaration and names the class that
           needs a provider. */
        if (t->fields[k].injected) {
            const struct type *i = t->fields[k].type->element;
            struct text path = {0};
            char *field = lower_cstr(&t->fields[k].name);
            text_appendf(&path, "%.*s.%.*s", (int)i->module.length,
                         i->module.text, (int)i->name.length, i->name.text);
            ir_class_inject(l->m, c, text_cstr(&path), field,
                            lower_class_descriptor(l, i)->index,
                            t->fields[k].inject_final);
            free(field);
            text_free(&path);
        }
    }
    /* DESIGN: a `provides` line stands at module level and names a class
       of the module, so its record carries the interfaces the library
       offers for it. The pass over the whole program then writes one
       table, and nothing of the line reaches a function body. */
    for (k = 0; k < module->provides_count; k++) {
        const struct provides *pr = &module->provides[k];
        struct text path = {0};
        if (pr->class_type != t || pr->type == NULL) {
            continue;
        }
        text_appendf(&path, "%.*s.%.*s", (int)pr->type->module.length,
                     pr->type->module.text, (int)pr->type->name.length,
                     pr->type->name.text);
        ir_class_provides(l->m, c, text_cstr(&path),
                          lower_class_descriptor(l, pr->type)->index);
        text_free(&path);
    }
    free(module_path);
}

/* Whether it is a copy of a generic of another module whose data a
   library file of the program holds already. Its descriptor tells. */
static bool known_copy(struct lowerer *l, const struct item *it)
{
    const struct type *t;
    struct text name = {0};
    const struct ir_global *g;

    if (it->home_module == NULL || it->symbol == NULL ||
        it->symbol->type == NULL ||
        (it->kind != ITEM_CLASS && it->kind != ITEM_STRUCT)) {
        return false;
    }
    t = it->symbol->type;
    type_symbol_name(&name, t);
    text_append(&name, ".descriptor");
    g = lower_find_global(l->m, it->home_module, text_cstr(&name));
    text_free(&name);
    return g != NULL && g->index < l->first_global && !g->is_extern;
}

bool lower_module(struct module *module, const char *module_name,
                  struct ir_module *out, struct diagnostics *diags,
                  unsigned options, const char *const *patterns,
                  size_t pattern_count, const char *version)
{
    struct lowerer l;
    /* Every function of the module sits past the ones the library files
       brought, so one pass at the end gives them their source. */
    size_t first = out->function_count;
    size_t done = 0;
    size_t i;

    /* Semantic analysis rejects every module that lowering cannot
       translate, so no construct reports an error here. */
    (void)diags;
    memset(&l, 0, sizeof l);
    l.m = out;
    l.module_name = module_name;
    l.first_function = out->function_count;
    l.first_global = out->global_count;
    l.file = module->file != NULL ? module->file : module_name;
    l.file_index = ir_file_add(out, l.file);
    l.no_reflect = (options & LOWER_NO_REFLECT) != 0;
    l.dev = (options & LOWER_DEV) != 0;
    l.hooks = (options & LOWER_NO_HOOKS) == 0;
    l.trace_marked = l.hooks && (options & LOWER_TRACE) != 0;
    l.trace_writes = l.hooks && (options & LOWER_TRACE_WRITES) != 0;
    l.patterns = patterns;
    l.pattern_count = patterns != NULL ? pattern_count : 0;
    l.version = version != NULL ? version : PACKAGE_VERSION_DEFAULT;
    /* A function of a struct body is a function of the module with one
       more segment in its name. It is declared and lowered like a free
       function. */
    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        size_t j;
        if (it->kind == ITEM_FN || it->kind == ITEM_EXTERN_FN) {
            declare_function(&l, it);
        }
        for (j = 0; j < it->member_count; j++) {
            if (it->members[j]->kind == ITEM_FN &&
                (it->members[j]->body != NULL ||
                 it->members[j]->singleton_get)) {
                declare_function(&l, it->members[j]);
            }
        }
    }
    /* DESIGN: the table and the descriptor of a class are data of the
       module that declares it. Every class builds them, whether or not a
       literal here constructs one. An abstract class has no complete
       value and therefore no table. */
    for (i = 0; i < module->item_count; i++) {
        const struct item *it = module->items[i];
        if (known_copy(&l, it)) {
            continue;
        }
        if (it->kind == ITEM_CLASS && !it->is_abstract &&
            it->symbol != NULL && it->symbol->type != NULL) {
            const struct type *t = it->symbol->type;
            const struct type *up;
            size_t k;
            lower_class_table(&l, t);
            for (up = t; up != NULL;
                 up = up->kind == TYPE_CLASS ? up->base : NULL) {
                for (k = 0; k < up->field_count; k++) {
                    if (up->fields[k].form == FIELD_IMPL) {
                        lower_interface_table(&l, t, &up->fields[k]);
                    }
                }
            }
            if (it->exported || !it->is_singleton) {
                class_init(&l, it);
            }
            if (it->exported) {
                class_construct(&l, it);
            }
            class_teardown(&l, t);
            if (lower_declared_copy(t) == NULL) {
                class_copy(&l, t);
            }
        }
    }
    for (i = 0; i < module->item_count; i++) {
        const struct item *it = module->items[i];
        if (known_copy(&l, it)) {
            continue;
        }
        if (it->kind == ITEM_CLASS && it->symbol != NULL &&
            it->symbol->type != NULL) {
            class_record(&l, module, it);
        }
        if (it->kind == ITEM_STRUCT && it->symbol != NULL &&
            it->symbol->type != NULL) {
            lower_struct_descriptor(&l, it->symbol->type);
        }
    }
    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        size_t j;
        if (it->kind == ITEM_FN) {
            lower_function(&l, it);
        }
        for (j = 0; j < it->member_count; j++) {
            if (it->members[j]->singleton_get) {
                lower_singleton_get(&l, it->members[j]);
            } else if (it->members[j]->kind == ITEM_FN &&
                it->members[j]->body != NULL) {
                lower_function(&l, it->members[j]);
            }
        }
        /* The anonymous functions each body met, and the ones those
           met in turn. */
        while (done < l.anonymous_count) {
            lower_function(&l, l.anonymous[done++]);
        }
    }
    free(l.anonymous);
    patterns_start(&l);
    free(l.regex_literals);
    /* A function or a datum defined here under the path of another
       module is a copy of a generic of that module. The object of the
       module being lowered holds it. */
    for (i = first; i < out->function_count; i++) {
        struct ir_function *f = out->functions[i];
        if (!f->is_extern && f->module != NULL &&
            strcmp(f->module, module_name) != 0) {
            f->unit = module_name;
        }
        if (!f->is_extern && f->module != NULL && f->file == IR_NO_INDEX &&
            ir_in_unit(f->module, f->unit, module_name)) {
            f->file = l.file_index;
        }
    }
    for (i = l.first_global; i < out->global_count; i++) {
        struct ir_global *g = out->globals[i];
        if (!g->is_extern && g->module != NULL &&
            strcmp(g->module, module_name) != 0 &&
            strcmp(g->module, RUNTIME_MODULE) != 0) {
            g->unit = module_name;
        }
    }
    return true;
}

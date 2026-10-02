#include "lower.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../rt/f16.h"
#include "alloc.h"
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
    return types_has_fields(t) || t->kind == TYPE_ARRAY ||
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
        types_name_qualified(&name, t);
    } else {
        types_name(&name, t);
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
    char *copy = alloc_zeroed(sizeof name, 1);

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
    size_t count = types_has_fields(t) ? t->field_count : 2;
    size_t i;

    if (agg != IR_NO_AGG) {
        free(name);
        return agg;
    }
    fields = alloc_zeroed(count + 1, sizeof *fields);
    if (t->kind == TYPE_ARRAY) {
        struct text text = {0};
        struct ir_vtype element = lower_vtype_of(l, t->element);
        uint32_t length = t->length_of != NULL
                              ? lower_sym_of(l, t->length_of)
                              : ir_sym_int(l->m, IR_I64, t->length);
        if (t->length_of != NULL) {
            types_symbolic_print(&text, t->length_of, false);
        } else {
            text_appendf(&text, "%llu", (unsigned long long)t->length);
        }
        agg = ir_array_add(l->m, name, element, length, text_cstr(&text));
        text_free(&text);
    } else if (types_has_fields(t)) {
        for (i = 0; i < count; i++) {
            char *field = alloc_zeroed(t->fields[i].name.length + 1, 1);
            memcpy(field, t->fields[i].name.text, t->fields[i].name.length);
            field[t->fields[i].name.length] = '\0';
            fields[i].name = field;
            fields[i].type = lower_vtype_of(l, t->fields[i].type);
            fields[i].bits = t->fields[i].bits;
            fields[i].ext = t->fields[i].bits == 0 ? IR_EXT_NONE
                            : types_is_signed(t->fields[i].type) ? IR_EXT_SIGN
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
    char *s = alloc_zeroed(name->length + 1, 1);

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
    char *copy = alloc_zeroed(t->length + 1, 1);

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
    return types_is_signed(t) ? IR_EXT_SIGN : IR_EXT_ZERO;
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
        if (types_find_field(s, name) != NULL) {
            return s;
        }
    }
    return t;
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
    uint32_t index;

    if (types_has_fields(s)) {
        index = (uint32_t)(types_find_field(s, name) - s->fields);
    } else {
        index = lower_name_is(name, "len") || lower_name_is(name, "entry")
                    ? 1
                    : 0;
    }

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
size_t lower_globals_of_module(struct lowerer *l)
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
    snprintf(name, sizeof name, "%zu", lower_globals_of_module(l));
    return ir_global_add(m, l->module_name, name, bytes, text->length + 1, 1);
}

struct ir_operand lower_literal_address(struct lowerer *l,
                                        const struct token_text *text)
{
    return lower_temp(l,
                      ir_addr(l->f, l->b,
                              ir_global_op(lower_literal_global(l, text))));
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

/* The extern declaration of the runtime function f, with the signature
   its row of RT_FUNCTIONS gives. A module that calls it twice shares the
   declaration. */
struct ir_function *lower_rt_declare(struct lowerer *l, enum rt_function f)
{
    const struct rt_signature *s = rt_signature(f);
    struct ir_function *fn = lower_find_function(l->m, NULL, s->name);
    size_t i;

    if (fn == NULL) {
        fn = ir_extern_add(l->m, s->name, s->types[0], false);
        for (i = 0; i < s->param_count; i++) {
            ir_param_add(fn, s->types[1 + i], IR_NO_AGG);
        }
    }
    return fn;
}

/* A call of anti_rt_delete, anti_rt_destroy or anti_rt_dup on object,
   which the program holds as a t. Only dup gives a value. */
struct ir_operand lower_object_call(struct lowerer *l, enum rt_function f,
                                    struct ir_operand object,
                                    const struct type *t)
{
    struct ir_operand args[2];

    args[0] = object;
    args[1] = lower_static_descriptor(l, t);
    return lower_rt_call(l, f, args);
}

/* A call of the runtime function f on the arguments its row counts. It
   gives the result when there is one. */
struct ir_operand lower_rt_call(struct lowerer *l, enum rt_function f,
                                const struct ir_operand *args)
{
    const struct rt_signature *s = rt_signature(f);
    struct ir_function *fn = lower_rt_declare(l, f);
    uint32_t call = ir_call(l->f, l->b, s->types[0], ir_func_op(fn), args,
                            s->param_count);

    return s->types[0] == IR_VOID ? lower_none() : lower_temp(l, call);
}

/* The memory of one object of `alloc T { }`, `alloc T(args)` or a
   singleton, size bytes of malloc. Out of memory is fatal for them, so a
   NULL from malloc ends the program through anti_rt_out_of_memory, and
   the code after it writes through the result. */
struct ir_operand lower_new_memory(struct lowerer *l, struct ir_operand size)
{
    struct ir_function *malloc_fn =
        lower_c_function(l, "malloc", IR_PTR, IR_I64);
    struct ir_operand made =
        lower_temp(l, ir_call(l->f, l->b, IR_PTR, ir_func_op(malloc_fn),
                              &size, 1));
    struct ir_operand none = lower_temp(
        l, ir_binary(l->f, l->b, IR_EQ, IR_I8, made, ir_int_op(IR_PTR, 0)));
    struct ir_block *lost = lower_new_block(l);
    struct ir_block *rest = lower_new_block(l);

    ir_branch(l->f, l->b, none, lost, rest);
    l->b = lost;
    lower_rt_call(l, RT_FN_OUT_OF_MEMORY, &size);
    ir_jump(l->f, l->b, rest);
    l->b = rest;
    return made;
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
    types_symbol_name(&name, t);
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
                lower_class_init(&l, it);
            }
            if (it->exported) {
                lower_class_construct(&l, it);
            }
            lower_class_teardown(&l, t);
            if (lower_declared_copy(t) == NULL) {
                lower_class_copy(&l, t);
            }
            if (it->default_eq != NULL) {
                lower_class_equals(&l, it);
            }
            if (it->default_hash != NULL) {
                lower_class_hash(&l, it);
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
            lower_class_record(&l, module, it);
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
    free(l.temps);
    lower_patterns_start(&l);
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

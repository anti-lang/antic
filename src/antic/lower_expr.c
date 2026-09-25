#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sema.h"
#include "target.h"
#include "text.h"
#include "types.h"
#include "lower_lowerer.h"

/* Whether e is a T that the checker made the `?T` holding it. In a copy
   of a generic that `?T` may be a `?*U`, which holds the `*U` as it is. */
static bool wraps(const struct expr *e)
{
    return e->to_optional != NULL && e->to_optional->kind == TYPE_OPTIONAL;
}

/* Put the value of e, of type t, at address. A T that becomes its `?T`
   is the `?T` there. */
void lower_store_value(struct lowerer *l, const struct type *t,
                       const struct expr *e, struct ir_operand address)
{
    struct ir_operand v;

    if (wraps(e)) {
        lower_build_into(l, e, address);
        return;
    }

    /* A named function as the default of an `own fn` field is checked
       before the function has its type, so the checker marks no
       conversion. The value is its code, with no snapshot. */
    if (lower_is_context(t) && !e->to_context && !lower_is_context(e->type)) {
        ir_store(l->f, l->b, IR_PTR, lower_expr(l, e), address);
        ir_store(l->f, l->b, IR_PTR, ir_int_op(IR_PTR, 0),
                 lower_context_word(l, t, address));
        return;
    }
    if (lower_is_aggregate(t)) {
        lower_build_into(l, e, address);
        return;
    }
    v = lower_expr(l, e);
    ir_store(l->f, l->b, lower_ir_type_of(t), v, address);
}

/* DESIGN: the slot of an injectable interface is one pointer per
   interface, of the runtime module and named INJECT_SLOT_PREFIX and
   the path of the interface. It holds the provider the build chose.
   The pass over the whole program writes it. One program then holds
   one slot per interface whichever module injects it, and the run-time
   configuration has one pointer to replace. Every module that injects
   refers to it. */
static struct ir_global *inject_slot(struct lowerer *l, const struct type *t)
{
    struct ir_module *m = l->m;
    struct text name = {0};
    struct ir_global *g;

    text_appendf(&name, INJECT_SLOT_PREFIX "%.*s.%.*s",
                 (int)t->module.length, t->module.text,
                 (int)t->name.length, t->name.text);
    g = lower_find_global(m, RUNTIME_MODULE, text_cstr(&name));
    if (g == NULL) {
        g = ir_global_add(m, RUNTIME_MODULE, text_cstr(&name), NULL, 0, 1);
        g->is_extern = true;
        g->mutable = true;
    }
    text_free(&name);
    return g;
}

/* Whether field has a default, declared here or read from a library
   file, or takes `T { }` as an inline class value. An `inject` field
   has none and is written all the same, by the provider of its
   interface. */
bool lower_has_default(const struct struct_field *field)
{
    return field->injected || field->value != NULL ||
           field->constant != NULL || sema_field_takes_literal(field);
}

/* DESIGN: every complete class has a function that prepares an object
   allocated elsewhere. It stores the table pointers and writes the
   default of every field of the chain, as a literal of the class does.
   Then it runs construct when that takes no arguments. An export
   class gives it to C as `anti_<Class>_init`. The registry that
   `reflect.new` reads names it for the others as `<Class>.init`. An
   abstract class has no complete value and therefore none. */
void lower_init_name(const struct type *t, bool exported, struct text *out)
{
    if (exported) {
        text_appendf(out, "anti_%.*s_init", (int)types_c_name(t).length,
                     types_c_name(t).text);
        return;
    }
    type_symbol_name(out, t);
    text_append(out, ".init");
}

/* The init function of class t, declared in the module that declares t
   and referred to from any other. */
struct ir_function *lower_init_function(struct lowerer *l,
                                        const struct type *t)
{
    char *module = lower_cstr(&t->module);
    struct ir_function *f;
    struct text name = {0};

    lower_init_name(t, t->item_exported, &name);
    f = lower_find_function(l->m, module, text_cstr(&name));
    if (f == NULL) {
        f = lower_defines(l, t, module)
                ? ir_function_add(l->m, module, text_cstr(&name), IR_VOID,
                                  IR_NO_AGG)
                : ir_declare_add(l->m, module, text_cstr(&name), IR_VOID,
                                 IR_NO_AGG);
        f->exported = t->item_exported;
        ir_param_add(f, IR_PTR, IR_NO_AGG);
    }
    text_free(&name);
    free(module);
    return f;
}

/* The scalar default of field. The declaring module lowers the
   expression, and any other module the value its library file carries. */
static struct ir_operand default_scalar(struct lowerer *l,
                                        const struct struct_field *field)
{
    if (field->value != NULL) {
        return lower_expr(l, field->value);
    }
    return lower_constant(l, field->constant, lower_ir_type_of(field->type));
}

/* Put the default of field at address. */
void lower_store_default(struct lowerer *l, const struct struct_field *field,
                         struct ir_operand address)
{
    struct ir_operand v;
    struct ir_vtype vtype;

    /* DESIGN: an `inject` field is filled by a call through the slot of
       its interface, before `construct` runs. The call is indirect, so
       a replacement of the slot reaches every site. */
    if (field->injected) {
        struct ir_operand slot =
            lower_temp(l, ir_addr(l->f, l->b, ir_global_op(inject_slot(
                                      l, field->type->element))));
        struct ir_operand provider =
            lower_temp(l, ir_load(l->f, l->b, IR_PTR, slot));
        struct ir_operand object =
            lower_temp(l, ir_call_indirect(l->f, l->b, IR_PTR, provider,
                                           lower_provider_signature(l), NULL,
                                           0));
        ir_store(l->f, l->b, IR_PTR, object, address);
        return;
    }
    /* A Mutex, and the hidden lock of a synchronized class, start free. */
    if (types_is_mutex(field->type) || types_is_object_lock(field->type)) {
        lower_zero_lock(l, field->type, address);
        return;
    }
    /* An inline class field without a default is written as `T { }`
       would write it, which the init of T does. */
    if (field->value == NULL && field->constant == NULL) {
        ir_call(l->f, l->b, IR_VOID,
                ir_func_op(lower_init_function(l, field->type)),
                &address, 1);
        return;
    }
    if (field->value != NULL) {
        lower_store_value(l, field->type, field->value, address);
        return;
    }
    /* A named function as the default of an `own fn` field is its code
       with no snapshot. */
    if (field->type->kind == TYPE_FN && field->type->context) {
        ir_store(l->f, l->b, IR_PTR,
                 lower_constant(l, field->constant, IR_PTR), address);
        ir_store(l->f, l->b, IR_PTR, ir_int_op(IR_PTR, 0),
                 lower_context_word(l, field->type, address));
        return;
    }
    if (lower_is_aggregate(field->type)) {
        v = lower_const_address(l, field->constant, field->type);
        vtype = lower_vtype_of(l, field->type);
        ir_memcopy(l->f, l->b, address, v, vtype);
        return;
    }
    v = default_scalar(l, field);
    ir_store(l->f, l->b, lower_ir_type_of(field->type), v, address);
}

/* Put the default of field i of owner into the object at object. A
   bitfield goes into its unit and every other field to its offset. The
   object's address is the address of every class of its chain. */
void lower_store_field_default(struct lowerer *l, const struct type *owner,
                               size_t i, struct ir_operand object)
{
    const struct struct_field *field = &owner->fields[i];

    if (field->bits != 0) {
        struct ir_operand v = default_scalar(l, field);
        ir_bitstore(l->f, l->b, lower_ir_type_of(field->type), v, object,
                    lower_agg_of(l, owner), (uint32_t)i);
        return;
    }
    lower_store_default(l, field,
                        lower_offset_address(l, object,
                                             lower_field_offset(l, owner,
                                                                &field->name)));
}

/* The repeat form of an array literal computes its value once, then
   fills every element in a loop. */
static void fill_array(struct lowerer *l, const struct expr *e,
                       struct ir_operand dest)
{
    const struct type *element = e->type->element;
    struct ir_operand size = lower_size_operand(l, element);
    struct ir_operand length = e->type->length_of != NULL
                                   ? ir_sym_operand(
                                         l->m,
                                         lower_sym_of(l, e->type->length_of))
                                   : ir_int_op(IR_I64, e->type->length);
    struct ir_operand v = lower_none();
    struct ir_block *test;
    struct ir_block *body;
    struct ir_block *done;
    uint32_t index;
    uint32_t more;
    struct ir_operand at;

    if (lower_is_aggregate(element)) {
        lower_build_into(l, e->as.array_repeat.value, dest);
    } else {
        v = lower_expr(l, e->as.array_repeat.value);
    }
    index = ir_unary(l->f, l->b, IR_COPY, IR_I64,
                     ir_int_op(IR_I64, lower_is_aggregate(element) ? 1 : 0));
    test = lower_new_block(l);
    body = lower_new_block(l);
    done = lower_new_block(l);
    ir_jump(l->f, l->b, test);
    l->b = test;
    more = ir_binary(l->f, l->b, IR_SLT, IR_I8, lower_temp(l, index), length);
    ir_branch(l->f, l->b, lower_temp(l, more), body, done);
    l->b = body;
    at = lower_temp(l, ir_ptradd(l->f, l->b, dest,
                                 lower_temp(l, ir_binary(l->f, l->b, IR_MUL,
                                                         IR_I64,
                                                         lower_temp(l, index),
                                                         size))));
    if (lower_is_aggregate(element)) {
        ir_memcopy(l->f, l->b, at, dest, lower_vtype_of(l, element));
    } else {
        ir_store(l->f, l->b, lower_ir_type_of(element), v, at);
    }
    ir_assign(l->f, l->b, index,
              lower_temp(l, ir_binary(l->f, l->b, IR_ADD, IR_I64,
                                      lower_temp(l, index),
                                      ir_int_op(IR_I64, 1))));
    ir_jump(l->f, l->b, test);
    l->b = done;
}

/* a[lo..hi] points lo elements after element 0 of a and holds hi - lo
   elements. */
static void build_slice(struct lowerer *l, const struct expr *e,
                        struct ir_operand dest)
{
    struct ir_operand whole;
    struct ir_operand base = lower_first_element(l, e->as.slice.base, &whole);
    struct ir_operand low = lower_expr(l, e->as.slice.low);
    struct ir_operand high = lower_expr(l, e->as.slice.high);
    struct ir_operand length;
    struct ir_operand at;
    uint32_t offset;

    offset = ir_binary(l->f, l->b, IR_MUL, IR_I64, low,
                       lower_size_operand(l, e->type->element));
    ir_store(l->f, l->b, IR_PTR,
             lower_temp(l, ir_ptradd(l->f, l->b, base, lower_temp(l, offset))),
             dest);
    length = lower_temp(l, ir_binary(l->f, l->b, IR_SUB, IR_I64, high, low));
    at = lower_offset_address(l, dest,
                              lower_field_offset(l, e->type, &lower_len_name));
    ir_store(l->f, l->b, IR_I64, length, at);
}

static const struct name tag_name = {VARIANT_TAG, sizeof VARIANT_TAG - 1};

static const struct name union_name = {VARIANT_UNION,
                                       sizeof VARIANT_UNION - 1};

/* The tag of the variant v at address, read as its integer. */
struct ir_operand lower_load_tag(struct lowerer *l, const struct type *v,
                                 struct ir_operand address)
{
    return lower_temp(
        l, ir_load(l->f, l->b, lower_ir_type_of(v->base),
                   lower_offset_address(l, address,
                                        lower_field_offset(l, v, &tag_name))));
}

/* The address of the fields of case index of the variant v at address.
   Every member of the union starts at its offset 0. */
struct ir_operand lower_case_address(struct lowerer *l, const struct type *v,
                                     struct ir_operand address)
{
    return lower_offset_address(l, address,
                                lower_field_offset(l, v, &union_name));
}

/* DESIGN: a literal of a variant writes the tag of its case and the
   fields of that case, and nothing else. The bytes of the union that
   the case leaves are never read, since `switch` reads the fields of the
   case the tag names alone. */
static void build_variant(struct lowerer *l, const struct expr *e,
                          struct ir_operand dest)
{
    const struct type *v = e->type;
    uint32_t index = e->as.struct_lit.variant_case - 1;
    const struct type *payload = v->params[index];
    struct ir_operand fields;
    size_t i;

    ir_store(l->f, l->b, lower_ir_type_of(v->base),
             ir_int_op(lower_ir_type_of(v->base),
                       v->base->fields[index].number),
             lower_offset_address(l, dest,
                                  lower_field_offset(l, v, &tag_name)));
    if (payload == NULL) {
        return;
    }
    fields = lower_case_address(l, v, dest);
    for (i = 0; i < e->as.struct_lit.field_count; i++) {
        const struct field_init *init = &e->as.struct_lit.fields[i];
        const struct struct_field *field = lower_field_of(payload, &init->name);
        lower_store_value(
            l, field->type, init->value,
            lower_offset_address(l, fields,
                                 lower_field_offset(l, payload, &field->name)));
    }
}

static void build_value_into(struct lowerer *l, const struct expr *e,
                             struct ir_operand dest);
static struct ir_operand lower_converted(struct lowerer *l,
                                         const struct expr *e);

/* Construct the aggregate value of e at dest. A literal fills its fields
   or elements in place, and any other value is copied. */
void lower_build_into(struct lowerer *l, const struct expr *e,
                      struct ir_operand dest)
{
    struct ir_operand src;

    /* A plain function in the place of two words, or a `keep own`
       parameter that moves, is a pair lower_expr builds. */
    if (e->to_context || e->moves_snapshot) {
        src = lower_expr(l, e);
        ir_memcopy(l->f, l->b, dest, src,
                   ir_aggregate(lower_agg_of(l, e->type)));
        return;
    }
    /* A T where a `?T` is expected is built in place, and the flag is
       written after it. */
    if (wraps(e)) {
        if (e->to_iface != NULL || !lower_is_aggregate(e->type)) {
            ir_store(l->f, l->b, lower_ir_type_of(e->type),
                     lower_converted(l, e), dest);
        } else {
            build_value_into(l, e, dest);
        }
        if (l->b != NULL) {
            lower_set_optional(l, e->to_optional, e->type, dest, 1);
        }
        return;
    }
    build_value_into(l, e, dest);
}

/* The value of e, of its own type, written to dest. */
static void build_value_into(struct lowerer *l, const struct expr *e,
                             struct ir_operand dest)
{
    const struct type *t = e->type;
    const struct type *owner;
    struct ir_operand src;
    size_t i;

    switch (e->kind) {
    /* `none` of a `?T` writes its flag, and the value stays as it is. */
    case EXPR_NONE:
        lower_set_optional(l, t, NULL, dest, 0);
        break;
    /* `T(args)` and `alloc T(args)` build the object in place and run
       its `construct` with the arguments. */
    case EXPR_CALL:
        if (e->as.call.builds != NULL) {
            struct ir_operand err = lower_construct(l, e, dest);
            if (err.kind != IR_NONE) {
                lower_handle_error(l, e, err, dest, !e->as.call.on_heap,
                                   e->as.call.on_heap ? dest : lower_none());
            }
            return;
        }
        /* Any other call gives an aggregate, which is copied. */
        src = lower_address(l, e);
        ir_memcopy(l->f, l->b, dest, src, lower_vtype_of(l, t));
        break;
    case EXPR_STRUCT_LIT:
        if (t->kind == TYPE_VARIANT) {
            build_variant(l, e, dest);
            break;
        }
        /* The table pointer is the first word of every object, and the
           base of a class sits at offset 0, so it goes at dest. */
        if (t->kind == TYPE_CLASS) {
            ir_store(l->f, l->b, IR_PTR,
                     lower_temp(l, ir_addr(l->f, l->b,
                                           ir_global_op(lower_class_table(l,
                                                                          t)))),
                     dest);
            lower_store_interface_tables(l, t, dest);
        }
        for (i = 0; i < e->as.struct_lit.field_count; i++) {
            const struct field_init *init = &e->as.struct_lit.fields[i];
            const struct type *at = lower_field_owner(t, &init->name);
            const struct struct_field *field = lower_field_of(at, &init->name);
            if (field->bits != 0) {
                struct ir_operand v = lower_expr(l, init->value);
                ir_bitstore(l->f, l->b, lower_ir_type_of(field->type), v, dest,
                            lower_agg_of(l, at),
                            (uint32_t)(field - at->fields));
                continue;
            }
            lower_store_value(
                l, field->type, init->value,
                lower_offset_address(l, dest,
                                     lower_field_offset(l, at, &field->name)));
        }
        /* DESIGN: a field the literal leaves out has a default, which the
           checker required, and its expression is written here. The value
           is therefore complete however the literal was written. */
        for (owner = t; owner != NULL;
             owner = owner->kind == TYPE_CLASS ? owner->base : NULL) {
        for (i = 0; i < owner->field_count; i++) {
            const struct struct_field *field = &owner->fields[i];
            size_t k;
            bool given = false;
            if (!lower_has_default(field)) {
                continue;
            }
            for (k = 0; k < e->as.struct_lit.field_count; k++) {
                given = given ||
                        (e->as.struct_lit.fields[k].name.length ==
                             field->name.length &&
                         memcmp(e->as.struct_lit.fields[k].name.text,
                                field->name.text, field->name.length) == 0);
            }
            if (given) {
                continue;
            }
            lower_store_field_default(l, owner, i, dest);
        }
        }
        lower_run_construct(l, t, dest);
        break;
    case EXPR_ARRAY_LIT:
        for (i = 0; i < e->as.array_lit.count; i++) {
            lower_store_value(
                l, t->element, e->as.array_lit.elements[i],
                lower_offset_address(l, dest,
                                     lower_element_offset(l, t->element, i)));
        }
        break;
    /* `(a, b)` writes one element per field, which is what a struct
       literal of the same types writes. */
    case EXPR_TUPLE:
        for (i = 0; i < e->as.tuple.count; i++) {
            lower_store_value(
                l, t->fields[i].type, e->as.tuple.elements[i],
                lower_offset_address(
                    l, dest, lower_field_offset(l, t, &t->fields[i].name)));
        }
        break;
    case EXPR_ARRAY_REPEAT:
        fill_array(l, e, dest);
        break;
    case EXPR_STRING:
    case EXPR_BYTES:
        ir_store(l->f, l->b, IR_PTR, lower_literal_address(l, &e->as.text),
                 dest);
        ir_store(l->f, l->b, IR_I64, ir_int_op(IR_I64, e->as.text.length),
                 lower_offset_address(l, dest,
                                      lower_field_offset(l, t,
                                                         &lower_len_name)));
        break;
    case EXPR_SLICE_LIT:
        for (i = 0; i < e->as.slice_lit.field_count; i++) {
            const struct field_init *init = &e->as.slice_lit.fields[i];
            bool len = lower_name_is(&init->name, "len");
            struct ir_operand v = lower_expr(l, init->value);
            ir_store(l->f, l->b, len ? IR_I64 : IR_PTR, v,
                     lower_offset_address(l, dest,
                                          lower_field_offset(l, t,
                                                             &init->name)));
        }
        break;
    case EXPR_SLICE:
        build_slice(l, e, dest);
        break;
    case EXPR_FIELD:
        /* DESIGN: a bound function holds the object and the entry of its
           table, so a call needs no receiver and the object may move.
           A `final` function and a `final` class take the address of the
           body, because no class below replaces it. */
        if (t->kind == TYPE_FN && t->bound) {
            const struct type *s = lower_struct_of_expr(e->as.field.base);
            size_t index = s != NULL
                               ? lower_table_index(s, &e->as.field.name,
                                                   t->param_count + 1)
                               : 0;
            struct ir_operand object =
                e->as.field.base->type->kind == TYPE_POINTER
                    ? lower_expr(l, e->as.field.base)
                    : lower_address(l, e->as.field.base);
            struct ir_operand entry;
            if (index > 0 && !lower_bound_is_direct(e, s)) {
                struct ir_operand table = lower_load_table(l, object, s);
                entry = lower_temp(
                    l, ir_load(l->f, l->b, IR_PTR,
                               lower_offset_address(
                                   l, table, lower_entry_offset(l, index))));
            } else {
                entry = lower_temp(
                    l, ir_addr(l->f, l->b,
                               ir_func_op(lower_callee_function(l,
                                                                e->symbol))));
            }
            ir_store(l->f, l->b, IR_PTR, object, dest);
            ir_store(l->f, l->b, IR_PTR, entry,
                     lower_offset_address(
                         l, dest, lower_field_offset(l, t, &lower_entry_name)));
            break;
        }
        src = lower_address(l, e);
        ir_memcopy(l->f, l->b, dest, src, lower_vtype_of(l, t));
        break;
    default:
        src = lower_address(l, e);
        ir_memcopy(l->f, l->b, dest, src, lower_vtype_of(l, t));
        break;
    }
}

static struct ir_operand lower_parallel(struct lowerer *l,
                                       const struct expr *e);

static struct ir_operand lower_dispatch(struct lowerer *l,
                                        const struct expr *e);

static struct ir_operand lower_join(struct lowerer *l, const struct expr *e);

/* DESIGN: a failing call that is an operand, as in `return try f();` or
   `g(try f())`, or a call inside a `try` block that is one, has no place
   of a `let` or an assignment to write into. It writes a slot of the
   frame, whose tables are zeroed first so that the `=` the callee runs
   destroys nothing, and the value is read from there. An aggregate is
   its slot, as the value of any other call is its memory. */
static struct ir_operand handled_operand(struct lowerer *l,
                                         const struct expr *e)
{
    bool has_out = e->as.call.out != NULL;
    struct ir_operand out = lower_none();
    struct ir_operand err;

    if (has_out) {
        out = lower_temp(l, ir_entry_slot(l->f, lower_vtype_of(l, e->type)));
        lower_clear_tables(l, out, e->type);
    }
    l->out_address = out;
    err = lower_call(l, e);
    lower_handle_error(l, e, err, out, has_out, lower_none());
    if (!has_out || lower_is_aggregate(e->type)) {
        return out;
    }
    return lower_temp(l, ir_load(l->f, l->b, lower_ir_type_of(e->type), out));
}

/* Give sym, a local that the checker wrote and no scope holds, the
   value v. An aggregate is bound by the address that v is, and a scalar
   whose address is taken gets a slot of its own. */
void lower_bind_value(struct lowerer *l, struct symbol *sym,
                      struct ir_operand v)
{
    if (sym->address_taken && !lower_is_aggregate(sym->type)) {
        sym->ir = ir_entry_slot(l->f, lower_vtype_of(l, sym->type));
        ir_store(l->f, l->b, lower_ir_type_of(sym->type), v,
                 lower_temp(l, sym->ir));
        return;
    }
    sym->ir = ir_unary(l->f, l->b, IR_COPY,
                       lower_is_aggregate(sym->type)
                           ? IR_PTR
                           : lower_ir_type_of(sym->type),
                       v);
}

/* The hidden local of an iteration takes the iterator its start gives:
   an object in a slot of the frame, or a pointer. */
void lower_bind_cursor(struct lowerer *l, const struct iteration *it)
{
    struct symbol *cursor = it->cursor;

    if (lower_is_aggregate(cursor->type)) {
        cursor->ir = ir_entry_slot(l->f, lower_vtype_of(l, cursor->type));
        lower_build_into(l, it->start, lower_temp(l, cursor->ir));
        return;
    }
    lower_bind_value(l, cursor, lower_expr(l, it->start));
}

/* The value of an iteration's `value` call written to at, an element of
   memory that holds the type t. */
static void store_value(struct lowerer *l, const struct expr *current,
                        struct ir_operand at)
{
    struct ir_operand v;

    if (lower_is_aggregate(current->type)) {
        lower_build_into(l, current, at);
        return;
    }
    /* The call may end the block it starts in, so the store goes into
       the block that follows it. */
    v = lower_expr(l, current);
    ir_store(l->f, l->b, lower_ir_type_of(current->type), v, at);
}

/* DESIGN: `to_slice` walks the iterator as `for` does and writes each
   value into memory from `realloc`. The room doubles plus four elements
   whenever it is full, so a walk of n values moves O(n) bytes. The
   memory is never shrunk, and the program frees it with
   `free(result.ptr)`. An iterator that gives nothing gives an empty
   slice whose ptr is zero, which `free` accepts. */
static struct ir_operand lower_collect(struct lowerer *l, const struct expr *e)
{
    static const enum ir_type grow_params[] = {IR_PTR, IR_I64};
    const struct iteration *it = &e->as.collect;
    const struct type *element = e->type->element;
    uint32_t slot = ir_entry_slot(l->f, lower_vtype_of(l, e->type));
    struct ir_operand size = lower_size_operand(l, element);
    struct ir_block *test = lower_new_block(l);
    struct ir_block *body = lower_new_block(l);
    struct ir_block *grow = lower_new_block(l);
    struct ir_block *put = lower_new_block(l);
    struct ir_block *exit = lower_new_block(l);
    struct ir_operand args[2];
    struct ir_operand bytes;
    struct ir_operand room;
    struct ir_operand result;
    uint32_t data;
    uint32_t count;
    uint32_t capacity;

    lower_bind_cursor(l, it);
    data = ir_unary(l->f, l->b, IR_COPY, IR_PTR, ir_int_op(IR_PTR, 0));
    count = ir_unary(l->f, l->b, IR_COPY, IR_I64, ir_int_op(IR_I64, 0));
    capacity = ir_unary(l->f, l->b, IR_COPY, IR_I64, ir_int_op(IR_I64, 0));
    ir_jump(l->f, l->b, test);
    l->b = test;
    lower_branch(l, it->advance, body, exit);
    l->b = body;
    ir_branch(l->f, l->b,
              lower_temp(l, ir_binary(l->f, l->b, IR_EQ, IR_I8,
                                      lower_temp(l, count),
                                      lower_temp(l, capacity))),
              grow, put);
    l->b = grow;
    room = lower_temp(l, ir_binary(l->f, l->b, IR_ADD, IR_I64,
                                   lower_temp(l, ir_binary(
                                                     l->f, l->b, IR_MUL,
                                                     IR_I64,
                                                     lower_temp(l, capacity),
                                                     ir_int_op(IR_I64, 2))),
                                   ir_int_op(IR_I64, 4)));
    ir_assign(l->f, l->b, capacity, room);
    bytes = lower_temp(l, ir_binary(l->f, l->b, IR_MUL, IR_I64,
                                    lower_temp(l, capacity), size));
    args[0] = lower_temp(l, data);
    args[1] = bytes;
    ir_assign(l->f, l->b, data,
              lower_temp(l, ir_call(l->f, l->b, IR_PTR,
                                    ir_func_op(lower_rt_function_giving(
                                        l, "realloc", IR_PTR, grow_params,
                                        2)),
                                    args, 2)));
    ir_jump(l->f, l->b, put);
    l->b = put;
    store_value(l, it->current,
                lower_temp(l, ir_ptradd(
                                  l->f, l->b, lower_temp(l, data),
                                  lower_temp(l, ir_binary(
                                                    l->f, l->b, IR_MUL,
                                                    IR_I64,
                                                    lower_temp(l, count),
                                                    size)))));
    if (l->b != NULL) {
        ir_assign(l->f, l->b, count,
                  lower_temp(l, ir_binary(l->f, l->b, IR_ADD, IR_I64,
                                          lower_temp(l, count),
                                          ir_int_op(IR_I64, 1))));
        ir_jump(l->f, l->b, test);
    }
    l->b = exit;
    result = lower_temp(l, slot);
    ir_store(l->f, l->b, IR_PTR, lower_temp(l, data), result);
    ir_store(l->f, l->b, IR_I64, lower_temp(l, count),
             lower_offset_address(l, result,
                                  lower_field_offset(l, e->type,
                                                     &lower_len_name)));
    return result;
}

/* DESIGN: an `f"..."` is a local `anti.text.Builder` in a slot of the
   frame, the calls the checker wrote on it, and the `str` its `take`
   gives. Each value is bound to the local the checker declared for it
   right before the call that appends it. The values are therefore
   computed from left to right, each once, between the texts around
   them. An aggregate is bound by its address, which the call reads at
   once. */
static struct ir_operand lower_format(struct lowerer *l, const struct expr *e)
{
    struct symbol *builder = e->as.format.builder;
    size_t i;

    builder->ir = ir_entry_slot(l->f, lower_vtype_of(l, builder->type));
    lower_build_into(l, e->as.format.start, lower_temp(l, builder->ir));
    for (i = 0; i < e->as.format.count; i++) {
        const struct format_part *part = &e->as.format.parts[i];
        struct ir_operand v;
        if (part->text_call != NULL) {
            lower_expr(l, part->text_call);
        }
        if (part->value == NULL) {
            continue;
        }
        v = lower_expr(l, part->value);
        part->bound->ir =
            ir_unary(l->f, l->b, IR_COPY,
                     lower_is_aggregate(part->bound->type)
                         ? IR_PTR
                         : lower_ir_type_of(part->bound->type),
                     v);
        lower_expr(l, part->value_call);
    }
    return lower_address(l, e->as.format.take);
}

static struct ir_operand lower_sync_op(struct lowerer *l,
                                       const struct expr *e);

/* The address of the memory that holds the aggregate value of e. A
   literal gets a slot of its own, and a constant is read-only data. */
static struct ir_operand coalesce(struct lowerer *l, const struct expr *e);

/* A function with its context in a slot of the frame: code and then
   context, in the aggregate of the form t. */
static struct ir_operand lower_pair(struct lowerer *l, const struct type *t,
                                    struct ir_operand code,
                                    struct ir_operand context)
{
    uint32_t agg = lower_agg_of(l, t);
    struct ir_operand slot =
        lower_temp(l, ir_entry_slot(l->f, ir_aggregate(agg)));

    ir_store(l->f, l->b, IR_PTR, code, slot);
    ir_store(l->f, l->b, IR_PTR, context,
             lower_offset_address(l, slot,
                                  ir_sym_operand(l->m,
                                                 ir_sym_offset_of(l->m, agg,
                                                                  1))));
    return slot;
}

/* The copy of the captured value at src into the record at dst. */
static void copy_captured(struct lowerer *l, const struct type *t,
                          struct ir_operand src, struct ir_operand dst)
{
    if (lower_is_aggregate(t)) {
        ir_memcopy(l->f, l->b, dst, src, lower_vtype_of(l, t));
        return;
    }
    ir_store(l->f, l->b, lower_ir_type_of(t),
             lower_temp(l, ir_load(l->f, l->b, lower_ir_type_of(t), src)),
             dst);
}

/* DESIGN: a snapshot copies each captured value into its record when it
   is made. At an `own` field or a `keep own` parameter the record goes
   on the heap with the bytes of each `str` after it. The runtime counts
   it until it is freed. Anywhere else it sits in a slot of the
   frame, and nothing is allocated. */
static struct ir_operand lower_snapshot(struct lowerer *l,
                                        const struct expr *e,
                                        struct ir_operand code)
{
    static const enum ir_type one[] = {IR_I64};
    static const enum ir_type four[] = {IR_PTR, IR_I64, IR_PTR, IR_I64};
    const struct item *it = e->as.fn;
    uint32_t agg = lower_snapshot_agg(l, it);
    struct ir_operand fixed =
        ir_sym_operand(l->m, ir_sym_size_of(l->m, ir_aggregate(agg)));
    struct ir_operand size = fixed;
    struct ir_operand block;
    struct ir_operand cursor;
    size_t i;

    if (it->snapshot_heap) {
        for (i = 0; i < it->capture_count; i++) {
            const struct symbol *sym = it->captures[i].symbol;
            struct ir_operand len;
            if (sym->type->kind != TYPE_STR) {
                continue;
            }
            len = lower_temp(
                l, ir_load(l->f, l->b, IR_I64,
                           lower_offset_address(
                               l, lower_temp(l, sym->ir),
                               lower_field_offset(l, sym->type,
                                                  &lower_len_name))));
            size = lower_temp(l, ir_binary(l->f, l->b, IR_ADD, IR_I64, size,
                                           len));
        }
        block = lower_rt_call(l, "anti_rt_snapshot_new", IR_PTR, one, &size,
                              1);
    } else {
        block = lower_temp(l, ir_entry_slot(l->f, ir_aggregate(agg)));
        ir_store(l->f, l->b, IR_I64, fixed, block);
    }
    cursor = fixed;
    for (i = 0; i < it->capture_count; i++) {
        const struct symbol *sym = it->captures[i].symbol;
        struct ir_operand src = lower_temp(l, sym->ir);
        struct ir_operand dst = lower_offset_address(
            l, block,
            ir_sym_operand(l->m,
                           ir_sym_offset_of(l->m, agg, (uint32_t)(i + 1))));
        struct ir_operand len_offset;
        struct ir_operand len;
        struct ir_operand args[4];

        if (!it->snapshot_heap || sym->type->kind != TYPE_STR) {
            copy_captured(l, sym->type, src, dst);
            continue;
        }
        len_offset = lower_field_offset(l, sym->type, &lower_len_name);
        len = lower_temp(l, ir_load(l->f, l->b, IR_I64,
                                    lower_offset_address(l, src, len_offset)));
        args[0] = block;
        args[1] = cursor;
        args[2] = lower_temp(l, ir_load(l->f, l->b, IR_PTR, src));
        args[3] = len;
        lower_rt_call(l, "anti_rt_snapshot_text", IR_VOID, four, args, 4);
        ir_store(l->f, l->b, IR_I64, cursor, dst);
        ir_store(l->f, l->b, IR_I64, len,
                 lower_offset_address(l, dst, len_offset));
        cursor = lower_temp(l, ir_binary(l->f, l->b, IR_ADD, IR_I64, cursor,
                                         len));
    }
    return lower_pair(l, e->type, code, block);
}

/* DESIGN: an anonymous function that captures nothing is the address of
   its code, as a named function is. A closure is the code and a context
   in the frame that makes it: a record of the address of every variable
   it captures. Creating one allocates nothing. The record and the pair
   sit in slots of the frame, which outlive every local the closure may
   be held in. */
static struct ir_operand lower_closure(struct lowerer *l,
                                       const struct expr *e)
{
    struct item *it = e->as.fn;
    struct ir_function *f = lower_anonymous_function(l, it);
    struct ir_operand code = lower_temp(l, ir_addr(l->f, l->b,
                                                   ir_func_op(f)));
    uint32_t agg;
    struct ir_operand record;
    size_t i;

    if (it->capture_count == 0) {
        return code;
    }
    if (it->snapshot) {
        return lower_snapshot(l, e, code);
    }
    agg = lower_captures_agg(l, it);
    record = lower_temp(l, ir_entry_slot(l->f, ir_aggregate(agg)));
    for (i = 0; i < it->capture_count; i++) {
        ir_store(l->f, l->b, IR_PTR,
                 lower_temp(l, it->captures[i].symbol->ir),
                 lower_offset_address(
                     l, record,
                     i == 0 ? lower_zero()
                            : ir_sym_operand(l->m,
                                             ir_sym_offset_of(l->m, agg,
                                                              (uint32_t)i))));
    }
    return lower_pair(l, e->type, code, record);
}

struct ir_operand lower_address(struct lowerer *l,
                                const struct expr *e)
{
    const struct symbol *sym = e->symbol;
    uint32_t slot;

    if (sym != NULL && sym->kind == SYMBOL_CONST &&
        (e->kind == EXPR_NAME || e->kind == EXPR_FIELD)) {
        return lower_const_address(l, sym->value, e->type);
    }
    /* A static field is a global, and its name is its whole address. */
    if (sym != NULL && sym->kind == SYMBOL_GLOBAL) {
        return lower_temp(l, ir_addr(l->f, l->b,
                                     ir_global_op(lower_static_global(l,
                                                                      sym))));
    }
    /* An operation on simd structs gives its value in a slot of its
       own. */
    if (e->kind == EXPR_BINARY && type_is_simd(e->as.binary.left->type)) {
        return lower_simd_binary(l, e);
    }
    if (e->kind == EXPR_UNARY && type_is_simd(e->type) &&
        (e->as.unary.op == TOKEN_MINUS || e->as.unary.op == TOKEN_TILDE)) {
        return lower_simd_unary(l, e);
    }
    if (e->kind == EXPR_CAST && (type_is_simd(e->type) ||
                                 type_is_simd(e->as.cast.operand->type))) {
        return lower_simd_cast(l, e);
    }
    if (e->kind == EXPR_BINARY && e->as.binary.op == TOKEN_QUESTION_QUESTION &&
        (lower_none_in_first_word(e->type) ||
         e->as.binary.left->type->kind == TYPE_OPTIONAL)) {
        return coalesce(l, e);
    }
    switch (e->kind) {
    case EXPR_SIMD:
        return lower_simd(l, e);
    case EXPR_NAME:
        return lower_temp(l, sym->ir);
    case EXPR_FIELD:
        return lower_field_address(l, e);
    case EXPR_INDEX:
        return lower_element_address(l, e);
    case EXPR_UNARY:
        return lower_expr(l, e->as.unary.operand);
    case EXPR_CALL:
        if (lower_is_handled_call(e)) {
            return handled_operand(l, e);
        }
        return lower_call(l, e);
    case EXPR_PARALLEL:
        return lower_parallel(l, e);
    case EXPR_DISPATCH:
        return lower_dispatch(l, e);
    case EXPR_JOIN:
        return lower_join(l, e);
    case EXPR_SYNC_OP:
        return lower_sync_op(l, e);
    case EXPR_HERE:
        return lower_const_address(l, lower_location_value(l, e->pos, e->type),
                                   e->type);
    case EXPR_FORMAT:
        return lower_format(l, e);
    case EXPR_PATTERN:
        return lower_pattern(l, e);
    case EXPR_COLLECT:
        return lower_collect(l, e);
    case EXPR_FN:
        return lower_closure(l, e);
    /* `dup` of an `own fn` copies the snapshot into a new pair. */
    case EXPR_OBJECT:
        slot = ir_entry_slot(l->f, lower_vtype_of(l, e->type));
        lower_dup_snapshot(l, e->type, lower_expr(l, e->as.object.operand),
                           lower_temp(l, slot));
        return lower_temp(l, slot);
    /* `none` in the form of two words has no code and no context. */
    case EXPR_NONE:
        if (lower_is_context(e->type)) {
            return lower_pair(l, e->type, ir_int_op(IR_PTR, 0),
                              ir_int_op(IR_PTR, 0));
        }
        slot = ir_entry_slot(l->f, lower_vtype_of(l, e->type));
        build_value_into(l, e, lower_temp(l, slot));
        return lower_temp(l, slot);
    default:
        slot = ir_entry_slot(l->f, lower_vtype_of(l, e->type));
        build_value_into(l, e, lower_temp(l, slot));
        return lower_temp(l, slot);
    }
}

struct ir_operand lower_read_place(struct lowerer *l, const struct place *p)
{
    if (p->in_temp) {
        return lower_temp(l, p->temp);
    }
    if (p->bitfield) {
        return lower_temp(l, ir_bitload(l->f, l->b, p->type, p->address, p->agg,
                                        p->field));
    }
    return lower_temp(l, ir_load(l->f, l->b, p->type, p->address));
}

static struct ir_operand lower_name(struct lowerer *l, const struct expr *e)
{
    const struct symbol *sym = e->symbol;
    struct place p;

    if (sym->kind == SYMBOL_CONST) {
        return lower_constant(l, sym->value, lower_ir_type_of(e->type));
    }
    if (sym->kind == SYMBOL_FN || sym->kind == SYMBOL_EXTERN_FN) {
        return lower_temp(l, ir_addr(l->f, l->b,
                                     ir_func_op(lower_callee_function(l,
                                                                      sym))));
    }
    lower_place(l, e, &p);
    return lower_read_place(l, &p);
}

static struct ir_operand lower_unary(struct lowerer *l, const struct expr *e)
{
    const struct expr *operand = e->as.unary.operand;
    enum ir_type type = lower_ir_type_of(e->type);
    struct ir_operand v;
    struct place p;

    switch (e->as.unary.op) {
    case TOKEN_MINUS:
        /* A '-' directly before a literal forms one constant. */
        if (operand->kind == EXPR_INT) {
            return ir_int_op(type, 0 - operand->as.integer);
        }
        if (operand->kind == EXPR_FLOAT) {
            return ir_float_op(type, -lower_float_literal(operand, type));
        }
        v = lower_expr(l, operand);
        return lower_temp(l, ir_unary(l->f, l->b,
                                      type == IR_F32 || type == IR_F64 ? IR_FNEG
                                                                       : IR_NEG,
                                      type, v));
    case TOKEN_BANG:
        v = lower_expr(l, operand);
        return lower_temp(l, ir_binary(l->f, l->b, IR_XOR, IR_I8, v,
                                       ir_int_op(IR_I8, 1)));
    case TOKEN_TILDE:
        v = lower_expr(l, operand);
        return lower_temp(l, ir_unary(l->f, l->b, IR_NOT, type, v));
    case TOKEN_STAR:
        v = lower_expr(l, operand);
        return lower_temp(l, ir_load(l->f, l->b, type, v));
    default: /* TOKEN_AMP: semantic analysis marked the operand */
        return lower_place(l, operand, &p) ? p.address : lower_none();
    }
}

enum ir_op lower_binary_op(enum token_kind op, const struct type *t)
{
    bool is_float = type_is_float(t);
    bool is_signed = type_is_signed(t);

    switch (op) {
    case TOKEN_PLUS: return is_float ? IR_FADD : IR_ADD;
    case TOKEN_MINUS: return is_float ? IR_FSUB : IR_SUB;
    case TOKEN_STAR: return is_float ? IR_FMUL : IR_MUL;
    case TOKEN_SLASH: return is_float ? IR_FDIV : is_signed ? IR_SDIV : IR_UDIV;
    case TOKEN_PERCENT: return is_signed ? IR_SREM : IR_UREM;
    case TOKEN_AMP: return IR_AND;
    case TOKEN_PIPE: return IR_OR;
    case TOKEN_CARET: return IR_XOR;
    case TOKEN_SHL: return IR_SHL;
    case TOKEN_SHR: return is_signed ? IR_SHR_S : IR_SHR_U;
    case TOKEN_PLUS_WRAP: return IR_ADD;
    case TOKEN_MINUS_WRAP: return IR_SUB;
    case TOKEN_STAR_WRAP: return IR_MUL;
    case TOKEN_PLUS_SAT: return is_signed ? IR_ADD_SAT_S : IR_ADD_SAT_U;
    case TOKEN_MINUS_SAT: return is_signed ? IR_SUB_SAT_S : IR_SUB_SAT_U;
    case TOKEN_STAR_SAT: return is_signed ? IR_MUL_SAT_S : IR_MUL_SAT_U;
    case TOKEN_MUL_HIGH: return is_signed ? IR_MULH_S : IR_MULH_U;
    case TOKEN_EQ: return is_float ? IR_FEQ : IR_EQ;
    case TOKEN_NE: return is_float ? IR_FNE : IR_NE;
    case TOKEN_LT: return is_float ? IR_FLT : is_signed ? IR_SLT : IR_ULT;
    case TOKEN_LE: return is_float ? IR_FLE : is_signed ? IR_SLE : IR_ULE;
    case TOKEN_GT: return is_float ? IR_FGT : is_signed ? IR_SGT : IR_UGT;
    default: return is_float ? IR_FGE : is_signed ? IR_SGE : IR_UGE;
    }
}

bool lower_is_comparison(enum token_kind op)
{
    return op == TOKEN_EQ || op == TOKEN_NE || op == TOKEN_LT ||
           op == TOKEN_LE || op == TOKEN_GT || op == TOKEN_GE;
}

/* a && b or a || b as a value. The result is a when a decides it, else b. */
static struct ir_operand short_circuit(struct lowerer *l, const struct expr *e)
{
    bool is_and = e->as.binary.op == TOKEN_AND_AND;
    struct ir_operand left = lower_expr(l, e->as.binary.left);
    struct ir_operand right;
    struct ir_block *rest;
    struct ir_block *join;
    uint32_t result;

    result = ir_unary(l->f, l->b, IR_COPY, IR_I8, left);
    rest = lower_new_block(l);
    join = lower_new_block(l);
    ir_branch(l->f, l->b, left, is_and ? rest : join, is_and ? join : rest);
    l->b = rest;
    right = lower_expr(l, e->as.binary.right);
    ir_assign(l->f, l->b, result, right);
    ir_jump(l->f, l->b, join);
    l->b = join;
    return lower_temp(l, result);
}

/* p ?? q as a value. The result is p when it is not `none`, and q
   otherwise, which runs only then. */
/* DESIGN: `o ?? v` of a `?T` gives the value o holds when its flag is
   set and v otherwise. The value lies at offset 0 of o, so the result
   reads it there. The result has a slot of its own, so a later write to
   o leaves it as it is. */
static struct ir_operand coalesce_value(struct lowerer *l,
                                        const struct expr *e)
{
    const struct type *t = e->type;
    const struct type *from = e->as.binary.left->type;
    struct ir_operand left = lower_expr(l, e->as.binary.left);
    struct ir_operand held = lower_optional_flag(l, from, left);
    struct ir_block *have = lower_new_block(l);
    struct ir_block *rest = lower_new_block(l);
    struct ir_block *join = lower_new_block(l);
    struct ir_operand slot;
    uint32_t result = 0;

    if (lower_is_aggregate(t)) {
        slot = lower_temp(l, ir_entry_slot(l->f, lower_vtype_of(l, t)));
    } else {
        result = ir_unary(l->f, l->b, IR_COPY, lower_ir_type_of(t),
                          ir_int_op(lower_ir_type_of(t), 0));
        slot = lower_none();
    }
    ir_branch(l->f, l->b,
              lower_temp(l, ir_binary(l->f, l->b, IR_NE, IR_I8, held,
                                      ir_int_op(IR_I8, 0))),
              have, rest);
    l->b = have;
    if (result == 0) {
        ir_memcopy(l->f, l->b, slot, left, lower_vtype_of(l, t));
    } else {
        ir_assign(l->f, l->b, result,
                  lower_temp(l, ir_load(l->f, l->b, lower_ir_type_of(t),
                                        left)));
    }
    ir_jump(l->f, l->b, join);
    l->b = rest;
    if (result == 0) {
        lower_build_into(l, e->as.binary.right, slot);
    } else {
        struct ir_operand v = lower_expr(l, e->as.binary.right);
        if (l->b != NULL) {
            ir_assign(l->f, l->b, result, v);
        }
    }
    if (l->b != NULL) {
        ir_jump(l->f, l->b, join);
    }
    l->b = join;
    return result == 0 ? slot : lower_temp(l, result);
}

static struct ir_operand coalesce(struct lowerer *l, const struct expr *e)
{
    if (e->as.binary.left->type->kind == TYPE_OPTIONAL) {
        return coalesce_value(l, e);
    }
    /* Of two functions with their context, the result is the address of
       the pair it takes, and the test reads the code. */
    bool pair = lower_none_in_first_word(e->type);
    enum ir_type type = pair ? IR_PTR : lower_ir_type_of(e->type);
    struct ir_operand left = lower_expr(l, e->as.binary.left);
    struct ir_operand right;
    struct ir_operand is_none;
    struct ir_block *rest;
    struct ir_block *join;
    uint32_t result;

    result = ir_unary(l->f, l->b, IR_COPY, type, left);
    is_none = lower_temp(
        l, ir_binary(l->f, l->b, IR_EQ, IR_I8,
                     pair ? lower_temp(l, ir_load(l->f, l->b, IR_PTR, left))
                          : left,
                     ir_int_op(type, 0)));
    rest = lower_new_block(l);
    join = lower_new_block(l);
    ir_branch(l->f, l->b, is_none, rest, join);
    l->b = rest;
    right = lower_expr(l, e->as.binary.right);
    if (l->b != NULL) {
        ir_assign(l->f, l->b, result, right);
        ir_jump(l->f, l->b, join);
    }
    l->b = join;
    return lower_temp(l, result);
}

/* p?.x and p?.f(args) as a value. The field or the call reads the local
   the checker bound to p, and runs only when p is not `none`. The result
   is `none` otherwise. */
/* DESIGN: a `?T` of a value as the base of `?.` binds the address of
   the value it holds, and its flag decides. */
static struct ir_operand lower_optional(struct lowerer *l,
                                        const struct expr *e)
{
    const struct type *base = e->as.optional.base->type;
    enum ir_type type = lower_ir_type_of(e->type);
    struct ir_operand p = lower_expr(l, e->as.optional.base);
    struct ir_operand v;
    struct ir_operand is_none;
    struct ir_block *rest;
    struct ir_block *join;
    uint32_t result;

    lower_bind_value(l, e->as.optional.bound, p);
    result = ir_unary(l->f, l->b, IR_COPY, type, ir_int_op(type, 0));
    is_none = base->kind == TYPE_OPTIONAL
                  ? lower_temp(l, ir_binary(l->f, l->b, IR_EQ, IR_I8,
                                            lower_optional_flag(l, base, p),
                                            ir_int_op(IR_I8, 0)))
                  : lower_temp(l, ir_binary(l->f, l->b, IR_EQ, IR_I8, p,
                                            ir_int_op(IR_PTR, 0)));
    rest = lower_new_block(l);
    join = lower_new_block(l);
    ir_branch(l->f, l->b, is_none, join, rest);
    l->b = rest;
    v = lower_expr(l, e->as.optional.access);
    if (l->b != NULL) {
        ir_assign(l->f, l->b, result, v);
        ir_jump(l->f, l->b, join);
    }
    l->b = join;
    return lower_temp(l, result);
}

/* DESIGN: `==` on class pointers compares object identity. A pointer to
   an interface sub-object points into the middle of an object. Each
   side therefore moves back to the start of its object before the
   addresses are compared. A pointer to a concrete class is already there. Only a
   pointer whose static type is abstract can be a sub-object. The runtime
   does the move, because it also has to answer for `none`. */
static bool may_be_sub(const struct type *t)
{
    return t->kind == TYPE_POINTER && t->element->kind == TYPE_CLASS &&
           t->element->has_abstract;
}

static struct ir_operand object_of(struct lowerer *l, struct ir_operand p)
{
    static const enum ir_type params[] = {IR_PTR};

    return lower_temp(
        l, ir_call(l->f, l->b, IR_PTR,
                   ir_func_op(lower_rt_function(l, "anti_rt_object_of",
                                                params, 1)),
                   &p, 1));
}

/* A bool of 0 or 1 as a value of type, which is itself for an i8. */
static struct ir_operand bool_as(struct lowerer *l, struct ir_operand v,
                                 enum ir_type type)
{
    return type == IR_I8
               ? v
               : lower_temp(l, ir_unary(l->f, l->b, IR_ZEXT, type, v));
}

/* DESIGN: `a <<% n` is the shift where the count is below the width and
   0 elsewhere. The count widens to 64 bits by its signedness and compares
   unsigned against the width, as the dev-mode check of `<<` does. A
   negative count therefore counts as one above it. The comparison becomes
   a mask of all ones or of zero, so no branch is taken. A constant count
   in the width leaves the shift alone after folding. */
struct ir_operand lower_shift_wrap(struct lowerer *l, const struct type *t,
                                   struct ir_operand value,
                                   struct ir_operand count)
{
    enum ir_type type = lower_ir_type_of(t);
    struct ir_operand wide = lower_widen_operand(l, count, t);
    struct ir_operand width =
        lower_temp(l, ir_binary(l->f, l->b, IR_MUL, IR_I64,
                                lower_size_operand(l, t),
                                ir_int_op(IR_I64, 8)));
    struct ir_operand inside =
        lower_temp(l, ir_binary(l->f, l->b, IR_ULT, IR_I8, wide, width));
    struct ir_operand shifted =
        lower_temp(l, ir_binary(l->f, l->b, IR_SHL, type, value, count));
    struct ir_operand one = bool_as(l, inside, type);
    struct ir_operand mask =
        lower_temp(l, ir_unary(l->f, l->b, IR_NEG, type, one));

    return lower_temp(l, ir_binary(l->f, l->b, IR_AND, type, shifted, mask));
}

/* The flag operation of the operator op on operands of type t. */
static enum ir_op flag_op(enum token_kind op, bool unary, const struct type *t)
{
    if (unary) {
        return IR_NEG_FL;
    }
    switch (op) {
    case TOKEN_PLUS: return IR_ADD_FL;
    case TOKEN_MINUS: return IR_SUB_FL;
    case TOKEN_STAR: return IR_MUL_FL;
    case TOKEN_SHL: return IR_SHL_FL;
    default: return type_is_signed(t) ? IR_SHR_S_FL : IR_SHR_U_FL;
    }
}

/* DESIGN: the flags form is one flag operation and one IR_FLAG per field
   in want, which holds the fields that the program reads. A field that
   nothing reads costs nothing. `a + b + f.carry` is one operation with
   the carry in, which the back end makes adc and adcs, and so is `a -
   b - f.carry` with the borrow. A carry after any other operand is the
   operand plus 0 and the carry. The operands are computed from left to
   right, the carry last. Writes flags[k] for each bit k of want and
   returns the result. */
struct ir_operand lower_flag_operation(struct lowerer *l,
                                       const struct expr *e,
                                       uint8_t want,
                                       struct ir_operand flags[4])
{
    enum ir_type type = lower_ir_type_of(e->type);
    struct ir_operand a;
    struct ir_operand b = lower_none();
    struct ir_operand c = lower_none();
    struct ir_operand result;
    enum ir_op op;
    int k;

    if (e->kind == EXPR_UNARY) {
        op = IR_NEG_FL;
        a = lower_expr(l, e->as.unary.operand);
    } else if (e->as.binary.carry) {
        const struct expr *left = e->as.binary.left;
        op = flag_op(e->as.binary.op, false, e->type);
        if (left->kind == EXPR_BINARY &&
            left->as.binary.op == e->as.binary.op &&
            !left->as.binary.carry && left->type == e->type &&
            left->as.binary.left->type == e->type) {
            a = lower_expr(l, left->as.binary.left);
            b = lower_expr(l, left->as.binary.right);
        } else {
            a = lower_expr(l, left);
            b = ir_int_op(type, 0);
        }
        c = lower_expr(l, e->as.binary.right);
    } else {
        op = flag_op(e->as.binary.op, false, e->as.binary.left->type);
        a = lower_expr(l, e->as.binary.left);
        b = lower_expr(l, e->as.binary.right);
    }
    result = lower_temp(l, ir_flag_op(l->f, l->b, op, type, a, b, c));
    for (k = 0; k < 4; k++) {
        if ((want >> k) & 1) {
            flags[k] = lower_temp(l,
                                  ir_flag(l->f, l->b, (enum ir_flag)k, result));
        }
    }
    return result;
}

/* `a + b + f.carry` or `a - f.carry` as a value. It keeps the check of
   the plain operator in a dev build, over the whole operation, and reads
   the overflow flag for it. */
static struct ir_operand lower_carry(struct lowerer *l, const struct expr *e)
{
    struct ir_operand flags[4];
    struct ir_operand result;
    struct ir_operand a;
    struct ir_operand b;
    const struct ir_global *text;
    const struct type *t = e->type;
    char operation[64];

    result = lower_flag_operation(
        l, e, type_is_signed(t) ? 1u << IR_FLAG_OVERFLOW : 0u, flags);
    if (!type_is_signed(t)) {
        return result;
    }
    /* The operation stands before its one read. */
    a = l->b->insts[l->b->count - 2].a;
    b = l->b->insts[l->b->count - 2].b;
    snprintf(operation, sizeof operation, "overflow in %s",
             e->as.binary.op == TOKEN_PLUS ? "+" : "-");
    text = lower_check_text(l, e->pos.line, operation);
    lower_check_branch(l, flags[IR_FLAG_OVERFLOW], true, text, CHECK_OVERFLOW,
                       a, b,
                       t);
    return result;
}

static struct ir_operand lower_binary(struct lowerer *l, const struct expr *e)
{
    enum token_kind op = e->as.binary.op;
    const struct type *operands = e->as.binary.left->type;
    struct ir_operand left;
    struct ir_operand right;
    struct ir_operand checked;
    bool identity = (op == TOKEN_EQ || op == TOKEN_NE) &&
                    may_be_sub(e->as.binary.left->type) &&
                    may_be_sub(e->as.binary.right->type);

    if (op == TOKEN_AND_AND || op == TOKEN_OR_OR) {
        return short_circuit(l, e);
    }
    if (op == TOKEN_QUESTION_QUESTION) {
        return coalesce(l, e);
    }
    if (e->as.binary.carry) {
        return lower_carry(l, e);
    }
    /* A `?T` of a value compares with `none` by its flag. */
    if ((op == TOKEN_EQ || op == TOKEN_NE) &&
        (e->as.binary.left->type->kind == TYPE_OPTIONAL ||
         e->as.binary.right->type->kind == TYPE_OPTIONAL)) {
        const struct expr *value = e->as.binary.left->kind == EXPR_NONE
                                       ? e->as.binary.right
                                       : e->as.binary.left;
        struct ir_operand held =
            lower_optional_flag(l, value->type, lower_expr(l, value));
        return lower_temp(l, ir_binary(l->f, l->b,
                                       op == TOKEN_EQ ? IR_EQ : IR_NE, IR_I8,
                                       held, ir_int_op(IR_I8, 0)));
    }
    left = lower_expr(l, e->as.binary.left);
    right = lower_expr(l, e->as.binary.right);
    /* A function with its context compares by its code, and a match by
       its pattern, each zero for `none`. */
    if (lower_none_in_first_word(e->as.binary.left->type)) {
        left = lower_temp(l, ir_load(l->f, l->b, IR_PTR, left));
    }
    if (lower_none_in_first_word(e->as.binary.right->type)) {
        right = lower_temp(l, ir_load(l->f, l->b, IR_PTR, right));
    }
    if (identity) {
        left = object_of(l, left);
        right = object_of(l, right);
    }
    if (op == TOKEN_SHL_WRAP) {
        return lower_shift_wrap(l, operands, left, right);
    }
    checked = lower_binary_checks(l, op, operands, left, right, e->pos.line);
    if (checked.kind != IR_NONE) {
        return checked;
    }
    return lower_temp(
        l, ir_binary(l->f, l->b, lower_binary_op(op, operands),
                     lower_is_comparison(op) ? IR_I8
                                             : lower_ir_type_of(operands),
                     left, right));
}

/* The field of a descriptor at index, read through the pointer d. */
static struct ir_operand descriptor_field(struct lowerer *l,
                                          struct ir_operand d, uint32_t index,
                                          enum ir_type type)
{
    struct ir_operand at =
        index == 0 ? d
                   : lower_temp(l, ir_ptradd(
                                       l->f, l->b, d,
                                       ir_sym_operand(
                                           l->m,
                                           ir_sym_offset_of(
                                               l->m, lower_descriptor_agg(l),
                                               index))));
    return lower_temp(l, ir_load(l->f, l->b, type, at));
}

struct ir_operand lower_load_table(struct lowerer *l, struct ir_operand p,
                                   const struct type *t)
{
    struct ir_operand table = lower_temp(l, ir_load(l->f, l->b, IR_PTR, p));

    if (t != NULL && t->kind == TYPE_POINTER) {
        t = t->element;
    }
    if (l->dev && t != NULL && t->kind == TYPE_CLASS) {
        lower_check_table(l, table, t);
    }
    return table;
}

/* Trap with the name of t when table is zero. */
void lower_check_table(struct lowerer *l, struct ir_operand table,
                       const struct type *t)
{
    static const enum ir_type params[] = {IR_PTR, IR_I64};
    struct ir_block *bad;
    struct ir_block *join;
    struct token_text text;
    struct ir_operand args[2];

    bad = lower_new_block(l);
    join = lower_new_block(l);
    ir_branch(l->f, l->b,
              lower_temp(l, ir_binary(l->f, l->b, IR_EQ, IR_I8, table,
                                      ir_int_op(IR_PTR, 0))),
              bad, join);
    l->b = bad;
    text.bytes = t->name.text;
    text.length = t->name.length;
    args[0] = lower_temp(l, ir_addr(l->f, l->b,
                                    ir_global_op(lower_literal_global(l,
                                                                      &text))));
    args[1] = ir_int_op(IR_I64, t->name.length);
    ir_call(l->f, l->b, IR_VOID,
            ir_func_op(lower_rt_function(l, "anti_rt_table_unset", params, 2)),
            args, 2);
    ir_jump(l->f, l->b, join);
    l->b = join;
}

/* DESIGN: `p is *T` holds when the object is T or a class below it. The
   ancestors of a class are its descriptors from the root down. The entry
   at T's depth is therefore T's descriptor for every class below T. An
   object shallower than T has no entry there, so the depth is compared
   first. The two comparisons are one branch each. */
static struct ir_operand class_test(struct lowerer *l, struct ir_operand p,
                                    const struct type *from,
                                    const struct type *to)
{
    uint32_t depth = lower_class_depth(to);
    struct ir_operand table;
    struct ir_operand descriptor;
    struct ir_operand object_depth;
    struct ir_block *deep;
    struct ir_block *join;
    uint32_t result;
    struct ir_operand ancestors;
    struct ir_operand at;
    struct ir_operand found;

    result = ir_unary(l->f, l->b, IR_COPY, IR_I8, ir_int_op(IR_I8, 0));
    deep = lower_new_block(l);
    join = lower_new_block(l);
    /* DESIGN: `is` and `as?` take a `?*T` as readily as a `*T`, and
       `none` is of no class. The table lies behind the pointer, so the
       test reads it only once the pointer proves to be there. This is
       the one place the nullable rules add an instruction, and it sits
       in a test that already branches. */
    if (type_is_nullable(from)) {
        struct ir_block *held = lower_new_block(l);
        ir_branch(l->f, l->b,
                  lower_temp(l, ir_binary(l->f, l->b, IR_NE, IR_I8, p,
                                          ir_int_op(IR_PTR, 0))),
                  held, join);
        l->b = held;
    }
    table = lower_load_table(l, p, from);
    descriptor = lower_temp(l, ir_load(l->f, l->b, IR_PTR, table));
    object_depth = descriptor_field(l, descriptor, 4, IR_I64);
    ir_branch(l->f, l->b,
              lower_temp(l, ir_binary(l->f, l->b, IR_SGE, IR_I8, object_depth,
                                      ir_int_op(IR_I64, depth))),
              deep, join);
    l->b = deep;
    ancestors = descriptor_field(l, descriptor, 5, IR_PTR);
    at = lower_temp(
        l, ir_load(l->f, l->b, IR_PTR,
                   lower_offset_address(l, ancestors,
                                        lower_entry_offset(l, depth))));
    found = lower_temp(
        l, ir_binary(l->f, l->b, IR_EQ, IR_I8, at,
                     lower_temp(l, ir_addr(l->f, l->b,
                                           ir_global_op(lower_class_descriptor(
                                               l, to))))));
    ir_assign(l->f, l->b, result, found);
    ir_jump(l->f, l->b, join);
    l->b = join;
    return lower_temp(l, result);
}

/* `p as *T` traps on a mismatch and `p as? *T` gives `none`. */
static struct ir_operand checked_cast(struct lowerer *l, struct ir_operand p,
                                      const struct type *from,
                                      const struct type *to, bool gives_null,
                                      bool from_sub)
{
    static const enum ir_type params[] = {IR_PTR, IR_I64};
    struct ir_operand ok = class_test(l, p, from, to);
    struct ir_block *bad = lower_new_block(l);
    struct ir_block *join = lower_new_block(l);
    struct token_text text;
    const struct ir_global *name;
    struct ir_operand args[2];
    uint32_t result = ir_unary(l->f, l->b, IR_COPY, IR_PTR, p);

    /* A pointer to an interface sub-object leads back to the object by
       the offset its descriptor holds. */
    if (from_sub) {
        struct ir_operand table = lower_temp(l, ir_load(l->f, l->b, IR_PTR, p));
        struct ir_operand descriptor =
            lower_temp(l, ir_load(l->f, l->b, IR_PTR, table));
        struct ir_operand offset =
            descriptor_field(l, descriptor, 9, IR_I64);
        ir_assign(l->f, l->b, result,
                  lower_temp(l, ir_ptradd(
                                    l->f, l->b, p,
                                    lower_temp(l, ir_binary(
                                                      l->f, l->b, IR_SUB,
                                                      IR_I64,
                                                      ir_int_op(IR_I64, 0),
                                                      offset)))));
    }
    ir_branch(l->f, l->b, ok, join, bad);
    l->b = bad;
    if (gives_null) {
        ir_assign(l->f, l->b, result, ir_int_op(IR_PTR, 0));
    } else {
        text.bytes = to->name.text;
        text.length = to->name.length;
        name = lower_literal_global(l, &text);
        args[0] = lower_temp(l, ir_addr(l->f, l->b, ir_global_op(name)));
        args[1] = ir_int_op(IR_I64, to->name.length);
        ir_call(l->f, l->b, IR_VOID,
                ir_func_op(lower_rt_function(l, "anti_rt_cast_failed", params,
                                             2)),
                args, 2);
    }
    ir_jump(l->f, l->b, join);
    l->b = join;
    return lower_temp(l, result);
}

/* The value of e as an i64, which every atomic operation of the runtime
   takes. A narrower value extends, and a pointer is copied. */
static struct ir_operand widen_to_i64(struct lowerer *l, const struct expr *e)
{
    struct ir_operand v = lower_expr(l, e);
    enum ir_type from = lower_ir_type_of(e->type);

    if (from == IR_I64 || from == IR_PTR) {
        return v;
    }
    return lower_temp(l, ir_unary(l->f, l->b,
                                  type_is_signed(e->type) ? IR_SEXT : IR_ZEXT,
                                  IR_I64, v));
}

/* The i64 a runtime operation gave back, as a value of type t. */
static struct ir_operand narrow_from_i64(struct lowerer *l,
                                         struct ir_operand v,
                                         const struct type *t)
{
    enum ir_type to = lower_ir_type_of(t);

    if (to == IR_I64 || to == IR_PTR) {
        return v;
    }
    return lower_temp(l, ir_unary(l->f, l->b, IR_TRUNC, to, v));
}

/* The integer type that t converts as. An enum converts as its base
   type, and char as the unsigned 32-bit value it already is. */
static const struct type *integer_form(const struct type *t)
{
    return t->kind == TYPE_ENUM ? t->base : t;
}

/* Whether t takes part in the conversion checks. An integer, char and an
   enum each have a range or a set of values of their own. */
static bool converts_as_integer(const struct type *t)
{
    return type_is_integer(t) || t->kind == TYPE_CHAR ||
           t->kind == TYPE_ENUM;
}

/* The value an enum declares for its name at index i, as an i64. A
   signed base type extends its sign, so the constant compares against
   the widened value the check builds. */
static uint64_t enum_value(const struct type *t, size_t i)
{
    uint64_t n = t->fields[i].number;
    int width = type_bits(t->base);

    if (type_is_signed(t->base) && width > 0 && width < 64 &&
        ((n >> (width - 1)) & 1) != 0) {
        n |= ~(uint64_t)0 << width;
    }
    return n;
}

/* DESIGN: a value that becomes a char must be a Unicode scalar value:
   at most 0x10FFFF and never one of the surrogates. The comparison is
   unsigned on the widened value, so a negative source fails the first
   test and needs no test of its own. The surrogates are one range, so
   subtracting its start turns the pair of bounds into one comparison. */
static void scalar_check(struct lowerer *l, const struct expr *e,
                         const struct type *from, struct ir_operand wide,
                         const char *operation, enum check_kind kind,
                         struct ir_operand v)
{
    struct ir_operand low =
        lower_temp(l, ir_binary(l->f, l->b, IR_ULE, IR_I8, wide,
                                ir_int_op(IR_I64, 0x10FFFF)));
    struct ir_operand off =
        lower_temp(l, ir_binary(l->f, l->b, IR_SUB, IR_I64, wide,
                                ir_int_op(IR_I64, 0xD800)));
    struct ir_operand off_ok =
        lower_temp(l, ir_binary(l->f, l->b, IR_UGE, IR_I8, off,
                                ir_int_op(IR_I64, 0x800)));
    struct ir_operand ok =
        lower_temp(l, ir_binary(l->f, l->b, IR_AND, IR_I8, low, off_ok));

    lower_check_branch(l, ok, false,
                       lower_check_text(l, e->pos.line, operation), kind, v,
                       lower_none(), from);
}

/* DESIGN: a value that becomes an enum must be one of the values the
   enum declares. The test compares the widened value against each of
   them and takes the union. That is one comparison per name and no
   block of its own. */
static void enum_check(struct lowerer *l, const struct expr *e,
                       const struct type *from, const struct type *to,
                       struct ir_operand wide, const char *operation,
                       enum check_kind kind, struct ir_operand v)
{
    struct ir_operand ok = lower_none();
    size_t i;

    for (i = 0; i < to->field_count; i++) {
        struct ir_operand is =
            lower_temp(l, ir_binary(l->f, l->b, IR_EQ, IR_I8, wide,
                                    ir_int_op(IR_I64, enum_value(to, i))));
        ok = ok.kind == IR_NONE
                 ? is
                 : lower_temp(l, ir_binary(l->f, l->b, IR_OR, IR_I8, ok, is));
    }
    lower_check_branch(l, ok, false,
                       lower_check_text(l, e->pos.line, operation), kind, v,
                       lower_none(), from);
}

/* DESIGN: a narrowing `as` is checked by the round trip. The value goes
   to the target type and back to the source with the target's
   signedness. A value the target cannot hold comes back changed.
   The round trip is blind to a change of sign alone, because a target of
   the same width keeps every bit. That case is a comparison against
   zero. A target-sized type leaves both to the back end, because a
   conversion that is a copy on one target passes the round trip.

   char and an enum have a set of values rather than a width, so each
   replaces the two tests with one of its own. */
static void narrow_check(struct lowerer *l, const struct expr *e,
                         const struct type *from, const struct type *to,
                         struct ir_operand v)
{
    const struct type *source_form = integer_form(from);
    enum ir_type source = lower_ir_type_of(from);
    enum ir_type target = lower_ir_type_of(to);
    enum check_kind kind =
        type_is_signed(source_form) ? CHECK_VALUE : CHECK_VALUE_U;
    bool sign_changes =
        type_is_signed(source_form) != type_is_signed(integer_form(to));
    struct text operation = {0};
    struct ir_operand ok;
    struct ir_operand round;

    if (from == to) {
        return;
    }
    text_append(&operation, to->kind == TYPE_ENUM ? "value not declared by "
                                                  : "value out of range for ");
    type_name(&operation, to);
    if (to->kind == TYPE_CHAR || to->kind == TYPE_ENUM) {
        struct ir_operand wide = lower_widen_operand(l, v, source_form);
        if (to->kind == TYPE_CHAR) {
            scalar_check(l, e, from, wide, text_cstr(&operation), kind, v);
        } else if (to->field_count > 0) {
            enum_check(l, e, from, to, wide, text_cstr(&operation), kind, v);
        }
        goto done;
    }
    if (sign_changes &&
        (type_is_signed(source_form) || lower_narrows(source, target))) {
        ok = lower_temp(l, ir_binary(l->f, l->b, IR_SGE, IR_I8, v,
                                     ir_int_op(source, 0)));
        lower_check_branch(l, ok, false,
                           lower_check_text(l, e->pos.line,
                                            text_cstr(&operation)), kind,
                           v, lower_none(), from);
    }
    if (source != target && lower_narrows(source, target)) {
        round = lower_temp(l, ir_unary(l->f, l->b, IR_TRUNC, target, v));
        round = lower_temp(
            l, ir_unary(l->f, l->b,
                        type_is_signed(integer_form(to)) ? IR_SEXT : IR_ZEXT,
                        source, round));
        ok = lower_temp(l, ir_binary(l->f, l->b, IR_EQ, IR_I8, round, v));
        lower_check_branch(l, ok, false,
                           lower_check_text(l, e->pos.line,
                                            text_cstr(&operation)), kind,
                           v, lower_none(), from);
    }
done:
    text_free(&operation);
}

/* The conversions of chapter 2. Two types with one IR type, such as u32
   and char, convert without an instruction. */
static struct ir_operand lower_cast(struct lowerer *l, const struct expr *e)
{
    const struct type *from = e->as.cast.operand->type;
    const struct type *to = e->type;
    enum ir_type source = lower_ir_type_of(from);
    enum ir_type target = lower_ir_type_of(to);
    struct ir_operand v = lower_expr(l, e->as.cast.operand);
    enum ir_op op;

    /* An f16 converts to and from an f32 alone, and to itself. */
    if (from->kind == TYPE_F16 || to->kind == TYPE_F16) {
        if (from->kind == to->kind) {
            return v;
        }
        op = to->kind == TYPE_F16 ? IR_HTRUNC : IR_HEXT;
        return lower_temp(l, ir_unary(l->f, l->b, op, target, v));
    }
    /* `v is Shape.Circle` compares the tag with the number of the case.
       The operand is a variant, so v is its address. */
    if (e->as.cast.variant_case != 0) {
        uint32_t index = e->as.cast.variant_case - 1;
        enum ir_type tag = lower_ir_type_of(from->base);
        return lower_temp(
            l, ir_binary(l->f, l->b, IR_EQ, IR_I8, lower_load_tag(l, from, v),
                         ir_int_op(tag, from->base->fields[index].number)));
    }
    /* A class test, and a conversion down a chain, which needs a check.
       A conversion up a chain is the same address and needs none. */
    if (e->as.cast.test) {
        return class_test(l, v, from, e->as.cast.target);
    }
    if (from->kind == TYPE_POINTER && from->element->kind == TYPE_CLASS &&
        to->kind == TYPE_POINTER && to->element->kind == TYPE_CLASS) {
        /* A conversion up a chain is the same address. One that leaves
           an interface sub-object moves back to the object, whatever the
           depths say, because the two chains are unrelated. */
        if (!e->as.cast.from_sub &&
            lower_class_depth(to->element) <=
                lower_class_depth(from->element)) {
            return v;
        }
        return checked_cast(l, v, from, to->element, e->as.cast.checked,
                            e->as.cast.from_sub);
    }
    if (converts_as_integer(from) && converts_as_integer(to)) {
        narrow_check(l, e, from, to, v);
    }
    if (type_is_float(from) && type_is_float(to)) {
        if (source == target) {
            return v;
        }
        op = target == IR_F64 ? IR_FEXT : IR_FTRUNC;
    } else if (type_is_float(from)) {
        op = type_is_signed(to) ? IR_FTOSI : IR_FTOUI;
    } else if (type_is_float(to)) {
        op = type_is_signed(from) ? IR_SITOF : IR_UITOF;
    } else if (source == target) {
        return v;
    } else if (lower_narrows(source, target)) {
        op = IR_TRUNC;
    } else {
        op = type_is_signed(from) ? IR_SEXT : IR_ZEXT;
    }
    return lower_temp(l, ir_unary(l->f, l->b, op, target, v));
}

/* `a <<% n` of symbolic values, with the mask of shift_wrap. */
static uint32_t shift_wrap_sym(struct lowerer *l, const struct symbolic *s,
                               uint32_t value, uint32_t count)
{
    enum ir_type type = lower_ir_type_of(s->type);
    uint32_t wide = ir_sym_op(l->m, type_is_signed(s->b->type) ? IR_SEXT
                                                                : IR_ZEXT,
                              IR_I64, count, IR_NO_AGG);
    uint32_t size = ir_sym_size_of(l->m, lower_vtype_of(l, s->type));
    uint32_t width =
        ir_sym_op(l->m, IR_MUL, IR_I64, size, ir_sym_int(l->m, IR_I64, 8));
    uint32_t inside = ir_sym_op(l->m, IR_ULT, IR_I8, wide, width);
    uint32_t shifted = ir_sym_op(l->m, IR_SHL, type, value, count);
    uint32_t one = ir_sym_op(l->m, IR_ZEXT, type, inside, IR_NO_AGG);
    uint32_t mask = ir_sym_op(l->m, IR_NEG, type, one, IR_NO_AGG);

    return ir_sym_op(l->m, IR_AND, type, shifted, mask);
}

/* The IR form of a symbolic value: the operations of lower_binary and
   lower_cast on symbolic operands. */
uint32_t lower_sym_of(struct lowerer *l, const struct symbolic *s)
{
    enum ir_type type = lower_ir_type_of(s->type);
    enum ir_type source;
    uint32_t a = 0;

    switch (s->kind) {
    case SYMBOLIC_INT:
        return ir_sym_int(l->m, type, s->value);
    case SYMBOLIC_SIZE_OF:
        return ir_sym_size_of(l->m, lower_vtype_of(l, s->of));
    case SYMBOLIC_UNARY:
        a = lower_sym_of(l, s->a);
        if (s->op == TOKEN_BANG) {
            return ir_sym_op(l->m, IR_XOR, IR_I8, a,
                             ir_sym_int(l->m, IR_I8, 1));
        }
        return ir_sym_op(l->m, s->op == TOKEN_MINUS ? IR_NEG : IR_NOT, type, a,
                         IR_NO_AGG);
    case SYMBOLIC_BINARY:
        a = lower_sym_of(l, s->a);
        if (s->op == TOKEN_SHL_WRAP) {
            return shift_wrap_sym(l, s, a, lower_sym_of(l, s->b));
        }
        if (s->op == TOKEN_AND_AND || s->op == TOKEN_OR_OR) {
            return ir_sym_op(l->m, s->op == TOKEN_AND_AND ? IR_AND : IR_OR,
                             IR_I8, a, lower_sym_of(l, s->b));
        }
        return ir_sym_op(l->m, lower_binary_op(s->op, s->a->type),
                         lower_is_comparison(s->op) ? IR_I8 : type, a,
                         lower_sym_of(l, s->b));
    case SYMBOLIC_CAST:
        a = lower_sym_of(l, s->a);
        source = lower_ir_type_of(s->a->type);
        if (source == type) {
            return a;
        }
        return ir_sym_op(l->m,
                         lower_narrows(source, type)        ? IR_TRUNC
                         : type_is_signed(s->a->type) ? IR_SEXT
                                                      : IR_ZEXT,
                         type, a, IR_NO_AGG);
    case SYMBOLIC_PARAM:
        /* A copy of a generic replaces every parameter before lowering,
           and the driver refuses a program that needs one, so no
           parameter reaches this switch. */
        break;
    }
    return a;
}

/* DESIGN: an argument that moves the error a handler binds into an `own`
   parameter reads the error, then writes `none` into the handler's copy.
   The delete that every exit of the handler runs then passes over it. Any
   other local that moves is handed over by `lower_move_argument`. */
struct ir_operand lower_argument(struct lowerer *l,
                                 const struct expr *arg)
{
    struct ir_operand value = lower_expr(l, arg);

    if (!arg->moves || l->b == NULL) {
        return value;
    }
    if (!arg->symbol->caught) {
        return lower_move_argument(l, arg, value);
    }
    value = lower_temp(l, ir_unary(l->f, l->b, IR_COPY, IR_PTR, value));
    ir_assign(l->f, l->b, arg->symbol->ir, ir_int_op(IR_PTR, 0));
    return value;
}

/* The parameters that the dispatched function of the call e declares,
   `self` among them. The out pointer of a `may fail` call is none of
   them, so the count comes from the callee where there is one. */
static size_t dispatched_params(const struct expr *e,
                                const struct symbol *sym, size_t given)
{
    if (sym != NULL && sym->type != NULL) {
        return sym->type->param_count;
    }
    return e->as.call.out != NULL ? given - 1 : given;
}

struct ir_operand lower_call(struct lowerer *l, const struct expr *e)
{
    const struct expr *callee = e->as.call.callee;
    const struct symbol *sym = callee->kind == EXPR_NAME ? callee->symbol
                                                         : NULL;
    bool direct = sym != NULL && (sym->kind == SYMBOL_FN ||
                                  sym->kind == SYMBOL_EXTERN_FN);
    size_t n = e->as.call.arg_count;
    const struct type *fn = callee->type != NULL &&
                                    callee->type->kind == TYPE_FN
                                ? callee->type
                                : NULL;
    /* DESIGN: the place this call writes its result to is read before
       the arguments are lowered. A failing call among them sets the out
       address of its own slot, which is no place of this call. */
    struct ir_operand out = l->out_address;
    struct ir_operand target;
    struct ir_operand bound = lower_none();
    struct ir_operand context = lower_none();
    struct ir_operand *args;
    uint32_t result;
    uint32_t slot;
    enum ir_type declared;
    size_t given;
    size_t i;

    if (e->as.call.hashes) {
        return lower_hash(l, e);
    }
    target = lower_none();
    /* The callee comes before the arguments, from left to right. A
       bound function gives its object as the first argument and its
       entry as the target. A function with its context gives its code
       as the target and its context as the last argument. */
    if (lower_is_context(fn)) {
        struct ir_operand value = lower_address(l, callee);
        target = lower_temp(l, ir_load(l->f, l->b, IR_PTR, value));
        context = lower_temp(
            l, ir_load(l->f, l->b, IR_PTR,
                       lower_offset_address(
                           l, value,
                           ir_sym_operand(l->m,
                                          ir_sym_offset_of(
                                              l->m, lower_agg_of(l, fn),
                                              1)))));
        direct = false;
    } else if (callee->type != NULL && callee->type->kind == TYPE_FN &&
        callee->type->bound) {
        struct ir_operand value = lower_address(l, callee);
        bound = lower_temp(l, ir_load(l->f, l->b, IR_PTR, value));
        target = lower_temp(
            l, ir_load(l->f, l->b, IR_PTR,
                       lower_offset_address(
                           l, value,
                           lower_field_offset(l, callee->type,
                                              &lower_entry_name))));
        direct = false;
    } else if (!direct) {
        target = lower_expr(l, callee);
    }
    args = ir_alloc(2 * n + 3, sizeof *args);
    given = 0;
    if (bound.kind != IR_NONE) {
        args[given++] = bound;
    }
    /* An argument at a parameter that does not keep it passes as two
       words. */
    for (i = 0; i < n; i++) {
        struct ir_operand value = lower_argument(l, e->as.call.args[i]);
        lower_push_argument(l, args, &given, value,
                            fn != NULL && i < fn->param_count ? fn->params[i]
                                                              : NULL);
    }
    if (bound.kind != IR_NONE) {
        n++;
    }
    if (e->as.call.out != NULL) {
        args[given++] = out;
        n++;
    }
    if (context.kind != IR_NONE) {
        args[given++] = context;
    }
    /* DESIGN: a call through the table loads the table pointer from the
       object, which is its first word, then the entry of the function.
       The index is the same in every class of a chain, so the entry the
       concrete class filled is the one this call reads. The call names
       the class and the index, so the passes over the whole program see
       which entries it may reach. */
    slot = 0;
    if (e->as.call.dispatch != NULL && n > 0) {
        size_t index = lower_table_index(e->as.call.dispatch, &e->as.call.entry,
                                         dispatched_params(e, sym, n));
        if (index > 0) {
            slot = (uint32_t)index;
            struct ir_operand table =
                lower_load_table(l, args[0], e->as.call.dispatch);
            target = lower_temp(
                l, ir_load(l->f, l->b, IR_PTR,
                           lower_offset_address(
                               l, table, lower_entry_offset(l, index))));
            direct = false;
        }
    }
    /* A call whose error a handler takes gives the error pointer here.
       The value of the expression is what the out parameter received, so
       the IR type comes from the function and not from the node. */
    declared = callee->type != NULL && callee->type->kind == TYPE_FN
                   ? lower_ir_type_of(callee->type->result)
                   : lower_ir_type_of(e->type);
    if (e->as.call.handler.kind == HANDLE_NONE) {
        declared = lower_ir_type_of(e->type);
    }
    if (direct) {
        result = ir_call(l->f, l->b, declared,
                         ir_func_op(lower_callee_function(l, sym)), args,
                         given);
    } else {
        result = ir_call_indirect(l->f, l->b, declared, target,
                                  bound.kind != IR_NONE
                                      ? lower_bound_signature(l, callee->type)
                                  : context.kind != IR_NONE
                                      ? lower_context_signature(l,
                                                                callee->type)
                                      : lower_signature(l, callee->type),
                                  args, given);
        if (slot > 0) {
            struct ir_inst *call = &l->b->insts[l->b->count - 1];
            call->c = ir_global_op(lower_class_descriptor(l,
                                                          e->as.call.dispatch));
            call->field = slot;
        }
    }
    free(args);
    return result == IR_NO_RESULT ? lower_none() : lower_temp(l, result);
}

/* Threads */

/* The count of `parallel` thunks already written here, which names the
   next one. */
static size_t next_thunk(const struct lowerer *l, const char *prefix)
{
    size_t length = strlen(prefix);
    size_t count = 0;
    size_t i;

    for (i = 0; i < l->m->function_count; i++) {
        const struct ir_function *g = l->m->functions[i];
        if (g->module != NULL && strcmp(g->module, l->module_name) == 0 &&
            strncmp(g->name, prefix, length) == 0) {
            count++;
        }
    }
    return count;
}

/* An extern declaration of a runtime function whose parameters are all
   scalars. A module that calls it twice shares the declaration. */
struct ir_function *lower_rt_function_giving(struct lowerer *l,
                                             const char *name,
                                             enum ir_type result,
                                             const enum ir_type *params,
                                             size_t count)
{
    struct ir_function *f = lower_find_function(l->m, NULL, name);
    size_t i;

    if (f == NULL) {
        f = ir_extern_add(l->m, name, result, false);
        for (i = 0; i < count; i++) {
            ir_param_add(f, params[i], IR_NO_AGG);
        }
    }
    return f;
}

struct ir_function *lower_rt_function(struct lowerer *l, const char *name,
                                      const enum ir_type *params,
                                      size_t count)
{
    return lower_rt_function_giving(l, name, IR_VOID, params, count);
}

/* The address of the descriptor of the class that t is or points at, or
   zero for any other type. The runtime names that class when the table
   of the object is zero. */
struct ir_operand lower_static_descriptor(struct lowerer *l,
                                          const struct type *t)
{
    if (t != NULL && t->kind == TYPE_POINTER) {
        t = t->element;
    }
    if (t == NULL || t->kind != TYPE_CLASS) {
        return ir_int_op(IR_PTR, 0);
    }
    return lower_temp(l,
                      ir_addr(l->f, l->b,
                              ir_global_op(lower_class_descriptor(l, t))));
}

/* A call of anti_rt_delete, anti_rt_destroy or anti_rt_dup on object,
   which the program holds as a t. Only dup gives a value. */
struct ir_operand lower_object_call(struct lowerer *l, const char *name,
                                    struct ir_operand object,
                                    const struct type *t)
{
    static const enum ir_type params[] = {IR_PTR, IR_PTR};
    enum ir_type result =
        strcmp(name, "anti_rt_dup") == 0 ? IR_PTR : IR_VOID;
    struct ir_operand args[2];
    struct ir_function *f;
    uint32_t call;

    args[0] = object;
    args[1] = lower_static_descriptor(l, t);
    f = lower_rt_function_giving(l, name, result, params, 2);
    call = ir_call(l->f, l->b, result, ir_func_op(f), args, 2);
    return result == IR_PTR ? lower_temp(l, call) : lower_none();
}

/* The aggregate that carries the arguments every chunk receives, or
   IR_NO_AGG when the worker takes the chunk alone. */
/* The type of parameter i + 1 of the worker that call names, which holds
   argument i of the call. */
static const struct type *worker_param(const struct expr *call, size_t i)
{
    return call->as.call.callee->symbol->type->params[i + 1];
}

static uint32_t context_aggregate(struct lowerer *l, const struct expr *call,
                                  const char *name)
{
    struct ir_field *fields;
    uint32_t agg;
    size_t n = call->as.call.arg_count;
    size_t i;

    fields = ir_alloc(n, sizeof *fields);
    for (i = 0; i < n; i++) {
        char *field = ir_alloc(24, 1);
        snprintf(field, 24, "a%zu", i);
        fields[i].name = field;
        fields[i].type = lower_vtype_of(l, worker_param(call, i));
        fields[i].bits = 0;
        fields[i].ext = IR_EXT_NONE;
    }
    agg = ir_struct_add(l->m, IR_AGG_STRUCT, name, fields, n, false, 0);
    for (i = 0; i < n; i++) {
        free((char *)fields[i].name);
    }
    free(fields);
    return agg;
}

/* Pack the arguments of call into a context in a slot of the frame,
   named `<name>.context`. Returns the address of the slot, or a null
   pointer for a call without arguments, and *agg receives the aggregate
   of the context or IR_NO_AGG. */
static struct ir_operand pack_context(struct lowerer *l,
                                      const struct expr *call, size_t extra,
                                      const char *name, uint32_t *agg)
{
    struct text context_name = {0};
    uint32_t slot;
    size_t i;

    *agg = IR_NO_AGG;
    if (extra == 0) {
        return ir_int_op(IR_PTR, 0);
    }
    text_appendf(&context_name, "%s.context", name);
    *agg = context_aggregate(l, call, text_cstr(&context_name));
    text_free(&context_name);
    slot = ir_slot(l->f, l->b, ir_aggregate(*agg));
    for (i = 0; i < extra; i++) {
        const struct expr *arg = call->as.call.args[i];
        struct ir_operand at =
            lower_offset_address(l, lower_temp(l, slot),
                                 ir_sym_operand(l->m,
                                                ir_sym_offset_of(l->m, *agg,
                                                                 (uint32_t)i)));
        lower_store_value(l, worker_param(call, i), arg, at);
    }
    return lower_temp(l, slot);
}

/* The body the two thunks share, written into the entry block of the
   thunk the lowerer is in. It calls the worker of call with first and
   the arguments the context of parameter 0 holds. What the worker gives
   goes to the address in parameter out. */
static void call_worker(struct lowerer *l, const struct expr *call,
                        uint32_t context, struct ir_operand first,
                        size_t out)
{
    const struct expr *callee = call->kind == EXPR_CALL
                                    ? call->as.call.callee : call;
    const struct type *result = callee->symbol->type->result;
    size_t extra = call->kind == EXPR_CALL ? call->as.call.arg_count : 0;
    struct ir_function *f = l->f;
    struct ir_block *entry = l->b;
    struct ir_operand *args = ir_alloc(2 * extra + 1, sizeof *args);
    struct ir_operand at_out;
    uint32_t value;
    size_t count = 1;
    size_t i;

    args[0] = first;
    for (i = 0; i < extra; i++) {
        const struct type *t = worker_param(call, i);
        struct ir_operand at =
            lower_offset_address(l, lower_temp(l, f->params[0].temp),
                                 ir_sym_operand(l->m,
                                                ir_sym_offset_of(l->m, context,
                                                                 (uint32_t)i)));
        lower_push_argument(
            l, args, &count,
            lower_is_aggregate(t)
                ? at
                : lower_temp(l, ir_load(f, entry, lower_ir_type_of(t), at)),
            t);
    }
    value = ir_call(f, entry, lower_ir_type_of(result),
                    ir_func_op(lower_callee_function(l, callee->symbol)), args,
                    count);
    free(args);
    at_out = lower_temp(l, f->params[out].temp);
    if (result->kind == TYPE_VOID) {
        /* A worker without a result writes nothing. */
    } else if (lower_is_aggregate(result)) {
        ir_memcopy(f, entry, at_out, lower_temp(l, value),
                   lower_vtype_of(l, result));
    } else {
        ir_store(f, entry, lower_ir_type_of(result), lower_temp(l, value),
                 at_out);
    }
    ir_ret(f, entry, IR_VOID, lower_none());
}

/* DESIGN: the worker pool calls one C signature, and a worker has the
   signature its own declaration gives. antic writes a thunk for each
   `parallel` that joins the two. The thunk rebuilds the chunk from the
   pointer and the length. It then reads the arguments that every chunk
   shares out of the context, calls the worker and stores its result. */
static struct ir_function *parallel_thunk(struct lowerer *l,
                                          const struct expr *e,
                                          const char *name, uint32_t context)
{
    const struct type *slice = e->as.parallel.array->type;
    struct ir_function *outer_f = l->f;
    struct ir_block *outer_b = l->b;
    struct ir_function *f;
    struct ir_block *entry;
    uint32_t slot;

    f = ir_function_add(l->m, l->module_name, name, IR_VOID, IR_NO_AGG);
    ir_param_add(f, IR_PTR, IR_NO_AGG);     /* the context */
    ir_param_add(f, IR_PTR, IR_NO_AGG);     /* the first element */
    ir_param_add(f, IR_I64, IR_NO_AGG);     /* the element count */
    ir_param_add(f, IR_PTR, IR_NO_AGG);     /* where the result goes */
    entry = ir_block_add(f);
    l->f = f;
    l->b = entry;

    slot = ir_slot(f, entry, lower_vtype_of(l, slice));
    ir_store(f, entry, IR_PTR, lower_temp(l, f->params[1].temp),
             lower_temp(l, slot));
    ir_store(f, entry, IR_I64, lower_temp(l, f->params[2].temp),
             lower_offset_address(l, lower_temp(l, slot),
                                  lower_field_offset(l, slice,
                                                     &lower_len_name)));
    call_worker(l, e->as.parallel.call, context, lower_temp(l, slot), 3);
    l->f = outer_f;
    l->b = outer_b;
    return f;
}

/* DESIGN: the thunk of a dispatch has the shape the runtime calls: the
   context, the object and the address of the result. It unpacks the
   context and calls the worker with the object first. */
static struct ir_function *dispatch_thunk(struct lowerer *l,
                                          const struct expr *e,
                                          const char *name, uint32_t context)
{
    struct ir_function *outer_f = l->f;
    struct ir_block *outer_b = l->b;
    struct ir_function *f;

    f = ir_function_add(l->m, l->module_name, name, IR_VOID, IR_NO_AGG);
    ir_param_add(f, IR_PTR, IR_NO_AGG);     /* the context */
    ir_param_add(f, IR_PTR, IR_NO_AGG);     /* the object */
    ir_param_add(f, IR_PTR, IR_NO_AGG);     /* where the result goes */
    l->f = f;
    l->b = ir_block_add(f);
    call_worker(l, e->as.dispatch.call, context,
                lower_temp(l, f->params[1].temp),
                2);
    l->f = outer_f;
    l->b = outer_b;
    return f;
}

/* DESIGN: `dispatch` is one call of the runtime with the object, the size
   of the result, the thunk and the context. The runtime gives a handle
   back, which the Job holds in its one field. */
static struct ir_operand lower_dispatch(struct lowerer *l,
                                        const struct expr *e)
{
    static const enum ir_type signature[] = {IR_PTR, IR_I64, IR_PTR, IR_PTR};
    const struct expr *call = e->as.dispatch.call;
    const struct expr *callee = call->kind == EXPR_CALL
                                    ? call->as.call.callee : call;
    const struct type *result = callee->symbol->type->result;
    size_t extra = call->kind == EXPR_CALL ? call->as.call.arg_count : 0;
    struct ir_operand context;
    struct ir_operand args[4];
    struct ir_function *f;
    uint32_t agg;
    uint32_t handle;
    uint32_t out;
    char name[32];

    snprintf(name, sizeof name, "dispatch.%zu", next_thunk(l, "dispatch."));
    args[0] = lower_expr(l, e->as.dispatch.object);
    context = pack_context(l, call, extra, name, &agg);
    args[1] = result->kind == TYPE_VOID ? ir_int_op(IR_I64, 0)
                                        : lower_size_operand(l, result);
    f = dispatch_thunk(l, e, name, agg);
    args[2] = lower_temp(l, ir_addr(l->f, l->b, ir_func_op(f)));
    args[3] = context;
    handle = ir_call(l->f, l->b, IR_PTR,
                     ir_func_op(lower_rt_function(l, "anti_rt_dispatch",
                                                  signature,
                                                  4)),
                     args, 4);
    lower_hook_object(l, HOOK_DISPATCHED, args[0]);
    out = ir_slot(l->f, l->b, lower_vtype_of(l, e->type));
    ir_store(l->f, l->b, IR_PTR, lower_temp(l, handle), lower_temp(l, out));
    return lower_temp(l, out);
}

/* `join` waits for one job and writes its result into a slot, and
   `join_all` walks the slice of jobs. */
static struct ir_operand lower_join(struct lowerer *l, const struct expr *e)
{
    static const enum ir_type one[] = {IR_PTR, IR_I64, IR_PTR};
    static const enum ir_type all[] = {IR_PTR, IR_I64};
    const struct type *job = e->as.join.job->type;
    struct ir_operand value = lower_address(l, e->as.join.job);
    struct ir_operand args[3];
    uint32_t out;

    if (e->as.join.all) {
        args[0] = lower_temp(l, ir_load(l->f, l->b, IR_PTR, value));
        args[1] = lower_temp(
            l, ir_load(l->f, l->b, IR_I64,
                       lower_offset_address(
                           l, value,
                           lower_field_offset(l, job, &lower_len_name))));
        ir_call(l->f, l->b, IR_VOID,
                ir_func_op(lower_rt_function(
                    l,
                    l->hooks ? "anti_rt_join_all_hooked" : "anti_rt_join_all",
                    all, 2)),
                args, 2);
        return lower_none();
    }
    args[0] = lower_temp(l, ir_load(l->f, l->b, IR_PTR, value));
    if (e->type->kind == TYPE_VOID) {
        args[1] = ir_int_op(IR_I64, 0);
        args[2] = ir_int_op(IR_PTR, 0);
        ir_call(l->f, l->b, IR_VOID,
                ir_func_op(lower_rt_function(
                    l, l->hooks ? "anti_rt_join_hooked" : "anti_rt_join",
                    one, 3)),
                args, 3);
        return lower_none();
    }
    out = ir_slot(l->f, l->b, lower_vtype_of(l, e->type));
    args[1] = lower_size_operand(l, e->type);
    args[2] = lower_temp(l, out);
    ir_call(l->f, l->b, IR_VOID,
            ir_func_op(lower_rt_function(
                l, l->hooks ? "anti_rt_join_hooked" : "anti_rt_join", one, 3)),
            args, 3);
    if (lower_is_aggregate(e->type)) {
        return lower_temp(l, out);
    }
    return lower_temp(l,
                      ir_load(l->f, l->b, lower_ir_type_of(e->type),
                              lower_temp(l, out)));
}

/* DESIGN: `parallel a by n -> f(x)` becomes one call of the runtime. The
   runtime decides the chunk count when n is absent. It therefore
   allocates the array of results and writes back the pointer and the
   count, and the expression is the slice of those results. */
static struct ir_operand lower_parallel(struct lowerer *l,
                                        const struct expr *e)
{
    static const enum ir_type signature[] = {IR_PTR, IR_I64, IR_I64, IR_I64,
                                             IR_I64, IR_PTR, IR_PTR, IR_PTR,
                                             IR_PTR};
    const struct expr *call = e->as.parallel.call;
    const struct expr *callee = call->kind == EXPR_CALL
                                    ? call->as.call.callee : call;
    const struct type *slice = e->as.parallel.array->type;
    const struct type *result = callee->symbol->type->result;
    size_t extra = call->kind == EXPR_CALL ? call->as.call.arg_count : 0;
    struct ir_operand context;
    struct ir_operand args[9];
    struct ir_operand array;
    struct ir_operand length;
    struct ir_operand length_at;
    uint32_t agg;
    uint32_t results;
    uint32_t count;
    uint32_t out;
    char name[32];

    snprintf(name, sizeof name, "parallel.%zu", next_thunk(l, "parallel."));
    array = lower_address(l, e->as.parallel.array);
    context = pack_context(l, call, extra, name, &agg);
    results = ir_slot(l->f, l->b, ir_scalar(IR_PTR));
    count = ir_slot(l->f, l->b, ir_scalar(IR_I64));
    args[0] = lower_temp(l, ir_load(l->f, l->b, IR_PTR, array));
    args[1] = lower_temp(
        l, ir_load(l->f, l->b, IR_I64,
                   lower_offset_address(
                       l, array,
                       lower_field_offset(l, slice, &lower_len_name))));
    args[2] = lower_size_operand(l, slice->element);
    args[3] = e->as.parallel.chunks != NULL
                  ? lower_expr(l, e->as.parallel.chunks)
                  : ir_int_op(IR_I64, 0);
    args[4] = lower_size_operand(l, result);
    args[5] = lower_temp(l, ir_addr(l->f, l->b,
                                    ir_func_op(parallel_thunk(l, e, name,
                                                              agg))));
    args[6] = context;
    args[7] = lower_temp(l, results);
    args[8] = lower_temp(l, count);
    ir_call(l->f, l->b, IR_VOID,
            ir_func_op(lower_rt_function(l, "anti_rt_parallel", signature, 9)),
            args, 9);
    out = ir_slot(l->f, l->b, lower_vtype_of(l, e->type));
    ir_store(l->f, l->b, IR_PTR, lower_temp(l, ir_load(l->f, l->b, IR_PTR,
                                                       lower_temp(l, results))),
             lower_temp(l, out));
    length = lower_temp(l, ir_load(l->f, l->b, IR_I64, lower_temp(l, count)));
    length_at = lower_offset_address(l, lower_temp(l, out),
                                     lower_field_offset(l, e->type,
                                                        &lower_len_name));
    ir_store(l->f, l->b, IR_I64, length, length_at);
    return lower_temp(l, out);
}

/* Locking and channels */

/* DESIGN: a channel is a struct of one handle, which names the object
   the runtime made. Each operation passes the handle to a function of
   the runtime, which holds the queue. A Mutex is the lock word itself,
   and each operation passes its address. A value that `send` puts and `recv` takes goes through a slot
   of the frame. The IR then names no size but the one of the element
   type. */

/* The handle that the channel e holds. A pointer to a channel points at
   its handle. */
struct ir_operand lower_load_handle(struct lowerer *l, const struct expr *e)
{
    struct ir_operand at = e->type->kind == TYPE_POINTER ? lower_expr(l, e)
                                                          : lower_address(l, e);

    return lower_temp(l, ir_load(l->f, l->b, IR_PTR, at));
}

/* A call of the runtime function name on count arguments. */
struct ir_operand lower_sync_call(struct lowerer *l, const char *name,
                                  enum ir_type result,
                                  const enum ir_type *params,
                                  const struct ir_operand *args,
                                  size_t count)
{
    struct ir_function *f = lower_rt_function_giving(l, name, result, params,
                                                     count);
    uint32_t value = ir_call(l->f, l->b, result, ir_func_op(f), args, count);

    return result == IR_VOID ? lower_none() : lower_temp(l, value);
}

static struct ir_operand lower_sync_op(struct lowerer *l,
                                       const struct expr *e)
{
    static const enum ir_type one[] = {IR_PTR};
    static const enum ir_type two[] = {IR_PTR, IR_PTR};
    static const enum ir_type sizes[] = {IR_I64, IR_I64};
    const struct expr *target = e->as.sync_op.target;
    const struct type *element = NULL;
    struct ir_operand args[2];
    struct ir_operand handle;
    uint32_t slot;

    switch (e->as.sync_op.op) {
    /* A new Mutex is the zero word, which is unlocked on every
       system. */
    case SYNC_MUTEX_NEW:
        slot = ir_entry_slot(l->f, lower_vtype_of(l, e->type));
        ir_store(l->f, l->b, IR_LOCK, ir_int_op(IR_LOCK, 0),
                 lower_temp(l, slot));
        return lower_temp(l, slot);
    case SYNC_CHAN_NEW:
        args[0] = lower_size_operand(l, e->type->element);
        args[1] = lower_expr(l, e->as.sync_op.value);
        handle = lower_sync_call(l, "anti_rt_chan_new", IR_PTR, sizes, args, 2);
        break;
    /* The word holds nothing of the system, and a dev build forgets the
       orders it recorded for the lock. */
    case SYNC_MUTEX_DESTROY:
        args[0] = target->type->kind == TYPE_POINTER
                      ? lower_expr(l, target)
                      : lower_address(l, target);
        lower_sync_call(l, "anti_rt_mutex_destroy", IR_VOID, one, args, 1);
        return lower_none();
    case SYNC_SEND:
        element = target->type->element;
        args[0] = lower_load_handle(l, target);
        slot = ir_entry_slot(l->f, lower_vtype_of(l, element));
        lower_store_value(l, element, e->as.sync_op.value, lower_temp(l, slot));
        args[1] = lower_temp(l, slot);
        lower_sync_call(l, "anti_rt_chan_send", IR_VOID, two, args, 2);
        return lower_none();
    /* `recv` gives the address of the slot it filled, or `none` when
       the channel is closed and empty. */
    case SYNC_RECV:
        element = target->type->element;
        args[0] = lower_load_handle(l, target);
        slot = ir_entry_slot(l->f, lower_vtype_of(l, element));
        args[1] = lower_temp(l, slot);
        return lower_sync_call(l, "anti_rt_chan_recv", IR_PTR, two, args, 2);
    case SYNC_CLOSE:
    case SYNC_CHAN_DELETE:
        args[0] = lower_load_handle(l, target);
        lower_sync_call(l,
                        e->as.sync_op.op == SYNC_CLOSE ? "anti_rt_chan_close"
                                                       : "anti_rt_chan_delete",
                        IR_VOID, one, args, 1);
        return lower_none();
    }
    /* A new channel is a slot that holds the handle. */
    slot = ir_entry_slot(l->f, lower_vtype_of(l, e->type));
    ir_store(l->f, l->b, IR_PTR, handle, lower_temp(l, slot));
    return lower_temp(l, slot);
}

static struct ir_operand lower_expr_value(struct lowerer *l,
                                         const struct expr *e);


/* The `?T` of type t that holds v, a value of type value, in a slot of
   the frame. */
static struct ir_operand lower_wrap(struct lowerer *l, const struct type *t,
                                    const struct type *value,
                                    struct ir_operand v)
{
    struct ir_operand slot =
        lower_temp(l, ir_entry_slot(l->f, lower_vtype_of(l, t)));

    if (lower_is_aggregate(value)) {
        ir_memcopy(l->f, l->b, slot, v, lower_vtype_of(l, value));
    } else {
        ir_store(l->f, l->b, lower_ir_type_of(value), v, slot);
    }
    lower_set_optional(l, t, value, slot, 1);
    return slot;
}

struct ir_operand lower_expr(struct lowerer *l, const struct expr *e)
{
    struct ir_operand v = lower_converted(l, e);

    if (!wraps(e) || l->b == NULL) {
        return v;
    }
    return lower_wrap(l, e->to_optional, e->type, v);
}

/* DESIGN: a pointer to a class becomes a pointer to one of its
   interfaces by adding the offset of the sub-object. The checker marked
   the expression, and every place a value flows into an interface slot
   passes through here. */
static struct ir_operand lower_converted(struct lowerer *l,
                                         const struct expr *e)
{
    struct ir_operand v = lower_expr_value(l, e);

    /* A plain function where the form of two words is expected takes
       the context `none`. */
    if (e->to_context) {
        return lower_pair(l, e->type, v, ir_int_op(IR_PTR, 0));
    }
    /* DESIGN: a `keep own` parameter that moves into an owner hands over
       a copy of its two words and keeps `none` as its context. The free
       at the exits of the function then passes over the snapshot. */
    if (e->moves_snapshot) {
        struct ir_operand copy = lower_temp(
            l, ir_entry_slot(l->f, lower_vtype_of(l, e->type)));
        ir_memcopy(l->f, l->b, copy, v, lower_vtype_of(l, e->type));
        ir_store(l->f, l->b, IR_PTR, ir_int_op(IR_PTR, 0),
                 lower_context_word(l, e->type, v));
        return copy;
    }
    if (e->to_iface == NULL) {
        return v;
    }
    return lower_temp(l, ir_ptradd(l->f, l->b, v,
                                   lower_field_offset(l, e->to_iface->home,
                                                      &e->to_iface->name)));
}

static struct ir_operand lower_expr_value(struct lowerer *l,
                                          const struct expr *e)
{
    enum ir_type type;
    struct ir_operand v;
    struct place p;
    uint32_t size;

    if (lower_is_aggregate(e->type)) {
        return lower_address(l, e);
    }
    type = lower_ir_type_of(e->type);
    switch (e->kind) {
    case EXPR_INT:
        return ir_int_op(type, e->as.integer);
    case EXPR_FLOAT:
        return ir_float_op(type, lower_float_literal(e, type));
    case EXPR_CHAR:
        return ir_int_op(type, e->as.character);
    case EXPR_BOOL:
        return ir_int_op(type, e->as.boolean);
    case EXPR_NONE:
        return ir_int_op(type, 0);
    case EXPR_FN:
        return lower_closure(l, e);
    case EXPR_NAME:
        return lower_name(l, e);
    case EXPR_UNARY:
        return lower_unary(l, e);
    case EXPR_BINARY:
        return lower_binary(l, e);
    case EXPR_CAST:
        return lower_cast(l, e);
    case EXPR_CALL:
        if (lower_is_handled_call(e)) {
            return handled_operand(l, e);
        }
        return lower_call(l, e);
    case EXPR_PARALLEL:
        return lower_parallel(l, e);
    case EXPR_DISPATCH:
        return lower_dispatch(l, e);
    case EXPR_JOIN:
        return lower_join(l, e);
    case EXPR_SYNC_OP:
        return lower_sync_op(l, e);
    case EXPR_SIMD:
        return lower_simd(l, e);
    /* The address of the descriptor of a class, which `lib.instance(I)`
       and `lib.supports(I, f)` pass to the loader. */
    case EXPR_DESCRIPTOR:
        return lower_temp(l, ir_addr(l->f, l->b,
                                     ir_global_op(lower_class_descriptor(
                                         l, e->as.descriptor_of))));
    case EXPR_FIELD:
        if (e->symbol != NULL && e->symbol->kind == SYMBOL_CONST) {
            return lower_constant(l, e->symbol->value, type);
        }
        /* A bound function is built into a slot, like any aggregate. */
        if (e->type != NULL && e->type->kind == TYPE_FN && e->type->bound) {
            return lower_address(l, e);
        }
        if (e->symbol != NULL && e->as.field.through != NULL) {
            return lower_temp(
                l, ir_addr(l->f, l->b,
                           ir_func_op(lower_reach_thunk(
                               l, e->as.field.through, e->symbol))));
        }
        if (e->symbol != NULL && (e->symbol->kind == SYMBOL_FN ||
                                  e->symbol->kind == SYMBOL_EXTERN_FN)) {
            return lower_temp(
                l, ir_addr(l->f, l->b,
                           ir_func_op(lower_callee_function(l, e->symbol))));
        }
        /* A value of an enum is the number the checker folded, in the
           underlying type of the enum. */
        if (e->as.field.enum_value != 0 && e->type->kind == TYPE_ENUM) {
            return ir_int_op(lower_ir_type_of(e->type->base),
                             e->type->fields[e->as.field.enum_value - 1]
                                 .number);
        }
        if (e->as.field.base->type->kind == TYPE_ARRAY) {
            /* .len is the only field of an array. */
            const struct type *array = e->as.field.base->type;
            lower_expr(l, e->as.field.base);
            return array->length_of != NULL
                       ? ir_sym_operand(l->m, lower_sym_of(l, array->length_of))
                       : ir_int_op(IR_I64, array->length);
        }
        return lower_place(l, e, &p) ? lower_read_place(l, &p) : lower_none();
    case EXPR_INDEX:
        return lower_place(l, e, &p) ? lower_read_place(l, &p) : lower_none();
    case EXPR_ALLOC:
        /* One object of the literal's type, with the literal written
           into it. The count form multiplies by the element size. */
        if (e->as.alloc.value != NULL) {
            v = lower_size_operand(l, e->type->element);
            v = lower_temp(l, ir_call(l->f, l->b, IR_PTR,
                                      ir_func_op(lower_c_function(l, "malloc",
                                                                  IR_PTR,
                                                                  IR_I64)),
                                      &v, 1));
            lower_build_into(l, e->as.alloc.value, v);
            return v;
        }
        v = lower_expr(l, e->as.alloc.count);
        /* DESIGN: the elements of a class come zeroed, so one the
           program has not filled has a zero table, which the zero-table
           check reports. Other elements are C's and keep malloc. */
        if (e->type->element->kind == TYPE_CLASS) {
            struct ir_operand args[2];
            args[0] = v;
            args[1] = lower_size_operand(l, e->type->element);
            return lower_temp(l, ir_call(l->f, l->b, IR_PTR,
                                         ir_func_op(lower_calloc_function(l)),
                                         args, 2));
        }
        size = ir_binary(l->f, l->b, IR_MUL, IR_I64, v,
                         lower_size_operand(l, e->type->element));
        v = lower_temp(l, size);
        return lower_temp(l, ir_call(l->f, l->b, IR_PTR,
                                     ir_func_op(lower_c_function(l, "malloc",
                                                                 IR_PTR,
                                                                 IR_I64)),
                                     &v, 1));
    case EXPR_FREE:
        v = lower_expr(l, e->as.free_pointer);
        ir_call(l->f, l->b, IR_VOID,
                ir_func_op(lower_c_function(l, "free", IR_VOID, IR_PTR)), &v,
                1);
        return lower_none();
    /* DESIGN: an atomic operation is a call of the runtime, which holds
       one body per operation and switches on the width. The runtime is
       compiled per target with optimisation, so each body is the
       instruction the target gives: `lock xadd` and `lock cmpxchg` on
       x86_64, `ldaddal` and `casal` on an ARM64 with LSE, and a
       load-store-exclusive loop on one without it. */
    case EXPR_ATOMIC: {
        static const char *const names[] = {
            "anti_rt_atomic_load", "anti_rt_atomic_store",
            "anti_rt_atomic_swap", "anti_rt_atomic_add",
            "anti_rt_atomic_sub", "anti_rt_atomic_and",
            "anti_rt_atomic_or", "anti_rt_atomic_compare_swap"
        };
        static const enum ir_type four[] = {IR_PTR, IR_I64, IR_I64, IR_I64};
        const struct type *of = e->as.atomic.place->type;
        /* The runtime gives an i64 back. A field of pointer width is
           the same bits, so the call takes that type and no conversion
           stands between them. */
        enum ir_type result = e->type->kind == TYPE_VOID ? IR_VOID
                              : e->as.atomic.op == ATOMIC_CAS ? IR_I8
                              : lower_ir_type_of(e->type) == IR_PTR ? IR_PTR
                                                              : IR_I64;
        struct ir_function *f;
        struct ir_operand args[4];
        size_t n = 2;
        uint32_t call;
        args[0] = lower_address(l, e->as.atomic.place);
        args[1] = lower_size_operand(l, of);
        if (e->as.atomic.a != NULL) {
            args[n++] = widen_to_i64(l, e->as.atomic.a);
        }
        if (e->as.atomic.b != NULL) {
            args[n++] = widen_to_i64(l, e->as.atomic.b);
        }
        f = lower_rt_function(l, names[e->as.atomic.op], four, n);
        f->result = result;
        call = ir_call(l->f, l->b, result, ir_func_op(f), args, n);
        if (result == IR_VOID) {
            return lower_none();
        }
        if (result == IR_I8 || result == IR_PTR) {
            return lower_temp(l, call);
        }
        return narrow_from_i64(l, lower_temp(l, call), e->type);
    }
    /* The three read the table of the object, so the runtime does the
       walk. The compiler passes the pointer and the class it has. */
    case EXPR_OBJECT: {
        const char *name = e->as.object.op == TOKEN_DUP    ? "anti_rt_dup"
                           : e->as.object.op == TOKEN_DELETE
                               ? "anti_rt_delete"
                               : "anti_rt_destroy";
        v = lower_expr(l, e->as.object.operand);
        if (e->as.object.from != NULL) {
            static const enum ir_type three[] = {IR_PTR, IR_PTR, IR_PTR};
            struct ir_operand args[3];
            args[0] = v;
            args[1] = lower_static_descriptor(l, e->as.object.operand->type);
            args[2] = lower_expr(l, e->as.object.from);
            return lower_rt_call(l,
                                 e->as.object.op == TOKEN_DELETE
                                     ? "anti_rt_delete_from"
                                     : "anti_rt_destroy_from",
                                 IR_VOID, three, args, 3);
        }
        if (e->as.object.op == TOKEN_DUP) {
            struct ir_operand made =
                lower_object_call(l, name, v, e->as.object.operand->type);
            lower_hook_copied(l, made, v);
            return made;
        }
        return lower_object_call(l, name, v, e->as.object.operand->type);
    }
    case EXPR_SIZE_OF:
        return lower_size_operand(l, e->as.size_of->type);
    case EXPR_OPTIONAL:
        return lower_optional(l, e);
    /* The value once, then the test the checker wrote over it. */
    case EXPR_IN:
        v = lower_expr(l, e->as.in.value);
        lower_bind_value(l, e->as.in.bound, v);
        return lower_expr(l, e->as.in.test);
    default:
        /* Literals of aggregates returned their address above. */
        return lower_none();
    }
}

/* Conditions */

/* Branch to then_block when e is true and to else_block when it is false.
   && and || branch after each operand, so a condition computes no bool
   value. The current block ends with the branch. */
void lower_branch(struct lowerer *l, const struct expr *e,
                  struct ir_block *then_block,
                  struct ir_block *else_block)
{
    struct ir_block *rest;
    struct ir_operand v;

    if (e->kind == EXPR_BINARY && (e->as.binary.op == TOKEN_AND_AND ||
                                   e->as.binary.op == TOKEN_OR_OR)) {
        rest = lower_new_block(l);
        if (e->as.binary.op == TOKEN_AND_AND) {
            lower_branch(l, e->as.binary.left, rest, else_block);
        } else {
            lower_branch(l, e->as.binary.left, then_block, rest);
        }
        l->b = rest;
        lower_branch(l, e->as.binary.right, then_block, else_block);
        return;
    }
    if (e->kind == EXPR_UNARY && e->as.unary.op == TOKEN_BANG) {
        lower_branch(l, e->as.unary.operand, else_block, then_block);
        return;
    }
    v = lower_expr(l, e);
    ir_branch(l->f, l->b, v, then_block, else_block);
}

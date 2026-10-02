/* Places and addresses in lowering. The address of a field, an element,
   a static and a name, the place an expression names and its value, and
   the constant data of a global with the value of `here`. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "sema.h"
#include "text.h"
#include "types.h"
#include "lower_lowerer.h"

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
    if (!pointer) {
        lower_keep_temp(l, base, address);
    }
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

struct ir_operand lower_element_address(struct lowerer *l,
                                        const struct expr *e)
{
    struct ir_operand length;
    struct ir_operand base = lower_first_element(l, e->as.index.base, &length);
    struct ir_operand index = lower_expr(l, e->as.index.index);
    uint32_t offset;

    if (length.kind != IR_NONE) {
        lower_bounds_check(l, e, index, length);
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
    f = type_find_field(s, &e->as.field.name);
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
            if (base->type->kind != TYPE_POINTER) {
                lower_keep_temp(l, base, p->address);
            }
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
            if (!pointer) {
                lower_keep_temp(l, base, address);
            }
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
    snprintf(name, sizeof name, "%zu", lower_globals_of_module(l));
    return lower_temp(
        l, ir_addr(l->f, l->b,
                   ir_global_op(ir_global_add_value(m, l->module_name, name,
                                                    value))));

}

/* DESIGN: a constant that holds a class value is written part by part.
   A class value needs its tables and the defaults its literal left out,
   and runs `construct`, which read-only data cannot give. Every other
   constant is one copy of its data or one store. */
void lower_store_constant(struct lowerer *l, const struct type *t,
                          const struct const_value *v, struct ir_operand dest)
{
    uint64_t i;

    if (t->kind == TYPE_CLASS) {
        lower_prepare_value(l, t, v, dest);
        lower_run_construct(l, t, dest);
        return;
    }
    if (sema_holds_class(t) && t->kind == TYPE_ARRAY) {
        for (i = 0; i < v->as.aggregate.count; i++) {
            lower_store_constant(
                l, t->element, &v->as.aggregate.items[i],
                lower_offset_address(l, dest,
                                     lower_element_offset(l, t->element, i)));
        }
        return;
    }
    if (sema_holds_class(t) && type_has_fields(t)) {
        for (i = 0; i < t->field_count; i++) {
            const struct struct_field *f = &t->fields[i];
            if (type_field_is_unit_break(f)) {
                continue;
            }
            if (f->bits != 0) {
                ir_bitstore(l->f, l->b, lower_ir_type_of(f->type),
                            lower_constant(l, &v->as.aggregate.items[i],
                                           lower_ir_type_of(f->type)),
                            dest, lower_agg_of(l, t), (uint32_t)i);
                continue;
            }
            lower_store_constant(
                l, f->type, &v->as.aggregate.items[i],
                lower_offset_address(l, dest,
                                     lower_field_offset(l, t, &f->name)));
        }
        return;
    }
    if (lower_is_aggregate(t)) {
        ir_memcopy(l->f, l->b, dest, lower_const_address(l, v, t),
                   lower_vtype_of(l, t));
        return;
    }
    /* A named function as an `own fn` is its code with no snapshot. */
    if (t->kind == TYPE_FN && t->context) {
        ir_store(l->f, l->b, IR_PTR, lower_constant(l, v, IR_PTR), dest);
        ir_store(l->f, l->b, IR_PTR, ir_int_op(IR_PTR, 0),
                 lower_context_word(l, t, dest));
        return;
    }
    ir_store(l->f, l->b, lower_ir_type_of(t),
             lower_constant(l, v, lower_ir_type_of(t)), dest);
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
        arena_alloc(l->m->arena, alloc_product(t->field_count + 1, sizeof *v));
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

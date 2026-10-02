#include "antl.h"
#include "alloc.h"
#include "antl_io.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* DESIGN: the file is a flat sequence of little-endian integers of fixed
   width. A string is a u32 byte count and the bytes. A float is the u64
   of its IEEE 754 bits. No pointer, padding or host byte order reaches the
   file. It stores enum values of types.h, sema.h and ir.h. These checks
   fail when one of them changes, and the version changes with it. */
_Static_assert(TYPE_STRUCT == 24, "raise ANTL_VERSION, then update this");
_Static_assert(TYPE_VARIANT == 28, "raise ANTL_VERSION, then update this");
_Static_assert(TYPE_OPTIONAL == 30, "raise ANTL_VERSION, then update this");
_Static_assert(SYMBOL_CONSTRAINT == 8, "raise ANTL_VERSION, then update this");
_Static_assert(CONST_SYMBOLIC == 8, "raise ANTL_VERSION, then update this");
_Static_assert(SYMBOLIC_CAST == 4, "raise ANTL_VERSION, then update this");
_Static_assert(TOKEN_KIND_COUNT == 180, "raise ANTL_VERSION, then update this");
_Static_assert(IR_LOCK == 11, "raise ANTL_VERSION, then update this");
_Static_assert(IR_RET == 85, "raise ANTL_VERSION, then update this");
_Static_assert(IR_FAIL_CHECK == 2, "raise ANTL_VERSION, then update this");
_Static_assert(IR_SYM == 7, "raise ANTL_VERSION, then update this");
_Static_assert(IR_EXT_ZERO == 2, "raise ANTL_VERSION, then update this");
_Static_assert(IR_CONST_AGG == 6, "raise ANTL_VERSION, then update this");
_Static_assert(IR_AGG_ARRAY == 2, "raise ANTL_VERSION, then update this");
_Static_assert(IR_SYM_OP == 3, "raise ANTL_VERSION, then update this");

static const uint8_t magic[4] = {'A', 'N', 'T', 'L'};

/* Writing */

static void put_str(struct writer *w, const char *s)
{
    antl_put_bytes(w, s, s == NULL ? 0 : strlen(s));
}

/* Doc text, or nothing for --strip-docs. */
static void put_doc(struct writer *w, const char *text, size_t length)
{
    antl_put_bytes(w, text, w->strip_docs || text == NULL ? 0 : length);
}

static uint64_t float_bits(double d)
{
    uint64_t bits;

    memcpy(&bits, &d, sizeof bits);
    return bits;
}

/* DESIGN: the compiler declares the root class, a Job, Flags, a Mutex
   and a channel. It declares a Regex, a Match and the hidden lock of a
   synchronized class as well. Each carries the path `anti.lang`. The
   library file of `anti.lang` names them as it names a struct of another
   module and declares none. A reader so takes the compiler's own. The
   root is the one class without a base. */
static bool is_local_struct(const struct writer *w, const struct type *t)
{
    const char *module = w->iface->module;

    if ((t->kind == TYPE_CLASS && t->base == NULL) ||
        t->kind == TYPE_OPTIONAL || types_is_job(t) ||
        types_is_flags(t) || types_is_mutex(t) || types_is_chan(t) ||
        types_is_regex(t) || types_is_match(t) || types_is_object_lock(t) ||
        types_is_field_descriptor(t)) {
        return false;
    }
    /* A copy of a generic is written in full wherever it stands, so the
       reader can make it when no module read before has. */
    return type_has_fields(t) &&
           (t->generic != NULL ||
            (t->module.length == strlen(module) &&
             memcmp(t->module.text, module, t->module.length) == 0));
}

/* The count of functions of the body of t that another module may see.
   A private function is never one of them, because no other module can
   name it. A protected one is, because a class below it may. */
/* DESIGN: a class carries its public and protected functions, and its
   `construct` and `destruct` whatever their level. A class of another
   module that inherits it runs them, and a private function it cannot
   name. */
static bool name_equals(const struct name *n, const char *s);

/* DESIGN: a generic struct, class or variant carries every function of
   its body, private ones among them. A module that makes a copy of it
   compiles the body of each. A function with type parameters of
   its own stays behind, as no copy compiles one yet. A copy carries no
   function: it has those of its generic, as the checker gives it. */
static bool carried_member(const struct type *t, const struct item *m)
{
    if (m->kind != ITEM_FN || m->symbol == NULL) {
        return false;
    }
    if (t->type_param_count > 0) {
        return m->type_param_count == 0;
    }
    return m->vis != VIS_PRIVATE ||
           (m->body != NULL && (name_equals(&m->name, "construct") ||
                                name_equals(&m->name, "destruct")));
}

static size_t public_members(const struct type *t)
{
    size_t count = 0;
    size_t i;

    for (i = 0; t->generic == NULL && i < t->member_count; i++) {
        if (carried_member(t, t->members[i])) {
            count++;
        }
    }
    return count;
}

/* The form of a struct, a class or a variant in the type table. */
enum { FORM_PLAIN, FORM_GENERIC, FORM_COPY };

static uint8_t struct_form(const struct type *t)
{
    return t->generic != NULL          ? FORM_COPY
           : t->type_param_count > 0 ? FORM_GENERIC
                                     : FORM_PLAIN;
}

/* Whether t is a pub struct or union of the interface. */
static bool is_public_struct(const struct writer *w, const struct type *t)
{
    size_t i;

    for (i = 0; i < w->iface->item_count; i++) {
        if (w->iface->items[i]->kind == SYMBOL_STRUCT &&
            w->iface->items[i]->type == t) {
            return true;
        }
    }
    return false;
}

/* Whether t is in the type table of w. If so, index receives its index. */
static bool find_type(const struct writer *w, const struct type *t,
                      size_t *index)
{
    size_t i;

    for (i = 0; i < w->type_count; i++) {
        if (w->types[i] == t) {
            *index = i;
            return true;
        }
    }
    return false;
}

static void add_type(struct writer *w, const struct type *t)
{
    w->types = alloc_grow(w->types, &w->type_capacity, w->type_count,
                          sizeof *w->types);
    w->types[w->type_count++] = t;
}

/* The types a symbolic value names: its own and those it measures. */
void antl_visit_symbolic(struct writer *w, const struct symbolic *s)
{
    if (s == NULL) {
        return;
    }
    antl_visit_type(w, s->type);
    if (s->of != NULL) {
        antl_visit_type(w, s->of);
    }
    antl_visit_symbolic(w, s->a);
    antl_visit_symbolic(w, s->b);
}

void antl_visit_value(struct writer *w, const struct const_value *v)
{
    size_t i;

    if (v->kind == CONST_SYMBOLIC) {
        antl_visit_symbolic(w, v->as.symbolic);
    } else if (v->kind == CONST_ARRAY || v->kind == CONST_STRUCT) {
        for (i = 0; i < v->as.aggregate.count; i++) {
            antl_visit_value(w, &v->as.aggregate.items[i]);
        }
    }
}

/* The types the constant defaults of a function's parameters name. */
void antl_visit_defaults(struct writer *w, const struct symbol *sym)
{
    size_t i;

    for (i = 0; sym->defaults != NULL && i < sym->default_count; i++) {
        if (sym->defaults[i].value != NULL) {
            antl_visit_value(w, sym->defaults[i].value);
        }
    }
}

/* Give t and every type inside it an index. A type comes after the types
   it is built from. A struct comes before its field types, so a struct
   can hold a pointer to itself. */
void antl_visit_type(struct writer *w, const struct type *t)
{
    size_t i;
    size_t index;

    if (find_type(w, t, &index)) {
        return;
    }
    switch (t->kind) {
    case TYPE_ARRAY:
        antl_visit_symbolic(w, t->length_of);
        antl_visit_type(w, t->element);
        break;
    case TYPE_POINTER:
    case TYPE_SLICE:
    case TYPE_OPTIONAL:
        antl_visit_type(w, t->element);
        break;
    case TYPE_FN:
        for (i = 0; i < t->param_count; i++) {
            antl_visit_type(w, t->params[i]);
        }
        antl_visit_type(w, t->result);
        break;
    /* A tuple is its elements in order, and nothing else, because two
       tuples of the same elements are one type. */
    case TYPE_TUPLE:
        for (i = 0; i < t->param_count; i++) {
            antl_visit_type(w, t->params[i]);
        }
        break;
    case TYPE_STRUCT:
    case TYPE_CLASS:
    case TYPE_VARIANT:
        /* A channel names its element, which comes first. So do the
           parameters of a generic, and the generic and the arguments of
           a copy. */
        if (types_is_chan(t)) {
            antl_visit_type(w, t->element);
        }
        for (i = 0; i < t->type_param_count; i++) {
            antl_visit_type(w, t->type_params[i]);
        }
        if (t->generic != NULL) {
            antl_visit_type(w, t->generic);
            for (i = 0; i < t->generic->type_param_count; i++) {
                if (t->values[i] != NULL) {
                    antl_visit_symbolic(w, t->values[i]);
                } else {
                    antl_visit_type(w, t->args[i]);
                }
            }
        }
        /* The generic may name this copy in a field. */
        if (find_type(w, t, &index)) {
            return;
        }
        add_type(w, t);
        if (is_local_struct(w, t)) {
            for (i = 0; i < t->field_count; i++) {
                antl_visit_type(w, t->fields[i].type);
                if (t->fields[i].constant != NULL) {
                    antl_visit_value(w, t->fields[i].constant);
                }
            }
            for (i = 0; t->generic == NULL && i < t->member_count; i++) {
                const struct item *m = t->members[i];
                if (carried_member(t, m)) {
                    antl_visit_type(w, m->symbol->type);
                    antl_visit_defaults(w, m->symbol);
                }
            }
        }
        return;
    /* The value a hook of a parameter gives follows the parameter, and a
       parameter follows the interfaces of its constraints. */
    case TYPE_PARAM:
        if (t->hook_owner != NULL) {
            antl_visit_type(w, t->hook_owner);
        }
        for (i = 0; i < t->iface_count; i++) {
            antl_visit_type(w, t->ifaces[i]);
        }
        if (find_type(w, t, &index)) {
            return;
        }
        add_type(w, t);
        if (t->walked != NULL) {
            antl_visit_type(w, t->walked);
        }
        if (t->indexed != NULL) {
            antl_visit_type(w, t->indexed);
        }
        return;
    /* The underlying integer comes first, because an enum names it by
       index and a reference reaches back only. */
    case TYPE_ENUM:
        antl_visit_type(w, t->base);
        add_type(w, t);
        return;
    default:
        break;
    }
    add_type(w, t);
}

void antl_put_type_ref(struct writer *w, const struct type *t)
{
    size_t index;

    /* visit_type puts every type the file names in the table first. */
    if (!find_type(w, t, &index)) {
        w->failed = true;
        index = 0;
    }
    antl_put_count(w, index);
}

void antl_put_type_or_none(struct writer *w, const struct type *t)
{
    if (t == NULL) {
        antl_put_u32(w, ANTL_NO_TYPE);
    } else {
        antl_put_type_ref(w, t);
    }
}

void antl_put_symbolic(struct writer *w, const struct symbolic *s)
{
    antl_put_u8(w, (uint8_t)s->kind);
    antl_put_type_ref(w, s->type);
    switch (s->kind) {
    case SYMBOLIC_INT:
        antl_put_u64(w, s->value);
        break;
    case SYMBOLIC_SIZE_OF:
        antl_put_type_ref(w, s->of);
        break;
    case SYMBOLIC_UNARY:
    case SYMBOLIC_CAST:
        antl_put_u8(w, (uint8_t)s->op);
        antl_put_symbolic(w, s->a);
        break;
    case SYMBOLIC_BINARY:
        antl_put_u8(w, (uint8_t)s->op);
        antl_put_symbolic(w, s->a);
        antl_put_symbolic(w, s->b);
        break;
    /* A constant parameter of a generic, `N`, names its parameter. */
    case SYMBOLIC_PARAM:
        antl_put_type_ref(w, s->of);
        break;
    }
}

/* The constraints of a parameter or of a `constraint` as written, each
   a module and a name. `anti doc` prints them. */
static void put_constraint_refs(struct writer *w,
                                const struct constraint_ref *refs,
                                size_t count)
{
    size_t i;

    antl_put_count(w, count);
    for (i = 0; i < count; i++) {
        antl_put_bytes(w, refs[i].module.text, refs[i].module.length);
        antl_put_bytes(w, refs[i].name.text, refs[i].name.length);
    }
}

/* DESIGN: a type parameter is an entry of the type table. It carries its
   name and a role: 0 for a parameter a generic declares, 1 for the value
   a walk of one gives, 2 for the value `e[i]` of one reads, and 3 for
   the set a `constraint` names. The value of a hook follows the index of
   its parameter. A declared parameter carries the mark of `N: int` and
   its constraints as written, and a set carries those of its
   `constraint`. Every one then carries the hooks it meets, one bit per
   entry of the hook table, and the interfaces it names. */
static void put_param_type(struct writer *w, const struct type *t)
{
    size_t i;

    antl_put_bytes(w, t->name.text, t->name.length);
    if (t->hook_owner != NULL) {
        antl_put_u8(w, t->hook_owner->walked == t ? 1 : 2);
        antl_put_type_ref(w, t->hook_owner);
    } else if (t->param != NULL) {
        antl_put_u8(w, 0);
        antl_put_u8(w, t->param->constant);
        put_constraint_refs(w, t->param->constraints,
                            t->param->constraint_count);
    } else {
        antl_put_u8(w, 3);
        put_constraint_refs(w,
                            t->declared_by != NULL
                                ? t->declared_by->constraints
                                : NULL,
                            t->declared_by != NULL
                                ? t->declared_by->constraint_count
                                : 0);
    }
    antl_put_u32(w, t->hooks);
    antl_put_count(w, t->iface_count);
    for (i = 0; i < t->iface_count; i++) {
        antl_put_type_ref(w, t->ifaces[i]);
    }
}

static void put_type(struct writer *w, const struct type *t)
{
    size_t i;
    size_t j;

    antl_put_u8(w, (uint8_t)t->kind);
    switch (t->kind) {
    case TYPE_POINTER:
        antl_put_type_ref(w, t->element);
        /* `*T`, `?*T` and their `lent` forms are four types, and a module
           that imports this one reads which of them a signature names.
           Bit 0 is `?` and bit 1 `lent`. */
        antl_put_u8(w, (uint8_t)((unsigned)t->nullable |
                                 (unsigned)t->lent << 1));
        break;
    /* `[]T` and `lent []T` are two types, told apart by one byte. */
    case TYPE_SLICE:
        antl_put_type_ref(w, t->element);
        antl_put_u8(w, (uint8_t)t->lent);
        break;
    /* `?T` is its value type alone, since one element makes one. */
    case TYPE_OPTIONAL:
        antl_put_type_ref(w, t->element);
        break;
    case TYPE_ARRAY:
        antl_put_type_ref(w, t->element);
        antl_put_u8(w, t->length_of != NULL);
        if (t->length_of != NULL) {
            antl_put_symbolic(w, t->length_of);
        } else {
            antl_put_u64(w, t->length);
        }
        break;
    /* DESIGN: a function type ends with a byte of its flags. Bit 0 is
       `?`, bit 1 bound, bit 2 `may fail` and bit 3 the out pointer of
       that form. Bit 4 is the form of two words of a parameter that does
       not keep its argument, and bit 5 marks it `concurrent`. Bit 6 is
       `own fn`, which stands with both. Each makes
       another type, and a module that imports this one reads the type
       the signature names. */
    case TYPE_FN:
        antl_put_count(w, t->param_count);
        for (i = 0; i < t->param_count; i++) {
            antl_put_type_ref(w, t->params[i]);
        }
        antl_put_type_ref(w, t->result);
        antl_put_u8(w, (uint8_t)((unsigned)t->nullable |
                                 (unsigned)t->bound << 1 |
                                 (unsigned)t->may_fail << 2 |
                                 (unsigned)t->has_out << 3 |
                                 (unsigned)t->context << 4 |
                                 (unsigned)t->concurrent << 5 |
                                 (unsigned)t->owned << 6));
        break;
    case TYPE_TUPLE:
        antl_put_count(w, t->param_count);
        for (i = 0; i < t->param_count; i++) {
            antl_put_type_ref(w, t->params[i]);
        }
        break;
    /* DESIGN: a class is written like a struct, with the form and the
       own bit of each field. Its base is the type of field 0, so the
       chain follows the ordinary type references. A variant is written
       as the struct C sees. Its tag is the enum of field 0, which names
       the cases. The union of field 1 holds the struct of each case that
       has fields, so the reader finds both again. */
    case TYPE_STRUCT:
    case TYPE_CLASS:
    case TYPE_VARIANT:
        antl_put_bytes(w, t->module.text, t->module.length);
        antl_put_bytes(w, t->name.text, t->name.length);
        /* DESIGN: a form byte follows the name. A generic carries its
           parameters after its body. A copy names its generic and its
           arguments, a type or a constant each, before its body. It
           carries that body in full wherever it stands. A reader that
           has the copy already takes that one. The copies of one generic
           with the same arguments are then one type in the program. */
        antl_put_u8(w, struct_form(t));
        if (t->generic != NULL) {
            antl_put_type_ref(w, t->generic);
            for (i = 0; i < t->generic->type_param_count; i++) {
                antl_put_u8(w, t->values[i] != NULL);
                if (t->values[i] != NULL) {
                    antl_put_symbolic(w, t->values[i]);
                } else {
                    antl_put_type_ref(w, t->args[i]);
                }
            }
        }
        /* `chan T` is one struct per element type, so the element
           follows its name. */
        if (types_is_chan(t)) {
            antl_put_type_ref(w, t->element);
        }
        if (is_local_struct(w, t)) {
            antl_put_u8(w, (uint8_t)((unsigned)t->is_union |
                                     (unsigned)t->packed << 1 |
                                     (unsigned)t->has_abstract << 2 |
                                     (unsigned)t->is_final << 3 |
                                     (unsigned)t->simd << 4 |
                                     (unsigned)t->traced << 5));
            /* A thread-safe class, and `unchecked(unguarded-field)` in
               its header. */
            antl_put_u8(w, (uint8_t)((unsigned)t->safety |
                                     (unsigned)t->unchecked_fields << 2));
            /* The `compatible` line of an abstract class, empty where
               the body has none. Every module that names the class
               writes the same descriptor, so the floor travels with
               it. */
            antl_put_bytes(w, t->compatible.text, t->compatible.length);
            antl_put_u64(w, t->align);
            antl_put_count(w, t->field_count);
            for (i = 0; i < t->field_count; i++) {
                const struct struct_field *f = &t->fields[i];
                antl_put_bytes(w, f->name.text, f->name.length);
                antl_put_type_ref(w, f->type);
                antl_put_u8(w, f->bits);
                antl_put_u8(w, (uint8_t)((unsigned)f->form |
                                         (unsigned)f->owned << 4 |
                                         (unsigned)f->atomic << 5 |
                                         (unsigned)f->writable << 6 |
                                         (unsigned)f->transient << 7));
                antl_put_u8(w, (uint8_t)f->vis);
                /* DESIGN: `inject` and `inject final` travel with the
                   field. A module that builds a class of another
                   module then calls the same provider through the same
                   slot, and `anti build` reports what a dependency
                   needs. */
                antl_put_u8(w, (uint8_t)((unsigned)f->injected |
                                         (unsigned)f->inject_final << 1 |
                                         (unsigned)f->hidden << 2 |
                                         (unsigned)f->unchecked << 3));
                /* DESIGN: the lock that guards the field travels by its
                   name. A lock of an enclosing class guards a field of
                   a nested type alone, which no other module reaches.
                   So the class it names stays behind. */
                antl_put_bytes(w, f->guard.text, f->guard.length);
                /* DESIGN: /// on a private item is never stored, and
                   the fields of a private struct are private items. */
                put_doc(w, f->doc.text,
                        is_public_struct(w, t) ? f->doc.length : 0);
            }
            /* The public functions of the body, so a call on a value of
               another module resolves and reaches the right symbol. */
            antl_put_count(w, public_members(t));
            for (i = 0; t->generic == NULL && i < t->member_count; i++) {
                const struct item *m = t->members[i];
                if (!carried_member(t, m)) {
                    continue;
                }
                antl_put_bytes(w, m->name.text, m->name.length);
                /* The qualifier decides the table a body fills and
                   the symbol it has, so an importing module builds
                   the same tables. */
                antl_put_bytes(w, m->qualifier.text, m->qualifier.length);
                antl_put_type_ref(w, m->symbol->type);
                antl_put_u8(w, (uint8_t)((unsigned)m->contract |
                                         (unsigned)m->is_final << 4 |
                                         (unsigned)m->is_operator << 5 |
                                         (unsigned)m->may_fail << 6));
                antl_put_u8(w, (uint8_t)m->vis);
                put_doc(w, m->doc.text, m->doc.length);
                /* DESIGN: the public interface keeps the parameter
                   names of every function. A function of a class body is
                   one, and `anti doc` and the generated header print
                   them. The count is the one the declaration wrote.
                   Neither `self` nor the out pointer of `may fail`
                   stands among them, so the reader needs no type to take
                   them. */
                antl_put_count(w, m->param_count);
                for (j = 0; j < m->param_count; j++) {
                    const struct name *n = m->symbol->params != NULL
                                               ? &m->symbol->params[j]
                                               : &m->params[j].name;
                    antl_put_bytes(w, n->text, n->length);
                }
            }
            if (t->type_param_count > 0) {
                antl_put_count(w, t->type_param_count);
                for (i = 0; i < t->type_param_count; i++) {
                    antl_put_type_ref(w, t->type_params[i]);
                }
            }
        }
        break;
    case TYPE_PARAM:
        put_param_type(w, t);
        break;
    /* An enum is its module, its name, its underlying integer and the
       name and number of each value. */
    case TYPE_ENUM:
        antl_put_bytes(w, t->module.text, t->module.length);
        antl_put_bytes(w, t->name.text, t->name.length);
        antl_put_type_ref(w, t->base);
        antl_put_count(w, t->field_count);
        for (i = 0; i < t->field_count; i++) {
            antl_put_bytes(w, t->fields[i].name.text, t->fields[i].name.length);
            antl_put_u64(w, t->fields[i].number);
            put_doc(w, t->fields[i].doc.text, t->fields[i].doc.length);
        }
        break;
    default:
        break;
    }
}

void antl_put_value(struct writer *w, const struct const_value *v)
{
    size_t i;

    antl_put_u8(w, (uint8_t)v->kind);
    switch (v->kind) {
    case CONST_INT:
        antl_put_u64(w, v->as.integer);
        break;
    case CONST_FLOAT:
        antl_put_u64(w, float_bits(v->as.floating));
        break;
    case CONST_BOOL:
        antl_put_u8(w, v->as.boolean);
        break;
    case CONST_CHAR:
        antl_put_u32(w, v->as.character);
        break;
    case CONST_NULL:
        break;
    case CONST_TEXT:
        antl_put_bytes(w, v->as.text.bytes, v->as.text.length);
        break;
    case CONST_ARRAY:
    case CONST_STRUCT:
        antl_put_count(w, v->as.aggregate.count);
        for (i = 0; i < v->as.aggregate.count; i++) {
            antl_put_value(w, &v->as.aggregate.items[i]);
        }
        break;
    case CONST_SYMBOLIC:
        antl_put_symbolic(w, v->as.symbolic);
        break;
    case CONST_DEFAULT:
        break;
    }
}

/* DESIGN: the defaults of a function's parameters follow its type. A
   count gives the parameters, `self` included, and is 0 when none has a
   default. Each parameter then has a byte: 0 without a default, 1 before
   a constant and 2 for `here`, which the call fills with its position. */
void antl_put_param_defaults(struct writer *w, const struct symbol *sym)
{
    size_t i;

    antl_put_count(w, sym->defaults != NULL ? sym->default_count : 0);
    for (i = 0; sym->defaults != NULL && i < sym->default_count; i++) {
        const struct param_default *d = &sym->defaults[i];
        antl_put_u8(w, d->here ? 2 : d->value != NULL ? 1 : 0);
        if (!d->here && d->value != NULL) {
            antl_put_value(w, d->value);
        }
    }
}

/* The `own` parameters follow the defaults: a count, `self` included and
   0 when none is `own`, then a byte of 0 or 1 per parameter. */
void antl_put_param_owned(struct writer *w, const struct symbol *sym)
{
    size_t i;

    antl_put_count(w, sym->owned != NULL ? sym->owned_count : 0);
    for (i = 0; sym->owned != NULL && i < sym->owned_count; i++) {
        antl_put_u8(w, sym->owned[i] ? 1 : 0);
    }
}

/* DESIGN: the defaults of the fields follow the type table. The reader
   reads a value against the type of its field, and it resolves that type
   once the whole table is in. Each field of a struct that the file
   declares gets one byte, and the value follows where that byte is 1. */
static void put_defaults(struct writer *w, const struct type *t)
{
    size_t i;

    if (!type_has_fields(t) || !is_local_struct(w, t)) {
        return;
    }
    for (i = 0; i < t->field_count; i++) {
        antl_put_u8(w, t->fields[i].constant != NULL);
        if (t->fields[i].constant != NULL) {
            antl_put_value(w, t->fields[i].constant);
        }
    }
    /* The functions of the body, in the order the type carries them, so
       the reader has their types in place. */
    for (i = 0; t->generic == NULL && i < t->member_count; i++) {
        if (carried_member(t, t->members[i])) {
            antl_put_param_defaults(w, t->members[i]->symbol);
            antl_put_param_owned(w, t->members[i]->symbol);
        }
    }
}

static void put_vtype(struct writer *w, struct ir_vtype v)
{
    antl_put_u8(w, (uint8_t)v.type);
    antl_put_u32(w, v.agg);
}

/* An aggregate constant, as the tree the back end lays out. Its type and
   its symbolic values are written as indices, which the reader maps. */
static void put_const(struct writer *w, const struct ir_const *c)
{
    size_t i;

    antl_put_u8(w, (uint8_t)c->kind);
    antl_put_u8(w, (uint8_t)c->scalar);
    switch (c->kind) {
    case IR_CONST_INT:
        antl_put_u64(w, c->integer);
        break;
    case IR_CONST_FLOAT:
        antl_put_u64(w, float_bits(c->floating));
        break;
    case IR_CONST_SYM:
        antl_put_u64(w, c->sym);
        break;
    case IR_CONST_ADDR:
    case IR_CONST_FUNC:
        antl_put_u64(w, c->global);
        break;
    case IR_CONST_AGG:
        antl_put_u64(w, c->item_count);
        put_vtype(w, c->type);
        for (i = 0; i < c->item_count; i++) {
            put_const(w, &c->items[i]);
        }
        break;
    default: /* IR_CONST_NONE */
        antl_put_u64(w, 0);
        break;
    }
}

static void put_operand(struct writer *w, const struct ir_operand *o)
{
    antl_put_u8(w, (uint8_t)o->kind);
    antl_put_u8(w, (uint8_t)o->type);
    switch (o->kind) {
    case IR_TEMP:
        antl_put_u64(w, o->as.temp);
        break;
    case IR_INT:
        antl_put_u64(w, o->as.integer);
        break;
    case IR_FLOAT:
        antl_put_u64(w, float_bits(o->as.floating));
        break;
    case IR_GLOBAL:
    case IR_FUNC:
    case IR_BLOCK:
    case IR_SYM:
        antl_put_u64(w, o->as.index);
        break;
    default:
        antl_put_u64(w, 0);
        break;
    }
}

static void put_inst(struct writer *w, const struct ir_inst *inst)
{
    size_t i;

    antl_put_u8(w, (uint8_t)inst->op);
    antl_put_u8(w, (uint8_t)inst->type);
    antl_put_u32(w, inst->line);
    antl_put_u32(w, inst->result);
    put_operand(w, &inst->a);
    put_operand(w, &inst->b);
    put_operand(w, &inst->c);
    put_vtype(w, inst->of);
    antl_put_u32(w, inst->field);
    antl_put_count(w, inst->arg_count);
    for (i = 0; i < inst->arg_count; i++) {
        put_operand(w, &inst->args[i]);
    }
}

static void put_ir(struct writer *w, const struct ir_module *ir)
{
    size_t i;
    size_t j;
    size_t k;

    /* The source files of the module, which its functions name by index.
       A path comes from the search root, so the bytes are the same on
       every host. */
    antl_put_count(w, ir->file_count);
    for (i = 0; i < ir->file_count; i++) {
        put_str(w, ir->files[i]);
    }
    antl_put_count(w, ir->sym_count);
    for (i = 0; i < ir->sym_count; i++) {
        const struct ir_sym *sym = &ir->syms[i];
        antl_put_u8(w, (uint8_t)sym->kind);
        antl_put_u8(w, (uint8_t)sym->type);
        antl_put_u64(w, sym->value);
        put_vtype(w, sym->of);
        antl_put_u32(w, sym->field);
        antl_put_u8(w, (uint8_t)sym->op);
        antl_put_u32(w, sym->a);
        antl_put_u32(w, sym->b);
    }
    antl_put_count(w, ir->agg_count);
    for (i = 0; i < ir->agg_count; i++) {
        const struct ir_aggtype *t = ir->aggs[i];
        antl_put_u8(w, (uint8_t)t->kind);
        put_str(w, t->name);
        antl_put_u8(w, (uint8_t)((unsigned)t->packed | (unsigned)t->simd << 1));
        antl_put_u64(w, t->align);
        antl_put_u32(w, t->length);
        put_str(w, t->length_text);
        antl_put_count(w, t->field_count);
        for (j = 0; j < t->field_count; j++) {
            put_str(w, t->fields[j].name);
            put_vtype(w, t->fields[j].type);
            antl_put_u8(w, t->fields[j].bits);
            antl_put_u8(w, (uint8_t)t->fields[j].ext);
        }
    }
    antl_put_count(w, ir->global_count);
    for (i = 0; i < ir->global_count; i++) {
        const struct ir_global *g = ir->globals[i];
        put_str(w, g->module);
        put_str(w, g->name);
        antl_put_u64(w, g->size);
        antl_put_u64(w, g->align);
        if (g->size > 0) {
            text_append_bytes(w->out, g->bytes, g->size);
        }
        antl_put_count(w, g->reloc_count);
        for (j = 0; j < g->reloc_count; j++) {
            antl_put_u64(w, g->relocs[j].offset);
            antl_put_u32(w, g->relocs[j].global);
            antl_put_u8(w, g->relocs[j].fn ? 1 : 0);
        }
        antl_put_u8(w, (uint8_t)((g->value != NULL ? 1 : 0) |
                                 (g->exported ? 2 : 0) |
                                 (g->is_extern ? 4 : 0) |
                                 (g->mutable ? 8 : 0)));
        if (g->value != NULL) {
            put_const(w, g->value);
        }
    }
    /* All signatures come before the first body, so a body can call a
       function that the file lists later. */
    antl_put_count(w, ir->function_count);
    for (i = 0; i < ir->function_count; i++) {
        const struct ir_function *f = ir->functions[i];
        antl_put_u8(w, (uint8_t)((f->is_extern ? 1 : 0) |
                                 (f->variadic ? 2 : 0) |
                                 (f->exported ? 4 : 0) | (f->worker ? 8 : 0)));
        put_str(w, f->module);
        put_str(w, f->name);
        antl_put_u8(w, (uint8_t)f->result);
        antl_put_u32(w, f->result_agg);
        antl_put_u32(w, f->file);
        antl_put_u32(w, f->decl_line);
        antl_put_count(w, f->param_count);
        for (j = 0; j < f->param_count; j++) {
            antl_put_u8(w, (uint8_t)f->params[j].type);
            antl_put_u8(w, (uint8_t)f->params[j].ext);
            antl_put_u32(w, f->params[j].agg);
        }
    }
    for (i = 0; i < ir->function_count; i++) {
        const struct ir_function *f = ir->functions[i];
        if (f->is_extern) {
            continue;
        }
        antl_put_u32(w, f->temp_count);
        for (j = 0; j < f->temp_count; j++) {
            antl_put_u8(w, (uint8_t)f->temps[j]);
        }
        antl_put_count(w, f->block_count);
        for (j = 0; j < f->block_count; j++) {
            /* The failure block of an assertion carries its flag, so the
               build that compiles the program can still drop it. */
            antl_put_u8(w, (uint8_t)f->blocks[j]->fail);
            antl_put_count(w, f->blocks[j]->count);
            for (k = 0; k < f->blocks[j]->count; k++) {
                put_inst(w, &f->blocks[j]->insts[k]);
            }
        }
    }
    antl_put_count(w, ir->class_count);
    for (i = 0; i < ir->class_count; i++) {
        const struct ir_class *c = ir->classes[i];
        put_str(w, c->module);
        put_str(w, c->name);
        antl_put_u8(w, (uint8_t)c->flags);
        antl_put_u32(w, c->descriptor);
        antl_put_u32(w, c->base);
        antl_put_u32(w, c->table);
        antl_put_u32(w, c->init);
        antl_put_u32(w, c->agg);
        antl_put_count(w, c->subtable_count);
        for (j = 0; j < c->subtable_count; j++) {
            antl_put_u32(w, c->subtables[j].interface);
            antl_put_u32(w, c->subtables[j].table);
            antl_put_u32(w, c->subtables[j].agg);
            antl_put_u32(w, c->subtables[j].field);
        }
        antl_put_count(w, c->mutable_count);
        for (j = 0; j < c->mutable_count; j++) {
            antl_put_u32(w, c->mutable_fields[j]);
        }
        /* The `inject` fields, so the pass over the whole program finds
           every interface of the program and the class that needs a
           provider for it. */
        antl_put_count(w, c->inject_count);
        for (j = 0; j < c->inject_count; j++) {
            put_str(w, c->injects[j].interface);
            put_str(w, c->injects[j].field);
            antl_put_u32(w, c->injects[j].descriptor);
            antl_put_u8(w, (uint8_t)(c->injects[j].final ? 1 : 0));
        }
        /* The `provides` lines, so a library file carries what its
           module offers to a host that loads it. */
        antl_put_count(w, c->provides_count);
        for (j = 0; j < c->provides_count; j++) {
            put_str(w, c->provides[j].interface);
            antl_put_u32(w, c->provides[j].descriptor);
        }
    }
}

/* The magic, the version, the package header, the module path, the
   imports and the module's doc text. */
static void put_header(struct writer *w, const struct interface *iface)
{
    const struct package *p = &iface->package;
    struct text *out = w->out;
    size_t i;

    text_append_bytes(out, magic, sizeof magic);
    antl_put_u32(w, ANTL_VERSION);
    put_str(w, p->name != NULL ? p->name : iface->module);
    put_str(w, p->version != NULL ? p->version : PACKAGE_VERSION_DEFAULT);
    antl_put_count(w, p->dependency_count);
    for (i = 0; i < p->dependency_count; i++) {
        put_str(w, p->dependencies[i].name);
        put_str(w, p->dependencies[i].constraint);
        put_str(w, p->dependencies[i].url);
    }
    put_str(w, p->license);
    put_str(w, p->license_text);
    antl_put_count(w, p->attribution_count);
    for (i = 0; i < p->attribution_count; i++) {
        put_str(w, p->attribution[i]);
    }
    put_str(w, iface->module);
    antl_put_count(w, iface->import_count);
    for (i = 0; i < iface->import_count; i++) {
        put_str(w, iface->imports[i]);
    }
    antl_put_count(w, iface->framework_count);
    for (i = 0; i < iface->framework_count; i++) {
        put_str(w, iface->frameworks[i]);
    }
    antl_put_count(w, iface->linux_library_count);
    for (i = 0; i < iface->linux_library_count; i++) {
        put_str(w, iface->linux_libraries[i]);
    }
    put_doc(w, iface->doc, iface->doc != NULL ? strlen(iface->doc) : 0);
}

bool antl_write_header(struct text *out, const struct interface *iface)
{
    struct writer w;

    memset(&w, 0, sizeof w);
    w.out = out;
    w.iface = iface;
    put_header(&w, iface);
    return !w.failed;
}

bool antl_write(struct text *out, const struct interface *iface,
                const struct ir_module *ir, bool strip_docs)
{
    struct writer w;
    size_t i;

    memset(&w, 0, sizeof w);
    w.out = out;
    w.iface = iface;
    w.strip_docs = strip_docs;
    for (i = 0; i < iface->item_count; i++) {
        antl_visit_type(&w, iface->items[i]->type);
        if (iface->items[i]->kind == SYMBOL_CONST) {
            antl_visit_value(&w, iface->items[i]->value);
        }
        antl_visit_defaults(&w, iface->items[i]);
    }
    antl_visit_generics(&w);
    put_header(&w, iface);
    antl_put_count(&w, w.type_count);
    for (i = 0; i < w.type_count; i++) {
        put_type(&w, w.types[i]);
    }
    for (i = 0; i < w.type_count; i++) {
        put_defaults(&w, w.types[i]);
    }
    antl_put_count(&w, iface->item_count);
    for (i = 0; i < iface->item_count; i++) {
        const struct symbol *sym = iface->items[i];
        antl_put_u8(&w, (uint8_t)sym->kind);
        antl_put_bytes(&w, sym->name.text, sym->name.length);
        antl_put_type_ref(&w, sym->type);
        /* DESIGN: the `may fail` flag is recorded, so a reader of the
           file sees the form the declaration wrote. The type alone gives
           the `?*Error` of the ABI and never the form. `worker` is
           recorded for the same reason, and `anti doc` prints it. */
        /* Bit 4 marks the name a `type` declares, bit 5 a generic
           function, whose declaration the section of the generics
           holds, and bit 6 a function written `operator fn`, which an
           importing module finds as the hook of a struct. */
        antl_put_u8(&w, (uint8_t)((unsigned)sym->exported |
                                  (unsigned)sym->internal << 1 |
                                  (unsigned)sym->may_fail << 2 |
                                  (unsigned)sym->worker << 3 |
                                  (unsigned)sym->alias << 4 |
                                  (unsigned)(sym->kind == SYMBOL_FN &&
                                             sym->item != NULL &&
                                             sym->item->type_param_count > 0)
                                      << 5 |
                                  (unsigned)(sym->kind == SYMBOL_FN &&
                                             sym->is_operator)
                                      << 6));
        put_doc(&w, sym->doc.text, sym->doc.length);
        if (sym->kind == SYMBOL_FN || sym->kind == SYMBOL_EXTERN_FN) {
            size_t j;
            for (j = 0; j < sym->type->param_count; j++) {
                antl_put_bytes(&w, sym->params[j].text, sym->params[j].length);
            }
            antl_put_param_defaults(&w, sym);
            antl_put_param_owned(&w, sym);
        }
        if (sym->kind == SYMBOL_EXTERN_FN) {
            antl_put_u8(&w, sym->variadic);
        } else if (sym->kind == SYMBOL_CONST) {
            antl_put_value(&w, sym->value);
        }
    }
    antl_put_generics(&w);
    put_ir(&w, ir);
    free((void *)w.types);
    free((void *)w.externs);
    return !w.failed;
}

/* Reading */

/* A string that the reader uses as a C string, such as a module path. A
   NUL inside it would cut it short, so the file is damaged then. */
static const char *get_cstr(struct reader *r)
{
    struct name n = antl_get_name(r);

    if (n.length > 0 && memchr(n.text, '\0', n.length) != NULL) {
        antl_damaged(r);
    }
    return n.text;
}

static bool name_equals(const struct name *n, const char *s)
{
    return n->length == strlen(s) && memcmp(n->text, s, n->length) == 0;
}

const struct interface *antl_library(const struct reader *r,
                                     const struct name *module)
{
    size_t i;

    for (i = 0; i < r->library_count; i++) {
        if (name_equals(module, r->libraries[i]->module)) {
            return r->libraries[i];
        }
    }
    return NULL;
}

static bool read_magic(struct reader *r)
{
    uint32_t version;

    if (r->size < sizeof magic || memcmp(r->data, magic, sizeof magic) != 0) {
        antl_fail(r, "is not a library file");
        return false;
    }
    r->pos = sizeof magic;
    version = antl_get_u32(r);
    if (!r->failed && version != ANTL_VERSION) {
        antl_fail(r, "has format version %u, and antic reads version %u",
                  (unsigned)version, (unsigned)ANTL_VERSION);
    }
    return !r->failed;
}

static void read_header(struct reader *r, struct interface *out)
{
    struct package *p = &out->package;
    struct package_dependency *deps;
    const char **lines;
    uint32_t i;

    memset(out, 0, sizeof *out);
    if (!read_magic(r)) {
        return;
    }
    p->name = get_cstr(r);
    p->version = get_cstr(r);
    p->dependency_count = antl_get_count(r, 12);
    deps = antl_allocate(r, p->dependency_count, sizeof *deps);
    for (i = 0; i < p->dependency_count && !r->failed; i++) {
        deps[i].name = get_cstr(r);
        deps[i].constraint = get_cstr(r);
        deps[i].url = get_cstr(r);
    }
    p->dependencies = deps;
    p->license = get_cstr(r);
    p->license_text = get_cstr(r);
    p->attribution_count = antl_get_count(r, 4);
    lines = antl_allocate(r, p->attribution_count, sizeof *lines);
    for (i = 0; i < p->attribution_count && !r->failed; i++) {
        lines[i] = get_cstr(r);
    }
    p->attribution = lines;
    out->module = get_cstr(r);
    out->import_count = antl_get_count(r, 4);
    out->imports = antl_allocate(r, out->import_count, sizeof *out->imports);
    for (i = 0; i < out->import_count && !r->failed; i++) {
        out->imports[i] = get_cstr(r);
    }
    out->framework_count = antl_get_count(r, 4);
    out->frameworks = antl_allocate(r, out->framework_count,
                                    sizeof *out->frameworks);
    for (i = 0; i < out->framework_count && !r->failed; i++) {
        out->frameworks[i] = get_cstr(r);
    }
    out->linux_library_count = antl_get_count(r, 4);
    out->linux_libraries = antl_allocate(r, out->linux_library_count,
                                         sizeof *out->linux_libraries);
    for (i = 0; i < out->linux_library_count && !r->failed; i++) {
        out->linux_libraries[i] = get_cstr(r);
    }
    out->doc = get_cstr(r);
}

bool antl_header(const uint8_t *data, size_t size, struct arena *arena,
                 struct interface *out, char *error, size_t error_size)
{
    struct reader r;

    memset(&r, 0, sizeof r);
    r.data = data;
    r.size = size;
    r.error = error;
    r.error_size = error_size;
    r.arena = arena;
    read_header(&r, out);
    return !r.failed;
}

/* Whether the function type fn of a member of class takes `self`: its
   first parameter is `*class`. */
static bool takes_self(const struct type *fn, const struct type *class)
{
    const struct type *first = fn->param_count > 0 ? fn->params[0] : NULL;

    return first != NULL && first->kind == TYPE_POINTER &&
           !first->nullable && first->element == class;
}

/* A bitfield as the checker admits one. Its type is an integer of a fixed
   width, it has no more bits than the integer, and it is not `_`. */
static bool bitfield_fits(const struct struct_field *f)
{
    return !type_field_is_unit_break(f) && type_is_integer(f->type) &&
           !type_is_target_sized(f->type) &&
           f->bits <= (unsigned)type_bits(f->type);
}

/* The type of the index the file holds next, below limit. ANTL_NO_TYPE
   gives NULL where none is allowed. */
static struct type *read_type_index(struct reader *r, uint32_t limit,
                                    bool none)
{
    uint32_t index = antl_get_u32(r);

    if (r->failed) {
        return NULL;
    }
    if (none && index == ANTL_NO_TYPE) {
        return NULL;
    }
    if (index >= limit) {
        antl_damaged(r);
        return NULL;
    }
    return r->table[index];
}

struct type *antl_type_ref(struct reader *r, uint32_t limit)
{
    return read_type_index(r, limit, false);
}

struct type *antl_type_or_none(struct reader *r)
{
    return read_type_index(r, r->table_count, true);
}

static bool name_equals_name(const struct name *a, const struct name *b)
{
    return a->length == b->length &&
           memcmp(a->text, b->text, a->length) == 0;
}

/* Whether module and name are those of `anti.lang` and the item text,
   which the compiler declares. */
static bool names_lang(const struct name *module, const struct name *name,
                       const char *text)
{
    static const struct name lang = {LANG_MODULE, sizeof LANG_MODULE - 1};
    struct name wanted;

    wanted.text = text;
    wanted.length = strlen(text);
    return name_equals_name(module, &lang) && name_equals_name(name, &wanted);
}

/* A struct of another module is the struct that module's library file
   declared. */
static struct type *foreign_struct(struct reader *r, const struct name *module,
                                   const struct name *name)
{
    const struct interface *lib;
    size_t i;

    /* The root of every class chain is the compiler's own, not a module
       any library file declares. */
    if (names_lang(module, name, LANG_OBJECT)) {
        return types_object(r->types);
    }
    if (names_lang(module, name, LANG_FLAGS)) {
        return types_flags(r->types);
    }
    if (names_lang(module, name, LANG_FIELD_DESCRIPTOR)) {
        return types_field_descriptor(r->types);
    }
    lib = antl_library(r, module);
    if (lib == NULL) {
        antl_fail(r, "needs module `%.*s`", (int)module->length, module->text);
        return NULL;
    }
    for (i = 0; i < lib->item_count; i++) {
        const struct symbol *sym = lib->items[i];
        if (sym->kind == SYMBOL_STRUCT && !sym->alias &&
            sym->name.length == name->length &&
            memcmp(sym->name.text, name->text, name->length) == 0) {
            return sym->type;
        }
    }
    /* A copy may name a private generic of another module, which the
       section of the generics of that module declares. */
    for (i = 0; i < lib->generic_count; i++) {
        const struct item *it = lib->generics[i];
        if (it->kind != ITEM_FN && name_equals_name(&it->name, name)) {
            return it->symbol->type;
        }
    }
    antl_fail(r, "needs struct `%.*s.%.*s`", (int)module->length, module->text,
              (int)name->length, name->text);
    return NULL;
}

static bool symbolic_op_ok(enum symbolic_kind kind, uint8_t op)
{
    switch (kind) {
    case SYMBOLIC_UNARY:
        return op == TOKEN_MINUS || op == TOKEN_TILDE || op == TOKEN_BANG;
    case SYMBOLIC_CAST:
        return op == TOKEN_AS;
    default:
        return (op >= TOKEN_PLUS && op <= TOKEN_CARET) ||
               (op >= TOKEN_SHL && op <= TOKEN_GE);
    }
}

/* A symbolic value whose types are the first limit entries of the type
   table. */
const struct symbolic *antl_read_symbolic(struct reader *r, uint32_t limit,
                                          int depth)
{
    struct symbolic key;
    uint8_t kind = antl_get_u8(r);

    memset(&key, 0, sizeof key);
    key.kind = (enum symbolic_kind)kind;
    key.type = antl_type_ref(r, limit);
    if (r->failed || kind > SYMBOLIC_PARAM || depth > 64) {
        antl_damaged(r);
        return NULL;
    }
    switch (key.kind) {
    case SYMBOLIC_INT:
        key.value = antl_get_u64(r);
        break;
    case SYMBOLIC_SIZE_OF:
        key.of = antl_type_ref(r, limit);
        break;
    case SYMBOLIC_UNARY:
    case SYMBOLIC_CAST:
    case SYMBOLIC_BINARY:
        key.op = (enum token_kind)antl_get_u8(r);
        if (!r->failed && !symbolic_op_ok(key.kind, (uint8_t)key.op)) {
            antl_damaged(r);
            return NULL;
        }
        key.a = antl_read_symbolic(r, limit, depth + 1);
        if (key.kind == SYMBOLIC_BINARY && !r->failed) {
            key.b = antl_read_symbolic(r, limit, depth + 1);
        }
        break;
    case SYMBOLIC_PARAM:
        key.of = antl_type_ref(r, limit);
        if (!r->failed && (key.of->kind != TYPE_PARAM ||
                           key.of->param == NULL ||
                           !key.of->param->constant)) {
            antl_damaged(r);
            return NULL;
        }
        break;
    }
    if (r->failed || !(type_is_integer(key.type) || key.type->kind == TYPE_BOOL)) {
        antl_damaged(r);
        return NULL;
    }
    return types_symbolic(r->types, &key);
}

struct field_refs {
    struct type *s;
    uint32_t count;
    struct struct_field *fields;
    uint32_t *types;
    /* The public functions of a class body, with the index of the
       function type of each. */
    uint32_t member_count;
    struct item **members;
    uint32_t *member_types;
};

/* The defaults of the parameters of sym, whose type is in place. */
void antl_read_param_defaults(struct reader *r, struct symbol *sym)
{
    uint32_t count = antl_get_u32(r);
    struct param_default *list;
    uint32_t i;

    if (r->failed || count == 0) {
        return;
    }
    if (sym->type == NULL || sym->type->kind != TYPE_FN ||
        count > sym->type->param_count) {
        antl_damaged(r);
        return;
    }
    list = antl_allocate(r, count, sizeof *list);
    for (i = 0; i < count && !r->failed; i++) {
        uint8_t kind = antl_get_u8(r);
        struct const_value *v;
        memset(&list[i], 0, sizeof list[i]);
        if (kind == 2) {
            list[i].here = true;
        } else if (kind == 1) {
            v = antl_allocate(r, 1, sizeof *v);
            if (!antl_read_value(r, sym->type->params[i], v, 0)) {
                antl_damaged(r);
                return;
            }
            list[i].value = v;
        } else if (kind != 0) {
            antl_damaged(r);
            return;
        }
    }
    sym->defaults = list;
    sym->default_count = count;
}

/* The `own` parameters of sym, whose type is in place. */
void antl_read_param_owned(struct reader *r, struct symbol *sym)
{
    uint32_t count = antl_get_u32(r);
    bool *list;
    uint32_t i;

    if (r->failed || count == 0) {
        return;
    }
    if (sym->type == NULL || sym->type->kind != TYPE_FN ||
        count > sym->type->param_count) {
        antl_damaged(r);
        return;
    }
    list = antl_allocate(r, count, sizeof *list);
    for (i = 0; i < count && !r->failed; i++) {
        uint8_t owned = antl_get_u8(r);
        if (owned > 1) {
            antl_damaged(r);
            return;
        }
        list[i] = owned == 1;
    }
    sym->owned = list;
    sym->owned_count = count;
}

enum { MAP_NONE, MAP_BUSY, MAP_DONE };

/* DESIGN: one limit bounds how deep the tables of a library file nest,
   TYPES_NEST_MAX of types.h, the limit of the checker as well. It covers
   the structs of the type table and the aggregates and the symbolic
   values of the IR. The reader recurses no deeper than the limit when it
   follows an index on demand. A table whose entries point back is read
   without recursion, and it is refused when it nests deeper. types_nest
   measures the type table as it measures the types of the source. The
   checker, the layout and the passes walk the same nesting recursively.
   See docs/decisions.md. */

/* The larger of two heights. */
static uint32_t nest_max(uint32_t height, uint32_t h)
{
    return h > height ? h : height;
}

/* Refuse a type table whose values nest deeper than TYPES_NEST_MAX or
   hold themselves. A type of a library read before was measured by its
   reader. */
static void check_nesting(struct reader *r, uint32_t count)
{
    uint32_t i;

    for (i = 0; i < count && !r->failed; i++) {
        if (type_has_fields(r->table[i]) &&
            types_nest(r->table[i], NULL) != NEST_FITS) {
            antl_damaged(r);
        }
    }
}

/* The number of hooks a type parameter can meet, one bit each. */
enum { HOOK_BITS = 20 };

/* The constraints as written of a parameter or a `constraint`. */
static struct constraint_ref *read_constraint_refs(struct reader *r,
                                                   size_t *count)
{
    uint32_t n = antl_get_count(r, 8);
    struct constraint_ref *refs = antl_allocate(r, n, sizeof *refs);
    uint32_t i;

    for (i = 0; i < n && !r->failed; i++) {
        memset(&refs[i], 0, sizeof refs[i]);
        refs[i].module = antl_get_name(r);
        refs[i].name = antl_get_name(r);
    }
    *count = r->failed ? 0 : n;
    return refs;
}

/* A type parameter of the table, entry at, as put_param_type wrote it. */
static struct type *read_param_type(struct reader *r, uint32_t at)
{
    struct name name = antl_get_name(r);
    uint8_t role = antl_get_u8(r);
    struct type *t;
    uint32_t n;
    uint32_t i;

    if (r->failed) {
        return NULL;
    }
    t = types_param(r->types, name);
    if (role == 1 || role == 2) {
        struct type *owner = antl_type_ref(r, at);
        if (r->failed || owner->kind != TYPE_PARAM || owner->param == NULL ||
            (role == 1 ? owner->walked : owner->indexed) != NULL) {
            antl_damaged(r);
            return NULL;
        }
        if (role == 1) {
            owner->walked = t;
        } else {
            owner->indexed = t;
        }
        t->hook_owner = owner;
    } else if (role == 0) {
        struct type_param *tp = antl_allocate(r, 1, sizeof *tp);
        uint8_t constant = antl_get_u8(r);
        tp->name = name;
        tp->constant = constant == 1;
        tp->constraints = read_constraint_refs(r, &tp->constraint_count);
        tp->type = t;
        t->param = tp;
        if (constant > 1) {
            antl_damaged(r);
        }
    } else if (role == 3) {
        struct item *set = antl_allocate(r, 1, sizeof *set);
        set->kind = ITEM_CONSTRAINT;
        set->name = name;
        set->constraints = read_constraint_refs(r, &set->constraint_count);
        t->declared_by = set;
    } else {
        antl_damaged(r);
        return NULL;
    }
    t->hooks = antl_get_u32(r);
    if (t->hooks >> HOOK_BITS != 0) {
        antl_damaged(r);
    }
    n = antl_get_count(r, 4);
    t->ifaces = antl_allocate(r, n, sizeof *t->ifaces);
    for (i = 0; i < n && !r->failed; i++) {
        const struct type *iface = antl_type_ref(r, at);
        if (r->failed || iface->kind != TYPE_CLASS || !iface->has_abstract) {
            antl_damaged(r);
            return NULL;
        }
        t->ifaces[i] = iface;
    }
    t->iface_count = r->failed ? 0 : n;
    return t;
}

/* The parameters of the generic t, which follow its body. */
static void read_type_params(struct reader *r, uint32_t at, struct type *t)
{
    uint32_t n = antl_get_count(r, 4);
    uint32_t i;

    t->type_params = antl_allocate(r, n, sizeof *t->type_params);
    for (i = 0; i < n && !r->failed; i++) {
        struct type *p = antl_type_ref(r, at);
        if (r->failed || p->kind != TYPE_PARAM || p->param == NULL) {
            antl_damaged(r);
            return;
        }
        t->type_params[i] = p;
    }
    if (n == 0) {
        antl_damaged(r);
        return;
    }
    t->type_param_count = n;
    t->generic_ready = true;
}

/* The generic a copy names and its arguments, each a type or a constant
   as its parameter asks, or NULL when the file is damaged. */
static struct type *read_copy_args(struct reader *r, uint32_t at,
                                   uint8_t kind, struct type ***args_out,
                                   const struct symbolic ***values_out)
{
    struct type *generic = antl_type_ref(r, at);
    struct type **args;
    const struct symbolic **values;
    size_t i;

    if (r->failed || generic->type_param_count == 0 ||
        generic->kind != (enum type_kind)kind) {
        antl_damaged(r);
        return NULL;
    }
    args = antl_allocate(r, generic->type_param_count, sizeof *args);
    values = antl_allocate(r, generic->type_param_count, sizeof *values);
    for (i = 0; i < generic->type_param_count && !r->failed; i++) {
        bool constant = generic->type_params[i]->param->constant;
        uint8_t is_value = antl_get_u8(r);
        if (r->failed || is_value != (constant ? 1 : 0)) {
            antl_damaged(r);
            return NULL;
        }
        if (constant) {
            values[i] = antl_read_symbolic(r, at, 0);
        } else {
            args[i] = antl_type_ref(r, at);
        }
    }
    if (r->failed) {
        return NULL;
    }
    *args_out = args;
    *values_out = values;
    return generic;
}

/* The copy of generic with these arguments that the program has, or
   NULL. Types and symbolic values are interned, so equal arguments are
   equal pointers. */
static struct type *copy_among(struct type *generic, struct type **args,
                               const struct symbolic **values)
{
    struct type *copy;
    size_t i;

    for (copy = generic->copies; copy != NULL; copy = copy->next_copy) {
        bool same = true;
        for (i = 0; i < generic->type_param_count && same; i++) {
            same = copy->args[i] == args[i] && copy->values[i] == values[i];
        }
        if (same) {
            return copy;
        }
    }
    return NULL;
}

static void read_types(struct reader *r)
{
    uint32_t count = antl_get_count(r, 1);
    struct field_refs *structs =
        alloc_zeroed((size_t)count + 1, sizeof *structs);
    uint32_t struct_count = 0;
    uint32_t i;
    uint32_t j;

    r->table = antl_allocate(r, count, sizeof *r->table);
    for (i = 0; i < count && !r->failed; i++) {
        uint8_t kind = antl_get_u8(r);
        struct type *t = NULL;
        switch (kind) {
        case TYPE_POINTER: {
            struct type *element = antl_type_ref(r, i);
            uint8_t form = antl_get_u8(r);
            if (form > 3) {
                antl_damaged(r);
            }
            if (element != NULL && !r->failed) {
                t = types_pointer_of(r->types, element, (form & 1) != 0);
                if ((form & 2) != 0) {
                    t = types_lent(r->types, t);
                }
            }
            break;
        }
        case TYPE_SLICE: {
            struct type *element = antl_type_ref(r, i);
            uint8_t lent = antl_get_u8(r);
            if (lent > 1) {
                antl_damaged(r);
            }
            if (element != NULL && !r->failed) {
                t = types_slice(r->types, element);
                if (lent != 0) {
                    t = types_lent(r->types, t);
                }
            }
            break;
        }
        /* The element of a `?T` is never a `*U` or a `fn(...)`, which
           would make a `?*U` of it. */
        case TYPE_OPTIONAL:
            t = antl_type_ref(r, i);
            if (t != NULL &&
                ((t->kind == TYPE_POINTER || t->kind == TYPE_FN) &&
                 !t->nullable)) {
                antl_damaged(r);
                t = NULL;
            }
            if (t != NULL) {
                t = types_with_none(r->types, t);
            }
            break;
        case TYPE_ARRAY: {
            struct type *element = antl_type_ref(r, i);
            if (antl_get_u8(r) != 0) {
                const struct symbolic *length = antl_read_symbolic(r, i, 0);
                if (element != NULL && length != NULL) {
                    t = types_array_symbolic(r->types, element, length);
                }
            } else {
                uint64_t length = antl_get_u64(r);
                if (element != NULL && !r->failed && length > 0) {
                    t = types_array(r->types, element, length);
                }
            }
            break;
        }
        case TYPE_FN: {
            uint32_t n = antl_get_count(r, 4);
            struct type **params = antl_allocate(r, n, sizeof *params);
            uint8_t flags;
            for (j = 0; j < n && !r->failed; j++) {
                params[j] = antl_type_ref(r, i);
            }
            t = antl_type_ref(r, i);
            flags = antl_get_u8(r);
            if (r->failed) {
                break;
            }
            /* The out pointer belongs to the `may fail` form alone, and it
               is the last parameter. `concurrent` marks the form of two
               words alone, which a bound function never has. */
            if (flags > 127 || ((flags & 32) != 0 && (flags & 16) == 0) ||
                ((flags & 64) != 0 && (flags & 48) != 48) ||
                ((flags & 16) != 0 && (flags & 2) != 0) ||
                ((flags & 8) != 0 &&
                 ((flags & 4) == 0 || n == 0 ||
                  params[n - 1] == NULL ||
                  params[n - 1]->kind != TYPE_POINTER))) {
                antl_damaged(r);
                t = NULL;
                break;
            }
            t = types_fn_flagged(r->types, params, n, t, (flags & 2) != 0,
                                 (flags & 4) != 0, (flags & 8) != 0);
            if ((flags & 64) != 0) {
                t = types_fn_owned(r->types, t);
            } else if ((flags & 16) != 0) {
                t = types_fn_form(r->types, t, true, (flags & 32) != 0);
            }
            if ((flags & 1) != 0) {
                t = types_with_none(r->types, t);
            }
            break;
        }
        /* The elements name types written before them, so the tuple this
           module reads is the one every other module of the program
           interns. */
        case TYPE_TUPLE: {
            uint32_t n = antl_get_count(r, 4);
            struct type **elements = antl_allocate(r, n, sizeof *elements);
            for (j = 0; j < n && !r->failed; j++) {
                elements[j] = antl_type_ref(r, i);
            }
            if (n < 2) {
                antl_damaged(r);
            }
            if (!r->failed) {
                t = types_tuple(r->types, elements, n);
            }
            break;
        }
        case TYPE_STRUCT:
        case TYPE_CLASS:
        case TYPE_VARIANT: {
            struct name module = antl_get_name(r);
            struct name name = antl_get_name(r);
            uint8_t struct_form_byte = antl_get_u8(r);
            struct type *generic = NULL;
            struct type **args = NULL;
            const struct symbolic **values = NULL;
            struct type *existing = NULL;
            uint8_t flags;
            uint8_t safety;
            if (r->failed) {
                break;
            }
            if (struct_form_byte > FORM_COPY) {
                antl_damaged(r);
                break;
            }
            if (struct_form_byte == FORM_COPY) {
                generic = read_copy_args(r, i, kind, &args, &values);
                if (generic == NULL) {
                    break;
                }
                existing = copy_among(generic, args, values);
            } else if (kind == TYPE_STRUCT &&
                       names_lang(&module, &name, LANG_CHAN)) {
                struct type *element = antl_type_ref(r, i);
                if (!r->failed) {
                    t = types_chan(r->types, element);
                }
                break;
            }
            if (kind == TYPE_STRUCT && names_lang(&module, &name, LANG_MUTEX)) {
                t = types_mutex(r->types);
                break;
            }
            if (kind == TYPE_STRUCT && names_lang(&module, &name, LANG_REGEX)) {
                t = types_regex(r->types);
                break;
            }
            if (kind == TYPE_STRUCT &&
                names_lang(&module, &name, LANG_BYTE_REGEX)) {
                t = types_byte_regex(r->types);
                break;
            }
            if (kind == TYPE_STRUCT &&
                names_lang(&module, &name, LANG_BYTE_MATCH)) {
                t = types_match_of(r->types, true);
                break;
            }
            /* A match of a literal is written as the plain match of its
               form, since the literal stays in the module that wrote
               it. */
            if (kind == TYPE_STRUCT && names_lang(&module, &name, LANG_MATCH)) {
                t = types_match(r->types, NULL);
                break;
            }
            if (kind == TYPE_STRUCT &&
                names_lang(&module, &name, LANG_OBJECT_LOCK)) {
                t = types_object_lock(r->types);
                break;
            }
            /* The root carries the path of `anti.lang` and is still
               no struct of its library file. */
            if ((struct_form_byte != FORM_COPY &&
                 !name_equals(&module, r->iface->module)) ||
                names_lang(&module, &name, LANG_OBJECT) ||
                names_lang(&module, &name, LANG_FLAGS) ||
                names_lang(&module, &name, LANG_FIELD_DESCRIPTOR)) {
                t = foreign_struct(r, &module, &name);
                break;
            }
            struct field_refs *s = &structs[struct_count++];
            t = types_struct(r->types, module, name);
            t->kind = (enum type_kind)kind;
            flags = antl_get_u8(r);
            t->is_union = (flags & 1) != 0;
            t->packed = (flags & 2) != 0;
            t->has_abstract = (flags & 4) != 0;
            t->is_final = (flags & 8) != 0;
            t->simd = (flags & 16) != 0;
            t->traced = (flags & 32) != 0;
            safety = antl_get_u8(r);
            t->safety = (enum thread_safety)(safety & 3);
            t->unchecked_fields = (safety >> 2 & 1) != 0;
            if ((safety & 3) > SAFETY_CONCURRENT || safety > 7) {
                antl_damaged(r);
            }
            t->compatible = antl_get_name(r);
            t->align = antl_get_u64(r);
            if (flags > 63 || (t->align & (t->align - 1)) != 0) {
                antl_damaged(r);
            }
            s->s = t;
            s->count = antl_get_count(r, 9);
            s->fields = antl_allocate(r, s->count, sizeof *s->fields);
            s->types = antl_allocate(r, s->count, sizeof *s->types);
            for (j = 0; j < s->count && !r->failed; j++) {
                struct name doc;
                uint8_t form;
                uint8_t marks;
                s->fields[j].name = antl_get_name(r);
                s->types[j] = antl_get_u32(r);
                s->fields[j].bits = antl_get_u8(r);
                form = antl_get_u8(r);
                s->fields[j].form = (enum field_form)(form & 15);
                s->fields[j].owned = (form >> 4 & 1) != 0;
                s->fields[j].atomic = (form >> 5 & 1) != 0;
                s->fields[j].writable = (form >> 6 & 1) != 0;
                s->fields[j].transient = (form >> 7 & 1) != 0;
                s->fields[j].vis = (enum visibility)antl_get_u8(r);
                marks = antl_get_u8(r);
                s->fields[j].injected = (marks & 1) != 0;
                s->fields[j].inject_final = (marks >> 1 & 1) != 0;
                s->fields[j].hidden = (marks >> 2 & 1) != 0;
                s->fields[j].unchecked = (marks >> 3 & 1) != 0;
                s->fields[j].guard = antl_get_name(r);
                if ((form & 15) > FIELD_IMPL || s->fields[j].vis > VIS_PUB ||
                    marks > 15) {
                    antl_damaged(r);
                }
                doc = antl_get_name(r);
                s->fields[j].doc.text = doc.text;
                s->fields[j].doc.length = doc.length;
            }
            if (s->count == 0) {
                antl_damaged(r);
            }
            s->member_count = antl_get_count(r, 8);
            s->members = antl_allocate(r, s->member_count, sizeof *s->members);
            s->member_types =
                antl_allocate(r, s->member_count, sizeof *s->member_types);
            for (j = 0; j < s->member_count && !r->failed; j++) {
                struct item *m = arena_alloc(r->arena, sizeof *m);
                struct symbol *sym = arena_alloc(r->arena, sizeof *sym);
                uint8_t marks;
                struct name note;
                memset(m, 0, sizeof *m);
                memset(sym, 0, sizeof *sym);
                m->name = antl_get_name(r);
                m->qualifier = antl_get_name(r);
                s->member_types[j] = antl_get_u32(r);
                marks = antl_get_u8(r);
                m->contract = (enum fn_contract)(marks & 15);
                m->is_final = (marks >> 4 & 1) != 0;
                m->is_operator = (marks >> 5 & 1) != 0;
                m->may_fail = (marks >> 6 & 1) != 0;
                m->vis = (enum visibility)antl_get_u8(r);
                if ((marks & 15) > FN_CONCRETE || m->vis > VIS_PUB) {
                    antl_damaged(r);
                }
                note = antl_get_name(r);
                m->doc.text = note.text;
                m->doc.length = note.length;
                m->kind = ITEM_FN;
                m->pub = m->vis == VIS_PUB;
                m->symbol = sym;
                /* The parameter names the declaration wrote, which
                   `anti doc` and the generated header print. */
                {
                    uint32_t total = antl_get_count(r, 4);
                    struct name *names = antl_allocate(r, total, sizeof *names);
                    struct param *list = antl_allocate(r, total, sizeof *list);
                    uint32_t k;
                    for (k = 0; k < total && !r->failed; k++) {
                        names[k] = antl_get_name(r);
                        memset(&list[k], 0, sizeof list[k]);
                        list[k].name = names[k];
                    }
                    if (total > 0 && !r->failed) {
                        sym->params = names;
                        m->params = list;
                        m->param_count = total;
                    }
                }
                sym->kind = SYMBOL_FN;
                sym->item = m;
                sym->home = r->iface;
                sym->may_fail = m->may_fail;
                sym->doc = m->doc;
                s->members[j] = m;
            }
            if (struct_form_byte == FORM_GENERIC) {
                read_type_params(r, i, t);
            } else if (struct_form_byte == FORM_COPY && existing != NULL) {
                /* The body read stays behind, and the copy the program
                   has already stands for it. */
                t = existing;
            } else if (struct_form_byte == FORM_COPY) {
                if (s->member_count > 0) {
                    antl_damaged(r);
                }
                t->generic = generic;
                t->args = args;
                t->values = values;
                t->next_copy = generic->copies;
                generic->copies = t;
            }
            break;
        }
        case TYPE_PARAM:
            t = read_param_type(r, i);
            break;
        /* An enum carries its values, each with a name and a number. */
        case TYPE_ENUM: {
            struct name module = antl_get_name(r);
            struct name name = antl_get_name(r);
            struct type *base;
            uint32_t n;
            struct struct_field *values;
            if (r->failed) {
                break;
            }
            base = antl_type_ref(r, i);
            n = antl_get_count(r, 8);
            values = antl_allocate(r, n, sizeof *values);
            /* The values of an enum are integers. */
            t = base != NULL && type_is_integer(base)
                    ? types_enum(r->types, module, name, base)
                    : NULL;
            for (j = 0; j < n && !r->failed; j++) {
                struct name doc;
                memset(&values[j], 0, sizeof values[j]);
                values[j].name = antl_get_name(r);
                values[j].number = antl_get_u64(r);
                values[j].type = t;
                doc = antl_get_name(r);
                values[j].doc.text = doc.text;
                values[j].doc.length = doc.length;
            }
            if (n == 0 || t == NULL) {
                antl_damaged(r);
            } else if (!r->failed) {
                types_set_fields(r->types, t, values, n);
            }
            break;
        }
        /* The tree of a generic carries the type of `none` before a
           context gives it one. It carries the error type as well, on
           the name of a module before a `.`, which no pass reads. */
        default:
            if (kind <= TYPE_ERROR) {
                t = types_builtin(r->types, (enum type_kind)kind);
            } else {
                antl_damaged(r);
            }
            break;
        }
        if (t == NULL) {
            antl_damaged(r);
        }
        r->table[i] = t;
    }
    r->table_count = r->failed ? 0 : count;
    for (i = 0; i < struct_count && !r->failed; i++) {
        struct field_refs *s = &structs[i];
        for (j = 0; j < s->count; j++) {
            if (s->types[j] >= count ||
                r->table[s->types[j]]->kind == TYPE_VOID) {
                antl_damaged(r);
                break;
            }
            s->fields[j].type = r->table[s->types[j]];
            if (s->fields[j].bits != 0 && !bitfield_fits(&s->fields[j])) {
                antl_damaged(r);
                break;
            }
        }
        if (!r->failed) {
            types_set_fields(r->types, s->s, s->fields, s->count);
            if (s->s->simd && !antl_verify_simd_type(s->s)) {
                antl_damaged(r);
            }
            /* The base of a class is the type of its field 0, so the
               chain is whole once the field types are in place. */
            if (s->s->kind == TYPE_CLASS && s->count > 0 &&
                s->fields[0].form == FIELD_BASE) {
                s->s->base = s->fields[0].type;
            }

        }
        /* DESIGN: a class of a library carries the public functions of
           its body. The reader builds one item per function. It has the
           name `T.f` or `T.Q.f` of its symbol, so a call resolves and
           reaches the symbol the library defines. */
        for (j = 0; j < s->member_count && !r->failed; j++) {
            struct item *m = s->members[j];
            struct type *fn;
            if (s->member_types[j] >= count) {
                antl_damaged(r);
                break;
            }
            /* A member is a function. It takes `self` when its first
               parameter is a pointer to the class, as the checker makes
               it, and a `get` of a singleton takes none. The names the
               declaration wrote are the rest, less the out pointer of
               `may fail`. */
            fn = r->table[s->member_types[j]];
            if (fn->kind != TYPE_FN || fn->bound) {
                antl_damaged(r);
                break;
            }
            m->has_self = takes_self(fn, s->s);
            if (fn->param_count != m->param_count + (m->has_self ? 1u : 0u) +
                                       (fn->has_out ? 1u : 0u)) {
                antl_damaged(r);
                break;
            }
            m->symbol->type = fn;
            m->symbol->name =
                types_member_symbol(r->arena, &s->s->name, m);
        }
        if (!r->failed && s->member_count > 0) {
            s->s->members = s->members;
            s->s->member_count = s->member_count;
        }
    }
    /* A copy has the functions of its generic, whose types name the
       parameters. The checker puts the arguments in at each use. */
    for (i = 0; i < struct_count && !r->failed; i++) {
        struct type *t = structs[i].s;
        if (t->generic != NULL) {
            t->members = t->generic->members;
            t->member_count = t->generic->member_count;
        }
    }
    /* The cases of a variant come from its union, whose fields are in
       place once every struct of the table has them. */
    for (i = 0; i < struct_count && !r->failed; i++) {
        if (structs[i].s->kind == TYPE_VARIANT &&
            !types_cases_from_fields(r->types, structs[i].s)) {
            antl_damaged(r);
        }
    }
    for (i = 0; i < struct_count && !r->failed; i++) {
        struct type *t = structs[i].s;
        for (j = 0; j < t->field_count && !r->failed; j++) {
            struct const_value *v;
            if (antl_get_u8(r) == 0) {
                continue;
            }
            v = antl_allocate(r, 1, sizeof *v);
            if (!antl_read_value(r, t->fields[j].type, v, 0)) {
                antl_damaged(r);
                break;
            }
            t->fields[j].constant = v;
        }
        for (j = 0; j < structs[i].member_count && !r->failed; j++) {
            antl_read_param_defaults(r, structs[i].members[j]->symbol);
            antl_read_param_owned(r, structs[i].members[j]->symbol);
        }
    }
    if (!r->failed) {
        check_nesting(r, count);
    }
    free(structs);
}

/* A constant of type t. Aggregates hold one value per element or field,
   which the constant evaluator relies on. */
bool antl_read_value(struct reader *r, struct type *t, struct const_value *v,
                     int depth)
{
    uint8_t kind = antl_get_u8(r);
    uint32_t i;
    uint32_t n;

    memset(v, 0, sizeof *v);
    v->type = t;
    v->kind = (enum const_kind)kind;
    if (r->failed || depth > 64) {
        antl_damaged(r);
        return false;
    }
    switch (kind) {
    case CONST_INT:
        v->as.integer = antl_get_u64(r);
        return !r->failed && (type_is_integer(t) || t->kind == TYPE_BOOL ||
                              t->kind == TYPE_ENUM);
    case CONST_FLOAT: {
        uint64_t bits = antl_get_u64(r);
        memcpy(&v->as.floating, &bits, sizeof bits);
        return !r->failed && (type_is_float(t) || t->kind == TYPE_F16);
    }
    case CONST_BOOL:
        v->as.boolean = antl_get_u8(r) != 0;
        return !r->failed && t->kind == TYPE_BOOL;
    case CONST_CHAR:
        v->as.character = antl_get_u32(r);
        return !r->failed && t->kind == TYPE_CHAR;
    case CONST_NULL:
        /* `none` stands for a pointer or a function that may be none. */
        return (t->kind == TYPE_POINTER || t->kind == TYPE_FN) && t->nullable;
    case CONST_TEXT: {
        struct name bytes = antl_get_name(r);
        v->as.text.bytes = bytes.text;
        v->as.text.length = bytes.length;
        /* Text is a `str` or the bytes of `b"..."`. */
        return !r->failed &&
               (t->kind == TYPE_STR ||
                (t->kind == TYPE_SLICE && t->element->kind == TYPE_U8));
    }
    case CONST_SYMBOLIC:
        v->as.symbolic = antl_read_symbolic(r, r->table_count, 0);
        return !r->failed && v->as.symbolic->type == t;
    case CONST_ARRAY:
    case CONST_STRUCT:
        n = antl_get_count(r, 1);
        if (r->failed || (kind == CONST_ARRAY
                              ? t->kind != TYPE_ARRAY || n != t->length
                              : !type_has_fields(t) ||
                                    n != t->field_count)) {
            return false;
        }
        v->as.aggregate.count = n;
        v->as.aggregate.items =
            antl_allocate(r, n, sizeof *v->as.aggregate.items);
        for (i = 0; i < n; i++) {
            struct type *item = kind == CONST_ARRAY ? t->element
                                                    : t->fields[i].type;
            if (!antl_read_value(r, item, &v->as.aggregate.items[i],
                                 depth + 1)) {
                return false;
            }
            /* A field a class literal leaves out stands in the value of
               a class alone, and its base is a value of the base. */
            if (v->as.aggregate.items[i].kind == CONST_DEFAULT &&
                (kind != CONST_STRUCT || t->kind != TYPE_CLASS ||
                 t->fields[i].form == FIELD_BASE)) {
                return false;
            }
        }
        return true;
    case CONST_DEFAULT:
        return depth > 0;
    default:
        return false;
    }
}

static void read_items(struct reader *r)
{
    struct interface *iface = r->iface;
    uint32_t count = antl_get_count(r, 14);
    uint32_t i;

    iface->items = antl_allocate(r, count, sizeof *iface->items);
    r->marked_generic = antl_allocate(r, count, sizeof *r->marked_generic);
    for (i = 0; i < count && !r->failed; i++) {
        struct symbol *sym = arena_alloc(r->arena, sizeof *sym);
        uint8_t kind = antl_get_u8(r);
        bool generic = false;
        bool ok;
        sym->name = antl_get_name(r);
        sym->type = antl_type_ref(r, r->table_count);
        {
            uint8_t marks = antl_get_u8(r);
            sym->exported = (marks & 1) != 0;
            sym->internal = (marks >> 1 & 1) != 0;
            sym->may_fail = (marks >> 2 & 1) != 0;
            sym->worker = (marks >> 3 & 1) != 0;
            sym->alias = (marks >> 4 & 1) != 0;
            /* A generic function is linked to its declaration once the
               section of the generics is read. */
            generic = (marks >> 5 & 1) != 0;
            sym->is_operator = (marks >> 6 & 1) != 0;
            if (marks > 127 || (sym->alias && kind != SYMBOL_STRUCT) ||
                ((generic || sym->is_operator) && kind != SYMBOL_FN)) {
                antl_damaged(r);
            }
        }
        {
            struct name doc = antl_get_name(r);
            sym->doc.text = doc.text;
            sym->doc.length = doc.length;
        }
        if (sym->exported && kind == SYMBOL_STRUCT && sym->type != NULL) {
            sym->type->item_exported = true;
        }
        sym->kind = (enum symbol_kind)kind;
        sym->state = EVAL_DONE;
        sym->home = iface;
        if (r->failed) {
            break;
        }
        switch (kind) {
        case SYMBOL_FN:
        case SYMBOL_EXTERN_FN:
            ok = sym->type->kind == TYPE_FN;
            if (ok) {
                size_t j;
                struct name *names = antl_allocate(r, sym->type->param_count,
                                                   sizeof *names);
                for (j = 0; j < sym->type->param_count; j++) {
                    names[j] = antl_get_name(r);
                }
                sym->params = names;
                antl_read_param_defaults(r, sym);
                antl_read_param_owned(r, sym);
            }
            if (kind == SYMBOL_EXTERN_FN) {
                sym->variadic = antl_get_u8(r) != 0;
            }
            ok = ok && !r->failed;
            break;
        case SYMBOL_STRUCT:
            /* A struct, a class, a variant and an enum share this
               symbol kind. The name a `type` declares may stand for
               any type. */
            ok = sym->alias ||
                 ((type_has_fields(sym->type) ||
                   sym->type->kind == TYPE_ENUM) &&
                  name_equals(&sym->type->module, iface->module));
            break;
        case SYMBOL_CONSTRAINT:
            ok = sym->type->kind == TYPE_PARAM && sym->type->param == NULL &&
                 sym->type->hook_owner == NULL;
            break;
        case SYMBOL_CONST:
            sym->value = arena_alloc(r->arena, sizeof *sym->value);
            ok = antl_read_value(r, sym->type, sym->value, 0);
            break;
        default:
            ok = false;
            break;
        }
        if (!ok) {
            antl_damaged(r);
        }
        r->marked_generic[iface->item_count] = generic;
        iface->items[iface->item_count++] = sym;
    }
}

/* The IR */

static bool valid_type(uint8_t type)
{
    return type <= IR_LOCK;
}

/* Where each function, global, aggregate and symbolic value of the file
   went in the program. */
struct ir_maps {
    uint32_t *files;                /* the program's index of each file */
    uint32_t file_count;
    uint32_t *functions;
    uint32_t function_count;
    uint32_t *globals;
    uint32_t global_count;
    struct ir_aggtype *aggs;        /* as read, with indices of the file */
    uint32_t *agg_map;
    uint8_t *agg_state;
    uint32_t *agg_height;           /* how deep each aggregate nests */
    uint32_t agg_count;
    struct ir_sym *syms;            /* as read, with indices of the file */
    uint32_t *sym_map;
    uint8_t *sym_state;
    uint32_t *sym_height;           /* how deep each value nests */
    uint32_t sym_count;
};

static uint32_t map_sym_at(struct reader *r, struct ir_module *program,
                           struct ir_maps *maps, uint32_t sym, uint32_t depth);

/* The program's index of aggregate agg of the file. The types an
   aggregate is built from are added before it. depth counts the
   aggregates and values this call is mapped for. */
static uint32_t map_agg_at(struct reader *r, struct ir_module *program,
                           struct ir_maps *maps, uint32_t agg, uint32_t depth)
{
    struct ir_aggtype *t;
    uint32_t height = 0;
    size_t i;

    if (agg >= maps->agg_count || maps->agg_state[agg] == MAP_BUSY ||
        depth >= TYPES_NEST_MAX) {
        antl_damaged(r);
        return 0;
    }
    if (maps->agg_state[agg] == MAP_DONE) {
        return maps->agg_map[agg];
    }
    maps->agg_state[agg] = MAP_BUSY;
    t = &maps->aggs[agg];
    for (i = 0; i < t->field_count && !r->failed; i++) {
        if (t->fields[i].type.type == IR_AGG) {
            uint32_t of = t->fields[i].type.agg;
            t->fields[i].type.agg = map_agg_at(r, program, maps, of,
                                               depth + 1);
            if (!r->failed) {
                height = nest_max(height, maps->agg_height[of]);
            }
        }
    }
    if (r->failed) {
        return 0;
    }
    if (t->kind == IR_AGG_ARRAY) {
        uint32_t length = map_sym_at(r, program, maps, t->length, depth + 1);
        if (!r->failed) {
            height = nest_max(height, maps->sym_height[t->length]);
        }
        maps->agg_map[agg] = r->failed ? 0
                                       : ir_array_add(program, t->name,
                                                      t->fields[0].type,
                                                      length, t->length_text);
    } else if (t->simd) {
        maps->agg_map[agg] = ir_simd_add(program, t->name, t->fields,
                                         t->field_count);
    } else {
        maps->agg_map[agg] = ir_struct_add(program, t->kind, t->name, t->fields,
                                           t->field_count, t->packed,
                                           t->align);
    }
    maps->agg_state[agg] = MAP_DONE;
    maps->agg_height[agg] = height + 1;
    if (height + 1 > TYPES_NEST_MAX) {
        antl_damaged(r);
    }
    return maps->agg_map[agg];
}

static uint32_t map_sym_at(struct reader *r, struct ir_module *program,
                           struct ir_maps *maps, uint32_t sym, uint32_t depth)
{
    struct ir_sym s;
    uint32_t height = 0;

    if (sym >= maps->sym_count || maps->sym_state[sym] == MAP_BUSY ||
        depth >= TYPES_NEST_MAX) {
        antl_damaged(r);
        return 0;
    }
    if (maps->sym_state[sym] == MAP_DONE) {
        return maps->sym_map[sym];
    }
    maps->sym_state[sym] = MAP_BUSY;
    s = maps->syms[sym];
    if (s.kind == IR_SYM_SIZE_OF || s.kind == IR_SYM_OFFSET_OF) {
        if (s.of.type == IR_AGG) {
            s.of.agg = map_agg_at(r, program, maps, s.of.agg, depth + 1);
            if (!r->failed) {
                height = maps->agg_height[maps->syms[sym].of.agg];
            }
        }
        if (!r->failed && s.kind == IR_SYM_OFFSET_OF &&
            (s.of.type != IR_AGG ||
             s.field >= program->aggs[s.of.agg]->field_count)) {
            antl_damaged(r);
        }
    } else if (s.kind == IR_SYM_OP) {
        s.a = map_sym_at(r, program, maps, s.a, depth + 1);
        if (!r->failed) {
            height = maps->sym_height[maps->syms[sym].a];
        }
        if (s.b != IR_NO_AGG) {
            s.b = map_sym_at(r, program, maps, s.b, depth + 1);
            if (!r->failed) {
                height = nest_max(height, maps->sym_height[maps->syms[sym].b]);
            }
        }
    }
    if (r->failed) {
        return 0;
    }
    switch (s.kind) {
    case IR_SYM_INT:
        maps->sym_map[sym] = ir_sym_int(program, s.type, s.value);
        break;
    case IR_SYM_SIZE_OF:
        maps->sym_map[sym] = ir_sym_size_of(program, s.of);
        break;
    case IR_SYM_OFFSET_OF:
        maps->sym_map[sym] = ir_sym_offset_of(program, s.of.agg, s.field);
        break;
    case IR_SYM_OP:
        maps->sym_map[sym] = ir_sym_op(program, (enum ir_op)s.op, s.type, s.a,
                                       s.b);
        break;
    }
    maps->sym_state[sym] = MAP_DONE;
    maps->sym_height[sym] = height + 1;
    if (height + 1 > TYPES_NEST_MAX) {
        antl_damaged(r);
    }
    return maps->sym_map[sym];
}

static uint32_t map_agg(struct reader *r, struct ir_module *program,
                        struct ir_maps *maps, uint32_t agg)
{
    return map_agg_at(r, program, maps, agg, 0);
}

static uint32_t map_sym(struct reader *r, struct ir_module *program,
                        struct ir_maps *maps, uint32_t sym)
{
    return map_sym_at(r, program, maps, sym, 0);
}

static struct ir_vtype read_vtype(struct reader *r, bool scalar_only)
{
    struct ir_vtype v;
    uint8_t type = antl_get_u8(r);

    v.agg = antl_get_u32(r);
    v.type = (enum ir_type)type;
    if (!r->failed && (!valid_type(type) || (scalar_only && type == IR_AGG) ||
                       ((type == IR_AGG) != (v.agg != IR_NO_AGG)))) {
        antl_damaged(r);
    }
    return v;
}

/* Whether the program holds everything that aggregate agg of the file
   is built from. */
static bool agg_ready(const struct ir_maps *maps, uint32_t agg)
{
    const struct ir_aggtype *t = &maps->aggs[agg];
    size_t i;

    for (i = 0; i < t->field_count; i++) {
        uint32_t of = t->fields[i].type.agg;
        if (t->fields[i].type.type == IR_AGG &&
            (of >= maps->agg_count || maps->agg_state[of] != MAP_DONE)) {
            return false;
        }
    }
    return t->kind != IR_AGG_ARRAY ||
           (t->length < maps->sym_count &&
            maps->sym_state[t->length] == MAP_DONE);
}

/* Whether the program holds everything that symbolic value sym of the
   file is computed from. */
static bool sym_ready(const struct ir_maps *maps, uint32_t sym)
{
    const struct ir_sym *s = &maps->syms[sym];

    if ((s->kind == IR_SYM_SIZE_OF || s->kind == IR_SYM_OFFSET_OF) &&
        s->of.type == IR_AGG) {
        return s->of.agg < maps->agg_count &&
               maps->agg_state[s->of.agg] == MAP_DONE;
    }
    if (s->kind == IR_SYM_OP) {
        return s->a < maps->sym_count && maps->sym_state[s->a] == MAP_DONE &&
               (s->b == IR_NO_AGG ||
                (s->b < maps->sym_count && maps->sym_state[s->b] == MAP_DONE));
    }
    return true;
}

/* The aggregate and symbolic tables of the file. They refer to each other
   by index, so both are read before either is added to the program. */
/* A bitfield of the IR has an integer type of a fixed width and no more
   bits than it. */
static bool ir_bitfield_fits(const struct ir_field *f)
{
    switch (f->type.type) {
    case IR_I8: return f->bits <= 8;
    case IR_I16: return f->bits <= 16;
    case IR_I32: return f->bits <= 32;
    case IR_I64: return f->bits <= 64;
    default: return false;
    }
}

static void read_tables(struct reader *r, struct ir_module *program,
                        struct ir_maps *maps)
{
    uint32_t i;
    uint32_t j;
    uint32_t a = 0;

    maps->file_count = antl_get_count(r, 4);
    maps->files = antl_allocate(r, maps->file_count, sizeof *maps->files);
    for (i = 0; i < maps->file_count && !r->failed; i++) {
        const char *path = get_cstr(r);
        if (!r->failed) {
            maps->files[i] = ir_file_add(program, path);
        }
    }
    maps->sym_count = antl_get_count(r, 27);
    maps->syms = antl_allocate(r, maps->sym_count, sizeof *maps->syms);
    maps->sym_map = antl_allocate(r, maps->sym_count, sizeof *maps->sym_map);
    maps->sym_state =
        antl_allocate(r, maps->sym_count, sizeof *maps->sym_state);
    maps->sym_height =
        antl_allocate(r, maps->sym_count, sizeof *maps->sym_height);
    for (i = 0; i < maps->sym_count && !r->failed; i++) {
        struct ir_sym *s = &maps->syms[i];
        uint8_t kind = antl_get_u8(r);
        uint8_t type = antl_get_u8(r);
        s->kind = (enum ir_sym_kind)kind;
        s->type = (enum ir_type)type;
        s->value = antl_get_u64(r);
        s->of = read_vtype(r, false);
        s->field = antl_get_u32(r);
        s->op = antl_get_u8(r);
        s->a = antl_get_u32(r);
        s->b = antl_get_u32(r);
        if (kind > IR_SYM_OP || type < IR_I8 ||
            (type > IR_I64 && type != IR_CLONG && type != IR_CWCHAR &&
             type != IR_LOCK) ||
            s->op > IR_RET ||
            ((kind == IR_SYM_SIZE_OF || kind == IR_SYM_OFFSET_OF) &&
             s->of.type == IR_VOID)) {
            antl_damaged(r);
        }
    }
    maps->agg_count = antl_get_count(r, 26);
    maps->aggs = antl_allocate(r, maps->agg_count, sizeof *maps->aggs);
    maps->agg_map = antl_allocate(r, maps->agg_count, sizeof *maps->agg_map);
    maps->agg_state =
        antl_allocate(r, maps->agg_count, sizeof *maps->agg_state);
    maps->agg_height =
        antl_allocate(r, maps->agg_count, sizeof *maps->agg_height);
    for (i = 0; i < maps->agg_count && !r->failed; i++) {
        struct ir_aggtype *t = &maps->aggs[i];
        uint8_t kind = antl_get_u8(r);
        uint8_t flags;
        t->kind = (enum ir_agg_kind)kind;
        t->name = get_cstr(r);
        flags = antl_get_u8(r);
        t->packed = (flags & 1) != 0;
        t->simd = (flags & 2) != 0;
        t->align = antl_get_u64(r);
        if (flags > 3 || (t->align & (t->align - 1)) != 0) {
            antl_damaged(r);
        }
        t->length = antl_get_u32(r);
        t->length_text = get_cstr(r);
        t->field_count = antl_get_count(r, 10);
        t->fields = antl_allocate(r, t->field_count, sizeof *t->fields);
        for (j = 0; j < t->field_count && !r->failed; j++) {
            uint8_t ext;
            t->fields[j].name = get_cstr(r);
            t->fields[j].type = read_vtype(r, false);
            t->fields[j].bits = antl_get_u8(r);
            ext = antl_get_u8(r);
            t->fields[j].ext = (enum ir_ext)ext;
            if (ext > IR_EXT_ZERO || t->fields[j].type.type == IR_VOID ||
                (t->fields[j].bits != 0 && !ir_bitfield_fits(&t->fields[j]))) {
                antl_damaged(r);
            }
        }
        if (kind > IR_AGG_ARRAY || t->name[0] == '\0' ||
            t->field_count == 0 ||
            (kind == IR_AGG_ARRAY && t->field_count != 1) ||
            (!r->failed && t->simd && !antl_verify_simd_agg(t))) {
            antl_damaged(r);
        }
    }
    /* DESIGN: the file keeps the aggregates and the symbolic values each
       in the order the program made them, and the two orders interleave.
       The reader keeps both orders. It makes the next aggregate when the
       program holds what it is built from, and the next value otherwise.
       The order they were made in is one such interleaving, so one of
       the two is always ready. A library read and written again then
       comes out byte for byte. A file that fits no interleaving is
       mapped on demand. */
    i = 0;
    while (!r->failed && (a < maps->agg_count || i < maps->sym_count)) {
        if (a < maps->agg_count &&
            (maps->agg_state[a] == MAP_DONE || agg_ready(maps, a))) {
            map_agg(r, program, maps, a++);
        } else if (i < maps->sym_count &&
                   (maps->sym_state[i] == MAP_DONE || sym_ready(maps, i))) {
            map_sym(r, program, maps, i++);
        } else if (a < maps->agg_count) {
            map_agg(r, program, maps, a++);
        } else {
            map_sym(r, program, maps, i++);
        }
    }
}

static uint32_t read_agg_ref(struct reader *r, struct ir_module *program,
                             struct ir_maps *maps, uint8_t type)
{
    uint32_t agg = antl_get_u32(r);

    if (r->failed || (type == IR_AGG) != (agg != IR_NO_AGG)) {
        antl_damaged(r);
        return IR_NO_AGG;
    }
    return type == IR_AGG ? map_agg(r, program, maps, agg) : IR_NO_AGG;
}

/* Read a constant tree, mapping its aggregate indices. A global index
   stays as written, because a constant may name a global that comes
   later. remap_const moves them. */
static struct ir_const *read_const(struct reader *r, struct ir_module *program,
                                   struct ir_maps *maps, int depth)
{
    struct ir_const *c;
    uint8_t kind;
    uint8_t scalar;
    uint64_t payload;
    uint64_t i;

    if (depth > 32) {
        antl_damaged(r);
        return NULL;
    }
    kind = antl_get_u8(r);
    scalar = antl_get_u8(r);
    payload = antl_get_u64(r);
    if (r->failed || kind > IR_CONST_AGG || !valid_type(scalar)) {
        antl_damaged(r);
        return NULL;
    }
    c = arena_alloc(r->arena, sizeof *c);
    c->kind = (enum ir_const_kind)kind;
    c->scalar = (enum ir_type)scalar;
    switch (c->kind) {
    case IR_CONST_INT:
        c->integer = payload;
        break;
    case IR_CONST_FLOAT:
        memcpy(&c->floating, &payload, sizeof payload);
        break;
    case IR_CONST_SYM:
        if (payload >= maps->sym_count) {
            antl_damaged(r);
            return NULL;
        }
        c->sym = map_sym(r, program, maps, (uint32_t)payload);
        break;
    case IR_CONST_ADDR:
    case IR_CONST_FUNC:
        c->global = (uint32_t)payload;
        break;
    case IR_CONST_AGG:
        c->type = read_vtype(r, false);
        if (r->failed || c->type.type != IR_AGG) {
            antl_damaged(r);
            return NULL;
        }
        c->type.agg = map_agg(r, program, maps, c->type.agg);
        /* An item costs at least its kind, its type and its payload, so
           a count past that many bytes cannot be honest. */
        if (payload > (uint64_t)(r->size - r->pos) / 10) {
            antl_damaged(r);
            return NULL;
        }
        c->item_count = (size_t)payload;
        c->items = arena_alloc(r->arena,
                               (payload == 0 ? 1 : payload) * sizeof *c->items);
        for (i = 0; i < payload && !r->failed; i++) {
            struct ir_const *item = read_const(r, program, maps, depth + 1);
            if (item == NULL) {
                return NULL;
            }
            c->items[i] = *item;
        }
        break;
    default: /* IR_CONST_NONE */
        break;
    }
    if (!r->failed && !antl_verify_const(c)) {
        antl_damaged(r);
    }
    return r->failed ? NULL : c;
}

/* DESIGN: move the addresses inside a constant to the indices of the
   program. A table entry names a function, and the file lists the
   functions after the globals. The two kinds of address are therefore
   moved in two passes, and `functions` says which pass this is. */
static void remap_const(struct reader *r, struct ir_const *c,
                        const struct ir_maps *maps, bool functions)
{
    size_t i;

    if (c->kind == IR_CONST_ADDR && !functions) {
        if (c->global >= maps->global_count) {
            antl_damaged(r);
            return;
        }
        c->global = maps->globals[c->global];
    } else if (c->kind == IR_CONST_FUNC && functions) {
        if (c->global >= maps->function_count) {
            antl_damaged(r);
            return;
        }
        c->global = maps->functions[c->global];
    } else if (c->kind == IR_CONST_AGG) {
        for (i = 0; i < c->item_count; i++) {
            remap_const(r, &c->items[i], maps, functions);
        }
    }
}

/* The targets of a jump and of a branch are blocks. read_operand has
   checked the index of each block. */
static bool targets_blocks(const struct ir_inst *inst)
{
    if (inst->op == IR_JUMP) {
        return inst->a.kind == IR_BLOCK;
    }
    if (inst->op == IR_BRANCH || inst->op == IR_BRANCH_OV) {
        return inst->b.kind == IR_BLOCK && inst->c.kind == IR_BLOCK;
    }
    return true;
}

static struct ir_operand read_operand(struct reader *r,
                                      const struct ir_function *f,
                                      const struct ir_maps *maps)
{
    struct ir_operand o = {IR_NONE, IR_VOID, {0}};
    uint8_t kind = antl_get_u8(r);
    uint8_t type = antl_get_u8(r);
    uint64_t payload = antl_get_u64(r);

    if (r->failed) {
        return o;
    }
    if (kind > IR_SYM || !valid_type(type)) {
        antl_damaged(r);
        return o;
    }
    o.kind = (enum ir_operand_kind)kind;
    o.type = (enum ir_type)type;
    switch (o.kind) {
    case IR_TEMP:
        if (payload >= f->temp_count || f->temps[payload] != o.type) {
            antl_damaged(r);
        } else {
            o.as.temp = (uint32_t)payload;
        }
        break;
    case IR_INT:
        o.as.integer = payload;
        break;
    case IR_FLOAT:
        memcpy(&o.as.floating, &payload, sizeof payload);
        break;
    case IR_GLOBAL:
        if (payload >= maps->global_count) {
            antl_damaged(r);
        } else {
            o.as.index = maps->globals[payload];
        }
        break;
    case IR_FUNC:
        if (payload >= maps->function_count) {
            antl_damaged(r);
        } else {
            o.as.index = maps->functions[payload];
        }
        break;
    case IR_BLOCK:
        if (payload >= f->block_count) {
            antl_damaged(r);
        } else {
            o.as.index = (uint32_t)payload;
        }
        break;
    case IR_SYM:
        if (payload >= maps->sym_count ||
            maps->syms[payload].type != o.type) {
            antl_damaged(r);
        } else {
            o.as.index = maps->sym_map[payload];
        }
        break;
    case IR_NONE:
        break;
    }
    return o;
}

static void read_body(struct reader *r, struct ir_module *program,
                      struct ir_function *f, struct ir_maps *maps)
{
    uint32_t temps = antl_get_count(r, 1);
    uint32_t blocks;
    uint32_t i;
    uint32_t j;
    uint32_t k;

    for (i = 0; i < temps && !r->failed; i++) {
        uint8_t type = antl_get_u8(r);
        if (i < f->param_count) {
            if (type != f->temps[i]) {
                antl_damaged(r);
            }
        } else if (!valid_type(type) || type == IR_VOID || type == IR_AGG) {
            antl_damaged(r);
        } else {
            ir_temp(f, (enum ir_type)type);
        }
    }
    if (temps < f->param_count) {
        antl_damaged(r);
    }
    blocks = antl_get_count(r, 4);
    /* A body starts at block 0, so it has one. */
    if (blocks == 0) {
        antl_damaged(r);
    }
    for (i = 0; i < blocks && !r->failed; i++) {
        ir_block_add(f);
    }
    for (i = 0; i < blocks && !r->failed; i++) {
        uint32_t count;
        uint8_t fail_kind;
        fail_kind = antl_get_u8(r);
        if (fail_kind > IR_FAIL_CHECK) {
            antl_damaged(r);
        }
        f->blocks[i]->fail = (enum ir_fail)fail_kind;
        count = antl_get_count(r, 50);
        for (j = 0; j < count && !r->failed; j++) {
            struct ir_inst inst;
            struct ir_operand *args;
            uint8_t op = antl_get_u8(r);
            uint8_t type = antl_get_u8(r);
            memset(&inst, 0, sizeof inst);
            inst.line = antl_get_u32(r);
            inst.result = antl_get_u32(r);
            inst.op = (enum ir_op)op;
            inst.type = (enum ir_type)type;
            if (op > IR_RET || !valid_type(type) ||
                (inst.result != IR_NO_RESULT && inst.result >= f->temp_count)) {
                antl_damaged(r);
                break;
            }
            inst.a = read_operand(r, f, maps);
            inst.b = read_operand(r, f, maps);
            inst.c = read_operand(r, f, maps);
            if (!targets_blocks(&inst)) {
                antl_damaged(r);
                break;
            }
            inst.of = read_vtype(r, false);
            inst.field = antl_get_u32(r);
            if (!r->failed && inst.of.type == IR_AGG) {
                inst.of.agg = map_agg(r, program, maps, inst.of.agg);
            }
            if (!r->failed &&
                ((op == IR_SLOT || op == IR_MEMCOPY || op == IR_BITLOAD ||
                  op == IR_BITSTORE ||
                  (op >= IR_VBINARY && op <= IR_VREDUCE)) ==
                     (inst.of.type == IR_VOID) ||
                 ((op == IR_BITLOAD || op == IR_BITSTORE) &&
                  (inst.of.type != IR_AGG ||
                   inst.field >= program->aggs[inst.of.agg]->field_count ||
                   program->aggs[inst.of.agg]->fields[inst.field].bits == 0)))) {
                antl_damaged(r);
            }
            inst.arg_count = antl_get_count(r, 10);
            args = antl_allocate(r, inst.arg_count, sizeof *args);
            for (k = 0; k < inst.arg_count && !r->failed; k++) {
                args[k] = read_operand(r, f, maps);
            }
            inst.args = args;
            if (!r->failed) {
                ir_inst_add(f->blocks[i], &inst);
            }
        }
    }
}

/* A function signature, mapped to a function of the program. A C function
   and a declaration share an existing entry of the same name. */
/* Whether name is that of a copy of a generic, or of a function or a
   datum of one. Only a copy carries `<`. */
static bool copy_name(const char *name)
{
    return strchr(name, '<') != NULL;
}

static uint32_t read_signature(struct reader *r, struct ir_module *program,
                               struct ir_maps *maps, bool *has_body,
                               bool *skip)
{
    uint8_t flags = antl_get_u8(r);
    const char *module = get_cstr(r);
    const char *name = get_cstr(r);
    uint8_t result = antl_get_u8(r);
    uint32_t result_agg = read_agg_ref(r, program, maps, result);
    uint32_t file = antl_get_u32(r);
    uint32_t decl_line = antl_get_u32(r);
    uint32_t param_count = antl_get_count(r, 6);
    struct ir_function *f = NULL;
    size_t i;

    *has_body = false;
    *skip = false;
    if (r->failed) {
        return 0;
    }
    if (!valid_type(result) || flags > 15 ||
        ((flags & 1) == 0 && module[0] == '\0') ||
        (module[0] != '\0' && (flags & 2) != 0) ||
        (module[0] == '\0' && (flags & 4) != 0)) {
        antl_damaged(r);
        return 0;
    }
    if (module[0] == '\0') {
        module = NULL;
    }
    for (i = 0; i < program->function_count; i++) {
        struct ir_function *g = program->functions[i];
        bool same_module = module == NULL
                               ? g->module == NULL
                               : g->module != NULL &&
                                     strcmp(g->module, module) == 0;
        if (same_module && strcmp(g->name, name) == 0) {
            f = g;
        }
    }
    /* DESIGN: each module that uses a copy of a generic defines it, so
       two library files may both define one. The program keeps the
       first, and the second stands for it. The copies are made from one
       tree with the same arguments, so they are the same code. */
    if (f != NULL && (flags & 1) == 0 && module != NULL && copy_name(name)) {
        if (f->is_extern) {
            f->is_extern = false;
            *has_body = true;
        } else {
            *skip = true;
        }
    } else if (f != NULL && (flags & 1) == 0) {
        antl_fail(r, "defines `%s.%s`, which another library defines",
                  module, name);
        return 0;
    }
    if (file != IR_NO_INDEX && file >= maps->file_count) {
        antl_damaged(r);
        return 0;
    }
    if (f == NULL) {
        if ((flags & 1) == 0) {
            f = ir_function_add(program, module, name, (enum ir_type)result,
                                result_agg);
            *has_body = true;
            /* A copy of a generic of another module belongs to the object
               of the module whose file defines it. */
            if (module != NULL && strcmp(module, r->iface->module) != 0) {
                f->unit = r->iface->module;
            }
        } else if (module == NULL) {
            f = ir_extern_add(program, name, (enum ir_type)result, flags & 2);
            f->result_agg = result_agg;
        } else {
            f = ir_declare_add(program, module, name, (enum ir_type)result,
                               result_agg);
        }
        f->exported = (flags & 4) != 0;
        f->worker = (flags & 8) != 0;
        f->file = file == IR_NO_INDEX ? IR_NO_INDEX : maps->files[file];
        f->decl_line = decl_line;
        for (i = 0; i < param_count && !r->failed; i++) {
            uint8_t type = antl_get_u8(r);
            uint8_t ext = antl_get_u8(r);
            uint32_t agg = read_agg_ref(r, program, maps, type);
            bool narrow = type == IR_I8 || type == IR_I16;
            if (!valid_type(type) || type == IR_VOID || ext > IR_EXT_ZERO ||
                (ext != IR_EXT_NONE) != narrow) {
                antl_damaged(r);
            } else {
                ir_param_add(f, (enum ir_type)type, agg);
                f->params[f->param_count - 1].ext = (enum ir_ext)ext;
            }
        }
    } else {
        for (i = 0; i < param_count && !r->failed; i++) {
            uint8_t type = antl_get_u8(r);
            antl_get_u8(r);
            read_agg_ref(r, program, maps, type);
        }
    }
    return f->index;
}

/* A global of the file as the program's index. none allows
   IR_NO_INDEX. */
static uint32_t map_global(struct reader *r, const struct ir_maps *maps,
                           uint32_t g, bool none)
{
    if (none && g == IR_NO_INDEX) {
        return g;
    }
    if (g >= maps->global_count) {
        antl_damaged(r);
        return 0;
    }
    return maps->globals[g];
}

/* Whether the program holds a record of the class of c before c. */
static bool has_class(const struct ir_module *program, const struct ir_class *c)
{
    size_t i;

    for (i = 0; i + 1 < program->class_count; i++) {
        if (strcmp(program->classes[i]->module, c->module) == 0 &&
            strcmp(program->classes[i]->name, c->name) == 0) {
            return true;
        }
    }
    return false;
}

/* The class records of the file. Each names globals, a function and an
   aggregate of the file, which move to the program's indices. */
static void read_classes(struct reader *r, struct ir_module *program,
                         struct ir_maps *maps)
{
    uint32_t count = antl_get_count(r, 37);
    uint32_t i;
    uint32_t j;

    for (i = 0; i < count && !r->failed; i++) {
        const char *module = get_cstr(r);
        const char *name = get_cstr(r);
        uint8_t flags = antl_get_u8(r);
        uint32_t descriptor = antl_get_u32(r);
        uint32_t base = antl_get_u32(r);
        uint32_t table = antl_get_u32(r);
        uint32_t init = antl_get_u32(r);
        uint32_t agg = antl_get_u32(r);
        uint32_t subtables;
        uint32_t mutables;
        uint32_t injects;
        uint32_t provides;
        struct ir_class *c;

        if (r->failed ||
            flags > (IR_CLASS_ABSTRACT | IR_CLASS_FINAL | IR_CLASS_SINGLETON |
                     IR_CLASS_ARGS | IR_CLASS_REQUIRED) ||
            module[0] == '\0' ||
            (init != IR_NO_INDEX && init >= maps->function_count)) {
            antl_damaged(r);
            return;
        }
        c = ir_class_add(program, module, name);
        c->flags = flags;
        c->descriptor = map_global(r, maps, descriptor, false);
        c->base = map_global(r, maps, base, false);
        c->table = map_global(r, maps, table, true);
        c->init = init == IR_NO_INDEX ? init : maps->functions[init];
        c->agg = map_agg(r, program, maps, agg);
        subtables = antl_get_count(r, 16);
        for (j = 0; j < subtables && !r->failed; j++) {
            uint32_t interface = antl_get_u32(r);
            uint32_t at = antl_get_u32(r);
            uint32_t sub_agg = antl_get_u32(r);
            uint32_t sub_field = antl_get_u32(r);
            uint32_t mapped = map_agg(r, program, maps, sub_agg);
            uint32_t mapped_interface;
            uint32_t mapped_at;
            if (r->failed || sub_field >= program->aggs[mapped]->field_count) {
                antl_damaged(r);
                return;
            }
            /* Each call may mark the file damaged, so each has a line
               of its own. */
            mapped_interface = map_global(r, maps, interface, false);
            mapped_at = map_global(r, maps, at, false);
            ir_class_subtable(c, mapped_interface, mapped_at, mapped,
                              sub_field);
        }
        mutables = antl_get_count(r, 4);
        for (j = 0; j < mutables && !r->failed; j++) {
            uint32_t field = antl_get_u32(r);
            if (r->failed || field >= program->aggs[c->agg]->field_count) {
                antl_damaged(r);
            }
            ir_class_mutable(c, field);
        }
        injects = antl_get_count(r, 13);
        for (j = 0; j < injects && !r->failed; j++) {
            const char *path = get_cstr(r);
            const char *named = get_cstr(r);
            uint32_t of = antl_get_u32(r);
            uint8_t last = antl_get_u8(r);
            if (r->failed || path[0] == '\0' || last > 1) {
                antl_damaged(r);
                return;
            }
            ir_class_inject(program, c, path, named,
                            map_global(r, maps, of, false), last != 0);
        }
        provides = antl_get_count(r, 8);
        for (j = 0; j < provides && !r->failed; j++) {
            const char *path = get_cstr(r);
            uint32_t of = antl_get_u32(r);
            if (r->failed || path[0] == '\0') {
                antl_damaged(r);
                return;
            }
            ir_class_provides(program, c, path, map_global(r, maps, of,
                                                           false));
        }
        /* The record of a copy of a generic that the program has
           already stays behind. */
        if (copy_name(name) && has_class(program, c)) {
            ir_class_free(c);
            program->class_count--;
        }
    }
}

/* The body of a copy the program has already: read to move past it,
   and dropped. */
static void skip_body(struct reader *r, struct ir_module *program,
                      const struct ir_function *f, struct ir_maps *maps)
{
    struct ir_function scratch;

    memset(&scratch, 0, sizeof scratch);
    scratch.params = f->params;
    scratch.param_count = f->param_count;
    scratch.temps = alloc_zeroed(f->param_count + 1, sizeof *scratch.temps);
    memcpy(scratch.temps, f->temps, f->param_count * sizeof *scratch.temps);
    scratch.temp_count = (uint32_t)f->param_count;
    scratch.temp_capacity = f->param_count + 1;
    read_body(r, program, &scratch, maps);
    ir_function_free_body(&scratch);
}

/* A datum of a copy of a generic that the program has already, as g,
   the global read last. The program keeps the first, and *index
   receives it. Returns whether g is such a twin, which the program then
   drops. */
static bool merge_copy_global(struct ir_module *program, struct ir_global *g,
                              uint32_t *index)
{
    size_t i;

    for (i = 0; i + 1 < program->global_count; i++) {
        struct ir_global *old = program->globals[i];
        if (old->module == NULL || strcmp(old->module, g->module) != 0 ||
            strcmp(old->name, g->name) != 0) {
            continue;
        }
        if (old->is_extern) {
            /* A declaration takes the definition. */
            old->bytes = g->bytes;
            old->size = g->size;
            old->align = g->align;
            old->value = g->value;
            old->exported = g->exported;
            old->mutable = g->mutable;
            old->is_extern = false;
            program->global_count--;
            *index = old->index;
            return false;
        }
        program->global_count--;
        *index = old->index;
        return true;
    }
    return false;
}

struct relocs {
    uint32_t count;
    uint64_t *offsets;
    uint32_t *targets;
    uint8_t *functions;             /* the target is a function */
};

static void read_ir(struct reader *r, struct ir_module *program)
{
    struct ir_maps maps;
    struct relocs *relocs;
    struct ir_function **bodies;
    bool *skips;
    bool *twins;
    uint32_t i;
    uint32_t j;

    memset(&maps, 0, sizeof maps);
    read_tables(r, program, &maps);
    maps.global_count = antl_get_count(r, 28);
    maps.globals = antl_allocate(r, maps.global_count, sizeof *maps.globals);
    relocs = antl_allocate(r, maps.global_count, sizeof *relocs);
    twins = antl_allocate(r, maps.global_count, sizeof *twins);
    for (i = 0; i < maps.global_count && !r->failed; i++) {
        const char *module = get_cstr(r);
        const char *name = get_cstr(r);
        uint64_t size = antl_get_u64(r);
        uint64_t align = antl_get_u64(r);
        struct ir_global *g;
        /* A global of the runtime has no module, and an empty name is
           how the file spells that. */
        if (module != NULL && module[0] == '\0') {
            module = NULL;
        }
        /* The back end writes the alignment as a power of two. */
        if ((align & (align - 1)) != 0) {
            antl_damaged(r);
        }
        if (!antl_take(r, size)) {
            break;
        }
        g = ir_global_add(program, module, name, r->data + r->pos, size, align);
        r->pos += size;
        maps.globals[i] = g->index;
        relocs[i].count = antl_get_count(r, 13);
        relocs[i].offsets = antl_allocate(r, relocs[i].count, sizeof(uint64_t));
        relocs[i].targets = antl_allocate(r, relocs[i].count, sizeof(uint32_t));
        relocs[i].functions =
            antl_allocate(r, relocs[i].count, sizeof(uint8_t));
        for (j = 0; j < relocs[i].count && !r->failed; j++) {
            relocs[i].offsets[j] = antl_get_u64(r);
            relocs[i].targets[j] = antl_get_u32(r);
            relocs[i].functions[j] = antl_get_u8(r) != 0 ? 1 : 0;
            if (relocs[i].offsets[j] > size ||
                size - relocs[i].offsets[j] < 8) {
                antl_damaged(r);
            }
        }
        {
            uint8_t marks = antl_get_u8(r);
            g->exported = (marks & 2) != 0;
            g->is_extern = (marks & 4) != 0;
            g->mutable = (marks & 8) != 0;
            if ((marks & 1) != 0 && !r->failed) {
                g->value = read_const(r, program, &maps, 0);
            }
        }
        if (!g->is_extern && module != NULL && !r->failed) {
            if (copy_name(name)) {
                twins[i] = merge_copy_global(program, g, &maps.globals[i]);
                g = program->globals[maps.globals[i]];
            }
            if (!twins[i] && strcmp(module, r->iface->module) != 0) {
                g->unit = r->iface->module;
            }
        }
    }
    /* A pointer may name a global that the file lists later. */
    for (i = 0; i < maps.global_count && !r->failed; i++) {
        struct ir_global *g = program->globals[maps.globals[i]];
        if (twins[i]) {
            continue;
        }
        if (g->value != NULL) {
            remap_const(r, g->value, &maps, false);
        }
        for (j = 0; j < relocs[i].count; j++) {
            if (relocs[i].functions[j]) {
                continue;
            }
            if (relocs[i].targets[j] >= maps.global_count) {
                antl_damaged(r);
                break;
            }
            ir_global_reloc(program, g, relocs[i].offsets[j],
                            maps.globals[relocs[i].targets[j]]);
        }
    }
    maps.function_count = antl_get_count(r, 15);
    maps.functions =
        antl_allocate(r, maps.function_count, sizeof *maps.functions);
    bodies = antl_allocate(r, maps.function_count, sizeof *bodies);
    skips = antl_allocate(r, maps.function_count, sizeof *skips);
    for (i = 0; i < maps.function_count && !r->failed; i++) {
        bool has_body;
        maps.functions[i] = read_signature(r, program, &maps, &has_body,
                                           &skips[i]);
        bodies[i] = has_body ? program->functions[maps.functions[i]] : NULL;
    }
    for (i = 0; i < maps.function_count && !r->failed; i++) {
        if (bodies[i] != NULL) {
            read_body(r, program, bodies[i], &maps);
        } else if (skips[i]) {
            skip_body(r, program, program->functions[maps.functions[i]],
                      &maps);
        }
    }
    /* A table entry names a function, and the file lists the functions
       after the globals, so those addresses wait until here. */
    for (i = 0; i < maps.global_count && !r->failed; i++) {
        struct ir_global *g = program->globals[maps.globals[i]];
        if (twins[i]) {
            continue;
        }
        if (g->value != NULL) {
            remap_const(r, g->value, &maps, true);
        }
        for (j = 0; j < relocs[i].count; j++) {
            if (!relocs[i].functions[j]) {
                continue;
            }
            if (relocs[i].targets[j] >= maps.function_count) {
                antl_damaged(r);
                break;
            }
            ir_global_reloc_fn(program, g, relocs[i].offsets[j],
                               maps.functions[relocs[i].targets[j]]);
        }
    }
    if (!r->failed) {
        read_classes(r, program, &maps);
    }
}

struct interface *antl_read(const uint8_t *data, size_t size,
                            const struct interface *const *libraries,
                            size_t library_count, struct types *types,
                            struct arena *arena, struct ir_module *program,
                            char *error, size_t error_size)
{
    struct reader r;
    size_t i;

    memset(&r, 0, sizeof r);
    r.data = data;
    r.size = size;
    r.error = error;
    r.error_size = error_size;
    r.arena = arena;
    r.types = types;
    r.libraries = libraries;
    r.library_count = library_count;
    r.iface = arena_alloc(arena, sizeof *r.iface);
    read_header(&r, r.iface);
    for (i = 0; i < r.iface->import_count && !r.failed; i++) {
        struct name imported;
        imported.text = r.iface->imports[i];
        imported.length = strlen(imported.text);
        if (antl_library(&r, &imported) == NULL) {
            antl_fail(&r, "needs module `%s`", imported.text);
        }
    }
    if (!r.failed) {
        read_types(&r);
    }
    if (!r.failed) {
        read_items(&r);
    }
    if (!r.failed) {
        antl_read_generics(&r);
    }
    if (!r.failed) {
        read_ir(&r, program);
    }
    if (!r.failed && r.pos != r.size) {
        antl_damaged(&r);
    }
    return r.failed ? NULL : r.iface;
}

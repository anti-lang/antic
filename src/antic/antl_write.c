/* The writer of the library file: the type table with the types every
   other part names, the constants and the defaults, the items, the IR,
   and antl_write and antl_write_header, which put them in order. */

#include "alloc.h"
#include "antl_io.h"

#include <stdlib.h>
#include <string.h>

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
    return types_has_fields(t) &&
           (t->generic != NULL ||
            (t->module.length == strlen(module) &&
             memcmp(t->module.text, module, t->module.length) == 0));
}

/* DESIGN: a class carries its public and protected functions, and its
   `construct` and `destruct` whatever their level. A class of another
   module that inherits it runs them, and a private function it cannot
   name. */
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
           (m->body != NULL && (antl_name_equals(&m->name, "construct") ||
                                antl_name_equals(&m->name, "destruct")));
}

/* The count of functions of the body of t that another module may see.
   A private function is never one of them, because no other module can
   name it. A protected one is, because a class below it may. */
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

static uint8_t struct_form(const struct type *t)
{
    return t->generic != NULL          ? ANTL_FORM_COPY
           : t->type_param_count > 0 ? ANTL_FORM_GENERIC
                                     : ANTL_FORM_PLAIN;
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
       `own fn`, which stands with both. Bit 7 is `-> never`. Each makes
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
                                 (unsigned)t->owned << 6 |
                                 (unsigned)t->never << 7));
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

    if (!types_has_fields(t) || !is_local_struct(w, t)) {
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
                                 (f->exported ? 4 : 0) | (f->worker ? 8 : 0) |
                                 (f->never_returns ? 16 : 0) |
                                 (f->writes_tables ? 32 : 0) |
                                 (f->allocates ? 64 : 0)));
        antl_put_u8(w, (uint8_t)((unsigned)f->effects |
                                 (unsigned)f->guarantees << 2));
        put_str(w, f->module);
        put_str(w, f->name);
        antl_put_u8(w, (uint8_t)f->result);
        antl_put_u32(w, f->result_agg);
        antl_put_u8(w, (uint8_t)f->result_ext);
        antl_put_u32(w, f->file);
        antl_put_u32(w, f->decl_line);
        antl_put_count(w, f->param_count);
        for (j = 0; j < f->param_count; j++) {
            antl_put_u8(w, (uint8_t)f->params[j].type);
            antl_put_u8(w, (uint8_t)f->params[j].ext);
            antl_put_u32(w, f->params[j].agg);
            antl_put_u8(w, (uint8_t)((f->params[j].nonnull ? 1 : 0) |
                                     (f->params[j].own ? 2 : 0)));
            antl_put_u32(w, f->params[j].deref_size);
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

    text_append_bytes(out, antl_magic, sizeof antl_magic);
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

#include "antl.h"
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

static void put_u8(struct writer *w, uint8_t v)
{
    text_append_bytes(w->out, &v, 1);
}

static void put_u32(struct writer *w, uint32_t v)
{
    uint8_t b[4];
    int i;

    for (i = 0; i < 4; i++) {
        b[i] = (uint8_t)(v >> (8 * i));
    }
    text_append_bytes(w->out, b, sizeof b);
}

/* A count or an index, which the file holds in 32 bits. A larger one
   marks the writer failed, and the file is refused. */
static void put_count(struct writer *w, size_t n)
{
    if (n > UINT32_MAX) {
        w->failed = true;
        n = 0;
    }
    put_u32(w, (uint32_t)n);
}

static void put_u64(struct writer *w, uint64_t v)
{
    uint8_t b[8];
    int i;

    for (i = 0; i < 8; i++) {
        b[i] = (uint8_t)(v >> (8 * i));
    }
    text_append_bytes(w->out, b, sizeof b);
}

static void put_bytes(struct writer *w, const char *s, size_t length)
{
    put_count(w, length);
    text_append_bytes(w->out, s, length);
}

static void put_str(struct writer *w, const char *s)
{
    put_bytes(w, s, s == NULL ? 0 : strlen(s));
}

/* Doc text, or nothing for --strip-docs. */
static void put_doc(struct writer *w, const char *text, size_t length)
{
    put_bytes(w, text, w->strip_docs || text == NULL ? 0 : length);
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
    if (w->type_count == w->type_capacity) {
        size_t capacity = w->type_capacity == 0 ? 16 : w->type_capacity * 2;
        const struct type **types = realloc((void *)w->types,
                                            capacity * sizeof *types);
        if (types == NULL) {
            fputs("antic: out of memory\n", stderr);
            exit(70);
        }
        w->types = types;
        w->type_capacity = capacity;
    }
    w->types[w->type_count++] = t;
}

/* Give t and every type inside it an index. A type comes after the types
   it is built from. A struct comes before its field types, so a struct
   can hold a pointer to itself. */
static void visit_type(struct writer *w, const struct type *t);

/* The types a symbolic value names: its own and those it measures. */
static void visit_symbolic(struct writer *w, const struct symbolic *s)
{
    if (s == NULL) {
        return;
    }
    visit_type(w, s->type);
    if (s->of != NULL) {
        visit_type(w, s->of);
    }
    visit_symbolic(w, s->a);
    visit_symbolic(w, s->b);
}

static void visit_value(struct writer *w, const struct const_value *v)
{
    size_t i;

    if (v->kind == CONST_SYMBOLIC) {
        visit_symbolic(w, v->as.symbolic);
    } else if (v->kind == CONST_ARRAY || v->kind == CONST_STRUCT) {
        for (i = 0; i < v->as.aggregate.count; i++) {
            visit_value(w, &v->as.aggregate.items[i]);
        }
    }
}

/* The types the constant defaults of a function's parameters name. */
static void visit_defaults(struct writer *w, const struct symbol *sym)
{
    size_t i;

    for (i = 0; sym->defaults != NULL && i < sym->default_count; i++) {
        if (sym->defaults[i].value != NULL) {
            visit_value(w, sym->defaults[i].value);
        }
    }
}

static void visit_type(struct writer *w, const struct type *t)
{
    size_t i;
    size_t index;

    if (find_type(w, t, &index)) {
        return;
    }
    switch (t->kind) {
    case TYPE_ARRAY:
        visit_symbolic(w, t->length_of);
        visit_type(w, t->element);
        break;
    case TYPE_POINTER:
    case TYPE_SLICE:
    case TYPE_OPTIONAL:
        visit_type(w, t->element);
        break;
    case TYPE_FN:
        for (i = 0; i < t->param_count; i++) {
            visit_type(w, t->params[i]);
        }
        visit_type(w, t->result);
        break;
    /* A tuple is its elements in order, and nothing else, because two
       tuples of the same elements are one type. */
    case TYPE_TUPLE:
        for (i = 0; i < t->param_count; i++) {
            visit_type(w, t->params[i]);
        }
        break;
    case TYPE_STRUCT:
    case TYPE_CLASS:
    case TYPE_VARIANT:
        /* A channel names its element, which comes first. So do the
           parameters of a generic, and the generic and the arguments of
           a copy. */
        if (types_is_chan(t)) {
            visit_type(w, t->element);
        }
        for (i = 0; i < t->type_param_count; i++) {
            visit_type(w, t->type_params[i]);
        }
        if (t->generic != NULL) {
            visit_type(w, t->generic);
            for (i = 0; i < t->generic->type_param_count; i++) {
                if (t->values[i] != NULL) {
                    visit_symbolic(w, t->values[i]);
                } else {
                    visit_type(w, t->args[i]);
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
                visit_type(w, t->fields[i].type);
                if (t->fields[i].constant != NULL) {
                    visit_value(w, t->fields[i].constant);
                }
            }
            for (i = 0; t->generic == NULL && i < t->member_count; i++) {
                const struct item *m = t->members[i];
                if (carried_member(t, m)) {
                    visit_type(w, m->symbol->type);
                    visit_defaults(w, m->symbol);
                }
            }
        }
        return;
    /* The value a hook of a parameter gives follows the parameter, and a
       parameter follows the interfaces of its constraints. */
    case TYPE_PARAM:
        if (t->hook_owner != NULL) {
            visit_type(w, t->hook_owner);
        }
        for (i = 0; i < t->iface_count; i++) {
            visit_type(w, t->ifaces[i]);
        }
        if (find_type(w, t, &index)) {
            return;
        }
        add_type(w, t);
        if (t->walked != NULL) {
            visit_type(w, t->walked);
        }
        if (t->indexed != NULL) {
            visit_type(w, t->indexed);
        }
        return;
    /* The underlying integer comes first, because an enum names it by
       index and a reference reaches back only. */
    case TYPE_ENUM:
        visit_type(w, t->base);
        add_type(w, t);
        return;
    default:
        break;
    }
    add_type(w, t);
}

static void put_type_ref(struct writer *w, const struct type *t)
{
    size_t index;

    /* visit_type puts every type the file names in the table first. */
    if (!find_type(w, t, &index)) {
        w->failed = true;
        index = 0;
    }
    put_count(w, index);
}

static void put_symbolic(struct writer *w, const struct symbolic *s)
{
    put_u8(w, (uint8_t)s->kind);
    put_type_ref(w, s->type);
    switch (s->kind) {
    case SYMBOLIC_INT:
        put_u64(w, s->value);
        break;
    case SYMBOLIC_SIZE_OF:
        put_type_ref(w, s->of);
        break;
    case SYMBOLIC_UNARY:
    case SYMBOLIC_CAST:
        put_u8(w, (uint8_t)s->op);
        put_symbolic(w, s->a);
        break;
    case SYMBOLIC_BINARY:
        put_u8(w, (uint8_t)s->op);
        put_symbolic(w, s->a);
        put_symbolic(w, s->b);
        break;
    /* A constant parameter of a generic, `N`, names its parameter. */
    case SYMBOLIC_PARAM:
        put_type_ref(w, s->of);
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

    put_count(w, count);
    for (i = 0; i < count; i++) {
        put_bytes(w, refs[i].module.text, refs[i].module.length);
        put_bytes(w, refs[i].name.text, refs[i].name.length);
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

    put_bytes(w, t->name.text, t->name.length);
    if (t->hook_owner != NULL) {
        put_u8(w, t->hook_owner->walked == t ? 1 : 2);
        put_type_ref(w, t->hook_owner);
    } else if (t->param != NULL) {
        put_u8(w, 0);
        put_u8(w, t->param->constant);
        put_constraint_refs(w, t->param->constraints,
                            t->param->constraint_count);
    } else {
        put_u8(w, 3);
        put_constraint_refs(w,
                            t->declared_by != NULL
                                ? t->declared_by->constraints
                                : NULL,
                            t->declared_by != NULL
                                ? t->declared_by->constraint_count
                                : 0);
    }
    put_u32(w, t->hooks);
    put_count(w, t->iface_count);
    for (i = 0; i < t->iface_count; i++) {
        put_type_ref(w, t->ifaces[i]);
    }
}

static void put_type(struct writer *w, const struct type *t)
{
    size_t i;
    size_t j;

    put_u8(w, (uint8_t)t->kind);
    switch (t->kind) {
    case TYPE_POINTER:
        put_type_ref(w, t->element);
        /* `*T`, `?*T` and their `lent` forms are four types, and a module
           that imports this one reads which of them a signature names.
           Bit 0 is `?` and bit 1 `lent`. */
        put_u8(w, (uint8_t)((unsigned)t->nullable | (unsigned)t->lent << 1));
        break;
    /* `?T` is its value type alone, since one element makes one. */
    case TYPE_SLICE:
    case TYPE_OPTIONAL:
        put_type_ref(w, t->element);
        break;
    case TYPE_ARRAY:
        put_type_ref(w, t->element);
        put_u8(w, t->length_of != NULL);
        if (t->length_of != NULL) {
            put_symbolic(w, t->length_of);
        } else {
            put_u64(w, t->length);
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
        put_count(w, t->param_count);
        for (i = 0; i < t->param_count; i++) {
            put_type_ref(w, t->params[i]);
        }
        put_type_ref(w, t->result);
        put_u8(w, (uint8_t)((unsigned)t->nullable | (unsigned)t->bound << 1 |
                            (unsigned)t->may_fail << 2 |
                            (unsigned)t->has_out << 3 |
                            (unsigned)t->context << 4 |
                            (unsigned)t->concurrent << 5 |
                            (unsigned)t->owned << 6));
        break;
    case TYPE_TUPLE:
        put_count(w, t->param_count);
        for (i = 0; i < t->param_count; i++) {
            put_type_ref(w, t->params[i]);
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
        put_bytes(w, t->module.text, t->module.length);
        put_bytes(w, t->name.text, t->name.length);
        /* DESIGN: a form byte follows the name. A generic carries its
           parameters after its body. A copy names its generic and its
           arguments, a type or a constant each, before its body. It
           carries that body in full wherever it stands. A reader that
           has the copy already takes that one. The copies of one generic
           with the same arguments are then one type in the program. */
        put_u8(w, struct_form(t));
        if (t->generic != NULL) {
            put_type_ref(w, t->generic);
            for (i = 0; i < t->generic->type_param_count; i++) {
                put_u8(w, t->values[i] != NULL);
                if (t->values[i] != NULL) {
                    put_symbolic(w, t->values[i]);
                } else {
                    put_type_ref(w, t->args[i]);
                }
            }
        }
        /* `chan T` is one struct per element type, so the element
           follows its name. */
        if (types_is_chan(t)) {
            put_type_ref(w, t->element);
        }
        if (is_local_struct(w, t)) {
            put_u8(w, (uint8_t)((unsigned)t->is_union |
                                (unsigned)t->packed << 1 |
                                (unsigned)t->has_abstract << 2 |
                                (unsigned)t->is_final << 3 |
                                (unsigned)t->simd << 4 |
                                (unsigned)t->traced << 5));
            /* A thread-safe class, and `unchecked(unguarded-field)` in
               its header. */
            put_u8(w, (uint8_t)((unsigned)t->safety |
                                (unsigned)t->unchecked_fields << 2));
            /* The `compatible` line of an abstract class, empty where
               the body has none. Every module that names the class
               writes the same descriptor, so the floor travels with
               it. */
            put_bytes(w, t->compatible.text, t->compatible.length);
            put_u64(w, t->align);
            put_count(w, t->field_count);
            for (i = 0; i < t->field_count; i++) {
                put_bytes(w, t->fields[i].name.text, t->fields[i].name.length);
                put_type_ref(w, t->fields[i].type);
                put_u8(w, t->fields[i].bits);
                put_u8(w, (uint8_t)((unsigned)t->fields[i].form |
                                    (unsigned)t->fields[i].owned << 4 |
                                    (unsigned)t->fields[i].atomic << 5 |
                                    (unsigned)t->fields[i].writable << 6 |
                                    (unsigned)t->fields[i].transient << 7));
                put_u8(w, (uint8_t)t->fields[i].vis);
                /* DESIGN: `inject` and `inject final` travel with the
                   field. A module that builds a class of another
                   module then calls the same provider through the same
                   slot, and `anti build` reports what a dependency
                   needs. */
                put_u8(w, (uint8_t)((unsigned)t->fields[i].injected |
                                    (unsigned)t->fields[i].inject_final << 1 |
                                    (unsigned)t->fields[i].hidden << 2 |
                                    (unsigned)t->fields[i].unchecked << 3));
                /* DESIGN: the lock that guards the field travels by its
                   name. A lock of an enclosing class guards a field of
                   a nested type alone, which no other module reaches.
                   So the class it names stays behind. */
                put_bytes(w, t->fields[i].guard.text,
                          t->fields[i].guard.length);
                /* DESIGN: /// on a private item is never stored, and
                   the fields of a private struct are private items. */
                put_doc(w, t->fields[i].doc.text,
                        is_public_struct(w, t) ? t->fields[i].doc.length : 0);
            }
            /* The public functions of the body, so a call on a value of
               another module resolves and reaches the right symbol. */
            put_count(w, public_members(t));
            for (i = 0; t->generic == NULL && i < t->member_count; i++) {
                const struct item *m = t->members[i];
                if (!carried_member(t, m)) {
                    continue;
                }
                put_bytes(w, m->name.text, m->name.length);
                /* The qualifier decides the table a body fills and
                   the symbol it has, so an importing module builds
                   the same tables. */
                put_bytes(w, m->qualifier.text, m->qualifier.length);
                put_type_ref(w, m->symbol->type);
                put_u8(w, (uint8_t)((unsigned)m->contract |
                                    (unsigned)m->is_final << 4 |
                                    (unsigned)m->is_operator << 5 |
                                    (unsigned)m->may_fail << 6));
                put_u8(w, (uint8_t)m->vis);
                put_doc(w, m->doc.text, m->doc.length);
                /* DESIGN: the public interface keeps the parameter
                   names of every function. A function of a class body is
                   one, and `anti doc` and the generated header print
                   them. The count is the one the declaration wrote.
                   Neither `self` nor the out pointer of `may fail`
                   stands among them, so the reader needs no type to take
                   them. */
                put_count(w, m->param_count);
                for (j = 0; j < m->param_count; j++) {
                    const struct name *n = m->symbol->params != NULL
                                               ? &m->symbol->params[j]
                                               : &m->params[j].name;
                    put_bytes(w, n->text, n->length);
                }
            }
            if (t->type_param_count > 0) {
                put_count(w, t->type_param_count);
                for (i = 0; i < t->type_param_count; i++) {
                    put_type_ref(w, t->type_params[i]);
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
        put_bytes(w, t->module.text, t->module.length);
        put_bytes(w, t->name.text, t->name.length);
        put_type_ref(w, t->base);
        put_count(w, t->field_count);
        for (i = 0; i < t->field_count; i++) {
            put_bytes(w, t->fields[i].name.text, t->fields[i].name.length);
            put_u64(w, t->fields[i].number);
            put_doc(w, t->fields[i].doc.text, t->fields[i].doc.length);
        }
        break;
    default:
        break;
    }
}

static void put_value(struct writer *w, const struct const_value *v)
{
    size_t i;

    put_u8(w, (uint8_t)v->kind);
    switch (v->kind) {
    case CONST_INT:
        put_u64(w, v->as.integer);
        break;
    case CONST_FLOAT:
        put_u64(w, float_bits(v->as.floating));
        break;
    case CONST_BOOL:
        put_u8(w, v->as.boolean);
        break;
    case CONST_CHAR:
        put_u32(w, v->as.character);
        break;
    case CONST_NULL:
        break;
    case CONST_TEXT:
        put_bytes(w, v->as.text.bytes, v->as.text.length);
        break;
    case CONST_ARRAY:
    case CONST_STRUCT:
        put_count(w, v->as.aggregate.count);
        for (i = 0; i < v->as.aggregate.count; i++) {
            put_value(w, &v->as.aggregate.items[i]);
        }
        break;
    case CONST_SYMBOLIC:
        put_symbolic(w, v->as.symbolic);
        break;
    }
}

/* DESIGN: the defaults of a function's parameters follow its type. A
   count gives the parameters, `self` included, and is 0 when none has a
   default. Each parameter then has a byte: 0 without a default, 1 before
   a constant and 2 for `here`, which the call fills with its position. */
static void put_param_defaults(struct writer *w, const struct symbol *sym)
{
    size_t i;

    put_count(w, sym->defaults != NULL ? sym->default_count : 0);
    for (i = 0; sym->defaults != NULL && i < sym->default_count; i++) {
        const struct param_default *d = &sym->defaults[i];
        put_u8(w, d->here ? 2 : d->value != NULL ? 1 : 0);
        if (!d->here && d->value != NULL) {
            put_value(w, d->value);
        }
    }
}

/* The `own` parameters follow the defaults: a count, `self` included and
   0 when none is `own`, then a byte of 0 or 1 per parameter. */
static void put_param_owned(struct writer *w, const struct symbol *sym)
{
    size_t i;

    put_count(w, sym->owned != NULL ? sym->owned_count : 0);
    for (i = 0; sym->owned != NULL && i < sym->owned_count; i++) {
        put_u8(w, sym->owned[i] ? 1 : 0);
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
        put_u8(w, t->fields[i].constant != NULL);
        if (t->fields[i].constant != NULL) {
            put_value(w, t->fields[i].constant);
        }
    }
    /* The functions of the body, in the order the type carries them, so
       the reader has their types in place. */
    for (i = 0; t->generic == NULL && i < t->member_count; i++) {
        if (carried_member(t, t->members[i])) {
            put_param_defaults(w, t->members[i]->symbol);
            put_param_owned(w, t->members[i]->symbol);
        }
    }
}

static void put_vtype(struct writer *w, struct ir_vtype v)
{
    put_u8(w, (uint8_t)v.type);
    put_u32(w, v.agg);
}

/* An aggregate constant, as the tree the back end lays out. Its type and
   its symbolic values are written as indices, which the reader maps. */
static void put_const(struct writer *w, const struct ir_const *c)
{
    size_t i;

    put_u8(w, (uint8_t)c->kind);
    put_u8(w, (uint8_t)c->scalar);
    switch (c->kind) {
    case IR_CONST_INT:
        put_u64(w, c->integer);
        break;
    case IR_CONST_FLOAT:
        put_u64(w, float_bits(c->floating));
        break;
    case IR_CONST_SYM:
        put_u64(w, c->sym);
        break;
    case IR_CONST_ADDR:
    case IR_CONST_FUNC:
        put_u64(w, c->global);
        break;
    case IR_CONST_AGG:
        put_u64(w, c->item_count);
        put_vtype(w, c->type);
        for (i = 0; i < c->item_count; i++) {
            put_const(w, &c->items[i]);
        }
        break;
    default: /* IR_CONST_NONE */
        put_u64(w, 0);
        break;
    }
}

static void put_operand(struct writer *w, const struct ir_operand *o)
{
    put_u8(w, (uint8_t)o->kind);
    put_u8(w, (uint8_t)o->type);
    switch (o->kind) {
    case IR_TEMP:
        put_u64(w, o->as.temp);
        break;
    case IR_INT:
        put_u64(w, o->as.integer);
        break;
    case IR_FLOAT:
        put_u64(w, float_bits(o->as.floating));
        break;
    case IR_GLOBAL:
    case IR_FUNC:
    case IR_BLOCK:
    case IR_SYM:
        put_u64(w, o->as.index);
        break;
    default:
        put_u64(w, 0);
        break;
    }
}

static void put_inst(struct writer *w, const struct ir_inst *inst)
{
    size_t i;

    put_u8(w, (uint8_t)inst->op);
    put_u8(w, (uint8_t)inst->type);
    put_u32(w, inst->line);
    put_u32(w, inst->result);
    put_operand(w, &inst->a);
    put_operand(w, &inst->b);
    put_operand(w, &inst->c);
    put_vtype(w, inst->of);
    put_u32(w, inst->field);
    put_count(w, inst->arg_count);
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
    put_count(w, ir->file_count);
    for (i = 0; i < ir->file_count; i++) {
        put_str(w, ir->files[i]);
    }
    put_count(w, ir->sym_count);
    for (i = 0; i < ir->sym_count; i++) {
        const struct ir_sym *sym = &ir->syms[i];
        put_u8(w, (uint8_t)sym->kind);
        put_u8(w, (uint8_t)sym->type);
        put_u64(w, sym->value);
        put_vtype(w, sym->of);
        put_u32(w, sym->field);
        put_u8(w, (uint8_t)sym->op);
        put_u32(w, sym->a);
        put_u32(w, sym->b);
    }
    put_count(w, ir->agg_count);
    for (i = 0; i < ir->agg_count; i++) {
        const struct ir_aggtype *t = ir->aggs[i];
        put_u8(w, (uint8_t)t->kind);
        put_str(w, t->name);
        put_u8(w, (uint8_t)((unsigned)t->packed | (unsigned)t->simd << 1));
        put_u64(w, t->align);
        put_u32(w, t->length);
        put_str(w, t->length_text);
        put_count(w, t->field_count);
        for (j = 0; j < t->field_count; j++) {
            put_str(w, t->fields[j].name);
            put_vtype(w, t->fields[j].type);
            put_u8(w, t->fields[j].bits);
            put_u8(w, (uint8_t)t->fields[j].ext);
        }
    }
    put_count(w, ir->global_count);
    for (i = 0; i < ir->global_count; i++) {
        const struct ir_global *g = ir->globals[i];
        put_str(w, g->module);
        put_str(w, g->name);
        put_u64(w, g->size);
        put_u64(w, g->align);
        if (g->size > 0) {
            text_append_bytes(w->out, g->bytes, g->size);
        }
        put_count(w, g->reloc_count);
        for (j = 0; j < g->reloc_count; j++) {
            put_u64(w, g->relocs[j].offset);
            put_u32(w, g->relocs[j].global);
            put_u8(w, g->relocs[j].fn ? 1 : 0);
        }
        put_u8(w, (uint8_t)((g->value != NULL ? 1 : 0) |
                            (g->exported ? 2 : 0) |
                            (g->is_extern ? 4 : 0) |
                            (g->mutable ? 8 : 0)));
        if (g->value != NULL) {
            put_const(w, g->value);
        }
    }
    /* All signatures come before the first body, so a body can call a
       function that the file lists later. */
    put_count(w, ir->function_count);
    for (i = 0; i < ir->function_count; i++) {
        const struct ir_function *f = ir->functions[i];
        put_u8(w, (uint8_t)((f->is_extern ? 1 : 0) | (f->variadic ? 2 : 0) |
                            (f->exported ? 4 : 0) | (f->worker ? 8 : 0)));
        put_str(w, f->module);
        put_str(w, f->name);
        put_u8(w, (uint8_t)f->result);
        put_u32(w, f->result_agg);
        put_u32(w, f->file);
        put_u32(w, f->decl_line);
        put_count(w, f->param_count);
        for (j = 0; j < f->param_count; j++) {
            put_u8(w, (uint8_t)f->params[j].type);
            put_u8(w, (uint8_t)f->params[j].ext);
            put_u32(w, f->params[j].agg);
        }
    }
    for (i = 0; i < ir->function_count; i++) {
        const struct ir_function *f = ir->functions[i];
        if (f->is_extern) {
            continue;
        }
        put_u32(w, f->temp_count);
        for (j = 0; j < f->temp_count; j++) {
            put_u8(w, (uint8_t)f->temps[j]);
        }
        put_count(w, f->block_count);
        for (j = 0; j < f->block_count; j++) {
            /* The failure block of an assertion carries its flag, so the
               build that compiles the program can still drop it. */
            put_u8(w, (uint8_t)f->blocks[j]->fail);
            put_count(w, f->blocks[j]->count);
            for (k = 0; k < f->blocks[j]->count; k++) {
                put_inst(w, &f->blocks[j]->insts[k]);
            }
        }
    }
    put_count(w, ir->class_count);
    for (i = 0; i < ir->class_count; i++) {
        const struct ir_class *c = ir->classes[i];
        put_str(w, c->module);
        put_str(w, c->name);
        put_u8(w, (uint8_t)c->flags);
        put_u32(w, c->descriptor);
        put_u32(w, c->base);
        put_u32(w, c->table);
        put_u32(w, c->init);
        put_u32(w, c->agg);
        put_count(w, c->subtable_count);
        for (j = 0; j < c->subtable_count; j++) {
            put_u32(w, c->subtables[j].interface);
            put_u32(w, c->subtables[j].table);
            put_u32(w, c->subtables[j].agg);
            put_u32(w, c->subtables[j].field);
        }
        put_count(w, c->mutable_count);
        for (j = 0; j < c->mutable_count; j++) {
            put_u32(w, c->mutable_fields[j]);
        }
        /* The `inject` fields, so the pass over the whole program finds
           every interface of the program and the class that needs a
           provider for it. */
        put_count(w, c->inject_count);
        for (j = 0; j < c->inject_count; j++) {
            put_str(w, c->injects[j].interface);
            put_str(w, c->injects[j].field);
            put_u32(w, c->injects[j].descriptor);
            put_u8(w, (uint8_t)(c->injects[j].final ? 1 : 0));
        }
        /* The `provides` lines, so a library file carries what its
           module offers to a host that loads it. */
        put_count(w, c->provides_count);
        for (j = 0; j < c->provides_count; j++) {
            put_str(w, c->provides[j].interface);
            put_u32(w, c->provides[j].descriptor);
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
    put_u32(w, ANTL_VERSION);
    put_str(w, p->name != NULL ? p->name : iface->module);
    put_str(w, p->version != NULL ? p->version : PACKAGE_VERSION_DEFAULT);
    put_count(w, p->dependency_count);
    for (i = 0; i < p->dependency_count; i++) {
        put_str(w, p->dependencies[i].name);
        put_str(w, p->dependencies[i].constraint);
        put_str(w, p->dependencies[i].url);
    }
    put_str(w, p->license);
    put_str(w, p->license_text);
    put_count(w, p->attribution_count);
    for (i = 0; i < p->attribution_count; i++) {
        put_str(w, p->attribution[i]);
    }
    put_str(w, iface->module);
    put_count(w, iface->import_count);
    for (i = 0; i < iface->import_count; i++) {
        put_str(w, iface->imports[i]);
    }
    put_count(w, iface->framework_count);
    for (i = 0; i < iface->framework_count; i++) {
        put_str(w, iface->frameworks[i]);
    }
    put_count(w, iface->linux_library_count);
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
        visit_type(&w, iface->items[i]->type);
        if (iface->items[i]->kind == SYMBOL_CONST) {
            visit_value(&w, iface->items[i]->value);
        }
        visit_defaults(&w, iface->items[i]);
    }
    antl_visit_generics(&w);
    put_header(&w, iface);
    put_count(&w, w.type_count);
    for (i = 0; i < w.type_count; i++) {
        put_type(&w, w.types[i]);
    }
    for (i = 0; i < w.type_count; i++) {
        put_defaults(&w, w.types[i]);
    }
    put_count(&w, iface->item_count);
    for (i = 0; i < iface->item_count; i++) {
        const struct symbol *sym = iface->items[i];
        put_u8(&w, (uint8_t)sym->kind);
        put_bytes(&w, sym->name.text, sym->name.length);
        put_type_ref(&w, sym->type);
        /* DESIGN: the `may fail` flag is recorded, so a reader of the
           file sees the form the declaration wrote. The type alone gives
           the `?*Error` of the ABI and never the form. `worker` is
           recorded for the same reason, and `anti doc` prints it. */
        /* Bit 4 marks the name a `type` declares, and bit 5 a generic
           function, whose declaration the section of the generics
           holds. */
        put_u8(&w, (uint8_t)((unsigned)sym->exported |
                             (unsigned)sym->internal << 1 |
                             (unsigned)sym->may_fail << 2 |
                             (unsigned)sym->worker << 3 |
                             (unsigned)sym->alias << 4 |
                             (unsigned)(sym->kind == SYMBOL_FN &&
                                        sym->item != NULL &&
                                        sym->item->type_param_count > 0)
                                 << 5));
        put_doc(&w, sym->doc.text, sym->doc.length);
        if (sym->kind == SYMBOL_FN || sym->kind == SYMBOL_EXTERN_FN) {
            size_t j;
            for (j = 0; j < sym->type->param_count; j++) {
                put_bytes(&w, sym->params[j].text, sym->params[j].length);
            }
            put_param_defaults(&w, sym);
            put_param_owned(&w, sym);
        }
        if (sym->kind == SYMBOL_EXTERN_FN) {
            put_u8(&w, sym->variadic);
        } else if (sym->kind == SYMBOL_CONST) {
            put_value(&w, sym->value);
        }
    }
    antl_put_generics(&w);
    put_ir(&w, ir);
    free((void *)w.types);
    free((void *)w.externs);
    return !w.failed;
}

/* Reading */

static void fail(struct reader *r, const char *format, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;

static void fail(struct reader *r, const char *format, ...)
{
    va_list args;
    int n;

    if (r->failed) {
        return;
    }
    va_start(args, format);
    n = vsnprintf(r->error, r->error_size, format, args);
    va_end(args);
    /* A message cut to fit ends in three dots. */
    if (n >= 0 && (size_t)n >= r->error_size && r->error_size >= 4) {
        memcpy(r->error + r->error_size - 4, "...", 4);
    }
    r->failed = true;
}

static void damaged(struct reader *r)
{
    fail(r, "is damaged at byte %zu", r->pos);
}

static bool take(struct reader *r, size_t n)
{
    if (r->failed || n > r->size - r->pos) {
        if (!r->failed) {
            r->pos = r->size;
            damaged(r);
        }
        return false;
    }
    return true;
}

static uint64_t get_uint(struct reader *r, int bytes)
{
    uint64_t v = 0;
    int i;

    if (!take(r, (size_t)bytes)) {
        return 0;
    }
    for (i = 0; i < bytes; i++) {
        v |= (uint64_t)r->data[r->pos + (size_t)i] << (8 * i);
    }
    r->pos += (size_t)bytes;
    return v;
}

static uint8_t get_u8(struct reader *r)
{
    return (uint8_t)get_uint(r, 1);
}

static uint32_t get_u32(struct reader *r)
{
    return (uint32_t)get_uint(r, 4);
}

static uint64_t get_u64(struct reader *r)
{
    return get_uint(r, 8);
}

/* A count of records that each take at least min bytes. A larger count
   cannot fit in the rest of the file, which keeps a damaged count from
   causing a huge allocation. */
static uint32_t get_count(struct reader *r, size_t min)
{
    uint32_t n = get_u32(r);

    if (!r->failed && n > (r->size - r->pos) / min) {
        damaged(r);
        return 0;
    }
    return n;
}

static void *allocate(struct reader *r, size_t count, size_t size)
{
    return types_alloc_array(r->arena, count + 1, size);
}

/* A string as a name that points into the memory pool. */
static struct name get_name(struct reader *r)
{
    struct name n = {"", 0};
    uint32_t length = get_count(r, 1);
    char *text;

    if (r->failed || !take(r, length)) {
        return n;
    }
    text = allocate(r, length, 1);
    memcpy(text, r->data + r->pos, length);
    r->pos += length;
    n.text = text;
    n.length = length;
    return n;
}

/* A string that the reader uses as a C string, such as a module path. A
   NUL inside it would cut it short, so the file is damaged then. */
static const char *get_cstr(struct reader *r)
{
    struct name n = get_name(r);

    if (n.length > 0 && memchr(n.text, '\0', n.length) != NULL) {
        damaged(r);
    }
    return n.text;
}

static bool name_equals(const struct name *n, const char *s)
{
    return n->length == strlen(s) && memcmp(n->text, s, n->length) == 0;
}

static const struct interface *library(const struct reader *r,
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
        fail(r, "is not a library file");
        return false;
    }
    r->pos = sizeof magic;
    version = get_u32(r);
    if (!r->failed && version != ANTL_VERSION) {
        fail(r, "has format version %u, and antic reads version %u",
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
    p->dependency_count = get_count(r, 12);
    deps = allocate(r, p->dependency_count, sizeof *deps);
    for (i = 0; i < p->dependency_count && !r->failed; i++) {
        deps[i].name = get_cstr(r);
        deps[i].constraint = get_cstr(r);
        deps[i].url = get_cstr(r);
    }
    p->dependencies = deps;
    p->license = get_cstr(r);
    p->license_text = get_cstr(r);
    p->attribution_count = get_count(r, 4);
    lines = allocate(r, p->attribution_count, sizeof *lines);
    for (i = 0; i < p->attribution_count && !r->failed; i++) {
        lines[i] = get_cstr(r);
    }
    p->attribution = lines;
    out->module = get_cstr(r);
    out->import_count = get_count(r, 4);
    out->imports = allocate(r, out->import_count, sizeof *out->imports);
    for (i = 0; i < out->import_count && !r->failed; i++) {
        out->imports[i] = get_cstr(r);
    }
    out->framework_count = get_count(r, 4);
    out->frameworks = allocate(r, out->framework_count,
                               sizeof *out->frameworks);
    for (i = 0; i < out->framework_count && !r->failed; i++) {
        out->frameworks[i] = get_cstr(r);
    }
    out->linux_library_count = get_count(r, 4);
    out->linux_libraries = allocate(r, out->linux_library_count,
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

static struct type *type_ref(struct reader *r, uint32_t limit)
{
    uint32_t index = get_u32(r);

    if (r->failed || index >= limit) {
        damaged(r);
        return NULL;
    }
    return r->table[index];
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
    lib = library(r, module);
    if (lib == NULL) {
        fail(r, "needs module `%.*s`", (int)module->length, module->text);
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
    fail(r, "needs struct `%.*s.%.*s`", (int)module->length, module->text,
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
static const struct symbolic *read_symbolic(struct reader *r, uint32_t limit,
                                            int depth)
{
    struct symbolic key;
    uint8_t kind = get_u8(r);

    memset(&key, 0, sizeof key);
    key.kind = (enum symbolic_kind)kind;
    key.type = type_ref(r, limit);
    if (r->failed || kind > SYMBOLIC_PARAM || depth > 64) {
        damaged(r);
        return NULL;
    }
    switch (key.kind) {
    case SYMBOLIC_INT:
        key.value = get_u64(r);
        break;
    case SYMBOLIC_SIZE_OF:
        key.of = type_ref(r, limit);
        break;
    case SYMBOLIC_UNARY:
    case SYMBOLIC_CAST:
    case SYMBOLIC_BINARY:
        key.op = (enum token_kind)get_u8(r);
        if (!r->failed && !symbolic_op_ok(key.kind, (uint8_t)key.op)) {
            damaged(r);
            return NULL;
        }
        key.a = read_symbolic(r, limit, depth + 1);
        if (key.kind == SYMBOLIC_BINARY && !r->failed) {
            key.b = read_symbolic(r, limit, depth + 1);
        }
        break;
    case SYMBOLIC_PARAM:
        key.of = type_ref(r, limit);
        if (!r->failed && (key.of->kind != TYPE_PARAM ||
                           key.of->param == NULL ||
                           !key.of->param->constant)) {
            damaged(r);
            return NULL;
        }
        break;
    }
    if (r->failed || !(type_is_integer(key.type) || key.type->kind == TYPE_BOOL)) {
        damaged(r);
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

static bool read_value(struct reader *r, struct type *t, struct const_value *v,
                       int depth);

/* The defaults of the parameters of sym, whose type is in place. */
static void read_param_defaults(struct reader *r, struct symbol *sym)
{
    uint32_t count = get_u32(r);
    struct param_default *list;
    uint32_t i;

    if (r->failed || count == 0) {
        return;
    }
    if (sym->type == NULL || sym->type->kind != TYPE_FN ||
        count > sym->type->param_count) {
        damaged(r);
        return;
    }
    list = allocate(r, count, sizeof *list);
    for (i = 0; i < count && !r->failed; i++) {
        uint8_t kind = get_u8(r);
        struct const_value *v;
        memset(&list[i], 0, sizeof list[i]);
        if (kind == 2) {
            list[i].here = true;
        } else if (kind == 1) {
            v = allocate(r, 1, sizeof *v);
            if (!read_value(r, sym->type->params[i], v, 0)) {
                damaged(r);
                return;
            }
            list[i].value = v;
        } else if (kind != 0) {
            damaged(r);
            return;
        }
    }
    sym->defaults = list;
    sym->default_count = count;
}

/* The `own` parameters of sym, whose type is in place. */
static void read_param_owned(struct reader *r, struct symbol *sym)
{
    uint32_t count = get_u32(r);
    bool *list;
    uint32_t i;

    if (r->failed || count == 0) {
        return;
    }
    if (sym->type == NULL || sym->type->kind != TYPE_FN ||
        count > sym->type->param_count) {
        damaged(r);
        return;
    }
    list = allocate(r, count, sizeof *list);
    for (i = 0; i < count && !r->failed; i++) {
        uint8_t owned = get_u8(r);
        if (owned > 1) {
            damaged(r);
            return;
        }
        list[i] = owned == 1;
    }
    sym->owned = list;
    sym->owned_count = count;
}

enum { MAP_NONE, MAP_BUSY, MAP_DONE };

/* DESIGN: one limit bounds how deep the tables of a library file nest.
   It covers the structs of the type table and the aggregates and the
   symbolic values of the IR. The reader recurses no deeper than the
   limit when it follows an index on demand. A table whose entries point
   back is read without recursion, and it is refused when it nests
   deeper. The checker, the layout and the passes walk the same nesting
   recursively. See docs/decisions.md. */
enum { NEST_LIMIT = 256 };

/* The larger of two heights. */
static uint32_t nest_max(uint32_t height, uint32_t h)
{
    return h > height ? h : height;
}

/* The types of a type table that hold fields, each once and sorted by
   address, with how deep each nests. */
struct nest {
    const struct type **types;
    uint32_t *heights;
    uint8_t *states;
    size_t count;
};

static int compare_types(const void *a, const void *b)
{
    uintptr_t x = (uintptr_t)*(const struct type *const *)a;
    uintptr_t y = (uintptr_t)*(const struct type *const *)b;

    return x < y ? -1 : x > y ? 1 : 0;
}

/* The place of t in n, or SIZE_MAX for a type of a library read before,
   whose reader bounded it. */
static size_t nest_find(const struct nest *n, const struct type *t)
{
    size_t low = 0;
    size_t high = n->count;

    while (low < high) {
        size_t mid = low + (high - low) / 2;
        if ((uintptr_t)n->types[mid] < (uintptr_t)t) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    return low < n->count && n->types[low] == t ? low : SIZE_MAX;
}

static uint32_t fields_height(struct nest *n, const struct type *t,
                              uint32_t depth);

static uint32_t value_height(struct nest *n, const struct type *t,
                             uint32_t depth);

/* How deep the structs that a symbolic value measures nest. Its own
   depth is bounded by read_symbolic. */
static uint32_t symbolic_height(struct nest *n, const struct symbolic *s,
                                uint32_t depth)
{
    uint32_t a;

    if (s == NULL) {
        return 0;
    }
    if (s->kind == SYMBOLIC_SIZE_OF) {
        return value_height(n, s->of, depth);
    }
    a = symbolic_height(n, s->a, depth);
    return nest_max(a, symbolic_height(n, s->b, depth));
}

/* How deep the structs inside a value of type t nest, 0 for none. The
   elements of an array are walked without recursion. */
static uint32_t value_height(struct nest *n, const struct type *t,
                             uint32_t depth)
{
    uint32_t height = 0;

    while (t->kind == TYPE_ARRAY) {
        height = nest_max(height, symbolic_height(n, t->length_of, depth));
        t = t->element;
    }
    return type_has_fields(t) ? nest_max(height, fields_height(n, t, depth))
                              : height;
}

/* How deep the struct t nests: 1 when no field holds a struct. A cycle,
   or a nesting deeper than NEST_LIMIT, gives UINT32_MAX. */
static uint32_t fields_height(struct nest *n, const struct type *t,
                              uint32_t depth)
{
    size_t at = nest_find(n, t);
    uint32_t height = 0;
    size_t i;

    if (at == SIZE_MAX) {
        return 1;
    }
    if (n->states[at] == MAP_DONE) {
        return n->heights[at];
    }
    if (n->states[at] == MAP_BUSY || depth >= NEST_LIMIT) {
        return UINT32_MAX;
    }
    n->states[at] = MAP_BUSY;
    for (i = 0; i < t->field_count && height != UINT32_MAX; i++) {
        height = nest_max(height, value_height(n, t->fields[i].type,
                                               depth + 1));
    }
    if (height == UINT32_MAX) {
        return UINT32_MAX;
    }
    n->states[at] = MAP_DONE;
    n->heights[at] = height + 1;
    return height + 1;
}

/* Refuse a type table whose structs, tuples and variants nest deeper
   than NEST_LIMIT or hold themselves. types_find_cycle then recurses no
   deeper than the limit. */
static void check_nesting(struct reader *r, uint32_t count)
{
    struct nest n;
    size_t kept = 0;
    uint32_t i;

    n.types = calloc((size_t)count + 1, sizeof *n.types);
    n.heights = calloc((size_t)count + 1, sizeof *n.heights);
    n.states = calloc((size_t)count + 1, sizeof *n.states);
    if (n.types == NULL || n.heights == NULL || n.states == NULL) {
        fputs("antic: out of memory\n", stderr);
        exit(70);
    }
    for (i = 0; i < count; i++) {
        if (type_has_fields(r->table[i])) {
            n.types[kept++] = r->table[i];
        }
    }
    qsort((void *)n.types, kept, sizeof *n.types, compare_types);
    n.count = 0;
    for (i = 0; i < kept; i++) {
        if (n.count == 0 || n.types[n.count - 1] != n.types[i]) {
            n.types[n.count++] = n.types[i];
        }
    }
    for (i = 0; i < n.count && !r->failed; i++) {
        if (fields_height(&n, n.types[i], 0) > NEST_LIMIT) {
            damaged(r);
        }
    }
    free((void *)n.types);
    free(n.heights);
    free(n.states);
}

/* The number of hooks a type parameter can meet, one bit each. */
enum { HOOK_BITS = 20 };

/* The constraints as written of a parameter or a `constraint`. */
static struct constraint_ref *read_constraint_refs(struct reader *r,
                                                   size_t *count)
{
    uint32_t n = get_count(r, 8);
    struct constraint_ref *refs = allocate(r, n, sizeof *refs);
    uint32_t i;

    for (i = 0; i < n && !r->failed; i++) {
        memset(&refs[i], 0, sizeof refs[i]);
        refs[i].module = get_name(r);
        refs[i].name = get_name(r);
    }
    *count = r->failed ? 0 : n;
    return refs;
}

/* A type parameter of the table, entry at, as put_param_type wrote it. */
static struct type *read_param_type(struct reader *r, uint32_t at)
{
    struct name name = get_name(r);
    uint8_t role = get_u8(r);
    struct type *t;
    uint32_t n;
    uint32_t i;

    if (r->failed) {
        return NULL;
    }
    t = types_param(r->types, name);
    if (role == 1 || role == 2) {
        struct type *owner = type_ref(r, at);
        if (r->failed || owner->kind != TYPE_PARAM || owner->param == NULL ||
            (role == 1 ? owner->walked : owner->indexed) != NULL) {
            damaged(r);
            return NULL;
        }
        if (role == 1) {
            owner->walked = t;
        } else {
            owner->indexed = t;
        }
        t->hook_owner = owner;
    } else if (role == 0) {
        struct type_param *tp = allocate(r, 1, sizeof *tp);
        uint8_t constant = get_u8(r);
        tp->name = name;
        tp->constant = constant == 1;
        tp->constraints = read_constraint_refs(r, &tp->constraint_count);
        tp->type = t;
        t->param = tp;
        if (constant > 1) {
            damaged(r);
        }
    } else if (role == 3) {
        struct item *set = allocate(r, 1, sizeof *set);
        set->kind = ITEM_CONSTRAINT;
        set->name = name;
        set->constraints = read_constraint_refs(r, &set->constraint_count);
        t->declared_by = set;
    } else {
        damaged(r);
        return NULL;
    }
    t->hooks = get_u32(r);
    if (t->hooks >> HOOK_BITS != 0) {
        damaged(r);
    }
    n = get_count(r, 4);
    t->ifaces = allocate(r, n, sizeof *t->ifaces);
    for (i = 0; i < n && !r->failed; i++) {
        const struct type *iface = type_ref(r, at);
        if (r->failed || iface->kind != TYPE_CLASS || !iface->has_abstract) {
            damaged(r);
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
    uint32_t n = get_count(r, 4);
    uint32_t i;

    t->type_params = allocate(r, n, sizeof *t->type_params);
    for (i = 0; i < n && !r->failed; i++) {
        struct type *p = type_ref(r, at);
        if (r->failed || p->kind != TYPE_PARAM || p->param == NULL) {
            damaged(r);
            return;
        }
        t->type_params[i] = p;
    }
    if (n == 0) {
        damaged(r);
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
    struct type *generic = type_ref(r, at);
    struct type **args;
    const struct symbolic **values;
    size_t i;

    if (r->failed || generic->type_param_count == 0 ||
        generic->kind != (enum type_kind)kind) {
        damaged(r);
        return NULL;
    }
    args = allocate(r, generic->type_param_count, sizeof *args);
    values = allocate(r, generic->type_param_count, sizeof *values);
    for (i = 0; i < generic->type_param_count && !r->failed; i++) {
        bool constant = generic->type_params[i]->param->constant;
        uint8_t is_value = get_u8(r);
        if (r->failed || is_value != (constant ? 1 : 0)) {
            damaged(r);
            return NULL;
        }
        if (constant) {
            values[i] = read_symbolic(r, at, 0);
        } else {
            args[i] = type_ref(r, at);
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
    uint32_t count = get_count(r, 1);
    struct field_refs *structs = calloc((size_t)count + 1, sizeof *structs);
    uint32_t struct_count = 0;
    uint32_t i;
    uint32_t j;

    if (structs == NULL) {
        fputs("antic: out of memory\n", stderr);
        exit(70);
    }
    r->table = allocate(r, count, sizeof *r->table);
    for (i = 0; i < count && !r->failed; i++) {
        uint8_t kind = get_u8(r);
        struct type *t = NULL;
        switch (kind) {
        case TYPE_POINTER: {
            struct type *element = type_ref(r, i);
            uint8_t form = get_u8(r);
            if (form > 3) {
                damaged(r);
            }
            if (element != NULL && !r->failed) {
                t = types_pointer_of(r->types, element, (form & 1) != 0);
                if ((form & 2) != 0) {
                    t = types_lent(r->types, t);
                }
            }
            break;
        }
        case TYPE_SLICE:
            t = type_ref(r, i);
            if (t != NULL) {
                t = types_slice(r->types, t);
            }
            break;
        /* The element of a `?T` is never a `*U` or a `fn(...)`, which
           would make a `?*U` of it. */
        case TYPE_OPTIONAL:
            t = type_ref(r, i);
            if (t != NULL &&
                ((t->kind == TYPE_POINTER || t->kind == TYPE_FN) &&
                 !t->nullable)) {
                damaged(r);
                t = NULL;
            }
            if (t != NULL) {
                t = types_with_none(r->types, t);
            }
            break;
        case TYPE_ARRAY: {
            struct type *element = type_ref(r, i);
            if (get_u8(r) != 0) {
                const struct symbolic *length = read_symbolic(r, i, 0);
                if (element != NULL && length != NULL) {
                    t = types_array_symbolic(r->types, element, length);
                }
            } else {
                uint64_t length = get_u64(r);
                if (element != NULL && !r->failed && length > 0) {
                    t = types_array(r->types, element, length);
                }
            }
            break;
        }
        case TYPE_FN: {
            uint32_t n = get_count(r, 4);
            struct type **params = allocate(r, n, sizeof *params);
            uint8_t flags;
            for (j = 0; j < n && !r->failed; j++) {
                params[j] = type_ref(r, i);
            }
            t = type_ref(r, i);
            flags = get_u8(r);
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
                damaged(r);
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
            uint32_t n = get_count(r, 4);
            struct type **elements = allocate(r, n, sizeof *elements);
            for (j = 0; j < n && !r->failed; j++) {
                elements[j] = type_ref(r, i);
            }
            if (n < 2) {
                damaged(r);
            }
            if (!r->failed) {
                t = types_tuple(r->types, elements, n);
            }
            break;
        }
        case TYPE_STRUCT:
        case TYPE_CLASS:
        case TYPE_VARIANT: {
            struct name module = get_name(r);
            struct name name = get_name(r);
            uint8_t struct_form_byte = get_u8(r);
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
                damaged(r);
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
                struct type *element = type_ref(r, i);
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
            flags = get_u8(r);
            t->is_union = (flags & 1) != 0;
            t->packed = (flags & 2) != 0;
            t->has_abstract = (flags & 4) != 0;
            t->is_final = (flags & 8) != 0;
            t->simd = (flags & 16) != 0;
            t->traced = (flags & 32) != 0;
            safety = get_u8(r);
            t->safety = (enum thread_safety)(safety & 3);
            t->unchecked_fields = (safety >> 2 & 1) != 0;
            if ((safety & 3) > SAFETY_CONCURRENT || safety > 7) {
                damaged(r);
            }
            t->compatible = get_name(r);
            t->align = get_u64(r);
            if (flags > 63 || (t->align & (t->align - 1)) != 0) {
                damaged(r);
            }
            s->s = t;
            s->count = get_count(r, 9);
            s->fields = allocate(r, s->count, sizeof *s->fields);
            s->types = allocate(r, s->count, sizeof *s->types);
            for (j = 0; j < s->count && !r->failed; j++) {
                struct name doc;
                uint8_t form;
                uint8_t marks;
                s->fields[j].name = get_name(r);
                s->types[j] = get_u32(r);
                s->fields[j].bits = get_u8(r);
                form = get_u8(r);
                s->fields[j].form = (enum field_form)(form & 15);
                s->fields[j].owned = (form >> 4 & 1) != 0;
                s->fields[j].atomic = (form >> 5 & 1) != 0;
                s->fields[j].writable = (form >> 6 & 1) != 0;
                s->fields[j].transient = (form >> 7 & 1) != 0;
                s->fields[j].vis = (enum visibility)get_u8(r);
                marks = get_u8(r);
                s->fields[j].injected = (marks & 1) != 0;
                s->fields[j].inject_final = (marks >> 1 & 1) != 0;
                s->fields[j].hidden = (marks >> 2 & 1) != 0;
                s->fields[j].unchecked = (marks >> 3 & 1) != 0;
                s->fields[j].guard = get_name(r);
                if ((form & 15) > FIELD_IMPL || s->fields[j].vis > VIS_PUB ||
                    marks > 15) {
                    damaged(r);
                }
                doc = get_name(r);
                s->fields[j].doc.text = doc.text;
                s->fields[j].doc.length = doc.length;
            }
            if (s->count == 0) {
                damaged(r);
            }
            s->member_count = get_count(r, 8);
            s->members = allocate(r, s->member_count, sizeof *s->members);
            s->member_types =
                allocate(r, s->member_count, sizeof *s->member_types);
            for (j = 0; j < s->member_count && !r->failed; j++) {
                struct item *m = arena_alloc(r->arena, sizeof *m);
                struct symbol *sym = arena_alloc(r->arena, sizeof *sym);
                uint8_t marks;
                struct name note;
                memset(m, 0, sizeof *m);
                memset(sym, 0, sizeof *sym);
                m->name = get_name(r);
                m->qualifier = get_name(r);
                s->member_types[j] = get_u32(r);
                marks = get_u8(r);
                m->contract = (enum fn_contract)(marks & 15);
                m->is_final = (marks >> 4 & 1) != 0;
                m->is_operator = (marks >> 5 & 1) != 0;
                m->may_fail = (marks >> 6 & 1) != 0;
                m->vis = (enum visibility)get_u8(r);
                if ((marks & 15) > FN_CONCRETE || m->vis > VIS_PUB) {
                    damaged(r);
                }
                note = get_name(r);
                m->doc.text = note.text;
                m->doc.length = note.length;
                m->kind = ITEM_FN;
                m->pub = m->vis == VIS_PUB;
                m->symbol = sym;
                /* The parameter names the declaration wrote, which
                   `anti doc` and the generated header print. */
                {
                    uint32_t total = get_count(r, 4);
                    struct name *names = allocate(r, total, sizeof *names);
                    struct param *list = allocate(r, total, sizeof *list);
                    uint32_t k;
                    for (k = 0; k < total && !r->failed; k++) {
                        names[k] = get_name(r);
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
                    damaged(r);
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
            struct name module = get_name(r);
            struct name name = get_name(r);
            struct type *base;
            uint32_t n;
            struct struct_field *values;
            if (r->failed) {
                break;
            }
            base = type_ref(r, i);
            n = get_count(r, 8);
            values = allocate(r, n, sizeof *values);
            /* The values of an enum are integers. */
            t = base != NULL && type_is_integer(base)
                    ? types_enum(r->types, module, name, base)
                    : NULL;
            for (j = 0; j < n && !r->failed; j++) {
                struct name doc;
                memset(&values[j], 0, sizeof values[j]);
                values[j].name = get_name(r);
                values[j].number = get_u64(r);
                values[j].type = t;
                doc = get_name(r);
                values[j].doc.text = doc.text;
                values[j].doc.length = doc.length;
            }
            if (n == 0 || t == NULL) {
                damaged(r);
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
                damaged(r);
            }
            break;
        }
        if (t == NULL) {
            damaged(r);
        }
        r->table[i] = t;
    }
    r->table_count = r->failed ? 0 : count;
    for (i = 0; i < struct_count && !r->failed; i++) {
        struct field_refs *s = &structs[i];
        for (j = 0; j < s->count; j++) {
            if (s->types[j] >= count ||
                r->table[s->types[j]]->kind == TYPE_VOID) {
                damaged(r);
                break;
            }
            s->fields[j].type = r->table[s->types[j]];
            if (s->fields[j].bits != 0 && !bitfield_fits(&s->fields[j])) {
                damaged(r);
                break;
            }
        }
        if (!r->failed) {
            types_set_fields(r->types, s->s, s->fields, s->count);
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
                damaged(r);
                break;
            }
            /* A member is a function. It takes `self` when its first
               parameter is a pointer to the class, as the checker makes
               it, and a `get` of a singleton takes none. The names the
               declaration wrote are the rest, less the out pointer of
               `may fail`. */
            fn = r->table[s->member_types[j]];
            if (fn->kind != TYPE_FN || fn->bound) {
                damaged(r);
                break;
            }
            m->has_self = takes_self(fn, s->s);
            if (fn->param_count != m->param_count + (m->has_self ? 1u : 0u) +
                                       (fn->has_out ? 1u : 0u)) {
                damaged(r);
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
            damaged(r);
        }
    }
    for (i = 0; i < struct_count && !r->failed; i++) {
        struct type *t = structs[i].s;
        for (j = 0; j < t->field_count && !r->failed; j++) {
            struct const_value *v;
            if (get_u8(r) == 0) {
                continue;
            }
            v = allocate(r, 1, sizeof *v);
            if (!read_value(r, t->fields[j].type, v, 0)) {
                damaged(r);
                break;
            }
            t->fields[j].constant = v;
        }
        for (j = 0; j < structs[i].member_count && !r->failed; j++) {
            read_param_defaults(r, structs[i].members[j]->symbol);
            read_param_owned(r, structs[i].members[j]->symbol);
        }
    }
    if (!r->failed) {
        check_nesting(r, count);
    }
    for (i = 0; i < struct_count && !r->failed; i++) {
        if (types_find_cycle(structs[i].s) != NULL) {
            damaged(r);
        }
    }
    free(structs);
}

/* A constant of type t. Aggregates hold one value per element or field,
   which the constant evaluator relies on. */
static bool read_value(struct reader *r, struct type *t, struct const_value *v,
                       int depth)
{
    uint8_t kind = get_u8(r);
    uint32_t i;
    uint32_t n;

    memset(v, 0, sizeof *v);
    v->type = t;
    v->kind = (enum const_kind)kind;
    if (r->failed || depth > 64) {
        damaged(r);
        return false;
    }
    switch (kind) {
    case CONST_INT:
        v->as.integer = get_u64(r);
        return !r->failed && (type_is_integer(t) || t->kind == TYPE_BOOL ||
                              t->kind == TYPE_ENUM);
    case CONST_FLOAT: {
        uint64_t bits = get_u64(r);
        memcpy(&v->as.floating, &bits, sizeof bits);
        return !r->failed && (type_is_float(t) || t->kind == TYPE_F16);
    }
    case CONST_BOOL:
        v->as.boolean = get_u8(r) != 0;
        return !r->failed && t->kind == TYPE_BOOL;
    case CONST_CHAR:
        v->as.character = get_u32(r);
        return !r->failed && t->kind == TYPE_CHAR;
    case CONST_NULL:
        return t->kind == TYPE_POINTER || t->kind == TYPE_FN;
    case CONST_TEXT: {
        struct name bytes = get_name(r);
        v->as.text.bytes = bytes.text;
        v->as.text.length = bytes.length;
        /* Text is a `str` or the bytes of `b"..."`. */
        return !r->failed &&
               (t->kind == TYPE_STR ||
                (t->kind == TYPE_SLICE && t->element->kind == TYPE_U8));
    }
    case CONST_SYMBOLIC:
        v->as.symbolic = read_symbolic(r, r->table_count, 0);
        return !r->failed && v->as.symbolic->type == t;
    case CONST_ARRAY:
    case CONST_STRUCT:
        n = get_count(r, 1);
        if (r->failed || (kind == CONST_ARRAY
                              ? t->kind != TYPE_ARRAY || n != t->length
                              : !type_has_fields(t) ||
                                    n != t->field_count)) {
            return false;
        }
        v->as.aggregate.count = n;
        v->as.aggregate.items = allocate(r, n, sizeof *v->as.aggregate.items);
        for (i = 0; i < n; i++) {
            struct type *item = kind == CONST_ARRAY ? t->element
                                                    : t->fields[i].type;
            if (!read_value(r, item, &v->as.aggregate.items[i], depth + 1)) {
                return false;
            }
        }
        return true;
    default:
        return false;
    }
}

static void read_items(struct reader *r)
{
    struct interface *iface = r->iface;
    uint32_t count = get_count(r, 14);
    uint32_t i;

    iface->items = allocate(r, count, sizeof *iface->items);
    r->marked_generic = allocate(r, count, sizeof *r->marked_generic);
    for (i = 0; i < count && !r->failed; i++) {
        struct symbol *sym = arena_alloc(r->arena, sizeof *sym);
        uint8_t kind = get_u8(r);
        bool generic = false;
        bool ok;
        sym->name = get_name(r);
        sym->type = type_ref(r, r->table_count);
        {
            uint8_t marks = get_u8(r);
            sym->exported = (marks & 1) != 0;
            sym->internal = (marks >> 1 & 1) != 0;
            sym->may_fail = (marks >> 2 & 1) != 0;
            sym->worker = (marks >> 3 & 1) != 0;
            sym->alias = (marks >> 4 & 1) != 0;
            /* A generic function is linked to its declaration once the
               section of the generics is read. */
            generic = (marks >> 5 & 1) != 0;
            if (marks > 63 || (sym->alias && kind != SYMBOL_STRUCT) ||
                (generic && kind != SYMBOL_FN)) {
                damaged(r);
            }
        }
        {
            struct name doc = get_name(r);
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
                struct name *names = allocate(r, sym->type->param_count,
                                              sizeof *names);
                for (j = 0; j < sym->type->param_count; j++) {
                    names[j] = get_name(r);
                }
                sym->params = names;
                read_param_defaults(r, sym);
                read_param_owned(r, sym);
            }
            if (kind == SYMBOL_EXTERN_FN) {
                sym->variadic = get_u8(r) != 0;
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
            ok = read_value(r, sym->type, sym->value, 0);
            break;
        default:
            ok = false;
            break;
        }
        if (!ok) {
            damaged(r);
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
        depth >= NEST_LIMIT) {
        damaged(r);
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
    if (height + 1 > NEST_LIMIT) {
        damaged(r);
    }
    return maps->agg_map[agg];
}

static uint32_t map_sym_at(struct reader *r, struct ir_module *program,
                           struct ir_maps *maps, uint32_t sym, uint32_t depth)
{
    struct ir_sym s;
    uint32_t height = 0;

    if (sym >= maps->sym_count || maps->sym_state[sym] == MAP_BUSY ||
        depth >= NEST_LIMIT) {
        damaged(r);
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
            damaged(r);
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
    if (height + 1 > NEST_LIMIT) {
        damaged(r);
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
    uint8_t type = get_u8(r);

    v.agg = get_u32(r);
    v.type = (enum ir_type)type;
    if (!r->failed && (!valid_type(type) || (scalar_only && type == IR_AGG) ||
                       ((type == IR_AGG) != (v.agg != IR_NO_AGG)))) {
        damaged(r);
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

/* Every field of a simd aggregate is a lane: a whole value, and not the
   unit break `_`, which has no bytes. */
static bool ir_lane(const struct ir_field *f)
{
    return f->bits == 0 && strcmp(f->name, "_") != 0;
}

static void read_tables(struct reader *r, struct ir_module *program,
                        struct ir_maps *maps)
{
    uint32_t i;
    uint32_t j;
    uint32_t a = 0;

    maps->file_count = get_count(r, 4);
    maps->files = allocate(r, maps->file_count, sizeof *maps->files);
    for (i = 0; i < maps->file_count && !r->failed; i++) {
        const char *path = get_cstr(r);
        if (!r->failed) {
            maps->files[i] = ir_file_add(program, path);
        }
    }
    maps->sym_count = get_count(r, 27);
    maps->syms = allocate(r, maps->sym_count, sizeof *maps->syms);
    maps->sym_map = allocate(r, maps->sym_count, sizeof *maps->sym_map);
    maps->sym_state = allocate(r, maps->sym_count, sizeof *maps->sym_state);
    maps->sym_height = allocate(r, maps->sym_count, sizeof *maps->sym_height);
    for (i = 0; i < maps->sym_count && !r->failed; i++) {
        struct ir_sym *s = &maps->syms[i];
        uint8_t kind = get_u8(r);
        uint8_t type = get_u8(r);
        s->kind = (enum ir_sym_kind)kind;
        s->type = (enum ir_type)type;
        s->value = get_u64(r);
        s->of = read_vtype(r, false);
        s->field = get_u32(r);
        s->op = get_u8(r);
        s->a = get_u32(r);
        s->b = get_u32(r);
        if (kind > IR_SYM_OP || type < IR_I8 ||
            (type > IR_I64 && type != IR_CLONG && type != IR_CWCHAR &&
             type != IR_LOCK) ||
            s->op > IR_RET ||
            ((kind == IR_SYM_SIZE_OF || kind == IR_SYM_OFFSET_OF) &&
             s->of.type == IR_VOID)) {
            damaged(r);
        }
    }
    maps->agg_count = get_count(r, 26);
    maps->aggs = allocate(r, maps->agg_count, sizeof *maps->aggs);
    maps->agg_map = allocate(r, maps->agg_count, sizeof *maps->agg_map);
    maps->agg_state = allocate(r, maps->agg_count, sizeof *maps->agg_state);
    maps->agg_height = allocate(r, maps->agg_count, sizeof *maps->agg_height);
    for (i = 0; i < maps->agg_count && !r->failed; i++) {
        struct ir_aggtype *t = &maps->aggs[i];
        uint8_t kind = get_u8(r);
        uint8_t flags;
        t->kind = (enum ir_agg_kind)kind;
        t->name = get_cstr(r);
        flags = get_u8(r);
        t->packed = (flags & 1) != 0;
        t->simd = (flags & 2) != 0;
        t->align = get_u64(r);
        if (flags > 3 || (t->align & (t->align - 1)) != 0) {
            damaged(r);
        }
        t->length = get_u32(r);
        t->length_text = get_cstr(r);
        t->field_count = get_count(r, 10);
        t->fields = allocate(r, t->field_count, sizeof *t->fields);
        for (j = 0; j < t->field_count && !r->failed; j++) {
            uint8_t ext;
            t->fields[j].name = get_cstr(r);
            t->fields[j].type = read_vtype(r, false);
            t->fields[j].bits = get_u8(r);
            ext = get_u8(r);
            t->fields[j].ext = (enum ir_ext)ext;
            if (ext > IR_EXT_ZERO || t->fields[j].type.type == IR_VOID ||
                (t->fields[j].bits != 0 && !ir_bitfield_fits(&t->fields[j])) ||
                (t->simd && !ir_lane(&t->fields[j]))) {
                damaged(r);
            }
        }
        if (kind > IR_AGG_ARRAY || t->name[0] == '\0' ||
            t->field_count == 0 ||
            (kind == IR_AGG_ARRAY && t->field_count != 1)) {
            damaged(r);
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
    uint32_t agg = get_u32(r);

    if (r->failed || (type == IR_AGG) != (agg != IR_NO_AGG)) {
        damaged(r);
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
        damaged(r);
        return NULL;
    }
    kind = get_u8(r);
    scalar = get_u8(r);
    payload = get_u64(r);
    if (r->failed || kind > IR_CONST_AGG || !valid_type(scalar)) {
        damaged(r);
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
            damaged(r);
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
            damaged(r);
            return NULL;
        }
        c->type.agg = map_agg(r, program, maps, c->type.agg);
        /* An item costs at least its kind, its type and its payload, so
           a count past that many bytes cannot be honest. */
        if (payload > (uint64_t)(r->size - r->pos) / 10) {
            damaged(r);
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
            damaged(r);
            return;
        }
        c->global = maps->globals[c->global];
    } else if (c->kind == IR_CONST_FUNC && functions) {
        if (c->global >= maps->function_count) {
            damaged(r);
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
    uint8_t kind = get_u8(r);
    uint8_t type = get_u8(r);
    uint64_t payload = get_u64(r);

    if (r->failed) {
        return o;
    }
    if (kind > IR_SYM || !valid_type(type)) {
        damaged(r);
        return o;
    }
    o.kind = (enum ir_operand_kind)kind;
    o.type = (enum ir_type)type;
    switch (o.kind) {
    case IR_TEMP:
        if (payload >= f->temp_count || f->temps[payload] != o.type) {
            damaged(r);
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
            damaged(r);
        } else {
            o.as.index = maps->globals[payload];
        }
        break;
    case IR_FUNC:
        if (payload >= maps->function_count) {
            damaged(r);
        } else {
            o.as.index = maps->functions[payload];
        }
        break;
    case IR_BLOCK:
        if (payload >= f->block_count) {
            damaged(r);
        } else {
            o.as.index = (uint32_t)payload;
        }
        break;
    case IR_SYM:
        if (payload >= maps->sym_count ||
            maps->syms[payload].type != o.type) {
            damaged(r);
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
    uint32_t temps = get_count(r, 1);
    uint32_t blocks;
    uint32_t i;
    uint32_t j;
    uint32_t k;

    for (i = 0; i < temps && !r->failed; i++) {
        uint8_t type = get_u8(r);
        if (i < f->param_count) {
            if (type != f->temps[i]) {
                damaged(r);
            }
        } else if (!valid_type(type) || type == IR_VOID || type == IR_AGG) {
            damaged(r);
        } else {
            ir_temp(f, (enum ir_type)type);
        }
    }
    if (temps < f->param_count) {
        damaged(r);
    }
    blocks = get_count(r, 4);
    /* A body starts at block 0, so it has one. */
    if (blocks == 0) {
        damaged(r);
    }
    for (i = 0; i < blocks && !r->failed; i++) {
        ir_block_add(f);
    }
    for (i = 0; i < blocks && !r->failed; i++) {
        uint32_t count;
        uint8_t fail_kind;
        fail_kind = get_u8(r);
        if (fail_kind > IR_FAIL_CHECK) {
            damaged(r);
        }
        f->blocks[i]->fail = (enum ir_fail)fail_kind;
        count = get_count(r, 50);
        for (j = 0; j < count && !r->failed; j++) {
            struct ir_inst inst;
            struct ir_operand *args;
            uint8_t op = get_u8(r);
            uint8_t type = get_u8(r);
            memset(&inst, 0, sizeof inst);
            inst.line = get_u32(r);
            inst.result = get_u32(r);
            inst.op = (enum ir_op)op;
            inst.type = (enum ir_type)type;
            if (op > IR_RET || !valid_type(type) ||
                (inst.result != IR_NO_RESULT && inst.result >= f->temp_count)) {
                damaged(r);
                break;
            }
            inst.a = read_operand(r, f, maps);
            inst.b = read_operand(r, f, maps);
            inst.c = read_operand(r, f, maps);
            if (!targets_blocks(&inst)) {
                damaged(r);
                break;
            }
            inst.of = read_vtype(r, false);
            inst.field = get_u32(r);
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
                damaged(r);
            }
            inst.arg_count = get_count(r, 10);
            args = allocate(r, inst.arg_count, sizeof *args);
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
    uint8_t flags = get_u8(r);
    const char *module = get_cstr(r);
    const char *name = get_cstr(r);
    uint8_t result = get_u8(r);
    uint32_t result_agg = read_agg_ref(r, program, maps, result);
    uint32_t file = get_u32(r);
    uint32_t decl_line = get_u32(r);
    uint32_t param_count = get_count(r, 6);
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
        damaged(r);
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
        fail(r, "defines `%s.%s`, which another library defines", module, name);
        return 0;
    }
    if (file != IR_NO_INDEX && file >= maps->file_count) {
        damaged(r);
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
            uint8_t type = get_u8(r);
            uint8_t ext = get_u8(r);
            uint32_t agg = read_agg_ref(r, program, maps, type);
            bool narrow = type == IR_I8 || type == IR_I16;
            if (!valid_type(type) || type == IR_VOID || ext > IR_EXT_ZERO ||
                (ext != IR_EXT_NONE) != narrow) {
                damaged(r);
            } else {
                ir_param_add(f, (enum ir_type)type, agg);
                f->params[f->param_count - 1].ext = (enum ir_ext)ext;
            }
        }
    } else {
        for (i = 0; i < param_count && !r->failed; i++) {
            uint8_t type = get_u8(r);
            get_u8(r);
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
        damaged(r);
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
    uint32_t count = get_count(r, 37);
    uint32_t i;
    uint32_t j;

    for (i = 0; i < count && !r->failed; i++) {
        const char *module = get_cstr(r);
        const char *name = get_cstr(r);
        uint8_t flags = get_u8(r);
        uint32_t descriptor = get_u32(r);
        uint32_t base = get_u32(r);
        uint32_t table = get_u32(r);
        uint32_t init = get_u32(r);
        uint32_t agg = get_u32(r);
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
            damaged(r);
            return;
        }
        c = ir_class_add(program, module, name);
        c->flags = flags;
        c->descriptor = map_global(r, maps, descriptor, false);
        c->base = map_global(r, maps, base, false);
        c->table = map_global(r, maps, table, true);
        c->init = init == IR_NO_INDEX ? init : maps->functions[init];
        c->agg = map_agg(r, program, maps, agg);
        subtables = get_count(r, 16);
        for (j = 0; j < subtables && !r->failed; j++) {
            uint32_t interface = get_u32(r);
            uint32_t at = get_u32(r);
            uint32_t sub_agg = get_u32(r);
            uint32_t sub_field = get_u32(r);
            uint32_t mapped = map_agg(r, program, maps, sub_agg);
            uint32_t mapped_interface;
            uint32_t mapped_at;
            if (r->failed || sub_field >= program->aggs[mapped]->field_count) {
                damaged(r);
                return;
            }
            /* Each call may mark the file damaged, so each has a line
               of its own. */
            mapped_interface = map_global(r, maps, interface, false);
            mapped_at = map_global(r, maps, at, false);
            ir_class_subtable(c, mapped_interface, mapped_at, mapped,
                              sub_field);
        }
        mutables = get_count(r, 4);
        for (j = 0; j < mutables && !r->failed; j++) {
            uint32_t field = get_u32(r);
            if (r->failed || field >= program->aggs[c->agg]->field_count) {
                damaged(r);
            }
            ir_class_mutable(c, field);
        }
        injects = get_count(r, 13);
        for (j = 0; j < injects && !r->failed; j++) {
            const char *path = get_cstr(r);
            const char *named = get_cstr(r);
            uint32_t of = get_u32(r);
            uint8_t last = get_u8(r);
            if (r->failed || path[0] == '\0' || last > 1) {
                damaged(r);
                return;
            }
            ir_class_inject(program, c, path, named,
                            map_global(r, maps, of, false), last != 0);
        }
        provides = get_count(r, 8);
        for (j = 0; j < provides && !r->failed; j++) {
            const char *path = get_cstr(r);
            uint32_t of = get_u32(r);
            if (r->failed || path[0] == '\0') {
                damaged(r);
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
    scratch.temps = malloc((f->param_count + 1) * sizeof *scratch.temps);
    if (scratch.temps == NULL) {
        fputs("antic: out of memory\n", stderr);
        exit(70);
    }
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
    maps.global_count = get_count(r, 28);
    maps.globals = allocate(r, maps.global_count, sizeof *maps.globals);
    relocs = allocate(r, maps.global_count, sizeof *relocs);
    twins = allocate(r, maps.global_count, sizeof *twins);
    for (i = 0; i < maps.global_count && !r->failed; i++) {
        const char *module = get_cstr(r);
        const char *name = get_cstr(r);
        uint64_t size = get_u64(r);
        uint64_t align = get_u64(r);
        struct ir_global *g;
        /* A global of the runtime has no module, and an empty name is
           how the file spells that. */
        if (module != NULL && module[0] == '\0') {
            module = NULL;
        }
        if (!take(r, size)) {
            break;
        }
        g = ir_global_add(program, module, name, r->data + r->pos, size, align);
        r->pos += size;
        maps.globals[i] = g->index;
        relocs[i].count = get_count(r, 13);
        relocs[i].offsets = allocate(r, relocs[i].count, sizeof(uint64_t));
        relocs[i].targets = allocate(r, relocs[i].count, sizeof(uint32_t));
        relocs[i].functions = allocate(r, relocs[i].count, sizeof(uint8_t));
        for (j = 0; j < relocs[i].count && !r->failed; j++) {
            relocs[i].offsets[j] = get_u64(r);
            relocs[i].targets[j] = get_u32(r);
            relocs[i].functions[j] = get_u8(r) != 0 ? 1 : 0;
            if (relocs[i].offsets[j] > size ||
                size - relocs[i].offsets[j] < 8) {
                damaged(r);
            }
        }
        {
            uint8_t marks = get_u8(r);
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
                damaged(r);
                break;
            }
            ir_global_reloc(program, g, relocs[i].offsets[j],
                            maps.globals[relocs[i].targets[j]]);
        }
    }
    maps.function_count = get_count(r, 15);
    maps.functions = allocate(r, maps.function_count, sizeof *maps.functions);
    bodies = allocate(r, maps.function_count, sizeof *bodies);
    skips = allocate(r, maps.function_count, sizeof *skips);
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
                damaged(r);
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

/* The helpers antl_tree.c shares, see antl_io.h. */

void antl_put_u8(struct writer *w, uint8_t v) { put_u8(w, v); }
void antl_put_u32(struct writer *w, uint32_t v) { put_u32(w, v); }
void antl_put_u64(struct writer *w, uint64_t v) { put_u64(w, v); }
void antl_put_count(struct writer *w, size_t n) { put_count(w, n); }

void antl_put_bytes(struct writer *w, const char *s, size_t length)
{
    put_bytes(w, s, length);
}

void antl_put_type_ref(struct writer *w, const struct type *t)
{
    put_type_ref(w, t);
}

void antl_visit_type(struct writer *w, const struct type *t)
{
    visit_type(w, t);
}

void antl_visit_value(struct writer *w, const struct const_value *v)
{
    visit_value(w, v);
}

void antl_visit_defaults(struct writer *w, const struct symbol *sym)
{
    visit_defaults(w, sym);
}

void antl_visit_symbolic(struct writer *w, const struct symbolic *s)
{
    visit_symbolic(w, s);
}

void antl_put_symbolic(struct writer *w, const struct symbolic *s)
{
    put_symbolic(w, s);
}

const struct symbolic *antl_read_symbolic(struct reader *r)
{
    return read_symbolic(r, r->table_count, 0);
}

void antl_put_value(struct writer *w, const struct const_value *v)
{
    put_value(w, v);
}

void antl_put_param_defaults(struct writer *w, const struct symbol *sym)
{
    put_param_defaults(w, sym);
}

void antl_put_param_owned(struct writer *w, const struct symbol *sym)
{
    put_param_owned(w, sym);
}

uint64_t antl_float_bits(double d) { return float_bits(d); }

void antl_damaged(struct reader *r) { damaged(r); }

void antl_fail_needs(struct reader *r, const char *what, const char *module,
                     const struct name *name)
{
    fail(r, "needs %s `%s.%.*s`", what, module, (int)name->length,
         name->text);
}

uint8_t antl_get_u8(struct reader *r) { return get_u8(r); }
uint32_t antl_get_u32(struct reader *r) { return get_u32(r); }
uint64_t antl_get_u64(struct reader *r) { return get_u64(r); }

uint32_t antl_get_count(struct reader *r, size_t min)
{
    return get_count(r, min);
}

void *antl_allocate(struct reader *r, size_t count, size_t size)
{
    return allocate(r, count, size);
}

struct name antl_get_name(struct reader *r) { return get_name(r); }

struct type *antl_type_ref(struct reader *r, uint32_t limit)
{
    return type_ref(r, limit);
}

bool antl_read_value(struct reader *r, struct type *t, struct const_value *v)
{
    return read_value(r, t, v, 0);
}

void antl_read_param_defaults(struct reader *r, struct symbol *sym)
{
    read_param_defaults(r, sym);
}

void antl_read_param_owned(struct reader *r, struct symbol *sym)
{
    read_param_owned(r, sym);
}

const struct interface *antl_library(const struct reader *r,
                                     const struct name *module)
{
    return library(r, module);
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
        if (library(&r, &imported) == NULL) {
            fail(&r, "needs module `%s`", imported.text);
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
        damaged(&r);
    }
    return r.failed ? NULL : r.iface;
}

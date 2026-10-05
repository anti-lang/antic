/* The reader of the type table, the constants and the items of a library
   file, with the type parameters, the copies of generics and the structs
   of other modules each names. */

#include "alloc.h"
#include "antl_io.h"

#include <stdlib.h>
#include <string.h>

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
    return !types_field_is_unit_break(f) && types_is_integer(f->type) &&
           !types_is_target_sized(f->type) &&
           f->bits <= (unsigned)types_bits(f->type);
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
    if (r->failed ||
        !(types_is_integer(key.type) || key.type->kind == TYPE_BOOL)) {
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

/* Refuse a type table whose values nest deeper than TYPES_NEST_MAX or
   hold themselves. A type of a library read before was measured by its
   reader. */
static void check_nesting(struct reader *r, uint32_t count)
{
    uint32_t i;

    for (i = 0; i < count && !r->failed; i++) {
        if (types_has_fields(r->table[i]) &&
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

/* A type parameter of the table, entry at, as put_param_type of
   antl_write.c wrote it. */
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
        struct type *iface = antl_type_ref(r, at);
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

/* A pointer to an element written before it, with its `?` and `lent`
   marks. */
static struct type *read_pointer(struct reader *r, uint32_t at)
{
    struct type *element = antl_type_ref(r, at);
    uint8_t form = antl_get_u8(r);
    struct type *t = NULL;

    if (form > 3) {
        antl_damaged(r);
    }
    if (element != NULL && !r->failed) {
        t = types_pointer_of(r->types, element, (form & 1) != 0);
        if ((form & 2) != 0) {
            t = types_lent(r->types, t);
        }
    }
    return t;
}

/* A slice of an element written before it, with its `lent` mark. */
static struct type *read_slice(struct reader *r, uint32_t at)
{
    struct type *element = antl_type_ref(r, at);
    uint8_t lent = antl_get_u8(r);
    struct type *t = NULL;

    if (lent > 1) {
        antl_damaged(r);
    }
    if (element != NULL && !r->failed) {
        t = types_slice(r->types, element);
        if (lent != 0) {
            t = types_lent(r->types, t);
        }
    }
    return t;
}

/* The element of a `?T` is never a `*U` or a `fn(...)`, which would make
   a `?*U` of it. */
static struct type *read_optional(struct reader *r, uint32_t at)
{
    struct type *t = antl_type_ref(r, at);

    if (t != NULL &&
        ((t->kind == TYPE_POINTER || t->kind == TYPE_FN) &&
         !t->nullable)) {
        antl_damaged(r);
        t = NULL;
    }
    if (t != NULL) {
        t = types_with_none(r->types, t);
    }
    return t;
}

/* An array of a number of elements, or of a length from `size_of`. */
static struct type *read_array(struct reader *r, uint32_t at)
{
    struct type *element = antl_type_ref(r, at);
    struct type *t = NULL;

    if (antl_get_u8(r) != 0) {
        const struct symbolic *length = antl_read_symbolic(r, at, 0);
        if (element != NULL && length != NULL) {
            t = types_array_symbolic(r->types, element, length);
        }
    } else {
        uint64_t length = antl_get_u64(r);
        if (element != NULL && !r->failed && length > 0) {
            t = types_array(r->types, element, length);
        }
    }
    return t;
}

/* A function type: its parameters, its result and the flags of its
   form. */
static struct type *read_fn(struct reader *r, uint32_t at)
{
    uint32_t n = antl_get_count(r, 4);
    struct type **params = antl_allocate(r, n, sizeof *params);
    struct type *t;
    uint8_t flags;
    uint32_t j;

    for (j = 0; j < n && !r->failed; j++) {
        params[j] = antl_type_ref(r, at);
    }
    t = antl_type_ref(r, at);
    flags = antl_get_u8(r);
    if (r->failed) {
        return t;
    }
    /* The out pointer belongs to the `may fail` form alone, and it
       is the last parameter. `concurrent` marks the form of two
       words alone, which a bound function never has. A function that
       returns `never` cannot fail. */
    if (((flags & 128) != 0 && (flags & 4) != 0) ||
        ((flags & 32) != 0 && (flags & 16) == 0) ||
        ((flags & 64) != 0 && (flags & 48) != 48) ||
        ((flags & 16) != 0 && (flags & 2) != 0) ||
        ((flags & 8) != 0 &&
         ((flags & 4) == 0 || n == 0 ||
          params[n - 1] == NULL ||
          params[n - 1]->kind != TYPE_POINTER))) {
        antl_damaged(r);
        return NULL;
    }
    t = types_fn_flagged(r->types, params, n, t, (flags & 2) != 0,
                         (flags & 4) != 0, (flags & 8) != 0);
    if ((flags & 128) != 0) {
        t = types_fn_never(r->types, t);
    }
    if ((flags & 64) != 0) {
        t = types_fn_owned(r->types, t);
    } else if ((flags & 16) != 0) {
        t = types_fn_form(r->types, t, true, (flags & 32) != 0);
    }
    if ((flags & 1) != 0) {
        t = types_with_none(r->types, t);
    }
    return t;
}

/* The elements name types written before them, so the tuple this
   module reads is the one every other module of the program interns. */
static struct type *read_tuple(struct reader *r, uint32_t at)
{
    uint32_t n = antl_get_count(r, 4);
    struct type **elements = antl_allocate(r, n, sizeof *elements);
    uint32_t j;

    for (j = 0; j < n && !r->failed; j++) {
        elements[j] = antl_type_ref(r, at);
    }
    if (n < 2) {
        antl_damaged(r);
    }
    return r->failed ? NULL : types_tuple(r->types, elements, n);
}

/* Whether module and name name a struct of `anti.lang` that the
   compiler knows and that carries nothing more, which goes to t. */
static bool lang_struct(struct reader *r, const struct name *module,
                        const struct name *name, struct type **t)
{
    if (names_lang(module, name, LANG_MUTEX)) {
        *t = types_mutex(r->types);
    } else if (names_lang(module, name, LANG_REGEX)) {
        *t = types_regex(r->types);
    } else if (names_lang(module, name, LANG_BYTE_REGEX)) {
        *t = types_byte_regex(r->types);
    } else if (names_lang(module, name, LANG_BYTE_MATCH)) {
        *t = types_match_of(r->types, true);
    } else if (names_lang(module, name, LANG_MATCH)) {
        /* A match of a literal is written as the plain match of its
           form, since the literal stays in the module that wrote it. */
        *t = types_match(r->types, NULL);
    } else if (names_lang(module, name, LANG_OBJECT_LOCK)) {
        *t = types_object_lock(r->types);
    } else {
        return false;
    }
    return true;
}

/* The fields of the struct s, whose types are indices into the table,
   which the reader puts in place once the table is whole. */
static void read_fields(struct reader *r, struct field_refs *s)
{
    uint32_t j;

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
}

/* The parameter names the declaration of m wrote, which `anti doc` and
   the generated header print. */
static void read_param_names(struct reader *r, struct item *m,
                             struct symbol *sym)
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

/* The public functions of the body of the class s, each an item with a
   symbol whose type is an index into the table. */
static void read_members(struct reader *r, struct field_refs *s)
{
    uint32_t j;

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
        read_param_names(r, m, sym);
        sym->kind = SYMBOL_FN;
        sym->item = m;
        sym->home = r->iface;
        sym->may_fail = m->may_fail;
        sym->doc = m->doc;
        s->members[j] = m;
    }
}

/* The marks of the struct t: its flags, its thread safety, the version
   it stays compatible with and its alignment. */
static void read_struct_marks(struct reader *r, struct type *t)
{
    uint8_t flags = antl_get_u8(r);
    uint8_t safety;

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
}

/* A struct, a class or a variant of kind. One the module declares takes
   the next of structs, whose fields and functions the reader binds once
   the table is whole. A copy the program has already stands for the one
   read. */
static struct type *read_struct(struct reader *r, uint32_t at, uint8_t kind,
                                struct field_refs *structs,
                                uint32_t *struct_count)
{
    struct name module = antl_get_name(r);
    struct name name = antl_get_name(r);
    uint8_t struct_form_byte = antl_get_u8(r);
    struct type *generic = NULL;
    struct type **args = NULL;
    const struct symbolic **values = NULL;
    struct type *existing = NULL;
    struct field_refs *s;
    struct type *t;

    if (r->failed) {
        return NULL;
    }
    if (struct_form_byte > ANTL_FORM_COPY) {
        antl_damaged(r);
        return NULL;
    }
    if (struct_form_byte == ANTL_FORM_COPY) {
        generic = read_copy_args(r, at, kind, &args, &values);
        if (generic == NULL) {
            return NULL;
        }
        existing = copy_among(generic, args, values);
    } else if (kind == TYPE_STRUCT &&
               names_lang(&module, &name, LANG_CHAN)) {
        struct type *element = antl_type_ref(r, at);
        return r->failed ? NULL : types_chan(r->types, element);
    }
    if (kind == TYPE_STRUCT && lang_struct(r, &module, &name, &t)) {
        return t;
    }
    /* The root carries the path of `anti.lang` and is still
       no struct of its library file. */
    if ((struct_form_byte != ANTL_FORM_COPY &&
         !antl_name_equals(&module, r->iface->module)) ||
        names_lang(&module, &name, LANG_OBJECT) ||
        names_lang(&module, &name, LANG_FLAGS) ||
        names_lang(&module, &name, LANG_FIELD_DESCRIPTOR)) {
        return foreign_struct(r, &module, &name);
    }
    s = &structs[(*struct_count)++];
    t = types_struct(r->types, module, name);
    t->kind = (enum type_kind)kind;
    read_struct_marks(r, t);
    s->s = t;
    read_fields(r, s);
    read_members(r, s);
    if (struct_form_byte == ANTL_FORM_GENERIC) {
        read_type_params(r, at, t);
    } else if (struct_form_byte == ANTL_FORM_COPY && existing != NULL) {
        /* The body read stays behind, and the copy the program
           has already stands for it. */
        t = existing;
    } else if (struct_form_byte == ANTL_FORM_COPY) {
        if (s->member_count > 0) {
            antl_damaged(r);
        }
        t->generic = generic;
        t->args = args;
        t->values = values;
        t->next_copy = generic->copies;
        generic->copies = t;
    }
    return t;
}

/* An enum carries its values, each with a name and a number. */
static struct type *read_enum(struct reader *r, uint32_t at)
{
    struct name module = antl_get_name(r);
    struct name name = antl_get_name(r);
    struct type *base;
    uint32_t n;
    struct struct_field *values;
    struct type *t;
    uint32_t j;

    if (r->failed) {
        return NULL;
    }
    base = antl_type_ref(r, at);
    n = antl_get_count(r, 8);
    values = antl_allocate(r, n, sizeof *values);
    /* The values of an enum are integers. */
    t = base != NULL && types_is_integer(base)
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
    return t;
}

/* The entry at of the table, whose kind is read. */
static struct type *read_type(struct reader *r, uint32_t at,
                              struct field_refs *structs,
                              uint32_t *struct_count)
{
    uint8_t kind = antl_get_u8(r);

    switch (kind) {
    case TYPE_POINTER:
        return read_pointer(r, at);
    case TYPE_SLICE:
        return read_slice(r, at);
    case TYPE_OPTIONAL:
        return read_optional(r, at);
    case TYPE_ARRAY:
        return read_array(r, at);
    case TYPE_FN:
        return read_fn(r, at);
    case TYPE_TUPLE:
        return read_tuple(r, at);
    case TYPE_STRUCT:
    case TYPE_CLASS:
    case TYPE_VARIANT:
        return read_struct(r, at, kind, structs, struct_count);
    case TYPE_PARAM:
        return read_param_type(r, at);
    case TYPE_ENUM:
        return read_enum(r, at);
    /* The tree of a generic carries the type of `none` before a
       context gives it one. It carries the error type as well, on
       the name of a module before a `.`, which no pass reads. */
    default:
        if (kind <= TYPE_ERROR) {
            return types_builtin(r->types, (enum type_kind)kind);
        }
        antl_damaged(r);
        return NULL;
    }
}

/* Put the types of the fields and the functions of the struct s in
   place, now that the table of count entries is whole. */
static void bind_struct(struct reader *r, struct field_refs *s,
                        uint32_t count)
{
    uint32_t j;

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

/* The constants that the fields of t take by default, and the defaults
   and owned parameters of the functions of s. */
static void read_struct_defaults(struct reader *r, const struct field_refs *s)
{
    struct type *t = s->s;
    uint32_t j;

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
    for (j = 0; j < s->member_count && !r->failed; j++) {
        antl_read_param_defaults(r, s->members[j]->symbol);
        antl_read_param_owned(r, s->members[j]->symbol);
    }
}

void antl_read_types(struct reader *r)
{
    uint32_t count = antl_get_count(r, 1);
    struct field_refs *structs =
        alloc_zeroed((size_t)count + 1, sizeof *structs);
    uint32_t struct_count = 0;
    uint32_t i;

    r->table = antl_allocate(r, count, sizeof *r->table);
    for (i = 0; i < count && !r->failed; i++) {
        struct type *t = read_type(r, i, structs, &struct_count);
        if (t == NULL) {
            antl_damaged(r);
        }
        r->table[i] = t;
    }
    r->table_count = r->failed ? 0 : count;
    for (i = 0; i < struct_count && !r->failed; i++) {
        bind_struct(r, &structs[i], count);
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
        read_struct_defaults(r, &structs[i]);
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
        return !r->failed && (types_is_integer(t) || t->kind == TYPE_BOOL ||
                              t->kind == TYPE_ENUM);
    case CONST_FLOAT: {
        uint64_t bits = antl_get_u64(r);
        memcpy(&v->as.floating, &bits, sizeof bits);
        return !r->failed && (types_is_float(t) || t->kind == TYPE_F16);
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
                              : !types_has_fields(t) ||
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

void antl_read_items(struct reader *r)
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
                 ((types_has_fields(sym->type) ||
                   sym->type->kind == TYPE_ENUM) &&
                  antl_name_equals(&sym->type->module, iface->module));
            break;
        case SYMBOL_CONSTRAINT:
            ok = sym->type->kind == TYPE_PARAM && sym->type->param == NULL &&
                 sym->type->hook_owner == NULL;
            break;
        case SYMBOL_CONST: {
            struct const_value *value = arena_alloc(r->arena, sizeof *value);
            ok = antl_read_value(r, sym->type, value, 0);
            sym->value = value;
            break;
        }
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

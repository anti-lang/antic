/* The checks of calls and members: fields, functions of a class,
   visibility, `construct`, errors and their handlers, atomics, channels,
   the calls of `anti.simd`, plugins and workers. */

#include <stdint.h>
#include <string.h>

#include "sema_checker.h"

static struct symbol *function_symbol(const struct expr *callee)
{
    if ((callee->kind == EXPR_NAME || callee->kind == EXPR_FIELD) &&
        callee->symbol != NULL &&
        (callee->symbol->kind == SYMBOL_FN ||
         callee->symbol->kind == SYMBOL_EXTERN_FN)) {
        return callee->symbol;
    }
    return NULL;
}

/* The struct or class behind a value of type T or *T, or NULL. */
struct type *sema_struct_of(struct type *t)
{
    if (t->kind == TYPE_POINTER) {
        t = t->element;
    }
    return type_has_fields(t) ? t : NULL;
}

const struct struct_field *sema_find_field(const struct type *s,
                                           const struct name *name)
{
    size_t i;

    for (i = 0; i < s->field_count; i++) {
        if (sema_same_name(&s->fields[i].name, name) &&
            !type_field_is_unit_break(&s->fields[i])) {
            return &s->fields[i];
        }
    }
    return NULL;
}

struct expr *sema_new_node(struct checker *c, enum expr_kind kind,
                           struct pos pos)
{
    struct expr *e = arena_alloc(c->arena, sizeof *e);
    e->kind = kind;
    e->pos = pos;
    return e;
}

/* DESIGN: a name used on a value or a type reaches the member that
   find_member gives, less the bodies qualified by an interface. Each of
   those fills the table of its interface alone. At one level a plain
   body comes before one qualified by a base. A call therefore reaches the
   unqualified function when there is one. Without one, bodies qualified
   by one interface give the nearest of them. A use of it goes through
   the table of that interface. Two interfaces make the name ambiguous,
   which ambiguous_member reports. */
static struct item *reached_member(const struct type *t,
                                   const struct name *name)
{
    const struct type *level;
    struct item *lone = NULL;
    bool two = false;
    size_t i;

    for (level = t; level != NULL;
         level = level->kind == TYPE_CLASS ? level->base : NULL) {
        struct item *base_body = NULL;
        for (i = 0; i < level->member_count; i++) {
            struct item *m = level->members[i];
            if (!sema_same_name(&m->name, name)) {
                continue;
            }
            if (m->kind != ITEM_FN) {
                return m;
            }
            switch (types_body_table(level, m)) {
            case BODY_PLAIN:
                return m;
            case BODY_BASE:
                base_body = base_body != NULL ? base_body : m;
                break;
            case BODY_INTERFACE:
                if (lone == NULL) {
                    lone = m;
                } else if (!sema_same_name(&lone->qualifier, &m->qualifier)) {
                    two = true;
                }
                break;
            }
        }
        if (base_body != NULL) {
            return base_body;
        }
    }
    return two ? NULL : lone;
}

/* DESIGN: take a name that only bodies qualified by two or more
   interfaces fill on the chain of t. It is ambiguous on a value of t,
   and the message names the interfaces. The program reaches one through
   a sub-object or an interface pointer. Returns whether it reported. */
static bool ambiguous_member(struct checker *c, struct pos pos,
                             const struct type *t, const struct name *name,
                             const char *what)
{
    const struct name *seen[16];
    size_t count = 0;
    struct text list = {0};
    const struct type *up;
    size_t i;
    size_t k;

    if (t == NULL || t->kind != TYPE_CLASS ||
        reached_member(t, name) != NULL) {
        return false;
    }
    for (up = t; up != NULL; up = up->base) {
        for (i = 0; i < up->member_count; i++) {
            const struct item *m = up->members[i];
            if (m->kind != ITEM_FN || !sema_same_name(&m->name, name) ||
                types_body_table(up, m) != BODY_INTERFACE) {
                continue;
            }
            for (k = 0; k < count; k++) {
                if (sema_same_name(seen[k], &m->qualifier)) {
                    break;
                }
            }
            if (k == count && count < sizeof seen / sizeof seen[0]) {
                seen[count++] = &m->qualifier;
            }
        }
    }
    if (count == 0) {
        return false;
    }
    for (k = 0; k < count; k++) {
        text_appendf(&list, "%s`%.*s`",
                     k == 0 ? "" : k + 1 == count ? " and " : ", ",
                     (int)seen[k]->length, seen[k]->text);
    }
    sema_error_at(c, pos, "`%s` fills `%.*s` only for %s, so %s is ambiguous, "
                  "reach it through an interface", sema_tn(t),
                  (int)name->length,
                  name->text, text_cstr(&list), what);
    text_free(&list);
    return true;
}

/* DESIGN: an abstract function has no body, and no function of the IR
   stands for it. A direct use of one, `self.super.f()` or `T.f`, would
   reach the function at index 0 of the module, so the checker refuses
   it. A use through a table reads the entry the object's class filled.
   Returns whether it reported. */
static bool refuse_abstract_call(struct checker *c, struct pos pos,
                                 const struct type *t, const struct item *m)
{
    const struct type *level;

    if (m == NULL || m->kind != ITEM_FN || m->contract != FN_ABSTRACT) {
        return false;
    }
    level = types_member_level(t, m);
    sema_error_at(c, pos, "`%.*s` is abstract in `%s` and has no body to call",
                  (int)m->name.length, m->name.text,
                  sema_tn(level != NULL ? level : t));
    return true;
}

/* DESIGN: a use of a function on a class reaches it through a table.
   Two kinds hold no entry of the primary table: a body qualified by an
   interface, and a plain body beside one qualified by a base. Each fills
   the table of an interface sub-object instead. The use goes through the
   first sub-object of the chain whose table it fills. A class below that
   replaces it is therefore honoured. A `final` function and a `final` class
   have no class below them, and a use of either stays direct. NULL when
   the use goes through the primary table or needs no table. */
static const struct struct_field *table_sub_object(const struct type *s,
                                                   const struct item *m)
{
    const struct type *up;
    size_t i;

    if (s == NULL || s->kind != TYPE_CLASS || m == NULL ||
        m->kind != ITEM_FN || !m->pub || m->is_final || s->is_final ||
        types_holds_entry(s, m)) {
        return NULL;
    }
    for (up = s; up != NULL; up = up->base) {
        for (i = 0; i < up->field_count; i++) {
            const struct struct_field *f = &up->fields[i];
            if (f->form == FIELD_IMPL &&
                types_interface_member(s, f->type, &m->name) == m) {
                return f;
            }
        }
    }
    return NULL;
}

/* DESIGN: a class with an open function is never a complete value. It
   appears as a base and behind a pointer, and nowhere else. A local, a
   plain field, an `alloc` and a literal of it are all refused. */
bool sema_refuse_abstract_value(struct checker *c, struct pos pos,
                                const char *what, const struct type *t)
{
    if (!type_has_fields(t) || !t->has_abstract) {
        return false;
    }
    sema_error_at(c, pos, "`%s` is abstract and has no complete value, and %s "
                  "needs one", sema_tn(t), what);
    return true;
}

/* The class that t inherits, or NULL. */
const struct type *sema_inherited(const struct type *t)
{
    return t != NULL && t->kind == TYPE_CLASS ? t->base : NULL;
}

/* DESIGN: a `use` or `inherits` field promotes the names of its type onto
   the struct that holds it. The checker rewrites `v.x` into `v.name.x`
   and `v.f(args)` into `T.f(&v.name, args)`, so nothing below the checker
   knows about promotion. The containing struct's own names win, and a
   name that two fields both provide is an error at the use. */
static const struct struct_field *promoting_field(struct checker *c,
                                                  const struct type *s,
                                                  const struct name *name,
                                                  bool *ambiguous);

/* Whether the type t provides name as a field or as a public function,
   directly or through its own promoting fields. */
/* DESIGN: a `use` field and an interface sub-object promote the public
   members of their type and nothing else. A base promotes what the class
   itself may see, because the chain is one namespace. */
static bool provides(struct checker *c, const struct type *t,
                     const struct name *name, bool pub_only)
{
    const struct struct_field *f;
    const struct item *m;
    bool ambiguous = false;

    if (!type_has_fields(t)) {
        return false;
    }
    f = sema_find_field(t, name);
    if (f != NULL) {
        return !pub_only || t->kind != TYPE_CLASS || f->vis == VIS_PUB ||
               f->form != FIELD_PLAIN;
    }
    m = sema_find_member(t, name);
    if (m != NULL && m->pub) {
        return true;
    }
    return promoting_field(c, t, name, &ambiguous) != NULL;
}

static const struct struct_field *promoting_field(struct checker *c,
                                                  const struct type *s,
                                                  const struct name *name,
                                                  bool *ambiguous)
{
    const struct struct_field *found = NULL;
    size_t i;

    for (i = 0; s != NULL && i < s->field_count; i++) {
        const struct struct_field *f = &s->fields[i];
        if (f->form == FIELD_PLAIN ||
            !provides(c, f->type, name, f->form != FIELD_BASE)) {
            continue;
        }
        if (found != NULL) {
            *ambiguous = true;
            sema_error_at(c, f->pos,
                          "`%.*s` is provided by both `%.*s` and `%.*s`",
                          (int)name->length, name->text,
                          (int)found->name.length, found->name.text,
                          (int)f->name.length, f->name.text);
            return NULL;
        }
        found = f;
    }
    return found;
}

/* Replace the base of e with `base.name`, the promoting field, so the
   next check sees the value that really holds the name. */
static void promote_base(struct checker *c, struct expr *e,
                         const struct struct_field *f)
{
    struct expr *inner = sema_new_node(c, EXPR_FIELD, e->as.field.base->pos);

    inner->as.field.base = e->as.field.base;
    inner->as.field.name = f->name;
    inner->as.field.promoted = true;
    e->as.field.base = inner;
}

/* The class a member or field belongs to, or NULL. */
const struct type *sema_declaring_class(const struct item *m)
{
    return m != NULL && m->owner != NULL && m->owner->symbol != NULL
               ? m->owner->symbol->type
               : NULL;
}

/* The class whose function is being checked, or NULL outside one. */
const struct type *sema_checking_class(const struct checker *c)
{
    const struct item *named = sema_named_function(c);
    const struct item *owner = named != NULL ? named->owner : NULL;
    return owner != NULL && owner->symbol != NULL ? owner->symbol->type : NULL;
}

/* DESIGN: a `tests` or `fixtures` block is part of the module it stands
   in. It sees every private item of that module, the insides of its
   classes included. The rule stands under "Tests and fixtures" of
   docs/anti-language-additions.md. It reaches a class of the module
   being compiled alone. A level of another module is checked as ever,
   and no block reaches into a library. */
static bool from_test_block(const struct checker *c, const struct type *t)
{
    const struct item *named = sema_named_function(c);

    return named != NULL && named->block != BLOCK_NONE && t != NULL &&
           sema_same_name(&t->module, &c->module_name);
}

static bool descends_or_copies(const struct type *a, const struct type *b);

/* DESIGN: the four levels of the object model document. A public member
   is visible everywhere. A protected one reaches the class that declares
   it and every class below it. A private one reaches its own class
   alone. Every level is decided here and costs nothing at run time. */
static bool level_allows(const struct checker *c, enum visibility vis,
                         const struct type *declared_in, const struct type *t)
{
    const struct type *from = sema_checking_class(c);

    if (vis == VIS_PUB) {
        return true;
    }
    if (from_test_block(c, declared_in != NULL ? declared_in : t)) {
        return true;
    }
    if (from == NULL) {
        return false;
    }
    if (declared_in == NULL) {
        declared_in = t;
    }
    /* A generic class below a generic base inherits a copy of it, whose
       members are those of the generic. */
    if (vis == VIS_PROTECTED) {
        return sema_descends_from(from, declared_in) ||
               descends_or_copies(from, declared_in);
    }
    return from == declared_in;
}

/* DESIGN: the class below calls a `construct` with arguments as
   `self.super.construct(x)`, whatever level it was declared at. The
   object model document declares `construct` without a level, which
   alone would keep it to its own class. A call of `construct` needs
   `self.super`, so nothing else reaches it. */
static bool member_visible(const struct checker *c, const struct type *t,
                           const struct item *m)
{
    enum visibility vis = m->vis;

    if (m->kind == ITEM_FN && sema_name_is(&m->name, "construct") &&
        vis != VIS_PUB) {
        vis = VIS_PROTECTED;
    }
    return level_allows(c, vis, sema_declaring_class(m), t);
}

/* Whether t is a class declared `singleton`. */
bool sema_singleton_type(const struct type *t)
{
    size_t i;

    if (t == NULL || t->kind != TYPE_CLASS) {
        return false;
    }
    for (i = 0; i < t->member_count; i++) {
        if (t->members[i]->singleton_get) {
            return true;
        }
    }
    return false;
}

/* Whether the class declares a `construct` that takes arguments. */
static bool constructs_with_arguments(const struct type *t)
{
    size_t i;

    for (i = 0; i < t->member_count; i++) {
        const struct item *m = t->members[i];
        if (m->kind == ITEM_FN && sema_name_is(&m->name, "construct") &&
            m->symbol != NULL && m->symbol->type->kind == TYPE_FN &&
            m->symbol->type->param_count > 1) {
            return true;
        }
    }
    return false;
}

/* DESIGN: an inline class field without a default takes `T { }` when a
   literal of T may leave every field out. Each field of its chain then
   has a default, written or taken this way, or T has none. `construct`
   runs on it as on any literal, so a `construct` with arguments rules it
   out. Any other such field is required in a literal, like any field
   without a default. */
static bool literal_complete(const struct type *t)
{
    const struct type *up;
    size_t i;

    if (t == NULL || t->kind != TYPE_CLASS || t->has_abstract ||
        sema_singleton_type(t) || constructs_with_arguments(t)) {
        return false;
    }
    for (up = t; up != NULL && up->kind == TYPE_CLASS; up = up->base) {
        for (i = 0; i < up->field_count; i++) {
            const struct struct_field *f = &up->fields[i];
            if (f->form == FIELD_BASE || f->form == FIELD_TABLE ||
                f->form == FIELD_IMPL || type_field_is_unit_break(f)) {
                continue;
            }
            if (f->value == NULL && f->constant == NULL &&
                !sema_field_takes_literal(f)) {
                return false;
            }
        }
    }
    return true;
}

struct stmt *sema_arm_fallthrough(const struct stmt *body)
{
    const struct block *b;

    if (body->kind != STMT_BLOCK) {
        return NULL;
    }
    b = body->as.block;
    return b->count > 0 && b->stmts[b->count - 1]->kind == STMT_FALLTHROUGH
               ? b->stmts[b->count - 1]
               : NULL;
}

/* DESIGN: a Mutex field without a default starts free, as `Mutex.new()`
   gives it, so a literal and `construct` may leave it out. So does the
   hidden lock of a synchronized class. */
bool sema_field_takes_literal(const struct struct_field *f)
{
    return f->value == NULL && f->constant == NULL &&
           (f->form == FIELD_PLAIN || f->form == FIELD_USE) &&
           (literal_complete(f->type) || types_is_mutex(f->type) ||
            types_is_object_lock(f->type));
}

/* DESIGN: every field of a struct is readable and writable everywhere,
   because a struct is the bytes C declares. A field of a class carries
   the level its declaration gave it. The base and the table pointer are
   the checker's own fields and belong to the class that holds them. */
static bool field_visible(const struct checker *c, const struct type *s,
                          const struct struct_field *f)
{
    /* DESIGN: the base, the table pointer, a `use` field and an
       interface sub-object carry no level of their own. They are the
       shape of the class rather than data it hides. The example of the
       object model document names a `use` field in a literal from
       outside the class. What they promote keeps its own level. */
    if (s->kind != TYPE_CLASS || f->form != FIELD_PLAIN) {
        return true;
    }
    /* DESIGN: a singleton is the program's one place for a value, so
       every field of it is readable wherever the class is. What a
       marker decides there is writing, not reading. */
    if (s->members != NULL && sema_singleton_type(s)) {
        return true;
    }
    return level_allows(c, f->vis, f->home != NULL ? f->home : s, s);
}

/* DESIGN: `v.f(args)` resolves in the namespace of v's type first, and
   then in the module that declares the type. A function of the body wins
   over a free function of the same name. */
/* The module-level `operator fn` of the module of s that shares the name
   name with another and takes s first, declared as `name:S`, or NULL. */
static struct symbol *shared_operator(const struct checker *c,
                                      const struct type *s,
                                      const struct name *name)
{
    const struct type *g = s->generic != NULL ? s->generic : s;
    const struct interface *lib;
    char text[256];
    struct name shared;

    if ((s->kind != TYPE_STRUCT && s->kind != TYPE_CLASS &&
         s->kind != TYPE_VARIANT) ||
        name->length + g->name.length + 2 > sizeof text) {
        return NULL;
    }
    memcpy(text, name->text, name->length);
    text[name->length] = ':';
    memcpy(text + name->length + 1, g->name.text, g->name.length);
    shared.text = text;
    shared.length = name->length + 1 + g->name.length;
    if (sema_same_name(&s->module, &c->module_name)) {
        return sema_scope_find_local(&c->module_scope, &shared);
    }
    lib = sema_find_library(c, &s->module);
    return lib != NULL ? sema_library_item(c, lib, &shared) : NULL;
}

struct symbol *sema_method_symbol(const struct checker *c,
                                  const struct type *s,
                                  const struct name *name)
{
    const struct interface *lib;
    struct item *m = reached_member(s, name);
    struct symbol *shared;

    if (m != NULL && m->kind == ITEM_FN && m->symbol != NULL &&
        member_visible(c, s, m)) {
        return m->symbol;
    }
    shared = shared_operator(c, s, name);
    if (shared != NULL) {
        return shared;
    }
    /* An item of another library that a direct import names is no
       function of the module that declares the type. */
    if (sema_same_name(&s->module, &c->module_name)) {
        struct symbol *sym = sema_scope_find_local(&c->module_scope, name);
        return sema_direct_item(sym) ? NULL : sym;
    }
    lib = sema_find_library(c, &s->module);
    return lib != NULL ? sema_library_item(c, lib, name) : NULL;
}

/* The import that the base of module.name refers to, or NULL when the
   base is not the name of a module. */
static const struct symbol *qualifier(const struct checker *c,
                                      const struct expr *field)
{
    const struct expr *base = field->as.field.base;
    const struct symbol *sym;

    if (base->kind != EXPR_NAME) {
        return NULL;
    }
    sym = sema_lookup(c, &base->as.name);
    return sym != NULL && sym->kind == SYMBOL_MODULE ? sym : NULL;
}

/* Rewrite module.name into a plain name of the imported item. A callee
   may name a variadic extern fn, which has no function pointer type. */
static struct type *check_qualified(struct checker *c, struct expr *e,
                                    const struct symbol *module, bool callee)
{
    struct name base = e->as.field.base->as.name;
    struct name name = e->as.field.name;
    struct symbol *item = sema_library_item(c, module->home, &name);

    if (item == NULL) {
        sema_error_at(c, e->pos, "`%.*s` has no public item `%.*s`",
                      (int)base.length, base.text, (int)name.length, name.text);
        return sema_builtin(c, TYPE_ERROR);
    }
    if (item->kind == SYMBOL_STRUCT) {
        sema_error_at(c, e->pos, "`%.*s.%.*s` is a type, not a value",
                      (int)base.length, base.text, (int)name.length, name.text);
        return sema_builtin(c, TYPE_ERROR);
    }
    if (!callee && item->kind == SYMBOL_EXTERN_FN && item->variadic) {
        sema_error_at(c, e->pos, "a variadic function has no function pointer "
                      "type");
        return sema_builtin(c, TYPE_ERROR);
    }
    e->kind = EXPR_NAME;
    e->as.name = name;
    e->symbol = item;
    return item->type;
}

/* DESIGN: an atomic field is read and written by calls alone, so that
   every access is one sequentially consistent operation. The checker
   rewrites `p.add(1)` on an atomic place into one node, which lowering
   turns into a call of the runtime. */
static const struct {
    const char *name;
    enum atomic_op op;
    size_t args;
    bool gives_value;
    bool gives_bool;
} atomic_ops[] = {
    {"load", ATOMIC_LOAD, 0, true, false},
    {"store", ATOMIC_STORE, 1, false, false},
    {"swap", ATOMIC_SWAP, 1, true, false},
    {"add", ATOMIC_ADD, 1, true, false},
    {"sub", ATOMIC_SUB, 1, true, false},
    {"and", ATOMIC_AND, 1, true, false},
    {"or", ATOMIC_OR, 1, true, false},
    {"compare_swap", ATOMIC_CAS, 2, false, true}
};

/* The type of the atomic field or local that e denotes, or NULL. */
static struct type *atomic_place(const struct expr *e)
{
    const struct struct_field *f;
    struct type *s;

    if (e->kind == EXPR_NAME) {
        return e->symbol != NULL && e->symbol->atomic ? e->symbol->type : NULL;
    }
    if (e->kind != EXPR_FIELD) {
        return NULL;
    }
    if (e->symbol != NULL && e->symbol->item != NULL &&
        e->symbol->item->is_static && e->symbol->item->atomic) {
        return e->symbol->type;
    }
    s = e->as.field.base->type != NULL ? sema_struct_of(e->as.field.base->type)
                                       : NULL;
    if (s == NULL) {
        return NULL;
    }
    f = sema_find_field(s, &e->as.field.name);
    return f != NULL && f->atomic ? f->type : NULL;
}

/* Whether t is one word an atomic operation of the runtime moves: an
   integer, a `bool` or a pointer. */
static bool swaps_as_word(const struct type *t)
{
    return type_is_integer(t) || t->kind == TYPE_BOOL ||
           t->kind == TYPE_POINTER;
}

/* DESIGN: `compare_swap` also takes a plain field of one word that
   `unchecked(unguarded-field)` marks in a concurrent class, after the
   field's type or in the class header. That is the lock-free structure
   of "Concurrent classes", whose fields the checker cannot follow. The
   call is the node of an atomic field, and it counts as a write, so the
   clause covers the field it marks. A field of a class that declares
   its own `compare_swap`, or of a pointer to one, keeps the call of that
   function. Any other field is refused, and the message names both
   fixes. */
static bool unchecked_swap(struct checker *c, struct expr *e,
                           struct type **out)
{
    struct expr *callee = e->as.call.callee;
    struct expr *place = callee->as.field.base;
    const struct type *s;
    const struct struct_field *f = NULL;
    const struct type *home;
    size_t i;

    if (place->kind != EXPR_FIELD || place->type == NULL) {
        return false;
    }
    s = sema_struct_of(place->type);
    if (s != NULL && sema_method_symbol(c, s, &callee->as.field.name) != NULL) {
        return false;
    }
    s = sema_struct_of(place->as.field.base->type);
    for (; s != NULL && f == NULL; s = s->kind == TYPE_CLASS ? s->base : NULL) {
        f = sema_find_field(s, &place->as.field.name);
    }
    if (f == NULL || f->form != FIELD_PLAIN) {
        return false;
    }
    home = f->home;
    if (home == NULL || home->safety != SAFETY_CONCURRENT ||
        !(f->unchecked || home->unchecked_fields)) {
        sema_error_at(c, callee->pos, "`compare_swap` takes an atomic field, "
                      "and `%.*s` is a plain one: mark it `atomic`, or "
                      "`unchecked(unguarded-field, \"reason\")` in a "
                      "concurrent class",
                      (int)f->name.length, f->name.text);
        *out = sema_builtin(c, TYPE_ERROR);
        return true;
    }
    if (f->bits != 0 || !swaps_as_word(f->type)) {
        sema_error_at(c, callee->pos, "`compare_swap` on a field that "
                      "`unchecked` marks takes one word, an integer, a "
                      "`bool` or a pointer, found `%s`", sema_tn(f->type));
        *out = sema_builtin(c, TYPE_ERROR);
        return true;
    }
    if (e->as.call.arg_count != 2) {
        sema_error_at(c, e->pos, "`compare_swap` takes 2 arguments");
        *out = sema_builtin(c, TYPE_ERROR);
        return true;
    }
    sema_note_field_write(c, place);
    e->as.atomic.a = e->as.call.args[0];
    e->as.atomic.b = e->as.call.args[1];
    e->as.atomic.op = ATOMIC_CAS;
    e->as.atomic.place = place;
    e->kind = EXPR_ATOMIC;
    for (i = 0; i < 2; i++) {
        struct expr *arg = i == 0 ? e->as.atomic.a : e->as.atomic.b;
        if (!sema_require(c, arg, sema_check_expr(c, arg, f->type),
                          f->type)) {
            *out = sema_builtin(c, TYPE_ERROR);
            return true;
        }
    }
    *out = sema_builtin(c, TYPE_BOOL);
    return true;
}

/* Rewrite a call on an atomic place. Returns false when the callee is no
   such call, and reports nothing then. */
static bool atomic_call(struct checker *c, struct expr *e, struct type **out)
{
    struct expr *callee = e->as.call.callee;
    struct expr *place;
    struct type *t;
    size_t i;

    bool was;

    if (callee->kind != EXPR_FIELD) {
        return false;
    }
    place = callee->as.field.base;
    if (place->kind == EXPR_NAME) {
        const struct symbol *sym = sema_lookup(c, &place->as.name);
        if (sym == NULL || !sym->atomic) {
            return false;
        }
    } else if (place->kind != EXPR_FIELD) {
        return false;
    }
    /* DESIGN: the base is checked quietly, because this is a probe. A
       call on anything else reaches the branches below, which report
       what is wrong with it. A probe inside another keeps its flag. */
    was = c->atomic_place;
    c->atomic_place = true;
    c->quiet++;
    /* The place is storage, so an f16 there stays an f16. */
    t = sema_check_storage(c, place);
    c->quiet--;
    c->atomic_place = was;
    if (t == NULL || sema_is_error(t)) {
        return false;
    }
    t = atomic_place(place);
    if (t == NULL) {
        return sema_name_is(&callee->as.field.name, "compare_swap") &&
               unchecked_swap(c, e, out);
    }
    for (i = 0; i < sizeof atomic_ops / sizeof atomic_ops[0]; i++) {
        if (!sema_name_is(&callee->as.field.name, atomic_ops[i].name)) {
            continue;
        }
        if (e->as.call.arg_count != atomic_ops[i].args) {
            sema_error_at(c, e->pos, "`%s` takes %zu argument%s",
                          atomic_ops[i].name,
                          atomic_ops[i].args,
                          atomic_ops[i].args == 1 ? "" : "s");
            *out = sema_builtin(c, TYPE_ERROR);
            return true;
        }
        /* An f16 has its bits exchanged and compared, and no arithmetic. */
        if ((atomic_ops[i].op == ATOMIC_ADD || atomic_ops[i].op == ATOMIC_SUB ||
             atomic_ops[i].op == ATOMIC_AND || atomic_ops[i].op == ATOMIC_OR) &&
            sema_refuses_half(c, e->pos, t)) {
            *out = sema_builtin(c, TYPE_ERROR);
            return true;
        }
        e->as.atomic.a = atomic_ops[i].args > 0 ? e->as.call.args[0] : NULL;
        e->as.atomic.b = atomic_ops[i].args > 1 ? e->as.call.args[1] : NULL;
        e->as.atomic.op = atomic_ops[i].op;
        e->as.atomic.place = place;
        e->kind = EXPR_ATOMIC;
        if (e->as.atomic.a != NULL &&
            !sema_require(c, e->as.atomic.a,
                          sema_check_expr(c, e->as.atomic.a, t), t)) {
            *out = sema_builtin(c, TYPE_ERROR);
            return true;
        }
        if (e->as.atomic.b != NULL &&
            !sema_require(c, e->as.atomic.b,
                          sema_check_expr(c, e->as.atomic.b, t), t)) {
            *out = sema_builtin(c, TYPE_ERROR);
            return true;
        }
        *out = atomic_ops[i].gives_bool  ? sema_builtin(c, TYPE_BOOL)
               : atomic_ops[i].gives_value ? t
                                           : sema_builtin(c, TYPE_VOID);
        return true;
    }
    sema_error_at(c, callee->pos, "an atomic %s has no `%.*s`",
                  place->kind == EXPR_NAME ? "local" : "field",
                  (int)callee->as.field.name.length,
                  callee->as.field.name.text);
    *out = sema_builtin(c, TYPE_ERROR);
    return true;
}

/* Whether any class of the program places a sub-object of t inside
   itself. A pointer to such a class points into the middle of an
   object. The receiver of a call through it needs the offset that only
   the thunk of the concrete class knows. */
bool sema_implemented_in(const struct type *t, const struct type *iface)
{
    size_t i;

    for (; t != NULL; t = t->kind == TYPE_CLASS ? t->base : NULL) {
        for (i = 0; i < t->field_count; i++) {
            const struct struct_field *f = &t->fields[i];
            if (f->form == FIELD_IMPL && sema_descends_from(f->type, iface)) {
                return true;
            }
        }
    }
    return false;
}

/* DESIGN: an interface sub-object, as `c.ser`, is a value inside an
   object of a class that fills its table. A call on it goes through that
   table, as a call through a pointer to the interface does. The table
   holds the thunk into the body of the class. */
static bool names_sub_object(const struct expr *e)
{
    const struct type *s;
    const struct struct_field *f;

    if (e->kind != EXPR_FIELD) {
        return false;
    }
    s = sema_struct_of(e->as.field.base->type);
    f = s != NULL ? sema_find_field(s, &e->as.field.name) : NULL;
    return f != NULL && f->form == FIELD_IMPL;
}

/* Rewrite v.f(args) into f(receiver, args). The struct T of v has no
   field f, and the module declares a function f whose first parameter is
   T or *T. Returns false after reporting an error. */
/* Whether a is b, a class below b, or a copy of the generic b or of a
   class below it. A function of b then takes a as `self`. So does a
   generic function whose first parameter is a copy of the generic of a,
   as `operator fn add<T>(a: Money<T>, b: Money<T>)` is. Its call infers
   the arguments. */
static bool descends_or_copies(const struct type *a, const struct type *b)
{
    for (; a != NULL; a = a->kind == TYPE_CLASS ? a->base : NULL) {
        if (a == b || (a->generic != NULL && a->generic == b) ||
            (b != NULL && a->generic != NULL && a->generic == b->generic &&
             sema_has_params(b))) {
            return true;
        }
    }
    return false;
}

static bool method_call(struct checker *c, struct expr *call)
{
    struct expr *field = call->as.call.callee;
    struct expr *receiver = field->as.field.base;
    struct type *t = receiver->type;
    struct type *s = sema_struct_of(t);
    struct symbol *f;
    const struct item *member = s != NULL
                                    ? reached_member(s, &field->as.field.name)
                                    : NULL;
    struct type *first;
    struct expr **args;
    struct expr *callee;

    if (ambiguous_member(c, field->pos, s, &field->as.field.name,
                         "the call")) {
        return false;
    }
    f = sema_method_symbol(c, s, &field->as.field.name);
    if (f == NULL || (f->kind != SYMBOL_FN && f->kind != SYMBOL_EXTERN_FN) ||
        sema_is_error(f->type) || f->type->param_count == 0 ||
        !descends_or_copies(s, sema_struct_of(f->type->params[0]))) {
        const struct item *hidden = reached_member(s, &field->as.field.name);
        bool ambiguous = false;
        const struct struct_field *through =
            promoting_field(c, s, &field->as.field.name, &ambiguous);
        if (ambiguous) {
            return false;
        }
        if (through != NULL) {
            promote_base(c, field, through);
            field->as.field.base->type =
                sema_check_expr(c, field->as.field.base, NULL);
            return method_call(c, call);
        }
        if (hidden != NULL && !member_visible(c, s, hidden)) {
            const struct type *owner = sema_declaring_class(hidden);
            sema_error_at(c, field->pos, "`%.*s` is %s `%s`",
                          (int)field->as.field.name.length,
                          field->as.field.name.text,
                          hidden->vis == VIS_PROTECTED ? "protected in"
                                                       : "private to",
                          sema_tn(owner != NULL ? owner : s));
            return false;
        }
        sema_error_at(c, field->pos, "`%s` has no function `%.*s`", sema_tn(s),
                      (int)field->as.field.name.length,
                      field->as.field.name.text);
        return false;
    }
    first = sema_member_type_in(c, f->type->params[0],
                                f->item != NULL ? f->item->owner : NULL, s);
    /* A generic function takes the receiver as it stands, and its call
       infers the arguments from it. */
    if (sema_has_params(first) && f->item != NULL &&
        f->item->type_param_count > 0 && f->item->owner == NULL) {
        first = first->kind == TYPE_POINTER
                    ? types_pointer(c->types, type_has_fields(t) ? t : s)
                    : s;
    }
    if (first->kind == TYPE_POINTER && type_has_fields(t)) {
        struct expr *address = sema_new_node(c, EXPR_UNARY, receiver->pos);
        if (!sema_is_place(receiver)) {
            sema_error_at(c, receiver->pos, "calling `%.*s` needs a place",
                          (int)f->name.length, f->name.text);
            return false;
        }
        sema_mark_address_taken(c, receiver);
        address->as.unary.op = TOKEN_AMP;
        address->as.unary.operand = receiver;
        address->type = first;
        receiver = address;
    } else if (type_has_fields(first) && t->kind == TYPE_POINTER) {
        struct expr *deref = sema_new_node(c, EXPR_UNARY, receiver->pos);
        deref->as.unary.op = TOKEN_STAR;
        deref->as.unary.operand = receiver;
        deref->type = first;
        receiver = deref;
    }
    /* `destruct` is the one function a program never calls itself. The
       compiler chains it, so `delete` and `destroy` are the spellings. */
    if (member != NULL && member->runtime == NULL &&
        sema_name_is(&field->as.field.name, "destruct")) {
        sema_error_at(c, field->pos, "`destruct` is never called directly, use "
                      "`delete` or `destroy`");
        return false;
    }
    /* `construct` runs after a literal and after `alloc`. A base with
       arguments is reached through `self.super`, and nowhere else. The
       receiver as written is checked, since the call takes its address
       above. */
    if (member != NULL && sema_name_is(&field->as.field.name, "construct") &&
        (field->as.field.base->kind != EXPR_FIELD ||
         !sema_name_is(&field->as.field.base->as.field.name, "super"))) {
        sema_error_at(c, field->pos,
                      "`construct` runs after a literal and after "
                      "`alloc`, and is not called directly");
        return false;
    }
    /* DESIGN: the `construct` below calls the one of its base at the top
       of its body, as the first statement. The compiler cannot know the
       arguments. The base part is then complete before the body below
       reads it, and a base that fails stops the body there. A call
       anywhere else, or in any other function, is refused. */
    if (member != NULL && sema_name_is(&field->as.field.name, "construct") &&
        (call != c->top_call || c->function == NULL ||
         !sema_name_is(&c->function->name, "construct"))) {
        sema_error_at(c, field->pos, "`self.super.construct` is called at the "
                      "top of the body of `construct`");
        return false;
    }
    if (t->kind == TYPE_POINTER || names_sub_object(field->as.field.base)) {
        const struct struct_field *sub = table_sub_object(s, member);
        if (sub != NULL) {
            promote_base(c, field, sub);
            field->as.field.base->type =
                sema_check_expr(c, field->as.field.base, NULL);
            return method_call(c, call);
        }
    }
    /* DESIGN: a call on a class reached through a pointer goes through
       the table. The object may be of a class below the static type. A
       `final` function and a `final` class have no class below them, so
       both call directly. A value has its concrete type, and a private
       function has no entry, so both call directly as well.

       DESIGN: a call is never made direct because no class replaces the
       function. The checker reads one module, and a class of a module
       that imports this one replaces what this one cannot see. Such a
       direct call is a miscompile, so `final` is the only way to one. */
    if ((t->kind == TYPE_POINTER || names_sub_object(field->as.field.base)) &&
        s->kind == TYPE_CLASS && member != NULL && member->pub &&
        !member->is_final && !s->is_final && types_holds_entry(s, member)) {
        call->as.call.dispatch = s;
        call->as.call.entry = field->as.field.name;
    }
    if (call->as.call.dispatch == NULL &&
        refuse_abstract_call(c, field->pos, s, member)) {
        return false;
    }
    callee = sema_new_node(c, EXPR_NAME, field->pos);
    callee->as.name = f->name;
    callee->symbol = f;
    callee->type = f->type;
    args = types_alloc_array(c->arena, call->as.call.arg_count + 1,
                             sizeof *args);
    args[0] = receiver;
    /* A call without arguments holds no array to copy. */
    if (call->as.call.arg_count > 0) {
        memcpy(args + 1, call->as.call.args,
               call->as.call.arg_count * sizeof *args);
    }
    call->as.call.callee = callee;
    call->as.call.args = args;
    call->as.call.arg_count++;
    return true;
}

static bool variadic_ok(const struct type *t)
{
    switch (t->kind) {
    case TYPE_I32:
    case TYPE_U32:
    case TYPE_I64:
    case TYPE_U64:
    case TYPE_F64:
    case TYPE_POINTER:
        return true;
    default:
        return false;
    }
}

static struct type *check_type_member(struct checker *c, struct expr *e,
                                      struct type *t);

/* DESIGN: a function that can fail returns a pointer to `anti.lang`'s
   `Error` or to a class below it. The compiler knows the convention by
   the module path and the class name, and nothing else of the standard
   library reaches the checker. */
/* DESIGN: `p catch fatal` and `p catch e { }` on a `?*T` follow the
   error forms, and the error is `anti.lang.NoneDereference`. The class is
   an ordinary imported one, so the module that writes the form imports
   `anti.lang` as it does for every other error it names. */
struct symbol *sema_null_pointer_maker(struct checker *c, struct pos pos)
{
    static const struct name module = {LANG_MODULE, sizeof LANG_MODULE - 1};
    static const struct name class_name = {
        LANG_NONE_DEREFERENCE, sizeof LANG_NONE_DEREFERENCE - 1};
    static const struct name maker = {"new", 3};
    struct symbol *sym = sema_std_item(c, &module, &class_name, false);
    const struct item *m = sym != NULL ? sema_find_member(sym->type,
                                                          &maker) : NULL;

    if (m == NULL || m->symbol == NULL || m->symbol->type == NULL) {
        sema_error_at(c, pos, "`catch` on a `?*T` gives an `" LANG_MODULE "."
                      LANG_NONE_DEREFERENCE "`, so the module imports `"
                      LANG_MODULE "`");
        return NULL;
    }
    return m->symbol;
}

/* DESIGN: `fail "text";` is `fail Error.new(0, "text");`. Code zero means
   "no code", and `fatal` turns it into exit status 1. The statement keeps
   the maker rather than a rewritten call, as the pointer guard does.
   Lowering then emits one direct call and the checker resolves one name. */
struct symbol *sema_error_maker(struct checker *c, struct pos pos)
{
    static const struct name maker = {"new", 3};
    struct type *t = sema_error_class(c, pos);
    const struct item *m = t != NULL ? sema_find_member(t, &maker) : NULL;

    if (m == NULL || m->symbol == NULL || m->symbol->type == NULL ||
        m->symbol->type->param_count != 2) {
        sema_error_at(c, pos,
                      "`fail \"text\"` calls `" LANG_MODULE "." LANG_ERROR
                      ".new`, which takes a code and a message");
        return NULL;
    }
    return m->symbol;
}

/* `anti.lang.StackTrace.capture`, which a `fail` calls with the count of
   frames to skip, or NULL after an error at pos. */
static struct symbol *trace_capture(struct checker *c, struct pos pos)
{
    static const struct name module = {LANG_MODULE, sizeof LANG_MODULE - 1};
    static const struct name class_name = {LANG_STACK_TRACE,
                                           sizeof LANG_STACK_TRACE - 1};
    static const struct name capture = {LANG_TRACE_CAPTURE,
                                        sizeof LANG_TRACE_CAPTURE - 1};
    struct symbol *sym = sema_std_item(c, &module, &class_name, true);
    const struct item *m =
        sym != NULL ? sema_find_member(sym->type, &capture) : NULL;

    if (m == NULL || m->symbol == NULL || m->symbol->type == NULL ||
        m->symbol->type->kind != TYPE_FN ||
        m->symbol->type->param_count != 1) {
        sema_error_at(c, pos,
                      "`fail` captures the frames with `" LANG_MODULE "."
                      LANG_STACK_TRACE "." LANG_TRACE_CAPTURE
                      "`, which takes the count of frames to skip");
        return NULL;
    }
    return m->symbol;
}

/* Resolve what a `fail` writes into the error it gives: the position in
   `at` and the frames in `frames` of `anti.lang.Error`. */
void sema_resolve_origin(struct checker *c, struct stmt *s)
{
    static const struct name at = {LANG_ERROR_AT, sizeof LANG_ERROR_AT - 1};
    static const struct name frames = {LANG_ERROR_FRAMES,
                                       sizeof LANG_ERROR_FRAMES - 1};
    struct type *error = sema_error_class(c, s->pos);
    const struct struct_field *where =
        error != NULL ? sema_find_field(error, &at) : NULL;

    if (error == NULL) {
        return;
    }
    if (where == NULL || sema_find_field(error, &frames) == NULL ||
        where->type != sema_location_type(c, s->pos)) {
        sema_error_at(c, s->pos, "`fail` writes `" LANG_ERROR_AT "` and `"
                      LANG_ERROR_FRAMES "` of `" LANG_MODULE "." LANG_ERROR
                      "`, which this `" LANG_MODULE "` lacks");
        return;
    }
    s->as.fail.error = error;
    s->as.fail.capture = trace_capture(c, s->pos);
}

/* The `*Error` a handler binds, from the `?*Error` a failing function
   returns. */
struct type *sema_caught_error(struct checker *c, struct type *result)
{
    return result != NULL && type_is_nullable(result)
               ? types_pointer(c->types, result->element)
               : result;
}

/* Declare the name the handler h binds, in the scope entered last, as a
   read-only local of type t that holds the caught error. A handler
   without a name declares nothing. */
void sema_declare_caught(struct checker *c, struct handler *h,
                         struct type *t)
{
    if (h->name.length == 0) {
        return;
    }
    sema_warn_catch_shadow(c, &h->name, h->pos);
    h->symbol = sema_declare(c, SYMBOL_LOCAL, &h->name, h->pos,
                             "`%.*s` is already declared in this block");
    if (h->symbol != NULL) {
        h->symbol->type = t;
        h->symbol->read_only = true;
        h->symbol->caught = true;
        h->symbol->caught_loops = c->loop_depth;
    }
}

/* DESIGN: a function fails when it is written `may fail`, and never
   because of its result type. A function that returns `*Error` or
   `?*Error` without the marking gives an error as a value, and its call
   needs no handler. The marking travels in the function type, so a call
   through a value of `fn(A) -> R may fail` is handled as a direct call
   is, and a `may fail` function converts to that type alone. */
static bool is_failing(const struct type *fn)
{
    return fn != NULL && fn->kind == TYPE_FN && fn->may_fail;
}

/* Whether the function whose body is checked may fail. */
bool sema_in_failing_function(const struct checker *c)
{
    return c->function != NULL && c->function->may_fail;
}

/* DESIGN: the error a `catch` binds belongs to the handler, which ends
   it. A handler that wants to keep it copies it with `dup`, so no error
   outlives the block that owns it. */
void sema_refuse_escaping_error(struct checker *c, const struct expr *e)
{
    if (e != NULL && e->kind == EXPR_NAME && e->symbol != NULL &&
        e->symbol->caught) {
        sema_error_at(c, e->pos, "`%.*s` outlives its `catch`, use `dup`",
                      (int)e->as.name.length, e->as.name.text);
    }
}

/* The name a message gives the place e: a variable, the last field of
   a path or the base of an element. Empty for any other expression. */
struct name sema_place_name(const struct expr *e)
{
    static const struct name none_name = {"", 0};

    while (e != NULL) {
        switch (e->kind) {
        case EXPR_NAME:
            return e->as.name;
        case EXPR_FIELD:
            return e->as.field.name;
        case EXPR_INDEX:
            e = e->as.index.base;
            break;
        case EXPR_UNARY:
            e = e->as.unary.operand;
            break;
        default:
            return none_name;
        }
    }
    return none_name;
}

/* The name of the function sym without the class before it. */
static struct name bare_name(const struct symbol *sym)
{
    struct name n = sym->name;
    size_t i;

    for (i = n.length; i > 0; i--) {
        if (n.text[i - 1] == '.') {
            n.text += i;
            n.length -= i;
            break;
        }
    }
    return n;
}

/* DESIGN: a local passed to an `own` parameter moves, and so does an
   `own` parameter that `=` or `let` stores. The local is not named after
   the move, so the checker refuses a mention after it in the order of
   the text. That order is the order of execution unless a loop repeats
   the move, a `defer` or an `undo` that names the local runs at an exit
   after it, or a closure that captures it runs later. All three are
   refused, and so is a move inside a closure, which may run more than
   once. A parameter that is not `own` belongs to the caller, so one
   whose value owns memory does not move. Returns whether e moved. */
bool sema_move_local(struct checker *c, struct expr *e,
                     const struct name *into, const struct name *by)
{
    struct symbol *sym = e->symbol;
    const struct name *target = by->length > 0 ? by : into;

    if (c->quiet > 0) {
        return true;
    }
    if (sym->frame != c->function) {
        sema_error_at(c, e->pos, "`%.*s` is captured, and a closure does not "
                      "move what it captures", (int)sym->name.length,
                      sym->name.text);
        return false;
    }
    if (sym->captured) {
        sema_error_at(c, e->pos, "`%.*s` is captured by a closure, which may "
                      "read it after it moves into `%.*s`",
                      (int)sym->name.length, sym->name.text,
                      (int)target->length, target->text);
        return false;
    }
    if (c->loop_depth > sym->loops) {
        sema_error_at(c, e->pos, "`%.*s` moves into `%.*s` inside a loop, "
                      "which would move it again", (int)sym->name.length,
                      sym->name.text, (int)target->length, target->text);
        return false;
    }
    if (sym->deferred) {
        sema_error_at(c, e->pos, "`%.*s` moves into `%.*s` while a `defer` or "
                      "an `undo` names it", (int)sym->name.length,
                      sym->name.text, (int)target->length, target->text);
        return false;
    }
    if (sym->kind == SYMBOL_PARAM && !sym->own_param && e->type != NULL &&
        sema_type_owns(e->type)) {
        sema_error_at(c, e->pos, "`%.*s` belongs to the caller and does not "
                      "move. Mark it `own` or pass `dup(%.*s)`",
                      (int)sym->name.length, sym->name.text,
                      (int)sym->name.length, sym->name.text);
        return false;
    }
    e->moves = true;
    sym->moved = true;
    sym->moved_to = *into;
    sym->moved_by = *by;
    return true;
}

/* DESIGN: the error a handler binds moves into an `own` parameter, and
   the handler then holds it no longer, as after `return e`. Lowering
   writes `none` into the handler's copy at the move, so the delete on
   every exit after it passes over the error. The error is not named
   after the move, so the checker refuses a mention after it in the order
   of the text. That order is the order of execution unless a loop inside
   the handler repeats the move, or a `defer` or an `undo` that the
   handler registered names the error at an exit after it. Both are
   refused.

   DESIGN: any other local moves by `sema_move_local`. A literal or a
   call result passed there has no other owner and needs nothing. Any
   other place holds a value that stays where it is, so one that owns
   memory is refused, as `=` refuses it. receiver is the object of a
   call of a function of a class, or NULL. */
static void note_move(struct checker *c, const struct symbol *callee,
                      size_t index, const struct expr *receiver,
                      struct expr *arg)
{
    struct symbol *moved = arg->kind == EXPR_NAME ? arg->symbol : NULL;
    struct name into;
    struct name by;

    if (callee == NULL || callee->owned == NULL ||
        index >= callee->owned_count || !callee->owned[index] ||
        c->quiet > 0 || arg->type == NULL || sema_is_error(arg->type)) {
        return;
    }
    if (moved != NULL && moved->caught) {
        if (c->loop_depth > moved->caught_loops) {
            sema_error_at(c, arg->pos,
                          "`%.*s` moves into `%.*s` inside a loop of its "
                          "handler, which would move it again",
                          (int)arg->as.name.length, arg->as.name.text,
                          (int)callee->name.length, callee->name.text);
            return;
        }
        if (moved->deferred) {
            sema_error_at(c, arg->pos,
                          "`%.*s` moves into `%.*s` while a `defer` or an "
                          "`undo` of its handler names it",
                          (int)arg->as.name.length, arg->as.name.text,
                          (int)callee->name.length, callee->name.text);
            return;
        }
        arg->moves = true;
        moved->moved_into = callee;
        return;
    }
    /* A `keep own` parameter takes a function by the rules of a
       snapshot, which the conversion to it checks. */
    if (callee->type != NULL && callee->type->kind == TYPE_FN &&
        index < callee->type->param_count &&
        callee->type->params[index]->kind == TYPE_FN) {
        return;
    }
    into = sema_place_name(receiver);
    by = bare_name(callee);
    if (moved != NULL &&
        (moved->kind == SYMBOL_LOCAL || moved->kind == SYMBOL_PARAM)) {
        if (receiver == NULL) {
            into = by;
            by.length = 0;
        }
        sema_move_local(c, arg, &into, &by);
        return;
    }
    if (sema_type_owns(arg->type) && sema_reads_existing(arg)) {
        sema_error_at(c, arg->pos, "`%s` %s, and a value that stays where it "
                      "is does not move into `%.*s`, use `dup`",
                      sema_tn(arg->type), sema_owns_phrase(arg->type),
                      (int)by.length, by.text);
    }
}

/* The value a handled call gives: what the out parameter points at, or
   nothing when the function writes no result. */
static struct type *handled_result(struct checker *c, const struct expr *e,
                                   const struct type *fn)
{
    if (e->as.call.builds != NULL) {
        return e->as.call.on_heap
                   ? types_pointer(c->types, (struct type *)e->as.call.builds)
                   : (struct type *)e->as.call.builds;
    }
    return e->as.call.out != NULL
               ? fn->params[fn->param_count - 1]->element
               : sema_builtin(c, TYPE_VOID);
}

/* Check the handler of a call that can fail. Every such call carries
   one, because an error that nothing reads is a bug the checker can
   see. */
static struct type *check_handled(struct checker *c, struct expr *e,
                                  struct type *fn, struct type *expected)
{
    struct handler *h = &e->as.call.handler;
    struct type *result = handled_result(c, e, fn);
    const struct symbol *callee = function_symbol(e->as.call.callee);
    struct scope scope;
    struct type *outer_yield = c->yields;
    int outer_depth = c->handler_depth;

    /* DESIGN: `yield` gives the value that takes the place of the
       result, so it is checked against the type the binding holds. A
       `let n: ?*T = alloc T(args) catch e { yield none; }` names that
       type, and the handler yields `none` against it. */
    if (expected != NULL && type_is_nullable(expected) &&
        result->kind == TYPE_POINTER && result->element == expected->element) {
        result = expected;
    }

    /* Inside a `try` block every failing call reaches the one handler,
       so it needs none of its own. */
    if (h->kind == HANDLE_NONE && c->try_block != NULL) {
        h->kind = HANDLE_ENCLOSING;
        if (c->error_type == NULL) {
            c->error_type = fn->result;
        }
    }
    switch (h->kind) {
    case HANDLE_ENCLOSING:
        return result;
    case HANDLE_NONE:
        if (callee != NULL) {
            sema_error_at(c, e->pos,
                          "`%.*s` may fail and its error is not handled",
                          (int)callee->name.length, callee->name.text);
        } else {
            sema_error_at(c, e->pos, "this call may fail and its error is not "
                          "handled");
        }
        return sema_builtin(c, TYPE_ERROR);
    case HANDLE_TRY:
        if (!sema_in_failing_function(c)) {
            sema_error_at(c, h->pos, "`try` outside a function that may fail");
            return sema_builtin(c, TYPE_ERROR);
        }
        /* `try` forwards the error, so the function does fail and the
           `may fail` warning has its answer. */
        c->saw_fail = true;
        return result;
    case HANDLE_FATAL:
        return result;
    case HANDLE_BLOCK:
        /* `catch none` counts the failure as `none`, which the result
           must be able to hold. */
        if (h->none && result->kind == TYPE_VOID) {
            sema_error_at(c, h->pos, "`catch none` needs a result that can "
                          "be `none`, and this call gives no result");
            return sema_builtin(c, TYPE_ERROR);
        }
        if (h->none && !type_is_nullable(result) && !sema_is_error(result)) {
            sema_error_at(c, h->pos, "`catch none` needs a result that can "
                          "be `none`, and this call gives `%s`",
                          sema_tn(result));
            return sema_builtin(c, TYPE_ERROR);
        }
        sema_enter_scope(c, &scope);
        /* DESIGN: a failing function returns `?*Error`, `none` on
           success. The handler runs on the failure alone, so the error it
           binds is the `*Error` of that result and needs no check of its
           own. */
        sema_declare_caught(c, h, sema_caught_error(c, fn->result));
        c->yields = result;
        c->handler_depth++;
        sema_check_block(c, h->body);
        c->handler_depth = outer_depth;
        c->yields = outer_yield;
        sema_leave_scope(c, &scope);
        return result;
    }
    return result;
}

/* DESIGN: `Circle(2.0)` builds a value and runs the `construct` of the
   class with those arguments. The defaults are written first, so the
   body sees a complete object. A `construct` that can fail makes the
   whole expression a failing call, which needs a handler like any
   other. The `construct` is the one the class declares. It runs one
   body per level, so a class that declares none has no `construct`
   with arguments, whatever its base declares, and a literal builds
   it. */
/* The fewest parameters of the type that a call of sym gives, which is
   every parameter before the first with a default. */
static size_t required_params(const struct symbol *sym, size_t count)
{
    size_t i;

    for (i = 0; sym != NULL && sym->defaults != NULL &&
                i < sym->default_count; i++) {
        if (sym->defaults[i].value != NULL || sym->defaults[i].here) {
            return i;
        }
    }
    return count;
}

/* How many parameters after the first `given` the defaults of sym fill,
   when every one after them has a default. The count is in the
   parameters of the type, `self` included. */
static size_t filled_by_defaults(const struct symbol *sym, size_t given)
{
    size_t i;

    if (sym == NULL || sym->defaults == NULL || given >= sym->default_count) {
        return 0;
    }
    for (i = given; i < sym->default_count; i++) {
        if (sym->defaults[i].value == NULL && !sym->defaults[i].here) {
            return 0;
        }
    }
    return sym->default_count - given;
}

/* The argument of a parameter that the call leaves out. A constant reads
   as a name of it, and `here` is the position of the call. Both carry
   their type already, so neither is checked again. */
static struct expr *default_argument(struct checker *c, const struct expr *call,
                                     const struct param_default *d,
                                     struct type *t)
{
    struct expr *arg = sema_new_node(c, d->here ? EXPR_HERE : EXPR_NAME,
                                     call->pos);
    struct symbol *value;

    arg->type = t;
    if (d->here) {
        return arg;
    }
    value = arena_alloc(c->arena, sizeof *value);
    value->kind = SYMBOL_CONST;
    value->type = t;
    value->value = (struct const_value *)d->value;
    value->state = EVAL_DONE;
    arg->symbol = value;
    return arg;
}

/* Append the arguments of the `filled` parameters after the first
   `given`, which count in the parameters of the type. */
static void append_defaults(struct checker *c, struct expr *e,
                            const struct symbol *sym, const struct type *fn,
                            size_t given, size_t filled)
{
    size_t have = e->as.call.arg_count;
    struct expr **args =
        types_alloc_array(c->arena, have + filled, sizeof *args);
    size_t i;

    if (have > 0) {
        memcpy(args, e->as.call.args, have * sizeof *args);
    }
    for (i = 0; i < filled; i++) {
        args[have + i] = default_argument(c, e, &sym->defaults[given + i],
                                          fn->params[given + i]);
    }
    e->as.call.args = args;
    e->as.call.arg_count = have + filled;
}

static struct type *check_construct(struct checker *c, struct expr *e,
                                    struct type *t, struct type *expected)
{
    static const struct name construct_name = {"construct", 9};
    struct item *m = NULL;
    struct type *fn;
    size_t given;
    size_t filled;
    size_t i;
    bool ok = true;

    for (i = 0; i < t->member_count; i++) {
        if (sema_same_name(&t->members[i]->name, &construct_name)) {
            m = t->members[i];
        }
    }

    if (m == NULL || m->kind != ITEM_FN || m->symbol == NULL ||
        m->param_count == 0) {
        sema_error_at(c, e->pos, "`%s` has no `construct` with arguments, so a "
                      "literal builds it", sema_tn(t));
        return sema_builtin(c, TYPE_ERROR);
    }
    if (sema_refuse_abstract_value(c, e->pos, "a value", t)) {
        return sema_builtin(c, TYPE_ERROR);
    }
    fn = m->symbol->type;
    /* A parameter of an unknown type leaves construct without a function
       type. That type had its message, and the call has no count to
       compare. */
    if (fn == NULL || fn->kind != TYPE_FN) {
        return sema_builtin(c, TYPE_ERROR);
    }
    fn = sema_member_type(c, fn, t);
    e->as.call.builds = t;
    e->as.call.callee->symbol = m->symbol;
    e->as.call.callee->type = fn;
    given = e->as.call.arg_count;
    filled = filled_by_defaults(m->symbol, given + 1);
    if (given + filled + 1 != fn->param_count) {
        size_t least = required_params(m->symbol, fn->param_count) - 1;
        size_t n = given < least ? least : fn->param_count - 1;
        sema_error_at(c, e->pos,
                      "`%s.construct` takes %s%zu argument%s, found %zu",
                      sema_tn(t),
                      least == fn->param_count - 1 ? ""
                      : given < least              ? "at least "
                                                   : "at most ",
                      n, n == 1 ? "" : "s", given);
        return sema_builtin(c, TYPE_ERROR);
    }
    for (i = 0; i < given; i++) {
        struct expr *arg = e->as.call.args[i];
        struct type *got = sema_check_expr(c, arg, fn->params[i + 1]);
        c->lent_use = LENT_PASSED;
        ok = sema_require(c, arg, got, fn->params[i + 1]) && ok;
        c->lent_use = LENT_STORED;
        sema_refuse_lock_copy(c, arg, fn->params[i + 1]);
        note_move(c, m->symbol, i + 1, NULL, arg);
    }
    if (!ok) {
        return sema_builtin(c, TYPE_ERROR);
    }
    if (filled > 0) {
        append_defaults(c, e, m->symbol, fn, given + 1, filled);
    }
    if (m->may_fail) {
        return check_handled(c, e, fn, expected);
    }
    return handled_result(c, e, fn);
}

/* DESIGN: `mul_high(a, b)` is a built-in, and a function or a local of
   that name wins over it, as a class named `Object` does. The call
   becomes a binary expression, so the rules of an operator hold for it:
   two operands of one integer type, constants, and the lowering. */
static struct type *check_mul_high(struct checker *c, struct expr *e,
                                   struct type *expected)
{
    struct expr *a;
    struct expr *b;

    if (e->as.call.arg_count != 2) {
        sema_error_at(c, e->pos, "`" MUL_HIGH "` takes 2 arguments, found %d",
                      (int)e->as.call.arg_count);
        return sema_builtin(c, TYPE_ERROR);
    }
    if (e->as.call.handler.kind != HANDLE_NONE) {
        sema_error_at(c, e->as.call.handler.pos,
                      "this call cannot fail, so it has no error to handle");
        return sema_builtin(c, TYPE_ERROR);
    }
    a = e->as.call.args[0];
    b = e->as.call.args[1];
    e->kind = EXPR_BINARY;
    memset(&e->as, 0, sizeof e->as);
    e->as.binary.op = TOKEN_MUL_HIGH;
    e->as.binary.left = a;
    e->as.binary.right = b;
    return sema_check_binary(c, e, expected);
}

/* Locking and channels */

/* The channel that the operation what reads. A pointer to one is
   written `*p`, as it is for the value a `switch` takes. */
static struct type *channel_of(struct checker *c, struct expr *e,
                               const char *what)
{
    struct type *t = sema_check_expr(c, e, NULL);

    if (!sema_is_error(t) && !types_is_chan(t)) {
        sema_error_at(c, e->pos, "`%s` takes a channel, found `%s`", what,
                      sema_tn(t));
        return sema_builtin(c, TYPE_ERROR);
    }
    return t;
}

/* Rewrite the call e into the operation op on target. None of the calls
   can fail, so a handler on one is refused. */
static bool sync_call(struct checker *c, struct expr *e, enum sync_op op,
                      struct expr *target)
{
    if (e->as.call.handler.kind != HANDLE_NONE) {
        sema_error_at(c, e->as.call.handler.pos,
                      "this call cannot fail, so it has no error to handle");
        return false;
    }
    e->kind = EXPR_SYNC_OP;
    memset(&e->as, 0, sizeof e->as);
    e->as.sync_op.op = op;
    e->as.sync_op.target = target;
    return true;
}

/* `close(c)` ends what a channel takes. What it holds is still
   received. */
static struct type *check_close(struct checker *c, struct expr *e)
{
    struct type *t;

    if (e->as.call.arg_count != 1) {
        sema_error_at(c, e->pos, "`" CHAN_CLOSE "` takes 1 argument, found %d",
                      (int)e->as.call.arg_count);
        return sema_builtin(c, TYPE_ERROR);
    }
    t = channel_of(c, e->as.call.args[0], CHAN_CLOSE);
    if (sema_is_error(t) || !sync_call(c, e, SYNC_CLOSE, e->as.call.args[0])) {
        return sema_builtin(c, TYPE_ERROR);
    }
    return sema_builtin(c, TYPE_VOID);
}

/* `Mutex.new()` makes a mutex, which the runtime holds. */
static struct type *check_mutex_new(struct checker *c, struct expr *e)
{
    const struct name *name = &e->as.call.callee->as.field.name;

    if (!sema_name_is(name, MUTEX_NEW)) {
        sema_error_at(c, e->as.call.callee->pos, "`" LANG_MUTEX "` has no "
                      "function `%.*s`", (int)name->length, name->text);
        return sema_builtin(c, TYPE_ERROR);
    }
    if (e->as.call.arg_count != 0) {
        sema_error_at(c, e->pos, "`" LANG_MUTEX "." MUTEX_NEW "` takes no "
                      "arguments");
        return sema_builtin(c, TYPE_ERROR);
    }
    if (!sync_call(c, e, SYNC_MUTEX_NEW, NULL)) {
        return sema_builtin(c, TYPE_ERROR);
    }
    return types_mutex(c->types);
}

/* DESIGN: `Regex.compile(text)` is the call `compile(text)` of
   `anti.regex`, which the checker writes over the callee and checks as
   any call, so the argument, the failure and its handler follow the rules
   of a program's own call. The module it names cannot be written in a
   program. It stands in a scope of its own, as the one of an `f"..."`
   does. */
static const struct name hidden_regex = {"<regex>", 7};

static struct type *check_regex_compile(struct checker *c, struct expr *e,
                                        struct type *expected)
{
    static const struct name module = {REGEX_MODULE,
                                       sizeof REGEX_MODULE - 1};
    struct expr *callee = e->as.call.callee;
    const struct name *name = &callee->as.field.name;
    bool bytes = sema_name_is(&callee->as.field.base->as.name,
                              LANG_BYTE_REGEX);
    const char *type = bytes ? LANG_BYTE_REGEX : LANG_REGEX;
    const char *function = bytes ? REGEX_COMPILE_BYTES : REGEX_COMPILE;
    const struct interface *lib;
    struct symbol *home;
    struct scope scope;
    struct type *t;

    if (!sema_name_is(name, REGEX_COMPILE)) {
        sema_error_at(c, callee->pos, "`%s` has no function `%.*s`", type,
                      (int)name->length, name->text);
        return sema_builtin(c, TYPE_ERROR);
    }
    lib = sema_find_library(c, &module);
    if (lib == NULL) {
        sema_error_at(c, e->pos, "`%s." REGEX_COMPILE "` calls `"
                      REGEX_MODULE ".%s`, so the module imports `"
                      REGEX_MODULE "`", type, function);
        return sema_builtin(c, TYPE_ERROR);
    }
    sema_enter_scope(c, &scope);
    home = sema_declare(c, SYMBOL_MODULE, &hidden_regex, e->pos,
                        "`%.*s` is already declared");
    home->home = lib;
    callee->as.field.base->as.name = hidden_regex;
    callee->as.field.name.text = function;
    callee->as.field.name.length = strlen(function);
    t = sema_check_call(c, e, expected);
    sema_leave_scope(c, &scope);
    return t;
}

/* `m.destroy()` releases the mutex that m names, a Mutex in a place or
   a pointer to one. */
static struct type *check_mutex_destroy(struct checker *c, struct expr *e,
                                        struct type *base)
{
    struct expr *m = e->as.call.callee->as.field.base;

    if (e->as.call.arg_count != 0) {
        sema_error_at(c, e->pos, "`" MUTEX_DESTROY "` takes no arguments");
        return sema_builtin(c, TYPE_ERROR);
    }
    if (base->kind == TYPE_POINTER) {
        sema_usable_pointer(c, m, base);
    } else if (!sema_is_place(m)) {
        sema_error_at(c, m->pos, "calling `" MUTEX_DESTROY "` needs a place");
        return sema_builtin(c, TYPE_ERROR);
    } else {
        sema_mark_address_taken(c, m);
    }
    if (!sync_call(c, e, SYNC_MUTEX_DESTROY, m)) {
        return sema_builtin(c, TYPE_ERROR);
    }
    return sema_builtin(c, TYPE_VOID);
}

/* Rewrite the call e into the built-in op of the simd struct simd with
   the operands args. None of the built-ins can fail, so a handler on one
   is refused. */
static bool simd_node(struct checker *c, struct expr *e, enum simd_op op,
                      struct expr **args, size_t count,
                      const struct type *simd)
{
    if (e->as.call.handler.kind != HANDLE_NONE) {
        sema_error_at(c, e->as.call.handler.pos,
                      "this call cannot fail, so it has no error to handle");
        return false;
    }
    e->kind = EXPR_SIMD;
    memset(&e->as, 0, sizeof e->as);
    e->as.simd.op = op;
    e->as.simd.args = args;
    e->as.simd.arg_count = count;
    e->as.simd.simd = simd;
    return true;
}

/* Whether the call e names a function of `anti.simd` that the compiler
   knows, through the import of that module. */
static bool simd_module_call(const struct checker *c, const struct expr *e)
{
    const struct expr *callee = e->as.call.callee;
    const struct symbol *module;

    if (callee->kind != EXPR_FIELD ||
        (module = qualifier(c, callee)) == NULL || module->home == NULL ||
        strcmp(module->home->module, SIMD_MODULE) != 0) {
        return false;
    }
    return sema_name_is(&callee->as.field.name, SIMD_SELECT) ||
           sema_name_is(&callee->as.field.name, SIMD_ANY) ||
           sema_name_is(&callee->as.field.name, SIMD_ALL);
}

/* The type a reduction of lanes of type lane gives. An f16 lane is read
   as an f32, and so is the value that it reduces to. */
static struct type *simd_scalar(struct checker *c, struct type *lane)
{
    return lane->kind == TYPE_F16 ? sema_builtin(c, TYPE_F32) : lane;
}

/* `simd.select(mask, a, b)`, `simd.any(mask)` and `simd.all(mask)`. The
   mask is a simd struct of `bool`, and select takes it with the lane
   count of a and b. */
static struct type *check_simd_module(struct checker *c, struct expr *e)
{
    const struct name *name = &e->as.call.callee->as.field.name;
    struct expr **args = e->as.call.args;
    size_t count = e->as.call.arg_count;
    bool select = sema_name_is(name, SIMD_SELECT);
    struct type *mask;
    struct type *a;
    struct type *b;

    if (count != (select ? 3u : 1u)) {
        sema_error_at(c, e->pos, "`simd.%.*s` takes %d argument%s, found %d",
                      (int)name->length, name->text, select ? 3 : 1,
                      select ? "s" : "", (int)count);
        return sema_builtin(c, TYPE_ERROR);
    }
    mask = sema_check_expr(c, args[0], NULL);
    if (sema_is_error(mask)) {
        return mask;
    }
    if (!type_is_mask(mask)) {
        sema_error_at(c, args[0]->pos,
                      "`simd.%.*s` takes a mask, a `simd struct` "
                      "of `bool`, found `%s`", (int)name->length, name->text,
                      sema_tn(mask));
        return sema_builtin(c, TYPE_ERROR);
    }
    if (!select) {
        if (!simd_node(c, e, sema_name_is(name, SIMD_ANY) ? SIMD_OP_ANY
                                                          : SIMD_OP_ALL,
                       args, 1, mask)) {
            return sema_builtin(c, TYPE_ERROR);
        }
        return sema_builtin(c, TYPE_BOOL);
    }
    a = sema_check_expr(c, args[1], NULL);
    if (sema_is_error(a)) {
        return a;
    }
    if (!type_is_simd(a)) {
        sema_error_at(c, args[1]->pos,
                      "`simd.select` chooses between values of a "
                      "`simd struct`, found `%s`", sema_tn(a));
        return sema_builtin(c, TYPE_ERROR);
    }
    b = sema_check_expr(c, args[2], a);
    if (!sema_require(c, args[2], b, a)) {
        return sema_builtin(c, TYPE_ERROR);
    }
    if (mask->field_count != a->field_count) {
        sema_error_at(c, args[0]->pos,
                      "the mask of `simd.select` has %zu lanes, "
                      "and `%s` has %zu", mask->field_count, sema_tn(a),
                      a->field_count);
        return sema_builtin(c, TYPE_ERROR);
    }
    if (!simd_node(c, e, SIMD_OP_SELECT, args, 3, a)) {
        return sema_builtin(c, TYPE_ERROR);
    }
    return a;
}

/* Whether name is a built-in on the simd struct itself, `T.splat` or
   `T.load`. */
static bool simd_static_name(const struct name *name)
{
    return sema_name_is(name, SIMD_SPLAT) || sema_name_is(name, SIMD_LOAD);
}

/* `T.splat(v)` writes v into every lane, and `T.load(slice, i)` reads
   the lanes from the elements of slice from i on. */
static struct type *check_simd_static(struct checker *c, struct expr *e,
                                      struct type *t)
{
    const struct name *name = &e->as.call.callee->as.field.name;
    struct expr **args = e->as.call.args;
    size_t count = e->as.call.arg_count;
    struct type *lane = type_simd_lane(t);
    struct type *i64 = sema_builtin(c, TYPE_I64);

    if (sema_name_is(name, SIMD_SPLAT)) {
        if (count != 1) {
            sema_error_at(c, e->pos,
                          "`%s." SIMD_SPLAT "` takes 1 argument, found "
                          "%d", sema_tn(t), (int)count);
            return sema_builtin(c, TYPE_ERROR);
        }
        if (!sema_require(c, args[0], sema_check_expr(c, args[0], lane),
                          lane) ||
            !simd_node(c, e, SIMD_OP_SPLAT, args, 1, t)) {
            return sema_builtin(c, TYPE_ERROR);
        }
        return t;
    }
    if (count != 2) {
        sema_error_at(c, e->pos,
                      "`%s." SIMD_LOAD "` takes 2 arguments, found %d",
                      sema_tn(t), (int)count);
        return sema_builtin(c, TYPE_ERROR);
    }
    {
        struct type *slice = types_slice(c->types, lane);
        bool ok = sema_require(c, args[0], sema_check_expr(c, args[0], slice),
                               slice);
        ok = sema_require(c, args[1], sema_check_expr(c, args[1], i64),
                          i64) && ok;
        if (!ok || !simd_node(c, e, SIMD_OP_LOAD, args, 2, t)) {
            return sema_builtin(c, TYPE_ERROR);
        }
    }
    return t;
}

/* Whether name is a built-in on a value of a simd struct. */
static bool simd_value_name(const struct name *name)
{
    return sema_name_is(name, SIMD_STORE) || sema_name_is(name, SIMD_SHUFFLE) ||
           sema_name_is(name, SIMD_SUM) || sema_name_is(name, SIMD_MIN) ||
           sema_name_is(name, SIMD_MAX) || sema_name_is(name, SIMD_DOT);
}

/* The built-ins on a value v of the simd struct s, or on a pointer to
   one: `v.store(slice, i)`, `v.shuffle(i, ...)`, `v.sum()`, `v.min()`,
   `v.max()` and `a.dot(b)`. A shuffle names the lane of v that each lane
   of its result takes, by a constant index. */
static struct type *check_simd_value(struct checker *c, struct expr *e,
                                     struct type *base, struct type *s)
{
    struct expr *receiver = e->as.call.callee->as.field.base;
    const struct name *name = &e->as.call.callee->as.field.name;
    size_t count = e->as.call.arg_count;
    struct type *lane = type_simd_lane(s);
    struct type *i64 = sema_builtin(c, TYPE_I64);
    struct expr **args = types_alloc_array(c->arena, count + 1, sizeof *args);
    enum simd_op op;
    size_t want;
    size_t i;

    if (base->kind == TYPE_POINTER) {
        sema_usable_pointer(c, receiver, base);
    }
    args[0] = receiver;
    memcpy(args + 1, e->as.call.args, count * sizeof *args);
    op = sema_name_is(name, SIMD_STORE)     ? SIMD_OP_STORE
         : sema_name_is(name, SIMD_SHUFFLE) ? SIMD_OP_SHUFFLE
         : sema_name_is(name, SIMD_SUM)     ? SIMD_OP_SUM
         : sema_name_is(name, SIMD_MIN)     ? SIMD_OP_MIN
         : sema_name_is(name, SIMD_MAX)     ? SIMD_OP_MAX
                                            : SIMD_OP_DOT;
    want = op == SIMD_OP_STORE     ? 2
           : op == SIMD_OP_SHUFFLE ? s->field_count
           : op == SIMD_OP_DOT     ? 1
                                   : 0;
    if (count != want) {
        sema_error_at(c, e->pos, "`%.*s` takes %zu argument%s, found %d",
                      (int)name->length, name->text, want, want == 1 ? "" : "s",
                      (int)count);
        return sema_builtin(c, TYPE_ERROR);
    }
    if (op != SIMD_OP_STORE && op != SIMD_OP_SHUFFLE &&
        !sema_simd_numeric(lane)) {
        sema_error_at(c, e->pos, "`%.*s` needs lanes of numbers, and `%s` has "
                      "`%s`", (int)name->length, name->text, sema_tn(s),
                      sema_tn(lane));
        return sema_builtin(c, TYPE_ERROR);
    }
    switch (op) {
    case SIMD_OP_STORE: {
        struct type *slice = types_slice(c->types, lane);
        bool ok = sema_require(c, args[1], sema_check_expr(c, args[1], slice),
                               slice);
        ok = sema_require(c, args[2], sema_check_expr(c, args[2], i64),
                          i64) && ok;
        if (!ok || !simd_node(c, e, op, args, 3, s)) {
            return sema_builtin(c, TYPE_ERROR);
        }
        return sema_builtin(c, TYPE_VOID);
    }
    case SIMD_OP_SHUFFLE: {
        uint32_t *lanes = types_alloc_array(c->arena, want, sizeof *lanes);
        for (i = 0; i < want; i++) {
            struct const_value v;
            if (!sema_require(c, args[i + 1],
                              sema_check_expr(c, args[i + 1], i64), i64)) {
                return sema_builtin(c, TYPE_ERROR);
            }
            if (!sema_eval_const(c, args[i + 1], &v) || v.kind != CONST_INT ||
                v.as.integer >= want) {
                sema_error_at(c, args[i + 1]->pos, "`" SIMD_SHUFFLE "` takes "
                              "constant lane indexes from 0 to %zu", want - 1);
                return sema_builtin(c, TYPE_ERROR);
            }
            lanes[i] = (uint32_t)v.as.integer;
        }
        if (!simd_node(c, e, op, args, 1, s)) {
            return sema_builtin(c, TYPE_ERROR);
        }
        e->as.simd.lanes = lanes;
        return s;
    }
    case SIMD_OP_DOT:
        if (!sema_require(c, args[1], sema_check_expr(c, args[1], s), s) ||
            !simd_node(c, e, op, args, 2, s)) {
            return sema_builtin(c, TYPE_ERROR);
        }
        return simd_scalar(c, lane);
    default:
        if (!simd_node(c, e, op, args, 1, s)) {
            return sema_builtin(c, TYPE_ERROR);
        }
        return simd_scalar(c, lane);
    }
}

/* Whether the module of s declares a function that `v.name(args)` calls
   through the method syntax. Such a function wins over a built-in of
   that name, as a function named `close` wins over `close(c)`. */
static bool simd_method_declared(struct checker *c, const struct type *s,
                                 const struct name *name)
{
    struct symbol *f = sema_method_symbol(c, s, name);

    return f != NULL && (f->kind == SYMBOL_FN || f->kind == SYMBOL_EXTERN_FN) &&
           f->type != NULL && !sema_is_error(f->type) &&
           f->type->kind == TYPE_FN &&
           f->type->param_count > 0 &&
           sema_struct_of(f->type->params[0]) == s;
}

/* `chan T(n)`, `send(c, v)` and `recv(c)`, which the parser writes. The
   nodes the checker writes from calls carry their type already. */
struct type *sema_check_sync_op(struct checker *c, struct expr *e)
{
    struct type *i64 = sema_builtin(c, TYPE_I64);
    struct type *t;

    switch (e->as.sync_op.op) {
    case SYNC_CHAN_NEW:
        t = sema_chan_element(c, e->as.sync_op.element);
        if (!sema_require(c, e->as.sync_op.value,
                          sema_check_expr(c, e->as.sync_op.value, i64), i64) ||
            sema_is_error(t)) {
            return sema_builtin(c, TYPE_ERROR);
        }
        return types_chan(c->types, t);
    case SYNC_SEND:
        t = channel_of(c, e->as.sync_op.target, "send");
        if (sema_is_error(t)) {
            sema_check_expr(c, e->as.sync_op.value, NULL);
            return t;
        }
        if (!sema_require(c, e->as.sync_op.value,
                          sema_check_expr(c, e->as.sync_op.value, t->element),
                          t->element)) {
            return sema_builtin(c, TYPE_ERROR);
        }
        /* The channel holds a copy of the value, which an `own` field
           would give two owners. */
        sema_refuse_owned_copy(c, e->as.sync_op.value, t->element);
        return sema_builtin(c, TYPE_VOID);
    case SYNC_RECV:
        t = channel_of(c, e->as.sync_op.target, "recv");
        return sema_is_error(t) ? t
                                : types_pointer_nullable(c->types, t->element);
    default:
        return e->type;
    }
}

/* The class a dotted path names. It is a class of the module being
   checked when the qualifier is empty, and a pub class of another
   module otherwise. The qualifier names that module by its alias or by
   its whole path. NULL when neither answers. */
const struct type *sema_interface_named(struct checker *c,
                                        const struct name *qualifier,
                                        const struct name *name,
                                        struct pos pos)
{
    const struct interface *lib;
    struct symbol *sym;

    if (qualifier->length == 0) {
        sym = sema_module_find(c, name);
        if (sym == NULL || sym->kind != SYMBOL_STRUCT) {
            sema_error_at(c, pos, "cannot find class `%.*s`", (int)name->length,
                          name->text);
            return NULL;
        }
        return sym->type;
    }
    sym = sema_scope_find_local(&c->module_scope, qualifier);
    lib = sym != NULL && sym->kind == SYMBOL_MODULE
              ? sym->home
              : sema_find_library(c, qualifier);
    if (lib == NULL) {
        sema_error_at(c, pos, "cannot find module `%.*s`",
                      (int)qualifier->length,
                      qualifier->text);
        return NULL;
    }
    sym = sema_library_item(c, lib, name);
    if (sym == NULL || sym->kind != SYMBOL_STRUCT) {
        sema_error_at(c, pos, "`%.*s` has no public class `%.*s`",
                      (int)qualifier->length, qualifier->text,
                      (int)name->length,
                      name->text);
        return NULL;
    }
    return sym->type;
}

/* The module and the class of `anti.plugin.Library`, whose two calls
   the compiler carries because each names an interface. */
#define PLUGIN_MODULE "anti.plugin"

#define PLUGIN_LIBRARY "Library"

static bool plugin_library(const struct type *t)
{
    return t != NULL && t->kind == TYPE_CLASS &&
           sema_name_is(&t->name, PLUGIN_LIBRARY) &&
           sema_name_is(&t->module, PLUGIN_MODULE);
}

/* The dotted path an expression of names spells, as `anti.log.Logger`
   writes one. The last name goes to `last` and the ones before it to
   `qualifier`. Returns false for any other expression. */
enum { PATH_PARTS = 8 };

static bool name_path(struct checker *c, const struct expr *e,
                      struct name *qualifier, struct name *last)
{
    const struct name *parts[PATH_PARTS];
    struct text path = {0};
    size_t count = 0;
    size_t length;
    size_t dot;
    char *copy;
    size_t i;

    for (; e->kind == EXPR_FIELD && !e->as.field.optional;
         e = e->as.field.base) {
        if (count == PATH_PARTS) {
            return false;
        }
        parts[count++] = &e->as.field.name;
    }
    if (e->kind != EXPR_NAME) {
        return false;
    }
    text_appendf(&path, "%.*s", (int)e->as.name.length, e->as.name.text);
    for (i = count; i > 0; i--) {
        text_appendf(&path, ".%.*s", (int)parts[i - 1]->length,
                     parts[i - 1]->text);
    }
    length = path.length;
    copy = arena_alloc(c->arena, length + 1);
    memcpy(copy, text_cstr(&path), length + 1);
    text_free(&path);
    dot = length;
    while (dot > 0 && copy[dot - 1] != '.') {
        dot--;
    }
    qualifier->text = copy;
    qualifier->length = dot > 0 ? dot - 1 : 0;
    last->text = copy + dot;
    last->length = length - dot;
    return true;
}

/* DESIGN: `lib.instance(I)` and `lib.supports(I, "f")` name an
   interface where a value stands, so the compiler carries them. Each
   becomes the call of an ordinary function of `anti.plugin.Library`
   with the descriptor of the interface in the place of the name.
   `instance` then has the type `?*I`, which `catch fatal` narrows. */
static const struct type *plugin_call(struct checker *c, struct expr *e,
                                      bool *ok)
{
    struct expr *callee = e->as.call.callee;
    bool instance = sema_name_is(&callee->as.field.name, "instance");
    size_t wanted = instance ? 1 : 2;
    struct expr *argument;
    const struct type *iface;
    struct name qualifier;
    struct name last;
    struct expr *given;

    *ok = false;
    if (e->as.call.arg_count != wanted) {
        sema_error_at(c, e->pos,
                      "`%s` of a library takes %zu argument%s, found "
                      "%zu", instance ? "instance" : "supports", wanted,
                      wanted == 1 ? "" : "s", e->as.call.arg_count);
        return NULL;
    }
    given = e->as.call.args[0];
    if (!name_path(c, given, &qualifier, &last)) {
        sema_error_at(c, given->pos, "the first argument names an interface");
        return NULL;
    }
    iface = sema_interface_named(c, &qualifier, &last, given->pos);
    if (iface == NULL) {
        return NULL;
    }
    if (iface->kind != TYPE_CLASS || !iface->has_abstract) {
        sema_error_at(c, given->pos, "`%s` is not abstract, and a library "
                      "provides an interface", sema_tn(iface));
        return NULL;
    }
    argument = sema_new_node(c, EXPR_DESCRIPTOR, given->pos);
    argument->as.descriptor_of = iface;
    e->as.call.args[0] = argument;
    callee->as.field.name.text = instance ? "instance_at" : "supports_at";
    /* Both names are 11 bytes long. */
    callee->as.field.name.length = sizeof "instance_at" - 1;
    *ok = true;
    return instance ? iface : NULL;
}

static struct type *check_call(struct checker *c, struct expr *e,
                              struct type *expected, struct generic_call *g);

/* DESIGN: a call carries what a generic needs from it past the rewrites
   of the checker. That is the type arguments written after the callee's
   name, and the class or the receiver the callee is reached through. The
   callee is the name checked now, so a generic function named there is
   a call and not a value. */
/* Refuse the field name of a value of type t that e reads. The callee of
   a bare call of a shared `operator fn` names the type none of them
   takes. */
static void refuse_field(struct checker *c, const struct expr *e,
                         const struct type *t, const struct name *name)
{
    if (e->as.field.bare) {
        sema_error_at(c, e->pos, "no `operator fn %.*s` of the module takes "
                      "`%s` first", (int)name->length, name->text,
                      sema_tn(t));
        return;
    }
    sema_error_at(c, e->pos, "`%s` has no field `%.*s`", sema_tn(t),
                  (int)name->length, name->text);
}

/* DESIGN: `--no-reflect` drops the field lists, the function lists and
   the registry. The default `serialize`, `Object.deserialize` and every
   function of `anti.reflect` read them. A call of one is therefore refused
   where it stands rather than giving an empty answer at run time. `==` and
   `hash` read none, since the compiler writes both as code, and a class
   that writes its own `serialize` keeps it. */
static void refuse_without_reflect(struct checker *c, const struct expr *e)
{
    const struct expr *callee;
    const struct symbol *sym;

    if (e->kind != EXPR_CALL) {
        return;
    }
    callee = e->as.call.callee;
    sym = callee->symbol;
    if (sym == NULL) {
        return;
    }
    if (sym->item != NULL && sym->item->runtime != NULL &&
        (sema_name_is(&sym->item->name, "serialize") ||
         sema_name_is(&sym->item->name, ROOT_DESERIALIZE))) {
        sema_error_at(c, e->pos, "`%.*s` reads the field lists, which "
                      "`--no-reflect` drops", (int)sym->item->name.length,
                      sym->item->name.text);
        return;
    }
    if (sym->home != NULL && sym->home->module != NULL &&
        strcmp(sym->home->module, REFLECT_MODULE) == 0) {
        sema_error_at(c, e->pos, "`" REFLECT_MODULE "` reads the field "
                      "lists, the function lists and the registry, which "
                      "`--no-reflect` drops");
    }
}

/* Whether the module declares a shared `operator fn` of the name name,
   one it holds as `name:Type`. */
static bool shared_in_module(const struct checker *c, const struct name *name)
{
    size_t i;

    for (i = 0; i < c->module_scope.count; i++) {
        const struct name *n = &c->module_scope.entries[i].name;
        if (n->length > name->length && n->text[name->length] == ':' &&
            memcmp(n->text, name->text, name->length) == 0) {
            return true;
        }
    }
    return false;
}

/* DESIGN: a bare call `eq(a, b)` of a shared `operator fn` picks the
   function by the type of its first argument, as `a == b` does. The call
   becomes `a.eq(b)`, whose receiver the lookup of a function of a type
   reads, and each argument is still checked once. */
static void call_on_first(struct checker *c, struct expr *e)
{
    struct expr *callee = e->as.call.callee;
    struct expr *field;

    if (callee->kind != EXPR_NAME || e->as.call.arg_count == 0 ||
        callee->type_arg_count > 0 ||
        sema_lookup(c, &callee->as.name) != NULL ||
        !shared_in_module(c, &callee->as.name)) {
        return;
    }
    field = sema_new_node(c, EXPR_FIELD, callee->pos);
    field->as.field.base = e->as.call.args[0];
    field->as.field.name = callee->as.name;
    field->as.field.bare = true;
    e->as.call.callee = field;
    e->as.call.args++;
    e->as.call.arg_count--;
}

struct type *sema_check_call(struct checker *c, struct expr *e,
                             struct type *expected)
{
    struct expr *callee;
    const struct expr *saved = c->callee;
    struct generic_call g;
    struct type *t;

    call_on_first(c, e);
    callee = e->as.call.callee;
    memset(&g, 0, sizeof g);
    g.written = callee->type_args;
    g.count = callee->type_arg_count;
    g.written_at = callee;
    g.name = callee->kind == EXPR_FIELD ? &callee->as.field.name
                                        : &callee->as.name;
    g.prechecked = types_alloc_array(c->arena, e->as.call.arg_count + 2,
                                     sizeof *g.prechecked);
    c->callee = callee;
    t = check_call(c, e, expected, &g);
    c->callee = saved;
    if (c->module->no_reflect && !sema_is_error(t)) {
        refuse_without_reflect(c, e);
    }
    return t;
}

static struct type *check_call(struct checker *c, struct expr *e,
                              struct type *expected, struct generic_call *g)
{
    struct expr *callee = e->as.call.callee;
    struct type *fn;
    struct symbol *sym;
    bool variadic = false;
    size_t fixed;
    size_t given;
    size_t filled;
    size_t i;
    bool ok = true;
    const struct symbol *module;
    /* `lib.instance(I)` gives `?*I`, which the function it becomes does
       not say. NULL for every other call. */
    const struct type *provided = NULL;

    /* An operation on an atomic field becomes one node of its own. */
    if (atomic_call(c, e, &fn)) {
        return fn;
    }
    if (callee->kind == EXPR_NAME && sema_name_is(&callee->as.name, MUL_HIGH) &&
        sema_lookup(c, &callee->as.name) == NULL) {
        return check_mul_high(c, e, expected);
    }
    if (callee->kind == EXPR_NAME &&
        sema_name_is(&callee->as.name, CHAN_CLOSE) &&
        sema_lookup(c, &callee->as.name) == NULL) {
        return check_close(c, e);
    }
    if (callee->kind == EXPR_FIELD &&
        callee->as.field.base->kind == EXPR_NAME &&
        sema_name_is(&callee->as.field.base->as.name, LANG_MUTEX) &&
        sema_lookup(c, &callee->as.field.base->as.name) == NULL) {
        return check_mutex_new(c, e);
    }
    if (callee->kind == EXPR_FIELD &&
        callee->as.field.base->kind == EXPR_NAME &&
        (sema_name_is(&callee->as.field.base->as.name, LANG_REGEX) ||
         sema_name_is(&callee->as.field.base->as.name, LANG_BYTE_REGEX)) &&
        sema_lookup(c, &callee->as.field.base->as.name) == NULL) {
        return check_regex_compile(c, e, expected);
    }
    if (simd_module_call(c, e)) {
        return check_simd_module(c, e);
    }
    if (callee->kind == EXPR_FIELD &&
        (module = qualifier(c, callee)) != NULL) {
        fn = check_qualified(c, callee, module, true);
        callee->type = fn;
        if (!sema_is_error(fn) && callee->symbol->kind == SYMBOL_EXTERN_FN) {
            variadic = callee->symbol->variadic;
        }
        fixed = 0;
    } else if (callee->kind == EXPR_FIELD &&
               callee->as.field.base->kind == EXPR_NAME &&
               (sym = (struct symbol *)sema_lookup(c,
                        &callee->as.field.base->as.name)) != NULL &&
               sym->kind == SYMBOL_STRUCT) {
        struct type *owner = sym->item != NULL && sym->item->kind == ITEM_TYPE
                                 ? sema_alias_type(c, sym)
                                 : sym->type;
        if (sema_is_error(owner)) {
            return owner;
        }
        if (type_is_simd(owner) &&
            simd_static_name(&callee->as.field.name)) {
            return check_simd_static(c, e, owner);
        }
        /* `List<int>.new()` names a copy, and `List.new()` leaves the
           arguments of the class to the call. */
        if (callee->as.field.base->type_arg_count > 0) {
            owner = sema_copy_of(c, owner, callee->as.field.base->type_args,
                                 callee->as.field.base->type_arg_count,
                                 callee->as.field.base->type_args_pos);
            if (sema_is_error(owner)) {
                return owner;
            }
        }
        g->owner = owner;
        /* T.f(args) calls a function of the body of T, which takes no
           self. An enum value is not callable. */
        fn = check_type_member(c, callee, owner);
        callee->type = fn;
        if (sema_is_error(fn)) {
            return fn;
        }
        if (fn->kind != TYPE_FN) {
            sema_error_at(c, callee->pos, "`%s` is not a function",
                          sema_tn(fn));
            return sema_builtin(c, TYPE_ERROR);
        }
        fixed = 0;
    } else if (callee->kind == EXPR_FIELD &&
               callee->as.field.base->kind == EXPR_NAME &&
               sema_lookup(c, &callee->as.field.base->as.name) == NULL &&
               sema_name_is(&callee->as.field.base->as.name, LANG_OBJECT)) {
        /* `Object.f(args)` calls a static function of the root. */
        struct type *root = types_object(c->types);
        fn = check_type_member(c, callee, root);
        callee->type = fn;
        if (sema_is_error(fn)) {
            return fn;
        }
        fixed = 0;
    } else if (callee->kind == EXPR_FIELD &&
               callee->as.field.base->kind == EXPR_FIELD &&
               (module = qualifier(c, callee->as.field.base)) != NULL &&
               (sym = sema_library_item(c, module->home,
                        &callee->as.field.base->as.field.name)) != NULL &&
               sym->kind == SYMBOL_STRUCT) {
        if (type_is_simd(sym->type) &&
            simd_static_name(&callee->as.field.name)) {
            return check_simd_static(c, e, sym->type);
        }
        /* `m.T.f(args)` calls a function of the body of a type of
           another module, which takes no self. */
        fn = check_type_member(c, callee, sym->type);
        callee->type = fn;
        if (sema_is_error(fn)) {
            return fn;
        }
        if (fn->kind != TYPE_FN) {
            sema_error_at(c, callee->pos, "`%s` is not a function",
                          sema_tn(fn));
            return sema_builtin(c, TYPE_ERROR);
        }
        fixed = 0;
    } else if (callee->kind == EXPR_FIELD && sema_pattern_call(c, e, &fn)) {
        /* A method of `str` with a pattern, or a function of a match,
           is now a call of `anti.regex` whose arguments are checked. */
        callee = e->as.call.callee;
        fixed = e->as.call.arg_count;
    } else if (callee->kind == EXPR_FIELD) {
        struct type *base =
            callee->as.field.checked
                ? callee->as.field.base->type
                : sema_check_expr(c, callee->as.field.base, NULL);
        struct type *s;
        if (sema_is_error(base)) {
            return base;
        }
        /* `x.hash()` on a type without a function of that name is its
           default hash, and on a type parameter the hook. */
        if (e->as.call.arg_count == 0 &&
            sema_name_is(&callee->as.field.name, LANG_HOOK_HASH) &&
            (base->kind == TYPE_PARAM || sema_struct_of(base) == NULL ||
             sema_find_field(sema_struct_of(base),
                             &callee->as.field.name) == NULL)) {
            struct type *hashed;
            if (sema_hash_call(c, e, base, &hashed)) {
                return hashed;
            }
        }
        /* A value of a type parameter reaches the functions of the
           interfaces its constraints name, as a pointer to the one that
           declares the function. */
        if (base->kind == TYPE_PARAM ||
            (base->kind == TYPE_POINTER &&
             base->element->kind == TYPE_PARAM)) {
            const struct type *p =
                base->kind == TYPE_PARAM ? base : base->element;
            const struct type *iface =
                sema_param_iface(p, &callee->as.field.name);
            if (iface == NULL) {
                sema_error_at(c, callee->pos, "`%s` has no function `%.*s`, "
                              "since no interface of its constraints "
                              "declares one", sema_tn(p),
                              (int)callee->as.field.name.length,
                              callee->as.field.name.text);
                return sema_builtin(c, TYPE_ERROR);
            }
            base = types_pointer(c->types, (struct type *)iface);
            callee->as.field.base->param_type = callee->as.field.base->type;
            callee->as.field.base->type = base;
            callee->as.field.checked = true;
        }
        g->owner = base;
        s = sema_struct_of(base);
        if (types_is_mutex(s) &&
            sema_name_is(&callee->as.field.name, MUTEX_DESTROY)) {
            return check_mutex_destroy(c, e, base);
        }
        if (type_is_simd(s) &&
            sema_find_field(s, &callee->as.field.name) == NULL &&
            simd_value_name(&callee->as.field.name) &&
            !simd_method_declared(c, s, &callee->as.field.name)) {
            return check_simd_value(c, e, base, s);
        }
        if (plugin_library(s) &&
            (sema_name_is(&callee->as.field.name, "instance") ||
             sema_name_is(&callee->as.field.name, "supports"))) {
            bool rewritten;
            provided = plugin_call(c, e, &rewritten);
            if (!rewritten) {
                return sema_builtin(c, TYPE_ERROR);
            }
        }
        /* DESIGN: a union has no methods, so v.f(args) on a union is
           always a call of the function pointer in field f. */
        if (s != NULL && !s->is_union &&
            sema_find_field(s, &callee->as.field.name) == NULL) {
            if (!method_call(c, e)) {
                return sema_builtin(c, TYPE_ERROR);
            }
            callee = e->as.call.callee;
            fn = callee->type;
            fixed = 1;
        } else {
            fn = sema_check_expr(c, callee, NULL);
            fixed = 0;
        }
    } else if (callee->kind == EXPR_NAME) {
        sym = sema_lookup(c, &callee->as.name);
        callee->symbol = sym;
        /* A class name in the place of a function builds a value. */
        if (sym != NULL && sym->kind == SYMBOL_STRUCT &&
            sym->type != NULL && sym->type->kind == TYPE_CLASS) {
            struct type *built = sema_generic_named(c, callee, sym->type,
                                                    &callee->as.name,
                                                    expected);
            if (sema_is_error(built)) {
                return built;
            }
            return check_construct(c, e, built, expected);
        }
        if (sym != NULL && sym->kind == SYMBOL_EXTERN_FN) {
            fn = sym->type;
            callee->type = fn;
            variadic = sym->variadic;
        } else {
            fn = sema_check_expr(c, callee, NULL);
        }
        fixed = 0;
    } else {
        fn = sema_check_expr(c, callee, NULL);
        fixed = 0;
    }
    if (sema_is_error(fn)) {
        return sema_builtin(c, TYPE_ERROR);
    }
    if (fn->kind != TYPE_FN) {
        sema_error_at(c, e->pos, "cannot call `%s`", sema_tn(fn));
        return sema_builtin(c, TYPE_ERROR);
    }
    /* A `?fn(...)` holds no function until the program has checked it. */
    fn = sema_usable_pointer(c, callee, fn);
    sema_note_call(c, callee);
    sym = function_symbol(callee);
    /* The parameters at the end that the call leaves out take their
       defaults, which are appended once the given ones are checked. */
    given = e->as.call.arg_count;
    filled = variadic ? 0 : filled_by_defaults(sym, given);
    /* DESIGN: a function that can fail returns `*Error` and writes its
       result through the last parameter. A call that gives one argument
       fewer than the function takes leaves that place to the compiler.
       The compiler passes the address of what the `let` declares. */
    if (is_failing(fn) && fn->has_out && !variadic &&
        given + filled + 1 == fn->param_count) {
        e->as.call.out = e;
    }
    if (e->as.call.out != NULL) {
        /* The out parameter is not written at the call, so the count of
           arguments the program gave is one less. */
    } else if (variadic ? given < fn->param_count
                 : given + filled != fn->param_count) {
        size_t least = required_params(sym, fn->param_count);
        size_t most = sym != NULL && sym->defaults != NULL
                          ? sym->default_count
                          : fn->param_count;
        size_t n = (given < least ? least : most) - fixed;
        const char *bound = variadic || (least < most && given < least)
                                ? "at least "
                            : least < most ? "at most "
                                           : "";
        if (sym != NULL) {
            sema_error_at(c, e->pos, "`%.*s` takes %s%zu argument%s, found %zu",
                          (int)sym->name.length, sym->name.text, bound, n,
                          n == 1 ? "" : "s", given - fixed);
        } else {
            sema_error_at(c, e->pos, "the call takes %zu argument%s, found %zu",
                          n,
                          n == 1 ? "" : "s", given - fixed);
        }
        return sema_builtin(c, TYPE_ERROR);
    }
    /* A generic function, or a function of a generic class, takes the
       arguments of its copy here, written or inferred. */
    fn = sema_generic_call(c, e, fn, sym, fixed, g);
    if (sema_is_error(fn)) {
        return fn;
    }
    callee->type = fn;
    for (i = fixed; i < given; i++) {
        struct expr *arg = e->as.call.args[i];
        if (i < fn->param_count) {
            struct type *t = g->prechecked[i] != NULL
                                 ? g->prechecked[i]
                                 : sema_check_expr(c, arg, fn->params[i]);
            c->lent_use = sym != NULL && sym->kind == SYMBOL_EXTERN_FN
                              ? LENT_TO_C
                              : LENT_PASSED;
            ok = sema_require(c, arg, t, fn->params[i]) && ok;
            c->lent_use = LENT_STORED;
            sema_refuse_lock_copy(c, arg, fn->params[i]);
            sema_check_leak_arg(c, e->as.call.callee, arg, fn->params[i]);
            if (sym != NULL && sym->worker) {
                sema_refuse_worker_closure(c, arg, fn->params[i]);
            }
            note_move(c, sym, i, fixed == 1 ? e->as.call.args[0] : NULL, arg);
        } else {
            struct type *t = sema_check_expr(c, arg, NULL);
            if (!sema_is_error(t) && !variadic_ok(t)) {
                sema_error_at(c, arg->pos,
                              "a variadic argument has type i32, u32, "
                              "int, u64, float or a pointer, found `%s`",
                              sema_tn(t));
                ok = false;
            }
        }
    }
    if (!ok) {
        return sema_builtin(c, TYPE_ERROR);
    }
    if (filled > 0) {
        append_defaults(c, e, sym, fn, given, filled);
    }
    if (is_failing(fn)) {
        return check_handled(c, e, fn, expected);
    }
    /* A call that cannot fail may still give a `?*T`, and a `catch` on
       it guards the pointer rather than an error. The `let` that holds
       it takes the handler over, so the two forms read alike. */
    if (e->as.call.handler.none) {
        sema_error_at(c, e->as.call.handler.pos,
                      "`catch none` counts a failure as `none`, and this "
                      "call cannot fail");
        return sema_builtin(c, TYPE_ERROR);
    }
    if (e->as.call.handler.kind != HANDLE_NONE &&
        types_is_maybe_match(fn->result)) {
        sema_error_at(c, e->as.call.handler.pos,
                      "this call cannot fail, and a match is tested with `if` "
                      "or `let ... else`");
        return sema_builtin(c, TYPE_ERROR);
    }
    if (e->as.call.handler.kind != HANDLE_NONE) {
        bool after_optional =
            e->as.call.optional &&
            (e->as.call.handler.kind == HANDLE_BLOCK ||
             e->as.call.handler.kind == HANDLE_FATAL);
        if (!type_is_nullable(fn->result) && !after_optional) {
            sema_error_at(c, e->as.call.handler.pos,
                          "this call cannot fail, so it has no error to "
                          "handle");
            return sema_builtin(c, TYPE_ERROR);
        }
        e->as.call.guards_pointer = true;
    }
    if (provided != NULL) {
        return types_with_none(
            c->types, types_pointer(c->types, (struct type *)provided));
    }
    /* The match of a pattern literal knows its groups. */
    if (e->as.call.pattern != NULL && types_is_maybe_match(fn->result)) {
        return types_with_none(c->types,
                               types_match(c->types, e->as.call.pattern));
    }
    return fn->result;
}

/* DESIGN: the function or constant that the body of t declares under
   name, or the one a class above it declares. A class inherits the
   namespace of its base, so a call on a derived pointer finds the
   inherited function and dispatches from the derived class. */
struct item *sema_find_member(const struct type *t, const struct name *name)
{
    size_t i;

    for (; t != NULL; t = t->kind == TYPE_CLASS ? t->base : NULL) {
        for (i = 0; i < t->member_count; i++) {
            if (sema_same_name(&t->members[i]->name, name)) {
                return t->members[i];
            }
        }
    }
    return NULL;
}

/* The text `a.b`, for the names the checker gives the parts of a
   variant. */
struct name sema_dotted(struct checker *c, const struct name *a,
                        const struct name *b)
{
    char *text = arena_alloc(c->arena, a->length + b->length + 2);
    struct name out;

    memcpy(text, a->text, a->length);
    text[a->length] = '.';
    memcpy(text + a->length + 1, b->text, b->length);
    out.text = text;
    out.length = a->length + b->length + 1;
    return out;
}

/* The name of case index of the variant v. */
const struct name *sema_case_name(const struct type *v, size_t index)
{
    return &v->base->fields[index].name;
}

/* DESIGN: `Shape.Empty` is the literal of a case without fields, and the
   checker writes it as the literal `Shape.Empty { }`, so lowering reads
   one form. A case with fields names them in braces. */
static struct type *variant_case_value(struct checker *c, struct expr *e,
                                       struct type *t)
{
    struct name name = e->as.field.name;
    size_t index;

    if (!types_case_index(t, &name, &index)) {
        sema_error_at(c, e->pos, "`%s` has no case `%.*s`", sema_tn(t),
                      (int)name.length, name.text);
        return sema_builtin(c, TYPE_ERROR);
    }
    if (t->params[index] != NULL) {
        sema_error_at(c, e->pos,
                      "`%s.%.*s` has fields, which its literal names "
                      "in braces", sema_tn(t), (int)name.length, name.text);
        return sema_builtin(c, TYPE_ERROR);
    }
    e->kind = EXPR_STRUCT_LIT;
    memset(&e->as.struct_lit, 0, sizeof e->as.struct_lit);
    e->as.struct_lit.module = t->name;
    e->as.struct_lit.name = name;
    e->as.struct_lit.variant_case = (uint32_t)(index + 1);
    return t;
}

/* `Shape.Circle { r: 2.0 }`: the fields of the case against its struct,
   which a case without fields does not have. */
struct type *sema_variant_literal(struct checker *c, struct expr *e,
                                  struct type *v, const struct name *name)
{
    size_t index;
    const struct type *payload;
    char written[160];

    if (!types_case_index(v, name, &index)) {
        sema_error_at(c, e->pos, "`%s` has no case `%.*s`", sema_tn(v),
                      (int)name->length, name->text);
        return sema_builtin(c, TYPE_ERROR);
    }
    payload = v->params[index];
    e->as.struct_lit.variant_case = (uint32_t)(index + 1);
    sema_format_to(written, sizeof written, "%s.%.*s", sema_tn(v),
                   (int)name->length,
                   name->text);
    return sema_check_field_inits(c, e, e->as.struct_lit.fields,
                                  e->as.struct_lit.field_count,
                                  payload != NULL ? payload->fields : NULL,
                                  payload != NULL ? payload->field_count : 0,
                                  written, false)
               ? v
               : sema_builtin(c, TYPE_ERROR);
}

static struct type *check_type_member(struct checker *c, struct expr *e,
                                      struct type *t)
{
    struct name *name = &e->as.field.name;
    struct item *m = reached_member(t, name);
    const struct struct_field *f;

    if (t->kind == TYPE_VARIANT) {
        return variant_case_value(c, e, t);
    }
    if (t->kind == TYPE_ENUM && (f = sema_find_field(t, name)) != NULL) {
        e->as.field.enum_value = (uint32_t)(f - t->fields) + 1;
        return t;
    }
    if (ambiguous_member(c, e->pos, t, name, "the name")) {
        return sema_builtin(c, TYPE_ERROR);
    }
    if (m == NULL) {
        sema_error_at(c, e->pos, "`%s` has no function `%.*s`", sema_tn(t),
                      (int)name->length, name->text);
        return sema_builtin(c, TYPE_ERROR);
    }
    if (m->symbol == NULL) {
        return sema_builtin(c, TYPE_ERROR);
    }
    /* DESIGN: a static field is a global of the class and not a
       constant. Its value is written into the data of the module and
       never folded into a use. The value is still evaluated here,
       because a global holds its bytes before the program runs. */
    if (m->kind == ITEM_CONST && !sema_const_symbol(c, m->symbol, e->pos)) {
        return sema_builtin(c, TYPE_ERROR);
    }
    if (m->is_static && m->atomic && !c->atomic_place) {
        sema_error_at(c, e->pos,
                      "`%.*s` is atomic, so it is read with `load()` "
                      "and written with `store(v)`", (int)name->length,
                      name->text);
        return sema_builtin(c, TYPE_ERROR);
    }
    /* DESIGN: `T.f` names the body of T. A body qualified by an
       interface is reached through the table of its sub-object instead,
       as a call on the class reaches it. A class of a library file has
       no item that owns its members, so the level is found on the
       chain. */
    if (refuse_abstract_call(c, e->pos, t, m)) {
        return sema_builtin(c, TYPE_ERROR);
    }
    if (m->kind == ITEM_FN && types_member_level(t, m) != NULL &&
        types_body_table(types_member_level(t, m), m) == BODY_INTERFACE) {
        e->as.field.through = table_sub_object(t, m);
    }
    e->symbol = m->symbol;
    if (m->runtime != NULL && sema_name_is(&m->name, ROOT_DESERIALIZE) &&
        m->symbol->type != NULL) {
        struct type *fn = sema_deserialize_type(c, e->pos, m->symbol->type);
        return fn != NULL ? fn : sema_builtin(c, TYPE_ERROR);
    }
    return m->symbol->type != NULL ? m->symbol->type
                                   : sema_builtin(c, TYPE_ERROR);
}

struct type *sema_check_field(struct checker *c, struct expr *e)
{
    const struct symbol *module = qualifier(c, e);
    const struct expr *saved_base;
    struct type *base;
    struct name *name = &e->as.field.name;
    struct type *s;
    const struct struct_field *f;

    if (module != NULL) {
        return check_qualified(c, e, module, false);
    }
    if (e->as.field.base->kind == EXPR_NAME) {
        struct symbol *sym = sema_lookup(c, &e->as.field.base->as.name);
        if (sym != NULL && sym->kind == SYMBOL_STRUCT) {
            /* `Result<int, str>.Missing` names a case of a copy. */
            struct type *t = sym->item != NULL && sym->item->kind == ITEM_TYPE
                                 ? sema_alias_type(c, sym)
                                 : sym->type;
            t = sema_generic_named(c, e->as.field.base, t,
                                   &e->as.field.base->as.name, NULL);
            if (sema_is_error(t)) {
                return t;
            }
            return check_type_member(c, e, t);
        }
        /* The root reaches its namespace by its name as it does as a
           type, for `Object.deserialize`. */
        if (sym == NULL &&
            sema_name_is(&e->as.field.base->as.name, LANG_OBJECT)) {
            return check_type_member(c, e, types_object(c->types));
        }
    }
    /* A type of another module reaches its namespace as well, so
       `m.Kind.Round` and `m.T.f` read like the unqualified forms. */
    if (e->as.field.base->kind == EXPR_FIELD) {
        const struct symbol *outer = qualifier(c, e->as.field.base);
        struct symbol *sym =
            outer != NULL
                ? sema_library_item(c, outer->home,
                                    &e->as.field.base->as.field.name)
                : NULL;
        if (sym != NULL && sym->kind == SYMBOL_STRUCT) {
            return check_type_member(c, e, sym->type);
        }
    }
    saved_base = c->field_base;
    c->field_base = e->as.field.base;
    base = sema_check_expr(c, e->as.field.base, NULL);
    c->field_base = saved_base;
    if (sema_is_error(base)) {
        return base;
    }
    base = sema_usable_pointer(c, e->as.field.base, base);
    if (types_is_flags(base) && e->as.field.base->kind == EXPR_NAME &&
        e->as.field.base->symbol != NULL &&
        (f = sema_find_field(base, name)) != NULL) {
        e->as.field.base->symbol->flags_read |=
            (uint8_t)(1u << (f - base->fields));
    }
    if ((s = sema_struct_of(base)) != NULL) {
        /* DESIGN: a variant gives its tag as a field, and `switch` alone
           reads the fields of its cases. The union `u` is the header's,
           and no program names it. */
        if (s->kind == TYPE_VARIANT && !sema_name_is(name, VARIANT_TAG)) {
            sema_error_at(c, e->pos, "a variant has the field `tag` alone, and "
                          "`switch` reads the fields of its cases");
            return sema_builtin(c, TYPE_ERROR);
        }
        if (types_is_match(s) && sema_find_field(s, name) == NULL) {
            return sema_match_field(c, e, s);
        }
        if ((f = sema_find_field(s, name)) == NULL && e->as.field.element) {
            /* `t.0` names the element `_0`, so the message names the
               number the program wrote. */
            if (s->kind != TYPE_TUPLE) {
                sema_error_at(c, e->pos, "`%s` is not a tuple, so it has no "
                              "element `%.*s`", sema_tn(s),
                              (int)name->length - 1,
                              name->text + 1);
            } else {
                sema_error_at(c, e->pos, "`%s` has %d elements, and `%.*s` is "
                              "none of them", sema_tn(s), (int)s->field_count,
                              (int)name->length - 1, name->text + 1);
            }
            return sema_builtin(c, TYPE_ERROR);
        }
        if (f == NULL) {
            const struct item *m = reached_member(s, name);
            bool ambiguous = false;
            const struct struct_field *through;
            if (ambiguous_member(c, e->pos, s, name, "the name")) {
                return sema_builtin(c, TYPE_ERROR);
            }
            /* A function of the chain wins over a name that a field
               promotes, as a call finds it. One without an entry of
               the primary table is bound through its sub-object. */
            through = m != NULL && m->kind == ITEM_FN
                          ? NULL
                          : promoting_field(c, s, name, &ambiguous);
            if (ambiguous) {
                return sema_builtin(c, TYPE_ERROR);
            }
            if (through == NULL &&
                (base->kind == TYPE_POINTER ||
                 names_sub_object(e->as.field.base))) {
                through = table_sub_object(s, m);
            }
            if (through != NULL) {
                promote_base(c, e, through);
                return sema_check_field(c, e);
            }
            /* DESIGN: a public function named on a value and not
               called is a bound function. It holds the object and the
               entry of its table. Its type is the signature without
               `self`, and calling it needs no receiver. */
            if (m != NULL && m->kind == ITEM_FN && m->pub &&
                m->symbol != NULL && m->symbol->type != NULL &&
                m->symbol->type->kind == TYPE_FN &&
                m->symbol->type->param_count > 0) {
                e->symbol = m->symbol;
                return types_bound_of(
                    c->types,
                    sema_member_type(c, m->symbol->type, s));
            }
            /* DESIGN: a constant of a body is reached as `T.N`, never
               through a value, so a constant is never mistaken for a
               field. */
            if (m != NULL && m->kind == ITEM_CONST) {
                sema_error_at(c, e->pos,
                              "`%.*s` is a constant of `%s`, reached as "
                              "`%s.%.*s`", (int)name->length, name->text,
                              sema_tn(s),
                              sema_tn(s), (int)name->length, name->text);
                return sema_builtin(c, TYPE_ERROR);
            }
            refuse_field(c, e, s, name);
            return sema_builtin(c, TYPE_ERROR);
        }
        /* A field the checker wrote to reach a base or a promoted name
           carries no level of its own. */
        if (!e->as.field.promoted && !field_visible(c, s, f)) {
            sema_error_at(c, e->pos, "`%.*s` is %s `%s`", (int)name->length,
                          name->text,
                          f->vis == VIS_PROTECTED ? "protected in"
                                                  : "private to",
                          sema_tn(s));
            return sema_builtin(c, TYPE_ERROR);
        }
        /* DESIGN: an atomic field is read and written by its own calls
           alone, so that every access is one operation of the memory
           model. A mention of it anywhere else is refused. */
        if (f->atomic && !c->atomic_place) {
            sema_error_at(c, e->pos, "`%.*s` is atomic, so it is read with "
                          "`load()` and written with `store(v)`",
                          (int)name->length, name->text);
            return sema_builtin(c, TYPE_ERROR);
        }
        if (!sema_check_reach(c, e, f)) {
            return sema_builtin(c, TYPE_ERROR);
        }
        return f->type;
    }
    if (sema_name_is(name, "len") && (base->kind == TYPE_STR ||
                                      base->kind == TYPE_SLICE ||
                                      base->kind == TYPE_ARRAY)) {
        return sema_builtin(c, TYPE_I64);
    }
    /* The `ptr` of a str and of a slice is `?*T` for the same reason:
       neither holds an address when it holds no bytes. */
    if (sema_name_is(name, "ptr") && base->kind == TYPE_STR) {
        return types_pointer_nullable(c->types, sema_builtin(c, TYPE_U8));
    }
    if (sema_name_is(name, "ptr") && base->kind == TYPE_SLICE) {
        struct type *ptr = types_pointer_nullable(c->types, base->element);
        return base->lent ? types_lent(c->types, ptr) : ptr;
    }
    if (e->as.field.element) {
        sema_error_at(c, e->pos, "`%s` is not a tuple, so it has no element "
                      "`%.*s`", sema_tn(base), (int)name->length - 1,
                      name->text + 1);
        return sema_builtin(c, TYPE_ERROR);
    }
    refuse_field(c, e, base, name);
    return sema_builtin(c, TYPE_ERROR);
}

/* The fields of a struct or slice literal against the fields of type s.
   Every field appears exactly once, and a union literal names one field,
   which skip_missing allows. */
/* DESIGN: a class literal names the fields of the whole chain directly,
   in any order, and never writes the base as a nested value. The checker
   flattens the chain into one list, base first, and checks the literal
   against it. The base and the table pointer are left out, because no
   literal names them and lowering writes them. */
size_t sema_chain_fields(const struct type *t, struct struct_field *out)
{
    size_t count = 0;
    size_t i;

    if (t == NULL) {
        return 0;
    }
    if (t->kind == TYPE_CLASS) {
        count = sema_chain_fields(t->base, out);
    }
    for (i = 0; i < t->field_count; i++) {
        if (t->fields[i].form == FIELD_BASE ||
            t->fields[i].form == FIELD_TABLE || t->fields[i].hidden) {
            continue;
        }
        if (out != NULL) {
            out[count] = t->fields[i];
        }
        count++;
    }
    return count;
}

/* DESIGN: a literal outside the class names its public fields alone. A
   private or protected field then takes its default, so the value is
   still complete. A literal inside the class may name any field. */
bool sema_check_field_inits(struct checker *c, struct expr *e,
                            struct field_init *inits, size_t count,
                            const struct struct_field *fields,
                            size_t field_count, const char *type_name,
                            bool skip_missing)
{
    bool ok = true;
    size_t i;
    size_t j;

    for (i = 0; i < count; i++) {
        const struct struct_field *f = NULL;
        for (j = 0; j < field_count; j++) {
            if (sema_same_name(&fields[j].name, &inits[i].name) &&
                !type_field_is_unit_break(&fields[j])) {
                f = &fields[j];
            }
        }
        if (f == NULL) {
            sema_error_at(c, inits[i].pos, "`%s` has no field `%.*s`",
                          type_name,
                          (int)inits[i].name.length, inits[i].name.text);
            ok = false;
            continue;
        }
        for (j = 0; j < i; j++) {
            if (sema_same_name(&inits[j].name, &inits[i].name)) {
                sema_error_at(c, inits[i].pos, "the field `%.*s` appears twice",
                              (int)inits[i].name.length, inits[i].name.text);
                ok = false;
            }
        }
        if (f->home != NULL && f->home->kind == TYPE_CLASS &&
            !field_visible(c, f->home, f)) {
            sema_error_at(c, inits[i].pos, "`%.*s` is %s `%s`",
                          (int)inits[i].name.length, inits[i].name.text,
                          f->vis == VIS_PROTECTED ? "protected in"
                                                  : "private to",
                          sema_tn(f->home));
            ok = false;
        }
        /* DESIGN: the provider of an interface fills an `inject` field,
           so a literal that names it would be overwritten. */
        if (f->injected) {
            sema_error_at(c, inits[i].pos,
                          "`%.*s` is `inject`, and its provider "
                          "fills it", (int)inits[i].name.length,
                          inits[i].name.text);
            ok = false;
        }
        if (sema_require(c, inits[i].value,
                         sema_check_expr(c, inits[i].value, f->type),
                         f->type)) {
            sema_refuse_owned_copy(c, inits[i].value, f->type);
        } else {
            ok = false;
        }
    }
    if (!ok || skip_missing) {
        return ok;
    }
    for (j = 0; j < field_count; j++) {
        /* An interface sub-object is the compiler's field. It carries a
           table pointer that the literal never writes, so a literal that
           leaves it out is complete. A field of the error type has had
           its message already. */
        if (type_field_is_unit_break(&fields[j]) ||
            fields[j].form == FIELD_IMPL || fields[j].injected ||
            sema_is_error(fields[j].type)) {
            continue;
        }
        for (i = 0; i < count; i++) {
            if (sema_same_name(&fields[j].name, &inits[i].name)) {
                break;
            }
        }
        /* DESIGN: a field with a default may be left out of a literal.
           The lowering writes the default in its place, so the value is
           complete and the layout is unchanged. */
        if (i == count && fields[j].value == NULL &&
            fields[j].constant == NULL &&
            !sema_field_takes_literal(&fields[j])) {
            sema_error_at(c, e->pos,
                          "the literal of `%s` misses the field `%.*s`",
                          type_name, (int)fields[j].name.length,
                          fields[j].name.text);
            return false;
        }
    }
    return true;
}

/* DESIGN: the safety of `parallel` rests on the types. The runtime does
   the splitting, so a worker reaches one contiguous slice of the array
   and nothing else. Its parameters and its result are pointer-free, so
   no worker can reach memory that another one writes. The rule is no
   data race, not memory safety. */
static bool worker_type(struct checker *c, struct pos pos, const char *what,
                        struct type *t)
{
    if (sema_is_error(t)) {
        return false;
    }
    if (!type_pointer_free(t)) {
        sema_error_at(c, pos,
                      "%s has type `%s`, which holds a pointer. A worker "
                      "takes and returns values alone", what, sema_tn(t));
        return false;
    }
    return true;
}

/* The function type of the worker that the call at slot of `parallel`
   or `dispatch` names. It has one parameter before the arguments of the
   call, for the chunk or the object that first names, of type given. A
   generic worker gives the signature of its copy. NULL after an error. */
static struct type *worker_callee(struct checker *c, struct expr **slot,
                                  const char *form, const char *first,
                                  struct type *given)
{
    struct expr *call = *slot;
    struct expr *callee = call->kind == EXPR_CALL ? call->as.call.callee : call;
    size_t arg_count = call->kind == EXPR_CALL ? call->as.call.arg_count : 0;
    const struct expr *outer_callee = c->callee;
    struct symbol *sym;
    struct type *fn;

    if (callee->kind != EXPR_NAME) {
        sema_error_at(c, callee->pos, "`%s` runs a worker named here", form);
        return NULL;
    }
    sym = sema_lookup(c, &callee->as.name);
    callee->symbol = sym;
    c->callee = callee;
    fn = sema_check_expr(c, callee, NULL);
    c->callee = outer_callee;
    if (sema_is_error(fn)) {
        return NULL;
    }
    if (fn->kind != TYPE_FN || sym == NULL || !sym->worker) {
        sema_error_at(c, callee->pos, "`%.*s` is not a `worker fn`",
                      (int)callee->as.name.length, callee->as.name.text);
        return NULL;
    }
    if (sym->item == NULL || sym->item->type_param_count == 0) {
        if (callee->type_arg_count > 0) {
            sema_refuse_type_args(c, callee, &callee->as.name);
            return NULL;
        }
    } else if (fn->param_count == arg_count + 1) {
        /* The copy is recorded on a call, so a worker named without
           arguments becomes a call of none. */
        if (call->kind != EXPR_CALL) {
            call = sema_new_node(c, EXPR_CALL, callee->pos);
            call->as.call.callee = callee;
            *slot = call;
        }
        fn = sema_worker_copy(c, call, fn, sym, given);
        if (fn == NULL) {
            return NULL;
        }
        callee->type = fn;
    }
    if (fn->param_count != arg_count + 1) {
        sema_error_at(c, call->pos,
                      "`%.*s` takes %zu argument%s beside the %s, "
                      "found %zu", (int)callee->as.name.length,
                      callee->as.name.text, fn->param_count - 1,
                      fn->param_count == 2 ? "" : "s", first, arg_count);
        return NULL;
    }
    return fn;
}

/* Check the result and the arguments of the call of the worker fn, each
   a value without a pointer, and give the call its type. */
static bool worker_args(struct checker *c, struct expr *call,
                        const struct type *fn)
{
    struct expr *callee = call->kind == EXPR_CALL ? call->as.call.callee : call;
    struct expr **args = call->kind == EXPR_CALL ? call->as.call.args : NULL;
    size_t arg_count = call->kind == EXPR_CALL ? call->as.call.arg_count : 0;
    bool ok = worker_type(c, callee->pos, "the result of a worker",
                          fn->result);
    size_t i;

    /* DESIGN: a worker may take a function value as a parameter. Every
       thread shares the code of a function. A closure reaches a worker
       only through a `concurrent` parameter. The checker has proved such
       a closure safe to call from more than one thread at once. */
    for (i = 0; i < arg_count; i++) {
        const struct type *param = fn->params[i + 1];
        ok = sema_require(c, args[i],
                          sema_check_expr(c, args[i], fn->params[i + 1]),
                          fn->params[i + 1]) && ok;
        sema_refuse_lock_copy(c, args[i], param);
        sema_refuse_worker_closure(c, args[i], param);
        /* DESIGN: a worker may take a pointer to a thread-safe object,
           the one exception to the rule that its parameters hold no
           pointer. A Mutex counts, since a worker cannot take a copy of
           one. */
        if (param->kind == TYPE_POINTER && sema_thread_safe(param->element) &&
            !types_is_chan(param->element)) {
            continue;
        }
        if (param->kind != TYPE_FN || param->bound) {
            ok = worker_type(c, args[i]->pos, "an argument of a worker",
                             fn->params[i + 1]) && ok;
        }
    }
    if (call->kind == EXPR_CALL) {
        call->type = fn->result;
    }
    return ok;
}

/* Check `parallel a by n -> f(x)`. It splits a into n chunks and runs f
   on each through the worker pool. The results come back in chunk
   order. */
struct type *sema_check_parallel(struct checker *c, struct expr *e)
{
    struct expr *call = e->as.parallel.call;
    struct expr *callee = call->kind == EXPR_CALL ? call->as.call.callee : call;
    struct type *array = sema_check_expr(c, e->as.parallel.array, NULL);
    struct type *fn;
    bool ok = true;

    if (e->as.parallel.chunks != NULL) {
        struct expr *n = e->as.parallel.chunks;
        ok = sema_require(c, n,
                          sema_check_expr(c, n, sema_builtin(c, TYPE_I64)),
                          sema_builtin(c, TYPE_I64));
    }
    if (sema_is_error(array)) {
        return sema_builtin(c, TYPE_ERROR);
    }
    if (array->kind != TYPE_SLICE) {
        sema_error_at(c, e->as.parallel.array->pos,
                      "`parallel` splits a slice, and this is `%s`",
                      sema_tn(array));
        return sema_builtin(c, TYPE_ERROR);
    }
    fn = worker_callee(c, &e->as.parallel.call, "parallel", "chunk", array);
    if (fn == NULL) {
        return sema_builtin(c, TYPE_ERROR);
    }
    call = e->as.parallel.call;
    if (fn->params[0]->kind != TYPE_SLICE ||
        fn->params[0]->element != array->element) {
        sema_error_at(c, callee->pos, "`%.*s` takes `%s` as its chunk, and the "
                      "slice is `%s`", (int)callee->as.name.length,
                      callee->as.name.text, sema_tn(fn->params[0]),
                      sema_tn(array));
        return sema_builtin(c, TYPE_ERROR);
    }
    ok = worker_type(c, callee->pos, "the chunk", array->element) && ok;
    ok = worker_args(c, call, fn) && ok;
    return ok ? types_slice(c->types, fn->result) : sema_builtin(c, TYPE_ERROR);
}

/* DESIGN: `dispatch obj -> f(args)` gives one object to the pool for a
   `worker fn` whose first parameter is a pointer to the object's class.
   The other arguments follow the pointer-free rule of `parallel`, and the
   expression gives a Job of the worker's result. */
struct type *sema_check_dispatch(struct checker *c, struct expr *e)
{
    struct expr *call = e->as.dispatch.call;
    struct expr *callee = call->kind == EXPR_CALL ? call->as.call.callee : call;
    struct type *object = sema_check_expr(c, e->as.dispatch.object, NULL);
    struct type *fn;

    if (sema_is_error(object)) {
        return sema_builtin(c, TYPE_ERROR);
    }
    if (object->kind != TYPE_POINTER || object->element->kind != TYPE_CLASS) {
        sema_error_at(c, e->as.dispatch.object->pos,
                      "`dispatch` submits a class pointer, and this is `%s`",
                      sema_tn(object));
        return sema_builtin(c, TYPE_ERROR);
    }
    fn = worker_callee(c, &e->as.dispatch.call, "dispatch", "object", object);
    if (fn == NULL) {
        return sema_builtin(c, TYPE_ERROR);
    }
    call = e->as.dispatch.call;
    if (fn->params[0] != object) {
        sema_error_at(c, callee->pos,
                      "`%.*s` takes `%s` as its object, and this is "
                      "`%s`", (int)callee->as.name.length, callee->as.name.text,
                      sema_tn(fn->params[0]), sema_tn(object));
        return sema_builtin(c, TYPE_ERROR);
    }
    return worker_args(c, call, fn) ? types_job(c->types, fn->result)
                                    : sema_builtin(c, TYPE_ERROR);
}

/* `join(job)` waits for one job and gives its result. `join_all(jobs)`
   waits for a slice of jobs and gives nothing. */
struct type *sema_check_join(struct checker *c, struct expr *e)
{
    struct type *t = sema_check_expr(c, e->as.join.job, NULL);
    const char *what = e->as.join.all ? "join_all" : "join";
    struct type *job = e->as.join.all && t->kind == TYPE_SLICE ? t->element : t;

    if (sema_is_error(t)) {
        return t;
    }
    if (e->as.join.all && t->kind != TYPE_SLICE) {
        sema_error_at(c, e->as.join.job->pos, "`join_all` waits for a slice of "
                      "jobs, and this is `%s`", sema_tn(t));
        return sema_builtin(c, TYPE_ERROR);
    }
    if (!types_is_job(job)) {
        sema_error_at(c, e->as.join.job->pos,
                      "`%s` waits for a job of `dispatch`, "
                      "and this is `%s`", what, sema_tn(t));
        return sema_builtin(c, TYPE_ERROR);
    }
    return e->as.join.all ? sema_builtin(c, TYPE_VOID) : job->result;
}

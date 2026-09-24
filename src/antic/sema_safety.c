/* The checks of the thread-safe classes: a synchronized class, whose
   fields its own functions alone reach, and a concurrent class, whose
   every field is guarded, atomic or fixed. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sema_checker.h"

/* The class a function of a body belongs to, or NULL. */
static const struct type *class_of(const struct item *fn)
{
    if (fn == NULL || fn->owner == NULL || fn->owner->symbol == NULL) {
        return NULL;
    }
    return fn->owner->symbol->type;
}

static bool is_lifecycle(const struct item *fn)
{
    return fn != NULL && (sema_name_is(&fn->name, "construct") ||
                          sema_name_is(&fn->name, "destruct"));
}

/* Whether the named function around the code checked now is `construct`
   or `destruct` of home or of a class that inherits it. No other thread
   sees the object then. */
static bool in_lifecycle_of(const struct checker *c, const struct type *home)
{
    const struct item *fn = sema_named_function(c);
    const struct type *t = class_of(fn);

    return is_lifecycle(fn) && t != NULL && sema_descends_from(t, home);
}

void sema_safety_declare(struct checker *c, struct item *it)
{
    struct type *t = it->symbol->type;
    const struct item *outer;

    if (it->synchronized) {
        t->safety = SAFETY_SYNCHRONIZED;
    } else if (it->concurrent) {
        t->safety = SAFETY_CONCURRENT;
    }
    t->unchecked_fields = it->unchecked_fields;
    /* A type nested in a concurrent class has its fields checked as the
       class has. */
    for (outer = it->outer; outer != NULL && t->safety == SAFETY_NONE;
         outer = outer->outer) {
        if (outer->concurrent) {
            t->safety = SAFETY_CONCURRENT;
            t->unchecked_fields = t->unchecked_fields ||
                                  outer->unchecked_fields;
        }
    }
    if (it->unchecked_fields && !it->concurrent) {
        sema_error_at(c, it->name_pos, "`unchecked(unguarded-field)` in a "
                      "class header belongs to a concurrent class");
    }
}

/* DESIGN: a thread-safe class inherits the root, an abstract class that
   declares no field, or a class of its own kind, and a class that
   inherits a thread-safe one is of its kind as well. The fields of a
   base of another kind would stand outside the lock or the check. */
void sema_safety_base(struct checker *c, const struct item *it,
                      const struct type *base)
{
    const struct type *t = it->symbol->type;
    size_t i;
    bool fields = false;

    if (base == NULL || base->base == NULL) {
        return;
    }
    for (i = 0; i < base->field_count; i++) {
        if (base->fields[i].form != FIELD_BASE &&
            base->fields[i].form != FIELD_TABLE) {
            fields = true;
        }
    }
    if (base->safety != SAFETY_NONE && base->safety != t->safety) {
        sema_error_at(c, it->base_pos, "`%.*s` inherits the %s class `%s`, "
                      "and is written `%s class`",
                      (int)it->name.length, it->name.text,
                      base->safety == SAFETY_SYNCHRONIZED ? "synchronized"
                                                          : "concurrent",
                      sema_tn(base),
                      base->safety == SAFETY_SYNCHRONIZED ? "synchronized"
                                                          : "concurrent");
    } else if (t->safety != SAFETY_NONE && base->safety == SAFETY_NONE &&
               (fields || !base->has_abstract)) {
        sema_error_at(c, it->base_pos, "the %s class `%.*s` inherits `%s`, "
                      "which is neither of its kind nor an abstract class "
                      "without fields",
                      t->safety == SAFETY_SYNCHRONIZED ? "synchronized"
                                                       : "concurrent",
                      (int)it->name.length, it->name.text, sema_tn(base));
    }
}

/* The hidden lock of a synchronized class that no base holds already.
   It is transient, so a copy of the object gets an unlocked one and the
   field list leaves it out. */
bool sema_needs_hidden_lock(const struct item *it)
{
    const struct type *base = it->symbol->type->base;

    return it->synchronized &&
           (base == NULL || base->safety != SAFETY_SYNCHRONIZED);
}

void sema_hidden_lock(struct checker *c, const struct item *it,
                      struct struct_field *f)
{
    static const char name_text[] = HIDDEN_LOCK;

    memset(f, 0, sizeof *f);
    f->name.text = name_text;
    f->name.length = sizeof name_text - 1;
    f->pos = it->name_pos;
    f->form = FIELD_PLAIN;
    f->vis = VIS_PRIVATE;
    f->transient = true;
    f->hidden = true;
    f->type = types_object_lock(c->types);
}

/* The class named `name` among the classes whose bodies declare it, the
   item itself first. */
static const struct item *enclosing_named(const struct item *it,
                                          const struct name *name)
{
    for (; it != NULL; it = it->outer) {
        const struct name *own = it->local_name.length > 0 ? &it->local_name
                                                           : &it->name;
        if (it->kind == ITEM_CLASS && sema_same_name(own, name)) {
            return it;
        }
    }
    return NULL;
}

/* DESIGN: `guarded by lock` names a Mutex field of the same object, and
   `guarded by PeopleList.lock` one of an enclosing object, named by its
   class. A field of a concurrent class or of a type nested in one takes
   it. The check runs once every field of the module is declared, since
   a nested type stands before the class that declares it. */
static void check_guards_of(struct checker *c, struct item *it)
{
    struct type *t = it->symbol->type;
    size_t i;

    for (i = 0; i < it->param_count; i++) {
        const struct param *p = &it->params[i];
        struct struct_field *f;
        const struct type *holder = t;
        const struct struct_field *lock;
        if (p->guard.length == 0) {
            continue;
        }
        f = (struct struct_field *)sema_find_field(t, &p->name);
        if (f == NULL) {
            continue;
        }
        if (t->safety != SAFETY_CONCURRENT) {
            sema_error_at(c, p->guard_pos, "`guarded by` marks a field of a "
                          "concurrent class or of a type nested in one");
            continue;
        }
        if (p->guard_class.length > 0) {
            const struct item *by = enclosing_named(it, &p->guard_class);
            if (by == NULL || by->symbol == NULL) {
                sema_error_at(c, p->guard_pos, "`%.*s` is no class around "
                              "`%.*s`", (int)p->guard_class.length,
                              p->guard_class.text, (int)p->name.length,
                              p->name.text);
                continue;
            }
            holder = by->symbol->type;
            f->guard_class = holder == t ? NULL : holder;
        }
        for (lock = NULL; holder != NULL && lock == NULL;
             holder = holder->kind == TYPE_CLASS ? holder->base : NULL) {
            lock = sema_find_field(holder, &p->guard);
        }
        if (lock == NULL || !types_is_mutex(lock->type)) {
            sema_error_at(c, p->guard_pos, "`%.*s` is no `" LANG_MUTEX
                          "` field of `%s`", (int)p->guard.length,
                          p->guard.text,
                          sema_tn(f->guard_class != NULL ? f->guard_class
                                                         : t));
            continue;
        }
        f->guard = p->guard;
        if (f->atomic) {
            sema_error_at(c, p->guard_pos, "`%.*s` is atomic and needs no "
                          "lock", (int)p->name.length, p->name.text);
        }
    }
}

void sema_check_guards(struct checker *c)
{
    size_t i;

    for (i = 0; i < c->module->item_count; i++) {
        struct item *it = c->module->items[i];
        if (it->symbol != NULL && it->symbol->type != NULL &&
            (it->kind == ITEM_CLASS || it->kind == ITEM_STRUCT)) {
            size_t j;
            check_guards_of(c, it);
            for (j = 0; j < it->param_count; j++) {
                struct struct_field *f = (struct struct_field *)
                    sema_find_field(it->symbol->type, &it->params[j].name);
                if (f != NULL) {
                    f->unchecked = it->params[j].unchecked;
                }
            }
        }
    }
}

/* The place e names with its `&` and `*` taken off. */
static const struct expr *bare(const struct expr *e)
{
    while (e->kind == EXPR_UNARY && (e->as.unary.op == TOKEN_AMP ||
                                     e->as.unary.op == TOKEN_STAR)) {
        e = e->as.unary.operand;
    }
    return e;
}

/* Whether a and b name one object: the same variable, or the same path
   of fields from it. */
static bool same_object(const struct expr *a, const struct expr *b)
{
    a = bare(a);
    b = bare(b);
    if (a->kind == EXPR_NAME && b->kind == EXPR_NAME) {
        return a->symbol != NULL && a->symbol == b->symbol;
    }
    if (a->kind == EXPR_FIELD && b->kind == EXPR_FIELD) {
        return sema_same_name(&a->as.field.name, &b->as.field.name) &&
               same_object(a->as.field.base, b->as.field.base);
    }
    return false;
}

/* Whether a `sync` around the code checked now holds the lock of field f
   of the object that e, a field access, reaches. */
static bool guard_held(const struct checker *c, const struct expr *e,
                       const struct struct_field *f)
{
    const struct held_mutex *h;

    for (h = c->held; h != NULL; h = h->outer) {
        const struct expr *m = bare(h->mutex);
        const struct type *owner;
        if (m->kind != EXPR_FIELD || !sema_same_name(&m->as.field.name,
                                                     &f->guard)) {
            continue;
        }
        if (f->guard_class == NULL) {
            if (same_object(m->as.field.base, e->as.field.base)) {
                return true;
            }
            continue;
        }
        owner = m->as.field.base->type;
        if (owner != NULL && owner->kind == TYPE_POINTER) {
            owner = owner->element;
        }
        if (owner != NULL && sema_descends_from(owner, f->guard_class)) {
            return true;
        }
    }
    return false;
}

/* DESIGN: a field of a synchronized class is reached by the functions of
   the class alone, which run under its lock, and by `construct` and
   `destruct`, which no other thread sees. A guarded field is reached
   inside a `sync` on its lock, apart from `construct` and `destruct`.
   Returns false after an error. */
bool sema_check_reach(struct checker *c, const struct expr *e,
                      const struct struct_field *f)
{
    const struct type *home = f->home;
    const struct item *fn = sema_named_function(c);
    const struct type *mine = class_of(fn);

    if (home == NULL || f->hidden) {
        return true;
    }
    if (home->safety == SAFETY_SYNCHRONIZED &&
        (mine == NULL || !sema_descends_from(mine, home) ||
         (!fn->has_self && !is_lifecycle(fn)))) {
        sema_error_at(c, e->pos, "`%.*s` is a field of the synchronized "
                      "class `%s`, which its own functions reach alone",
                      (int)f->name.length, f->name.text, sema_tn(home));
        return false;
    }
    if (f->guard.length > 0 && !guard_held(c, e, f) &&
        !(home->kind == TYPE_CLASS && in_lifecycle_of(c, home)) &&
        !(f->guard_class != NULL && in_lifecycle_of(c, f->guard_class))) {
        sema_check_at(c, NAME_UNGUARDED_FIELD, e->pos,
                      "`%.*s` is guarded by `%.*s`, and no `sync` holds it "
                      "here", (int)f->name.length, f->name.text,
                      (int)f->guard.length, f->guard.text);
    }
    return true;
}

/* The field of a concurrent class or of a type nested in one that a
   write to e changes, or NULL. A write to an element of an array field
   or to a field of a struct field changes the field. */
static const struct struct_field *written_field(const struct expr *e)
{
    const struct struct_field *found = NULL;

    while (e->kind == EXPR_FIELD || e->kind == EXPR_INDEX) {
        const struct expr *base = e->kind == EXPR_FIELD ? e->as.field.base
                                                        : e->as.index.base;
        if (base->type == NULL) {
            return NULL;
        }
        if (e->kind == EXPR_FIELD) {
            struct type *s = sema_struct_of(base->type);
            const struct struct_field *f =
                s != NULL ? sema_find_field(s, &e->as.field.name) : NULL;
            const struct type *t;
            for (t = s; f == NULL && t != NULL && t->kind == TYPE_CLASS;
                 t = t->base) {
                f = sema_find_field(t, &e->as.field.name);
            }
            if (f != NULL && f->home != NULL &&
                f->home->safety == SAFETY_CONCURRENT) {
                found = f;
            }
        } else if (base->type->kind != TYPE_ARRAY) {
            break;
        }
        if (base->type->kind == TYPE_POINTER || base->type->kind == TYPE_SLICE) {
            break;
        }
        e = base;
    }
    return found;
}

static bool is_free_field(const struct struct_field *f)
{
    return f->guard.length > 0 || f->atomic || f->hidden ||
           f->form != FIELD_PLAIN;
}

/* DESIGN: a field of a concurrent class that is neither guarded nor
   atomic is fixed: `construct` writes it, and nothing after. A write
   anywhere else makes it none of the three. A field this module
   declares is reported where it is declared, once, so `unchecked` after
   its type covers it. A field of another module is reported at the
   write, and a field that carries `unchecked` in its library passes. */
void sema_note_field_write(struct checker *c, const struct expr *e)
{
    const struct struct_field *f = written_field(e);
    struct written_field *w;

    if (f == NULL || is_free_field(f) || in_lifecycle_of(c, f->home)) {
        return;
    }
    if (!sema_same_name(&f->home->module, &c->module_name)) {
        if (f->unchecked || f->home->unchecked_fields) {
            return;
        }
        sema_check_at(c, NAME_UNGUARDED_FIELD, e->pos,
                      "`%.*s` is not guarded, atomic or fixed",
                      (int)f->name.length, f->name.text);
        return;
    }
    if (c->written_count == c->written_capacity) {
        size_t capacity = c->written_capacity == 0 ? 8
                                                   : c->written_capacity * 2;
        w = realloc(c->written, capacity * sizeof *w);
        if (w == NULL) {
            fputs("antic: out of memory\n", stderr);
            exit(70);
        }
        c->written = w;
        c->written_capacity = capacity;
    }
    c->written[c->written_count++].field = f;
}

/* Report each field of the module that a write made none of the three,
   in the order of the declarations. */
void sema_report_unfixed(struct checker *c)
{
    size_t i;
    size_t j;
    size_t k;

    for (i = 0; i < c->module->item_count; i++) {
        const struct item *it = c->module->items[i];
        const struct type *t;
        if (it->symbol == NULL || it->symbol->type == NULL ||
            (it->kind != ITEM_CLASS && it->kind != ITEM_STRUCT)) {
            continue;
        }
        t = it->symbol->type;
        for (j = 0; j < t->field_count; j++) {
            for (k = 0; k < c->written_count; k++) {
                if (c->written[k].field == &t->fields[j]) {
                    sema_check_at(c, NAME_UNGUARDED_FIELD, t->fields[j].pos,
                                  "`%.*s` is not guarded, atomic or fixed",
                                  (int)t->fields[j].name.length,
                                  t->fields[j].name.text);
                    break;
                }
            }
        }
    }
    free(c->written);
    c->written = NULL;
    c->written_count = 0;
    c->written_capacity = 0;
}

/* Pointers into the fields */

/* Whether fn is a function of a thread-safe class that code outside the
   class calls: one with `self` that is not private. */
static const struct type *guarded_api(const struct item *fn)
{
    const struct type *t = class_of(fn);

    if (t == NULL || t->safety == SAFETY_NONE || !fn->has_self ||
        fn->vis == VIS_PRIVATE || is_lifecycle(fn)) {
        return NULL;
    }
    return t;
}

/* Whether e names a place inside the fields of the object that self of
   fn points at: a field of self, or a part of one reached without a
   pointer. */
static bool in_fields(const struct expr *e, const struct item *fn)
{
    bool field = false;

    e = bare(e);
    while (e->kind == EXPR_FIELD || e->kind == EXPR_INDEX) {
        const struct expr *base = e->kind == EXPR_FIELD ? e->as.field.base
                                                        : e->as.index.base;
        if (e->kind == EXPR_FIELD) {
            field = true;
        }
        if (base->type != NULL && base->type->kind == TYPE_SLICE) {
            return false;
        }
        if (base->type != NULL && base->type->kind == TYPE_POINTER &&
            !(base->kind == EXPR_NAME && base->symbol == fn->self)) {
            return false;
        }
        e = base;
    }
    return field && e->kind == EXPR_NAME && e->symbol != NULL &&
           e->symbol == fn->self;
}

/* DESIGN: a value gives out the fields of the object when it is a
   pointer or a slice made from them: `&` of a place in the fields, a
   slice of an array field, an array field that becomes a slice, the
   `ptr` of one, or a slice field, whose elements the object holds. A
   local that took such a value from its `let` gives it too. */
bool sema_points_into_fields(const struct expr *e, const struct item *fn,
                             const struct type *to)
{
    if (e == NULL || fn == NULL || fn->self == NULL) {
        return false;
    }
    switch (e->kind) {
    case EXPR_UNARY:
        return e->as.unary.op == TOKEN_AMP &&
               in_fields(e->as.unary.operand, fn);
    case EXPR_SLICE:
        return in_fields(e->as.slice.base, fn) ||
               sema_points_into_fields(e->as.slice.base, fn, NULL);
    case EXPR_NAME:
        return e->symbol != NULL && e->symbol->into_fields;
    case EXPR_FIELD:
        if (sema_name_is(&e->as.field.name, "ptr") &&
            e->as.field.base->type != NULL &&
            (e->as.field.base->type->kind == TYPE_SLICE ||
             e->as.field.base->type->kind == TYPE_ARRAY)) {
            return sema_points_into_fields(e->as.field.base, fn, NULL) ||
                   in_fields(e->as.field.base, fn);
        }
        if (e->type != NULL && e->type->kind == TYPE_SLICE &&
            in_fields(e, fn)) {
            return true;
        }
        return to != NULL && to->kind == TYPE_SLICE && e->type != NULL &&
               e->type->kind == TYPE_ARRAY && in_fields(e, fn);
    default:
        return false;
    }
}

void sema_check_leak_return(struct checker *c, const struct expr *value,
                            const struct type *result)
{
    const struct item *fn = c->function;
    const struct type *t = guarded_api(fn);

    if (t != NULL && sema_points_into_fields(value, fn, result)) {
        sema_error_at(c, value->pos, "`%.*s` of the thread-safe class `%s` "
                      "gives out a pointer into its fields, return a copy",
                      (int)fn->name.length, fn->name.text, sema_tn(t));
    }
}

void sema_check_leak_arg(struct checker *c, const struct expr *callee,
                         const struct expr *arg, const struct type *param)
{
    const struct item *fn = sema_named_function(c);
    const struct type *t = guarded_api(fn);

    if (t == NULL || callee->kind != EXPR_NAME || callee->symbol == NULL ||
        (callee->symbol->kind != SYMBOL_LOCAL &&
         callee->symbol->kind != SYMBOL_PARAM)) {
        return;
    }
    if (sema_points_into_fields(arg, fn, param)) {
        sema_error_at(c, arg->pos, "`%.*s` of the thread-safe class `%s` "
                      "passes a pointer into its fields to a function "
                      "value, pass a copy", (int)fn->name.length,
                      fn->name.text, sema_tn(t));
    }
}

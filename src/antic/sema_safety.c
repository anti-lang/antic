/* The checks of the thread-safe classes: a synchronized class, whose
   fields its own functions alone reach, and a concurrent class, whose
   every field is guarded, atomic or fixed. */

#include <stdlib.h>
#include <string.h>

#include "alloc.h"
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

void sema_safety_declare(struct item *it)
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
        if (it->kind == ITEM_CLASS && types_same_name(own, name)) {
            return it;
        }
    }
    return NULL;
}

/* The field of t named name, or NULL, as types_find_field finds it, in a
   form the checker may write to. */
static struct struct_field *own_field(struct type *t, const struct name *name)
{
    const struct struct_field *f = types_find_field(t, name);

    return f != NULL ? &t->fields[f - t->fields] : NULL;
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
        f = own_field(t, &p->name);
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
            lock = types_find_field(holder, &p->guard);
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
                struct struct_field *f = own_field(it->symbol->type,
                                                   &it->params[j].name);
                if (f != NULL) {
                    f->unchecked = it->params[j].unchecked;
                }
            }
        }
    }
}

/* The `sync` around the code checked now that holds the lock of field f
   of the object that e, a field access, reaches, or NULL. The blocks
   around an anonymous function do not count, since it may run after
   them. */
static const struct held_mutex *guard_holder(const struct checker *c,
                                             const struct expr *e,
                                             const struct struct_field *f)
{
    const struct held_mutex *h;

    for (h = c->held; h != c->held_outside; h = h->outer) {
        const struct expr *m = sema_bare_place(h->mutex);
        const struct type *owner;
        if (m->kind != EXPR_FIELD || !types_same_name(&m->as.field.name,
                                                     &f->guard)) {
            continue;
        }
        if (f->guard_class == NULL) {
            if (sema_same_place(m->as.field.base, e->as.field.base)) {
                return h;
            }
            continue;
        }
        owner = m->as.field.base->type;
        if (owner != NULL && owner->kind == TYPE_POINTER) {
            owner = owner->element;
        }
        if (owner != NULL && sema_descends_from(owner, f->guard_class)) {
            return h;
        }
    }
    return NULL;
}

/* Whether the code checked now stands in `construct` or `destruct` of
   the class that guards field f, where no other thread sees the object
   and the field is reached without its lock. */
static bool guard_free(const struct checker *c, const struct struct_field *f)
{
    const struct type *home = f->home;

    return (home != NULL && home->kind == TYPE_CLASS &&
            in_lifecycle_of(c, home)) ||
           (f->guard_class != NULL && in_lifecycle_of(c, f->guard_class));
}

/* The access of a guarded field that the place e lies in, with the
   field in *field, or NULL. A place behind a pointer or a slice that the
   object holds lies outside the object, and in no field of it. */
static const struct expr *guarded_access(const struct expr *e,
                                         const struct struct_field **field)
{
    e = sema_bare_place(e);
    while (e->kind == EXPR_FIELD || e->kind == EXPR_INDEX) {
        const struct expr *base = e->kind == EXPR_FIELD ? e->as.field.base
                                                        : e->as.index.base;
        const struct type *s =
            e->kind == EXPR_FIELD ? sema_struct_of(base->type) : NULL;
        const struct struct_field *f =
            s != NULL ? types_find_field(s, &e->as.field.name) : NULL;
        if (f != NULL && f->guard.length > 0) {
            *field = f;
            return e;
        }
        if (base->type == NULL || base->type->kind == TYPE_POINTER ||
            base->type->kind == TYPE_SLICE) {
            return NULL;
        }
        e = base;
    }
    return NULL;
}

/* The access of the guarded field that the value e points into, with
   the field in *field, or NULL: `&` of a place in the field, a slice of
   it, the `ptr` of one, and any of them converted with `as`. */
static const struct expr *guarded_target(const struct expr *e,
                                         const struct struct_field **field)
{
    switch (e->kind) {
    case EXPR_UNARY:
        return e->as.unary.op == TOKEN_AMP
                   ? guarded_access(e->as.unary.operand, field) : NULL;
    case EXPR_SLICE:
        return guarded_access(e->as.slice.base, field);
    case EXPR_CAST:
        return guarded_target(e->as.cast.operand, field);
    case EXPR_FIELD:
        if (sema_name_is(&e->as.field.name, "ptr") &&
            e->as.field.base->type != NULL &&
            (e->as.field.base->type->kind == TYPE_SLICE ||
             e->as.field.base->type->kind == TYPE_ARRAY)) {
            const struct expr *at = guarded_target(e->as.field.base, field);
            return at != NULL ? at : guarded_access(e->as.field.base, field);
        }
        return NULL;
    default:
        return NULL;
    }
}

/* Record that the loop s opens, and count it. */
void sema_loop_opens(struct checker *c, const struct stmt *s)
{
    if (c->loop_depth >= 0 && c->loop_depth < LOOPS_OPEN_MAX) {
        c->loops_open[c->loop_depth] = s;
    }
    c->loop_depth++;
}

/* The loop that began first after the declaration of sym and is open
   now, or NULL: the one whose next turn runs the code before the
   statement checked now with the value sym has here. */
static const struct stmt *loop_around(const struct checker *c,
                                      const struct symbol *sym)
{
    if (sym->frame != c->ctx.function || sym->loops < 0 ||
        sym->loops >= LOOPS_OPEN_MAX || c->loop_depth <= sym->loops) {
        return NULL;
    }
    return c->loops_open[sym->loops];
}

static void refuse_guarded_pointer(struct checker *c, struct pos pos,
                                   const struct symbol *sym,
                                   const char *when)
{
    const struct struct_field *f = sym->guarded_field;

    sema_check_at(c, NAME_UNGUARDED_FIELD, pos,
                  "`%.*s` points at `%.*s`%s, which is guarded by `%.*s`, "
                  "and no `sync` holds it here", (int)sym->name.length,
                  sym->name.text, (int)f->name.length, f->name.text, when,
                  (int)f->guard.length, f->guard.text);
}

/* DESIGN: a pointer to a guarded field reaches the field without its
   lock, so it is used inside the `sync` block that took it and nowhere
   after it, by R092 of "Rules of 2026-10-13". The lock that block holds
   is the one of the object the pointer leads into, whatever the names of
   the place hold later, so a later block on the same place does not
   count. The checker follows the pointer through the variable that
   holds it: a `let` or a `=` gives it `&` of a place in the field, a
   slice of it, its `ptr`, or a copy of a variable that holds one.

   The record is one per variable. A `=` of another value ends it where
   the `=` stands in the block that declares the variable and so runs on
   every path. One in a block inside it leaves the record. A use that
   stands before the `sync` in a loop runs after the block in the next
   turn, so the first use of the variable outside every `sync` of a loop
   is kept and refused once a `sync` of that loop gives it the pointer.

   A pointer taken where no `sync` holds the lock is not followed: in
   `construct` and `destruct` the field is free, and anywhere else the
   reach of the field is refused already. A pointer inside a struct, a
   tuple, a field or a result is not followed either. */
void sema_note_guarded_pointer(struct checker *c, struct symbol *sym,
                               const struct expr *value)
{
    const struct struct_field *f = NULL;
    const struct expr *sync = NULL;
    const struct expr *at;

    if (sym == NULL) {
        return;
    }
    if (value != NULL && value->kind == EXPR_NAME && value->symbol != NULL) {
        sync = value->symbol->guarded_sync;
        f = value->symbol->guarded_field;
    } else if (value != NULL &&
               (at = guarded_target(value, &f)) != NULL) {
        const struct held_mutex *h = guard_holder(c, at, f);
        sync = h != NULL ? h->mutex : NULL;
    }
    if (sync == NULL) {
        /* The block of a function body stands one block inside its
           parameters. */
        int declared = sym->depth + (sym->kind == SYMBOL_PARAM ? 1 : 0);
        if (c->ctx.scope != NULL && c->ctx.scope->depth <= declared &&
            c->deferring == 0 && sym->frame == c->ctx.function) {
            sym->guarded_sync = NULL;
            sym->guarded_field = NULL;
        }
        return;
    }
    sym->guarded_sync = sync;
    sym->guarded_field = f;
    if (sym->free_loop != NULL && sym->free_loop == loop_around(c, sym) &&
        !guard_free(c, f)) {
        refuse_guarded_pointer(c, sym->free_use, sym,
                               " in the next turn of the loop");
    }
}

/* Refuse the use e of the variable sym where it holds a pointer to a
   guarded field outside the `sync` block that took it. A use of a
   variable that holds none, outside every `sync` of a loop, is kept for
   sema_note_guarded_pointer. */
void sema_check_guarded_pointer(struct checker *c, const struct expr *e,
                                struct symbol *sym)
{
    const struct held_mutex *h;

    if (c->assigned == e) {
        return;
    }
    if (sym->guarded_sync == NULL) {
        const struct stmt *loop = loop_around(c, sym);
        if (loop != NULL && c->held == c->held_outside &&
            sym->free_loop != loop) {
            sym->free_loop = loop;
            sym->free_use = e->pos;
        }
        return;
    }
    for (h = c->held; h != c->held_outside; h = h->outer) {
        if (h->mutex == sym->guarded_sync) {
            return;
        }
    }
    if (!guard_free(c, sym->guarded_field)) {
        refuse_guarded_pointer(c, e->pos, sym, "");
    }
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
    if (f->guard.length > 0 && guard_holder(c, e, f) == NULL &&
        !guard_free(c, f)) {
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
                s != NULL ? types_find_field(s, &e->as.field.name) : NULL;
            const struct type *t;
            for (t = s; f == NULL && t != NULL && t->kind == TYPE_CLASS;
                 t = t->base) {
                f = types_find_field(t, &e->as.field.name);
            }
            /* The base of a class is the same object, so `super` on the
               way up leaves the field found below it, T168 of
               docs/audit/tests-findings.md. */
            if (f != NULL && f->home != NULL && f->form != FIELD_BASE &&
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

    if (f == NULL || is_free_field(f) || in_lifecycle_of(c, f->home)) {
        return;
    }
    if (!types_same_name(&f->home->module, &c->module_name)) {
        if (f->unchecked || f->home->unchecked_fields) {
            return;
        }
        sema_check_at(c, NAME_UNGUARDED_FIELD, e->pos,
                      "`%.*s` is not guarded, atomic or fixed",
                      (int)f->name.length, f->name.text);
        return;
    }
    c->written = alloc_grow(c->written, &c->written_capacity,
                            c->written_count, sizeof *c->written);
    c->written[c->written_count++].field = f;
}

/* DESIGN: a field that code outside the class reaches may be written in
   another module, which this one does not see. Its `unchecked` is
   therefore never unused: the field is reported here as well, where
   the clause covers it. */
static bool written_elsewhere(const struct type *t,
                              const struct struct_field *f)
{
    return t->safety == SAFETY_CONCURRENT && f->vis != VIS_PRIVATE &&
           !is_free_field(f) && (f->unchecked || t->unchecked_fields);
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
            for (k = 0; k <= c->written_count; k++) {
                if (k == c->written_count
                        ? written_elsewhere(t, &t->fields[j])
                        : c->written[k].field == &t->fields[j]) {
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

    e = sema_bare_place(e);
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
    size_t i;

    if (e == NULL || fn == NULL || fn->self == NULL) {
        return false;
    }
    switch (e->kind) {
    case EXPR_UNARY:
        return e->as.unary.op == TOKEN_AMP &&
               in_fields(e->as.unary.operand, fn);
    /* A tuple, an array or a struct that holds such a value gives it
       out as the value itself does, by T062 of
       docs/audit/tests-findings.md. */
    case EXPR_TUPLE:
        for (i = 0; i < e->as.tuple.count; i++) {
            const struct type *part =
                to != NULL && to->kind == TYPE_TUPLE && i < to->param_count
                    ? to->params[i]
                    : NULL;
            if (sema_points_into_fields(e->as.tuple.elements[i], fn, part)) {
                return true;
            }
        }
        return false;
    case EXPR_ARRAY_LIT:
        for (i = 0; i < e->as.array_lit.count; i++) {
            const struct type *part =
                to != NULL && to->kind == TYPE_ARRAY ? to->element : NULL;
            if (sema_points_into_fields(e->as.array_lit.elements[i], fn,
                                        part)) {
                return true;
            }
        }
        return false;
    case EXPR_STRUCT_LIT:
        for (i = 0; i < e->as.struct_lit.field_count; i++) {
            const struct field_init *init = &e->as.struct_lit.fields[i];
            const struct type *s =
                e->type != NULL && e->as.struct_lit.variant_case == 0
                    ? sema_struct_of(e->type)
                    : NULL;
            const struct struct_field *f =
                s != NULL ? types_find_field(s, &init->name) : NULL;
            if (sema_points_into_fields(init->value, fn,
                                        f != NULL ? f->type : NULL)) {
                return true;
            }
        }
        return false;
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

/* A `return` of a call of a function of the class on self. */
struct leak_call {
    const struct item *caller;
    const struct item *callee;
    struct pos pos;
};

/* The function of fn's class that value calls on self, or NULL. The
   checker writes `self.inner()` as the call of `Box.inner` with self as
   its first argument. */
static const struct item *self_call(const struct expr *value,
                                    const struct item *fn)
{
    const struct expr *callee;
    const struct expr *object;
    const struct item *it;

    if (value->kind != EXPR_CALL || value->as.call.optional) {
        return NULL;
    }
    callee = value->as.call.callee;
    if (callee->symbol == NULL || callee->symbol->item == NULL) {
        return NULL;
    }
    if (callee->kind == EXPR_FIELD) {
        object = callee->as.field.base;
    } else if (callee->kind == EXPR_NAME && value->as.call.arg_count > 0) {
        object = value->as.call.args[0];
    } else {
        return NULL;
    }
    it = callee->symbol->item;
    object = sema_bare_place(object);
    if (!it->has_self || it->owner != fn->owner ||
        object->kind != EXPR_NAME || object->symbol != fn->self) {
        return NULL;
    }
    return it;
}

static void add_leaking(struct checker *c, const struct item *fn)
{
    size_t i;

    for (i = 0; i < c->leaking_count; i++) {
        if (c->leaking[i] == fn) {
            return;
        }
    }
    c->leaking = alloc_grow(c->leaking, &c->leaking_capacity,
                            c->leaking_count, sizeof *c->leaking);
    c->leaking[c->leaking_count++] = fn;
}

/* DESIGN: a public function of a thread-safe class gives out no pointer
   into the fields, and a private one may hand one to the class itself.
   The return of such a private function from a public one gives the
   pointer out, by T062 of docs/audit/tests-findings.md. A private
   function may stand after the public one that calls it, so each
   `return` of a call on self is kept, and the end of the module reports
   it once the functions that return a pointer into the fields are
   known. */
void sema_check_leak_return(struct checker *c, const struct expr *value,
                            const struct type *result)
{
    const struct item *fn = c->ctx.function;
    const struct type *owner = class_of(fn);
    const struct type *t = guarded_api(fn);
    const struct item *callee;

    if (owner == NULL || owner->safety == SAFETY_NONE || !fn->has_self) {
        return;
    }
    if (sema_points_into_fields(value, fn, result)) {
        if (t != NULL) {
            sema_error_at(c, value->pos, "`%.*s` of the thread-safe class "
                          "`%s` gives out a pointer into its fields, return "
                          "a copy", (int)fn->name.length, fn->name.text,
                          sema_tn(t));
        } else if (c->ctx.quiet == 0) {
            add_leaking(c, fn);
        }
        return;
    }
    callee = self_call(value, fn);
    if (callee != NULL && c->ctx.quiet == 0) {
        c->leak_calls = alloc_grow(c->leak_calls, &c->leak_call_capacity,
                                   c->leak_call_count, sizeof *c->leak_calls);
        c->leak_calls[c->leak_call_count].caller = fn;
        c->leak_calls[c->leak_call_count].callee = callee;
        c->leak_calls[c->leak_call_count].pos = value->pos;
        c->leak_call_count++;
    }
}

/* Whether fn is one of the functions that return a pointer into the
   fields. */
static bool is_leaking(const struct checker *c, const struct item *fn)
{
    size_t i;

    for (i = 0; i < c->leaking_count; i++) {
        if (c->leaking[i] == fn) {
            return true;
        }
    }
    return false;
}

void sema_report_leak_calls(struct checker *c)
{
    bool changed = true;
    size_t i;

    while (changed) {
        changed = false;
        for (i = 0; i < c->leak_call_count; i++) {
            const struct leak_call *k = &c->leak_calls[i];
            if (guarded_api(k->caller) == NULL && is_leaking(c, k->callee) &&
                !is_leaking(c, k->caller)) {
                add_leaking(c, k->caller);
                changed = true;
            }
        }
    }
    for (i = 0; i < c->leak_call_count; i++) {
        const struct leak_call *k = &c->leak_calls[i];
        const struct type *t = guarded_api(k->caller);
        if (t != NULL && is_leaking(c, k->callee)) {
            sema_error_at(c, k->pos, "`%.*s` of the thread-safe class `%s` "
                          "gives out a pointer into its fields, return a "
                          "copy", (int)k->caller->name.length,
                          k->caller->name.text, sema_tn(t));
        }
    }
    free(c->leaking);
    free(c->leak_calls);
    c->leaking = NULL;
    c->leak_calls = NULL;
    c->leaking_count = 0;
    c->leaking_capacity = 0;
    c->leak_call_count = 0;
    c->leak_call_capacity = 0;
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

/* DESIGN: a public function of a thread-safe class gives no pointer into
   its fields away, by R094 of "Rules of 2026-10-13". A `=` that stores
   one outside the object does: through an out parameter, in another
   object or in a global. A place in the fields of the object itself
   keeps it under the lock, and a local or a parameter that takes one
   gives it out as the pointer itself does, so the `=` marks it. */
void sema_check_leak_store(struct checker *c, const struct expr *target,
                           const struct expr *value, const struct type *to)
{
    const struct item *fn = sema_named_function(c);
    const struct type *t = guarded_api(fn);

    if (t == NULL || !sema_points_into_fields(value, fn, to)) {
        return;
    }
    if (target->kind == EXPR_NAME && target->symbol != NULL &&
        (target->symbol->kind == SYMBOL_LOCAL ||
         target->symbol->kind == SYMBOL_PARAM)) {
        target->symbol->into_fields = true;
        return;
    }
    if (in_fields(target, fn)) {
        return;
    }
    sema_error_at(c, value->pos, "`%.*s` of the thread-safe class `%s` "
                  "stores a pointer into its fields outside the object, "
                  "store a copy", (int)fn->name.length, fn->name.text,
                  sema_tn(t));
}

/* DESIGN: a type is thread-safe when it is built to be changed from more
   than one thread at once. Such are a `Mutex`, a channel, a synchronized
   class and a concurrent class. An atomic is a field or a local marked
   `atomic`, so sema_thread_safe_symbol asks the variable as well. */
bool sema_thread_safe(const struct type *t)
{
    return t != NULL &&
           (types_is_mutex(t) || types_is_chan(t) ||
            ((t->kind == TYPE_CLASS || t->kind == TYPE_STRUCT) &&
             t->safety != SAFETY_NONE));
}

bool sema_thread_safe_symbol(const struct symbol *sym)
{
    return sym->atomic || sema_thread_safe(sym->type);
}

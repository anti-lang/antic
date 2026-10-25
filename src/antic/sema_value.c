/* The value rules of the checker: what a value owns, when a value moves
   and when it is copied, and the copies `=`, `let`, a literal and a call
   refuse. Lowering tears down what sema_needs_teardown names. */

#include <stdlib.h>
#include <string.h>

#include "sema_checker.h"

static bool holds_param(const struct type *t);

/* DESIGN: R115 of "Rules of 2026-10-13". The `destroyed` hook of a class
   that replaces it runs at the end of the life of every value, as
   `created` runs at its start. The teardown calls the hook, so a level
   that replaces it asks for a teardown as a `destruct` does. The hook
   takes `self` alone. The `destroyed` of a TraceHandler takes the object
   as well and is no hook of its own class. */
static bool replaces_destroyed(const struct item *m)
{
    static const char destroyed[] = ROOT_DESTROYED;
    size_t params;

    if (m->kind != ITEM_FN || m->runtime != NULL || !sema_has_body(m) ||
        m->name.length != sizeof destroyed - 1 ||
        memcmp(m->name.text, destroyed, sizeof destroyed - 1) != 0) {
        return false;
    }
    params = m->symbol != NULL && m->symbol->type != NULL
                 ? m->symbol->type->param_count
                 : (size_t)m->param_count + (m->has_self ? 1 : 0);
    return m->has_self && params == 1;
}

/* Whether a class of the chain of t declares a field `own`. */
static bool own_field_in_chain(const struct type *t)
{
    size_t i;

    for (; t != NULL && t->kind == TYPE_CLASS; t = t->base) {
        for (i = 0; i < t->field_count; i++) {
            if (t->fields[i].owned) {
                return true;
            }
        }
    }
    return false;
}

/* Whether a class of the chain of t below the root has a `destruct` with
   a body. */
static bool destruct_in_chain(const struct type *t)
{
    size_t i;

    for (; t != NULL && t->kind == TYPE_CLASS; t = t->base) {
        for (i = 0; i < t->member_count; i++) {
            const struct item *m = t->members[i];
            if (m->kind == ITEM_FN && sema_name_is(&m->name, "destruct") &&
                m->runtime == NULL && sema_has_body(m)) {
                return true;
            }
        }
    }
    return false;
}

/* Whether a class of the chain of t replaces the `destroyed` hook. */
static bool destroyed_in_chain(const struct type *t)
{
    size_t i;

    for (; t != NULL && t->kind == TYPE_CLASS; t = t->base) {
        for (i = 0; i < t->member_count; i++) {
            if (replaces_destroyed(t->members[i])) {
                return true;
            }
        }
    }
    return false;
}

static bool needs_teardown(const struct type *t, struct ptr_set *answered);

/* Whether a plain or `use` field of t, or of a class of its chain, needs
   a teardown. */
static bool field_needs_teardown(const struct type *t,
                                 struct ptr_set *answered)
{
    size_t i;

    for (; t != NULL; t = t->kind == TYPE_CLASS ? t->base : NULL) {
        for (i = 0; i < t->field_count; i++) {
            const struct struct_field *f = &t->fields[i];
            if ((f->form == FIELD_PLAIN || f->form == FIELD_USE) &&
                needs_teardown(f->type, answered)) {
                return true;
            }
        }
    }
    return false;
}

/* DESIGN: a struct or a tuple is owning when any of its parts owns
   something, transitively. Such a part is a class value that needs a
   teardown, and a collection is one. So is an `own fn`, a `?T` or an
   array of an owning type, and an owning struct or tuple. An owning one
   follows the value rules of a class value, and one that owns nothing
   stays plain data. A union owns nothing, since no teardown knows which
   of its fields it holds. A variant is owning when the struct of any of
   its cases is, since its tag names the case it holds, and its teardown
   tears down the fields of that case. answered holds the structs, the
   tuples, the variants and the classes met in this walk. None of them
   needs a teardown, since the walk ends at the first that does, and a
   type that holds another twice is walked once. */
static bool needs_teardown(const struct type *t, struct ptr_set *answered)
{
    size_t i;

    if (t == NULL) {
        return false;
    }
    switch (t->kind) {
    case TYPE_OPTIONAL:
    case TYPE_ARRAY:
        return needs_teardown(t->element, answered);
    case TYPE_FN:
        return t->owned;
    case TYPE_STRUCT:
    case TYPE_TUPLE:
        return !t->is_union && ptr_set_add(answered, t) &&
               field_needs_teardown(t, answered);
    case TYPE_VARIANT:
        if (!ptr_set_add(answered, t)) {
            return false;
        }
        for (i = 0; i < t->param_count; i++) {
            if (needs_teardown(t->params[i], answered)) {
                return true;
            }
        }
        return false;
    case TYPE_CLASS:
        /* The root's `destruct` is empty and never asks for one, which
           destruct_in_chain keeps out. */
        return ptr_set_add(answered, t) &&
               (destruct_in_chain(t) || destroyed_in_chain(t) ||
                own_field_in_chain(t) || field_needs_teardown(t, answered));
    default:
        return false;
    }
}

bool sema_needs_teardown(const struct type *t)
{
    struct ptr_set answered;
    bool needs;

    memset(&answered, 0, sizeof answered);
    needs = needs_teardown(t, &answered);
    ptr_set_free(&answered);
    return needs;
}

static const char own_fields_phrase[] = "has `own` fields";

/* The first field of the class, struct or tuple t whose type owns
   something, through the chain of a class, or NULL. */
static const struct struct_field *owning_field(const struct type *t)
{
    size_t i;

    for (; t != NULL; t = t->kind == TYPE_CLASS ? t->base : NULL) {
        for (i = 0; i < t->field_count; i++) {
            const struct struct_field *f = &t->fields[i];
            if ((f->form == FIELD_PLAIN || f->form == FIELD_USE) &&
                sema_type_owns(f->type)) {
                return f;
            }
        }
    }
    return NULL;
}

/* DESIGN: the refusal of a copy names what makes the class owning. Its own
   `own` fields and its `destruct` come first. Otherwise the message follows
   the first field that owns something down to its reason, `holds `Counted`
   in `counted`, which has a `destruct``. A field whose class has `own`
   fields keeps the phrase of the class itself. Writes into out. */
static void owns_reason(char *out, size_t size, const struct type *t)
{
    const struct struct_field *f;
    const struct type *inner;
    char sub[160];

    if (t->kind == TYPE_CLASS && own_field_in_chain(t)) {
        text_format(out, size, "%s", own_fields_phrase);
        return;
    }
    if (t->kind == TYPE_CLASS && destruct_in_chain(t)) {
        text_format(out, size, "has a `destruct`");
        return;
    }
    if (t->kind == TYPE_VARIANT) {
        text_format(out, size, "owns what its parts own");
        return;
    }
    f = owning_field(t);
    inner = f != NULL ? f->type : NULL;
    while (inner != NULL &&
           (inner->kind == TYPE_ARRAY || inner->kind == TYPE_OPTIONAL)) {
        inner = inner->element;
    }
    if (inner == NULL || (inner->kind != TYPE_CLASS &&
                          inner->kind != TYPE_STRUCT &&
                          inner->kind != TYPE_TUPLE &&
                          inner->kind != TYPE_VARIANT)) {
        text_format(out, size, "%s", own_fields_phrase);
        return;
    }
    owns_reason(sub, sizeof sub, inner);
    if (inner->kind == TYPE_CLASS && strcmp(sub, own_fields_phrase) == 0) {
        text_format(out, size, "%s", own_fields_phrase);
        return;
    }
    /* An element of a tuple is named `_0` upwards inside the compiler,
       and the program writes it as its number. */
    if (t->kind == TYPE_TUPLE && f->name.length > 1 && f->name.text[0] == '_') {
        text_format(out, size, "holds `%s` in `%.*s`, which %s",
                    sema_tn(inner), (int)f->name.length - 1, f->name.text + 1,
                    sub);
        return;
    }
    text_format(out, size, "holds `%s` in `%.*s`, which %s", sema_tn(inner),
                (int)f->name.length, f->name.text, sub);
}

const char *sema_owns_phrase(const struct type *t)
{
    /* DESIGN: the phrase stands in one of two buffers that take turns,
       so the phrases of two calls stay apart while one message holds
       both, and the next call but one writes a buffer again. The phrase
       is read before then, since each caller formats its message at
       once. The checker runs on one thread, the work thread of
       platform_run_on_stack, so no two calls race on a buffer. */
    static char buffers[2][160];
    static size_t next;
    char *buffer = buffers[next++ % 2];

    while (t != NULL && (t->kind == TYPE_ARRAY || t->kind == TYPE_OPTIONAL)) {
        t = t->element;
    }
    if (t == NULL || t->kind != TYPE_CLASS) {
        return "owns what its parts own";
    }
    owns_reason(buffer, sizeof buffers[0], t);
    return buffer;
}

/* DESIGN: one rule says what owns something. A type owns something when
   its teardown does anything: an `own` field, a part that owns something,
   or a `destruct` of its own in its chain. `sema_needs_teardown` is that
   rule. A function moves by the rules of a snapshot instead. A value of
   an owning type is not copied by `=`, `let` or `if let`, and `dup`
   copies it. */
bool sema_type_owns(const struct type *t)
{
    while (t != NULL && (t->kind == TYPE_ARRAY || t->kind == TYPE_OPTIONAL)) {
        t = t->element;
    }
    return t != NULL && t->kind != TYPE_FN && sema_needs_teardown(t);
}

/* Whether e reads a value that already lives somewhere. A literal, a
   call and `dup(x)` make a fresh one instead. */
bool sema_reads_existing(const struct expr *e)
{
    switch (e->kind) {
    case EXPR_NAME:
    case EXPR_FIELD:
    case EXPR_INDEX:
        return true;
    case EXPR_UNARY:
        return e->as.unary.op == TOKEN_STAR;
    case EXPR_CAST:
        /* `as` to the type a value has already gives that value, by R059
           of "Rules of 2026-10-13", and no fresh one. */
        return !e->as.cast.test && e->type != NULL &&
               e->type == e->as.cast.operand->type &&
               sema_reads_existing(e->as.cast.operand);
    default:
        return false;
    }
}

/* DESIGN: an `own` parameter owns its value, so `=`, `let`, `if let` and
   `let ... else` move one that owns memory or has a teardown rather than
   copy it, and the function tears it down no more. A copy would run the
   teardown twice, once for the parameter and once for the place. A value
   of a type parameter may own memory in a copy, so it moves as well. A
   function moves by the rules of a snapshot, which the conversion to
   the place checks. into names the place. Returns whether value moved.

   DESIGN: in generic code a local of a type parameter, or of a value that
   holds one in place, moves on `let` and `=` too, for every `T`. The
   checker cannot know whether the `T` of a copy owns something, and the
   copy for an owning `T` would otherwise give the value two owners. `dup`
   is the copy. */
static bool moves_own_param(struct checker *c, struct expr *value,
                            struct name into)
{
    static const struct name no_name = {"", 0};
    struct symbol *sym = value->kind == EXPR_NAME ? value->symbol : NULL;
    bool generic_local = sym != NULL && sym->kind == SYMBOL_LOCAL &&
                         !sym->caught && value->type != NULL &&
                         holds_param(value->type);

    if (sym == NULL || !(sym->own_param || generic_local) ||
        value->type == NULL || sema_is_error(value->type) ||
        value->type->kind == TYPE_FN ||
        !(sema_type_owns(value->type) || sema_needs_teardown(value->type) ||
          sema_has_params(value->type))) {
        return false;
    }
    sema_move_local(c, value, &into, &no_name);
    return true;
}

/* The concurrent class that a value of t is or holds in place, or NULL.
   A pointer holds none. answered holds the types with fields met in this
   walk, none of which holds one. */
static const struct type *concurrent_in(const struct type *t,
                                        struct ptr_set *answered)
{
    const struct type *found;
    size_t i;

    if (t == NULL) {
        return NULL;
    }
    if (t->kind == TYPE_CLASS && t->safety == SAFETY_CONCURRENT) {
        return t;
    }
    if (t->kind == TYPE_ARRAY || t->kind == TYPE_OPTIONAL) {
        return concurrent_in(t->element, answered);
    }
    if ((t->kind != TYPE_STRUCT && t->kind != TYPE_CLASS &&
         t->kind != TYPE_TUPLE && t->kind != TYPE_VARIANT) ||
        !ptr_set_add(answered, t)) {
        return NULL;
    }
    for (i = 0; i < t->field_count; i++) {
        found = concurrent_in(t->fields[i].type, answered);
        if (found != NULL) {
            return found;
        }
    }
    return NULL;
}

/* DESIGN: a concurrent class is shared state, its atomic fields among
   it, and a copy of it is not. By R039 of "Rules of 2026-10-13" `=` and
   `let` never copy one out of the place it lives in, also when it holds
   no Mutex. A fresh value moves, and `dup` copies. */
void sema_refuse_concurrent_copy(struct checker *c, const struct expr *value,
                                 const struct type *t)
{
    struct ptr_set answered;
    const struct type *found;

    if (sema_is_error(t) || !sema_reads_existing(value)) {
        return;
    }
    memset(&answered, 0, sizeof answered);
    found = concurrent_in(t, &answered);
    ptr_set_free(&answered);
    if (found == t) {
        sema_error_at(c, value->pos, "`%s` is a concurrent class and is not "
                      "copied by `=`, pass a pointer to it", sema_tn(t));
    } else if (found != NULL) {
        sema_error_at(c, value->pos, "`%s` holds the concurrent class `%s` "
                      "and is not copied by `=`, pass a pointer to it",
                      sema_tn(t), sema_tn(found));
    }
}

void sema_bind_value(struct checker *c, struct expr *value, struct name into,
                     const struct type *t)
{
    if (!moves_own_param(c, value, into)) {
        sema_refuse_owned_copy(c, value, t);
        sema_refuse_concurrent_copy(c, value, t);
    }
}

/* DESIGN: `=` refuses to copy an existing value that owns memory, since
   the bytes would give it two owners. A fresh value on the right has no
   other owner, so `=` moves it. The bytes it replaces are not destroyed:
   the element of an `alloc(T, n)` it fills has no value yet. */
void sema_refuse_owned_copy(struct checker *c, const struct expr *value,
                            const struct type *t)
{
    if (sema_refuse_caller_value(c, value)) {
        return;
    }
    if (!sema_is_error(t) && sema_type_owns(t) && sema_reads_existing(value)) {
        sema_error_at(c, value->pos, "`%s` %s, use `dup` instead of `=`",
                      sema_tn(t), sema_owns_phrase(t));
    }
    sema_refuse_lock_copy(c, value, t);
}

/* DESIGN: an `own` field owns the memory its pointer or slice names, and
   its type alone does not say so. `=` or a literal that reads one `own`
   field into another, `self.spare = self.head`, gives that memory two
   owners, and both free it, as T187 showed. "Ownership and copies" in
   `docs/anti-object-model.md` refuses the copy that gives a value two
   owners. `take` moves the value and `dup` copies it. A field given the
   pointer it holds, `b.one = b.one`, keeps one owner. A type that owns
   memory itself is refused by `sema_refuse_owned_copy`. */
void sema_refuse_own_field_copy(struct checker *c,
                                const struct struct_field *into,
                                const struct expr *value,
                                const struct expr *target)
{
    const struct type *s;
    const struct struct_field *from;
    struct text spelled = {0};
    struct text place = {0};

    if (into == NULL || !into->owned || value->kind != EXPR_FIELD ||
        value->type == NULL || sema_is_error(value->type) ||
        sema_type_owns(value->type) || value->as.field.base->type == NULL) {
        return;
    }
    s = sema_struct_of(value->as.field.base->type);
    from = s != NULL ? types_find_field(s, &value->as.field.name) : NULL;
    if (from == NULL || !from->owned) {
        return;
    }
    if (!sema_spell(&spelled, value)) {
        text_free(&spelled);
        text_appendf(&spelled, "the value");
    }
    if (target != NULL && sema_spell(&place, target) &&
        strcmp(text_cstr(&place), text_cstr(&spelled)) == 0) {
        text_free(&place);
        text_free(&spelled);
        return;
    }
    text_free(&place);
    sema_error_at(c, value->pos, "`%s` is held by an `own` field, use `take` "
                  "to move it or `dup` to copy it", text_cstr(&spelled));
    text_free(&spelled);
}

/* The field of the class s or of a class above it that name names. */
static const struct struct_field *field_up(const struct type *s,
                                           const struct name *name)
{
    const struct struct_field *f;

    for (; s != NULL; s = s->kind == TYPE_CLASS ? s->base : NULL) {
        if ((f = types_find_field(s, name)) != NULL) {
            return f;
        }
    }
    return NULL;
}

/* DESIGN: Q17 of "Answers of 2026-10-24". `take(x.f)` moves the object
   out of an `own ?*T` field and leaves `none` there, the move that the
   refusal of `=` between two `own` fields left without a form (T187).
   The value is a fresh owner of the type of the field, which moves on
   as the value of a call does. The argument is an `own` field of an
   optional pointer in a place, and `take` writes the place as `=`
   does, so a place `=` refuses is refused with the same words. A
   value that no new owner keeps is refused, since nothing would own
   the object, as sema_check_kept says. `take` is a built-in name, as `close` is, and a function
   `take` in scope keeps its name. The `[provisional]` entries after the
   same answers in docs/decisions.md say both. */
struct type *sema_check_take(struct checker *c, struct expr *e)
{
    struct expr *arg;
    struct type *t;
    const struct struct_field *f = NULL;
    const struct type *s;
    struct text spelled = {0};
    struct text name = {0};
    struct type *result = sema_builtin(c, TYPE_ERROR);

    if (e->as.call.handler.kind != HANDLE_NONE) {
        sema_error_at(c, e->as.call.handler.pos,
                      "this call cannot fail, so it has no error to handle");
        return result;
    }
    if (e->as.call.arg_count != 1) {
        sema_error_at(c, e->pos, "`" TAKE_NAME "` takes 1 argument, found %zu",
                      e->as.call.arg_count);
        return result;
    }
    arg = e->as.call.args[0];
    t = sema_check_storage(c, arg);
    if (sema_is_error(t)) {
        return t;
    }
    if (sema_spell(&name, arg)) {
        text_appendf(&spelled, "`%s`", text_cstr(&name));
    } else {
        text_appendf(&spelled, "its argument");
    }
    text_free(&name);
    if (arg->kind != EXPR_FIELD || arg->symbol != NULL) {
        sema_error_at(c, arg->pos, "`" TAKE_NAME "` takes an `own` field, and "
                      "%s is no field", text_cstr(&spelled));
        goto done;
    }
    s = sema_struct_of(arg->as.field.base->type);
    f = s != NULL ? field_up(s, &arg->as.field.name) : NULL;
    if (f == NULL || !f->owned) {
        sema_error_at(c, arg->pos, "`" TAKE_NAME "` takes an `own` field, and "
                      "%s is not `own`", text_cstr(&spelled));
        goto done;
    }
    if (!types_is_nullable(t)) {
        sema_error_at(c, arg->pos, "`" TAKE_NAME "` leaves `none` in %s, "
                      "whose type `%s` cannot be `none`", text_cstr(&spelled),
                      sema_tn(t));
        goto done;
    }
    if (t->kind != TYPE_POINTER) {
        sema_error_at(c, arg->pos, "`" TAKE_NAME "` takes an `own` field of "
                      "an optional pointer, and %s has type `%s`",
                      text_cstr(&spelled), sema_tn(t));
        goto done;
    }
    if (!sema_is_place(arg)) {
        sema_error_at(c, arg->pos, "`" TAKE_NAME "` writes `none` into its "
                      "field, and the field of a temporary value stands in "
                      "no place");
        goto done;
    }
    if (sema_refuse_write(c, arg)) {
        goto done;
    }
    /* The object has the value as its one owner, and a place that
       drops the value would lose it. */
    if (c->keeper != e) {
        sema_error_at(c, e->pos, "the object that `" TAKE_NAME "` moves out "
                      "of %s is dropped here, and `=` with `none` tears it "
                      "down", text_cstr(&spelled));
        goto done;
    }
    sema_note_write(c, arg);
    e->kind = EXPR_TAKE;
    memset(&e->as, 0, sizeof e->as);
    e->as.take = arg;
    result = t;
done:
    text_free(&spelled);
    return result;
}

/* DESIGN: the value of `take(x.f)` is the one owner of its object, so
   it stands where a new owner takes it over: the value of a `let`, of
   an `if let`, of `=` and of `return`, an argument of an `own`
   parameter and a part of a literal. Anywhere else the value is
   dropped, in a comparison and as an argument of a parameter that
   belongs to the caller among them, by T203 of
   docs/audit/tests-findings.md. Checks e as sema_check_expr does, and
   as such a value when kept is true. */
struct type *sema_check_kept(struct checker *c, struct expr *e,
                             struct type *expected, bool kept)
{
    const struct expr *outer = c->keeper;
    struct type *t;

    if (kept) {
        c->keeper = e;
    }
    t = sema_check_expr(c, e, expected);
    c->keeper = outer;
    return t;
}

/* Whether parameter index of callee is `own`. */
bool sema_owned_param(const struct symbol *callee, size_t index)
{
    return callee != NULL && callee->owned != NULL &&
           index < callee->owned_count && callee->owned[index];
}

/* The largest `align(N)` of t and of the parts it holds in place, or 0.
   A packed type puts its fields at any offset, so it has its own N
   alone. answered holds the types with fields met in this walk. */
static uint64_t declared_align(const struct type *t, struct ptr_set *answered)
{
    uint64_t align;
    size_t i;

    if (t == NULL) {
        return 0;
    }
    if (t->kind == TYPE_ARRAY || t->kind == TYPE_OPTIONAL) {
        return declared_align(t->element, answered);
    }
    if ((t->kind != TYPE_STRUCT && t->kind != TYPE_CLASS &&
         t->kind != TYPE_TUPLE && t->kind != TYPE_VARIANT) ||
        !ptr_set_add(answered, t)) {
        return 0;
    }
    align = t->align;
    for (i = 0; !t->packed && i < t->field_count; i++) {
        uint64_t part = declared_align(t->fields[i].type, answered);
        align = part > align ? part : align;
    }
    return align;
}

/* DESIGN: every type the language builds in aligns to 16 at most on every
   target, a simd struct among them. An alignment above that comes from
   `align(N)` alone, which is a number of the source. The checker therefore
   knows the alignment of a type that malloc does not cover, without a
   layout. */
uint64_t sema_declared_align(const struct type *t)
{
    struct ptr_set answered;
    uint64_t align;

    memset(&answered, 0, sizeof answered);
    align = declared_align(t, &answered);
    ptr_set_free(&answered);
    return align;
}

/* A value holds a Mutex when it is one, or a struct, a class, a tuple, a
   variant, a `?T` or an array with one inside it. A pointer holds none. */
bool sema_holds_mutex(const struct type *t)
{
    return types_holds(t, types_is_mutex);
}

/* DESIGN: a Mutex is the lock word itself, so a copy would be a second
   lock that guards nothing. A value that holds one is never copied out
   of the place it lives in: not by `=`, not into a parameter and not by
   `return`. A fresh value, a literal, a call or `Mutex.new()`, moves. */
void sema_refuse_lock_copy(struct checker *c, const struct expr *value,
                           const struct type *t)
{
    if (!sema_is_error(t) && sema_holds_mutex(t) &&
        sema_reads_existing(value)) {
        if (types_is_mutex(t)) {
            sema_error_at(c, value->pos, "a `" LANG_MUTEX "` cannot be "
                          "copied, pass a pointer to it");
        } else {
            sema_error_at(c, value->pos, "`%s` holds a `" LANG_MUTEX "` and "
                          "cannot be copied, pass a pointer to it",
                          sema_tn(t));
        }
    }
}

/* Whether a value of t may own something in a copy of a generic: a type
   parameter, or a value that holds one in place. A pointer or a slice
   to one owns nothing. */
static bool holds_param(const struct type *t)
{
    size_t i;

    switch (t->kind) {
    case TYPE_PARAM:
        return true;
    case TYPE_ARRAY:
    case TYPE_OPTIONAL:
        return holds_param(t->element);
    case TYPE_TUPLE:
        for (i = 0; i < t->param_count; i++) {
            if (holds_param(t->params[i])) {
                return true;
            }
        }
        return false;
    case TYPE_STRUCT:
    case TYPE_CLASS:
    case TYPE_VARIANT:
        return !types_is_chan(t) && sema_has_params(t);
    default:
        return false;
    }
}

/* DESIGN: a parameter that is not `own` belongs to the caller, so a copy
   of its bytes would give what it owns two owners, the caller and the
   place. It leaves the function in no way that gives it one: not by `let`
   or `=`, not in a tuple, struct, class or variant literal, not as an
   `own` argument and not by `return`. That holds for a parameter whose
   value owns something and, in generic code, for one whose type holds a
   type parameter, since the `T` of a copy may own something, whatever `T`
   is. The message names both fixes: `own` takes the value over, and `dup`
   copies it. Returns whether value was refused. */
bool sema_refuse_caller_value(struct checker *c, const struct expr *value)
{
    const struct symbol *sym =
        value->kind == EXPR_NAME ? value->symbol : NULL;
    const struct type *t = value->type;

    if (sym == NULL || sym->kind != SYMBOL_PARAM || sym->own_param ||
        sym->caught || t == NULL || sema_is_error(t) || t->kind == TYPE_FN ||
        !(sema_type_owns(t) || holds_param(t))) {
        return false;
    }
    sema_error_at(c, value->pos, "`%.*s` belongs to the caller and does not "
                  "move. Mark it `own` to take it over, or copy it with "
                  "`dup(%.*s)`", (int)sym->name.length, sym->name.text,
                  (int)sym->name.length, sym->name.text);
    return true;
}

/* DESIGN: an owning local named in a tuple literal, an array literal, a
   struct literal, a class literal or the literal of a variant case moves
   into it, as into an `own` parameter. A copy would give what it owns two owners, the
   local and the literal, and both would tear it down. So does an `own`
   parameter, and a local of a type parameter, which may own something in
   a copy. A parameter that is not `own` belongs to the caller, and
   `sema_refuse_owned_copy` refuses it. Anything else that reads an
   existing value is refused as `=` refuses it. */
bool sema_literal_moves(const struct expr *value)
{
    const struct symbol *sym =
        value->kind == EXPR_NAME ? value->symbol : NULL;
    const struct type *t = value->type;

    if (sym == NULL || t == NULL || sema_is_error(t) || t->kind == TYPE_FN ||
        (sym->kind != SYMBOL_LOCAL && sym->kind != SYMBOL_PARAM) ||
        (sym->kind == SYMBOL_PARAM && !sym->own_param) || sym->caught) {
        return false;
    }
    if (sema_type_owns(t) || sema_needs_teardown(t)) {
        return true;
    }
    return holds_param(t) && (sym->kind == SYMBOL_LOCAL || sym->own_param);
}

/* DESIGN: the parts move once the literal has read every part, so a part
   after the local may still read it, and lowering clears the local's
   tables after the last part is stored. A local that a part of the same
   literal already moved, `(k, k)` or `(k, take(k))`, is refused. literal
   names the type of the literal. */
void sema_move_into_literal(struct checker *c, struct expr *value,
                            const char *literal)
{
    static const struct name no_name = {"", 0};
    struct name into;
    char *text;
    size_t length;

    /* The error a handler binds is stored into no literal. */
    sema_refuse_escaping_error(c, value);
    if (!sema_literal_moves(value)) {
        return;
    }
    length = strlen(literal);
    text = types_alloc_array(c->arena, length + 1, 1);
    memcpy(text, literal, length + 1);
    into.text = text;
    into.length = length;
    sema_move_local(c, value, &into, &no_name);
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
   `own` parameter that `=` or `let` stores. The checker marks the move
   and notes where it went. The safety check `use-after-free` follows
   the paths of the body and refuses a use of the local after the move,
   in the next turn of a loop among them, until a new value. A `defer` or
   an `undo` that names the local runs at an exit after the move, and a
   closure that captures it may run later, so both are refused, and so
   is a move inside a closure, which may run more than once. A parameter
   that is not `own` belongs to the caller, so one whose value may own
   memory does not move, as `sema_refuse_caller_value` says. Returns
   whether e moved. */
bool sema_move_local(struct checker *c, struct expr *e,
                     const struct name *into, const struct name *by)
{
    struct symbol *sym = e->symbol;
    const struct name *target = by->length > 0 ? by : into;
    struct move_note *note;

    if (c->ctx.quiet > 0) {
        return true;
    }
    if (sym->frame != c->ctx.function) {
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
    if (sym->deferred) {
        sema_error_at(c, e->pos, "`%.*s` moves into `%.*s` while a `defer` or "
                      "an `undo` names it", (int)sym->name.length,
                      sym->name.text, (int)target->length, target->text);
        return false;
    }
    if (sema_refuse_caller_value(c, e)) {
        return false;
    }
    note = arena_alloc(c->arena, sizeof *note);
    note->into = *into;
    note->by = *by;
    ptr_map_put(&c->moves, e, note);
    e->moves = true;
    sym->moved = true;
    return true;
}

/* DESIGN: a value that owns nothing and holds no pointer is copied into
   an `own` parameter and does not move, so a caller that passes `int`
   locals to `max<T>(own a: T, own b: T)` keeps them. The type is concrete
   at the call site. A value that owns something, one of a type parameter
   and one that holds a pointer move, since the callee takes over what a
   pointer reaches. A channel and a Mutex are handles, and they move. */
static bool moves_at_call(const struct type *t)
{
    return sema_type_owns(t) || sema_needs_teardown(t) ||
           holds_param(t) || !types_pointer_free(t) || types_is_chan(t) ||
           types_is_mutex(t);
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
void sema_note_move(struct checker *c, const struct symbol *callee,
                      size_t index, const struct expr *receiver,
                      struct expr *arg)
{
    struct symbol *moved = arg->kind == EXPR_NAME ? arg->symbol : NULL;
    struct name into;
    struct name by;

    if (callee == NULL || callee->owned == NULL ||
        index >= callee->owned_count || !callee->owned[index] ||
        c->ctx.quiet > 0 || arg->type == NULL || sema_is_error(arg->type)) {
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
    if (!moves_at_call(arg->type)) {
        return;
    }
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

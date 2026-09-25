/* The checks of expressions: places, literals, operators, casts,
   interpolation and the dispatch over the kinds of an expression. */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "arith.h"
#include "pattern.h"
#include "sema_checker.h"

/* Places and literals */

/* An expression that denotes a location in memory, as chapter 2 lists. */
bool sema_is_place(const struct expr *e)
{
    const struct type *base;

    switch (e->kind) {
    case EXPR_NAME:
        return e->symbol != NULL && (e->symbol->kind == SYMBOL_LOCAL ||
                                     e->symbol->kind == SYMBOL_PARAM);
    case EXPR_UNARY:
        return e->as.unary.op == TOKEN_STAR;
    case EXPR_INDEX:
        base = e->as.index.base->type;
        return base->kind == TYPE_POINTER || base->kind == TYPE_SLICE ||
               (base->kind == TYPE_ARRAY && sema_is_place(e->as.index.base));
    case EXPR_FIELD:
        base = e->as.field.base->type;
        /* The tag of a variant changes with the whole value alone. */
        if ((base->kind == TYPE_POINTER ? base->element : base)->kind ==
            TYPE_VARIANT) {
            return false;
        }
        return base->kind == TYPE_POINTER ||
               (type_has_fields(base) && sema_is_place(e->as.field.base));
    default:
        return false;
    }
}

/* Whether e reads a bitfield, which has no address. */
static bool is_bitfield(const struct expr *e)
{
    const struct type *s;
    const struct struct_field *f;

    if (e->kind != EXPR_FIELD || e->as.field.base->type == NULL) {
        return false;
    }
    s = sema_struct_of(e->as.field.base->type);
    f = s != NULL ? sema_find_field(s, &e->as.field.name) : NULL;
    return f != NULL && f->bits != 0;
}

void sema_mark_address_taken(struct checker *c, struct expr *e)
{
    const struct expr *place = e;

    while (e->kind == EXPR_INDEX || e->kind == EXPR_FIELD) {
        struct expr *base = e->kind == EXPR_INDEX ? e->as.index.base
                                                  : e->as.field.base;
        if (base->type->kind == TYPE_POINTER ||
            base->type->kind == TYPE_SLICE) {
            return;
        }
        e = base;
    }
    if (e->kind == EXPR_NAME && e->symbol != NULL) {
        e->symbol->address_taken = true;
    }
    /* An address taken of a captured variable may change it. */
    sema_note_write(c, place);
}

/* A literal whose type comes from its context: an integer or float
   literal, one of those after unary '-', or `none`. */
static bool is_untyped(const struct expr *e)
{
    if (e->kind == EXPR_UNARY && e->as.unary.op == TOKEN_MINUS) {
        e = e->as.unary.operand;
        return e->kind == EXPR_INT || e->kind == EXPR_FLOAT;
    }
    return e->kind == EXPR_INT || e->kind == EXPR_FLOAT || e->kind == EXPR_NONE;
}

static uint64_t max_of(const struct type *t)
{
    int bits = type_bits(t);
    uint64_t all = bits == 64 ? UINT64_MAX : (((uint64_t)1 << bits) - 1);
    return type_is_signed(t) ? all >> 1 : all;
}

static void set_type(struct expr *e, struct type *t)
{
    e->type = t;
}

/* An integer literal, with negative set for '-' in front of it. */
static struct type *integer_literal(struct checker *c, struct expr *e,
                                    struct expr *literal, bool negative,
                                    struct type *expected)
{
    struct type *t = sema_builtin(c, TYPE_I64);
    uint64_t magnitude = literal->as.integer;
    const char *sign = negative ? "-" : "";

    if (expected != NULL && !sema_is_error(expected) &&
        expected->kind != TYPE_VOID) {
        if (type_is_integer(expected)) {
            t = expected;
        } else {
            sema_error_at(c, e->pos, "expected `%s`, found an integer literal",
                          sema_tn(expected));
            set_type(literal, sema_builtin(c, TYPE_ERROR));
            return literal->type;
        }
    }
    if (negative ? (!type_is_signed(t) ? magnitude != 0
                                       : magnitude > max_of(t) + 1)
                 : magnitude > max_of(t)) {
        sema_error_at(c, e->pos, "`%s%.*s` does not fit `%s`%s", sign,
                      (int)literal->spelling.length, literal->spelling.bytes,
                      sema_tn(t),
                      type_is_target_sized(t) ? " on every target" : "");
        t = sema_builtin(c, TYPE_ERROR);
    }
    set_type(literal, t);
    return t;
}

static struct type *float_literal(struct checker *c, struct expr *e,
                                  struct expr *literal, bool negative,
                                  struct type *expected)
{
    struct type *t = sema_builtin(c, TYPE_F64);

    (void)negative;
    if (expected != NULL && !sema_is_error(expected) &&
        expected->kind != TYPE_VOID) {
        if (type_is_float(expected)) {
            t = expected;
        } else {
            sema_error_at(c, e->pos, "expected `%s`, found a float literal",
                          sema_tn(expected));
            set_type(literal, sema_builtin(c, TYPE_ERROR));
            return literal->type;
        }
    }
    if (isinf(arith_float_literal(literal->as.text.bytes,
                                  literal->as.text.length,
                                  t->kind == TYPE_F32))) {
        sema_error_at(c, e->pos, "`%.*s` does not fit `%s`",
                      (int)literal->spelling.length, literal->spelling.bytes,
                      sema_tn(t));
        t = sema_builtin(c, TYPE_ERROR);
    }
    set_type(literal, t);
    return t;
}

/* Report a value of type got where the context expects another type. */
/* DESIGN: a pointer to a struct converts to a pointer to any type of its
   `inherits` chain. That is the one implicit conversion of the language.
   The base lies at offset 0, so the address is the same. */
static bool converts_to_base(const struct type *got,
                             const struct type *expected)
{
    const struct type *t;

    if (got->kind != TYPE_POINTER || expected->kind != TYPE_POINTER) {
        return false;
    }
    for (t = sema_inherited(got->element); t != NULL; t = sema_inherited(t)) {
        if (t == expected->element) {
            return true;
        }
    }
    return false;
}

/* DESIGN: a pointer to a class converts to a pointer to any interface
   it implements. The sub-object sits inside the object, so the value
   moves by its offset, which is the one conversion in the language that
   changes an address. A name two sub-objects both reach has no single
   answer, and the program writes the path itself. */
static const struct struct_field *converts_to_interface(struct checker *c,
                                                        const struct expr *e,
                                                        const struct type *got,
                                                        const struct type *to)
{
    const struct struct_field *found = NULL;
    const struct type *t;

    if (got->kind != TYPE_POINTER || to->kind != TYPE_POINTER ||
        got->element->kind != TYPE_CLASS || to->element->kind != TYPE_CLASS) {
        return NULL;
    }
    for (t = got->element; t != NULL; t = sema_inherited(t)) {
        size_t i;
        for (i = 0; i < t->field_count; i++) {
            if (t->fields[i].form != FIELD_IMPL ||
                !sema_descends_from(t->fields[i].type, to->element)) {
                continue;
            }
            if (found != NULL) {
                sema_error_at(c, e->pos,
                              "`%s` converts to `%s` through `%.*s` and "
                              "through `%.*s`, name one", sema_tn(got),
                              sema_tn(to),
                              (int)found->name.length, found->name.text,
                              (int)t->fields[i].name.length,
                              t->fields[i].name.text);
                return NULL;
            }
            found = &t->fields[i];
        }
    }
    return found;
}

/* The spelling of e for a message: a name, or a path of field names on
   one. Anything else has no short spelling and the caller says "the
   value" instead. */
bool sema_spell(struct text *out, const struct expr *e)
{
    if (e->kind == EXPR_NAME) {
        text_appendf(out, "%.*s", (int)e->as.name.length, e->as.name.text);
        return true;
    }
    if (e->kind == EXPR_FIELD && !e->as.field.promoted &&
        sema_spell(out, e->as.field.base)) {
        text_appendf(out, ".%.*s", (int)e->as.field.name.length,
                     e->as.field.name.text);
        return true;
    }
    /* `p?.x`, which the checker holds as the field it reads on p. */
    if (e->kind == EXPR_OPTIONAL &&
        e->as.optional.access->kind == EXPR_FIELD &&
        sema_spell(out, e->as.optional.base)) {
        text_appendf(out, "?.%.*s",
                     (int)e->as.optional.access->as.field.name.length,
                     e->as.optional.access->as.field.name.text);
        return true;
    }
    return false;
}

static void error_may_be_none(struct checker *c, const struct expr *e,
                              const struct type *t)
{
    struct text spelling = {0};
    const char *form = t != NULL && t->kind == TYPE_FN ? "?fn(...)" : "?*T";

    /* A `?T` of a value, a match among them, is tested before it is
       read. */
    if (t != NULL && t->kind == TYPE_OPTIONAL) {
        if (sema_spell(&spelling, e)) {
            sema_error_at(c, e->pos, "`%s` may be `none`, test it before "
                          "reading it", text_cstr(&spelling));
        } else {
            sema_error_at(c, e->pos, "the %s may be `none`, test it before "
                          "reading it",
                          types_is_maybe_match(t) ? "match" : "value");
        }
        text_free(&spelling);
        return;
    }
    if (sema_spell(&spelling, e)) {
        sema_error_at(c, e->pos, "`%s` may be `none`, check it or use `%s`",
                      text_cstr(&spelling), form);
    } else {
        sema_error_at(c, e->pos,
                      "the value may be `none`, check it or use `%s`",
                      form);
    }
    text_free(&spelling);
}

/* DESIGN: a `?*T` reaching a place that dereferences it is the error the
   nullable rule exists for. Checking continues with the `*T` of the same
   element, so one unchecked pointer reports once and the rest of the
   expression is still checked. */
struct type *sema_usable_pointer(struct checker *c, const struct expr *e,
                                 struct type *t)
{
    if (!type_is_nullable(t)) {
        return t;
    }
    error_may_be_none(c, e, t);
    return types_without_none(c->types, t);
}

/* DESIGN: a name narrowed from a `?T` reads the value it holds. A
   comparison with `none` and a test read the whole variable again, its
   flag included, so the name takes the declared type there. */
struct type *sema_whole_optional(struct type *t, struct expr *e)
{
    if (e->kind == EXPR_NAME && e->symbol != NULL && e->symbol->type != NULL &&
        e->symbol->type->kind == TYPE_OPTIONAL && !sema_is_error(t)) {
        e->type = e->symbol->type;
        return e->type;
    }
    return t;
}

/* `*T` passes where `?*T` is expected, because a pointer that never
   holds `none` is one of the values a `?*T` holds. The reverse needs a
   check the program wrote. */
static bool widens_to_nullable(const struct type *got,
                               const struct type *expected)
{
    return (got->kind == TYPE_POINTER || got->kind == TYPE_FN) &&
           !got->nullable && type_is_nullable(expected) &&
           got->kind == expected->kind;
}

/* The first variable of the anonymous function it that a `concurrent`
   parameter refuses, or NULL. Refused are a variable it changes whose
   type is not thread-safe and a function it calls that is not
   `concurrent`. */
static const struct capture *unsafe_capture(const struct item *it)
{
    size_t i;

    for (i = 0; it != NULL && i < it->capture_count; i++) {
        const struct capture *cap = &it->captures[i];
        if ((cap->written && !sema_thread_safe_symbol(cap->symbol)) ||
            cap->called) {
            return cap;
        }
    }
    return NULL;
}

/* Refuse a function that is not kept where one is kept: a field, a
   global, a result, a `keep` parameter or a parameter of an `extern fn`.
   A closure by reference is refused with `snapshot fn` as the fix, and a
   snapshot with `keep own`. */
static void refuse_kept(struct checker *c, const struct expr *e)
{
    const struct symbol *sym = e->kind == EXPR_NAME ? e->symbol : NULL;

    if (e->kind == EXPR_FN && e->as.fn->capture_count > 0 &&
        e->as.fn->snapshot) {
        sema_error_at(c, e->pos, "a `snapshot fn` is kept where it is owned, "
                      "by a `keep own` parameter or an `own` field");
    } else if (e->kind == EXPR_FN && e->as.fn->capture_count > 0) {
        const struct symbol *first = e->as.fn->captures[0].symbol;
        sema_error_at(c, e->pos, "a closure is never kept, and this one "
                      "captures `%.*s`, so a `snapshot fn` at a `keep own` "
                      "parameter or an `own` field takes its value",
                      (int)first->name.length, first->name.text);
    } else if (sym != NULL && sym->kind == SYMBOL_PARAM) {
        sema_error_at(c, e->pos, "`%.*s` is not marked `keep` and cannot be "
                      "stored", (int)sym->name.length, sym->name.text);
    } else if (sym != NULL) {
        sema_error_at(c, e->pos, "`%.*s` holds a function that is not kept "
                      "and cannot be stored", (int)sym->name.length,
                      sym->name.text);
    } else {
        sema_error_at(c, e->pos,
                      "a function that is not kept cannot be stored");
    }
}

/* Refuse the name e of sym, which moved before it in the order of the
   text. */
static void refuse_moved(struct checker *c, const struct expr *e,
                         const struct symbol *sym)
{
    if (sym->moved_by.length > 0 && sym->moved_to.length > 0) {
        sema_error_at(c, e->pos, "`%.*s` was moved into `%.*s` by `%.*s`",
                      (int)e->as.name.length, e->as.name.text,
                      (int)sym->moved_to.length, sym->moved_to.text,
                      (int)sym->moved_by.length, sym->moved_by.text);
    } else if (sym->moved_by.length == 0 && sym->moved_to.length == 0) {
        sema_error_at(c, e->pos, "`%.*s` was moved", (int)e->as.name.length,
                      e->as.name.text);
    } else {
        const struct name *to = sym->moved_by.length > 0 ? &sym->moved_by
                                                         : &sym->moved_to;
        sema_error_at(c, e->pos, "`%.*s` was moved into `%.*s`",
                      (int)e->as.name.length, e->as.name.text,
                      (int)to->length, to->text);
    }
}

/* The variable that the place or the address e starts from, or NULL. */
static const struct symbol *root_of(const struct expr *e)
{
    for (;;) {
        switch (e->kind) {
        case EXPR_NAME:
            return e->symbol;
        case EXPR_FIELD:
            e = e->as.field.base;
            break;
        case EXPR_INDEX:
            e = e->as.index.base;
            break;
        case EXPR_SLICE:
            e = e->as.slice.base;
            break;
        case EXPR_UNARY:
            e = e->as.unary.operand;
            break;
        default:
            return NULL;
        }
    }
}

/* DESIGN: a pointer or a slice derived from a lent one is lent as well:
   `&p.field`, `&s[i]` and `&(*p)`, `s.ptr`, a part of a lent slice and a
   slice of an array that a lent pointer reaches. The place e is derived
   when its path passes through a lent pointer or a lent slice. A pointer
   the object holds in a field is its own value and not derived. */
static bool through_lent(const struct expr *e)
{
    for (;;) {
        const struct type *base;
        switch (e->kind) {
        case EXPR_FIELD:
            base = e->as.field.base->type;
            if (base == NULL || type_is_lent(base)) {
                return base != NULL;
            }
            if (base->kind == TYPE_POINTER) {
                return false;
            }
            e = e->as.field.base;
            break;
        case EXPR_INDEX:
            base = e->as.index.base->type;
            if (base == NULL || type_is_lent(base)) {
                return base != NULL;
            }
            if (base->kind != TYPE_ARRAY) {
                return false;
            }
            e = e->as.index.base;
            break;
        case EXPR_UNARY:
            return e->as.unary.op == TOKEN_STAR &&
                   type_is_lent(e->as.unary.operand->type);
        default:
            return false;
        }
    }
}

/* Refuse the `lent` pointer e where a pointer is kept. */
static void refuse_lent(struct checker *c, const struct expr *e)
{
    static const char *const uses[] = {
        [LENT_STORED] = "cannot be stored",
        [LENT_RETURNED] = "cannot be returned",
        [LENT_PASSED] = "passes on to a `lent` parameter alone",
        [LENT_TO_C] = "passes on to a `lent` parameter alone",
    };
    const struct symbol *root = root_of(e);

    if (e->kind == EXPR_NAME && e->symbol != NULL && e->symbol->lent_turn) {
        sema_error_at(c, e->pos, "`%.*s` is lent for one turn of the loop "
                      "and %s", (int)e->as.name.length, e->as.name.text,
                      uses[c->lent_use]);
    } else if (e->kind == EXPR_NAME) {
        sema_error_at(c, e->pos, "`%.*s` is lent for the call and %s",
                      (int)e->as.name.length, e->as.name.text,
                      uses[c->lent_use]);
    } else {
        sema_error_at(c, e->pos, "the %s is lent for %s and %s",
                      e->type != NULL && e->type->kind == TYPE_SLICE
                          ? "slice"
                          : "pointer",
                      root != NULL && root->lent_turn
                          ? "one turn of the loop"
                          : "the call",
                      uses[c->lent_use]);
    }
}

/* Whether e is `dup` of a function value, a copy that no one owns yet. */
static bool is_fn_dup(const struct expr *e)
{
    return e->kind == EXPR_OBJECT && e->as.object.op == TOKEN_DUP &&
           e->type != NULL && e->type->kind == TYPE_FN && e->type->owned;
}

/* DESIGN: an `own fn` place takes a fresh value: a snapshot, a function
   that captures nothing, `dup` of an owned value, or a `keep own`
   parameter of the function, which moves. `=` copies no owned value,
   since two owners would free one snapshot twice. Returns whether the
   value e of type got is taken. */
static bool require_owned(struct checker *c, struct expr *e,
                          struct type *got)
{
    struct symbol *sym = e->kind == EXPR_NAME ? e->symbol : NULL;

    if (!got->context) {
        e->to_context = true;
        return true;
    }
    if (e->kind == EXPR_FN) {
        if (!e->as.fn->snapshot) {
            refuse_kept(c, e);
            return false;
        }
        e->as.fn->snapshot_heap = true;
        return true;
    }
    if (!got->owned) {
        if (sym != NULL && sym->kind == SYMBOL_PARAM) {
            sema_error_at(c, e->pos, "`%.*s` is not marked `keep own` and "
                          "cannot be owned", (int)sym->name.length,
                          sym->name.text);
        } else {
            refuse_kept(c, e);
        }
        return false;
    }
    if (is_fn_dup(e)) {
        return true;
    }
    if (sym != NULL && sym->kind == SYMBOL_PARAM && sym->frame != c->function) {
        sema_error_at(c, e->pos, "`%.*s` is captured, and a closure does not "
                      "move what it captures", (int)sym->name.length,
                      sym->name.text);
        return false;
    }
    if (sym != NULL && sym->kind == SYMBOL_PARAM && c->quiet == 0) {
        if (c->loop_depth > 0) {
            sema_error_at(c, e->pos, "`%.*s` moves into an owner inside a "
                          "loop, which would move it again",
                          (int)sym->name.length, sym->name.text);
            return false;
        }
        e->moves_snapshot = true;
        sym->snapshot_moved = true;
        sym->snapshot_move = e->pos;
        return true;
    }
    if (sym != NULL && sym->kind == SYMBOL_PARAM) {
        return true;
    }
    sema_error_at(c, e->pos, "an `own fn` value is not copied by `=`, use "
                  "`dup`");
    return false;
}

/* Refuse a function that is not `concurrent` at a `concurrent`
   parameter. The message names the variable a closure changes. */
static void refuse_not_concurrent(struct checker *c, const struct expr *e)
{
    const struct symbol *sym = e->kind == EXPR_NAME ? e->symbol : NULL;
    const struct item *closure = e->kind == EXPR_FN ? e->as.fn
                                 : sym != NULL      ? sym->closure
                                                    : NULL;
    const struct capture *cap = unsafe_capture(closure);

    if (cap != NULL && cap->written &&
        !sema_thread_safe_symbol(cap->symbol)) {
        sema_error_at(c, cap->write, "`%.*s` is changed in a closure at a "
                      "`concurrent` parameter, and `%s` is not thread-safe, "
                      "so a `snapshot fn` reads a copy of it instead",
                      (int)cap->symbol->name.length, cap->symbol->name.text,
                      sema_tn(cap->symbol->type));
    } else if (cap != NULL) {
        sema_error_at(c, cap->call, "`%.*s` is called in a closure at a "
                      "`concurrent` parameter and is not marked `concurrent`",
                      (int)cap->symbol->name.length, cap->symbol->name.text);
    } else if (sym != NULL && sym->kind == SYMBOL_PARAM) {
        sema_error_at(c, e->pos, "`%.*s` is passed on to a `concurrent` "
                      "parameter and is not marked `concurrent`",
                      (int)sym->name.length, sym->name.text);
    } else if (sym != NULL) {
        sema_error_at(c, e->pos, "`%.*s` holds a function that is not marked "
                      "`concurrent`", (int)sym->name.length, sym->name.text);
    } else {
        sema_error_at(c, e->pos, "the function is not marked `concurrent`");
    }
}

/* DESIGN: the forms of one function type convert in one direction. A
   plain function takes the form of two words with the context `none`,
   and a `concurrent` one is taken where any is. Nothing converts to the
   plain form, so a function that is not kept never reaches a place that
   keeps it. An `own fn` lends itself to a parameter of two words and is
   taken where it is owned, which `require_owned` decides. Returns -1
   when got and expected are not forms of one type, and else whether the
   conversion holds. */
static int require_fn_form(struct checker *c, struct expr *e,
                           struct type *got, struct type *expected)
{
    if (got->kind != TYPE_FN || expected->kind != TYPE_FN || got->bound ||
        expected->bound || (!got->context && !expected->context) ||
        types_fn_form(c->types, types_without_none(c->types, got), false,
                      false) !=
            types_fn_form(c->types, types_without_none(c->types, expected),
                          false, false)) {
        return -1;
    }
    if (got->nullable && !expected->nullable) {
        error_may_be_none(c, e, got);
        return 0;
    }
    if (expected->owned) {
        return require_owned(c, e, got) ? 1 : 0;
    }
    if (!expected->context && got->owned) {
        sema_error_at(c, e->pos, "an `own fn` value owns its snapshot and "
                      "does not pass as one C function pointer");
        return 0;
    }
    if (!expected->context) {
        refuse_kept(c, e);
        return 0;
    }
    if (is_fn_dup(e)) {
        sema_error_at(c, e->pos, "the copy `dup` makes of a function has no "
                      "owner here, and goes to an `own` field or a `keep own` "
                      "parameter");
        return 0;
    }
    if (expected->concurrent && got->context && !got->concurrent) {
        refuse_not_concurrent(c, e);
        return 0;
    }
    e->to_context = !got->context;
    return 1;
}

bool sema_require(struct checker *c, struct expr *e, struct type *got,
                  struct type *expected)
{
    const struct struct_field *iface;
    int form;

    if (sema_is_error(got) || sema_is_error(expected)) {
        return !sema_is_error(got);
    }
    /* DESIGN: a `lent` pointer goes where a `lent` pointer is expected,
       and nowhere else a pointer is kept. Any pointer goes where a `lent`
       one is expected, since lending promises the callee less. Past that
       the two forms follow the rules of `*T`. */
    /* DESIGN: the one exit of a lent pointer or slice is an argument of an
       `extern fn`. C cannot be checked, and whether it keeps what it takes
       is its contract, as for every pointer given to C. */
    if (type_is_lent(got) && !type_is_lent(expected) &&
        (expected->kind == TYPE_POINTER || expected->kind == TYPE_SLICE) &&
        c->lent_use != LENT_TO_C) {
        refuse_lent(c, e);
        return false;
    }
    if (type_is_lent(got) || type_is_lent(expected)) {
        return sema_require(c, e, types_unlent(c->types, got),
                            types_unlent(c->types, expected));
    }
    if ((form = require_fn_form(c, e, got, expected)) >= 0) {
        return form == 1;
    }
    if (got == expected) {
        return true;
    }
    /* A match of a literal is a plain match, and a `?Match` of a literal
       a plain `?Match`, since all of them have one layout. */
    if (((types_is_match(got) && types_is_match(expected)) ||
         (types_is_maybe_match(got) && types_is_maybe_match(expected))) &&
        types_match_plain(c->types, got) == expected) {
        return true;
    }
    /* DESIGN: a T is one of the values a `?T` holds, so it passes where
       a `?T` is expected. Every conversion that reaches T reaches the `?T`
       as well. Lowering writes the flag beside it. The reverse needs a
       test the program wrote. */
    if (expected->kind == TYPE_OPTIONAL && got->kind != TYPE_OPTIONAL &&
        got->kind != TYPE_NONE) {
        bool held;
        c->quiet++;
        held = sema_require(c, e, got, expected->element);
        c->quiet--;
        if (held) {
            e->to_optional = expected;
            return true;
        }
    }
    if (got->kind == TYPE_OPTIONAL && expected->kind != TYPE_OPTIONAL) {
        bool held;
        c->quiet++;
        held = sema_require(c, e, got->element, expected);
        c->quiet--;
        if (held) {
            e->to_iface = NULL;
            e->to_context = false;
            error_may_be_none(c, e, got);
            return false;
        }
    }
    /* The one implicit conversion of a pointer and the widening to
       `?*T` compose: a `*Circle` reaches a `?*Shape` parameter. */
    if (widens_to_nullable(got, expected)) {
        struct type *bare = types_without_none(c->types, expected);
        if (got == bare || converts_to_base(got, bare)) {
            return true;
        }
        if ((iface = converts_to_interface(c, e, got, bare)) != NULL) {
            e->to_iface = iface;
            return true;
        }
    }
    /* A `?*T` where a `*T` is expected is the nullable rule itself, and
       names the value rather than the two types. */
    if (type_is_nullable(got) && got->kind == expected->kind &&
        !expected->nullable) {
        struct type *bare = types_without_none(c->types, got);
        if (bare == expected ||
            (expected->kind == TYPE_POINTER &&
             (converts_to_base(bare, expected) ||
              converts_to_interface(c, e, bare, expected) != NULL))) {
            error_may_be_none(c, e, got);
            return false;
        }
    }
    if (converts_to_base(got, expected)) {
        return true;
    }
    if ((iface = converts_to_interface(c, e, got, expected)) != NULL) {
        e->to_iface = iface;
        return true;
    }
    if (got->kind == TYPE_VOID) {
        if (e->kind == EXPR_CALL && e->as.call.callee->kind == EXPR_NAME) {
            sema_error_at(c, e->pos, "`%.*s` returns no value",
                          (int)e->as.call.callee->as.name.length,
                          e->as.call.callee->as.name.text);
        } else {
            sema_error_at(c, e->pos, "the call returns no value");
        }
        return false;
    }
    sema_error_at(c, e->pos, "expected `%s`, found `%s`", sema_tn(expected),
                  sema_tn(got));
    return false;
}

/* Expressions */

static struct symbol *operator_symbol(struct checker *c, struct type *t,
                                      const char *text);

/* Whether the lanes of type lane are numbers, which `+ - * /` take. An
   f16 lane is one, since each operation reads it as an f32. */
bool sema_simd_numeric(const struct type *lane)
{
    return type_is_numeric(lane) || lane->kind == TYPE_F16;
}

/* Unary `-` and `~` on a simd struct apply lane by lane, with the lanes
   that each takes on one value. */
static struct type *check_simd_unary(struct checker *c, struct expr *e,
                                     struct type *t)
{
    struct type *lane = type_simd_lane(t);

    if (e->as.unary.op == TOKEN_MINUS && !type_is_signed(lane) &&
        !type_is_float(lane) && lane->kind != TYPE_F16) {
        sema_error_at(c, e->pos, "unary `-` needs lanes of signed integers or "
                      "floats, and `%s` has `%s`", sema_tn(t), sema_tn(lane));
        return sema_builtin(c, TYPE_ERROR);
    }
    if (e->as.unary.op == TOKEN_TILDE && !type_is_integer(lane)) {
        sema_error_at(c, e->pos,
                      "unary `~` needs integer lanes, and `%s` has `%s`",
                      sema_tn(t), sema_tn(lane));
        return sema_builtin(c, TYPE_ERROR);
    }
    return t;
}

static struct type *check_unary(struct checker *c, struct expr *e,
                                struct type *expected)
{
    struct expr *operand = e->as.unary.operand;
    struct type *t;

    /* Unary `-` and `~` call `neg` and `not` on a type that declares
       them, as the binary operators call their own. */
    if (e->as.unary.op == TOKEN_MINUS || e->as.unary.op == TOKEN_TILDE) {
        const char *called = e->as.unary.op == TOKEN_MINUS ? "neg" : "not";
        struct symbol *fn;
        if (operand->kind != EXPR_INT && operand->kind != EXPR_FLOAT) {
            t = sema_check_expr(c, operand, NULL);
            operand->type = t;
            if (!sema_is_error(t) && type_is_simd(t)) {
                return check_simd_unary(c, e, t);
            }
            /* A parameter has the operator its constraints give. */
            if (!sema_is_error(t) && t->kind == TYPE_PARAM) {
                return sema_param_operator(c, e, e->as.unary.op, called, t)
                           ? t
                           : sema_builtin(c, TYPE_ERROR);
            }
            if (!sema_is_error(t) &&
                (fn = operator_symbol(c, t, called)) != NULL) {
                struct expr *call = sema_new_node(c, EXPR_CALL, e->pos);
                struct expr *callee = sema_new_node(c, EXPR_FIELD, e->pos);
                callee->as.field.base = operand;
                callee->as.field.name = fn->item->name;
                callee->as.field.promoted = true;
                call->as.call.callee = callee;
                call->as.call.arg_count = 0;
                *e = *call;
                return sema_check_expr(c, e, NULL);
            }
        }
    }
    switch (e->as.unary.op) {
    case TOKEN_MINUS:
        if (operand->kind == EXPR_INT) {
            return integer_literal(c, e, operand, true, expected);
        }
        if (operand->kind == EXPR_FLOAT) {
            return float_literal(c, e, operand, true, expected);
        }
        t = sema_check_expr(c, operand, expected);
        if (sema_refuses_half(c, e->pos, t)) {
            return sema_builtin(c, TYPE_ERROR);
        }
        if (!sema_is_error(t) && !type_is_signed(t) && !type_is_float(t)) {
            sema_error_at(c, e->pos,
                          "unary `-` needs a signed integer or a float, "
                          "found `%s`",
                          sema_tn(t));
            return sema_builtin(c, TYPE_ERROR);
        }
        return t;
    case TOKEN_BANG:
        t = sema_check_test(c, operand);
        if (!sema_is_error(t) && t->kind != TYPE_BOOL) {
            sema_error_at(c, e->pos, "unary `!` needs a `bool`, found `%s`",
                          sema_tn(t));
            return sema_builtin(c, TYPE_ERROR);
        }
        return t;
    case TOKEN_TILDE:
        t = sema_check_expr(c, operand, expected);
        if (!sema_is_error(t) && !type_is_integer(t)) {
            sema_error_at(c, e->pos, "unary `~` needs an integer, found `%s`",
                          sema_tn(t));
            return sema_builtin(c, TYPE_ERROR);
        }
        return t;
    case TOKEN_STAR:
        t = sema_check_expr(c, operand, NULL);
        if (sema_is_error(t)) {
            return t;
        }
        if (t->kind != TYPE_POINTER) {
            sema_error_at(c, e->pos, "unary `*` needs a pointer, found `%s`",
                          sema_tn(t));
            return sema_builtin(c, TYPE_ERROR);
        }
        return sema_usable_pointer(c, operand, t)->element;
    case TOKEN_AMP:
        t = sema_check_storage(c, operand);
        if (sema_is_error(t)) {
            return t;
        }
        if (operand->kind == EXPR_NAME && operand->symbol != NULL &&
            operand->symbol->kind == SYMBOL_CONST) {
            sema_error_at(c, operand->pos, "a constant has no address");
            return sema_builtin(c, TYPE_ERROR);
        }
        if (!sema_is_place(operand)) {
            sema_error_at(c, operand->pos, "unary `&` needs a place");
            return sema_builtin(c, TYPE_ERROR);
        }
        if (is_bitfield(operand)) {
            sema_error_at(c, operand->pos, "a bitfield has no address");
            return sema_builtin(c, TYPE_ERROR);
        }
        sema_mark_address_taken(c, operand);
        return through_lent(operand)
                   ? types_lent(c->types, types_pointer(c->types, t))
                   : types_pointer(c->types, t);
    default:
        return sema_builtin(c, TYPE_ERROR);
    }
}

/* Check both operands of a binary operator so that a literal takes the
   type of the other operand. outer is the type the context expects of
   the result, used when both operands are literals. */
/* Whether e is written `x.carry`, the form a carry into `+` and a borrow
   into `-` take when x is a Flags value. */
static bool names_carry(const struct expr *e)
{
    return e->kind == EXPR_FIELD && !e->as.field.optional &&
           sema_name_is(&e->as.field.name, FLAGS_CARRY);
}

static bool binary_operands(struct checker *c, struct expr *e,
                            struct type *outer, struct type **left,
                            struct type **right)
{
    struct expr *l = e->as.binary.left;
    struct expr *r = e->as.binary.right;

    /* `p == none` and `p != none` are the two comparisons the narrowing
       rule reads, and they are written against a `*T` as readily as
       against a `?*T`. The `none` side takes the nullable form of the
       other, so the comparison names no type the program did not. */
    if (l->kind == EXPR_NONE || r->kind == EXPR_NONE) {
        struct expr *value = l->kind == EXPR_NONE ? r : l;
        struct expr *none = l->kind == EXPR_NONE ? l : r;
        struct type **value_type = l->kind == EXPR_NONE ? right : left;
        struct type **none_type = l->kind == EXPR_NONE ? left : right;
        if (value->kind != EXPR_NONE) {
            *value_type = sema_whole_optional(sema_check_expr(c, value, outer),
                                              value);
            if (sema_is_error(*value_type)) {
                return false;
            }
            /* A value that is no pointer holds `none` only as a `?T`. */
            if (!type_is_nullable(*value_type) &&
                (*value_type)->kind != TYPE_POINTER &&
                (*value_type)->kind != TYPE_FN) {
                sema_error_at(c, none->pos, "`%s` cannot hold `none`",
                              sema_tn(*value_type));
                return false;
            }
            *none_type = sema_check_expr(
                c, none,
                type_is_nullable(*value_type)
                    ? *value_type
                    : types_with_none(c->types, *value_type));
            return !sema_is_error(*left) && !sema_is_error(*right);
        }
    }
    /* A literal beside an f16 takes no type from it, so the refusal of
       the f16 is the one message. A literal before a carry takes the type
       of the context, since the carry is a bool. */
    if (is_untyped(l) && !is_untyped(r) && !names_carry(r)) {
        *right = sema_check_expr(c, r, outer);
        *left = sema_check_expr(c, l, (*right)->kind == TYPE_F16 ? NULL
                                      : sema_is_error(*right)    ? outer
                                                                 : *right);
    } else {
        *left = sema_check_expr(c, l, outer);
        *right = sema_check_expr(c, r, (*left)->kind == TYPE_F16 ? NULL
                                       : sema_is_error(*left)    ? outer
                                                                 : *left);
    }
    if (sema_refuses_half(c, e->pos, *left) ||
        sema_refuses_half(c, e->pos, *right)) {
        return false;
    }
    /* A `?T` takes part in an operator after a test alone, and compares
       with `none` above. */
    if (!sema_is_error(*left) && (*left)->kind == TYPE_OPTIONAL) {
        error_may_be_none(c, l, *left);
        return false;
    }
    if (!sema_is_error(*right) && (*right)->kind == TYPE_OPTIONAL) {
        error_may_be_none(c, r, *right);
        return false;
    }
    return !sema_is_error(*left) && !sema_is_error(*right);
}

const char *sema_op_text(enum token_kind op, char buffer[OP_TEXT])
{
    const char *quoted = token_kind_name(op);

    snprintf(buffer, OP_TEXT, "%.*s", (int)(strlen(quoted) - 2), quoted + 1);
    return buffer;
}

/* DESIGN: the operator table is closed. An operator calls the function
   of that name on the left operand's type, and nothing else is
   overloadable. The table is the one the object model document holds. */
static const char *operator_name(enum token_kind op)
{
    switch (op) {
    case TOKEN_PLUS: return "add";
    case TOKEN_MINUS: return "sub";
    case TOKEN_STAR: return "mul";
    case TOKEN_SLASH: return "div";
    case TOKEN_PERCENT: return "rem";
    case TOKEN_EQ:
    case TOKEN_NE: return "eq";
    case TOKEN_LT:
    case TOKEN_LE:
    case TOKEN_GT:
    case TOKEN_GE: return "lt";
    case TOKEN_AMP: return "and";
    case TOKEN_PIPE: return "or";
    case TOKEN_CARET: return "xor";
    case TOKEN_SHL: return "shl";
    case TOKEN_SHR: return "shr";
    default: return NULL;
    }
}

/* Whether name is one of the nineteen the operator table holds: the
   fourteen operators and the five language hooks. */
bool sema_operator_named(const struct name *name)
{
    static const char *const names[] = {
        "add", "sub", "mul", "div", "rem", "neg", "eq",
        "lt", "and", "or", "xor", "shl", "shr", "not",
        LANG_HOOK_ITER, LANG_HOOK_NEXT, LANG_HOOK_VALUE, LANG_HOOK_INDEX,
        LANG_HOOK_SET_INDEX, LANG_HOOK_HASH
    };
    size_t i;

    for (i = 0; i < sizeof names / sizeof names[0]; i++) {
        if (sema_name_is(name, names[i])) {
            return true;
        }
    }
    return false;
}

/* The operator function `name` that the type t declares, or that the
   module of t declares for a struct. */
static struct symbol *operator_symbol(struct checker *c, struct type *t,
                                      const char *text);

struct symbol *sema_operator_symbol(struct checker *c, struct type *t,
                                    const char *text)
{
    return operator_symbol(c, t, text);
}

/* Whether sym is a function written `operator fn`, of the module being
   checked or of a library file. */
static bool symbol_is_operator(const struct symbol *sym)
{
    return sym->item != NULL ? sym->item->is_operator : sym->is_operator;
}

static struct symbol *operator_symbol(struct checker *c, struct type *t,
                                      const char *text)
{
    struct name name;
    struct item *m;
    struct symbol *sym;

    if (t == NULL || !type_has_fields(t)) {
        return NULL;
    }
    name.text = text;
    name.length = strlen(text);
    m = sema_find_member(t, &name);
    if (m != NULL && m->kind == ITEM_FN && m->is_operator) {
        return m->symbol;
    }
    sym = sema_method_symbol(c, t, &name);
    if (sym != NULL && symbol_is_operator(sym)) {
        return sym;
    }
    return NULL;
}

static struct expr *format_word(struct checker *c, struct pos pos,
                                const struct name *name);
static struct expr *format_field(struct checker *c, struct expr *base,
                                 const char *name);
static struct expr *format_call(struct checker *c, struct expr *callee,
                                struct expr **args, size_t count);

/* DESIGN: a language hook belongs to the type t, or to the type that a
   `*T` points to. A collection is mostly reached through a pointer, and
   a pointer walks nothing of its own. A function of a class body is the
   class's own, inherited ones among them. A free function of the module
   of a struct is its hook when its first parameter is the struct or a
   pointer to it. A hook of another struct of the module is not taken
   for it. */
struct symbol *sema_hook(struct checker *c, struct type *t, const char *text)
{
    struct name name;
    struct item *m;
    struct symbol *sym;
    const struct type *first;

    if (t != NULL && t->kind == TYPE_POINTER && !t->nullable) {
        t = t->element;
    }
    if (t == NULL || !type_has_fields(t) || type_is_simd(t)) {
        return NULL;
    }
    name.text = text;
    name.length = strlen(text);
    m = sema_find_member(t, &name);
    if (m != NULL) {
        return m->kind == ITEM_FN && m->is_operator ? m->symbol : NULL;
    }
    sym = sema_method_symbol(c, t, &name);
    if (sym == NULL || !symbol_is_operator(sym) ||
        sym->type == NULL || sym->type->kind != TYPE_FN ||
        sym->type->param_count == 0) {
        return NULL;
    }
    first = sym->type->params[0];
    if (first->kind == TYPE_POINTER) {
        first = first->element;
    }
    /* A generic hook of a generic struct takes each copy of it. */
    if (first != t && !(first->generic != NULL && first->generic == t->generic &&
                        sema_has_params(first))) {
        return NULL;
    }
    return sym;
}

/* Whether the type t, or the type a `*T` points to, has the hooks of an
   iterator. */
bool sema_is_iterator(struct checker *c, struct type *t)
{
    return sema_hook(c, t, LANG_HOOK_NEXT) != NULL &&
           sema_hook(c, t, LANG_HOOK_VALUE) != NULL;
}

/* The call `base.name(args)` that a construct of the language writes
   for a hook. */
struct expr *sema_hook_call(struct checker *c, struct expr *base,
                            const char *name, struct expr **args,
                            size_t count)
{
    struct expr *callee = format_field(c, base, name);

    callee->as.field.promoted = true;
    return format_call(c, callee, args, count);
}

static const struct name hidden_iterator = {"<iterator>", 10};

/* A call of the hook name on the hidden local of an iteration. */
static struct expr *cursor_call(struct checker *c, struct pos pos,
                                const char *name)
{
    return sema_hook_call(c, format_word(c, pos, &hidden_iterator), name,
                          NULL, 0);
}

/* The field name of base, written by the checker, so the visibility of
   the program does not apply. */
static struct expr *watch_field(struct checker *c, struct expr *base,
                                const struct name *name)
{
    struct expr *e = sema_new_node(c, EXPR_FIELD, base->pos);

    e->as.field.base = base;
    e->as.field.name = *name;
    e->as.field.promoted = true;
    return e;
}

static struct expr *watch_word(struct checker *c, struct expr *base,
                               const char *text)
{
    struct name name;

    name.text = text;
    name.length = strlen(text);
    return watch_field(c, base, &name);
}

/* `<iterator>.watch.changes.at`, or `<iterator>.watch` when at is false,
   a new tree on every call. */
static struct expr *watch_path(struct checker *c, struct pos pos,
                               const struct struct_field *watch, bool at)
{
    struct expr *e = watch_field(c, format_word(c, pos, &hidden_iterator),
                                 &watch->name);

    return at ? watch_word(c, watch_word(c, e, WATCH_CHANGES), CHANGES_AT) : e;
}

/* DESIGN: an iterator watches the changes of its collection through its
   first field of type `anti.lang.Watch`. The loop compares the count of
   the changes with the count the watch remembers before every turn. It
   reads the file and the line of the last change for the message. The
   collection and the iterator hold the two counts, so the compiler
   needs no name of a field of either. Fields of a base are not read. */
static void watch_changes(struct checker *c, struct pos pos,
                          struct iteration *it)
{
    struct type *s = sema_struct_of(it->cursor->type);
    const struct struct_field *watch = NULL;
    struct expr *e;
    size_t i;

    for (i = 0; s != NULL && i < s->field_count && watch == NULL; i++) {
        if (s->fields[i].form == FIELD_PLAIN &&
            types_is_watch(s->fields[i].type)) {
            watch = &s->fields[i];
        }
    }
    if (watch == NULL) {
        return;
    }
    e = sema_new_node(c, EXPR_BINARY, pos);
    e->as.binary.op = TOKEN_NE;
    e->as.binary.left = watch_word(
        c, watch_word(c, watch_path(c, pos, watch, false), WATCH_CHANGES),
        CHANGES_COUNT);
    e->as.binary.right =
        watch_word(c, watch_path(c, pos, watch, false), WATCH_COUNT);
    it->changed = e;
    it->change_file = watch_word(
        c, watch_word(c, watch_path(c, pos, watch, true), LANG_LOCATION_FILE),
        "ptr");
    it->change_file_length = watch_word(
        c, watch_word(c, watch_path(c, pos, watch, true), LANG_LOCATION_FILE),
        "len");
    it->change_line =
        watch_word(c, watch_path(c, pos, watch, true), LANG_LOCATION_LINE);
    if (sema_is_error(sema_check_expr(c, it->changed, NULL)) ||
        sema_is_error(sema_check_expr(c, it->change_file, NULL)) ||
        sema_is_error(sema_check_expr(c, it->change_file_length, NULL)) ||
        sema_is_error(sema_check_expr(c, it->change_line, NULL))) {
        it->changed = NULL;
    }
}

/* DESIGN: an iteration binds its iterator to a hidden local and calls
   `next` and `value` on that local by name, as the `while` form of the
   same loop does. A collection gives a new iterator from `iter`, so every
   loop starts at the beginning and nested loops each keep their own. An
   iterator that stands in a place is walked in that place through its
   address, so the loop advances it and the program reads it afterwards,
   the `post` of `find_all` among them. Any other iterator, a pointer
   among them, is held by the local itself. e has been checked, and t is
   its type. It gives false and writes nothing when t is neither a
   collection nor an iterator. The caller has entered the scope that
   holds the local. */
bool sema_iterate(struct checker *c, struct expr *e, struct type *t,
                  struct iteration *it, struct type **element)
{
    struct expr *start = e;

    /* A value of a type parameter walks as its constraints allow, and so
       does one a pointer reaches, as a collection is walked through one. */
    if (t->kind == TYPE_PARAM) {
        return sema_param_iterate(c, e, t, element);
    }
    if (t->kind == TYPE_POINTER && !t->nullable &&
        t->element->kind == TYPE_PARAM) {
        return sema_param_iterate(c, e, t->element, element);
    }
    if (sema_hook(c, t, LANG_HOOK_ITER) != NULL) {
        start = sema_hook_call(c, e, LANG_HOOK_ITER, NULL, 0);
        t = sema_check_expr(c, start, NULL);
        if (sema_is_error(t)) {
            *element = t;
            return true;
        }
        if (!sema_is_iterator(c, t)) {
            sema_error_at(c, e->pos, "`operator fn iter` gives `%s`, which "
                          "has no `operator fn next` and `operator fn value`",
                          sema_tn(t));
            *element = sema_builtin(c, TYPE_ERROR);
            return true;
        }
    } else if (!sema_is_iterator(c, t)) {
        return false;
    } else if (t->kind != TYPE_POINTER && sema_is_place(e)) {
        start = sema_new_node(c, EXPR_UNARY, e->pos);
        start->as.unary.op = TOKEN_AMP;
        start->as.unary.operand = e;
        t = types_pointer(c->types, t);
        start->type = t;
        sema_mark_address_taken(c, e);
    }
    it->start = start;
    it->cursor = sema_declare(c, SYMBOL_LOCAL, &hidden_iterator, e->pos,
                              "`%.*s` is already declared");
    it->cursor->type = t;
    it->advance = cursor_call(c, e->pos, LANG_HOOK_NEXT);
    it->current = cursor_call(c, e->pos, LANG_HOOK_VALUE);
    if (sema_is_error(sema_check_expr(c, it->advance, NULL))) {
        *element = sema_builtin(c, TYPE_ERROR);
        return true;
    }
    *element = sema_check_expr(c, it->current, NULL);
    /* DESIGN: a `value` that gives a `lent` pointer gives each element in
       place. `for x in &e` binds the pointer, and `for x in e` and
       `to_slice` take a copy of what it points at, so one iterator serves
       both forms of a walk. */
    if (type_is_lent(*element)) {
        struct expr *copy = sema_new_node(c, EXPR_UNARY, e->pos);
        copy->as.unary.op = TOKEN_STAR;
        copy->as.unary.operand = it->current;
        copy->type = (*element)->element;
        it->place = it->current;
        it->current = copy;
        *element = copy->type;
    }
    watch_changes(c, e->pos, it);
    return true;
}

/* DESIGN: `it.to_slice()` collects an iterator into a new `[]T` of the
   type its `value` gives, freed with `free(result.ptr)`. Every iterator
   has it without generics, as `find_all` and `split` return iterators
   that have it. A type that declares a function `to_slice` of its own
   keeps it. The call is the iteration of a `for` whose body appends. */
static bool check_collect(struct checker *c, struct expr *e)
{
    struct expr *callee = e->as.call.callee;
    struct expr *base = callee->as.field.base;
    struct type *t;
    struct type *owner;
    struct type *element = NULL;
    struct iteration it;
    struct scope scope;

    if (e->as.call.arg_count != 0 ||
        !sema_name_is(&callee->as.field.name, LANG_HOOK_TO_SLICE)) {
        return false;
    }
    t = sema_check_expr(c, base, NULL);
    owner = t;
    if (owner->kind == TYPE_POINTER && !owner->nullable) {
        owner = owner->element;
    }
    if (sema_is_error(t) || !type_has_fields(owner) ||
        sema_method_symbol(c, owner, &callee->as.field.name) != NULL ||
        !sema_is_iterator(c, t)) {
        return false;
    }
    memset(&it, 0, sizeof it);
    sema_enter_scope(c, &scope);
    sema_iterate(c, base, t, &it, &element);
    sema_leave_scope(c, &scope);
    e->kind = EXPR_COLLECT;
    e->as.collect = it;
    e->type = sema_is_error(element) ? NULL : types_slice(c->types, element);
    return true;
}

/* Rewrite `a op b` into the call the operator names. `!=`, `>`, `<=` and
   `>=` derive from `eq` and `lt`, so a type declares two functions and
   gets six operators. */
/* The receiver of an operator call: the operand itself, or its address
   when the function takes `self`. */
static struct expr *operator_receiver(struct checker *c, struct expr *a,
                                      struct type *first)
{
    struct expr *address;

    if (first->kind != TYPE_POINTER || a->type == first) {
        return a;
    }
    if (!sema_is_place(a)) {
        struct expr *slot = sema_new_node(c, EXPR_UNARY, a->pos);
        slot->as.unary.op = TOKEN_AMP;
        slot->as.unary.operand = a;
        slot->type = first;
        return slot;
    }
    sema_mark_address_taken(c, a);
    address = sema_new_node(c, EXPR_UNARY, a->pos);
    address->as.unary.op = TOKEN_AMP;
    address->as.unary.operand = a;
    address->type = first;
    return address;
}

static struct type *check_operator(struct checker *c, struct expr *e,
                                   struct type *right, struct symbol *fn)
{
    enum token_kind op = e->as.binary.op;
    bool negate = op == TOKEN_NE || op == TOKEN_LE || op == TOKEN_GE;
    bool swap = op == TOKEN_GT || op == TOKEN_LE;
    struct expr *a = swap ? e->as.binary.right : e->as.binary.left;
    struct expr *b = swap ? e->as.binary.left : e->as.binary.right;
    struct type *sig = fn->type;
    struct expr *call = sema_new_node(c, EXPR_CALL, e->pos);
    struct expr *callee = sema_new_node(c, EXPR_NAME, e->pos);
    struct expr **args = arena_alloc(c->arena, 2 * sizeof *args);

    sig = sema_operator_copy(c, call, sig, fn, a->type, b->type);
    if (sig == NULL) {
        return sema_builtin(c, TYPE_ERROR);
    }
    if (sig->param_count != 2) {
        sema_error_at(c, e->pos,
                      "`operator fn %.*s` takes one operand beside its "
                      "own", (int)fn->name.length, fn->name.text);
        return sema_builtin(c, TYPE_ERROR);
    }
    if (!sema_require(c, b, right, sig->params[1])) {
        return sema_builtin(c, TYPE_ERROR);
    }
    callee->symbol = fn;
    callee->type = sig;
    callee->as.name = fn->name;
    args[0] = operator_receiver(c, a, sig->params[0]);
    args[1] = b;
    call->as.call.callee = callee;
    call->as.call.args = args;
    call->as.call.arg_count = 2;
    call->type = sig->result;
    if (!negate) {
        *e = *call;
        return sig->result;
    }
    e->kind = EXPR_UNARY;
    e->as.unary.op = TOKEN_BANG;
    e->as.unary.operand = call;
    e->type = sig->result;
    return sig->result;
}

/* DESIGN: `==` on two class pointers compares the identity of the
   objects. A pointer to one interface of an object and a pointer to
   another are therefore equal. Their types differ, and the comparison is
   still the one the program means. Two pointers of one element compare
   whether or not either of them may hold `none`, which is what
   `p != none` is written for. */
static bool comparable_pointers(const struct type *a, const struct type *b)
{
    if (a->kind == TYPE_FN && b->kind == TYPE_FN) {
        return a->params == b->params && a->param_count == b->param_count &&
               a->result == b->result && a->bound == b->bound &&
               a->may_fail == b->may_fail && a->has_out == b->has_out;
    }
    if (a->kind != TYPE_POINTER || b->kind != TYPE_POINTER) {
        return false;
    }
    return a->element == b->element ||
           (a->element->kind == TYPE_CLASS && b->element->kind == TYPE_CLASS);
}

static struct type *check_coalesce(struct checker *c, struct expr *e,
                                   struct type *expected);

/* DESIGN: an operator on two values of one simd struct applies lane by
   lane. `+ - * /` take numbers, the bitwise operators take integers, and
   a comparison takes the lanes it takes on one value and gives the mask.
   `%`, the wrapping and saturating operators and the logic operators
   are not among the operators of the section, so they are refused. */
static struct type *check_simd_binary(struct checker *c, struct expr *e,
                                      struct type *left, struct type *right)
{
    enum token_kind op = e->as.binary.op;
    char spelling[OP_TEXT];
    const char *o = sema_op_text(op, spelling);
    struct type *lane;

    if (left != right) {
        sema_error_at(c, e->pos, "the operands of `%s` have the types `%s` and "
                      "`%s`", o, sema_tn(left), sema_tn(right));
        return sema_builtin(c, TYPE_ERROR);
    }
    lane = type_simd_lane(left);
    switch (op) {
    case TOKEN_PLUS:
    case TOKEN_MINUS:
    case TOKEN_STAR:
    case TOKEN_SLASH:
        if (!sema_simd_numeric(lane)) {
            sema_error_at(c, e->pos,
                          "`%s` needs lanes of numbers, and `%s` has "
                          "`%s`", o, sema_tn(left), sema_tn(lane));
            return sema_builtin(c, TYPE_ERROR);
        }
        return left;
    case TOKEN_AMP:
    case TOKEN_PIPE:
    case TOKEN_CARET:
    case TOKEN_SHL:
    case TOKEN_SHR:
        if (!type_is_integer(lane)) {
            sema_error_at(c, e->pos,
                          "`%s` needs integer lanes, and `%s` has `%s`",
                          o, sema_tn(left), sema_tn(lane));
            return sema_builtin(c, TYPE_ERROR);
        }
        return left;
    case TOKEN_EQ:
    case TOKEN_NE:
        if (!sema_simd_numeric(lane) && lane->kind != TYPE_CHAR &&
            lane->kind != TYPE_BOOL) {
            sema_error_at(c, e->pos, "`%s` is not defined on the lanes of `%s`",
                          o,
                          sema_tn(left));
            return sema_builtin(c, TYPE_ERROR);
        }
        return types_mask(c->types, left);
    case TOKEN_LT:
    case TOKEN_LE:
    case TOKEN_GT:
    case TOKEN_GE:
        if (!sema_simd_numeric(lane) && lane->kind != TYPE_CHAR) {
            sema_error_at(c, e->pos,
                          "`%s` needs lanes of numbers or `char`, and "
                          "`%s` has `%s`", o, sema_tn(left), sema_tn(lane));
            return sema_builtin(c, TYPE_ERROR);
        }
        return types_mask(c->types, left);
    default:
        sema_error_at(c, e->pos, "`%s` does not apply to a `simd struct`", o);
        return sema_builtin(c, TYPE_ERROR);
    }
}

/* DESIGN: an operator on a type parameter calls the hook of the
   operator table, which its constraints must give. Both operands are the
   same parameter, as for any operator, and the result is the parameter,
   or bool for a comparison. */
static struct type *param_binary(struct checker *c, struct expr *e,
                                 const char *called, struct type *left,
                                 struct type *right)
{
    enum token_kind op = e->as.binary.op;
    struct type *param = left->kind == TYPE_PARAM ? left : right;
    char spelling[OP_TEXT];

    if (called == NULL) {
        sema_error_at(c, e->pos, "`%s` is not defined on `%s`",
                      sema_op_text(op, spelling), sema_tn(param));
        return sema_builtin(c, TYPE_ERROR);
    }
    if (!sema_param_operator(c, e, op, called, param)) {
        return sema_builtin(c, TYPE_ERROR);
    }
    if (left != right) {
        sema_error_at(c, e->pos, "the operands of `%s` have the types `%s` and "
                      "`%s`", sema_op_text(op, spelling), sema_tn(left),
                      sema_tn(right));
        return sema_builtin(c, TYPE_ERROR);
    }
    switch (op) {
    case TOKEN_EQ:
    case TOKEN_NE:
    case TOKEN_LT:
    case TOKEN_LE:
    case TOKEN_GT:
    case TOKEN_GE:
        return sema_builtin(c, TYPE_BOOL);
    default:
        return left;
    }
}

struct type *sema_check_binary(struct checker *c, struct expr *e,
                               struct type *expected)
{
    enum token_kind op = e->as.binary.op;
    struct type *left;
    struct type *right;
    char spelling[OP_TEXT];
    const char *o = sema_op_text(op, spelling);
    const char *called = operator_name(op);

    switch (op) {
    case TOKEN_QUESTION_QUESTION:
        return check_coalesce(c, e, expected);
    case TOKEN_AND_AND:
    case TOKEN_OR_OR: {
        /* The right operand runs only where the left one decided it,
           so it sees the names the left proved. `p != none && p.n > 0`
           and `p == none || p.n > 0` both read `p` as checked. */
        struct symbol *proved[PROVED_MAX];
        size_t count;
        struct scope narrowed;
        size_t i;
        left = sema_check_test(c, e->as.binary.left);
        count = sema_proved_names(e->as.binary.left, op == TOKEN_AND_AND,
                                  proved,
                                  0);
        sema_enter_scope(c, &narrowed);
        for (i = 0; i < count; i++) {
            sema_narrow(c, proved[i], sema_proved_type(c, proved[i]));
        }
        right = sema_check_test(c, e->as.binary.right);
        sema_leave_scope(c, &narrowed);
        if (sema_is_error(left) || sema_is_error(right)) {
            return sema_builtin(c, TYPE_ERROR);
        }
        if (left->kind != TYPE_BOOL || right->kind != TYPE_BOOL) {
            sema_error_at(c, e->pos, "`%s` needs `bool` operands, found `%s`",
                          o,
                          sema_tn(left->kind != TYPE_BOOL ? left : right));
            return sema_builtin(c, TYPE_ERROR);
        }
        return left;
    }
    case TOKEN_EQ:
    case TOKEN_NE:
    case TOKEN_LT:
    case TOKEN_LE:
    case TOKEN_GT:
    case TOKEN_GE:
        if (!binary_operands(c, e, NULL, &left, &right)) {
            return sema_builtin(c, TYPE_ERROR);
        }
        /* A `?T` of a value compares with `none` alone, which reads its
           flag. */
        if ((op == TOKEN_EQ || op == TOKEN_NE) &&
            (e->as.binary.left->kind == EXPR_NONE ||
             e->as.binary.right->kind == EXPR_NONE) &&
            (left->kind == TYPE_OPTIONAL || right->kind == TYPE_OPTIONAL)) {
            return sema_builtin(c, TYPE_BOOL);
        }
        if (left->kind == TYPE_PARAM || right->kind == TYPE_PARAM) {
            return param_binary(c, e, called, left, right);
        }
        if (type_is_simd(left) || type_is_simd(right)) {
            return check_simd_binary(c, e, left, right);
        }
        if (called != NULL) {
            struct symbol *fn = operator_symbol(c, left, called);
            if (fn != NULL) {
                return check_operator(c, e, right, fn);
            }
        }
        if (left != right &&
            !((op == TOKEN_EQ || op == TOKEN_NE) &&
              comparable_pointers(left, right))) {
            sema_error_at(c, e->pos,
                          "the operands of `%s` have the types `%s` and "
                          "`%s`", o, sema_tn(left), sema_tn(right));
            return sema_builtin(c, TYPE_ERROR);
        }
        /* DESIGN: a `str` has the hooks `eq` and `lt`. `==` compares
           the text and `<` its bytes as unsigned numbers, which is the
           order of the code points for UTF-8. */
        if (op == TOKEN_EQ || op == TOKEN_NE) {
            if (type_has_fields(left) || left->kind == TYPE_ARRAY ||
                left->kind == TYPE_SLICE) {
                sema_error_at(c, e->pos, "`%s` is not defined on `%s`", o,
                              sema_tn(left));
                return sema_builtin(c, TYPE_ERROR);
            }
        } else if (!type_is_numeric(left) && left->kind != TYPE_CHAR &&
                   left->kind != TYPE_STR) {
            sema_error_at(c, e->pos,
                          "`%s` needs numeric, `char` or `str` operands, "
                          "found `%s`", o, sema_tn(left));
            return sema_builtin(c, TYPE_ERROR);
        }
        return sema_builtin(c, TYPE_BOOL);
    default:
        if (!binary_operands(c, e, expected, &left, &right)) {
            return sema_builtin(c, TYPE_ERROR);
        }
        if (left->kind == TYPE_PARAM || right->kind == TYPE_PARAM) {
            return param_binary(c, e, called, left, right);
        }
        if (type_is_simd(left) || type_is_simd(right)) {
            return check_simd_binary(c, e, left, right);
        }
        if (called != NULL) {
            struct symbol *fn = operator_symbol(c, left, called);
            if (fn != NULL) {
                return check_operator(c, e, right, fn);
            }
        }
        /* DESIGN: `a + f.carry` and `a - f.carry` take the `carry` of a
           Flags value as a carry or a borrow into the operation. The
           field is a bool, and the operation keeps the type of a. */
        if ((op == TOKEN_PLUS || op == TOKEN_MINUS) &&
            names_carry(e->as.binary.right) &&
            types_is_flags(sema_struct_of(
                e->as.binary.right->as.field.base->type))) {
            if (!type_is_integer(left)) {
                sema_error_at(c, e->pos,
                              "a carry goes into an integer, found `%s`",
                              sema_tn(left));
                return sema_builtin(c, TYPE_ERROR);
            }
            e->as.binary.carry = true;
            return left;
        }
        if (left != right &&
            !((op == TOKEN_EQ || op == TOKEN_NE) &&
              comparable_pointers(left, right))) {
            sema_error_at(c, e->pos,
                          "the operands of `%s` have the types `%s` and "
                          "`%s`", o, sema_tn(left), sema_tn(right));
            return sema_builtin(c, TYPE_ERROR);
        }
        if (op == TOKEN_PLUS || op == TOKEN_MINUS || op == TOKEN_STAR ||
            op == TOKEN_SLASH) {
            if (!type_is_numeric(left)) {
                sema_error_at(c, e->pos,
                              "`%s` needs numeric operands, found `%s`",
                              o, sema_tn(left));
                return sema_builtin(c, TYPE_ERROR);
            }
        } else if (!type_is_integer(left)) {
            sema_error_at(c, e->pos, "`%s` needs integer operands, found `%s`",
                          o,
                          sema_tn(left));
            return sema_builtin(c, TYPE_ERROR);
        }
        if ((op == TOKEN_SLASH || op == TOKEN_PERCENT || op == TOKEN_SHL ||
             op == TOKEN_SHR) && type_is_integer(left) &&
            sema_undefined_on_constants(c, e, left)) {
            return sema_builtin(c, TYPE_ERROR);
        }
        return left;
    }
}

/* The conversion table of chapter 2. */
static bool can_convert(const struct type *from, const struct type *to)
{
    /* An f16 is made from an f32 and read as one, and it converts to
       nothing else. */
    if (from->kind == TYPE_F16 || to->kind == TYPE_F16) {
        return (from->kind == TYPE_F16 || from->kind == TYPE_F32) &&
               (to->kind == TYPE_F16 || to->kind == TYPE_F32);
    }
    /* DESIGN: an enum converts to and from its underlying type and to
       any other numeric type, as a C enum does. Its values carry no
       other meaning to the compiler. */
    if (from->kind == TYPE_ENUM) {
        from = from->base;
    }
    if (to->kind == TYPE_ENUM) {
        to = to->base;
    }
    if (type_is_numeric(from) && type_is_numeric(to)) {
        return true;
    }
    if (from->kind == TYPE_BOOL && type_is_integer(to)) {
        return true;
    }
    if ((from->kind == TYPE_CHAR && to->kind == TYPE_U32) ||
        (from->kind == TYPE_U32 && to->kind == TYPE_CHAR)) {
        return true;
    }
    return from->kind == TYPE_POINTER && to->kind == TYPE_POINTER;
}

/* Whether a is b or a class below b, so a pointer to a converts to a
   pointer to b without a check. */
bool sema_descends_from(const struct type *a, const struct type *b)
{
    for (; a != NULL; a = a->kind == TYPE_CLASS ? a->base : NULL) {
        if (a == b) {
            return true;
        }
    }
    return false;
}

/* DESIGN: `p is *T` and `p as *T` on a class pointer compare the
   ancestor of the object at T's depth with T's descriptor. `is` gives a
   bool, `as` traps on a mismatch and `as?` gives `none`. Both need a class
   pointer on each side, and a conversion up the chain is the implicit
   one, which needs no check. */
static struct type *check_class_cast(struct checker *c, struct expr *e,
                                     struct type *from, struct type *to)
{
    const char *op = e->as.cast.test ? "is" : "as";

    if (from->kind != TYPE_POINTER || from->element->kind != TYPE_CLASS ||
        to->kind != TYPE_POINTER || to->element->kind != TYPE_CLASS) {
        sema_error_at(c, e->pos,
                      "`%s` needs a class pointer on each side, found "
                      "`%s` and `%s`", op, sema_tn(from), sema_tn(to));
        return sema_builtin(c, TYPE_ERROR);
    }
    /* A conversion to an interface the class implements is the implicit
       one. It needs no check, only the offset of the sub-object. */
    if (!e->as.cast.test) {
        const struct struct_field *iface =
            converts_to_interface(c, e, from, to);
        if (iface != NULL) {
            e->as.cast.operand->to_iface = iface;
            e->as.cast.target = NULL;
            return to;
        }
    }
    /* A cast from an interface pointer down to a class that implements
       it reaches the object through the offset the descriptor holds. */
    if (sema_implemented_in(to->element, from->element)) {
        e->as.cast.from_sub = true;
    } else if (!sema_descends_from(to->element, from->element) &&
               !sema_descends_from(from->element, to->element)) {
        sema_error_at(c, e->pos,
                      "`%s` is never `%s`, the classes share no chain",
                      sema_tn(from), sema_tn(to));
        return sema_builtin(c, TYPE_ERROR);
    }
    e->as.cast.target = to->element;
    return e->as.cast.test ? sema_builtin(c, TYPE_BOOL) : to;
}

/* A float literal, alone or after unary `-`. */
static bool is_float_literal(const struct expr *e)
{
    if (e->kind == EXPR_UNARY && e->as.unary.op == TOKEN_MINUS) {
        e = e->as.unary.operand;
    }
    return e->kind == EXPR_FLOAT;
}

/* DESIGN: `v is Shape.Circle` tests the tag of v. The case is named by
   its variant, which is the type of v, as a literal names it. A copy of
   a generic variant takes the name of the generic, `r is Result.Ok`,
   since the type of v gives the arguments. */
static struct type *check_variant_test(struct checker *c, struct expr *e,
                                       struct type *from)
{
    const struct type_expr *target = e->as.cast.type;
    const struct type *named = NULL;
    const struct name *which;
    size_t index;

    /* A variant without cases has had its message and has no case to
       give as the example. */
    if ((target->kind != TYPEX_NAMED || target->module.length == 0) &&
        from->base->field_count == 0) {
        sema_error_at(c, e->pos, "`is` on `%s` names one of its cases",
                      sema_tn(from));
        return sema_builtin(c, TYPE_ERROR);
    }
    if (target->kind != TYPEX_NAMED || target->module.length == 0) {
        sema_error_at(c, e->pos, "`is` on `%s` names one of its cases, as "
                      "`%s.%.*s`", sema_tn(from), sema_tn(from),
                      (int)sema_case_name(from, 0)->length,
                      sema_case_name(from, 0)->text);
        return sema_builtin(c, TYPE_ERROR);
    }
    if (target->member.length > 0) {
        named = sema_imported_struct(c, &target->module, &target->name,
                                     target->pos);
        if (sema_is_error(named)) {
            return sema_builtin(c, TYPE_ERROR);
        }
        which = &target->member;
    } else {
        const struct symbol *sym = sema_lookup(c, &target->module);
        named = sym != NULL && sym->kind == SYMBOL_STRUCT ? sym->type : NULL;
        which = &target->name;
    }
    if (named != from && (from->generic == NULL || named != from->generic)) {
        sema_error_at(c, e->pos, "`%.*s.%.*s` is not a case of `%s`",
                      (int)target->module.length, target->module.text,
                      (int)target->name.length, target->name.text,
                      sema_tn(from));
        return sema_builtin(c, TYPE_ERROR);
    }
    if (!types_case_index(from, which, &index)) {
        sema_error_at(c, e->pos, "`%s` has no case `%.*s`", sema_tn(from),
                      (int)which->length, which->text);
        return sema_builtin(c, TYPE_ERROR);
    }
    e->as.cast.variant_case = (uint32_t)(index + 1);
    e->as.cast.target = from;
    return sema_builtin(c, TYPE_BOOL);
}

/* The size and the alignment of t, which are the same on every target,
   as the C rules lay it out. False for a type that holds a width the
   target decides, a symbolic length, a bitfield or a table. */
static bool fixed_layout(const struct type *t, uint64_t *size,
                         uint64_t *align)
{
    uint64_t offset = 0;
    uint64_t most = 1;
    size_t i;

    switch (t->kind) {
    case TYPE_POINTER:
        *size = 8;
        *align = 8;
        return true;
    case TYPE_FN:
    case TYPE_STR:
    case TYPE_SLICE:
        *size = t->kind == TYPE_FN && !t->bound && !t->context ? 8 : 16;
        *align = 8;
        return true;
    case TYPE_ENUM:
        return fixed_layout(t->base, size, align);
    case TYPE_ARRAY:
        if (t->length_of != NULL || !fixed_layout(t->element, size, align) ||
            (t->length != 0 && *size > UINT64_MAX / t->length)) {
            return false;
        }
        *size *= t->length;
        return true;
    case TYPE_STRUCT:
    case TYPE_TUPLE:
        for (i = 0; i < t->field_count; i++) {
            uint64_t n;
            uint64_t a;
            if (t->fields[i].bits != 0 ||
                !fixed_layout(t->fields[i].type, &n, &a)) {
                return false;
            }
            a = t->packed ? 1 : a;
            most = a > most ? a : most;
            if (t->is_union) {
                offset = n > offset ? n : offset;
            } else if (offset > UINT64_MAX - (a - 1) ||
                       (offset + a - 1) / a * a > UINT64_MAX - n) {
                return false;
            } else {
                offset = (offset + a - 1) / a * a + n;
            }
        }
        if (t->simd) {
            most = offset < 16 ? offset : 16;
        }
        most = t->align > most ? t->align : most;
        if (offset > UINT64_MAX - (most - 1)) {
            return false;
        }
        *size = (offset + most - 1) / most * most;
        *align = most;
        return true;
    default:
        *size = type_lane_bytes(t);
        *align = *size;
        return *size != 0;
    }
}

/* DESIGN: `as` between a simd struct and an array or a plain struct of
   the same bytes is free both ways, since it copies the bytes. The two
   have one size on every target, which the checker computes by the C
   rules. Two simd structs do not convert into each other, and neither
   does a class, a union or a variant, which are no plain structs. */
static struct type *check_simd_cast(struct checker *c, struct expr *e,
                                    struct type *from, struct type *to)
{
    struct type *other = type_is_simd(from) ? to : from;
    uint64_t from_size;
    uint64_t to_size;
    uint64_t align;

    if ((other->kind != TYPE_ARRAY &&
         (other->kind != TYPE_STRUCT || other->is_union || other->simd)) ||
        !fixed_layout(from, &from_size, &align) ||
        !fixed_layout(to, &to_size, &align)) {
        sema_error_at(c, e->pos, "cannot convert `%s` to `%s`, a `simd struct` "
                      "converts to an array or a plain struct of the same "
                      "bytes", sema_tn(from), sema_tn(to));
        return sema_builtin(c, TYPE_ERROR);
    }
    if (from_size != to_size) {
        sema_error_at(c, e->pos, "cannot convert `%s` of %llu bytes to `%s` of "
                      "%llu", sema_tn(from), (unsigned long long)from_size,
                      sema_tn(to),
                      (unsigned long long)to_size);
        return sema_builtin(c, TYPE_ERROR);
    }
    return to;
}

static struct type *check_cast(struct checker *c, struct expr *e)
{
    const struct type_expr *target = e->as.cast.type;
    struct expr *operand = e->as.cast.operand;
    /* A float literal before `as f16` is an f32, the one type an f16 is
       made from. The read the checker wrote converts the f16 itself. */
    struct type *from =
        e->as.cast.promoted ? sema_check_storage(c, operand)
        : target->kind == TYPEX_BUILTIN && target->builtin == TOKEN_F16 &&
                is_float_literal(operand)
            ? sema_check_expr(c, operand, sema_builtin(c, TYPE_F32))
            : sema_check_expr(c, operand, NULL);
    struct type *to;

    if (e->as.cast.test && !sema_is_error(from) && from->kind == TYPE_VARIANT) {
        return check_variant_test(c, e, from);
    }
    if (e->as.cast.test && !sema_is_error(from) && from->kind == TYPE_POINTER &&
        from->element->kind == TYPE_VARIANT) {
        sema_error_at(c, e->pos, "`is` takes a variant as a value, found the "
                      "pointer `%s`", sema_tn(from));
        return sema_builtin(c, TYPE_ERROR);
    }
    if (target->kind == TYPEX_NAMED && target->member.length > 0) {
        if (!sema_is_error(from)) {
            sema_error_at(c, e->pos, "`is` names a case on a variant alone, "
                          "found `%s`", sema_tn(from));
        }
        return sema_builtin(c, TYPE_ERROR);
    }
    to = sema_resolve_type(c, e->as.cast.type);
    if (sema_is_error(from) || sema_is_error(to)) {
        return sema_builtin(c, TYPE_ERROR);
    }
    if (e->as.cast.test || e->as.cast.checked ||
        (from->kind == TYPE_POINTER && from->element->kind == TYPE_CLASS &&
         to->kind == TYPE_POINTER && to->element->kind == TYPE_CLASS)) {
        return check_class_cast(c, e, from, to);
    }
    if (type_is_simd(from) || type_is_simd(to)) {
        return check_simd_cast(c, e, from, to);
    }
    if (!can_convert(from, to)) {
        sema_error_at(c, e->pos, "cannot convert `%s` to `%s`", sema_tn(from),
                      sema_tn(to));
        return sema_builtin(c, TYPE_ERROR);
    }
    if (type_is_float(from) && type_is_integer(to) &&
        sema_undefined_on_constants(c, e, to)) {
        return sema_builtin(c, TYPE_ERROR);
    }
    return to;
}

/* Interpolation */

/* DESIGN: an `f"..."` is checked as the calls it makes on a local
   `anti.text.Builder`: `new` makes it, `append` writes each text, one
   `append_*` of the value's type writes each `{expr}` and `take` gives
   the `str`. The checker writes those calls as nodes and checks them as
   it checks any call, so the functions are found, and their arguments
   converted, by the rules a program's own call follows. The names they
   use cannot be written in a program: `<text>` is the module, `<builder>`
   the local and `<value>` the value of one `{expr}`, which is checked
   once and bound to that local. Each lives in a scope of its own. The
   module that writes the literal imports `anti.text`, as the one that
   writes `here` imports `anti.lang`. */
static const struct name hidden_module = {"<text>", 6};

static const struct name hidden_builder = {"<builder>", 9};

const struct name sema_hidden_value = {"<value>", 7};

static const struct name hidden_object = {"<object>", 8};

/* The name of the literal in a message. */
const char *sema_format_name(const struct expr *e)
{
    return e->as.format.raw ? "`rf\"...\"`" : "`f\"...\"`";
}

static struct expr *format_word(struct checker *c, struct pos pos,
                                const struct name *name)
{
    struct expr *e = sema_new_node(c, EXPR_NAME, pos);
    e->as.name = *name;
    return e;
}

static struct expr *format_field(struct checker *c, struct expr *base,
                                 const char *name)
{
    struct expr *e = sema_new_node(c, EXPR_FIELD, base->pos);
    e->as.field.base = base;
    e->as.field.name.text = name;
    e->as.field.name.length = strlen(name);
    return e;
}

static struct expr *format_call(struct checker *c, struct expr *callee,
                                struct expr **args, size_t count)
{
    struct expr *e = sema_new_node(c, EXPR_CALL, callee->pos);
    e->as.call.callee = callee;
    if (count > 0) {
        e->as.call.args = types_alloc_array(c->arena, count, sizeof *args);
        memcpy(e->as.call.args, args, count * sizeof *args);
    }
    e->as.call.arg_count = count;
    return e;
}

/* A call of function name of `anti.text.Builder` on the local builder. */
static struct expr *builder_call(struct checker *c, struct pos pos,
                                 const char *name, struct expr **args,
                                 size_t count)
{
    return format_call(
        c, format_field(c, format_word(c, pos, &hidden_builder), name), args,
        count);
}

static struct expr *format_number(struct checker *c, struct pos pos,
                                  int64_t value)
{
    struct expr *e = sema_new_node(c, EXPR_INT, pos);
    struct expr *minus;

    e->as.integer = (uint64_t)(value < 0 ? -value : value);
    if (value >= 0) {
        return e;
    }
    minus = sema_new_node(c, EXPR_UNARY, pos);
    minus->as.unary.op = TOKEN_MINUS;
    minus->as.unary.operand = e;
    return minus;
}

static struct expr *format_truth(struct checker *c, struct pos pos, bool value)
{
    struct expr *e = sema_new_node(c, EXPR_BOOL, pos);
    e->as.boolean = value;
    return e;
}

/* `<text>.Align.<name>`. */
static struct expr *format_align(struct checker *c, struct pos pos,
                                 char align, const char *fallback)
{
    const char *name = align == '<'   ? TEXT_ALIGN_LEFT
                       : align == '>' ? TEXT_ALIGN_RIGHT
                       : align == '^' ? TEXT_ALIGN_CENTER
                                      : fallback;
    return format_field(
        c, format_field(c, format_word(c, pos, &hidden_module), TEXT_ALIGN),
        name);
}

/* `<value> as T`, for T a builtin type keyword. */
static struct expr *format_cast(struct checker *c, struct expr *value,
                                enum token_kind type)
{
    struct expr *e = sema_new_node(c, EXPR_CAST, value->pos);
    struct type_expr *t = arena_alloc(c->arena, sizeof *t);

    t->kind = TYPEX_BUILTIN;
    t->pos = value->pos;
    t->builtin = type;
    e->as.cast.operand = value;
    e->as.cast.type = t;
    return e;
}

/* Whether a value of type t is an object, which `to_text` writes. A
   `?*T` is refused, since it may hold no object. */
static bool format_object(const struct type *t)
{
    return t->kind == TYPE_CLASS ||
           (t->kind == TYPE_POINTER && !t->nullable &&
            t->element->kind == TYPE_CLASS);
}

/* The call that appends `<value>`, of type t, with the format
   specification of part, or NULL after an error. An integer goes to
   `append_int` or `append_uint` as an `int` or a `u64`, a float to
   `append_float` or `append_f32`, and a `bool`, a `char` and a `str` to
   their own. An object is written by its `to_text`. Every argument is
   given, so the defaults of the functions stay out of the literal. */
static struct expr *format_value_call(struct checker *c,
                                      const struct expr *e,
                                      const struct format_part *part,
                                      struct type *t)
{
    const struct format_spec *spec = &part->spec;
    struct pos pos = part->value->pos;
    struct expr *value = format_word(c, pos, &sema_hidden_value);
    int64_t width = spec->width < 0 ? 0 : spec->width;
    struct expr *args[6];
    const char *name;
    bool fits;

    if (type_is_integer(t)) {
        bool is_signed = type_is_signed(t);
        fits = spec->precision < 0 && spec->kind != 'e' && spec->kind != 'f';
        name = is_signed ? TEXT_APPEND_INT : TEXT_APPEND_UINT;
        args[0] = t->kind == TYPE_I64 || t->kind == TYPE_U64
                      ? value
                      : format_cast(c, value,
                                    is_signed ? TOKEN_INT_TYPE : TOKEN_U64);
        args[1] = format_number(c, pos,
                                spec->kind == 'x' || spec->kind == 'X' ? 16
                                : spec->kind == 'b'                    ? 2
                                : spec->kind == 'o'                    ? 8
                                                                       : 10);
        args[2] = format_truth(c, pos, spec->kind == 'X');
        args[3] = format_number(c, pos, width);
        args[4] = format_align(c, pos, spec->align, TEXT_ALIGN_RIGHT);
        args[5] = format_truth(c, pos, spec->zero);
    } else if (type_is_float(t)) {
        fits = spec->kind == 0 || spec->kind == 'e' || spec->kind == 'f';
        name = t->kind == TYPE_F32 ? TEXT_APPEND_F32 : TEXT_APPEND_FLOAT;
        args[0] = value;
        args[1] = format_number(c, pos,
                                spec->precision >= 0 ? spec->precision
                                : spec->kind != 0    ? 6
                                                     : -1);
        args[2] = format_truth(c, pos, spec->kind == 'e');
        args[3] = format_number(c, pos, width);
        args[4] = format_align(c, pos, spec->align, TEXT_ALIGN_RIGHT);
        args[5] = format_truth(c, pos, spec->zero);
    } else if (t->kind == TYPE_BOOL || t->kind == TYPE_CHAR ||
               t->kind == TYPE_STR || format_object(t)) {
        fits = spec->kind == 0 && spec->precision < 0 && !spec->zero;
        name = t->kind == TYPE_BOOL   ? TEXT_APPEND_BOOL
               : t->kind == TYPE_CHAR ? TEXT_APPEND_CHAR
                                      : TEXT_APPEND_TEXT;
        args[0] = format_object(t)
                      ? format_call(c, format_field(c, value, ROOT_TO_TEXT),
                                    NULL, 0)
                      : value;
        args[1] = format_number(c, pos, width);
        args[2] = format_align(c, pos, spec->align, TEXT_ALIGN_LEFT);
    } else {
        sema_error_at(c, pos, "%s cannot write a `%s`", sema_format_name(e),
                      sema_tn(t));
        return NULL;
    }
    if (!fits) {
        sema_error_at(c, part->pos, "unknown format `%.*s` for `%s`",
                      (int)part->source.length, part->source.bytes, sema_tn(t));
        return NULL;
    }
    return builder_call(c, pos, name, args,
                        type_is_integer(t) || type_is_float(t) ? 6 : 3);
}

/* Check the value of one `{expr}`, bind it to `<value>` in a scope of
   its own, and check the call that appends it. */
static bool check_format_value(struct checker *c, const struct expr *e,
                               struct format_part *part)
{
    struct type *t = sema_check_expr(c, part->value, NULL);
    struct scope scope;
    bool ok;

    if (sema_is_error(t)) {
        return false;
    }
    sema_enter_scope(c, &scope);
    part->bound = sema_declare(c, SYMBOL_LOCAL, &sema_hidden_value,
                               part->value->pos,
                               "`%.*s` is already declared");
    part->bound->type = t;
    part->value_call = format_value_call(c, e, part, t);
    ok = part->value_call != NULL &&
         !sema_is_error(sema_check_expr(c, part->value_call, NULL));
    sema_leave_scope(c, &scope);
    return ok;
}

/* Whether the class `anti.text.Builder` has every function an `f"..."`
   calls. A library of another version may lack one, and the message then
   names it rather than the name only the checker writes. */
static bool format_api(struct checker *c, const struct expr *e,
                       const struct type *builder)
{
    static const char *const names[] = {
        TEXT_NEW, TEXT_APPEND, TEXT_APPEND_INT, TEXT_APPEND_UINT,
        TEXT_APPEND_FLOAT, TEXT_APPEND_F32, TEXT_APPEND_BOOL,
        TEXT_APPEND_CHAR, TEXT_APPEND_TEXT, TEXT_TAKE
    };
    size_t count = sizeof names / sizeof names[0];
    size_t i;

    for (i = 0; i < count; i++) {
        struct name name;
        name.text = names[i];
        name.length = strlen(names[i]);
        if (sema_find_member(builder, &name) == NULL) {
            sema_error_at(c, e->pos, "%s calls `" TEXT_MODULE "." TEXT_BUILDER
                          ".%s`, which this `" TEXT_MODULE "` lacks",
                          sema_format_name(e), names[i]);
            return false;
        }
    }
    return true;
}

static struct type *check_format(struct checker *c, struct expr *e)
{
    static const struct name module = {TEXT_MODULE, sizeof TEXT_MODULE - 1};
    static const struct name class_name = {TEXT_BUILDER,
                                           sizeof TEXT_BUILDER - 1};
    const struct interface *lib = sema_find_library(c, &module);
    struct symbol *builder =
        lib != NULL ? sema_library_item(c, lib, &class_name) : NULL;
    struct scope scope;
    struct symbol *home;
    struct expr *new_callee;
    bool ok;
    size_t i;

    if (builder == NULL || builder->kind != SYMBOL_STRUCT ||
        builder->type == NULL || builder->type->kind != TYPE_CLASS) {
        sema_error_at(c, e->pos, "%s builds its text with `" TEXT_MODULE "."
                      TEXT_BUILDER "`, so the module imports `" TEXT_MODULE "`",
                      sema_format_name(e));
        return sema_builtin(c, TYPE_ERROR);
    }
    if (!format_api(c, e, builder->type)) {
        return sema_builtin(c, TYPE_ERROR);
    }
    sema_enter_scope(c, &scope);
    home = sema_declare(c, SYMBOL_MODULE, &hidden_module, e->pos,
                        "`%.*s` is already declared");
    home->home = lib;
    e->as.format.builder = sema_declare(c, SYMBOL_LOCAL, &hidden_builder,
                                        e->pos,
                                        "`%.*s` is already declared");
    e->as.format.builder->type = builder->type;
    new_callee = format_field(
        c, format_field(c, format_word(c, e->pos, &hidden_module),
                        TEXT_BUILDER),
        TEXT_NEW);
    e->as.format.start = format_call(c, new_callee, NULL, 0);
    ok = !sema_is_error(sema_check_expr(c, e->as.format.start, NULL));
    for (i = 0; i < e->as.format.count; i++) {
        struct format_part *part = &e->as.format.parts[i];
        if (part->text.length > 0) {
            struct expr *text = sema_new_node(c, EXPR_STRING, part->pos);
            text->as.text = part->text;
            text->spelling = part->text;
            part->text_call = builder_call(c, part->pos, TEXT_APPEND, &text, 1);
            ok = !sema_is_error(sema_check_expr(c, part->text_call,
                                                NULL)) && ok;
        }
        if (part->value != NULL) {
            ok = check_format_value(c, e, part) && ok;
        }
    }
    e->as.format.take = builder_call(c, e->pos, TEXT_TAKE, NULL, 0);
    ok = !sema_is_error(sema_check_expr(c, e->as.format.take, NULL)) && ok;
    sema_leave_scope(c, &scope);
    return ok ? sema_builtin(c, TYPE_STR) : sema_builtin(c, TYPE_ERROR);
}

/* Patterns */

/* The position in the file of byte offset of the pattern of the literal
   e. The pattern holds the bytes between the quotes, with a CR LF read
   as one LF, and the column counts bytes, as the lexer does. */
static struct pos pattern_position(const struct expr *e, size_t offset)
{
    const char *s = e->spelling.bytes;
    size_t n = e->spelling.length;
    struct pos p = e->pos;
    size_t i = 2;
    size_t k;

    p.column += 2;
    while (i < n && s[i] == '#') {
        i++;
        p.column++;
    }
    i++;
    p.column++;
    for (k = 0; k < offset && i < n; k++, i++) {
        if (s[i] == '\r' && i + 1 < n && s[i + 1] == '\n') {
            i++;
        }
        if (s[i] == '\n') {
            p.line++;
            p.column = 1;
        } else {
            p.column++;
        }
    }
    return p;
}

/* The bytes of span of the pattern, cut to fit a message. */
static void pattern_piece(char *out, size_t size, const struct expr *e,
                          struct pattern_span span)
{
    size_t length = span.end - span.start;

    if (length > 24) {
        sema_format_to(out, size, "%.*s...", 21,
                       e->as.text.bytes + span.start);
    } else {
        sema_format_to(out, size, "%.*s", (int)length,
                       e->as.text.bytes + span.start);
    }
}

/* DESIGN: a pattern literal is checked where it stands. PCRE2, linked
   into antic, compiles it with the options the program compiles it with
   at start, so a malformed pattern is an error at the byte PCRE2 names.
   The safety check `exponential-pattern` then reads its repeats. The
   module imports `anti.regex`, whose classes are the failures of a
   pattern and which names what the program links, as an `f"..."` asks
   for `anti.text`. A literal takes its mode from where it stands, as an
   anonymous function takes its types: where a `ByteRegex` is expected it
   is a byte pattern, and everywhere else a pattern of text. */
static struct type *check_pattern(struct checker *c, struct expr *e,
                                  struct type *expected)
{
    static const struct name module = {REGEX_MODULE,
                                       sizeof REGEX_MODULE - 1};
    bool bytes = expected != NULL && types_is_byte_regex(expected);
    struct pattern_span inner;
    struct pattern_span outer;
    char message[ANTI_PATTERN_MESSAGE];
    size_t offset = 0;

    if (sema_find_library(c, &module) == NULL) {
        sema_error_at(c, e->pos, "a pattern literal is compiled by `"
                      REGEX_MODULE "`, so the module imports `" REGEX_MODULE
                      "`");
        return sema_builtin(c, TYPE_ERROR);
    }
    if (!pattern_compiles(e->as.text.bytes, e->as.text.length, bytes,
                          &offset, message, sizeof message)) {
        sema_error_at(c, pattern_position(e, offset), "malformed pattern: %s",
                      message);
        return sema_builtin(c, TYPE_ERROR);
    }
    if (pattern_exponential(e->as.text.bytes, e->as.text.length, &inner,
                            &outer)) {
        char in[32];
        char around[32];
        pattern_piece(in, sizeof in, e, inner);
        pattern_piece(around, sizeof around, e, outer);
        sema_check_at(c, NAME_EXPONENTIAL_PATTERN,
                      pattern_position(e, inner.start),
                      "the repeat `%s` inside `%s` can take exponential time, "
                      "write a possessive quantifier, `a++`, or an atomic "
                      "group, `(?>...)`", in, around);
    }
    return bytes ? types_byte_regex(c->types) : types_regex(c->types);
}

/* DESIGN: `x in lo..hi` is `x >= lo && x < hi`. The checker binds x to
   a local that no scope holds and writes the two comparisons over it.
   The value is then computed once, and the high bound only when the
   value reaches the low one. A type with an `lt` operator takes it, as
   the two comparisons would. The first of the three operands that is no
   literal names the type, and the literals take it. */
static struct type *check_in(struct checker *c, struct expr *e)
{
    struct expr *operands[3];
    struct type *types[3];
    struct expr *sides[2];
    struct symbol *bound;
    struct symbol *lt;
    struct expr *test;
    size_t first = 0;
    size_t i;
    bool ok = true;

    operands[0] = e->as.in.value;
    operands[1] = e->as.in.low;
    operands[2] = e->as.in.high;
    while (first < 3 && is_untyped(operands[first])) {
        first++;
    }
    first = first == 3 ? 0 : first;
    types[first] = sema_check_expr(c, operands[first], NULL);
    if (!sema_is_error(types[first]) &&
        sema_refuses_half(c, e->pos, types[first])) {
        types[first] = sema_builtin(c, TYPE_ERROR);
    }
    for (i = 0; i < 3; i++) {
        if (i != first) {
            types[i] = sema_check_expr(c, operands[i],
                                       sema_is_error(types[first])
                                           ? NULL
                                           : types[first]);
            ok = !sema_is_error(types[first]) &&
                 sema_require(c, operands[i], types[i], types[first]) && ok;
        }
    }
    if (sema_is_error(types[first]) || !ok) {
        return sema_builtin(c, TYPE_ERROR);
    }
    lt = operator_symbol(c, types[first], "lt");
    if (lt == NULL && !type_is_numeric(types[first]) &&
        types[first]->kind != TYPE_CHAR) {
        sema_error_at(c, e->pos, "`in` needs numeric or `char` operands, found "
                      "`%s`", sema_tn(types[first]));
        return sema_builtin(c, TYPE_ERROR);
    }
    bound = arena_alloc(c->arena, sizeof *bound);
    bound->kind = SYMBOL_LOCAL;
    bound->name = sema_hidden_value;
    bound->pos = e->as.in.value->pos;
    bound->type = types[first];
    e->as.in.bound = bound;
    for (i = 0; i < 2; i++) {
        struct expr *read = sema_new_node(c, EXPR_NAME, e->pos);
        struct expr *side = sema_new_node(c, EXPR_BINARY, e->pos);
        read->symbol = bound;
        read->type = bound->type;
        read->as.name = sema_hidden_value;
        side->as.binary.op = i == 0 ? TOKEN_GE : TOKEN_LT;
        side->as.binary.left = read;
        side->as.binary.right = i == 0 ? e->as.in.low : e->as.in.high;
        side->type = sema_builtin(c, TYPE_BOOL);
        if (lt != NULL &&
            !sema_require(c, side, check_operator(c, side, types[i + 1], lt),
                          sema_builtin(c, TYPE_BOOL))) {
            return sema_builtin(c, TYPE_ERROR);
        }
        sides[i] = side;
    }
    test = sema_new_node(c, EXPR_BINARY, e->pos);
    test->as.binary.op = TOKEN_AND_AND;
    test->as.binary.left = sides[0];
    test->as.binary.right = sides[1];
    test->type = sema_builtin(c, TYPE_BOOL);
    e->as.in.test = test;
    return sema_builtin(c, TYPE_BOOL);
}

/* DESIGN: `p ?? q` gives p as `*T` when it is not `none` and q
   otherwise. q converts to the element of p as any pointer does, and
   the result is `?*T` when q may be `none` as well. A function value
   follows the pointer rule. */
static struct type *check_coalesce(struct checker *c, struct expr *e,
                                   struct type *expected)
{
    struct expr *right = e->as.binary.right;
    struct type *hint = NULL;
    struct type *left;
    struct type *got;

    if (expected != NULL && !sema_is_error(expected) &&
        (expected->kind != TYPE_FN || !expected->bound)) {
        hint = type_is_nullable(expected) ? expected
                                          : types_with_none(c->types, expected);
    }
    left = sema_check_expr(c, e->as.binary.left, hint);
    if (!sema_is_error(left) && !type_is_nullable(left)) {
        sema_error_at(c, e->pos,
                      "`??` follows a value of type `?*T` or `?T`, found `%s`",
                      sema_tn(left));
        left = sema_builtin(c, TYPE_ERROR);
    }
    if (sema_is_error(left)) {
        sema_check_expr(c, right, NULL);
        return left;
    }
    /* Two matches of two literals give a plain match. */
    if (types_is_maybe_match(left)) {
        left = types_match_plain(c->types, left);
    }
    /* A right side that may be `none` gives a result that may be, and
       any other gives the value. */
    got = sema_check_expr(c, right, left);
    if (!type_is_nullable(got) && got->kind != TYPE_NONE &&
        !sema_is_error(got)) {
        struct type *bare = types_without_none(c->types, left);
        bool held;
        c->quiet++;
        held = sema_require(c, right, got, bare);
        c->quiet--;
        if (held) {
            return bare;
        }
    }
    if (!sema_require(c, right, got, left)) {
        return sema_builtin(c, TYPE_ERROR);
    }
    return left;
}

/* DESIGN: `p?.x` and `p?.f(args)` give `none` when p is `none` and the
   field or the call otherwise. The checker binds p to a local of type
   `*T` in a scope of its own and checks the field or the call on it, so
   every rule of `.` applies unchanged. A `?T` of a value binds the
   address of the value it holds, so a call changes that value and no
   copy. The result is the `?*U` of a pointer field or result, and
   anything else is refused, as "Small things" of the additions says. A
   function value follows the pointer rule. A chain `p?.a?.b` checks each
   `?.` on the `?*U` the one before gave. */
static struct type *check_optional(struct checker *c, struct expr *e)
{
    struct expr *access = sema_new_node(c, e->kind, e->pos);
    struct expr *field = e->kind == EXPR_CALL ? e->as.call.callee : e;
    struct expr *base = field->as.field.base;
    struct name name = field->as.field.name;
    const char *call = e->kind != EXPR_CALL        ? ""
                       : e->as.call.arg_count == 0 ? "()"
                                                   : "(...)";
    struct type *t = sema_check_expr(c, base, NULL);
    struct expr *read;
    struct symbol *bound;
    struct type *result;
    struct scope scope;

    if (sema_is_error(t)) {
        return t;
    }
    if ((t->kind != TYPE_POINTER && t->kind != TYPE_OPTIONAL) ||
        !t->nullable) {
        sema_error_at(c, base->pos, "`?.` follows a value of type `?*T` or "
                      "`?T`, found `%s`", sema_tn(t));
        return sema_builtin(c, TYPE_ERROR);
    }
    *access = *e;
    if (e->kind == EXPR_CALL) {
        field = sema_new_node(c, EXPR_FIELD, field->pos);
        *field = *e->as.call.callee;
        access->as.call.callee = field;
        access->as.call.optional = true;
    } else {
        field = access;
    }
    read = sema_new_node(c, EXPR_NAME, base->pos);
    read->as.name = hidden_object;
    field->as.field.base = read;
    field->as.field.optional = false;
    sema_enter_scope(c, &scope);
    bound = sema_declare(c, SYMBOL_LOCAL, &hidden_object, base->pos,
                         "`%.*s` is already declared");
    if (bound == NULL) {
        sema_leave_scope(c, &scope);
        return sema_builtin(c, TYPE_ERROR);
    }
    bound->type = t->kind == TYPE_OPTIONAL
                      ? types_pointer(c->types, t->element)
                      : types_without_none(c->types, t);
    result = sema_check_expr(c, access, NULL);
    sema_leave_scope(c, &scope);
    if (sema_is_error(result)) {
        return result;
    }
    /* The message spells the field or the call as `.` reads it. */
    if (result->kind != TYPE_POINTER &&
        (result->kind != TYPE_FN || result->bound)) {
        struct text spelling = {0};
        if (sema_spell(&spelling, base)) {
            text_append(&spelling, ".");
        }
        text_appendf(&spelling, "%.*s%s", (int)name.length, name.text, call);
        sema_error_at(c, e->pos, "`?.` on `%s`, which is not a pointer",
                      text_cstr(&spelling));
        text_free(&spelling);
        return sema_builtin(c, TYPE_ERROR);
    }
    memset(&e->as, 0, sizeof e->as);
    e->kind = EXPR_OPTIONAL;
    e->as.optional.base = base;
    e->as.optional.bound = bound;
    e->as.optional.access = access;
    return type_is_nullable(result) ? result
                                    : types_with_none(c->types, result);
}

static struct type *check_expr_inner(struct checker *c, struct expr *e,
                                     struct type *expected)
{
    struct type *t;
    struct symbol *sym;
    size_t i;

    switch (e->kind) {
    case EXPR_INT:
        return integer_literal(c, e, e, false, expected);
    case EXPR_FLOAT:
        return float_literal(c, e, e, false, expected);
    case EXPR_CHAR:
        return sema_builtin(c, TYPE_CHAR);
    case EXPR_STRING:
        return sema_builtin(c, TYPE_STR);
    case EXPR_BYTES:
        return types_slice(c->types, sema_builtin(c, TYPE_U8));
    case EXPR_BOOL:
        return sema_builtin(c, TYPE_BOOL);
    case EXPR_NONE:
        /* `none` has type `?*T` for every T, and the context names the
           T. A `*T` there is the one type that cannot hold it. */
        if (expected != NULL &&
            (expected->kind == TYPE_POINTER || expected->kind == TYPE_FN) &&
            !expected->nullable) {
            sema_error_at(c, e->pos, "`%s` cannot hold `none`",
                          sema_tn(expected));
            return sema_builtin(c, TYPE_ERROR);
        }
        if (expected != NULL && (expected->kind == TYPE_POINTER ||
                                 expected->kind == TYPE_FN)) {
            return expected;
        }
        /* A `?T` holds `none`, and a value of any other type does not. */
        if (expected != NULL && expected->kind == TYPE_OPTIONAL) {
            return expected;
        }
        if (expected != NULL && !sema_is_error(expected) &&
            expected->kind != TYPE_PARAM) {
            sema_error_at(c, e->pos, "`%s` cannot hold `none`",
                          sema_tn(expected));
            return sema_builtin(c, TYPE_ERROR);
        }
        if (expected == NULL || !sema_is_error(expected)) {
            sema_error_at(c, e->pos,
                          "`none` needs a type that may be `none` from its "
                          "context");
        }
        return sema_builtin(c, TYPE_ERROR);
    case EXPR_NAME:
        sym = sema_lookup(c, &e->as.name);
        e->symbol = sym;
        if (sym == NULL) {
            sema_error_at(c, e->pos, "unknown name `%.*s`",
                          (int)e->as.name.length,
                          e->as.name.text);
            return sema_builtin(c, TYPE_ERROR);
        }
        if (sym->kind == SYMBOL_STRUCT) {
            sema_error_at(c, e->pos, "`%.*s` is a type, not a value",
                          (int)e->as.name.length, e->as.name.text);
            return sema_builtin(c, TYPE_ERROR);
        }
        if (sym->kind == SYMBOL_MODULE) {
            sema_error_at(c, e->pos, "`%.*s` is a module, not a value",
                          (int)e->as.name.length, e->as.name.text);
            return sema_builtin(c, TYPE_ERROR);
        }
        if (sym->kind == SYMBOL_CONSTRAINT) {
            sema_error_at(c, e->pos, "`%.*s` is a constraint, not a value",
                          (int)e->as.name.length, e->as.name.text);
            return sema_builtin(c, TYPE_ERROR);
        }
        /* DESIGN: a generic function names a copy where it is called,
           since the arguments of the call give its type arguments. A
           name of one anywhere else names no function. */
        if (sym->kind == SYMBOL_FN && sym->item != NULL &&
            sym->item->type_param_count > 0 && c->callee != e) {
            sema_error_at(c, e->pos, "`%.*s` is generic, and names a function "
                          "where it is called", (int)e->as.name.length,
                          e->as.name.text);
            return sema_builtin(c, TYPE_ERROR);
        }
        if (e->type_arg_count > 0 && c->callee != e) {
            sema_refuse_type_args(c, e, &e->as.name);
            return sema_builtin(c, TYPE_ERROR);
        }
        /* A local of another frame is a variable an anonymous function
           captures. */
        if ((sym->kind == SYMBOL_LOCAL || sym->kind == SYMBOL_PARAM) &&
            sym->frame != NULL && sym->frame != c->function) {
            sema_capture(c, sym);
        }
        if (sym->kind == SYMBOL_EXTERN_FN && sym->variadic) {
            sema_error_at(c, e->pos,
                          "a variadic function has no function pointer "
                          "type");
            return sema_builtin(c, TYPE_ERROR);
        }
        if (sym->caught && sym->moved_into != NULL) {
            sema_error_at(c, e->pos, "`%.*s` has moved into `%.*s` and is not "
                          "named after it", (int)e->as.name.length,
                          e->as.name.text, (int)sym->moved_into->name.length,
                          sym->moved_into->name.text);
            return sema_builtin(c, TYPE_ERROR);
        }
        if (sym->moved) {
            refuse_moved(c, e, sym);
            return sema_builtin(c, TYPE_ERROR);
        }
        if ((sym->kind == SYMBOL_LOCAL || sym->kind == SYMBOL_PARAM) &&
            c->deferring > 0) {
            sym->deferred = true;
        }
        /* An atomic local is read and written by its own calls alone,
           as an atomic field is. */
        if (sym->atomic && !c->atomic_place) {
            sema_error_at(c, e->pos, "`%.*s` is atomic, so it is read with "
                          "`load()` and written with `store(v)`",
                          (int)e->as.name.length, e->as.name.text);
            return sema_builtin(c, TYPE_ERROR);
        }
        if (sym->snapshot_moved) {
            sema_error_at(c, e->pos, "`%.*s` has moved into an owner on line "
                          "%u and is not named after it",
                          (int)e->as.name.length, e->as.name.text,
                          (unsigned)sym->snapshot_move.line);
            return sema_builtin(c, TYPE_ERROR);
        }
        /* A Flags value that is not the base of a field is read whole. */
        if (types_is_flags(sym->type) && c->field_base != e) {
            sym->flags_read = FLAGS_READ_ALL;
        }
        if (sym->kind == SYMBOL_CONST && sym->type == NULL) {
            struct const_value v;
            if (!sema_eval_const(c, e, &v)) {
                return sema_builtin(c, TYPE_ERROR);
            }
        }
        /* A closure may run after the variable has changed, so a
           captured variable keeps its declared type. */
        if (sym->type != NULL && type_is_nullable(sym->type) &&
            sym->frame == c->function) {
            struct type *proved = sema_narrowed_type(c, sym);
            if (proved != NULL) {
                return proved;
            }
        }
        return sym->type != NULL ? sym->type : sema_builtin(c, TYPE_ERROR);
    case EXPR_UNARY:
        return check_unary(c, e, expected);
    case EXPR_FN:
        return sema_check_anonymous(c, e, expected);
    case EXPR_BINARY:
        return sema_check_binary(c, e, expected);
    /* A pointer converted from a `lent` one is lent as well. */
    case EXPR_CAST:
        t = check_cast(c, e);
        return !e->as.cast.test && e->as.cast.operand->type != NULL &&
                       type_is_lent(e->as.cast.operand->type)
                   ? types_lent(c->types, t)
                   : t;
    case EXPR_CALL:
        if (e->as.call.callee->kind == EXPR_FIELD &&
            e->as.call.callee->as.field.optional) {
            return check_optional(c, e);
        }
        if (e->as.call.callee->kind == EXPR_FIELD && check_collect(c, e)) {
            return e->type != NULL ? e->type : sema_builtin(c, TYPE_ERROR);
        }
        return sema_check_call(c, e, expected);
    case EXPR_COLLECT:
        return e->type != NULL ? e->type : sema_builtin(c, TYPE_ERROR);
    case EXPR_PARALLEL:
        return sema_check_parallel(c, e);
    case EXPR_DISPATCH:
        return sema_check_dispatch(c, e);
    case EXPR_JOIN:
        return sema_check_join(c, e);
    case EXPR_HERE:
        t = sema_location_type(c, e->pos);
        return t != NULL ? t : sema_builtin(c, TYPE_ERROR);
    case EXPR_DESCRIPTOR:
        /* The checker writes the node with its class already set. */
        return types_pointer(c->types, sema_builtin(c, TYPE_U8));
    case EXPR_FORMAT:
        return check_format(c, e);
    case EXPR_PATTERN:
        return check_pattern(c, e, expected);
    case EXPR_IN:
        return check_in(c, e);
    case EXPR_INDEX:
        t = sema_check_expr(c, e->as.index.base, NULL);
        /* DESIGN: `e[i]` on a type with the `index` hook is the call
           `e.index(i)`, and the index takes the type the hook names.
           `e[x, y]` is `e.index(x, y)`. */
        if (!sema_is_error(t) && sema_hook(c, t, LANG_HOOK_INDEX) != NULL) {
            struct expr *index = e->as.index.index;
            *e = e->as.index.several
                     ? *sema_hook_call(c, e->as.index.base, LANG_HOOK_INDEX,
                                       index->as.tuple.elements,
                                       index->as.tuple.count)
                     : *sema_hook_call(c, e->as.index.base, LANG_HOOK_INDEX,
                                       &index, 1);
            return sema_check_expr(c, e, expected);
        }
        if (!sema_is_error(t) && t->kind == TYPE_PARAM) {
            return sema_param_index(c, e, t, false);
        }
        if (!sema_is_error(t) && e->as.index.several) {
            sema_error_at(c, e->pos, "`%s` has no `operator fn index`, which "
                          "`e[x, y]` calls", sema_tn(t));
            sema_check_expr(c, e->as.index.index, NULL);
            return sema_builtin(c, TYPE_ERROR);
        }
        if (!sema_require(c, e->as.index.index,
                          sema_check_expr(c, e->as.index.index,
                                          sema_builtin(c, TYPE_I64)),
                          sema_builtin(c, TYPE_I64)) || sema_is_error(t)) {
            return sema_builtin(c, TYPE_ERROR);
        }
        switch (t->kind) {
        case TYPE_ARRAY:
        case TYPE_SLICE:
            return t->element;
        case TYPE_POINTER:
            return sema_usable_pointer(c, e->as.index.base, t)->element;
        case TYPE_STR:
            return sema_builtin(c, TYPE_U8);
        default:
            sema_error_at(c, e->pos, "cannot index `%s`", sema_tn(t));
            return sema_builtin(c, TYPE_ERROR);
        }
    case EXPR_SLICE: {
        struct type *index = sema_builtin(c, TYPE_I64);
        bool ok;
        t = sema_check_expr(c, e->as.slice.base, NULL);
        ok = sema_require(c, e->as.slice.low,
                          sema_check_expr(c, e->as.slice.low, index), index);
        ok = sema_require(c, e->as.slice.high,
                          sema_check_expr(c, e->as.slice.high, index),
                          index) && ok;
        if (!ok || sema_is_error(t)) {
            return sema_builtin(c, TYPE_ERROR);
        }
        if (t->kind == TYPE_ARRAY) {
            if (!sema_is_place(e->as.slice.base)) {
                sema_error_at(c, e->pos, "slicing an array needs a place");
                return sema_builtin(c, TYPE_ERROR);
            }
            sema_mark_address_taken(c, e->as.slice.base);
            return through_lent(e->as.slice.base)
                       ? types_lent(c->types, types_slice(c->types, t->element))
                       : types_slice(c->types, t->element);
        }
        if (t->kind == TYPE_SLICE) {
            return t;
        }
        if (t->kind == TYPE_STR) {
            return types_slice(c->types, sema_builtin(c, TYPE_U8));
        }
        sema_error_at(c, e->pos, "cannot slice `%s`", sema_tn(t));
        return sema_builtin(c, TYPE_ERROR);
    }
    case EXPR_FIELD:
        if (e->as.field.optional) {
            return check_optional(c, e);
        }
        return sema_check_field(c, e);
    case EXPR_OPTIONAL:
        return e->type;
    case EXPR_STRUCT_LIT: {
        struct name *name = &e->as.struct_lit.name;
        const struct symbol *named;
        /* A literal the checker wrote from `Shape.Empty` is complete. */
        if (e->as.struct_lit.variant_case != 0 && e->type != NULL) {
            return e->type;
        }
        /* `geo.Shape.Circle { }` and `Shape.Circle { }` name a case. */
        if (e->as.struct_lit.member.length > 0) {
            t = sema_imported_struct(c, &e->as.struct_lit.module, name, e->pos);
            if (sema_is_error(t)) {
                return t;
            }
            if (t->kind != TYPE_VARIANT) {
                sema_error_at(c, e->pos, "`%s` is not a variant", sema_tn(t));
                return sema_builtin(c, TYPE_ERROR);
            }
            return sema_variant_literal(c, e, t, &e->as.struct_lit.member);
        }
        if (e->as.struct_lit.module.length > 0 &&
            (named = sema_lookup(c, &e->as.struct_lit.module)) != NULL &&
            named->kind == SYMBOL_STRUCT && named->type != NULL &&
            named->type->kind == TYPE_VARIANT) {
            t = sema_generic_named(c, e, named->type,
                                   &e->as.struct_lit.module, expected);
            if (sema_is_error(t)) {
                return t;
            }
            return sema_variant_literal(c, e, t, name);
        }
        if (e->as.struct_lit.module.length > 0) {
            t = sema_imported_struct(c, &e->as.struct_lit.module, name, e->pos);
            if (sema_is_error(t)) {
                return t;
            }
        } else {
            sym = sema_module_find(c, name);
            if (sym == NULL && sema_name_is(name, LANG_FLAGS)) {
                t = types_flags(c->types);
            } else if (sym == NULL &&
                       sema_name_is(name, LANG_FIELD_DESCRIPTOR)) {
                t = types_field_descriptor(c->types);
            } else if (sym == NULL && sema_name_is(name, LANG_REGEX)) {
                t = types_regex(c->types);
            } else if (sym == NULL && sema_name_is(name, LANG_BYTE_REGEX)) {
                t = types_byte_regex(c->types);
            } else if (sym == NULL || sym->kind != SYMBOL_STRUCT) {
                sema_error_at(c, e->pos, "unknown struct `%.*s`",
                              (int)name->length, name->text);
                return sema_builtin(c, TYPE_ERROR);
            } else {
                t = sym->item != NULL && sym->item->kind == ITEM_TYPE
                        ? sema_alias_type(c, sym)
                        : sym->type;
            }
        }
        t = sema_generic_named(c, e, t, name, expected);
        if (sema_is_error(t)) {
            return t;
        }
        if (t->kind == TYPE_VARIANT) {
            sema_error_at(c, e->pos,
                          "a literal of variant `%s` names one of its "
                          "cases", sema_tn(t));
            return sema_builtin(c, TYPE_ERROR);
        }
        if (sema_singleton_type(t) && sema_checking_class(c) != t) {
            sema_error_at(c, e->pos,
                          "`%s` is a singleton, and `%s.get()` gives "
                          "its one instance", sema_tn(t), sema_tn(t));
            return sema_builtin(c, TYPE_ERROR);
        }
        if (sema_refuse_abstract_value(c, e->pos, "a literal", t)) {
            return sema_builtin(c, TYPE_ERROR);
        }
        if (t->is_union && e->as.struct_lit.field_count != 1) {
            sema_error_at(c, e->pos,
                          "a literal of union `%s` names exactly one "
                          "field", sema_tn(t));
            return sema_builtin(c, TYPE_ERROR);
        }
        if (t->kind == TYPE_CLASS) {
            struct struct_field *flat;
            size_t count = sema_chain_fields(t, NULL);
            flat = types_alloc_array(c->arena, count + 1, sizeof *flat);
            sema_chain_fields(t, flat);
            return sema_check_field_inits(c, e, e->as.struct_lit.fields,
                                          e->as.struct_lit.field_count, flat,
                                          count, sema_tn(t), false)
                       ? t
                       : sema_builtin(c, TYPE_ERROR);
        }
        return sema_check_field_inits(c, e, e->as.struct_lit.fields,
                                      e->as.struct_lit.field_count, t->fields,
                                      t->field_count, sema_tn(t), t->is_union)
                   ? t
                   : sema_builtin(c, TYPE_ERROR);
    }
    case EXPR_SLICE_LIT: {
        struct struct_field fields[2];
        struct type *element = sema_resolve_type(c, e->as.slice_lit.element);
        if (sema_is_error(element)) {
            return element;
        }
        t = types_slice(c->types, element);
        memset(fields, 0, sizeof fields);
        fields[0].name.text = "ptr";
        fields[0].name.length = 3;
        /* DESIGN: the `ptr` of a slice is `?*T`. A slice of no
           elements holds no address, and `[]T { ptr: none, len: 0 }` is
           how a program writes one. */
        fields[0].type = types_pointer_nullable(c->types, element);
        fields[1].name.text = "len";
        fields[1].name.length = 3;
        fields[1].type = sema_builtin(c, TYPE_I64);
        return sema_check_field_inits(c, e, e->as.slice_lit.fields,
                                      e->as.slice_lit.field_count, fields, 2,
                                      sema_tn(t),
                                      false)
                   ? t
                   : sema_builtin(c, TYPE_ERROR);
    }
    case EXPR_ARRAY_LIT: {
        struct type *element = expected != NULL && expected->kind == TYPE_ARRAY
                                   ? expected->element
                                   : NULL;
        bool ok = true;
        for (i = 0; i < e->as.array_lit.count; i++) {
            struct expr *item = e->as.array_lit.elements[i];
            struct type *it = sema_check_expr(c, item, element);
            if (element == NULL) {
                element = it;
            } else {
                ok = sema_require(c, item, it, element) && ok;
            }
            sema_refuse_owned_copy(c, item, it);
        }
        if (!ok || sema_is_error(element)) {
            return sema_builtin(c, TYPE_ERROR);
        }
        return types_array(c->types, element, e->as.array_lit.count);
    }
    /* `(a, b)` builds a tuple of the types of its elements. A context
       that names a tuple of the same count gives each element its type.
       An integer literal in a tuple so takes the type it is written
       into. */
    case EXPR_TUPLE: {
        struct type **elements =
            types_alloc_array(c->arena, e->as.tuple.count, sizeof *elements);
        const struct type *want =
            expected != NULL && expected->kind == TYPE_TUPLE &&
                    expected->param_count == e->as.tuple.count
                ? expected
                : NULL;
        bool ok = true;
        for (i = 0; i < e->as.tuple.count; i++) {
            struct expr *item = e->as.tuple.elements[i];
            struct type *element =
                want != NULL ? want->params[i] : NULL;
            elements[i] = sema_check_expr(c, item, element);
            if (element != NULL) {
                ok = sema_require(c, item, elements[i], element) && ok;
                elements[i] = element;
            }
            sema_refuse_owned_copy(c, item, elements[i]);
            ok = ok && !sema_is_error(elements[i]);
        }
        if (!ok) {
            return sema_builtin(c, TYPE_ERROR);
        }
        return types_tuple(c->types, elements, e->as.tuple.count);
    }
    case EXPR_ARRAY_REPEAT: {
        struct type *element = expected != NULL && expected->kind == TYPE_ARRAY
                                   ? expected->element
                                   : NULL;
        t = sema_check_expr(c, e->as.array_repeat.value, element);
        /* The repeat form writes one value into every element, which
           gives what it owns one owner per element. */
        if (!sema_is_error(t) && sema_type_owns(t)) {
            sema_error_at(c, e->as.array_repeat.value->pos, "`%s` has `own` "
                          "fields, and the repeat form copies one value into "
                          "every element", sema_tn(t));
        }
        return sema_array_of(c, e->as.array_repeat.count, t);
    }
    case EXPR_ALLOC:
        /* DESIGN: `alloc T { ... }` allocates one object and writes the
           literal into it, so its type is the type of the literal. */
        if (e->as.alloc.value != NULL) {
            /* `alloc T(args)` puts the object on the heap and runs its
               `construct` with the arguments. */
            if (e->as.alloc.value->kind == EXPR_CALL) {
                e->as.alloc.value->as.call.on_heap = true;
                t = sema_check_expr(c, e->as.alloc.value, expected);
                if (sema_is_error(t) ||
                    e->as.alloc.value->as.call.builds == NULL) {
                    sema_error_at(c, e->as.alloc.value->pos,
                                  "`alloc` of one object takes a literal or a "
                                  "class with arguments");
                    return sema_builtin(c, TYPE_ERROR);
                }
                return t;
            }
            if (e->as.alloc.value->kind != EXPR_STRUCT_LIT) {
                sema_error_at(c, e->as.alloc.value->pos,
                              "`alloc` of one object takes a literal");
                return sema_builtin(c, TYPE_ERROR);
            }
            t = sema_check_expr(c, e->as.alloc.value, NULL);
            if (sema_is_error(t)) {
                return t;
            }
            return types_pointer(c->types, t);
        }
        t = sema_resolve_type(c, e->as.alloc.type);
        if (!sema_require(c, e->as.alloc.count,
                          sema_check_expr(c, e->as.alloc.count,
                                          sema_builtin(c, TYPE_I64)),
                          sema_builtin(c, TYPE_I64)) || sema_is_error(t)) {
            return sema_builtin(c, TYPE_ERROR);
        }
        if (sema_refuse_abstract_value(c, e->pos, "`alloc`", t)) {
            return sema_builtin(c, TYPE_ERROR);
        }
        /* DESIGN: `alloc(T, n)` gives `?*T`, because it is `malloc` and
           `malloc` gives none when the memory is not there. `alloc T { }`
           and `alloc T(args)` give `*T`: out of memory is fatal there. */
        return types_pointer_nullable(c->types, t);
    case EXPR_FREE:
        t = sema_check_expr(c, e->as.free_pointer, NULL);
        if (!sema_is_error(t) && t->kind != TYPE_POINTER) {
            sema_error_at(c, e->as.free_pointer->pos,
                          "`free` needs a pointer, found `%s`", sema_tn(t));
            return sema_builtin(c, TYPE_ERROR);
        }
        return sema_is_error(t) ? t : sema_builtin(c, TYPE_VOID);
    /* DESIGN: `dup` copies an object of its concrete class and gives a
       pointer of the static type back. `delete` runs the destruct chain and
       frees the object. `destroy` runs the chain without the free, for
       an object that is not on a heap of its own. All three need a class
       pointer, because all three read the table of the object. */
    /* A node the checker makes, already typed. */
    case EXPR_ATOMIC:
        return e->type;
    case EXPR_SYNC_OP:
        return sema_check_sync_op(c, e);
    case EXPR_SIMD:
        return e->type;
    case EXPR_OBJECT: {
        const char *what = e->as.object.op == TOKEN_DUP      ? "dup"
                           : e->as.object.op == TOKEN_DELETE ? "delete"
                                                             : "destroy";
        t = sema_check_expr(c, e->as.object.operand, NULL);
        if (sema_is_error(t)) {
            return t;
        }
        if (e->as.object.from != NULL &&
            (e->as.object.op == TOKEN_DUP || types_is_chan(t))) {
            sema_error_at(c, e->as.object.from->pos, "`%s` takes 1 argument, "
                          "found 2", what);
            return sema_builtin(c, TYPE_ERROR);
        }
        if (e->as.object.from != NULL && !sema_check_object_from(c, e, what)) {
            return sema_builtin(c, TYPE_ERROR);
        }
        /* DESIGN: `delete(c)` ends a channel and frees what the runtime
           holds for it, as `delete` frees an object on the heap. */
        if (e->as.object.op == TOKEN_DELETE && types_is_chan(t)) {
            struct expr *operand = e->as.object.operand;
            e->kind = EXPR_SYNC_OP;
            memset(&e->as, 0, sizeof e->as);
            e->as.sync_op.op = SYNC_CHAN_DELETE;
            e->as.sync_op.target = operand;
            return sema_builtin(c, TYPE_VOID);
        }
        /* DESIGN: `dup` of an `own fn` copies its snapshot, and the
           copy goes to an owner, which the conversion checks. */
        if (e->as.object.op == TOKEN_DUP && t->kind == TYPE_FN && t->owned) {
            return t;
        }
        if (e->as.object.op == TOKEN_DUP && t->kind == TYPE_FN &&
            !t->bound) {
            sema_error_at(c, e->as.object.operand->pos,
                          "`dup` copies an `own fn`, and this is `%s`",
                          sema_tn(t));
            return sema_builtin(c, TYPE_ERROR);
        }
        /* All three read the table of the object, so all three need a
           pointer the program has checked. `dup(p)` then gives the type
           of `p`, which is the `*T` this leaves behind. */
        t = sema_usable_pointer(c, e->as.object.operand, t);
        if (e->as.object.op == TOKEN_DELETE && t->kind == TYPE_POINTER &&
            sema_singleton_type(t->element)) {
            sema_error_at(c, e->pos, "`%s` is a singleton and outlives the "
                          "program", sema_tn(t->element));
            return sema_builtin(c, TYPE_ERROR);
        }
        if (t->kind != TYPE_POINTER || t->element->kind != TYPE_CLASS) {
            sema_error_at(c, e->as.object.operand->pos,
                          "`%s` needs a class pointer, found `%s`", what,
                          sema_tn(t));
            return sema_builtin(c, TYPE_ERROR);
        }
        /* A lent object stays with its owner, and `dup` makes one that
           belongs to no one yet. */
        if (e->as.object.op != TOKEN_DUP && type_is_lent(t)) {
            sema_error_at(c, e->as.object.operand->pos, "the object is lent "
                          "for the call and stays with its owner, so `%s` "
                          "does not take it", what);
            return sema_builtin(c, TYPE_ERROR);
        }
        return e->as.object.op == TOKEN_DUP ? types_unlent(c->types, t)
                                            : sema_builtin(c, TYPE_VOID);
    }
    case EXPR_SIZE_OF:
        t = sema_resolve_type(c, e->as.size_of);
        return sema_is_error(t) ? t : sema_builtin(c, TYPE_I64);
    }
    return sema_builtin(c, TYPE_ERROR);
}

/* DESIGN: a read of an f16 gives an f32. The checker writes it as an
   `as f32` of the f16, marked promoted, so that lowering converts it
   where it converts every other `as`. An `as f16` stays f16, because it
   is the value that a write takes. */
/* DESIGN: a literal where a `?T` is expected is a T, which then passes
   as the `?T` that holds it. It takes its type from the T, so `0` where a
   `?u8` stands is a `u8`. Every other expression keeps the `?T`, which
   `none`, `??` and a handler that yields read. */
static struct type *value_expected(const struct expr *e,
                                   struct type *expected)
{
    if (expected == NULL || expected->kind != TYPE_OPTIONAL) {
        return expected;
    }
    switch (e->kind) {
    case EXPR_INT:
    case EXPR_FLOAT:
    case EXPR_CHAR:
    case EXPR_STRING:
    case EXPR_BYTES:
    case EXPR_BOOL:
    case EXPR_UNARY:
    case EXPR_STRUCT_LIT:
    case EXPR_TUPLE:
    case EXPR_SLICE_LIT:
    case EXPR_ARRAY_LIT:
    case EXPR_ARRAY_REPEAT:
    case EXPR_FORMAT:
        return value_expected(e, expected->element);
    default:
        return expected;
    }
}

struct type *sema_check_expr(struct checker *c, struct expr *e,
                             struct type *expected)
{
    struct type *t;
    struct expr *read;
    struct expr *cast;

    if (e->prechecked) {
        return e->type;
    }
    t = check_expr_inner(c, e, value_expected(e, expected));
    e->type = t;
    if (t->kind != TYPE_F16 || e->kind == EXPR_CAST) {
        return t;
    }
    read = sema_new_node(c, e->kind, e->pos);
    *read = *e;
    cast = format_cast(c, read, TOKEN_F32);
    cast->as.cast.promoted = true;
    cast->type = sema_builtin(c, TYPE_F32);
    cast->as.cast.type->type = cast->type;
    *e = *cast;
    return e->type;
}

/* e where it names storage: the target of an assignment and the operand
   of `&`. A read of an f16 there stays an f16. */
struct type *sema_check_storage(struct checker *c, struct expr *e)
{
    struct type *t;

    if (e->prechecked) {
        return e->type;
    }
    t = check_expr_inner(c, e, NULL);
    e->type = t;
    return t;
}

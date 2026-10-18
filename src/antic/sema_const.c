/* The evaluation of constant expressions, the order in which the
   constants of a module are evaluated, and whether a type holds a class,
   which no constant may. */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../rt/f16.h"
#include "arith.h"
#include "sema_checker.h"

/* Constants */

/* DESIGN: a constant of a target-sized type is computed at 64 bits and
   must fit the narrower width, so it has one value on every target. */
static void wrap(struct const_value *v)
{
    int bits = types_is_target_sized(v->type) ? 64 : types_bits(v->type);

    if (bits < 64) {
        uint64_t mask = ((uint64_t)1 << bits) - 1;
        v->as.integer &= mask;
        if (types_is_signed(v->type) && (v->as.integer >> (bits - 1)) != 0) {
            v->as.integer |= ~mask;
        }
    }
}

/* Whether a constant has one value on every target: one of a
   target-sized type fits its narrowest width. */
static bool one_value(const struct const_value *v)
{
    int bits = types_bits(v->type);

    if (v->kind != CONST_INT || !types_is_target_sized(v->type)) {
        return true;
    }
    return types_is_signed(v->type)
               ? sema_signed_bits(v->as.integer) >=
                         -((int64_t)1 << (bits - 1)) &&
                     sema_signed_bits(v->as.integer) <
                         ((int64_t)1 << (bits - 1))
               : v->as.integer < ((uint64_t)1 << bits);
}

/* Report a constant of a target-sized type whose value differs between
   targets. */
static bool fits_every_target(struct checker *c, const struct expr *e,
                              const struct const_value *v)
{
    bool fits = one_value(v);

    if (!fits) {
        sema_error_at(c, e->pos, "the value does not fit `%s` on every target",
                      sema_tn(v->type));
    }
    return fits;
}

static bool fail_const(struct checker *c, const struct expr *e,
                       const char *what)
{
    sema_error_at(c, e->pos, "%s is not a constant expression", what);
    return false;
}

/* Two texts in the order of their bytes, and a text before every longer
   one it begins, as `==` and `<` of `str` compare them at run time. */
static int compare_text(const struct token_text *a, const struct token_text *b)
{
    size_t n = a->length < b->length ? a->length : b->length;
    int cmp = n > 0 ? memcmp(a->bytes, b->bytes, n) : 0;

    if (cmp != 0) {
        return cmp < 0 ? -1 : 1;
    }
    return a->length < b->length ? -1 : a->length > b->length ? 1 : 0;
}

/* DESIGN: the value of a class literal holds each field of the class at
   its index. The base is field 0, a value of the base class from the
   same literal, since the literal names an inherited field as its own.
   A field the literal names holds its value, and every other field is
   CONST_DEFAULT. Lowering writes the tables, then each CONST_DEFAULT as
   a literal writes a field it leaves out, then runs `construct`. A table,
   a lock, an `inject` field and a default that is no constant all keep
   their meaning, and the value names no other class's defaults, which
   may not be evaluated yet. */
static bool class_value(struct checker *c, const struct expr *lit,
                        struct type *s, struct const_value *out)
{
    size_t i;
    size_t j;

    out->kind = CONST_STRUCT;
    out->type = s;
    out->as.aggregate.count = s->field_count;
    out->as.aggregate.items =
        types_alloc_array(c->arena, s->field_count,
                          sizeof *out->as.aggregate.items);
    for (j = 0; j < s->field_count; j++) {
        struct const_value *item = &out->as.aggregate.items[j];
        item->kind = CONST_DEFAULT;
        item->type = s->fields[j].type;
        /* The base field of a class whose base was refused has no
           type. The refusal is the message, and the field stays a
           default. */
        if (s->fields[j].form == FIELD_BASE) {
            if (s->fields[j].type != NULL &&
                !class_value(c, lit, s->fields[j].type, item)) {
                return false;
            }
            continue;
        }
        for (i = 0; i < lit->as.struct_lit.field_count; i++) {
            if (sema_same_name(&s->fields[j].name,
                               &lit->as.struct_lit.fields[i].name) &&
                !sema_eval_const(c, lit->as.struct_lit.fields[i].value,
                                 item)) {
                return false;
            }
        }
    }
    return true;
}

/* A constant as a symbolic node: a symbolic value itself, or a number,
   a bool or a character as a node of its type. */
static const struct symbolic *as_symbolic(struct checker *c,
                                          const struct const_value *v)
{
    struct symbolic key;

    if (v->kind == CONST_SYMBOLIC) {
        return v->as.symbolic;
    }
    memset(&key, 0, sizeof key);
    key.kind = SYMBOLIC_INT;
    key.type = v->type;
    key.value = v->kind == CONST_BOOL   ? (uint64_t)v->as.boolean
                : v->kind == CONST_CHAR ? v->as.character
                                        : v->as.integer;
    return types_symbolic(c->types, &key);
}

/* Make out the symbolic value of an operation on a and, for a binary
   operation, b. */
static bool symbolic_value(struct checker *c, struct const_value *out,
                           enum symbolic_kind kind, enum token_kind op,
                           const struct const_value *a,
                           const struct const_value *b)
{
    struct symbolic key;

    memset(&key, 0, sizeof key);
    key.kind = kind;
    key.type = out->type;
    key.op = op;
    key.a = as_symbolic(c, a);
    key.b = b != NULL ? as_symbolic(c, b) : NULL;
    out->kind = CONST_SYMBOLIC;
    out->as.symbolic = types_symbolic(c->types, &key);
    return true;
}

/* True when a float converts to integer type t with a value that t
   holds after truncation toward zero. The truncated value is compared
   with the least value, since `lo - 1.0` rounds back to lo at 64 bits
   and would refuse the least value itself. NaN fits nothing. */
static bool float_fits(double x, const struct type *t)
{
    int bits = types_bits(t);
    double lo = types_is_signed(t) ? -ldexp(1.0, bits - 1) : 0.0;
    double hi = types_is_signed(t) ? ldexp(1.0, bits - 1) : ldexp(1.0, bits);

    return trunc(x) >= lo && x < hi;
}

/* Append the float x as an Anti float literal. It has the fewest
   significant digits that read back as x, and digits on both sides of the
   dot. An exponent follows outside 1e-5 to 1e21. NaN and the infinities
   have no literal and print as nan and inf. */
static void float_as_literal(struct text *out, double x, bool single)
{
    /* The longest text of "%.*e" below, at 16 digits after the point, is
       a sign, a digit, the point, the 16 digits and `e-308`: 24 bytes and
       the NUL. So neither snprintf cuts its text, and digits takes the 17
       digits of it. */
    char buffer[40];
    char digits[24];
    const char *p;
    size_t count = 0;
    size_t i;
    int precision;
    int exponent;

    if (isnan(x)) {
        text_append(out, "nan");
        return;
    }
    if (isinf(x)) {
        text_append(out, x < 0 ? "-inf" : "inf");
        return;
    }
    for (precision = 1; precision < 17; precision++) {
        snprintf(buffer, sizeof buffer, "%.*e", precision - 1, x);
        if (single ? strtof(buffer, NULL) == (float)x
                   : strtod(buffer, NULL) == x) {
            break;
        }
    }
    snprintf(buffer, sizeof buffer, "%.*e", precision - 1, x);
    p = buffer;
    if (*p == '-') {
        text_append(out, "-");
        p++;
    }
    for (; *p != 'e'; p++) {
        if (*p != '.') {
            digits[count++] = *p;
        }
    }
    exponent = atoi(p + 1);
    while (count > 1 && digits[count - 1] == '0') {
        count--;
    }
    if (exponent < -5 || exponent >= 21) {
        text_append_bytes(out, digits, 1);
        text_append(out, ".");
        text_append_bytes(out, count > 1 ? digits + 1 : "0",
                          count > 1 ? count - 1 : 1);
        text_appendf(out, "e%d", exponent);
    } else if (exponent < 0) {
        text_append(out, "0.");
        for (i = 1; i < (size_t)-exponent; i++) {
            text_append(out, "0");
        }
        text_append_bytes(out, digits, count);
    } else {
        for (i = 0; i <= (size_t)exponent; i++) {
            text_append_bytes(out, i < count ? digits + i : "0", 1);
        }
        text_append(out, ".");
        if (count > (size_t)exponent + 1) {
            text_append_bytes(out, digits + exponent + 1,
                              count - (size_t)exponent - 1);
        } else {
            text_append(out, "0");
        }
    }
}

/* Append an operand of a constant error in the form the reader wrote it.
   A literal keeps its spelling and a negated literal its sign. Any other
   operand prints as the literal of its value v. */
static void operand_text(struct text *out, const struct expr *e,
                         const struct const_value *v)
{
    const struct expr *literal = e;

    if (e->kind == EXPR_UNARY && e->as.unary.op == TOKEN_MINUS) {
        literal = e->as.unary.operand;
    }
    if ((literal->kind == EXPR_INT || literal->kind == EXPR_FLOAT) &&
        literal->spelling.length > 0) {
        text_append(out, literal == e ? "" : "-");
        text_append_bytes(out, literal->spelling.bytes,
                          literal->spelling.length);
    } else if (v->kind == CONST_SYMBOLIC) {
        types_symbolic_print(out, v->as.symbolic, false);
    } else if (v->kind == CONST_FLOAT) {
        float_as_literal(out, v->as.floating, v->type->kind == TYPE_F32);
    } else if (types_is_signed(v->type->kind == TYPE_ENUM ? v->type->base
                                                          : v->type)) {
        text_appendf(out, "%lld", (long long)sema_signed_bits(v->as.integer));
    } else {
        text_appendf(out, "%llu", (unsigned long long)v->as.integer);
    }
}

/* True when the integer constant a has a value that integer type t
   holds. A target-sized type is measured at 64 bits, as its constants
   are computed, and fits_every_target then holds it to its width. An
   enum counts as its base. */
static bool integer_fits(const struct const_value *a, const struct type *t)
{
    const struct type *from =
        a->type->kind == TYPE_ENUM ? a->type->base : a->type;
    int bits = types_is_target_sized(t) ? 64 : types_bits(t);
    int high = types_is_signed(t) ? bits - 1 : bits;

    if (types_is_signed(from) && sema_signed_bits(a->as.integer) < 0) {
        return types_is_signed(t) &&
               (bits == 64 ||
                sema_signed_bits(a->as.integer) >= -((int64_t)1 << high));
    }
    return high == 64 || a->as.integer < (uint64_t)1 << high;
}

/* True when v is a Unicode scalar value: at most 0x10FFFF and no
   surrogate. */
static bool is_scalar_value(uint64_t v)
{
    return v <= 0x10FFFF && (v < 0xD800 || v > 0xDFFF);
}

/* Report an operation on the constant operands a and b that has no value.
   Integer division and remainder fail by zero and for the minimum value by
   -1. A shift fails for a count outside the bits of the type, and `<<%`
   for a count below zero alone. `+`, `-`, `*` and unary `-` fail for a
   result outside the range of their type. A conversion fails for a value
   outside the range of the integer type, and for a value that is no
   `char`. e is a binary expression, a unary `-` or a conversion to
   result, and b is NULL for the last two. in_constant is set where e is
   computed as a constant, and clear where its operands are constants in
   an expression that runs. An unsigned `+`, `-` or `*` wraps when it
   runs, so it is reported in a constant alone. */
static bool reports_undefined(struct checker *c, const struct expr *e,
                              const struct type *result,
                              const struct const_value *a,
                              const struct const_value *b, bool in_constant)
{
    struct text x = {0};
    struct text y = {0};
    char spelling[OP_TEXT];
    bool found = false;

    if (e->kind == EXPR_CAST) {
        /* DESIGN: a dev build stops the same conversion of a variable
           at run time, by R026 and R027 of "Rules of 2026-10-13". */
        if (types_is_integer(result) &&
            (a->kind == CONST_FLOAT ? !float_fits(a->as.floating, result)
             : a->kind == CONST_INT && !integer_fits(a, result))) {
            operand_text(&x, e->as.cast.operand, a);
            sema_error_at(c, e->pos, "`%s as %s` does not fit `%s`",
                          text_cstr(&x), sema_tn(result), sema_tn(result));
            found = true;
        } else if (result->kind == TYPE_CHAR && a->kind == CONST_INT &&
                   !is_scalar_value(a->as.integer)) {
            operand_text(&x, e->as.cast.operand, a);
            sema_error_at(c, e->pos,
                          "`%s as char` is not a Unicode scalar value",
                          text_cstr(&x));
            found = true;
        }
    } else if (e->kind == EXPR_UNARY) {
        /* DESIGN: unary `-` of the least value has no value in its
           type, by R031 of "Rules of 2026-10-13". A literal under the
           `-` is the least value itself, which integer_literal holds to
           the range. */
        int bits = types_is_target_sized(result) ? 64 : types_bits(result);
        if (a->kind == CONST_INT && types_is_signed(result) &&
            e->as.unary.operand->kind != EXPR_INT &&
            arith_overflows('-', 0, a->as.integer, bits, true)) {
            operand_text(&x, e->as.unary.operand, a);
            sema_error_at(c, e->pos,
                          text_cstr(&x)[0] == '-'
                              ? "`-(%s)` does not fit `%s`"
                              : "`-%s` does not fit `%s`",
                          text_cstr(&x), sema_tn(result));
            found = true;
        }
    } else if (types_is_integer(e->as.binary.left->type) &&
               b->kind != CONST_SYMBOLIC) {
        enum token_kind op = e->as.binary.op;
        const struct type *t = e->as.binary.left->type;
        int bits = types_bits(t);
        uint64_t mask = bits == 64 ? UINT64_MAX : ((uint64_t)1 << bits) - 1;
        bool divides = op == TOKEN_SLASH || op == TOKEN_PERCENT;
        const char *o = sema_op_text(op, spelling);
        char sign = op == TOKEN_PLUS    ? '+'
                    : op == TOKEN_MINUS ? '-'
                    : op == TOKEN_STAR  ? '*'
                                        : 0;

        operand_text(&x, e->as.binary.left, a);
        operand_text(&y, e->as.binary.right, b);
        if (divides && b->as.integer == 0) {
            sema_error_at(c, e->pos, "`%s %s %s` divides by zero",
                          text_cstr(&x), o,
                          text_cstr(&y));
            found = true;
        } else if (divides && a->kind != CONST_SYMBOLIC && types_is_signed(t) &&
                   (a->as.integer & mask) == (uint64_t)1 << (bits - 1) &&
                   sema_signed_bits(b->as.integer) == -1) {
            sema_error_at(c, e->pos, "`%s %s %s` does not fit `%s`",
                          text_cstr(&x),
                          o, text_cstr(&y), sema_tn(t));
            found = true;
        } else if ((op == TOKEN_SHL || op == TOKEN_SHR) &&
                   ((types_is_signed(t) &&
                     sema_signed_bits(b->as.integer) < 0) ||
                    b->as.integer >= (uint64_t)bits)) {
            sema_error_at(c, e->pos, "`%s %s %s` shifts out of range",
                          text_cstr(&x), o, text_cstr(&y));
            found = true;
        } else if (op == TOKEN_SHL_WRAP && types_is_signed(t) &&
                   sema_signed_bits(b->as.integer) < 0) {
            /* DESIGN: `<<%` gives 0 for a count at or above the width,
               and a count below zero is no count, by R032 of "Rules of
               2026-10-13". */
            sema_error_at(c, e->pos, "`%s %s %s` shifts out of range",
                          text_cstr(&x), o, text_cstr(&y));
            found = true;
        } else if (sign != 0 && a->kind != CONST_SYMBOLIC &&
                   (in_constant || types_is_signed(t)) &&
                   arith_overflows(sign, a->as.integer, b->as.integer,
                                   types_is_target_sized(t) ? 64 : bits,
                                   types_is_signed(t))) {
            /* DESIGN: a constant has the value the reader computes, by
               R029 of "Rules of 2026-10-13", in an unsigned type as
               well. `+%` and `+|` name a result outside the range. A
               signed operation that runs stops a dev build, so it is
               refused wherever its operands are constants. */
            sema_error_at(c, e->pos, "`%s %s %s` does not fit `%s`",
                          text_cstr(&x), o, text_cstr(&y), sema_tn(t));
            found = true;
        }
    }
    text_free(&x);
    text_free(&y);
    return found;
}

/* DESIGN: C leaves these operations undefined, so no value exists that
   the constant folder could produce. With constant operands they are
   compile errors wherever they appear. A variable operand leaves them to
   run time, as chapter 2 states. The operands are already checked, so a
   constant name among them has its value, and the quiet evaluation adds
   no message of its own. */
bool sema_undefined_on_constants(struct checker *c, struct expr *e,
                                 const struct type *result)
{
    struct const_value a;
    struct const_value b;
    struct context saved;
    bool constant;

    sema_enter_quiet(c, &saved);
    if (e->kind == EXPR_CAST) {
        constant = sema_eval_const(c, e->as.cast.operand, &a);
    } else if (e->kind == EXPR_UNARY) {
        constant = sema_eval_const(c, e->as.unary.operand, &a);
    } else {
        constant = sema_eval_const(c, e->as.binary.left, &a) &&
                   sema_eval_const(c, e->as.binary.right, &b);
    }
    sema_leave(c, &saved);
    return constant &&
           reports_undefined(c, e, result, &a,
                             e->kind == EXPR_BINARY ? &b : NULL, false);
}

/* DESIGN: a constant stored into a bitfield fits its width, by R048 of
   "Rules of 2026-10-13", as a constant fits the type of a narrowing `as`.
   A signed field of n bits holds -2^(n-1) to 2^(n-1) - 1, and an unsigned
   one 0 to 2^n - 1. A value that is no constant is checked where a dev
   build stores it. value is checked already and has the type of field. */
bool sema_bitfield_constant(struct checker *c, struct expr *value,
                            const struct struct_field *field)
{
    struct const_value v;
    struct context saved;
    struct text x = {0};
    int bits = field->bits;
    bool constant;
    bool fits;

    if (bits == 0 || bits >= 64 || !types_is_integer(field->type)) {
        return false;
    }
    sema_enter_quiet(c, &saved);
    constant = sema_eval_const(c, value, &v);
    sema_leave(c, &saved);
    if (!constant || v.kind != CONST_INT) {
        return false;
    }
    fits = types_is_signed(field->type)
               ? sema_signed_bits(v.as.integer) >=
                         -((int64_t)1 << (bits - 1)) &&
                     sema_signed_bits(v.as.integer) <
                         ((int64_t)1 << (bits - 1))
               : v.as.integer < ((uint64_t)1 << bits);
    if (fits) {
        return false;
    }
    operand_text(&x, value, &v);
    sema_error_at(c, value->pos, "`%s` does not fit the %d bit%s of `%.*s`",
                  text_cstr(&x), bits, bits == 1 ? "" : "s",
                  (int)field->name.length, field->name.text);
    text_free(&x);
    return true;
}

/* DESIGN: a constant index below zero on a fixed array names no element,
   by R030 of "Rules of 2026-10-13", and neither does one past the end,
   by decision 24 of "Decisions of 2026-10-11". The length is a number,
   or the text of a length computed from `size_of`, whose value differs
   between targets, so an index is held to such a length in dev mode
   alone. e is the index expression and array the type of its base. */
bool sema_index_outside(struct checker *c, struct expr *e,
                        const struct type *array)
{
    struct const_value v;
    struct context saved;
    struct text length = {0};
    bool constant;

    sema_enter_quiet(c, &saved);
    constant = sema_eval_const(c, e, &v);
    sema_leave(c, &saved);
    if (!constant || v.kind != CONST_INT ||
        (sema_signed_bits(v.as.integer) >= 0 &&
         (array->length_of != NULL || v.as.integer < array->length))) {
        return false;
    }
    if (array->length_of != NULL) {
        types_symbolic_print(&length, array->length_of, false);
    } else {
        text_appendf(&length, "%llu", (unsigned long long)array->length);
    }
    sema_error_at(c, e->pos, "the index %lld is outside the array of length "
                  "%s", (long long)sema_signed_bits(v.as.integer),
                  text_cstr(&length));
    text_free(&length);
    return true;
}

/* DESIGN: a constant holds one value per element, and lowering writes
   one item per element into the data of the module. A repeated array of
   more than 2^20 elements is refused before the checker allocates them.
   The count goes through nested arrays and the fields of structs. The
   product of a length and a size then cannot wrap. */
#define CONST_ELEMENTS_MAX (1 << 20)

/* The scalar elements of a value of type t, or UINT64_MAX once the count
   passes that. */
static uint64_t const_elements(const struct type *t)
{
    uint64_t total = 0;
    uint64_t one;
    size_t i;

    if (t == NULL) {
        return 1;
    }
    if (t->kind == TYPE_ARRAY) {
        one = const_elements(t->element);
        return one != 0 && t->length > UINT64_MAX / one ? UINT64_MAX
                                                       : t->length * one;
    }
    if (!types_has_fields(t) || t->kind == TYPE_CLASS) {
        return 1;
    }
    for (i = 0; i < t->field_count; i++) {
        one = const_elements(t->fields[i].type);
        total = one > UINT64_MAX - total ? UINT64_MAX : total + one;
    }
    return total;
}

/* A cast of a constant operand. */
static bool const_as(struct checker *c, struct expr *e,
                     struct const_value *out)
{
    struct const_value a;

    if (!sema_eval_const(c, e->as.cast.operand, &a)) {
        return false;
    }
    out->type = e->type;
    if (a.kind != CONST_SYMBOLIC &&
        reports_undefined(c, e, e->type, &a, NULL, true)) {
        return false;
    }
    if (a.kind == CONST_SYMBOLIC) {
        if (!types_is_integer(e->type)) {
            sema_error_at(c, e->pos,
                          "a value computed from `size_of` converts "
                          "only to an integer type in a constant "
                          "expression");
            return false;
        }
        return symbolic_value(c, out, SYMBOLIC_CAST, TOKEN_AS, &a, NULL);
    }
    if (e->type->kind == TYPE_F16) {
        /* The runtime's own rounding, so a constant and a computed
           value of one f32 are the same sixteen bits. */
        out->kind = CONST_FLOAT;
        out->as.floating =
            anti_rt_f16_widen(anti_rt_f16_narrow((float)a.as.floating));
    } else if (types_is_float(e->type)) {
        /* An integer converts to an f32 in one rounding, as the program
           does: through an f64 first, a value just above the middle of
           two f32 neighbours lands on the middle and rounds the other
           way. An enum converts as its base. */
        const struct type *from =
            a.type->kind == TYPE_ENUM ? a.type->base : a.type;
        bool single = e->type->kind == TYPE_F32;
        out->kind = CONST_FLOAT;
        if (a.kind == CONST_FLOAT) {
            out->as.floating =
                single ? (float)a.as.floating : a.as.floating;
        } else if (types_is_signed(from)) {
            int64_t n = sema_signed_bits(a.as.integer);
            out->as.floating = single ? (double)(float)n : (double)n;
        } else {
            out->as.floating = single ? (double)(float)a.as.integer
                                      : (double)a.as.integer;
        }
    } else if (types_is_integer(e->type)) {
        out->kind = CONST_INT;
        if (a.kind == CONST_FLOAT) {
            out->as.integer = types_is_signed(e->type)
                                  ? (uint64_t)(int64_t)a.as.floating
                                  : (uint64_t)a.as.floating;
        } else if (a.kind == CONST_BOOL) {
            out->as.integer = a.as.boolean ? 1 : 0;
        } else if (a.kind == CONST_CHAR) {
            out->as.integer = a.as.character;
        } else {
            out->as.integer = a.as.integer;
        }
        wrap(out);
    } else if (e->type->kind == TYPE_CHAR) {
        out->kind = CONST_CHAR;
        out->as.character = (uint32_t)a.as.integer;
    } else if (e->type->kind == TYPE_ENUM) {
        return fail_const(c, e, "a conversion to an enum");
    } else {
        return fail_const(c, e, "a pointer conversion");
    }
    return fits_every_target(c, e, out);
}

/* A unary operator on a constant operand. */
static bool const_unary(struct checker *c, struct expr *e,
                        struct const_value *out)
{
    struct const_value a;

    if (e->as.unary.op == TOKEN_AMP || e->as.unary.op == TOKEN_STAR) {
        return fail_const(c, e, "an address or a dereference");
    }
    if (!sema_eval_const(c, e->as.unary.operand, &a)) {
        return false;
    }
    if (a.kind == CONST_SYMBOLIC) {
        return symbolic_value(c, out, SYMBOLIC_UNARY, e->as.unary.op, &a,
                              NULL);
    }
    if (e->as.unary.op == TOKEN_MINUS &&
        reports_undefined(c, e, e->type, &a, NULL, true)) {
        return false;
    }
    *out = a;
    out->type = e->type;
    if (e->as.unary.op == TOKEN_BANG) {
        out->as.boolean = !a.as.boolean;
    } else if (e->as.unary.op == TOKEN_TILDE) {
        out->as.integer = ~a.as.integer;
        wrap(out);
    } else if (a.kind == CONST_FLOAT) {
        out->as.floating = -a.as.floating;
    } else {
        out->as.integer = (uint64_t)0 - a.as.integer;
        wrap(out);
    }
    return fits_every_target(c, e, out);
}

/* The comparison op of the constants a and b into the bool out, the
   operands being signed integers when is_signed holds. */
static void const_compare(enum token_kind op, const struct const_value *a,
                          const struct const_value *b, bool is_signed,
                          struct const_value *out)
{
    bool is_float = a->kind == CONST_FLOAT;
    int cmp;

    out->kind = CONST_BOOL;
    if (is_float) {
        cmp = a->as.floating < b->as.floating ? -1
              : a->as.floating > b->as.floating ? 1 : 0;
    } else if (a->kind == CONST_NULL || b->kind == CONST_NULL) {
        cmp = a->kind == b->kind ? 0 : 1;
    } else if (a->kind == CONST_TEXT && b->kind == CONST_TEXT) {
        cmp = compare_text(&a->as.text, &b->as.text);
    } else if (is_signed) {
        cmp = sema_signed_bits(a->as.integer) <
                      sema_signed_bits(b->as.integer)
                  ? -1
              : sema_signed_bits(a->as.integer) >
                      sema_signed_bits(b->as.integer)
                  ? 1
                  : 0;
    } else {
        uint64_t x = a->kind == CONST_CHAR ? a->as.character
                     : a->kind == CONST_BOOL ? a->as.boolean
                                             : a->as.integer;
        uint64_t y = b->kind == CONST_CHAR ? b->as.character
                     : b->kind == CONST_BOOL ? b->as.boolean
                                             : b->as.integer;
        cmp = x < y ? -1 : x > y ? 1 : 0;
    }
    out->as.boolean = op == TOKEN_EQ ? cmp == 0
                      : op == TOKEN_NE ? cmp != 0
                      : op == TOKEN_LT ? cmp < 0
                      : op == TOKEN_LE ? cmp <= 0
                      : op == TOKEN_GT ? cmp > 0
                                       : cmp >= 0;
    if (is_float && (isnan(a->as.floating) || isnan(b->as.floating))) {
        out->as.boolean = op == TOKEN_NE;
    }
}

/* The integer operation op of the constants a and b, whose operands have
   type operand, into out before it wraps. */
static void const_integer(enum token_kind op, const struct type *operand,
                          const struct const_value *a,
                          const struct const_value *b,
                          struct const_value *out)
{
    bool is_signed = types_is_signed(operand);
    int bits = types_bits(operand);

    switch (op) {
    case TOKEN_PLUS:
    case TOKEN_PLUS_WRAP:
        out->as.integer = a->as.integer + b->as.integer;
        break;
    case TOKEN_MINUS:
    case TOKEN_MINUS_WRAP:
        out->as.integer = a->as.integer - b->as.integer;
        break;
    case TOKEN_STAR:
    case TOKEN_STAR_WRAP:
        out->as.integer = a->as.integer * b->as.integer;
        break;
    /* A count at or above the width gives 0, and a negative count
       holds its sign in the bits above, which makes it one. A
       target-sized type computes at 64 bits and must fit the narrower
       width, as every constant of it does. */
    case TOKEN_SHL_WRAP:
        out->as.integer =
            b->as.integer >= (uint64_t)(types_is_target_sized(operand)
                                            ? 64
                                            : bits)
                ? 0
                : a->as.integer << b->as.integer;
        break;
    case TOKEN_PLUS_SAT:
    case TOKEN_MINUS_SAT:
    case TOKEN_STAR_SAT:
        out->as.integer = arith_saturate(
            op == TOKEN_PLUS_SAT ? '+' : op == TOKEN_MINUS_SAT ? '-' : '*',
            a->as.integer, b->as.integer,
            types_is_target_sized(operand) ? 64 : bits, is_signed);
        break;
    case TOKEN_MUL_HIGH:
        out->as.integer =
            arith_mul_high(a->as.integer, b->as.integer, bits, is_signed);
        break;
    case TOKEN_AMP: out->as.integer = a->as.integer & b->as.integer; break;
    case TOKEN_PIPE: out->as.integer = a->as.integer | b->as.integer; break;
    case TOKEN_CARET: out->as.integer = a->as.integer ^ b->as.integer; break;
    case TOKEN_SLASH:
    case TOKEN_PERCENT: {
        if (is_signed) {
            int64_t x = sema_signed_bits(a->as.integer);
            int64_t y = sema_signed_bits(b->as.integer);
            out->as.integer = (uint64_t)(op == TOKEN_SLASH ? x / y : x % y);
        } else {
            out->as.integer = op == TOKEN_SLASH
                                  ? a->as.integer / b->as.integer
                                  : a->as.integer % b->as.integer;
        }
        break;
    }
    case TOKEN_SHL:
    case TOKEN_SHR:
        if (op == TOKEN_SHL) {
            out->as.integer = a->as.integer << b->as.integer;
        } else if (is_signed) {
            /* An arithmetic shift in unsigned operations, since C
               leaves the shift of a negative value to the
               implementation. */
            out->as.integer = (a->as.integer >> 63) != 0
                                  ? ~(~a->as.integer >> b->as.integer)
                                  : a->as.integer >> b->as.integer;
        } else {
            uint64_t mask = bits == 64 ? UINT64_MAX
                                       : ((uint64_t)1 << bits) - 1;
            out->as.integer = (a->as.integer & mask) >> b->as.integer;
        }
        break;
    default:
        break;
    }
}

/* The number s holds, a node of kind SYMBOLIC_INT, as a constant. */
static void symbolic_number(const struct symbolic *s, struct const_value *out)
{
    memset(out, 0, sizeof *out);
    out->type = s->type;
    if (s->type->kind == TYPE_BOOL) {
        out->kind = CONST_BOOL;
        out->as.boolean = s->value != 0;
    } else {
        out->kind = CONST_INT;
        out->as.integer = s->value;
    }
}

/* Whether the integer operation op of the numbers a and b, of type t, has
   a value in its type, by the rules reports_undefined holds a constant
   to. */
static bool defined_integer(enum token_kind op, const struct type *t,
                            const struct const_value *a,
                            const struct const_value *b)
{
    int bits = types_bits(t);
    uint64_t mask = bits == 64 ? UINT64_MAX : ((uint64_t)1 << bits) - 1;
    bool divides = op == TOKEN_SLASH || op == TOKEN_PERCENT;
    char sign = op == TOKEN_PLUS    ? '+'
                : op == TOKEN_MINUS ? '-'
                : op == TOKEN_STAR  ? '*'
                                    : 0;

    if (divides && b->as.integer == 0) {
        return false;
    }
    if (divides && types_is_signed(t) &&
        (a->as.integer & mask) == (uint64_t)1 << (bits - 1) &&
        sema_signed_bits(b->as.integer) == -1) {
        return false;
    }
    if ((op == TOKEN_SHL || op == TOKEN_SHR) &&
        ((types_is_signed(t) && sema_signed_bits(b->as.integer) < 0) ||
         b->as.integer >= (uint64_t)bits)) {
        return false;
    }
    if (op == TOKEN_SHL_WRAP && types_is_signed(t) &&
        sema_signed_bits(b->as.integer) < 0) {
        return false;
    }
    if (op == TOKEN_MUL_HIGH && types_is_target_sized(t)) {
        return false;
    }
    return sign == 0 ||
           !arith_overflows(sign, a->as.integer, b->as.integer,
                            types_is_target_sized(t) ? 64 : bits,
                            types_is_signed(t));
}

/* DESIGN: a copy of a generic puts the value of each constant parameter
   in place of it, and an operation whose operands are then numbers is
   the number the evaluator gives it, by T107 of
   docs/audit/tests-findings.md. `[-(-N)]int` of `Row<3>` is `[3]int`. An
   operation without a value in its type stays as it is, and the back
   end refuses it on each target. */
const struct symbolic *sema_fold_symbolic(struct checker *c,
                                          const struct symbolic *s)
{
    struct const_value a;
    struct const_value b;
    struct const_value out;
    const struct type *operand;

    if ((s->kind != SYMBOLIC_UNARY && s->kind != SYMBOLIC_BINARY &&
         s->kind != SYMBOLIC_CAST) ||
        s->a == NULL || s->a->kind != SYMBOLIC_INT ||
        (s->kind == SYMBOLIC_BINARY &&
         (s->b == NULL || s->b->kind != SYMBOLIC_INT))) {
        return s;
    }
    operand = s->a->type;
    symbolic_number(s->a, &a);
    memset(&out, 0, sizeof out);
    out.type = s->type;
    if (s->kind == SYMBOLIC_CAST) {
        if (!types_is_integer(operand) || !types_is_integer(s->type) ||
            !integer_fits(&a, s->type)) {
            return s;
        }
        out.kind = CONST_INT;
        out.as.integer = a.as.integer;
    } else if (s->kind == SYMBOLIC_UNARY) {
        if (s->op == TOKEN_BANG && a.kind == CONST_BOOL) {
            out.kind = CONST_BOOL;
            out.as.boolean = !a.as.boolean;
            return as_symbolic(c, &out);
        }
        if (!types_is_integer(operand) ||
            (s->op != TOKEN_MINUS && s->op != TOKEN_TILDE) ||
            (s->op == TOKEN_MINUS && types_is_signed(operand) &&
             arith_overflows('-', 0, a.as.integer,
                             types_is_target_sized(operand)
                                 ? 64
                                 : types_bits(operand),
                             true))) {
            return s;
        }
        out.kind = CONST_INT;
        out.as.integer = s->op == TOKEN_MINUS ? (uint64_t)0 - a.as.integer
                                              : ~a.as.integer;
    } else {
        symbolic_number(s->b, &b);
        if (s->type->kind == TYPE_BOOL &&
            (s->op == TOKEN_AND_AND || s->op == TOKEN_OR_OR)) {
            out.kind = CONST_BOOL;
            out.as.boolean = s->op == TOKEN_AND_AND
                                 ? a.as.boolean && b.as.boolean
                                 : a.as.boolean || b.as.boolean;
            return as_symbolic(c, &out);
        }
        if (s->type->kind == TYPE_BOOL) {
            const_compare(s->op, &a, &b, types_is_signed(operand), &out);
            return as_symbolic(c, &out);
        }
        if (!types_is_integer(operand) ||
            !defined_integer(s->op, operand, &a, &b)) {
            return s;
        }
        out.kind = CONST_INT;
        const_integer(s->op, operand, &a, &b, &out);
    }
    wrap(&out);
    return one_value(&out) ? as_symbolic(c, &out) : s;
}

/* A binary operator on constant operands. */
static bool const_binary(struct checker *c, struct expr *e,
                         struct const_value *out)
{
    struct const_value a;
    struct const_value b;
    enum token_kind op = e->as.binary.op;
    struct type *operand = e->as.binary.left->type;
    bool is_float;

    if (!sema_eval_const(c, e->as.binary.left, &a)) {
        return false;
    }
    /* `??` is not among the forms of a constant expression, which the
       entry on `const` under "Declarations and statements" in
       docs/decisions.md lists, whatever its left side holds. */
    if (op == TOKEN_QUESTION_QUESTION) {
        return fail_const(c, e, "`??`");
    }
    if (op == TOKEN_AND_AND || op == TOKEN_OR_OR) {
        if (a.kind != CONST_SYMBOLIC &&
            a.as.boolean == (op == TOKEN_OR_OR)) {
            *out = a;
            return true;
        }
        if (a.kind != CONST_SYMBOLIC) {
            return sema_eval_const(c, e->as.binary.right, out);
        }
    }
    if (!sema_eval_const(c, e->as.binary.right, &b)) {
        return false;
    }
    if (reports_undefined(c, e, e->type, &a, &b, true)) {
        return false;
    }
    /* The upper half of a product of c_long has another value at
       each width, and no symbolic value carries the signedness of
       c_wchar. */
    if (op == TOKEN_MUL_HIGH && types_is_target_sized(operand)) {
        return fail_const(c, e, "`" MUL_HIGH "` of a type whose width "
                          "the target decides");
    }
    if (a.kind == CONST_SYMBOLIC || b.kind == CONST_SYMBOLIC) {
        out->type = e->type;
        return symbolic_value(c, out, SYMBOLIC_BINARY, op, &a, &b);
    }
    is_float = a.kind == CONST_FLOAT;
    out->kind = CONST_INT;
    out->type = e->type;
    if (e->type->kind == TYPE_BOOL) {
        const_compare(op, &a, &b, types_is_signed(operand), out);
        return true;
    }
    if (is_float) {
        out->kind = CONST_FLOAT;
        out->as.floating = op == TOKEN_PLUS    ? a.as.floating + b.as.floating
                           : op == TOKEN_MINUS ? a.as.floating - b.as.floating
                           : op == TOKEN_STAR  ? a.as.floating * b.as.floating
                                               : a.as.floating / b.as.floating;
        if (e->type->kind == TYPE_F32) {
            out->as.floating = (float)out->as.floating;
        }
        return true;
    }
    const_integer(op, operand, &a, &b, out);
    wrap(out);
    return fits_every_target(c, e, out);
}

/* The constant of kind of the count values, an array or a tuple. */
static bool const_items(struct checker *c, struct expr *const *values,
                        size_t count, enum const_kind kind,
                        struct const_value *out)
{
    size_t i;

    out->kind = kind;
    out->as.aggregate.count = count;
    out->as.aggregate.items =
        types_alloc_array(c->arena, count, sizeof *out->as.aggregate.items);
    for (i = 0; i < count; i++) {
        if (!sema_eval_const(c, values[i], &out->as.aggregate.items[i])) {
            return false;
        }
    }
    return true;
}

/* `[v; n]` of a constant v. */
static bool const_repeat(struct checker *c, struct expr *e,
                         struct const_value *out)
{
    struct const_value a;
    size_t i;

    /* The default of a field keeps the value once, by R043 and R045 of
       "Rules of 2026-10-13". */
    if (e->type->length_of != NULL && c->ctx.target_sized) {
        out->kind = CONST_REPEAT;
        out->as.aggregate.count = 1;
        out->as.aggregate.items =
            arena_alloc(c->arena, sizeof *out->as.aggregate.items);
        return sema_eval_const(c, e->as.array_repeat.value,
                               &out->as.aggregate.items[0]);
    }
    if (e->type->length_of != NULL) {
        return fail_const(c, e, "an array with a length from `size_of`");
    }
    if (const_elements(e->type) > CONST_ELEMENTS_MAX) {
        sema_error_at(c, e->pos,
                      "an array of more than %d elements is not a "
                      "constant expression", CONST_ELEMENTS_MAX);
        return false;
    }
    if (!sema_eval_const(c, e->as.array_repeat.value, &a)) {
        return false;
    }
    out->kind = CONST_ARRAY;
    out->as.aggregate.count = e->type->length;
    out->as.aggregate.items = types_alloc_array(
        c->arena, e->type->length, sizeof *out->as.aggregate.items);
    for (i = 0; i < e->type->length; i++) {
        out->as.aggregate.items[i] = a;
    }
    return true;
}

/* A struct literal of constant fields. */
static bool const_struct(struct checker *c, struct expr *e,
                         struct const_value *out)
{
    struct type *s = e->type;
    size_t i;
    size_t j;

    if (s->is_union) {
        return fail_const(c, e, "a union");
    }
    if (s->kind == TYPE_VARIANT) {
        return fail_const(c, e, "a variant");
    }
    if (s->kind == TYPE_CLASS) {
        return class_value(c, e, s, out);
    }
    out->kind = CONST_STRUCT;
    out->as.aggregate.count = s->field_count;
    out->as.aggregate.items =
        types_alloc_array(c->arena, s->field_count,
                          sizeof *out->as.aggregate.items);
    /* A zero-width bitfield holds no value and keeps a zero. */
    for (j = 0; j < s->field_count; j++) {
        out->as.aggregate.items[j].kind = CONST_INT;
        out->as.aggregate.items[j].type = s->fields[j].type;
        out->as.aggregate.items[j].as.integer = 0;
    }
    for (i = 0; i < e->as.struct_lit.field_count; i++) {
        for (j = 0; j < s->field_count; j++) {
            if (sema_same_name(&s->fields[j].name,
                               &e->as.struct_lit.fields[i].name) &&
                !sema_eval_const(c, e->as.struct_lit.fields[i].value,
                                 &out->as.aggregate.items[j])) {
                return false;
            }
        }
    }
    return true;
}

/* A field of a constant: a value of an enum, a field of a constant
   struct, or the length of a constant array, a string literal or a
   constant `str`. */
static bool const_field(struct checker *c, struct expr *e,
                        struct const_value *out)
{
    struct const_value a;
    const struct struct_field *f;
    struct type *base;

    /* A value of an enum is the number the declaration folded. Its
       base is a type name, which carries no value of its own. */
    if (e->as.field.enum_value != 0 && e->type->kind == TYPE_ENUM) {
        out->kind = CONST_INT;
        out->type = e->type;
        out->as.integer =
            e->type->fields[e->as.field.enum_value - 1].number;
        return true;
    }
    /* A field of what a call gives names the call, which is the part
       that is no constant expression, by T077 of
       docs/audit/tests-findings.md. */
    if (e->as.field.base->kind == EXPR_CALL) {
        return sema_eval_const(c, e->as.field.base, &a) &&
               fail_const(c, e, "this field");
    }
    /* A constant of a class, `Name.NAME`, is a constant expression, as
       "Classes" in docs/anti-object-model.md declares it. */
    if (e->symbol != NULL && e->symbol->kind == SYMBOL_CONST) {
        if (!sema_const_symbol(c, e->symbol, e->pos)) {
            return false;
        }
        *out = *e->symbol->value;
        return true;
    }
    /* A function and a static of a type are read at run time. */
    if (e->symbol != NULL) {
        return fail_const(c, e, "this field");
    }
    base = e->as.field.base->type;
    /* The value of a class literal leaves its tables and the fields
       it does not name to lowering, as CONST_DEFAULT. A field of it
       is therefore read only at run time. */
    if (base->kind == TYPE_CLASS) {
        return fail_const(c, e, "a field of a class");
    }
    if (types_has_fields(base)) {
        if (!sema_eval_const(c, e->as.field.base, &a)) {
            return false;
        }
        f = types_find_field(base, &e->as.field.name);
        if (f == NULL || a.kind != CONST_STRUCT ||
            (size_t)(f - base->fields) >= a.as.aggregate.count) {
            return fail_const(c, e, "this field");
        }
        *out = a.as.aggregate.items[f - base->fields];
        return true;
    }
    if (sema_name_is(&e->as.field.name, "len") &&
        base->kind == TYPE_ARRAY &&
        base->length_of != NULL) {
        return fail_const(c, e, "the length of an array from `size_of`");
    }
    /* The length of a constant `str` is the count of its bytes, by R033
       of "Rules of 2026-10-13". */
    if (sema_name_is(&e->as.field.name, "len") && base->kind == TYPE_STR &&
        e->as.field.base->kind != EXPR_STRING) {
        if (!sema_eval_const(c, e->as.field.base, &a)) {
            return false;
        }
        if (a.kind != CONST_TEXT) {
            return fail_const(c, e, "this field");
        }
        out->kind = CONST_INT;
        out->as.integer = a.as.text.length;
        return true;
    }
    /* The base of .len is a constant array or a string literal. */
    if (sema_name_is(&e->as.field.name, "len") &&
        base->kind == TYPE_ARRAY &&
        !sema_eval_const(c, e->as.field.base, &a)) {
        return false;
    }
    if (sema_name_is(&e->as.field.name, "len") &&
        (base->kind == TYPE_ARRAY ||
         e->as.field.base->kind == EXPR_STRING)) {
        out->kind = CONST_INT;
        out->as.integer = base->kind == TYPE_ARRAY
                              ? base->length
                              : e->as.field.base->as.text.length;
        return true;
    }
    return fail_const(c, e, "this field");
}

/* An element of a constant array at a constant index. */
static bool const_index(struct checker *c, struct expr *e,
                        struct const_value *out)
{
    struct const_value a;
    struct const_value b;

    if (e->as.index.base->type->kind != TYPE_ARRAY) {
        return fail_const(c, e, "indexing anything but a constant array");
    }
    if (!sema_eval_const(c, e->as.index.base, &a) ||
        !sema_eval_const(c, e->as.index.index, &b)) {
        return false;
    }
    if (b.kind == CONST_SYMBOLIC) {
        return fail_const(c, e->as.index.index,
                          "an index computed from `size_of`");
    }
    /* A repeated array of a length that is no number holds its value
       once, and no index is known to stand inside it. */
    if (a.kind == CONST_REPEAT) {
        return fail_const(c, e, "an element of an array whose length is no "
                          "number");
    }
    /* The checker refused an index outside the array where it checked
       the expression, by sema_index_outside, so none reaches the fold. */
    if (b.as.integer >= a.as.aggregate.count) {
        return false;
    }
    *out = a.as.aggregate.items[b.as.integer];
    return true;
}

/* The two comparisons of numbers fold, with the value in both. An
   `lt` operator is a call and never a constant. */
static bool const_in(struct checker *c, struct expr *e,
                     struct const_value *out)
{
    struct expr *sides[3];
    const struct type *t = e->as.in.value->type;
    size_t i;

    if (!types_is_numeric(t) && t->kind != TYPE_CHAR) {
        return fail_const(c, e, "a call");
    }
    for (i = 0; i < 3; i++) {
        sides[i] = sema_new_node(c, EXPR_BINARY, e->pos);
        sides[i]->type = sema_builtin(c, TYPE_BOOL);
    }
    sides[0]->as.binary.op = TOKEN_GE;
    sides[0]->as.binary.left = e->as.in.value;
    sides[0]->as.binary.right = e->as.in.low;
    sides[1]->as.binary.op = TOKEN_LT;
    sides[1]->as.binary.left = e->as.in.value;
    sides[1]->as.binary.right = e->as.in.high;
    sides[2]->as.binary.op = TOKEN_AND_AND;
    sides[2]->as.binary.left = sides[0];
    sides[2]->as.binary.right = sides[1];
    return sema_eval_const(c, sides[2], out);
}

/* Evaluate a checked expression. The expression must use only what
   chapter 2 allows in a constant. */
bool sema_eval_const(struct checker *c, struct expr *e,
                     struct const_value *out)
{
    memset(out, 0, sizeof *out);
    out->type = e->type;
    switch (e->kind) {
    case EXPR_INT:
        out->kind = CONST_INT;
        out->as.integer = e->as.integer;
        wrap(out);
        return true;
    case EXPR_FLOAT:
        out->kind = CONST_FLOAT;
        out->as.floating = arith_float_literal(
            e->as.text.bytes, e->as.text.length, e->type->kind == TYPE_F32);
        return true;
    case EXPR_CHAR:
        out->kind = CONST_CHAR;
        out->as.character = e->as.character;
        return true;
    case EXPR_BOOL:
        out->kind = CONST_BOOL;
        out->as.boolean = e->as.boolean;
        return true;
    case EXPR_NONE:
        out->kind = CONST_NULL;
        return true;
    case EXPR_STRING:
    case EXPR_BYTES:
        out->kind = CONST_TEXT;
        out->as.text = e->as.text;
        return true;
    case EXPR_NAME:
        if (e->symbol != NULL && (e->symbol->kind == SYMBOL_FN ||
                                  e->symbol->kind == SYMBOL_EXTERN_FN)) {
            return fail_const(c, e, "a function");
        }
        if (e->symbol == NULL || e->symbol->kind != SYMBOL_CONST) {
            return fail_const(c, e, "a variable");
        }
        if (!sema_const_symbol(c, e->symbol, e->pos)) {
            return false;
        }
        *out = *e->symbol->value;
        return true;
    case EXPR_SIZE_OF: {
        struct symbolic key;
        memset(&key, 0, sizeof key);
        key.kind = SYMBOLIC_SIZE_OF;
        key.type = e->type;
        key.of = e->as.size_of->type;
        out->kind = CONST_SYMBOLIC;
        out->as.symbolic = types_symbolic(c->types, &key);
        return !sema_is_error(key.of);
    }
    case EXPR_CAST:
        return const_as(c, e, out);
    case EXPR_UNARY:
        return const_unary(c, e, out);
    case EXPR_BINARY:
        return const_binary(c, e, out);
    case EXPR_ARRAY_LIT:
        return const_items(c, e->as.array_lit.elements,
                           e->as.array_lit.count, CONST_ARRAY, out);
    /* A tuple is a struct, so a constant one is the constant struct of
       its elements. */
    case EXPR_TUPLE:
        return const_items(c, e->as.tuple.elements, e->as.tuple.count,
                           CONST_STRUCT, out);
    case EXPR_ARRAY_REPEAT:
        return const_repeat(c, e, out);
    case EXPR_STRUCT_LIT:
        return const_struct(c, e, out);
    case EXPR_FIELD:
        return const_field(c, e, out);
    case EXPR_INDEX:
        return const_index(c, e, out);
    case EXPR_CALL:
    case EXPR_FREE:
    case EXPR_OBJECT:
    case EXPR_ATOMIC:
        return fail_const(c, e, "a call");
    case EXPR_ALLOC:
        return fail_const(c, e, "`alloc`");
    case EXPR_PARALLEL:
        return fail_const(c, e, "`parallel`");
    case EXPR_DISPATCH:
    case EXPR_JOIN:
    case EXPR_SYNC_OP:
    case EXPR_SIMD:
    case EXPR_DESCRIPTOR:
        return fail_const(c, e, "a call");
    case EXPR_SLICE:
    case EXPR_SLICE_LIT:
        return fail_const(c, e, "a slice");
    /* The position is data of the call or of the function around it.
       It is a default of a parameter and never a constant. */
    case EXPR_HERE:
        return fail_const(c, e, "`here`");
    /* The text is built at run time, into memory of its own. */
    case EXPR_FORMAT:
        return fail_const(c, e, sema_format_name(e));
    /* The pattern is compiled at start, into memory of the runtime. */
    case EXPR_PATTERN:
        return fail_const(c, e, "a pattern literal");
    /* The slice is collected at run time, into memory of its own. */
    case EXPR_COLLECT:
        return fail_const(c, e, "`to_slice`");
    case EXPR_FN:
        return fail_const(c, e, "an anonymous function");
    case EXPR_IN:
        return const_in(c, e, out);
    /* The field or the call reads through a pointer. */
    case EXPR_OPTIONAL:
        return fail_const(c, e, "`?.`");
    }
    return false;
}

/* DESIGN: a constant that names another evaluates that one first. A
   long chain is taken apart by sema_chain_prepare. CONST_DEPTH_MAX
   bounds what is left: a cycle through a long chain, or a form the walk
   does not follow. */
#define CONST_DEPTH_MAX 64

/* Add sym to d when it is a constant with no value yet. */
static void const_deps_add(struct chain_deps *d, struct symbol *sym)
{
    if (sym != NULL && sym->kind == SYMBOL_CONST) {
        sema_chain_add(d, sym);
    }
}

/* Collect the constants e names that have no value yet. The walk
   follows the forms of a constant expression. */
static void const_deps_of(struct chain_deps *d, const struct expr *e)
{
    size_t i;

    if (e == NULL) {
        return;
    }
    switch (e->kind) {
    case EXPR_NAME:
        const_deps_add(d, sema_lookup(d->c, &e->as.name));
        return;
    case EXPR_FIELD:
        const_deps_of(d, e->as.field.base);
        return;
    case EXPR_UNARY:
        const_deps_of(d, e->as.unary.operand);
        return;
    case EXPR_BINARY:
        const_deps_of(d, e->as.binary.left);
        const_deps_of(d, e->as.binary.right);
        return;
    case EXPR_CAST:
        const_deps_of(d, e->as.cast.operand);
        return;
    case EXPR_INDEX:
        const_deps_of(d, e->as.index.base);
        const_deps_of(d, e->as.index.index);
        return;
    case EXPR_IN:
        const_deps_of(d, e->as.in.value);
        const_deps_of(d, e->as.in.low);
        const_deps_of(d, e->as.in.high);
        return;
    case EXPR_ARRAY_LIT:
        for (i = 0; i < e->as.array_lit.count; i++) {
            const_deps_of(d, e->as.array_lit.elements[i]);
        }
        return;
    case EXPR_TUPLE:
        for (i = 0; i < e->as.tuple.count; i++) {
            const_deps_of(d, e->as.tuple.elements[i]);
        }
        return;
    case EXPR_ARRAY_REPEAT:
        const_deps_of(d, e->as.array_repeat.value);
        return;
    case EXPR_STRUCT_LIT:
        for (i = 0; i < e->as.struct_lit.field_count; i++) {
            const_deps_of(d, e->as.struct_lit.fields[i].value);
        }
        return;
    default:
        return;
    }
}

/* DESIGN: the default of a field of a class is a constant expression,
   and the checker reads it before it declares the types of functions. A
   call there finds no type yet and gives no message, so the walk refuses
   it first, with the message of the evaluator. `??` is refused at its
   place once its left side holds no call. The value of an enum, the
   width of a bitfield, the N of `align(N)` and the length of an array
   are read as early, by T020 of `docs/audit/tests-findings.md`, and take
   the same walk. So does a constant that one of them names before the
   functions have their types. Later the check of the call gives the
   message that names what it calls. */
bool sema_refuse_runtime_parts(struct checker *c, const struct expr *e)
{
    size_t i;

    if (e == NULL) {
        return false;
    }
    switch (e->kind) {
    case EXPR_CALL:
        /* The built-in `mul_high` becomes an operator, which folds. */
        if (e->as.call.callee->kind == EXPR_NAME &&
            sema_name_is(&e->as.call.callee->as.name, MUL_HIGH) &&
            sema_lookup(c, &e->as.call.callee->as.name) == NULL) {
            for (i = 0; i < e->as.call.arg_count; i++) {
                if (sema_refuse_runtime_parts(c, e->as.call.args[i])) {
                    return true;
                }
            }
            return false;
        }
        return !fail_const(c, e, "a call");
    case EXPR_FREE:
    case EXPR_OBJECT:
    case EXPR_ATOMIC:
        return !fail_const(c, e, "a call");
    case EXPR_FIELD:
        return sema_refuse_runtime_parts(c, e->as.field.base);
    case EXPR_UNARY:
        return sema_refuse_runtime_parts(c, e->as.unary.operand);
    case EXPR_BINARY:
        if (sema_refuse_runtime_parts(c, e->as.binary.left)) {
            return true;
        }
        if (e->as.binary.op == TOKEN_QUESTION_QUESTION) {
            return !fail_const(c, e, "`??`");
        }
        return sema_refuse_runtime_parts(c, e->as.binary.right);
    case EXPR_CAST:
        return sema_refuse_runtime_parts(c, e->as.cast.operand);
    case EXPR_INDEX:
        return sema_refuse_runtime_parts(c, e->as.index.base) ||
               sema_refuse_runtime_parts(c, e->as.index.index);
    case EXPR_ARRAY_LIT:
        for (i = 0; i < e->as.array_lit.count; i++) {
            if (sema_refuse_runtime_parts(c, e->as.array_lit.elements[i])) {
                return true;
            }
        }
        return false;
    case EXPR_TUPLE:
        for (i = 0; i < e->as.tuple.count; i++) {
            if (sema_refuse_runtime_parts(c, e->as.tuple.elements[i])) {
                return true;
            }
        }
        return false;
    case EXPR_ARRAY_REPEAT:
        return sema_refuse_runtime_parts(c, e->as.array_repeat.value);
    case EXPR_STRUCT_LIT:
        for (i = 0; i < e->as.struct_lit.field_count; i++) {
            if (sema_refuse_runtime_parts(c,
                                          e->as.struct_lit.fields[i].value)) {
                return true;
            }
        }
        return false;
    default:
        return false;
    }
}

/* Enter the context the value of sym is checked in. A constant of the
   module or of a class takes that of its declaration, wherever it is
   first named, and a local constant stays in the body it stands in. */
static void enter_const(struct checker *c, const struct symbol *sym,
                        struct context *saved)
{
    struct context at =
        sym->item != NULL ? sema_declaration_context(c, sema_within(sym->item))
                          : c->ctx;

    sema_enter(c, &at, saved);
}

/* Add the constants the value of sym names to d, in the context the
   value is checked in. */
static void const_deps_of_symbol(struct chain_deps *d, struct symbol *sym)
{
    struct context saved;

    enter_const(d->c, sym, &saved);
    const_deps_of(d, sym->item != NULL ? sym->item->value
                                       : sym->stmt->as.let.value);
    sema_leave(d->c, &saved);
}

static bool is_class(const struct type *t)
{
    return t->kind == TYPE_CLASS;
}

bool sema_holds_class(const struct type *t)
{
    return types_holds(t, is_class);
}

/* Evaluate a constant that a chain names. */
static void const_resolve(struct checker *c, struct symbol *sym)
{
    sema_const_symbol(c, sym, sym->pos);
}

/* Give a constant symbol its type and value, once. */
bool sema_const_symbol(struct checker *c, struct symbol *sym,
                       struct pos use)
{
    struct type_expr *type_expr;
    struct expr *value;
    struct context saved;
    struct type *t;
    bool ok;

    if (sym->state == EVAL_DONE) {
        return sym->value != NULL;
    }
    if (sym->state == EVAL_BUSY) {
        sema_error_at(c, use, "`%.*s` depends on itself", (int)sym->name.length,
                      sym->name.text);
        return false;
    }
    if (c->const_depth == 0) {
        c->const_depth++;
        sema_chain_prepare(c, sym, const_deps_of_symbol, const_resolve);
        c->const_depth--;
        if (sym->state == EVAL_DONE) {
            return sym->value != NULL;
        }
    }
    /* The refusal is kept, so it is reported in the context of the
       constant, which a quiet probe does not silence. */
    if (c->const_depth >= CONST_DEPTH_MAX) {
        enter_const(c, sym, &saved);
        sema_error_at(c, use, "`%.*s` needs a chain of more than %d constants",
                      (int)sym->name.length, sym->name.text, CONST_DEPTH_MAX);
        sema_leave(c, &saved);
        sym->type = sema_builtin(c, TYPE_ERROR);
        sym->state = EVAL_DONE;
        return false;
    }
    c->const_depth++;
    sym->state = EVAL_BUSY;
    if (sym->item != NULL) {
        type_expr = sym->item->type;
        value = sym->item->value;
    } else {
        type_expr = sym->stmt->as.let.type;
        value = sym->stmt->as.let.value;
    }
    enter_const(c, sym, &saved);
    t = sema_resolve_type(c, type_expr);
    ok = !sema_is_error(t);
    if (ok && sym->item != NULL && sym->item->is_static &&
        sym->item->atomic) {
        sema_refuse_atomic_field(c, sym->item->name_pos, t);
    }
    /* DESIGN: a class holds its base, its tables and the defaults of
       its fields, which the value of a constant does not carry. A
       constant that holds a class is refused. A default of a field
       still takes a class literal, which lowering writes from the
       expression. */
    if (ok && sema_holds_class(t)) {
        sema_error_at(c, value->pos, "a class is not a constant expression");
        ok = false;
    }
    /* DESIGN: a constant is a value of the program text, which each use
       reads as a literal, and a Mutex is a place that threads share. By
       R047 of "Rules of 2026-10-13" a constant is no Mutex and holds
       none. */
    if (ok && sema_holds_mutex(t)) {
        if (types_is_mutex(t)) {
            sema_error_at(c, value->pos, "a `" LANG_MUTEX "` cannot be a "
                          "constant");
        } else {
            sema_error_at(c, value->pos, "`%s` holds a `" LANG_MUTEX "` and "
                          "cannot be a constant", sema_tn(t));
        }
        ok = false;
    }
    ok = ok && (c->fns_typed || !sema_refuse_runtime_parts(c, value)) &&
         sema_require(c, value, sema_check_expr(c, value, t), t);
    if (ok) {
        struct const_value *made = arena_alloc(c->arena, sizeof *made);
        ok = sema_eval_const(c, value, made);
        sym->value = ok ? made : NULL;
    }
    sym->type = ok ? t : sema_builtin(c, TYPE_ERROR);
    sym->state = EVAL_DONE;
    c->const_depth--;
    sema_leave(c, &saved);
    return ok;
}

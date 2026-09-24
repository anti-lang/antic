/* The evaluation of constant expressions, and the order in which the
   constants of a module are evaluated. */

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
    int bits = type_is_target_sized(v->type) ? 64 : type_bits(v->type);

    if (bits < 64) {
        uint64_t mask = ((uint64_t)1 << bits) - 1;
        v->as.integer &= mask;
        if (type_is_signed(v->type) && (v->as.integer >> (bits - 1)) != 0) {
            v->as.integer |= ~mask;
        }
    }
}

/* Report a constant of a target-sized type whose value differs between
   targets. */
static bool fits_every_target(struct checker *c, const struct expr *e,
                              const struct const_value *v)
{
    int bits = type_bits(v->type);
    bool fits;

    if (v->kind != CONST_INT || !type_is_target_sized(v->type)) {
        return true;
    }
    fits = type_is_signed(v->type)
               ? sema_signed_bits(v->as.integer) >=
                         -((int64_t)1 << (bits - 1)) &&
                     sema_signed_bits(v->as.integer) <
                         ((int64_t)1 << (bits - 1))
               : v->as.integer < ((uint64_t)1 << bits);
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
   holds after truncation toward zero. */
static bool float_fits(double x, const struct type *t)
{
    int bits = type_bits(t);
    double lo = type_is_signed(t) ? -ldexp(1.0, bits - 1) : 0.0;
    double hi = type_is_signed(t) ? ldexp(1.0, bits - 1) : ldexp(1.0, bits);

    return x > lo - 1.0 && x < hi;
}

/* Append the float x as an Anti float literal. It has the fewest
   significant digits that read back as x, and digits on both sides of the
   dot. An exponent follows outside 1e-5 to 1e21. NaN and the infinities
   have no literal and print as nan and inf. */
static void float_as_literal(struct text *out, double x, bool single)
{
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
        symbolic_print(out, v->as.symbolic, false);
    } else if (v->kind == CONST_FLOAT) {
        float_as_literal(out, v->as.floating, v->type->kind == TYPE_F32);
    } else if (type_is_signed(v->type)) {
        text_appendf(out, "%lld", (long long)v->as.integer);
    } else {
        text_appendf(out, "%llu", (unsigned long long)v->as.integer);
    }
}

/* Report an operation on the constant operands a and b that has no value.
   Integer division and remainder fail by zero and for the minimum value by
   -1. A shift fails for a count outside the bits of the type, and a float
   conversion outside the range of the integer type. e is a binary
   expression or a conversion to result, and b is NULL for a conversion. */
static bool reports_undefined(struct checker *c, const struct expr *e,
                              const struct type *result,
                              const struct const_value *a,
                              const struct const_value *b)
{
    struct text x = {0};
    struct text y = {0};
    char spelling[OP_TEXT];
    bool found = false;

    if (e->kind == EXPR_CAST) {
        if (a->kind == CONST_FLOAT && type_is_integer(result) &&
            !float_fits(a->as.floating, result)) {
            operand_text(&x, e->as.cast.operand, a);
            sema_error_at(c, e->pos, "`%s as %s` does not fit `%s`",
                          text_cstr(&x), sema_tn(result), sema_tn(result));
            found = true;
        }
    } else if (type_is_integer(e->as.binary.left->type) &&
               b->kind != CONST_SYMBOLIC) {
        enum token_kind op = e->as.binary.op;
        const struct type *t = e->as.binary.left->type;
        int bits = type_bits(t);
        uint64_t mask = bits == 64 ? UINT64_MAX : ((uint64_t)1 << bits) - 1;
        bool divides = op == TOKEN_SLASH || op == TOKEN_PERCENT;
        const char *o = sema_op_text(op, spelling);

        operand_text(&x, e->as.binary.left, a);
        operand_text(&y, e->as.binary.right, b);
        if (divides && b->as.integer == 0) {
            sema_error_at(c, e->pos, "`%s %s %s` divides by zero",
                          text_cstr(&x), o,
                          text_cstr(&y));
            found = true;
        } else if (divides && a->kind != CONST_SYMBOLIC && type_is_signed(t) &&
                   (a->as.integer & mask) == (uint64_t)1 << (bits - 1) &&
                   sema_signed_bits(b->as.integer) == -1) {
            sema_error_at(c, e->pos, "`%s %s %s` does not fit `%s`",
                          text_cstr(&x),
                          o, text_cstr(&y), sema_tn(t));
            found = true;
        } else if ((op == TOKEN_SHL || op == TOKEN_SHR) &&
                   ((type_is_signed(t) &&
                     sema_signed_bits(b->as.integer) < 0) ||
                    b->as.integer >= (uint64_t)bits)) {
            sema_error_at(c, e->pos, "`%s %s %s` shifts out of range",
                          text_cstr(&x), o, text_cstr(&y));
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
    bool constant;

    c->quiet++;
    if (e->kind == EXPR_CAST) {
        constant = sema_eval_const(c, e->as.cast.operand, &a);
    } else {
        constant = sema_eval_const(c, e->as.binary.left, &a) &&
                   sema_eval_const(c, e->as.binary.right, &b);
    }
    c->quiet--;
    return constant &&
           reports_undefined(c, e, result, &a,
                             e->kind == EXPR_CAST ? NULL : &b);
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
    if (!type_has_fields(t) || t->kind == TYPE_CLASS) {
        return 1;
    }
    for (i = 0; i < t->field_count; i++) {
        one = const_elements(t->fields[i].type);
        total = one > UINT64_MAX - total ? UINT64_MAX : total + one;
    }
    return total;
}

/* Evaluate a checked expression. The expression must use only what
   chapter 2 allows in a constant. */
bool sema_eval_const(struct checker *c, struct expr *e,
                     struct const_value *out)
{
    struct const_value a;
    struct const_value b;
    size_t i;

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
        if (!sema_eval_const(c, e->as.cast.operand, &a)) {
            return false;
        }
        out->type = e->type;
        if (a.kind == CONST_SYMBOLIC) {
            if (!type_is_integer(e->type)) {
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
        } else if (type_is_float(e->type)) {
            out->kind = CONST_FLOAT;
            out->as.floating = a.kind == CONST_FLOAT ? a.as.floating
                               : type_is_signed(a.type)
                                   ? (double)sema_signed_bits(a.as.integer)
                                   : (double)a.as.integer;
            if (e->type->kind == TYPE_F32) {
                out->as.floating = (float)out->as.floating;
            }
        } else if (type_is_integer(e->type)) {
            out->kind = CONST_INT;
            if (a.kind == CONST_FLOAT) {
                if (reports_undefined(c, e, e->type, &a, NULL)) {
                    return false;
                }
                out->as.integer = type_is_signed(e->type)
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
        } else {
            return fail_const(c, e, "a pointer conversion");
        }
        return fits_every_target(c, e, out);
    case EXPR_UNARY:
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
    case EXPR_BINARY: {
        enum token_kind op = e->as.binary.op;
        struct type *operand = e->as.binary.left->type;
        bool is_float;
        bool is_signed = type_is_signed(operand);
        int bits = type_bits(operand);

        if (!sema_eval_const(c, e->as.binary.left, &a)) {
            return false;
        }
        /* A constant pointer is `none`, so `??` gives its right side. */
        if (op == TOKEN_QUESTION_QUESTION) {
            return a.kind == CONST_NULL
                       ? sema_eval_const(c, e->as.binary.right, out)
                       : fail_const(c, e, "`??`");
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
        if (reports_undefined(c, e, e->type, &a, &b)) {
            return false;
        }
        /* The upper half of a product of c_long has another value at
           each width, and no symbolic value carries the signedness of
           c_wchar. */
        if (op == TOKEN_MUL_HIGH && type_is_target_sized(operand)) {
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
            int cmp;
            out->kind = CONST_BOOL;
            if (is_float) {
                cmp = a.as.floating < b.as.floating ? -1
                      : a.as.floating > b.as.floating ? 1 : 0;
            } else if (a.kind == CONST_NULL || b.kind == CONST_NULL) {
                cmp = a.kind == b.kind ? 0 : 1;
            } else if (is_signed) {
                cmp = sema_signed_bits(a.as.integer) <
                              sema_signed_bits(b.as.integer)
                          ? -1
                      : sema_signed_bits(a.as.integer) >
                              sema_signed_bits(b.as.integer)
                          ? 1
                          : 0;
            } else {
                uint64_t x = a.kind == CONST_CHAR ? a.as.character
                             : a.kind == CONST_BOOL ? a.as.boolean
                                                    : a.as.integer;
                uint64_t y = b.kind == CONST_CHAR ? b.as.character
                             : b.kind == CONST_BOOL ? b.as.boolean
                                                    : b.as.integer;
                cmp = x < y ? -1 : x > y ? 1 : 0;
            }
            out->as.boolean = op == TOKEN_EQ ? cmp == 0
                              : op == TOKEN_NE ? cmp != 0
                              : op == TOKEN_LT ? cmp < 0
                              : op == TOKEN_LE ? cmp <= 0
                              : op == TOKEN_GT ? cmp > 0
                                               : cmp >= 0;
            if (is_float && (isnan(a.as.floating) || isnan(b.as.floating))) {
                out->as.boolean = op == TOKEN_NE;
            }
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
        switch (op) {
        case TOKEN_PLUS:
        case TOKEN_PLUS_WRAP:
            out->as.integer = a.as.integer + b.as.integer;
            break;
        case TOKEN_MINUS:
        case TOKEN_MINUS_WRAP:
            out->as.integer = a.as.integer - b.as.integer;
            break;
        case TOKEN_STAR:
        case TOKEN_STAR_WRAP:
            out->as.integer = a.as.integer * b.as.integer;
            break;
        /* A count at or above the width gives 0, and a negative count
           holds its sign in the bits above, which makes it one. A
           target-sized type computes at 64 bits and must fit the narrower
           width, as every constant of it does. */
        case TOKEN_SHL_WRAP:
            out->as.integer =
                b.as.integer >= (uint64_t)(type_is_target_sized(operand)
                                               ? 64
                                               : bits)
                    ? 0
                    : a.as.integer << b.as.integer;
            break;
        case TOKEN_PLUS_SAT:
        case TOKEN_MINUS_SAT:
        case TOKEN_STAR_SAT:
            out->as.integer = arith_saturate(
                op == TOKEN_PLUS_SAT ? '+' : op == TOKEN_MINUS_SAT ? '-' : '*',
                a.as.integer, b.as.integer,
                type_is_target_sized(operand) ? 64 : bits, is_signed);
            break;
        case TOKEN_MUL_HIGH:
            out->as.integer =
                arith_mul_high(a.as.integer, b.as.integer, bits, is_signed);
            break;
        case TOKEN_AMP: out->as.integer = a.as.integer & b.as.integer; break;
        case TOKEN_PIPE: out->as.integer = a.as.integer | b.as.integer; break;
        case TOKEN_CARET: out->as.integer = a.as.integer ^ b.as.integer; break;
        case TOKEN_SLASH:
        case TOKEN_PERCENT: {
            if (is_signed) {
                int64_t x = sema_signed_bits(a.as.integer);
                int64_t y = sema_signed_bits(b.as.integer);
                out->as.integer = (uint64_t)(op == TOKEN_SLASH ? x / y : x % y);
            } else {
                out->as.integer = op == TOKEN_SLASH ? a.as.integer / b.as.integer
                                                    : a.as.integer % b.as.integer;
            }
            break;
        }
        case TOKEN_SHL:
        case TOKEN_SHR:
            if (op == TOKEN_SHL) {
                out->as.integer = a.as.integer << b.as.integer;
            } else if (is_signed) {
                /* An arithmetic shift in unsigned operations, since C
                   leaves the shift of a negative value to the
                   implementation. */
                out->as.integer = (a.as.integer >> 63) != 0
                                      ? ~(~a.as.integer >> b.as.integer)
                                      : a.as.integer >> b.as.integer;
            } else {
                uint64_t mask = bits == 64 ? UINT64_MAX
                                           : ((uint64_t)1 << bits) - 1;
                out->as.integer = (a.as.integer & mask) >> b.as.integer;
            }
            break;
        default:
            break;
        }
        wrap(out);
        return fits_every_target(c, e, out);
    }
    case EXPR_ARRAY_LIT:
        out->kind = CONST_ARRAY;
        out->as.aggregate.count = e->as.array_lit.count;
        out->as.aggregate.items = arena_alloc(
            c->arena, e->as.array_lit.count * sizeof *out->as.aggregate.items);
        for (i = 0; i < e->as.array_lit.count; i++) {
            if (!sema_eval_const(c, e->as.array_lit.elements[i],
                                 &out->as.aggregate.items[i])) {
                return false;
            }
        }
        return true;
    /* A tuple is a struct, so a constant one is the constant struct of
       its elements. */
    case EXPR_TUPLE:
        out->kind = CONST_STRUCT;
        out->as.aggregate.count = e->as.tuple.count;
        out->as.aggregate.items = arena_alloc(
            c->arena, e->as.tuple.count * sizeof *out->as.aggregate.items);
        for (i = 0; i < e->as.tuple.count; i++) {
            if (!sema_eval_const(c, e->as.tuple.elements[i],
                                 &out->as.aggregate.items[i])) {
                return false;
            }
        }
        return true;
    case EXPR_ARRAY_REPEAT:
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
        out->as.aggregate.items = arena_alloc(
            c->arena, e->type->length * sizeof *out->as.aggregate.items);
        for (i = 0; i < e->type->length; i++) {
            out->as.aggregate.items[i] = a;
        }
        return true;
    case EXPR_STRUCT_LIT: {
        struct type *s = e->type;
        size_t j;
        if (s->is_union) {
            return fail_const(c, e, "a union");
        }
        if (s->kind == TYPE_VARIANT) {
            return fail_const(c, e, "a variant");
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
                                   &e->as.struct_lit.fields[i].name)) {
                    if (!sema_eval_const(c, e->as.struct_lit.fields[i].value,
                                         &out->as.aggregate.items[j])) {
                        return false;
                    }
                }
            }
        }
        return true;
    }
    case EXPR_FIELD: {
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
        base = e->as.field.base->type;
        if (base == NULL) {
            return fail_const(c, e, "this field");
        }
        /* The value of a class literal keeps the filler above in its
           base, its tables and the fields it does not name. A field of
           it is therefore read only at run time. */
        if (base->kind == TYPE_CLASS) {
            return fail_const(c, e, "a field of a class");
        }
        if (type_has_fields(base)) {
            if (!sema_eval_const(c, e->as.field.base, &a)) {
                return false;
            }
            f = sema_find_field(base, &e->as.field.name);
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
    case EXPR_INDEX:
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
        if (b.as.integer >= a.as.aggregate.count) {
            sema_error_at(c, e->as.index.index->pos,
                          "the index %lld is outside the "
                          "constant array", (long long)b.as.integer);
            return false;
        }
        *out = a.as.aggregate.items[b.as.integer];
        return true;
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
    /* The two comparisons of numbers fold, with the value in both. An
       `lt` operator is a call and never a constant. */
    case EXPR_IN: {
        struct expr *sides[3];
        const struct type *t = e->as.in.value->type;
        if (!type_is_numeric(t) && t->kind != TYPE_CHAR) {
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
    /* The field or the call reads through a pointer. */
    case EXPR_OPTIONAL:
        return fail_const(c, e, "`?.`");
    }
    return false;
}

/* DESIGN: a constant that names another evaluates that one first, and
   the checker recursed once per link of a chain. A chain deeper than
   CONST_CHAIN_DIRECT links is taken apart first. A walk with a stack of
   its own finds the constants the first one depends on. Each is then
   evaluated after the ones it names, so no evaluation goes more than one
   link deep. A shallower chain is evaluated as it is met, which keeps
   the order of the messages. CONST_DEPTH_MAX bounds what is left: a
   cycle through a long chain, or a form the walk does not follow. */
#define CONST_CHAIN_DIRECT 16

#define CONST_DEPTH_MAX 64

/* The constants that expressions name, collected on one stack. */
struct const_deps {
    struct checker *c;
    struct symbol **items;
    size_t count;
    size_t capacity;
};

static void const_deps_add(struct const_deps *d, struct symbol *sym)
{
    if (sym == NULL || sym->kind != SYMBOL_CONST || sym->state != EVAL_NONE) {
        return;
    }
    if (d->count == d->capacity) {
        size_t capacity = d->capacity == 0 ? 64 : d->capacity * 2;
        struct symbol **items =
            capacity <= SIZE_MAX / sizeof *items
                ? realloc(d->items, capacity * sizeof *items)
                : NULL;
        if (items == NULL) {
            fputs("antic: out of memory\n", stderr);
            exit(70);
        }
        d->items = items;
        d->capacity = capacity;
    }
    d->items[d->count++] = sym;
}

/* Collect the constants e names that have no value yet. The walk
   follows the forms of a constant expression. */
static void const_deps_of(struct const_deps *d, const struct expr *e)
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

/* Add the constants the value of sym names to d, in the scope the value
   is checked in. */
static void const_deps_of_symbol(struct const_deps *d, struct symbol *sym)
{
    struct scope *saved = d->c->scope;
    const struct item *within = d->c->within;

    if (sym->item != NULL) {
        d->c->scope = &d->c->module_scope;
        d->c->within = sema_within(sym->item);
        const_deps_of(d, sym->item->value);
    } else {
        const_deps_of(d, sym->stmt->as.let.value);
    }
    d->c->scope = saved;
    d->c->within = within;
}

/* Whether a value of type t holds a class, itself or in a field or an
   element. */
static bool holds_class(const struct type *t)
{
    size_t i;

    if (t == NULL) {
        return false;
    }
    if (t->kind == TYPE_CLASS) {
        return true;
    }
    if (t->kind == TYPE_ARRAY) {
        return holds_class(t->element);
    }
    if (!type_has_fields(t)) {
        return false;
    }
    for (i = 0; i < t->field_count; i++) {
        if (holds_class(t->fields[i].type)) {
            return true;
        }
    }
    return false;
}

/* One constant on the stack of const_prepare, and its range of deps. */
struct const_frame {
    struct symbol *sym;
    size_t start;
    size_t next;
    size_t end;
};

/* Evaluate the constants root depends on, each after the ones it names,
   when the chain below root is deeper than CONST_CHAIN_DIRECT. */
static void const_prepare(struct checker *c, struct symbol *root)
{
    struct const_deps deps;
    struct ptr_set seen;
    struct const_frame *stack = NULL;
    size_t stack_count = 0;
    size_t stack_capacity = 0;
    size_t deepest = 0;
    struct symbol **order = NULL;
    size_t order_count = 0;
    size_t order_capacity = 0;
    struct symbol *next = root;
    size_t i;

    memset(&deps, 0, sizeof deps);
    memset(&seen, 0, sizeof seen);
    deps.c = c;
    sema_ptr_set_add(&seen, root);
    for (;;) {
        struct const_frame *top;
        if (next != NULL) {
            if (stack_count == stack_capacity) {
                size_t capacity = stack_capacity == 0 ? 64 : stack_capacity * 2;
                struct const_frame *grown =
                    capacity <= SIZE_MAX / sizeof *grown
                        ? realloc(stack, capacity * sizeof *grown)
                        : NULL;
                if (grown == NULL) {
                    fputs("antic: out of memory\n", stderr);
                    exit(70);
                }
                stack = grown;
                stack_capacity = capacity;
            }
            top = &stack[stack_count++];
            top->sym = next;
            top->start = deps.count;
            const_deps_of_symbol(&deps, next);
            top->next = top->start;
            top->end = deps.count;
            deepest = stack_count > deepest ? stack_count : deepest;
            next = NULL;
            continue;
        }
        if (stack_count == 0) {
            break;
        }
        top = &stack[stack_count - 1];
        if (top->next < top->end) {
            struct symbol *dep = deps.items[top->next++];
            if (dep->state == EVAL_NONE && sema_ptr_set_add(&seen, dep)) {
                next = dep;
            }
            continue;
        }
        if (order_count == order_capacity) {
            size_t capacity = order_capacity == 0 ? 64 : order_capacity * 2;
            struct symbol **grown =
                capacity <= SIZE_MAX / sizeof *grown
                    ? realloc(order, capacity * sizeof *grown)
                    : NULL;
            if (grown == NULL) {
                fputs("antic: out of memory\n", stderr);
                exit(70);
            }
            order = grown;
            order_capacity = capacity;
        }
        order[order_count++] = top->sym;
        deps.count = top->start;
        stack_count--;
    }
    /* The last in the order is root, which the caller evaluates. */
    if (deepest > CONST_CHAIN_DIRECT) {
        for (i = 0; i + 1 < order_count; i++) {
            if (order[i]->state == EVAL_NONE) {
                sema_const_symbol(c, order[i], order[i]->pos);
            }
        }
    }
    free(order);
    free(stack);
    free(deps.items);
    free(seen.slots);
}

/* Give a constant symbol its type and value, once. */
bool sema_const_symbol(struct checker *c, struct symbol *sym,
                       struct pos use)
{
    struct type_expr *type_expr;
    struct expr *value;
    struct scope *saved = c->scope;
    const struct item *within = c->within;
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
        const_prepare(c, sym);
        c->const_depth--;
        if (sym->state == EVAL_DONE) {
            return sym->value != NULL;
        }
    }
    if (c->const_depth >= CONST_DEPTH_MAX) {
        sema_error_at(c, use, "`%.*s` needs a chain of more than %d constants",
                      (int)sym->name.length, sym->name.text, CONST_DEPTH_MAX);
        sym->type = sema_builtin(c, TYPE_ERROR);
        sym->state = EVAL_DONE;
        return false;
    }
    c->const_depth++;
    sym->state = EVAL_BUSY;
    if (sym->item != NULL) {
        type_expr = sym->item->type;
        value = sym->item->value;
        c->scope = &c->module_scope;
        c->within = sema_within(sym->item);
    } else {
        type_expr = sym->stmt->as.let.type;
        value = sym->stmt->as.let.value;
    }
    t = sema_resolve_type(c, type_expr);
    ok = !sema_is_error(t);
    /* DESIGN: a class holds its base, its tables and the defaults of
       its fields, which the value of a constant does not carry. A
       constant that holds a class is refused. A default of a field
       still takes a class literal, which lowering writes from the
       expression. */
    if (ok && holds_class(t)) {
        sema_error_at(c, value->pos, "a class is not a constant expression");
        ok = false;
    }
    ok = ok && sema_require(c, value, sema_check_expr(c, value, t), t);
    if (ok) {
        sym->value = arena_alloc(c->arena, sizeof *sym->value);
        ok = sema_eval_const(c, value, sym->value);
        if (!ok) {
            sym->value = NULL;
        }
    }
    sym->type = ok ? t : sema_builtin(c, TYPE_ERROR);
    sym->state = EVAL_DONE;
    c->const_depth--;
    c->scope = saved;
    c->within = within;
    return ok;
}

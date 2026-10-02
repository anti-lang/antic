/* The checks of simd structs: the operators lane by lane, `as` between
   a simd struct and an array or a struct of the same layout, and the
   built-ins of `anti.simd`, of a simd struct and of its values. */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "sema_checker.h"

/* Whether the lanes of type lane are numbers, which `+ - * /` take. An
   f16 lane is one, since each operation reads it as an f32. */
bool sema_simd_numeric(const struct type *lane)
{
    return types_is_numeric(lane) || lane->kind == TYPE_F16;
}

/* Unary `-` and `~` on a simd struct apply lane by lane, with the lanes
   that each takes on one value. */
struct type *sema_check_simd_unary(struct checker *c, struct expr *e,
                                   struct type *t)
{
    struct type *lane = types_simd_lane(t);

    if (e->as.unary.op == TOKEN_MINUS && !types_is_signed(lane) &&
        !types_is_float(lane) && lane->kind != TYPE_F16) {
        sema_error_at(c, e->pos, "unary `-` needs lanes of signed integers or "
                      "floats, and `%s` has `%s`", sema_tn(t), sema_tn(lane));
        return sema_builtin(c, TYPE_ERROR);
    }
    if (e->as.unary.op == TOKEN_TILDE && !types_is_integer(lane)) {
        sema_error_at(c, e->pos,
                      "unary `~` needs integer lanes, and `%s` has `%s`",
                      sema_tn(t), sema_tn(lane));
        return sema_builtin(c, TYPE_ERROR);
    }
    return t;
}

/* DESIGN: an operator on two values of one simd struct applies lane by
   lane. `+ - * /` take numbers, the bitwise operators take integers, and
   a comparison takes the lanes it takes on one value and gives the mask.
   `%`, the wrapping and saturating operators and the logic operators
   are not among the operators of the section, so they are refused. */
struct type *sema_check_simd_binary(struct checker *c, struct expr *e,
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
    lane = types_simd_lane(left);
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
        if (!types_is_integer(lane)) {
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

/* What fixed_layout found for one struct or tuple. */
struct fixed_answer {
    bool fixed;
    uint64_t size;
    uint64_t align;
};

static bool layout_in(const struct type *t, uint64_t *size, uint64_t *align,
                      struct ptr_map *answered);

/* The size and the alignment of the fields of a struct or a tuple. */
static bool fields_layout(const struct type *t, uint64_t *size,
                          uint64_t *align, struct ptr_map *answered)
{
    uint64_t offset = 0;
    uint64_t most = 1;
    size_t i;

    for (i = 0; i < t->field_count; i++) {
        uint64_t n;
        uint64_t a;
        /* A unit break `_` takes no bytes and aligns what follows by
           the rules of each target, so it has no one layout (S02 of the
           audit). */
        if (t->fields[i].bits != 0 ||
            types_field_is_unit_break(&t->fields[i]) ||
            !layout_in(t->fields[i].type, &n, &a, answered)) {
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
}

static bool layout_in(const struct type *t, uint64_t *size, uint64_t *align,
                      struct ptr_map *answered)
{
    struct fixed_answer *known;

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
        return layout_in(t->base, size, align, answered);
    case TYPE_ARRAY:
        if (t->length_of != NULL ||
            !layout_in(t->element, size, align, answered) ||
            (t->length != 0 && *size > UINT64_MAX / t->length)) {
            return false;
        }
        *size *= t->length;
        return true;
    case TYPE_STRUCT:
    case TYPE_TUPLE:
        known = ptr_map_get(answered, t);
        if (known == NULL) {
            known = alloc_zeroed(1, sizeof *known);
            known->fixed = fields_layout(t, &known->size, &known->align,
                                         answered);
            ptr_map_put(answered, t, known);
        }
        *size = known->size;
        *align = known->align;
        return known->fixed;
    default:
        /* The word of a Mutex is as wide as the lock of the target, and
           a Mutex is never made of bytes (S02 of the audit). */
        if (t->lock_word) {
            return false;
        }
        *size = types_lane_bytes(t);
        *align = *size;
        return *size != 0;
    }
}

/* The size and the alignment of t, which are the same on every target,
   as the C rules lay it out. False for a type that holds a width the
   target decides, a symbolic length, a bitfield, a unit break, the word
   of a Mutex or a table. answered
   holds the answer for each struct and tuple met, so a struct that holds
   another twice measures it once. */
static bool fixed_layout(const struct type *t, uint64_t *size,
                         uint64_t *align)
{
    struct ptr_map answered;
    bool fixed;
    size_t i;

    memset(&answered, 0, sizeof answered);
    fixed = layout_in(t, size, align, &answered);
    for (i = 0; i < answered.capacity; i++) {
        free(answered.values[i]);
    }
    ptr_map_free(&answered);
    return fixed;
}

/* DESIGN: `as` between a simd struct and an array or a plain struct of
   the same bytes is free both ways, since it copies the bytes. The two
   have one size on every target, which the checker computes by the C
   rules. Two simd structs do not convert into each other, and neither
   does a class, a union or a variant, which are no plain structs. */
struct type *sema_check_simd_cast(struct checker *c, struct expr *e,
                                  struct type *from, struct type *to)
{
    struct type *other = types_is_simd(from) ? to : from;
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

/* Rewrite the call e into the built-in op of the simd struct simd with
   the operands args. None of the built-ins can fail, so a handler on one
   is refused. */
static bool simd_node(struct checker *c, struct expr *e, enum simd_op op,
                      struct expr **args, size_t count,
                      struct type *simd)
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
bool sema_simd_module_call(const struct checker *c, const struct expr *e)
{
    const struct expr *callee = e->as.call.callee;
    const struct symbol *module;

    if (callee->kind != EXPR_FIELD ||
        (module = sema_qualifier(c, callee)) == NULL || module->home == NULL ||
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
struct type *sema_check_simd_module(struct checker *c, struct expr *e)
{
    const struct name *name = &e->as.call.callee->as.field.name;
    struct expr **args = e->as.call.args;
    size_t count = e->as.call.arg_count;
    bool select = sema_name_is(name, SIMD_SELECT);
    struct type *mask;
    struct type *a;
    struct type *b;

    if (count != (select ? 3u : 1u)) {
        sema_error_at(c, e->pos, "`simd.%.*s` takes %d argument%s, found %zu",
                      (int)name->length, name->text, select ? 3 : 1,
                      select ? "s" : "", count);
        return sema_builtin(c, TYPE_ERROR);
    }
    mask = sema_check_expr(c, args[0], NULL);
    if (sema_is_error(mask)) {
        return mask;
    }
    if (!types_is_mask(mask)) {
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
    if (!types_is_simd(a)) {
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
bool sema_simd_static_name(const struct name *name)
{
    return sema_name_is(name, SIMD_SPLAT) || sema_name_is(name, SIMD_LOAD);
}

/* `T.splat(v)` writes v into every lane, and `T.load(slice, i)` reads
   the lanes from the elements of slice from i on. */
struct type *sema_check_simd_static(struct checker *c, struct expr *e,
                                    struct type *t)
{
    const struct name *name = &e->as.call.callee->as.field.name;
    struct expr **args = e->as.call.args;
    size_t count = e->as.call.arg_count;
    struct type *lane = types_simd_lane(t);
    struct type *i64 = sema_builtin(c, TYPE_I64);

    if (sema_name_is(name, SIMD_SPLAT)) {
        if (count != 1) {
            sema_error_at(c, e->pos,
                          "`%s." SIMD_SPLAT "` takes 1 argument, found "
                          "%zu", sema_tn(t), count);
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
                      "`%s." SIMD_LOAD "` takes 2 arguments, found %zu",
                      sema_tn(t), count);
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
bool sema_simd_value_name(const struct name *name)
{
    return sema_name_is(name, SIMD_STORE) || sema_name_is(name, SIMD_SHUFFLE) ||
           sema_name_is(name, SIMD_SUM) || sema_name_is(name, SIMD_MIN) ||
           sema_name_is(name, SIMD_MAX) || sema_name_is(name, SIMD_DOT);
}

/* The built-ins on a value v of the simd struct s, or on a pointer to
   one: `v.store(slice, i)`, `v.shuffle(i, ...)`, `v.sum()`, `v.min()`,
   `v.max()` and `a.dot(b)`. A shuffle names the lane of v that each lane
   of its result takes, by a constant index. */
struct type *sema_check_simd_value(struct checker *c, struct expr *e,
                                   struct type *base, struct type *s)
{
    struct expr *receiver = e->as.call.callee->as.field.base;
    const struct name *name = &e->as.call.callee->as.field.name;
    size_t count = e->as.call.arg_count;
    struct type *lane = types_simd_lane(s);
    struct type *i64 = sema_builtin(c, TYPE_I64);
    struct expr **args = types_alloc_array(c->arena, count + 1, sizeof *args);
    enum simd_op op;
    size_t want;
    size_t i;

    if (base->kind == TYPE_POINTER) {
        sema_usable_pointer(c, receiver, base);
    }
    args[0] = receiver;
    /* memcpy takes no null pointer, even for no bytes, and a call with no
       arguments holds none. */
    if (count > 0) {
        memcpy(args + 1, e->as.call.args, count * sizeof *args);
    }
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
        sema_error_at(c, e->pos, "`%.*s` takes %zu argument%s, found %zu",
                      (int)name->length, name->text, want, want == 1 ? "" : "s",
                      count);
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
bool sema_simd_method_declared(struct checker *c, const struct type *s,
                               const struct name *name)
{
    struct symbol *f = sema_method_symbol(c, s, name);

    return f != NULL && (f->kind == SYMBOL_FN || f->kind == SYMBOL_EXTERN_FN) &&
           f->type != NULL && !sema_is_error(f->type) &&
           f->type->kind == TYPE_FN &&
           f->type->param_count > 0 &&
           sema_struct_of(f->type->params[0]) == s;
}

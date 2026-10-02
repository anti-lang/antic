/* The checks of the binary operators, `in` and `??` among them, of the
   operator table, and of the `operator fn` functions of a type that the
   operators and the language hooks call. */

#include <string.h>

#include "sema_checker.h"

/* Whether e is written `x.carry`, the form a carry into `+` and a borrow
   into `-` take when x is a Flags value. */
static bool names_carry(const struct expr *e)
{
    return e->kind == EXPR_FIELD && !e->as.field.optional &&
           sema_name_is(&e->as.field.name, FLAGS_CARRY);
}

/* Check both operands of a binary operator so that a literal takes the
   type of the other operand. outer is the type the context expects of
   the result, used when both operands are literals. */
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
            if (!types_is_nullable(*value_type) &&
                (*value_type)->kind != TYPE_POINTER &&
                (*value_type)->kind != TYPE_FN) {
                sema_error_at(c, none->pos, "`%s` cannot hold `none`",
                              sema_tn(*value_type));
                return false;
            }
            *none_type = sema_check_expr(
                c, none,
                types_is_nullable(*value_type)
                    ? *value_type
                    : types_with_none(c->types, *value_type));
            return !sema_is_error(*left) && !sema_is_error(*right);
        }
    }
    /* A literal beside an f16 takes no type from it, so the refusal of
       the f16 is the one message. A literal before a carry takes the type
       of the context, since the carry is a bool. */
    if (sema_is_untyped(l) && !sema_is_untyped(r) && !names_carry(r)) {
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
       with `none` above. `==` and `!=` of two `?T` values compare them
       whole, flag and value, as the default `==` of a `?T` does. */
    if ((e->as.binary.op == TOKEN_EQ || e->as.binary.op == TOKEN_NE) &&
        !sema_is_error(*left) && !sema_is_error(*right) &&
        (*left)->kind == TYPE_OPTIONAL && (*right)->kind == TYPE_OPTIONAL) {
        return true;
    }
    if (!sema_is_error(*left) && (*left)->kind == TYPE_OPTIONAL) {
        sema_error_may_be_none(c, l, *left);
        return false;
    }
    if (!sema_is_error(*right) && (*right)->kind == TYPE_OPTIONAL) {
        sema_error_may_be_none(c, r, *right);
        return false;
    }
    return !sema_is_error(*left) && !sema_is_error(*right);
}

const char *sema_op_text(enum token_kind op, char buffer[OP_TEXT])
{
    const char *quoted = lexer_token_kind_name(op);

    text_format(buffer, OP_TEXT, "%.*s", (int)(strlen(quoted) - 2),
                quoted + 1);
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

/* Whether the free function sym of the module of t takes t, or a
   pointer to it, first. A generic function of a generic struct takes
   each copy of it. */
static bool takes_first(const struct symbol *sym, const struct type *t)
{
    const struct type *first;

    if (sym->type == NULL || sym->type->kind != TYPE_FN ||
        sym->type->param_count == 0) {
        return false;
    }
    first = sym->type->params[0];
    if (first->kind == TYPE_POINTER) {
        first = first->element;
    }
    return first == t || (first->generic != NULL &&
                          first->generic == t->generic &&
                          sema_has_params(first));
}

/* DESIGN: an operator of a struct is a free function of its module, and
   one module declares one function of a name. The `operator fn eq` of
   one struct is therefore no operator of another struct of the module,
   which keeps its default `==`. */
static struct symbol *operator_symbol(struct checker *c, struct type *t,
                                      const char *text)
{
    struct name name;
    struct item *m;
    struct symbol *sym;

    if (t == NULL || !types_has_fields(t)) {
        return NULL;
    }
    name.text = text;
    name.length = strlen(text);
    m = sema_find_member(t, &name);
    if (m != NULL && m->kind == ITEM_FN && m->is_operator) {
        return m->symbol;
    }
    sym = sema_method_symbol(c, t, &name);
    if (sym != NULL && symbol_is_operator(sym) &&
        (m != NULL || takes_first(sym, t))) {
        return sym;
    }
    return NULL;
}

bool sema_module_operator(struct checker *c, struct type *t,
                          const char *text)
{
    struct name name;
    struct symbol *fn;

    if (operator_symbol(c, t, text) != NULL) {
        return true;
    }
    name.text = text;
    name.length = strlen(text);
    fn = sema_module_function(c, t, &name);
    return fn != NULL && fn->kind == SYMBOL_FN && symbol_is_operator(fn) &&
           takes_first(fn, t);
}

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

    if (t != NULL && t->kind == TYPE_POINTER && !t->nullable) {
        t = t->element;
    }
    if (t == NULL || !types_has_fields(t) || types_is_simd(t)) {
        return NULL;
    }
    name.text = text;
    name.length = strlen(text);
    m = sema_find_member(t, &name);
    if (m != NULL) {
        return m->kind == ITEM_FN && m->is_operator ? m->symbol : NULL;
    }
    sym = sema_method_symbol(c, t, &name);
    if (sym == NULL || !symbol_is_operator(sym) || !takes_first(sym, t)) {
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

/* Rewrite `a op b` into the call the operator names. `!=`, `>`, `<=` and
   `>=` derive from `eq` and `lt`, so a type declares two functions and
   gets six operators. */
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
    /* DESIGN: an operator that takes a pointer takes the address of
       either operand, as a function with `self` takes its receiver. A
       synchronized class hands its objects themselves to its operators
       that way, which lock them, since a copy would copy the lock. */
    if (sig->params[1]->kind == TYPE_POINTER &&
        right == sig->params[1]->element) {
        b = operator_receiver(c, b, sig->params[1]);
        right = sig->params[1];
    }
    if (!sema_require(c, b, right, sig->params[1])) {
        return sema_builtin(c, TYPE_ERROR);
    }
    sema_refuse_lock_copy(c, a, sig->params[0]);
    sema_refuse_lock_copy(c, b, sig->params[1]);
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

struct expr *sema_stand_in(struct checker *c, struct pos pos,
                           struct type *t)
{
    struct expr *hole = sema_new_node(c, EXPR_NONE, pos);
    struct expr *at = sema_new_node(c, EXPR_UNARY, pos);

    hole->type = types_pointer(c->types, t);
    hole->prechecked = true;
    at->as.unary.op = TOKEN_STAR;
    at->as.unary.operand = hole;
    at->type = t;
    at->prechecked = true;
    return at;
}

/* The checked call of the `operator fn text` that the module of the class
   t gives it, on count stand-ins, when its result has the kind result and
   the arguments of t meet the constraints of the operator. NULL
   otherwise, and NULL for an operator that would copy a lock. The call is
   checked with the errors held back, and the constraints are read after
   it. */
static struct expr *class_operator(struct checker *c, struct type *t,
                                   struct pos pos, const char *text,
                                   size_t count, enum type_kind result)
{
    struct name name;
    struct symbol *fn;
    const struct item *it;
    struct expr *call;
    struct expr *callee;
    struct expr **args;
    struct type *sig;
    struct context quiet;
    size_t i;
    bool ok;

    name.text = text;
    name.length = strlen(text);
    fn = sema_module_function(c, t, &name);
    if (fn == NULL || fn->kind != SYMBOL_FN || !symbol_is_operator(fn) ||
        !takes_first(fn, t)) {
        return NULL;
    }
    it = fn->item;
    call = sema_new_node(c, EXPR_CALL, pos);
    callee = sema_new_node(c, EXPR_NAME, pos);
    args = arena_alloc(c->arena, count * sizeof *args);
    sema_enter_quiet(c, &quiet);
    sig = sema_operator_copy(c, call, fn->type, fn, t, count > 1 ? t : NULL);
    sema_leave(c, &quiet);
    if (sig == NULL || sema_is_error(sig) || sig->param_count != count ||
        sig->result == NULL || sig->result->kind != result) {
        return NULL;
    }
    ok = it != NULL && call->as.call.copy_count == it->type_param_count;
    for (i = 0; ok && i < call->as.call.copy_count; i++) {
        ok = call->as.call.copy_args[i] == NULL ||
             sema_meets_param(c, call->as.call.copy_args[i],
                              it->type_params[i].type);
    }
    for (i = 0; ok && i < count; i++) {
        ok = sig->params[i]->kind == TYPE_POINTER || !sema_holds_mutex(t);
    }
    if (!ok) {
        return NULL;
    }
    callee->symbol = fn;
    callee->type = sig;
    callee->as.name = fn->name;
    for (i = 0; i < count; i++) {
        args[i] = operator_receiver(c, sema_stand_in(c, pos, t),
                                    sig->params[i]);
    }
    call->as.call.callee = callee;
    call->as.call.args = args;
    call->as.call.arg_count = count;
    call->type = sig->result;
    return call;
}

/* DESIGN: a module that gives a class `==` with a free `operator fn eq`
   gives it its hash with an `operator fn hash` beside it, which a
   collection declares as `operator fn hash<T: hash>(a: List<T>) -> u64`.
   It is then the hash of every value of the class: the compiler writes
   the class's `hash` as a call of it, so the table, the default hash of a
   holder and `x.hash()` agree. A copy of a generic class whose arguments
   miss a constraint of the operator keeps the default hash, as it has no
   `==` either. */
struct expr *sema_class_hash_operator(struct checker *c, struct type *t,
                                      struct pos pos)
{
    return class_operator(c, t, pos, LANG_HOOK_HASH, 1, TYPE_U64);
}

/* DESIGN: the `equals` of a class whose module gives it `operator fn eq`
   calls that operator, as its `hash` calls `operator fn hash`. `equals`
   through `*Object` and `==` then agree, and a collection compares its
   elements alone in both, never its count of changes, its room or its
   versions. */
struct expr *sema_class_eq_operator(struct checker *c, struct type *t,
                                    struct pos pos)
{
    return class_operator(c, t, pos, LANG_HOOK_EQ, 2, TYPE_BOOL);
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
        if (types_is_simd(left) || types_is_simd(right)) {
            return sema_check_simd_binary(c, e, left, right);
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
            if (types_has_fields(left) || left->kind == TYPE_ARRAY ||
                left->kind == TYPE_SLICE) {
                const struct struct_field *gap;
                if (sema_equals(c, e, left)) {
                    return sema_builtin(c, TYPE_BOOL);
                }
                gap = left->kind == TYPE_STRUCT && !left->is_union
                          ? sema_eq_gap(c, left)
                      : left->kind == TYPE_CLASS ? sema_class_gap(c, left)
                                                 : NULL;
                if (sema_concurrent_lacks(c, left, LANG_HOOK_EQ)) {
                    sema_error_at(c, e->pos, "`%s` is not defined on `%s`. "
                                  "A concurrent class has no default `==` "
                                  "and declares `concrete fn equals` or "
                                  "`operator fn eq` of its own", o,
                                  sema_tn(left));
                } else if (gap != NULL) {
                    sema_error_at(c, e->pos, "`%s` is not defined on `%s`, "
                                  "whose field `%.*s` has no `eq`", o,
                                  sema_tn(left), (int)gap->name.length,
                                  gap->name.text);
                } else {
                    sema_error_at(c, e->pos, "`%s` is not defined on `%s`",
                                  o, sema_tn(left));
                }
                return sema_builtin(c, TYPE_ERROR);
            }
        } else if (left->kind == TYPE_STRUCT || left->kind == TYPE_CLASS) {
            sema_error_at(c, e->pos, "`%s` is not defined on `%s`%s", o,
                          sema_tn(left), sema_no_order(left, LANG_HOOK_LT));
            return sema_builtin(c, TYPE_ERROR);
        } else if (!types_is_numeric(left) && left->kind != TYPE_CHAR &&
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
        if (types_is_simd(left) || types_is_simd(right)) {
            return sema_check_simd_binary(c, e, left, right);
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
            if (!types_is_integer(left)) {
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
            if (!types_is_numeric(left)) {
                sema_error_at(c, e->pos,
                              "`%s` needs numeric operands, found `%s`",
                              o, sema_tn(left));
                return sema_builtin(c, TYPE_ERROR);
            }
        } else if (!types_is_integer(left)) {
            sema_error_at(c, e->pos, "`%s` needs integer operands, found `%s`",
                          o,
                          sema_tn(left));
            return sema_builtin(c, TYPE_ERROR);
        }
        if ((op == TOKEN_SLASH || op == TOKEN_PERCENT || op == TOKEN_SHL ||
             op == TOKEN_SHR) && types_is_integer(left) &&
            sema_undefined_on_constants(c, e, left)) {
            return sema_builtin(c, TYPE_ERROR);
        }
        return left;
    }
}

/* DESIGN: `x in lo..hi` is `x >= lo && x < hi`. The checker binds x to
   a local that no scope holds and writes the two comparisons over it.
   The value is then computed once, and the high bound only when the
   value reaches the low one. A type with an `lt` operator takes it, as
   the two comparisons would. The first of the three operands that is no
   literal names the type, and the literals take it. */
struct type *sema_check_in(struct checker *c, struct expr *e)
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
    while (first < 3 && sema_is_untyped(operands[first])) {
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
    if (lt == NULL && !types_is_numeric(types[first]) &&
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
        hint = types_is_nullable(expected) ? expected
                                          : types_with_none(c->types, expected);
    }
    left = sema_check_expr(c, e->as.binary.left, hint);
    if (!sema_is_error(left) && !types_is_nullable(left)) {
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
    if (!types_is_nullable(got) && got->kind != TYPE_NONE &&
        !sema_is_error(got)) {
        struct type *bare = types_without_none(c->types, left);
        struct context quiet;
        bool held;
        sema_enter_quiet(c, &quiet);
        held = sema_require(c, right, got, bare);
        sema_leave(c, &quiet);
        if (held) {
            return bare;
        }
    }
    if (!sema_require(c, right, got, left)) {
        return sema_builtin(c, TYPE_ERROR);
    }
    return left;
}

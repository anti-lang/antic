/* Types, and the scan that tells a list of type arguments after `<` from
   a comparison. */

#include <stdlib.h>

#include "parser_parser.h"

/* DESIGN: the rule of C# for `<` in an expression. The tokens from the
   `<` are read as a list of types without building anything. A scan
   holds the token it stands at, as a count ahead of the parser, and
   whether the first `>` of a `>>` there is taken. depth counts the
   levels of the parser and the types the scan is inside, and a scan that
   would pass PARSE_DEPTH_MAX reads no list, as parser_type would refuse it. */
struct angle_scan {
    size_t at;
    bool half;
    int depth;
};

static bool scan_type(const struct parser *p, struct angle_scan *s);

/* Close one list at the scan: a `>`, the first `>` of a `>>`, or the
   second one that a list inside took the first of. */
static bool scan_close(const struct parser *p, struct angle_scan *s)
{
    enum token_kind k = parser_peek_at(p, s->at)->kind;

    if (s->half) {
        s->half = false;
        s->at++;
        return true;
    }
    if (k == TOKEN_GT) {
        s->at++;
        return true;
    }
    if (k == TOKEN_SHR) {
        s->half = true;
        return true;
    }
    return false;
}

/* A list of type arguments from the `<` at the scan. An argument is a
   type or an integer literal. */
static bool scan_list(const struct parser *p, struct angle_scan *s)
{
    s->at++;
    for (;;) {
        if (s->half) {
            return false;
        }
        if (parser_peek_at(p, s->at)->kind == TOKEN_INT) {
            s->at++;
        } else if (!scan_type(p, s)) {
            return false;
        }
        if (!s->half && parser_peek_at(p, s->at)->kind == TOKEN_COMMA) {
            s->at++;
            continue;
        }
        return scan_close(p, s);
    }
}

/* The types of a list that `(` opened at the scan, up to its `)`. */
static bool scan_types(const struct parser *p, struct angle_scan *s)
{
    s->at++;
    while (parser_peek_at(p, s->at)->kind != TOKEN_RPAREN) {
        while (parser_is_fn_mark(p, s->at) ||
               parser_is_word(p, parser_peek_at(p, s->at), "own")) {
            s->at++;
        }
        if (s->half || !scan_type(p, s) || s->half) {
            return false;
        }
        if (parser_peek_at(p, s->at)->kind != TOKEN_COMMA) {
            break;
        }
        s->at++;
    }
    if (parser_peek_at(p, s->at)->kind != TOKEN_RPAREN) {
        return false;
    }
    s->at++;
    return true;
}

static bool scan_type_level(const struct parser *p, struct angle_scan *s)
{
    enum token_kind k = parser_peek_at(p, s->at)->kind;

    if (lexer_token_is_builtin_type(k)) {
        s->at++;
        return true;
    }
    switch (k) {
    case TOKEN_QUESTION:
    case TOKEN_QUESTION_QUESTION:
    case TOKEN_STAR:
    case TOKEN_QUESTION_STAR:
    case TOKEN_CHAN:
        s->at++;
        return scan_type(p, s);
    case TOKEN_IDENT:
        s->at++;
        if (parser_peek_at(p, s->at)->kind == TOKEN_DOT &&
            parser_peek_at(p, s->at + 1)->kind == TOKEN_IDENT) {
            s->at += 2;
        }
        return parser_peek_at(p, s->at)->kind != TOKEN_LT || scan_list(p, s);
    case TOKEN_LBRACKET:
        s->at++;
        k = parser_peek_at(p, s->at)->kind;
        if (k == TOKEN_INT || k == TOKEN_IDENT) {
            s->at++;
        }
        if (parser_peek_at(p, s->at)->kind != TOKEN_RBRACKET) {
            return false;
        }
        s->at++;
        return scan_type(p, s);
    case TOKEN_LPAREN:
        return scan_types(p, s);
    case TOKEN_FN:
        s->at++;
        if (parser_peek_at(p, s->at)->kind != TOKEN_LPAREN ||
            !scan_types(p, s)) {
            return false;
        }
        if (parser_peek_at(p, s->at)->kind == TOKEN_ARROW) {
            s->at++;
            if (!scan_type(p, s)) {
                return false;
            }
        }
        /* A failing function type ends with `may fail`. */
        if (!s->half && parser_is_word(p, parser_peek_at(p, s->at), "may") &&
            parser_peek_at(p, s->at + 1)->kind == TOKEN_FAIL) {
            s->at += 2;
        }
        return true;
    default:
        return false;
    }
}

/* Every recursion of the scan passes here, once per type. */
static bool scan_type(const struct parser *p, struct angle_scan *s)
{
    bool ok;

    if (s->depth >= PARSE_DEPTH_MAX) {
        return false;
    }
    s->depth++;
    ok = scan_type_level(p, s);
    s->depth--;
    return ok;
}

/* Where the list of type arguments whose `<` stands at ahead ends, as a
   count ahead of the parser, or 0. It is a list when its tokens read as
   types closed by `>` and the token after it is `(`, `.` or `{`. */
size_t parser_generic_end(const struct parser *p, size_t ahead)
{
    struct angle_scan s;
    enum token_kind after;

    if (parser_peek_at(p, ahead)->kind != TOKEN_LT) {
        return 0;
    }
    s.at = ahead;
    s.half = false;
    s.depth = p->depth;
    if (!scan_list(p, &s) || s.half) {
        return 0;
    }
    after = parser_peek_at(p, s.at)->kind;
    return after == TOKEN_LPAREN || after == TOKEN_DOT || after == TOKEN_LBRACE
               ? s.at
               : 0;
}

/* Close one list of type arguments, the `>` or one half of a `>>`. */
bool parser_close_angle(struct parser *p)
{
    if (p->half) {
        p->half = false;
        parser_next(p);
        return true;
    }
    if (parser_check(p, TOKEN_SHR)) {
        if (p->angles < 2) {
            parser_error_here(p, "expected `>`");
            return false;
        }
        p->half = true;
        return true;
    }
    return parser_expect(p, TOKEN_GT);
}

/* `<int, str>`: the type arguments after a name. An argument is a type,
   or an integer literal for a constant parameter. */
struct type_expr **parser_type_args(struct parser *p, size_t *count)
{
    struct list args = {NULL, 0, 0, sizeof(struct type_expr *)};
    bool ok = true;

    parser_next(p);
    p->angles++;
    for (;;) {
        struct type_expr *arg;
        if (parser_check(p, TOKEN_INT)) {
            const struct token *t = parser_next(p);
            arg = parser_node(p, sizeof *arg);
            arg->kind = TYPEX_CONST;
            arg->pos = parser_pos_of(t);
            arg->length = parser_new_expr(p, EXPR_INT, t);
            arg->length->as.integer = t->value.integer;
        } else if ((arg = parser_type(p)) == NULL) {
            ok = false;
            break;
        }
        parser_list_push(&args, &arg);
        if (!p->half && parser_accept(p, TOKEN_COMMA)) {
            continue;
        }
        ok = parser_close_angle(p);
        break;
    }
    p->angles--;
    if (!ok) {
        free(args.data);
        *count = 0;
        return NULL;
    }
    return parser_list_finish(p, &args, count);
}

static struct type_expr *type_level(struct parser *p)
{
    const struct token *t = parser_peek(p);
    struct type_expr *ty = parser_node(p, sizeof *ty);

    ty->pos = parser_pos_of(t);
    /* `?T` is a T or `none`, for any type. `?*T` is one token, and `?fn`
       stays the function type that may hold `none`, which the branches
       below read. `??T` is `?` twice. */
    if ((t->kind == TOKEN_QUESTION && parser_peek_at(p, 1)->kind != TOKEN_FN) ||
        t->kind == TOKEN_QUESTION_QUESTION) {
        parser_next(p);
        ty->kind = TYPEX_OPTIONAL;
        if ((ty->element = parser_type(p)) == NULL) {
            return NULL;
        }
        if (t->kind == TOKEN_QUESTION_QUESTION) {
            struct type_expr *outer = parser_node(p, sizeof *outer);
            outer->pos = ty->pos;
            outer->kind = TYPEX_OPTIONAL;
            outer->element = ty;
            ty = outer;
        }
        return ty;
    }
    if (lexer_token_is_builtin_type(t->kind)) {
        parser_next(p);
        ty->kind = TYPEX_BUILTIN;
        ty->builtin = t->kind;
    } else if (t->kind == TOKEN_IDENT) {
        ty->kind = TYPEX_NAMED;
        parser_expect_name(p, &ty->name);
        if (parser_accept(p, TOKEN_DOT)) {
            ty->module = ty->name;
            if (!parser_expect_name(p, &ty->name)) {
                return NULL;
            }
        }
        /* In a type, `<` after a name always opens type arguments. */
        if (parser_check(p, TOKEN_LT) &&
            (ty->args = parser_type_args(p, &ty->arg_count)) == NULL) {
            return NULL;
        }
    } else if (parser_accept(p, TOKEN_CHAN)) {
        /* `chan T`, the channel of values of T. */
        ty->kind = TYPEX_CHAN;
        if ((ty->element = parser_type(p)) == NULL) {
            return NULL;
        }
    } else if (parser_check(p, TOKEN_STAR) ||
               parser_check(p, TOKEN_QUESTION_STAR)) {
        ty->nullable = parser_accept(p, TOKEN_QUESTION_STAR);
        if (!ty->nullable) {
            parser_next(p);
        }
        ty->kind = TYPEX_POINTER;
        if ((ty->element = parser_type(p)) == NULL) {
            return NULL;
        }
    } else if (parser_accept(p, TOKEN_LBRACKET)) {
        if (parser_accept(p, TOKEN_RBRACKET)) {
            ty->kind = TYPEX_SLICE;
        } else {
            ty->kind = TYPEX_ARRAY;
            if ((ty->length = parser_expression(p)) == NULL ||
                !parser_expect(p, TOKEN_RBRACKET)) {
                return NULL;
            }
        }
        if ((ty->element = parser_type(p)) == NULL) {
            return NULL;
        }
    } else if (parser_accept(p, TOKEN_LPAREN)) {
        /* `(int, str)`, an anonymous struct with C layout. Two elements
           are the fewest that have no name of their own, so `()` and
           `(T)` are refused. */
        struct list elements = {NULL, 0, 0, sizeof(struct type_expr *)};

        ty->kind = TYPEX_TUPLE;
        while (!parser_check(p, TOKEN_RPAREN)) {
            struct type_expr *element;
            /* `(K, lent *V)`: `lent` is a contextual word, so a type of
               that name still stands alone as an element. */
            bool lent = parser_is_word(p, parser_peek(p), "lent") &&
                        parser_peek_at(p, 1)->kind != TOKEN_COMMA &&
                        parser_peek_at(p, 1)->kind != TOKEN_RPAREN;
            struct pos lent_pos = parser_pos_of(parser_peek(p));
            if (lent) {
                parser_next(p);
            }
            element = parser_type(p);
            if (element == NULL) {
                free(elements.data);
                return NULL;
            }
            element->lent = lent;
            if (lent) {
                element->pos = lent_pos;
            }
            parser_list_push(&elements, &element);
            if (!parser_accept(p, TOKEN_COMMA)) {
                break;
            }
        }
        ty->params = parser_list_finish(p, &elements, &ty->param_count);
        if (ty->param_count < 2) {
            parser_error_here(p, "a tuple has two or more elements");
            return NULL;
        }
        if (!parser_expect(p, TOKEN_RPAREN)) {
            return NULL;
        }
    } else if (parser_check(p, TOKEN_FN) ||
               (parser_check(p, TOKEN_QUESTION) &&
                parser_peek_at(p, 1)->kind == TOKEN_FN)) {
        struct list params = {NULL, 0, 0, sizeof(struct type_expr *)};

        /* A function value follows the pointer rule, so `?fn(...)` may
           hold `none` and `fn(...)` may not. */
        ty->nullable = parser_accept(p, TOKEN_QUESTION);
        parser_next(p);
        ty->kind = TYPEX_FN;
        if (!parser_expect(p, TOKEN_LPAREN)) {
            return NULL;
        }
        while (!parser_check(p, TOKEN_RPAREN)) {
            bool keep = false;
            bool concurrent = false;
            bool owned = false;
            bool lent = false;
            struct type_expr *param;
            parser_fn_param_marks(p, &keep, &concurrent, &owned);
            /* `fn(lent *T)`: `lent` is a contextual word, so a type of
               that name still stands alone in the list. */
            if (parser_is_word(p, parser_peek(p), "lent") &&
                parser_peek_at(p, 1)->kind != TOKEN_COMMA &&
                parser_peek_at(p, 1)->kind != TOKEN_RPAREN) {
                parser_next(p);
                lent = true;
            }
            param = parser_type(p);
            if (param == NULL) {
                free(params.data);
                return NULL;
            }
            param->keep = keep;
            param->concurrent = concurrent;
            param->owned = owned;
            param->lent = lent;
            parser_list_push(&params, &param);
            if (!parser_accept(p, TOKEN_COMMA)) {
                break;
            }
        }
        ty->params = parser_list_finish(p, &params, &ty->param_count);
        if (!parser_expect(p, TOKEN_RPAREN)) {
            return NULL;
        }
        if (parser_accept(p, TOKEN_ARROW)) {
            if (parser_never_result(p)) {
                ty->never = true;
                parser_next(p);
            } else if ((ty->result = parser_type(p)) == NULL) {
                return NULL;
            }
        }
        /* DESIGN: `may fail` after a function type belongs to that type,
           the innermost one when a result is a function type in turn. A
           signature that fails itself and returns such a type writes the
           words twice. */
        if (parser_is_word(p, parser_peek(p), "may") &&
            parser_peek_at(p, 1)->kind == TOKEN_FAIL) {
            parser_next(p);
            parser_next(p);
            ty->may_fail = true;
        }
    } else {
        parser_error_here(p, "expected a type");
        return NULL;
    }
    return ty;
}

struct type_expr *parser_type(struct parser *p)
{
    struct type_expr *ty;

    if (!parser_descend(p)) {
        return NULL;
    }
    ty = type_level(p);
    parser_ascend(p);
    return ty;
}

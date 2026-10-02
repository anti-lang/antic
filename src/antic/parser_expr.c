/* Expressions: the primary forms with `f"..."` and anonymous functions,
   the postfix links, the unary operators and casts, and the binary
   operators by precedence climbing. */

#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "parser_parser.h"

static struct expr *postfix(struct parser *p);

struct expr *parser_new_expr(struct parser *p, enum expr_kind kind,
                             const struct token *at)
{
    struct expr *e = parser_node(p, sizeof *e);

    e->kind = kind;
    e->pos = parser_pos_of(at);
    e->spelling.bytes = p->source + at->offset;
    e->spelling.length = at->length;
    return e;
}

/* The comma-separated name: value pairs and the closing brace of a struct
   or slice literal. */
static struct field_init *field_inits(struct parser *p, size_t *count)
{
    struct list fields = {NULL, 0, 0, sizeof(struct field_init)};
    bool saved = p->no_struct_literal;

    p->no_struct_literal = false;
    while (!parser_check(p, TOKEN_RBRACE)) {
        struct field_init f;
        f.pos = parser_pos_of(parser_peek(p));
        if (!parser_expect_name(p, &f.name) || !parser_expect(p, TOKEN_COLON) ||
            (f.value = parser_expression(p)) == NULL) {
            free(fields.data);
            p->no_struct_literal = saved;
            return NULL;
        }
        parser_list_push(&fields, &f);
        if (!parser_accept(p, TOKEN_COMMA)) {
            break;
        }
    }
    p->no_struct_literal = saved;
    if (!parser_expect(p, TOKEN_RBRACE)) {
        free(fields.data);
        return NULL;
    }
    return parser_list_finish(p, &fields, count);
}

/* A comma-separated list of expressions up to the closing token. */
static struct expr **expressions(struct parser *p, enum token_kind close,
                                 size_t *count)
{
    struct list items = {NULL, 0, 0, sizeof(struct expr *)};

    while (!parser_check(p, close)) {
        struct expr *e = parser_expression(p);
        if (e == NULL) {
            free(items.data);
            return NULL;
        }
        parser_list_push(&items, &e);
        if (!parser_accept(p, TOKEN_COMMA)) {
            break;
        }
    }
    if (!parser_expect(p, close)) {
        free(items.data);
        return NULL;
    }
    return parser_list_finish(p, &items, count);
}

/* Read the format specification of an `{expr}` into out: an alignment
   `<`, `>` or `^`, a `0` for zero padding, a width, a `.` and a
   precision, and one of `x X b o e f`, each optional and in that order.
   Returns false for any other text, an empty one included. A width and a
   precision take at most nine digits. `0` pads a numeric value of a
   given width and stands without an alignment. */
static bool format_spec(struct token_text spec, struct format_spec *out)
{
    const char *s = spec.bytes;
    size_t n = spec.length;
    size_t i = 0;
    size_t first;

    out->align = 0;
    out->zero = false;
    out->width = -1;
    out->precision = -1;
    out->kind = 0;
    if (i < n && (s[i] == '<' || s[i] == '>' || s[i] == '^')) {
        out->align = s[i++];
    }
    if (i < n && s[i] == '0') {
        out->zero = true;
        i++;
    }
    for (first = i; i < n && s[i] >= '0' && s[i] <= '9' && i - first < 9;
         i++) {
        out->width = (out->width < 0 ? 0 : out->width * 10) + (s[i] - '0');
    }
    if (i < n && s[i] == '.') {
        i++;
        for (first = i; i < n && s[i] >= '0' && s[i] <= '9' && i - first < 9;
             i++) {
            out->precision =
                (out->precision < 0 ? 0 : out->precision * 10) + (s[i] - '0');
        }
        if (i == first) {
            return false;
        }
    }
    if (i < n && s[i] != '\0' && strchr("xXboef", s[i]) != NULL) {
        out->kind = s[i++];
    }
    if (out->zero && (out->align != 0 || out->width <= 0)) {
        return false;
    }
    return n > 0 && i == n;
}

/* The expression of one `{expr}`, parsed from the tokens the lexer made
   for it. The parser reads them as it reads any expression and reports
   at the positions in the file. A doc comment among them is dropped, as
   one inside any other expression is. */
static struct expr *placeholder(struct parser *p,
                                const struct format_piece *piece)
{
    struct parser inner = *p;
    struct token *kept = alloc_zeroed(piece->token_count, sizeof *kept);
    size_t *origin = alloc_zeroed(piece->token_count, sizeof *origin);
    bool *taken = alloc_zeroed(piece->token_count, sizeof *taken);
    size_t count = parser_keep_tokens(piece->tokens, piece->token_count, kept,
                                      origin);
    struct expr *e;

    inner.tokens = kept;
    inner.all = piece->tokens;
    inner.origin = origin;
    inner.taken = taken;
    inner.pos = 0;
    inner.last = count - 1;
    inner.panic = false;
    inner.ok = true;
    inner.no_struct_literal = false;
    e = parser_expression(&inner);
    if (inner.ok && !parser_check(&inner, TOKEN_EOF)) {
        parser_error_here(&inner, "expected `}` or `:` after the expression");
    }
    parser_drop_untaken(&inner, piece->token_count);
    free(kept);
    free(origin);
    free(taken);
    if (!inner.ok) {
        p->ok = false;
        return NULL;
    }
    return e;
}

/* `f"..."` and `rf"..."`: the text of each piece, the expression of each
   `{expr}` and its format specification. */
static struct expr *format_literal(struct parser *p, const struct token *t)
{
    struct expr *e = parser_new_expr(p, EXPR_FORMAT, t);
    size_t n = t->value.format.count;
    struct format_part *parts = parser_node(p, n * sizeof *parts);
    size_t i;

    parser_next(p);
    e->as.format.raw = p->source[t->offset] == 'r';
    for (i = 0; i < n; i++) {
        const struct format_piece *piece = &t->value.format.pieces[i];
        struct format_part *part = &parts[i];
        part->text = piece->text;
        part->pos.line = piece->line;
        part->pos.column = piece->column;
        part->source.bytes = p->source + piece->offset;
        part->source.length = piece->length;
        part->spec.width = -1;
        part->spec.precision = -1;
        if (piece->token_count == 0) {
            continue;
        }
        part->value = placeholder(p, piece);
        if (piece->spec.bytes != NULL &&
            !format_spec(piece->spec, &part->spec)) {
            diagnostics_add(p->diags, piece->line, piece->column,
                            "unknown format `%.*s`", (int)piece->length,
                            p->source + piece->offset);
            p->ok = false;
        }
    }
    e->as.format.parts = parts;
    e->as.format.count = n;
    return e;
}

/* DESIGN: `fn(params) -> R { body }` in the place of an expression is an
   anonymous function. The parser writes it as an ITEM_FN of its own, so
   the checker and lowering treat its body as they treat any function's.
   A parameter may leave out its type, and a missing result or `may
   fail` may come from the target as well, which the checker decides.
   The body is a block of its own, so a struct literal is allowed in it
   inside a condition too. */
static struct expr *anonymous_fn(struct parser *p, const struct token *at)
{
    struct list list = {NULL, 0, 0, sizeof(struct param)};
    struct expr *e = parser_new_expr(p, EXPR_FN, at);
    struct item *it = parser_node(p, sizeof *it);
    bool saved = p->no_struct_literal;

    parser_next(p);
    it->kind = ITEM_FN;
    it->pos = parser_pos_of(at);
    it->name_pos = it->pos;
    it->name.text = p->source + at->offset;
    it->name.length = at->length;
    e->as.fn = it;
    if (!parser_expect(p, TOKEN_LPAREN)) {
        return NULL;
    }
    while (!parser_check(p, TOKEN_RPAREN)) {
        struct param param;
        memset(&param, 0, sizeof param);
        parser_fn_param_marks(p, &param.keep, &param.concurrent, &param.owned);
        param.lent = parser_lent_mark(p);
        param.pos = parser_pos_of(parser_peek(p));
        if (!parser_expect_name(p, &param.name) ||
            (parser_accept(p, TOKEN_COLON) &&
             (param.type = parser_type(p)) == NULL)) {
            free(list.data);
            return NULL;
        }
        parser_list_push(&list, &param);
        if (!parser_accept(p, TOKEN_COMMA)) {
            break;
        }
    }
    if (!parser_expect(p, TOKEN_RPAREN)) {
        free(list.data);
        return NULL;
    }
    it->params = parser_list_finish(p, &list, &it->param_count);
    if (!parser_result_type(p, it)) {
        return NULL;
    }
    parser_may_fail_after(p, it);
    p->no_struct_literal = false;
    it->body = parser_block(p);
    p->no_struct_literal = saved;
    return it->body != NULL ? e : NULL;
}

/* The type arguments after the name of the literal e, with their
   position. False after an error. */
static bool literal_type_args(struct parser *p, struct expr *e)
{
    e->type_args_pos = parser_pos_of(parser_peek(p));
    e->type_args = parser_type_args(p, &e->type_arg_count);
    return e->type_args != NULL;
}

/* The `{` of the literal e and its fields, or NULL after an error. */
static struct expr *literal_fields(struct parser *p, struct expr *e)
{
    parser_next(p);
    e->as.struct_lit.fields = field_inits(p, &e->as.struct_lit.field_count);
    return p->panic ? NULL : e;
}

static struct expr *primary(struct parser *p)
{
    const struct token *t = parser_peek(p);
    struct expr *e;

    switch (t->kind) {
    case TOKEN_INT:
        parser_next(p);
        e = parser_new_expr(p, EXPR_INT, t);
        e->as.integer = t->value.integer;
        return e;
    case TOKEN_FLOAT:
    case TOKEN_STRING:
    case TOKEN_BYTES:
        parser_next(p);
        e = parser_new_expr(p, t->kind == TOKEN_FLOAT    ? EXPR_FLOAT
                               : t->kind == TOKEN_STRING ? EXPR_STRING
                                                         : EXPR_BYTES,
                            t);
        e->as.text = t->value.text;
        return e;
    case TOKEN_FORMAT:
        return format_literal(p, t);
    case TOKEN_PATTERN:
        parser_next(p);
        e = parser_new_expr(p, EXPR_PATTERN, t);
        e->as.text = t->value.text;
        return e;
    case TOKEN_CHAR:
        parser_next(p);
        e = parser_new_expr(p, EXPR_CHAR, t);
        e->as.character = t->value.character;
        return e;
    case TOKEN_TRUE:
    case TOKEN_FALSE:
        parser_next(p);
        e = parser_new_expr(p, EXPR_BOOL, t);
        e->as.boolean = t->kind == TOKEN_TRUE;
        return e;
    case TOKEN_NONE:
        parser_next(p);
        return parser_new_expr(p, EXPR_NONE, t);
    case TOKEN_HERE:
        parser_next(p);
        return parser_new_expr(p, EXPR_HERE, t);
    case TOKEN_FN:
        return anonymous_fn(p, t);
    /* `self` is the receiver of a function of a struct body. It reads as
       a name, and the checker gives it the type *T. */
    case TOKEN_SELF:
        parser_next(p);
        e = parser_new_expr(p, EXPR_NAME, t);
        e->as.name.text = p->source + t->offset;
        e->as.name.length = t->length;
        return e;
    case TOKEN_IDENT: {
        bool qualified;
        bool member;
        size_t end;
        /* `snapshot fn(...) { }` is an anonymous function that copies
           what it captures. `snapshot` is a contextual word. */
        if (parser_is_word(p, t, "snapshot") &&
            parser_peek_at(p, 1)->kind == TOKEN_FN) {
            parser_next(p);
            e = anonymous_fn(p, parser_peek(p));
            if (e != NULL) {
                e->pos = parser_pos_of(t);
                e->as.fn->snapshot = true;
            }
            return e;
        }
        /* `Pair<int, str> { }`, `max<int>(a, b)` and `List<int>.new()`
           name a generic with its type arguments, by the rule of C#. A
           variant's case follows its arguments,
           `Result<int, str>.Ok { }`. */
        end = parser_generic_end(p, 1);
        if (end > 0) {
            if (!p->no_struct_literal &&
                (parser_peek_at(p, end)->kind == TOKEN_LBRACE ||
                 (parser_peek_at(p, end)->kind == TOKEN_DOT &&
                  parser_peek_at(p, end + 1)->kind == TOKEN_IDENT &&
                  parser_peek_at(p, end + 2)->kind == TOKEN_LBRACE))) {
                bool case_of = parser_peek_at(p, end)->kind == TOKEN_DOT;
                e = parser_new_expr(p, EXPR_STRUCT_LIT, t);
                parser_expect_name(p, &e->as.struct_lit.name);
                if (!literal_type_args(p, e)) {
                    return NULL;
                }
                if (case_of) {
                    e->as.struct_lit.module = e->as.struct_lit.name;
                    parser_next(p);
                    parser_expect_name(p, &e->as.struct_lit.name);
                }
                return literal_fields(p, e);
            }
            e = parser_new_expr(p, EXPR_NAME, t);
            parser_expect_name(p, &e->as.name);
            e->type_args_pos = parser_pos_of(parser_peek(p));
            e->type_args = parser_type_args(p, &e->type_arg_count);
            return e->type_args == NULL ? NULL : e;
        }
        /* `geo.Pair<int, str> { }` names a generic of another module. */
        if (!p->no_struct_literal && parser_peek_at(p, 1)->kind == TOKEN_DOT &&
            parser_peek_at(p, 2)->kind == TOKEN_IDENT &&
            (end = parser_generic_end(p, 3)) > 0 &&
            parser_peek_at(p, end)->kind == TOKEN_LBRACE) {
            e = parser_new_expr(p, EXPR_STRUCT_LIT, t);
            parser_expect_name(p, &e->as.struct_lit.module);
            parser_next(p);
            parser_expect_name(p, &e->as.struct_lit.name);
            return literal_type_args(p, e) ? literal_fields(p, e) : NULL;
        }
        qualified = parser_peek_at(p, 1)->kind == TOKEN_DOT &&
                    parser_peek_at(p, 2)->kind == TOKEN_IDENT &&
                    parser_peek_at(p, 3)->kind == TOKEN_LBRACE;
        /* `geo.Shape.Circle { }` names the case of a variant of another
           module. */
        member = parser_peek_at(p, 1)->kind == TOKEN_DOT &&
                 parser_peek_at(p, 2)->kind == TOKEN_IDENT &&
                 parser_peek_at(p, 3)->kind == TOKEN_DOT &&
                 parser_peek_at(p, 4)->kind == TOKEN_IDENT &&
                 parser_peek_at(p, 5)->kind == TOKEN_LBRACE;
        if (!p->no_struct_literal &&
            (qualified || member ||
             parser_peek_at(p, 1)->kind == TOKEN_LBRACE)) {
            e = parser_new_expr(p, EXPR_STRUCT_LIT, t);
            parser_expect_name(p, &e->as.struct_lit.name);
            if (qualified || member) {
                e->as.struct_lit.module = e->as.struct_lit.name;
                parser_next(p);
                parser_expect_name(p, &e->as.struct_lit.name);
            }
            if (member) {
                parser_next(p);
                parser_expect_name(p, &e->as.struct_lit.member);
            }
            return literal_fields(p, e);
        }
        e = parser_new_expr(p, EXPR_NAME, t);
        parser_expect_name(p, &e->as.name);
        return e;
    }
    case TOKEN_LPAREN: {
        bool saved = p->no_struct_literal;
        struct expr *first;
        parser_next(p);
        p->no_struct_literal = false;
        first = parser_expression(p);
        /* `(a)` groups and `(a, b)` builds a tuple. */
        if (first != NULL && parser_check(p, TOKEN_COMMA)) {
            struct list elements = {NULL, 0, 0, sizeof(struct expr *)};
            e = parser_new_expr(p, EXPR_TUPLE, t);
            parser_list_push(&elements, &first);
            while (parser_accept(p, TOKEN_COMMA)) {
                struct expr *element = parser_expression(p);
                if (element == NULL) {
                    free(elements.data);
                    return NULL;
                }
                parser_list_push(&elements, &element);
            }
            e->as.tuple.elements =
                parser_list_finish(p, &elements, &e->as.tuple.count);
            p->no_struct_literal = saved;
            return parser_expect(p, TOKEN_RPAREN) ? e : NULL;
        }
        p->no_struct_literal = saved;
        return first != NULL && parser_expect(p, TOKEN_RPAREN) ? first : NULL;
    }
    case TOKEN_LBRACKET:
        if (parser_peek_at(p, 1)->kind == TOKEN_RBRACKET) {
            if (p->no_struct_literal) {
                break;
            }
            e = parser_new_expr(p, EXPR_SLICE_LIT, t);
            parser_next(p);
            parser_next(p);
            if ((e->as.slice_lit.element = parser_type(p)) == NULL ||
                !parser_expect(p, TOKEN_LBRACE)) {
                return NULL;
            }
            e->as.slice_lit.fields =
                field_inits(p, &e->as.slice_lit.field_count);
            return p->panic ? NULL : e;
        }
        parser_next(p);
        {
            struct expr *first = parser_expression(p);
            if (first == NULL) {
                return NULL;
            }
            if (parser_accept(p, TOKEN_SEMICOLON)) {
                e = parser_new_expr(p, EXPR_ARRAY_REPEAT, t);
                e->as.array_repeat.value = first;
                if ((e->as.array_repeat.count = parser_expression(p)) == NULL ||
                    !parser_expect(p, TOKEN_RBRACKET)) {
                    return NULL;
                }
                return e;
            }
            e = parser_new_expr(p, EXPR_ARRAY_LIT, t);
            {
                struct list items = {NULL, 0, 0, sizeof(struct expr *)};
                parser_list_push(&items, &first);
                while (parser_accept(p, TOKEN_COMMA) &&
                       !parser_check(p, TOKEN_RBRACKET)) {
                    struct expr *item = parser_expression(p);
                    if (item == NULL) {
                        free(items.data);
                        return NULL;
                    }
                    parser_list_push(&items, &item);
                }
                e->as.array_lit.elements =
                    parser_list_finish(p, &items, &e->as.array_lit.count);
            }
            return parser_expect(p, TOKEN_RBRACKET) ? e : NULL;
        }
    case TOKEN_ALLOC:
        parser_next(p);
        e = parser_new_expr(p, EXPR_ALLOC, t);
        /* DESIGN: `alloc T { ... }` puts one object on the heap and
           writes the literal into it. `alloc(T, n)` stays the raw form
           for any type and any count. */
        if (!parser_check(p, TOKEN_LPAREN)) {
            if ((e->as.alloc.value = parser_expression(p)) == NULL) {
                return NULL;
            }
            return e;
        }
        if (!parser_expect(p, TOKEN_LPAREN) ||
            (e->as.alloc.type = parser_type(p)) == NULL ||
            !parser_expect(p, TOKEN_COMMA) ||
            (e->as.alloc.count = parser_expression(p)) == NULL) {
            return NULL;
        }
        parser_accept(p, TOKEN_COMMA);
        return parser_expect(p, TOKEN_RPAREN) ? e : NULL;
    case TOKEN_DUP:
    case TOKEN_DELETE:
    case TOKEN_DESTROY:
        e = parser_new_expr(p, EXPR_OBJECT, t);
        e->as.object.op = t->kind;
        parser_next(p);
        if (!parser_expect(p, TOKEN_LPAREN) ||
            (e->as.object.operand = parser_expression(p)) == NULL) {
            return NULL;
        }
        if (parser_accept(p, TOKEN_COMMA) && !parser_check(p, TOKEN_RPAREN)) {
            if ((e->as.object.from = parser_expression(p)) == NULL) {
                return NULL;
            }
            parser_accept(p, TOKEN_COMMA);
        }
        return parser_expect(p, TOKEN_RPAREN) ? e : NULL;
    case TOKEN_FREE:
        parser_next(p);
        e = parser_new_expr(p, EXPR_FREE, t);
        if (!parser_expect(p, TOKEN_LPAREN) ||
            (e->as.free_pointer = parser_expression(p)) == NULL) {
            return NULL;
        }
        parser_accept(p, TOKEN_COMMA);
        return parser_expect(p, TOKEN_RPAREN) ? e : NULL;
    case TOKEN_SIZE_OF:
        parser_next(p);
        e = parser_new_expr(p, EXPR_SIZE_OF, t);
        if (!parser_expect(p, TOKEN_LPAREN) ||
            (e->as.size_of = parser_type(p)) == NULL) {
            return NULL;
        }
        parser_accept(p, TOKEN_COMMA);
        return parser_expect(p, TOKEN_RPAREN) ? e : NULL;
    case TOKEN_DISPATCH:
        /* DESIGN: `dispatch obj -> f(args)` reads as one expression, as
           `parallel` does. The object stands before the arrow and the
           worker after it, alone or called with the arguments that the
           worker receives after the object. */
        parser_next(p);
        e = parser_new_expr(p, EXPR_DISPATCH, t);
        if ((e->as.dispatch.object = parser_expression(p)) == NULL ||
            !parser_expect(p, TOKEN_ARROW) ||
            (e->as.dispatch.call = postfix(p)) == NULL) {
            return NULL;
        }
        return e;
    case TOKEN_JOIN:
    case TOKEN_JOIN_ALL:
        e = parser_new_expr(p, EXPR_JOIN, t);
        e->as.join.all = t->kind == TOKEN_JOIN_ALL;
        parser_next(p);
        if (!parser_expect(p, TOKEN_LPAREN) ||
            (e->as.join.job = parser_expression(p)) == NULL) {
            return NULL;
        }
        parser_accept(p, TOKEN_COMMA);
        return parser_expect(p, TOKEN_RPAREN) ? e : NULL;
    /* DESIGN: `chan T(n)` makes a channel of capacity n, and `send(c, v)`
       and `recv(c)` read as calls of their keyword. `close(c)` is a call
       of a name, which the checker takes as the built-in unless a
       function of that name is in scope. */
    case TOKEN_CHAN:
        parser_next(p);
        e = parser_new_expr(p, EXPR_SYNC_OP, t);
        e->as.sync_op.op = SYNC_CHAN_NEW;
        if ((e->as.sync_op.element = parser_type(p)) == NULL ||
            !parser_expect(p, TOKEN_LPAREN) ||
            (e->as.sync_op.value = parser_expression(p)) == NULL) {
            return NULL;
        }
        parser_accept(p, TOKEN_COMMA);
        return parser_expect(p, TOKEN_RPAREN) ? e : NULL;
    case TOKEN_SEND:
    case TOKEN_RECV:
        parser_next(p);
        e = parser_new_expr(p, EXPR_SYNC_OP, t);
        e->as.sync_op.op = t->kind == TOKEN_SEND ? SYNC_SEND : SYNC_RECV;
        if (!parser_expect(p, TOKEN_LPAREN) ||
            (e->as.sync_op.target = parser_expression(p)) == NULL) {
            return NULL;
        }
        if (t->kind == TOKEN_SEND &&
            (!parser_expect(p, TOKEN_COMMA) ||
             (e->as.sync_op.value = parser_expression(p)) == NULL)) {
            return NULL;
        }
        parser_accept(p, TOKEN_COMMA);
        return parser_expect(p, TOKEN_RPAREN) ? e : NULL;
    case TOKEN_PARALLEL:
        /* DESIGN: `parallel a by n -> f(x)` reads as one expression. The
           array and the chunk count are expressions, and `by` is a
           contextual word rather than a keyword. After the arrow stands
           the worker, alone or called with the arguments that every
           chunk receives after its own. */
        parser_next(p);
        e = parser_new_expr(p, EXPR_PARALLEL, t);
        if ((e->as.parallel.array = parser_expression(p)) == NULL) {
            return NULL;
        }
        if (parser_is_word(p, parser_peek(p), "by")) {
            parser_next(p);
            if ((e->as.parallel.chunks = parser_expression(p)) == NULL) {
                return NULL;
            }
        }
        if (!parser_expect(p, TOKEN_ARROW) ||
            (e->as.parallel.call = postfix(p)) == NULL) {
            return NULL;
        }
        return e;
    default:
        break;
    }
    parser_error_here(p, "expected an expression");
    return NULL;
}

/* The field name of a tuple element: `_` and the digits of the number,
   which is the name types.c gives the element. */
static bool element_name(struct parser *p, struct name *out)
{
    const struct token *t = parser_peek(p);
    const char *digits = p->source + t->offset;
    char *text;
    size_t i;

    for (i = 0; i < t->length; i++) {
        if (digits[i] < '0' || digits[i] > '9') {
            parser_error_here(p, "an element of a tuple is a decimal number");
            return false;
        }
    }
    text = arena_alloc(p->arena, t->length + 1);
    text[0] = '_';
    memcpy(text + 1, digits, t->length);
    out->text = text;
    out->length = t->length + 1;
    parser_next(p);
    return true;
}

static struct expr *postfix_links(struct parser *p)
{
    struct expr *e = primary(p);

    while (e != NULL) {
        const struct token *t = parser_peek(p);
        struct expr *outer;

        if (parser_accept(p, TOKEN_LPAREN)) {
            bool saved = p->no_struct_literal;
            outer = parser_new_expr(p, EXPR_CALL, t);
            outer->pos = e->pos;
            outer->as.call.callee = e;
            p->no_struct_literal = false;
            outer->as.call.args =
                expressions(p, TOKEN_RPAREN, &outer->as.call.arg_count);
            p->no_struct_literal = saved;
            if (p->panic) {
                return NULL;
            }
        } else if (parser_accept(p, TOKEN_LBRACKET)) {
            bool saved = p->no_struct_literal;
            struct expr *first;
            p->no_struct_literal = false;
            first = parser_expression(p);
            if (first != NULL && parser_accept(p, TOKEN_DOT_DOT)) {
                outer = parser_new_expr(p, EXPR_SLICE, t);
                outer->as.slice.base = e;
                outer->as.slice.low = first;
                outer->as.slice.high = parser_expression(p);
            } else {
                outer = parser_new_expr(p, EXPR_INDEX, t);
                outer->as.index.base = e;
                outer->as.index.index = first;
                /* `g[x, y]` gives the hooks every index. */
                if (first != NULL && parser_check(p, TOKEN_COMMA)) {
                    struct list indices = {NULL, 0, 0,
                                           sizeof(struct expr *)};
                    struct expr *all = parser_new_expr(p, EXPR_TUPLE, t);
                    all->pos = first->pos;
                    parser_list_push(&indices, &first);
                    while (parser_accept(p, TOKEN_COMMA)) {
                        struct expr *index = parser_expression(p);
                        if (index == NULL) {
                            free(indices.data);
                            return NULL;
                        }
                        parser_list_push(&indices, &index);
                    }
                    all->as.tuple.elements =
                        parser_list_finish(p, &indices, &all->as.tuple.count);
                    outer->as.index.index = all;
                    outer->as.index.several = true;
                }
            }
            p->no_struct_literal = saved;
            outer->pos = e->pos;
            if (p->panic || !parser_expect(p, TOKEN_RBRACKET)) {
                return NULL;
            }
        } else if (parser_accept(p, TOKEN_DOT) ||
                   parser_accept(p, TOKEN_QUESTION_DOT)) {
            outer = parser_new_expr(p, EXPR_FIELD, t);
            outer->pos = e->pos;
            outer->as.field.base = e;
            outer->as.field.optional = t->kind == TOKEN_QUESTION_DOT;
            /* An element of a tuple is its number, and the field it
               names is `_0` upwards. */
            if (parser_check(p, TOKEN_INT)) {
                outer->as.field.element = true;
                if (!element_name(p, &outer->as.field.name)) {
                    return NULL;
                }
            } else if (parser_check(p, TOKEN_SUPER) ||
                       parser_check(p, TOKEN_DESTROY) ||
                       parser_check(p, TOKEN_SELECT)) {
                /* `self.super` names the base, `m.destroy()` the
                   function that releases a Mutex and `simd.select` the
                   choice of `anti.simd`. The built-in `destroy` and the
                   statement `select` never follow `.`. */
                outer->as.field.name.text = p->source + parser_peek(p)->offset;
                outer->as.field.name.length = parser_peek(p)->length;
                parser_next(p);
            } else if (!parser_expect_function_name(p, &outer->as.field.name)) {
                return NULL;
            }
            /* `geo.max<int>(a, b)` and `geo.List<int>.new()`. */
            if (!outer->as.field.element && parser_generic_end(p, 0) > 0) {
                outer->type_args_pos = parser_pos_of(parser_peek(p));
                outer->type_args = parser_type_args(p, &outer->type_arg_count);
                if (outer->type_args == NULL) {
                    return NULL;
                }
            }
        } else {
            return e;
        }
        if (!parser_chain_link(p)) {
            return NULL;
        }
        e = outer;
    }
    return NULL;
}

static struct expr *postfix(struct parser *p)
{
    struct chain c;
    struct expr *e;

    parser_chain_begin(p, &c);
    e = postfix_links(p);
    parser_chain_end(p, &c);
    return e;
}

static struct expr *unary(struct parser *p);

static struct expr *unary_level(struct parser *p)
{
    const struct token *t = parser_peek(p);

    switch (t->kind) {
    case TOKEN_MINUS:
    case TOKEN_BANG:
    case TOKEN_TILDE:
    case TOKEN_STAR:
    case TOKEN_AMP: {
        struct expr *e;
        parser_next(p);
        e = parser_new_expr(p, EXPR_UNARY, t);
        e->as.unary.op = t->kind;
        e->as.unary.operand = unary(p);
        return e->as.unary.operand != NULL ? e : NULL;
    }
    /* DESIGN: `try f(args)` is `f(args) catch e { return e; }`, so it
       stands where the call stands and carries the same handler. */
    case TOKEN_TRY: {
        struct expr *e;
        parser_next(p);
        e = postfix(p);
        if (e == NULL) {
            return NULL;
        }
        if (e->kind != EXPR_CALL) {
            parser_error_here(p, "`try` stands before a call");
            return NULL;
        }
        e->as.call.handler.kind = HANDLE_TRY;
        e->as.call.handler.pos = parser_pos_of(t);
        return e;
    }
    default:
        return postfix(p);
    }
}

/* Every operand passes here, so a `(` or a prefix operator is one
   level. */
static struct expr *unary(struct parser *p)
{
    struct expr *e;

    if (!parser_descend(p)) {
        return NULL;
    }
    e = unary_level(p);
    parser_ascend(p);
    return e;
}

static struct expr *cast_links(struct parser *p)
{
    struct expr *e = unary(p);

    while (e != NULL &&
           (parser_check(p, TOKEN_AS) || parser_check(p, TOKEN_IS))) {
        bool test = parser_check(p, TOKEN_IS);
        struct expr *c = parser_new_expr(p, EXPR_CAST, parser_next(p));
        c->pos = e->pos;
        c->as.cast.operand = e;
        c->as.cast.test = test;
        /* `as?` on a class pointer gives `none` where `as` traps. A `?`
           before `fn` opens a nullable function type, which `as?` never
           takes, so the type keeps it. */
        c->as.cast.checked = !test && parser_peek_at(p, 1)->kind != TOKEN_FN &&
                             parser_accept(p, TOKEN_QUESTION);
        if ((c->as.cast.type = parser_type(p)) == NULL) {
            return NULL;
        }
        /* `v is geo.Shape.Circle` names the case of a variant of another
           module, one name more than a type has. */
        if (test && c->as.cast.type->kind == TYPEX_NAMED &&
            c->as.cast.type->module.length > 0 && parser_check(p, TOKEN_DOT) &&
            parser_peek_at(p, 1)->kind == TOKEN_IDENT) {
            parser_next(p);
            parser_expect_name(p, &c->as.cast.type->member);
        }
        if (!parser_chain_link(p)) {
            return NULL;
        }
        e = c;
    }
    return e;
}

/* unary { "as" type }: as binds tighter than the binary operators. */
static struct expr *cast(struct parser *p)
{
    struct chain c;
    struct expr *e;

    parser_chain_begin(p, &c);
    e = cast_links(p);
    parser_chain_end(p, &c);
    return e;
}

/* The precedence of a binary operator, from 1 for || to 11 for * / %.
   0 means the token is no binary operator. */
int parser_precedence(enum token_kind kind)
{
    switch (kind) {
    case TOKEN_OR_OR: return 1;
    case TOKEN_AND_AND: return 2;
    case TOKEN_PIPE: return 3;
    case TOKEN_CARET: return 4;
    case TOKEN_AMP: return 5;
    case TOKEN_EQ:
    case TOKEN_NE: return 6;
    case TOKEN_LT:
    case TOKEN_LE:
    case TOKEN_GT:
    case TOKEN_GE: return 7;
    /* DESIGN: `??` binds tighter than the comparisons, so `p ?? q == r`
       compares the pointer it gives. Its operands are pointers, which
       take none of the operators that bind tighter. */
    case TOKEN_QUESTION_QUESTION: return 8;
    /* The wrapping and saturating operators bind as the plain operator
       they extend. */
    case TOKEN_SHL:
    case TOKEN_SHL_WRAP:
    case TOKEN_SHR: return 9;
    case TOKEN_PLUS:
    case TOKEN_PLUS_WRAP:
    case TOKEN_PLUS_SAT:
    case TOKEN_MINUS:
    case TOKEN_MINUS_WRAP:
    case TOKEN_MINUS_SAT: return 10;
    case TOKEN_STAR:
    case TOKEN_STAR_WRAP:
    case TOKEN_STAR_SAT:
    case TOKEN_SLASH:
    case TOKEN_PERCENT: return 11;
    default: return 0;
    }
}

static struct expr *binary(struct parser *p, int min);

/* DESIGN: `x in lo..hi` binds as `<` does, since it is the two
   comparisons `x >= lo && x < hi`. Each bound takes the operators that
   bind tighter, so `i + 1 in 0..n * 2` needs no parentheses. A range
   stands here and after `for`, and nowhere else in an expression. */
static struct expr *in_range(struct parser *p, struct expr *value)
{
    struct expr *e = parser_new_expr(p, EXPR_IN, parser_next(p));
    int bound = parser_precedence(TOKEN_LT) + 1;

    e->pos = value->pos;
    e->as.in.value = value;
    if ((e->as.in.low = binary(p, bound)) == NULL) {
        return NULL;
    }
    if (!parser_accept(p, TOKEN_DOT_DOT)) {
        parser_error_here(p, "`in` takes a range");
        return NULL;
    }
    e->as.in.high = binary(p, bound);
    return e->as.in.high != NULL ? e : NULL;
}

/* Precedence climbing: parse operands and every operator that binds at
   least as tightly as min. The right operand only takes operators that
   bind tighter, which makes each level group from left to right. `??`
   groups from the right, so `a ?? b ?? c` is `a ?? (b ?? c)` and each
   operand but the last is a `?*T`. */
static struct expr *binary_links(struct parser *p, int min)
{
    struct expr *left = cast(p);

    while (left != NULL) {
        const struct token *op = parser_peek(p);
        struct expr *e;
        /* `in` is a contextual word, as it is after `for`. A name right
           after an operand can be nothing else. */
        if (parser_is_word(p, op, "in")) {
            if (parser_precedence(TOKEN_LT) < min) {
                break;
            }
            left = in_range(p, left);
            if (left != NULL && !parser_chain_link(p)) {
                return NULL;
            }
            continue;
        }
        if (parser_precedence(op->kind) == 0 ||
            parser_precedence(op->kind) < min) {
            break;
        }
        parser_next(p);
        e = parser_new_expr(p, EXPR_BINARY, op);
        e->pos = left->pos;
        e->as.binary.op = op->kind;
        e->as.binary.left = left;
        /* The right operand of `??` is one level deeper, since `??`
           groups from the right. Any other operator's right operand
           takes a higher precedence, which bounds its nesting. */
        if (op->kind == TOKEN_QUESTION_QUESTION) {
            if (!parser_descend(p)) {
                return NULL;
            }
            e->as.binary.right = binary(p, parser_precedence(op->kind));
            parser_ascend(p);
        } else {
            e->as.binary.right = binary(p, parser_precedence(op->kind) + 1);
        }
        if (e->as.binary.right == NULL || !parser_chain_link(p)) {
            return NULL;
        }
        left = e;
    }
    return left;
}

static struct expr *binary(struct parser *p, int min)
{
    struct chain c;
    struct expr *left;

    parser_chain_begin(p, &c);
    left = binary_links(p, min);
    parser_chain_end(p, &c);
    return left;
}

struct expr *parser_expression(struct parser *p)
{
    return binary(p, 1);
}

/* A condition may not hold a struct or slice literal outside
   parentheses, because its '{' would be read as the body. */
struct expr *parser_condition(struct parser *p)
{
    bool saved = p->no_struct_literal;
    struct expr *e;

    p->no_struct_literal = true;
    e = parser_expression(p);
    p->no_struct_literal = saved;
    return e;
}

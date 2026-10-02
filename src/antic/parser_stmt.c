/* Statements and blocks: `let` and `const`, `if` and `if let`, the arms
   of `switch` and `select`, the handlers of `catch` and every other
   statement. */

#include <stdlib.h>
#include <string.h>

#include "parser_parser.h"

bool parser_is_assign_op(enum token_kind kind)
{
    return (kind >= TOKEN_ASSIGN && kind <= TOKEN_SHR_ASSIGN) ||
           (kind >= TOKEN_PLUS_WRAP_ASSIGN && kind <= TOKEN_STAR_SAT_ASSIGN);
}

static struct stmt *new_stmt(struct parser *p, enum stmt_kind kind,
                             const struct token *at)
{
    struct stmt *s = parser_node(p, sizeof *s);

    s->kind = kind;
    s->pos = parser_pos_of(at);
    return s;
}

static bool read_handler(struct parser *p, struct handler *out,
                         bool required);

/* The names of a destructuring, `(a, b)`. The opening parenthesis is
   read, and the list holds two names or more. */
static struct binding *bindings(struct parser *p, size_t *count)
{
    struct list names = {NULL, 0, 0, sizeof(struct binding)};

    while (!parser_check(p, TOKEN_RPAREN)) {
        struct binding b;
        memset(&b, 0, sizeof b);
        b.pos = parser_pos_of(parser_peek(p));
        if (!parser_expect_name(p, &b.name)) {
            free(names.data);
            return NULL;
        }
        parser_list_push(&names, &b);
        if (!parser_accept(p, TOKEN_COMMA)) {
            break;
        }
    }
    if (names.count < 2) {
        parser_error_here(p, "a destructuring names two elements or more");
        free(names.data);
        return NULL;
    }
    if (!parser_expect(p, TOKEN_RPAREN)) {
        free(names.data);
        return NULL;
    }
    return parser_list_finish(p, &names, count);
}

static struct stmt *let_or_const(struct parser *p)
{
    const struct token *t = parser_next(p);
    struct stmt *s = new_stmt(p, t->kind == TOKEN_LET ? STMT_LET : STMT_CONST,
                              t);

    s->as.let.name_pos = parser_pos_of(parser_peek(p));
    /* `let (a, b) = e;` takes a tuple apart. It binds no name of its
       own, and the type after a name is therefore the one-name form's. */
    if (t->kind == TOKEN_LET && parser_accept(p, TOKEN_LPAREN)) {
        s->as.let.names = bindings(p, &s->as.let.name_count);
        if (s->as.let.names == NULL) {
            return NULL;
        }
        if (!parser_expect(p, TOKEN_ASSIGN) ||
            (s->as.let.value = parser_expression(p)) == NULL) {
            return NULL;
        }
        if (parser_check(p, TOKEN_CATCH) &&
            !read_handler(p, s->as.let.value->kind == EXPR_CALL
                                 ? &s->as.let.value->as.call.handler
                                 : &s->as.let.guard,
                          false)) {
            return NULL;
        }
        return parser_expect(p, TOKEN_SEMICOLON) ? s : NULL;
    }
    if (!parser_expect_name(p, &s->as.let.name)) {
        return NULL;
    }
    if (t->kind == TOKEN_CONST ? !parser_expect(p, TOKEN_COLON)
                               : !parser_accept(p, TOKEN_COLON)) {
        if (p->panic) {
            return NULL;
        }
    } else {
        /* DESIGN: `atomic` before the type of a local makes it an
           atomic local, which the atomic operations alone reach. */
        if (t->kind == TOKEN_LET && parser_check(p, TOKEN_ATOMIC)) {
            parser_next(p);
            s->as.let.atomic = true;
        }
        if ((s->as.let.type = parser_type(p)) == NULL) {
            return NULL;
        }
    }
    if (!parser_expect(p, TOKEN_ASSIGN) ||
        (s->as.let.value = parser_expression(p)) == NULL) {
        return NULL;
    }
    /* `let n = f(args) catch e { ... };` handles the error of the call
       that gives n its value. `alloc T(args)` carries the handler of the
       `construct` it runs. */
    if (parser_check(p, TOKEN_CATCH)) {
        struct expr *v = s->as.let.value;
        if (v->kind == EXPR_ALLOC && v->as.alloc.value != NULL) {
            v = v->as.alloc.value;
        }
        /* A `catch` on anything but a call guards a `?*T`, and the
           checker refuses a value that is neither. The two forms read
           alike. A call that cannot fail is a guard as well, and the
           checker decides which of the two it holds. */
        if (!read_handler(p, v->kind == EXPR_CALL ? &v->as.call.handler
                                                  : &s->as.let.guard,
                          false)) {
            return NULL;
        }
    }
    /* `let m = p else { }` binds m as `*T` and runs the block when p is
       `none`. The block leaves the block the `let` stands in, so the
       name below it is bound on every path. */
    if (parser_accept(p, TOKEN_ELSE) &&
        (s->as.let.otherwise = parser_block(p)) == NULL) {
        return NULL;
    }
    return parser_expect(p, TOKEN_SEMICOLON) ? s : NULL;
}

/* DESIGN: the arms of a switch are separated by commas, so the body of an
   arm carries no `;` of its own. It is a block, or one assignment or call
   that the comma or the closing brace ends. Anything longer takes a
   block. */
/* The refusal of an arm body that is none of the three forms. */
#define SWITCH_ARM "an arm of a `switch` is a call, an assignment or a block"
#define SELECT_ARM "an arm of a `select` is a call, an assignment or a block"

static struct stmt *arm_body(struct parser *p, const char *refusal)
{
    const struct token *t = parser_peek(p);
    struct stmt *s;
    struct expr *e;

    if (parser_check(p, TOKEN_LBRACE)) {
        s = new_stmt(p, STMT_BLOCK, t);
        return (s->as.block = parser_block(p)) == NULL ? NULL : s;
    }
    if ((e = parser_expression(p)) == NULL) {
        return NULL;
    }
    if (parser_is_assign_op(parser_peek(p)->kind)) {
        s = new_stmt(p, STMT_ASSIGN, t);
        s->as.assign.op = parser_next(p)->kind;
        s->as.assign.target = e;
        return (s->as.assign.value = parser_expression(p)) == NULL ? NULL : s;
    }
    if (e->kind != EXPR_CALL && e->kind != EXPR_SYNC_OP) {
        parser_error_here(p, "%s", refusal);
        return NULL;
    }
    s = new_stmt(p, STMT_EXPR, t);
    s->as.expr = e;
    return s;
}

static struct stmt *if_statement(struct parser *p);

/* A statement as a block of its own, for the `else` of an `if let`. */
static struct block *block_of(struct parser *p, struct stmt *s)
{
    struct block *b = parser_node(p, sizeof *b);

    b->pos = s->pos;
    b->end = s->pos;
    b->stmts = parser_node(p, sizeof *b->stmts);
    b->stmts[0] = s;
    b->count = 1;
    return b;
}

/* `if let Circle c = s { } else { }`, after `if`. The `else` is empty
   when the text writes none, and `else if` holds the rest of the chain
   as one statement. */
static struct stmt *if_let(struct parser *p, const struct token *t)
{
    struct stmt *s = new_stmt(p, STMT_SWITCH, t);
    struct switch_arm *arm = parser_node(p, sizeof *arm);
    struct stmt *body;
    struct stmt *otherwise;

    parser_next(p);
    s->as.switch_stmt.if_let = true;
    arm->pos = parser_pos_of(parser_peek(p));
    arm->value = parser_new_expr(p, EXPR_NAME, parser_peek(p));
    if (!parser_expect_name(p, &arm->value->as.name)) {
        return NULL;
    }
    if (parser_check(p, TOKEN_IDENT)) {
        arm->binds_pos = parser_pos_of(parser_peek(p));
        parser_expect_name(p, &arm->binds);
    }
    if (!parser_expect(p, TOKEN_ASSIGN)) {
        return NULL;
    }
    p->no_struct_literal = true;
    s->as.switch_stmt.value = parser_expression(p);
    p->no_struct_literal = false;
    body = new_stmt(p, STMT_BLOCK, parser_peek(p));
    if (s->as.switch_stmt.value == NULL ||
        (body->as.block = parser_block(p)) == NULL) {
        return NULL;
    }
    arm->body = body;
    otherwise = new_stmt(p, STMT_BLOCK, parser_peek(p));
    if (!parser_accept(p, TOKEN_ELSE)) {
        otherwise->as.block = parser_node(p, sizeof *otherwise->as.block);
        otherwise->as.block->pos = otherwise->pos;
        otherwise->as.block->end = otherwise->pos;
    } else if (parser_check(p, TOKEN_IF)) {
        struct stmt *rest = if_statement(p);
        if (rest == NULL) {
            return NULL;
        }
        otherwise->as.block = block_of(p, rest);
    } else if ((otherwise->as.block = parser_block(p)) == NULL) {
        return NULL;
    }
    s->as.switch_stmt.arms = arm;
    s->as.switch_stmt.count = 1;
    s->as.switch_stmt.otherwise = otherwise;
    s->as.switch_stmt.otherwise_at = 1;
    return s;
}

static struct stmt *if_level(struct parser *p)
{
    const struct token *t = parser_next(p);
    struct stmt *s;
    struct list branches = {NULL, 0, 0, sizeof(struct if_branch)};

    if (parser_check(p, TOKEN_LET)) {
        return if_let(p, t);
    }
    s = new_stmt(p, STMT_IF, t);
    for (;;) {
        struct if_branch b;
        if ((b.cond = parser_condition(p)) == NULL ||
            (b.body = parser_block(p)) == NULL) {
            free(branches.data);
            return NULL;
        }
        parser_list_push(&branches, &b);
        if (!parser_accept(p, TOKEN_ELSE)) {
            break;
        }
        /* `else if let` ends the chain with an `if let`, which holds the
           rest of it. */
        if (parser_check(p, TOKEN_IF) &&
            parser_peek_at(p, 1)->kind == TOKEN_LET) {
            struct stmt *rest = if_statement(p);
            if (rest == NULL) {
                free(branches.data);
                return NULL;
            }
            s->as.if_chain.else_body = block_of(p, rest);
            break;
        }
        if (!parser_accept(p, TOKEN_IF)) {
            if ((s->as.if_chain.else_body = parser_block(p)) == NULL) {
                free(branches.data);
                return NULL;
            }
            break;
        }
    }
    s->as.if_chain.branches = parser_list_finish(p, &branches,
                                                 &s->as.if_chain.count);
    return s;
}

/* An `else if let` holds the rest of its chain, so each is one level. */
static struct stmt *if_statement(struct parser *p)
{
    struct stmt *s;

    if (!parser_descend(p)) {
        return NULL;
    }
    s = if_level(p);
    parser_ascend(p);
    return s;
}

/* `catch e { }`, `catch { }` or `catch fatal`. With required set the
   handler must be there. */
static bool read_handler(struct parser *p, struct handler *out, bool required)
{
    memset(out, 0, sizeof *out);
    out->pos = parser_pos_of(parser_peek(p));
    if (!parser_accept(p, TOKEN_CATCH)) {
        if (required) {
            parser_error_here(p, "expected `catch`");
            return false;
        }
        return true;
    }
    if (parser_is_word(p, parser_peek(p), "fatal")) {
        parser_next(p);
        out->kind = HANDLE_FATAL;
        return true;
    }
    /* DESIGN: `catch none` counts a failure as `none`. It is the handler
       `catch { yield none; }`, which deletes the error on its way out as
       every handler does, so the parser writes that block and the
       checker refuses it where the result cannot be `none`. */
    if (parser_check(p, TOKEN_NONE)) {
        const struct token *t = parser_next(p);
        struct stmt *yield = new_stmt(p, STMT_YIELD, t);
        yield->as.yielded = parser_new_expr(p, EXPR_NONE, t);
        out->kind = HANDLE_BLOCK;
        out->none = true;
        out->body = parser_node(p, sizeof *out->body);
        out->body->pos = parser_pos_of(t);
        out->body->end = parser_pos_of(t);
        out->body->stmts = parser_node(p, sizeof *out->body->stmts);
        out->body->stmts[0] = yield;
        out->body->count = 1;
        return true;
    }
    out->kind = HANDLE_BLOCK;
    if (parser_peek(p)->kind == TOKEN_IDENT) {
        out->pos = parser_pos_of(parser_peek(p));
        if (!parser_expect_name(p, &out->name)) {
            return false;
        }
    }
    return (out->body = parser_block(p)) != NULL;
}

/* Whether the `for` at the current token names a tuple pattern, `for (k,
   v) in e`. The pattern is a parenthesis, two names or more apart by
   commas, the closing parenthesis and `in`. A range that begins with a
   parenthesis names no variable. */
static bool tuple_pattern(struct parser *p)
{
    size_t at = 2;

    if (parser_peek_at(p, 1)->kind != TOKEN_LPAREN) {
        return false;
    }
    for (;;) {
        if (parser_peek_at(p, at)->kind != TOKEN_IDENT) {
            return false;
        }
        if (parser_peek_at(p, at + 1)->kind != TOKEN_COMMA) {
            break;
        }
        at += 2;
    }
    return at > 2 && parser_peek_at(p, at + 1)->kind == TOKEN_RPAREN &&
           parser_is_word(p, parser_peek_at(p, at + 2), "in");
}

static struct stmt *statement(struct parser *p);

static struct stmt *statement_level(struct parser *p)
{
    const struct token *t = parser_peek(p);
    struct stmt *s;

    switch (t->kind) {
    case TOKEN_LET:
    case TOKEN_CONST:
        return let_or_const(p);
    case TOKEN_IF:
        return if_statement(p);
    case TOKEN_WHILE:
        parser_next(p);
        s = new_stmt(p, STMT_WHILE, t);
        if ((s->as.loop.cond = parser_condition(p)) == NULL ||
            !parser_expect(p, TOKEN_DO) ||
            (s->as.loop.body = parser_block(p)) == NULL) {
            return NULL;
        }
        return s;
    /* DESIGN: `for i in lo..hi` and `for x in slice` read one variable,
       a range or a sequence, and a block. The checker and lowering give
       them the loop that the core would have written. `for i, x in items`
       names two, which is the destructuring of the `(int, T)` of each
       element. */
    case TOKEN_FOR: {
        size_t from;
        /* DESIGN: the binding is optional over a range, because a loop
           that repeats a block needs no counter. A name and `in` open the
           bound form, and anything else is the range itself. */
        bool bound = parser_peek_at(p, 1)->kind == TOKEN_IDENT &&
                     (parser_is_word(p, parser_peek_at(p, 2), "in") ||
                      (parser_peek_at(p, 2)->kind == TOKEN_COMMA &&
                       parser_peek_at(p, 3)->kind == TOKEN_IDENT &&
                       parser_is_word(p, parser_peek_at(p, 4), "in")));
        bool pattern = tuple_pattern(p);
        parser_next(p);
        s = new_stmt(p, STMT_FOR, t);
        if (pattern) {
            parser_next(p);
        }
        s->as.for_loop.pattern = pattern;
        if (bound || pattern) {
            struct list names = {NULL, 0, 0, sizeof(struct binding)};
            do {
                struct binding b;
                memset(&b, 0, sizeof b);
                b.pos = parser_pos_of(parser_peek(p));
                if (!parser_expect_name(p, &b.name)) {
                    free(names.data);
                    return NULL;
                }
                parser_list_push(&names, &b);
            } while (parser_accept(p, TOKEN_COMMA));
            s->as.for_loop.names =
                parser_list_finish(p, &names, &s->as.for_loop.name_count);
            if (pattern) {
                parser_next(p);
            }
            /* DESIGN: `in` is a contextual word, as `packed` and `align`
               are. The decision adds four keywords and `in` is not among
               them, so a program may still name a variable `in`. */
            parser_next(p);
        }
        p->no_struct_literal = true;
        from = parser_peek_at(p, parser_check(p, TOKEN_AMP) ? 1 : 0)->offset;
        if (parser_accept(p, TOKEN_AMP)) {
            s->as.for_loop.by_pointer = true;
            s->as.for_loop.over = parser_expression(p);
        } else if ((s->as.for_loop.low = parser_expression(p)) != NULL &&
                   parser_accept(p, TOKEN_DOT_DOT)) {
            s->as.for_loop.high = parser_expression(p);
        } else {
            s->as.for_loop.over = s->as.for_loop.low;
            s->as.for_loop.low = NULL;
        }
        /* The source of the walked expression, which the refusal of a
           change to a copy and the trap of a dev build name. */
        if (s->as.for_loop.over != NULL) {
            size_t to = p->all[p->origin[p->pos - 1]].offset +
                        p->all[p->origin[p->pos - 1]].length;
            s->as.for_loop.over_text.bytes = p->source + from;
            s->as.for_loop.over_text.length = to > from ? to - from : 0;
        }
        /* `by k` steps a range. It is the contextual word that `parallel`
           uses for its chunk count. */
        if (s->as.for_loop.high != NULL &&
            parser_is_word(p, parser_peek(p), "by")) {
            parser_next(p);
            s->as.for_loop.step_pos = parser_pos_of(parser_peek(p));
            s->as.for_loop.step = parser_expression(p);
        }
        p->no_struct_literal = false;
        if (p->panic ||
            (s->as.for_loop.over == NULL && s->as.for_loop.high == NULL)) {
            return NULL;
        }
        if ((s->as.for_loop.body = parser_block(p)) == NULL) {
            return NULL;
        }
        return s;
    }
    case TOKEN_DO:
        parser_next(p);
        s = new_stmt(p, STMT_DO_WHILE, t);
        if ((s->as.loop.body = parser_block(p)) == NULL ||
            !parser_expect(p, TOKEN_WHILE) ||
            (s->as.loop.cond = parser_condition(p)) == NULL) {
            return NULL;
        }
        return s;
    /* DESIGN: `assert(cond)` keeps the source of its condition, so the
       failure names what was asserted. A release build removes the whole
       statement, which is why a call in the condition warns. */
    case TOKEN_ASSERT: {
        size_t from;
        size_t to;
        parser_next(p);
        s = new_stmt(p, STMT_ASSERT, t);
        if (!parser_expect(p, TOKEN_LPAREN)) {
            return NULL;
        }
        from = parser_peek(p)->offset;
        if ((s->as.assertion.cond = parser_expression(p)) == NULL) {
            return NULL;
        }
        to = p->pos == 0 ? from : p->all[p->origin[p->pos - 1]].offset +
                                  p->all[p->origin[p->pos - 1]].length;
        s->as.assertion.text.bytes = p->source + from;
        s->as.assertion.text.length = to > from ? to - from : 0;
        if (parser_accept(p, TOKEN_COMMA)) {
            if (!parser_check(p, TOKEN_STRING)) {
                parser_error_here(p, "the message of an `assert` is a string "
                                     "literal");
                return NULL;
            }
            s->as.assertion.message = parser_peek(p)->value.text;
            parser_next(p);
        }
        return parser_expect(p, TOKEN_RPAREN) &&
                       parser_expect(p, TOKEN_SEMICOLON)
                   ? s
                   : NULL;
    }
    /* DESIGN: `switch e { A => stmt, else => stmt }` names one value per
       arm and runs one statement. An arm falls through only where it
       ends in `fallthrough;`, so no arm needs a break, and `else` takes
       the rest. The switch keeps the place the text gives `else`, since
       the arm after it is the one its `fallthrough` enters. */
    case TOKEN_SWITCH: {
        struct list arms = {NULL, 0, 0, sizeof(struct switch_arm)};
        parser_next(p);
        s = new_stmt(p, STMT_SWITCH, t);
        p->no_struct_literal = true;
        s->as.switch_stmt.value = parser_expression(p);
        p->no_struct_literal = false;
        if (s->as.switch_stmt.value == NULL ||
            !parser_expect(p, TOKEN_LBRACE)) {
            return NULL;
        }
        while (!parser_check(p, TOKEN_RBRACE) && !parser_check(p, TOKEN_EOF)) {
            struct switch_arm arm;
            memset(&arm, 0, sizeof arm);
            arm.pos = parser_pos_of(parser_peek(p));
            if (parser_accept(p, TOKEN_ELSE)) {
                if (s->as.switch_stmt.otherwise != NULL) {
                    parser_error_here(p, "a `switch` has one `else`");
                    free(arms.data);
                    return NULL;
                }
                s->as.switch_stmt.otherwise_at = arms.count;
                if (!parser_expect(p, TOKEN_FAT_ARROW) ||
                    (s->as.switch_stmt.otherwise =
                         arm_body(p, SWITCH_ARM)) == NULL) {
                    free(arms.data);
                    return NULL;
                }
            } else {
                /* `Circle c =>` binds the fields of a case of a variant
                   to c. The case is a name, which the checker reads. */
                if (parser_check(p, TOKEN_IDENT) &&
                    parser_peek_at(p, 1)->kind == TOKEN_IDENT &&
                    parser_peek_at(p, 2)->kind == TOKEN_FAT_ARROW) {
                    arm.value = parser_new_expr(p, EXPR_NAME, parser_peek(p));
                    parser_expect_name(p, &arm.value->as.name);
                    arm.binds_pos = parser_pos_of(parser_peek(p));
                    parser_expect_name(p, &arm.binds);
                }
                if ((arm.value == NULL &&
                     (arm.value = parser_expression(p)) == NULL) ||
                    !parser_expect(p, TOKEN_FAT_ARROW) ||
                    (arm.body = arm_body(p, SWITCH_ARM)) == NULL) {
                    free(arms.data);
                    return NULL;
                }
                parser_list_push(&arms, &arm);
            }
            if (!parser_accept(p, TOKEN_COMMA)) {
                break;
            }
        }
        s->as.switch_stmt.arms = parser_list_finish(p, &arms,
                                                    &s->as.switch_stmt.count);
        return parser_expect(p, TOKEN_RBRACE) ? s : NULL;
    }
    /* DESIGN: `sync m { }` holds the mutex m for the block. The operand
       is read as a condition is, so its `{` opens the block. `sync a, b
       { }` names two synchronized objects, whose locks the runtime takes
       in an order of its own. */
    case TOKEN_SYNC:
        parser_next(p);
        s = new_stmt(p, STMT_SYNC, t);
        if ((s->as.sync.mutex = parser_condition(p)) == NULL) {
            return NULL;
        }
        if (parser_accept(p, TOKEN_COMMA) &&
            (s->as.sync.second = parser_condition(p)) == NULL) {
            return NULL;
        }
        if ((s->as.sync.body = parser_block(p)) == NULL) {
            return NULL;
        }
        return s;
    /* DESIGN: `select { a x => stmt, b => stmt }` is written like a
       `switch` over the channels. Each arm names a channel and, before
       the arrow, the name that takes what the channel gives. It has no
       `else`, since it waits until one of its channels has a value or is
       closed. */
    case TOKEN_SELECT: {
        struct list arms = {NULL, 0, 0, sizeof(struct switch_arm)};
        parser_next(p);
        s = new_stmt(p, STMT_SELECT, t);
        if (!parser_expect(p, TOKEN_LBRACE)) {
            return NULL;
        }
        if (parser_check(p, TOKEN_RBRACE)) {
            parser_error_here(p, "a `select` waits on one channel or more");
            return NULL;
        }
        while (!parser_check(p, TOKEN_RBRACE) && !parser_check(p, TOKEN_EOF)) {
            struct switch_arm arm;
            memset(&arm, 0, sizeof arm);
            arm.pos = parser_pos_of(parser_peek(p));
            if (parser_check(p, TOKEN_ELSE)) {
                parser_error_here(p, "a `select` has no `else`");
                free(arms.data);
                return NULL;
            }
            if ((arm.value = parser_expression(p)) == NULL) {
                free(arms.data);
                return NULL;
            }
            if (parser_check(p, TOKEN_IDENT) &&
                parser_peek_at(p, 1)->kind == TOKEN_FAT_ARROW) {
                arm.binds_pos = parser_pos_of(parser_peek(p));
                parser_expect_name(p, &arm.binds);
            }
            if (!parser_expect(p, TOKEN_FAT_ARROW) ||
                (arm.body = arm_body(p, SELECT_ARM)) == NULL) {
                free(arms.data);
                return NULL;
            }
            parser_list_push(&arms, &arm);
            if (!parser_accept(p, TOKEN_COMMA)) {
                break;
            }
        }
        s->as.select.arms = parser_list_finish(p, &arms, &s->as.select.count);
        return parser_expect(p, TOKEN_RBRACE) ? s : NULL;
    }
    /* DESIGN: `defer stmt;` records a statement that runs at every exit
       of the enclosing block, in reverse order of declaration. */
    case TOKEN_DEFER:
        parser_next(p);
        s = new_stmt(p, STMT_DEFER, t);
        if ((s->as.deferred = statement(p)) == NULL) {
            return NULL;
        }
        if (s->as.deferred->kind == STMT_DEFER) {
            parser_error_here(p, "a `defer` holds one statement, not another "
                                 "`defer`");
            return NULL;
        }
        return s;
    /* DESIGN: `undo stmt;` records a statement that runs only when the
       enclosing block leaves through an error, before the `defer`
       statements of the same block. `defer` is always, `undo` is only if
       this block fails. */
    case TOKEN_UNDO:
        parser_next(p);
        s = new_stmt(p, STMT_UNDO, t);
        if ((s->as.deferred = statement(p)) == NULL) {
            return NULL;
        }
        if (s->as.deferred->kind == STMT_UNDO) {
            parser_error_here(p, "an `undo` holds one statement, not another "
                                 "`undo`");
            return NULL;
        }
        return s;
    /* `fail e;` leaves on the error channel, and `fail "text";` is
       `fail Error.new(0, "text");`. */
    case TOKEN_FAIL:
        parser_next(p);
        s = new_stmt(p, STMT_FAIL, t);
        if ((s->as.fail.value = parser_expression(p)) == NULL) {
            return NULL;
        }
        return parser_expect(p, TOKEN_SEMICOLON) ? s : NULL;
    case TOKEN_BREAK:
    case TOKEN_CONTINUE:
        parser_next(p);
        s = new_stmt(p, t->kind == TOKEN_BREAK ? STMT_BREAK : STMT_CONTINUE, t);
        return parser_expect(p, TOKEN_SEMICOLON) ? s : NULL;
    /* The checker decides where `fallthrough;` may stand, since the
       grammar of a block does not know that the block is an arm. */
    case TOKEN_FALLTHROUGH:
        parser_next(p);
        s = new_stmt(p, STMT_FALLTHROUGH, t);
        return parser_expect(p, TOKEN_SEMICOLON) ? s : NULL;
    case TOKEN_RETURN:
        parser_next(p);
        s = new_stmt(p, STMT_RETURN, t);
        if (!parser_check(p, TOKEN_SEMICOLON) &&
            (s->as.return_value = parser_expression(p)) == NULL) {
            return NULL;
        }
        return parser_expect(p, TOKEN_SEMICOLON) ? s : NULL;
    case TOKEN_LBRACE:
        s = new_stmt(p, STMT_BLOCK, t);
        return (s->as.block = parser_block(p)) != NULL ? s : NULL;
    /* `yield v;` ends a handler with the value that takes the place of
       the result the failing call would have given. */
    case TOKEN_YIELD:
        parser_next(p);
        s = new_stmt(p, STMT_YIELD, t);
        if (!parser_check(p, TOKEN_SEMICOLON) &&
            (s->as.yielded = parser_expression(p)) == NULL) {
            return NULL;
        }
        return parser_expect(p, TOKEN_SEMICOLON) ? s : NULL;
    /* `try { } catch e { }` handles every failing call of the block. */
    case TOKEN_TRY:
        if (parser_peek_at(p, 1)->kind != TOKEN_LBRACE) {
            break;
        }
        parser_next(p);
        s = new_stmt(p, STMT_TRY, t);
        if ((s->as.try_block.body = parser_block(p)) == NULL ||
            !read_handler(p, &s->as.try_block.handler, true)) {
            return NULL;
        }
        return s;
    default:
        break;
    }

    {
        struct expr *e = parser_expression(p);
        if (e == NULL) {
            return NULL;
        }
        if (parser_is_assign_op(parser_peek(p)->kind)) {
            s = new_stmt(p, STMT_ASSIGN, t);
            s->as.assign.op = parser_next(p)->kind;
            s->as.assign.target = e;
            if ((s->as.assign.value = parser_expression(p)) == NULL) {
                return NULL;
            }
            /* `*p = f(args) catch e { ... };` handles the error of the
               call that fills the place, as the `let` of the same call
               does. The two forms of a failing call stand wherever the
               call stands, so neither `try` nor `catch` has a statement
               it is missing from. A `catch` on anything else guards a
               pointer and binds the one it proved, which an assignment
               has no name for. */
            if (parser_check(p, TOKEN_CATCH)) {
                struct expr *v = s->as.assign.value;
                if (v->kind != EXPR_CALL) {
                    parser_error_here(p, "`catch` stands after a call here, "
                                         "and a `catch` that guards a pointer "
                                         "stands in a `let`");
                    return NULL;
                }
                if (!read_handler(p, &v->as.call.handler, false)) {
                    return NULL;
                }
            }
        } else {
            /* A call that can fail carries its handler here. */
            if (e->kind == EXPR_CALL && parser_check(p, TOKEN_CATCH) &&
                !read_handler(p, &e->as.call.handler, false)) {
                return NULL;
            }
            if (!parser_check(p, TOKEN_SEMICOLON)) {
                parser_expect(p, TOKEN_SEMICOLON);
                return NULL;
            }
            if (e->kind != EXPR_CALL && e->kind != EXPR_FREE &&
                e->kind != EXPR_OBJECT && e->kind != EXPR_JOIN &&
                e->kind != EXPR_SYNC_OP) {
                parser_error_here(p, "expected a call or an assignment");
                return NULL;
            }
            s = new_stmt(p, STMT_EXPR, t);
            s->as.expr = e;
        }
        return parser_expect(p, TOKEN_SEMICOLON) ? s : NULL;
    }
}

/* A nested block, `defer` and `undo` each hold a statement, so each
   statement is one level. */
static struct stmt *statement(struct parser *p)
{
    size_t mark = p->clauses->count;
    struct pos from = parser_pos_of(parser_peek(p));
    struct stmt *s = NULL;

    if (!parser_descend(p)) {
        return NULL;
    }
    while (parser_statement_clause(p)) {
        if (!parser_read_clause(p, CLAUSE_STATEMENT)) {
            parser_ascend(p);
            return NULL;
        }
    }
    if (p->clauses->count > mark &&
        (parser_check(p, TOKEN_RBRACE) || parser_check(p, TOKEN_EOF))) {
        parser_error_here(p, "a clause of a statement stands before the "
                             "statement");
    } else {
        s = statement_level(p);
    }
    parser_ascend(p);
    parser_close_clauses(p, mark, from);
    return s;
}

struct block *parser_block(struct parser *p)
{
    struct block *b = parser_node(p, sizeof *b);
    struct list stmts = {NULL, 0, 0, sizeof(struct stmt *)};

    b->pos = parser_pos_of(parser_peek(p));
    if (!parser_expect(p, TOKEN_LBRACE)) {
        return NULL;
    }
    while (!parser_check(p, TOKEN_RBRACE) && !parser_check(p, TOKEN_EOF)) {
        struct stmt *s = statement(p);
        if (s != NULL) {
            parser_list_push(&stmts, &s);
        } else {
            parser_sync_statement(p);
        }
    }
    b->stmts = parser_list_finish(p, &stmts, &b->count);
    b->end = parser_pos_of(parser_peek(p));
    return parser_expect(p, TOKEN_RBRACE) ? b : NULL;
}

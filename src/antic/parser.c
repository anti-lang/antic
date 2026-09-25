#include "parser.h"

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "text.h"
#include "warnings.h"

/* DESIGN: recursive descent, one function per grammar rule of chapter 2,
   with precedence climbing for the binary operators. After an error the
   parser is in panic mode and reports nothing more. It skips to the start
   of a statement or an item, so one mistake produces one message. */

struct list;

struct parser {
    const char *source;
    const struct token *tokens;     /* without doc comments */
    const struct token *all;        /* with doc comments */
    size_t *origin;                 /* index in all of each token */
    bool *taken;                    /* the doc comments that were read */
    size_t pos;
    struct arena *arena;
    struct diagnostics *diags;
    bool panic;
    bool ok;
    bool no_struct_literal;         /* inside a condition */
    int depth;                      /* levels entered by descend */
    struct list *clauses;           /* every `allow` and `unchecked` */
    enum fn_block block;            /* the test block being read */
    /* DESIGN: `>>` closes two lists of type arguments. The inner list
       takes the first `>` and sets half, and the outer list takes the
       second and moves past the token. angles counts the lists open. */
    bool half;
    int angles;
};

/* A growable array of fixed-size elements, copied into the memory pool
   when the list is complete. */
struct list {
    void *data;
    size_t count;
    size_t capacity;
    size_t size;
};

static void list_push(struct list *l, const void *element)
{
    if (l->count == l->capacity) {
        size_t capacity = l->capacity == 0 ? 8 : l->capacity * 2;
        void *data = capacity <= SIZE_MAX / l->size
                         ? realloc(l->data, capacity * l->size)
                         : NULL;
        if (data == NULL) {
            fputs("antic: out of memory\n", stderr);
            exit(70);
        }
        l->data = data;
        l->capacity = capacity;
    }
    memcpy((char *)l->data + l->count * l->size, element, l->size);
    l->count++;
}

static void *list_finish(struct parser *p, struct list *l, size_t *count)
{
    void *copy = NULL;

    *count = l->count;
    if (l->count > 0) {
        copy = arena_alloc(p->arena, l->count * l->size);
        memcpy(copy, l->data, l->count * l->size);
    }
    free(l->data);
    l->data = NULL;
    return copy;
}

static const struct token *peek(const struct parser *p)
{
    return &p->tokens[p->pos];
}

static const struct token *peek_at(const struct parser *p, size_t ahead)
{
    size_t i = p->pos;

    while (ahead > 0 && p->tokens[i].kind != TOKEN_EOF) {
        i++;
        ahead--;
    }
    return &p->tokens[i];
}

static bool check(const struct parser *p, enum token_kind kind)
{
    return peek(p)->kind == kind;
}

static const struct token *next(struct parser *p)
{
    const struct token *t = peek(p);

    if (t->kind != TOKEN_EOF) {
        p->pos++;
    }
    return t;
}

static bool accept(struct parser *p, enum token_kind kind)
{
    if (check(p, kind)) {
        next(p);
        return true;
    }
    return false;
}

static struct pos pos_of(const struct token *t)
{
    struct pos pos = {t->line, t->column};
    return pos;
}

static void error_here(struct parser *p, const char *message)
{
    p->ok = false;
    if (p->panic) {
        return;
    }
    p->panic = true;
    diagnostics_add(p->diags, peek(p)->line, peek(p)->column, "%s", message);
}

/* Enter one level of nesting. Past PARSE_DEPTH_MAX it reports the
   source and returns false, and the caller returns NULL without
   entering. Each true return is paired with one ascend. */
static bool descend(struct parser *p)
{
    if (p->depth >= PARSE_DEPTH_MAX) {
        p->ok = false;
        if (!p->panic) {
            p->panic = true;
            diagnostics_add(p->diags, peek(p)->line, peek(p)->column,
                            "nesting deeper than %d levels",
                            PARSE_DEPTH_MAX);
        }
        return false;
    }
    p->depth++;
    return true;
}

static void ascend(struct parser *p)
{
    p->depth--;
}

static bool expect(struct parser *p, enum token_kind kind)
{
    char message[64];

    if (accept(p, kind)) {
        return true;
    }
    snprintf(message, sizeof message, "expected %s", token_kind_name(kind));
    error_here(p, message);
    return false;
}

static bool expect_name(struct parser *p, struct name *name)
{
    const struct token *t = peek(p);

    if (!expect(p, TOKEN_IDENT)) {
        return false;
    }
    name->text = p->source + t->offset;
    name->length = t->length;
    return true;
}

/* DESIGN: `alloc` and `free` name a function of a struct or class and
   follow `.`, so that `anti.mem.Allocator` has the two functions its
   work order names. They stay keywords everywhere else. A built-in
   `alloc` or `free` never stands after `fn` or `.`, so each position
   has one reading. */
/* Whether the token may be the name of an `inject` field: an ordinary
   name, or `alloc` or `free`. A name must stand after `inject`, so a
   built-in of either name never does. */
static bool is_field_name(const struct token *t)
{
    return t->kind == TOKEN_IDENT || t->kind == TOKEN_ALLOC ||
           t->kind == TOKEN_FREE;
}

static bool expect_member_name(struct parser *p, struct name *name)
{
    const struct token *t = peek(p);

    if (t->kind == TOKEN_ALLOC || t->kind == TOKEN_FREE) {
        next(p);
        name->text = p->source + t->offset;
        name->length = t->length;
        return true;
    }
    return expect_name(p, name);
}

static void *node(struct parser *p, size_t size)
{
    return arena_alloc(p->arena, size);
}

static bool is_doc(enum token_kind kind)
{
    return kind == TOKEN_DOC || kind == TOKEN_MODULE_DOC ||
           kind == TOKEN_NOTE || kind == TOKEN_MODULE_NOTE;
}

/* The length of the doc marker that opens the token text s of length
   bytes. It is 4 for the two developer markers of the module and 3 for
   the six others. Every marker has at least 3 bytes. */
static size_t marker_length(const char *s, size_t length)
{
    return length >= 4 && s[2] == '#' && s[3] == '!' ? 4 : 3;
}

/* The text of the doc comments of one kind between the previous token and
   the current one. DESIGN: two comments of one kind join with a blank
   line, the paragraph break of the doc markup, so no text is lost. */
static struct doc_text doc_before(struct parser *p, enum token_kind kind)
{
    struct doc_text doc = {NULL, 0, 0, 0};
    struct text joined = {0};
    size_t i = p->pos == 0 ? 0 : p->origin[p->pos - 1] + 1;
    char *copy;

    for (; i < p->origin[p->pos]; i++) {
        const struct token *t = &p->all[i];
        if (t->kind != kind) {
            continue;
        }
        p->taken[i] = true;
        if (joined.length > 0) {
            text_append(&joined, "\n\n");
        } else {
            doc.line = t->line;
            doc.column = t->column;
        }
        text_append_bytes(&joined, t->value.text.bytes, t->value.text.length);
    }
    if (joined.length > 0) {
        copy = node(p, joined.length + 1);
        memcpy(copy, text_cstr(&joined), joined.length + 1);
        doc.text = copy;
        doc.length = joined.length;
    }
    text_free(&joined);
    return doc;
}

/* Skip to the start of the next statement. That is past a semicolon, or
   up to a closing brace or a keyword that starts a statement. */
static void sync_statement(struct parser *p)
{
    p->half = false;
    p->angles = 0;
    for (;;) {
        switch (peek(p)->kind) {
        case TOKEN_SEMICOLON:
            next(p);
            p->panic = false;
            return;
        case TOKEN_RBRACE:
        case TOKEN_EOF:
        case TOKEN_LET:
        case TOKEN_CONST:
        case TOKEN_IF:
        case TOKEN_WHILE:
        case TOKEN_DO:
        case TOKEN_RETURN:
        case TOKEN_BREAK:
        case TOKEN_CONTINUE:
            p->panic = false;
            return;
        default:
            next(p);
        }
    }
}

/* Skip to the next item at the outermost brace level. */
static void sync_item(struct parser *p)
{
    int depth = 0;

    p->half = false;
    p->angles = 0;
    for (;;) {
        switch (peek(p)->kind) {
        case TOKEN_EOF:
            p->panic = false;
            return;
        case TOKEN_LBRACE:
            depth++;
            break;
        case TOKEN_RBRACE:
            if (depth > 0) {
                depth--;
            }
            break;
        case TOKEN_FN:
        case TOKEN_STRUCT:
        case TOKEN_UNION:
        case TOKEN_VARIANT:
        case TOKEN_EXTERN:
        case TOKEN_CONST:
        case TOKEN_CONSTRAINT:
        case TOKEN_TYPE:
        case TOKEN_PUB:
        case TOKEN_EXPORT:
        case TOKEN_IMPORT:
            if (depth == 0) {
                p->panic = false;
                return;
            }
            break;
        default:
            break;
        }
        next(p);
    }
}

static struct expr *expression(struct parser *p);
static struct expr *postfix(struct parser *p);
static bool is_word(const struct parser *p, const struct token *t,
                    const char *word);
static struct type_expr *type(struct parser *p);
static struct block *block(struct parser *p);
static struct expr *new_expr(struct parser *p, enum expr_kind kind,
                             const struct token *at);

/* DESIGN: `keep` and `concurrent` are contextual words before a
   parameter of function type, in a parameter list and in the list of a
   function type. A mark stands before a name followed by a colon, or
   before `fn` or `?fn`, so a parameter or a type may still carry either
   name. `own` after `keep` makes `keep own`, a parameter that keeps and
   owns what it is given. */
static bool is_fn_mark(const struct parser *p, size_t at)
{
    const struct token *t = peek_at(p, at);
    const struct token *after = peek_at(p, at + 1);

    if (!is_word(p, t, "keep") && !is_word(p, t, "concurrent")) {
        return false;
    }
    return after->kind == TOKEN_FN ||
           (after->kind == TOKEN_QUESTION &&
            peek_at(p, at + 2)->kind == TOKEN_FN) ||
           (after->kind == TOKEN_IDENT &&
            peek_at(p, at + 2)->kind == TOKEN_COLON) ||
           is_word(p, after, "keep") || is_word(p, after, "concurrent") ||
           (is_word(p, after, "own") &&
            (peek_at(p, at + 2)->kind == TOKEN_IDENT ||
             peek_at(p, at + 2)->kind == TOKEN_FN ||
             peek_at(p, at + 2)->kind == TOKEN_QUESTION));
}

/* Read the marks before a parameter. Both may stand, and the checker
   refuses the pair. `own` is read after a mark. */
static void fn_param_marks(struct parser *p, bool *keep, bool *concurrent,
                           bool *owned)
{
    while (is_fn_mark(p, 0)) {
        if (is_word(p, peek(p), "keep")) {
            *keep = true;
        } else {
            *concurrent = true;
        }
        next(p);
        if (is_word(p, peek(p), "own") &&
            (peek_at(p, 1)->kind == TOKEN_IDENT ||
             peek_at(p, 1)->kind == TOKEN_FN ||
             peek_at(p, 1)->kind == TOKEN_QUESTION)) {
            next(p);
            *owned = true;
        }
    }
}

/* `-> R` after the parameters of the function it, where R may be written
   `lent *T` or `lent ?*T`. It reports false after an error. `lent` is a
   contextual word, and no type name is followed by `*` or `?`. */
static bool result_type(struct parser *p, struct item *it)
{
    if (!accept(p, TOKEN_ARROW)) {
        return true;
    }
    if (is_word(p, peek(p), "lent") &&
        (peek_at(p, 1)->kind == TOKEN_STAR ||
         peek_at(p, 1)->kind == TOKEN_QUESTION ||
         peek_at(p, 1)->kind == TOKEN_QUESTION_STAR)) {
        it->result_lent = true;
        it->result_lent_pos = pos_of(peek(p));
        next(p);
    }
    return (it->result = type(p)) != NULL;
}

/* `lent name`: the pointer the parameter takes is valid only during the
   call. `lent` is a contextual word, so a parameter may carry that name. */
static bool lent_mark(struct parser *p)
{
    if (is_word(p, peek(p), "lent") && peek_at(p, 1)->kind == TOKEN_IDENT) {
        next(p);
        return true;
    }
    return false;
}

/* Types */

static bool is_builtin_type(enum token_kind kind)
{
    return kind >= TOKEN_BOOL_TYPE && kind <= TOKEN_C_WCHAR;
}

/* DESIGN: the rule of C# for `<` in an expression. The tokens from the
   `<` are read as a list of types without building anything. A scan
   holds the token it stands at, as a count ahead of the parser, and
   whether the first `>` of a `>>` there is taken. */
struct angle_scan {
    size_t at;
    bool half;
};

static bool scan_type(const struct parser *p, struct angle_scan *s);

/* Close one list at the scan: a `>`, the first `>` of a `>>`, or the
   second one that a list inside took the first of. */
static bool scan_close(const struct parser *p, struct angle_scan *s)
{
    enum token_kind k = peek_at(p, s->at)->kind;

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
        if (peek_at(p, s->at)->kind == TOKEN_INT) {
            s->at++;
        } else if (!scan_type(p, s)) {
            return false;
        }
        if (!s->half && peek_at(p, s->at)->kind == TOKEN_COMMA) {
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
    while (peek_at(p, s->at)->kind != TOKEN_RPAREN) {
        while (is_fn_mark(p, s->at) || is_word(p, peek_at(p, s->at), "own")) {
            s->at++;
        }
        if (s->half || !scan_type(p, s) || s->half) {
            return false;
        }
        if (peek_at(p, s->at)->kind != TOKEN_COMMA) {
            break;
        }
        s->at++;
    }
    if (peek_at(p, s->at)->kind != TOKEN_RPAREN) {
        return false;
    }
    s->at++;
    return true;
}

static bool scan_type(const struct parser *p, struct angle_scan *s)
{
    enum token_kind k = peek_at(p, s->at)->kind;

    if (is_builtin_type(k)) {
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
        if (peek_at(p, s->at)->kind == TOKEN_DOT &&
            peek_at(p, s->at + 1)->kind == TOKEN_IDENT) {
            s->at += 2;
        }
        return peek_at(p, s->at)->kind != TOKEN_LT || scan_list(p, s);
    case TOKEN_LBRACKET:
        s->at++;
        k = peek_at(p, s->at)->kind;
        if (k == TOKEN_INT || k == TOKEN_IDENT) {
            s->at++;
        }
        if (peek_at(p, s->at)->kind != TOKEN_RBRACKET) {
            return false;
        }
        s->at++;
        return scan_type(p, s);
    case TOKEN_LPAREN:
        return scan_types(p, s);
    case TOKEN_FN:
        s->at++;
        if (peek_at(p, s->at)->kind != TOKEN_LPAREN || !scan_types(p, s)) {
            return false;
        }
        if (peek_at(p, s->at)->kind == TOKEN_ARROW) {
            s->at++;
            if (!scan_type(p, s)) {
                return false;
            }
        }
        /* A failing function type ends with `may fail`. */
        if (!s->half && is_word(p, peek_at(p, s->at), "may") &&
            peek_at(p, s->at + 1)->kind == TOKEN_FAIL) {
            s->at += 2;
        }
        return true;
    default:
        return false;
    }
}

/* Where the list of type arguments whose `<` stands at ahead ends, as a
   count ahead of the parser, or 0. It is a list when its tokens read as
   types closed by `>` and the token after it is `(`, `.` or `{`. */
static size_t generic_end(const struct parser *p, size_t ahead)
{
    struct angle_scan s;
    enum token_kind after;

    if (peek_at(p, ahead)->kind != TOKEN_LT) {
        return 0;
    }
    s.at = ahead;
    s.half = false;
    if (!scan_list(p, &s) || s.half) {
        return 0;
    }
    after = peek_at(p, s.at)->kind;
    return after == TOKEN_LPAREN || after == TOKEN_DOT || after == TOKEN_LBRACE
               ? s.at
               : 0;
}

/* Close one list of type arguments, the `>` or one half of a `>>`. */
static bool close_angle(struct parser *p)
{
    if (p->half) {
        p->half = false;
        next(p);
        return true;
    }
    if (check(p, TOKEN_SHR)) {
        if (p->angles < 2) {
            error_here(p, "expected `>`");
            return false;
        }
        p->half = true;
        return true;
    }
    return expect(p, TOKEN_GT);
}

/* `<int, str>`: the type arguments after a name. An argument is a type,
   or an integer literal for a constant parameter. */
static struct type_expr **type_args(struct parser *p, size_t *count)
{
    struct list args = {NULL, 0, 0, sizeof(struct type_expr *)};
    bool ok = true;

    next(p);
    p->angles++;
    for (;;) {
        struct type_expr *arg;
        if (check(p, TOKEN_INT)) {
            const struct token *t = next(p);
            arg = node(p, sizeof *arg);
            arg->kind = TYPEX_CONST;
            arg->pos = pos_of(t);
            arg->length = new_expr(p, EXPR_INT, t);
            arg->length->as.integer = t->value.integer;
        } else if ((arg = type(p)) == NULL) {
            ok = false;
            break;
        }
        list_push(&args, &arg);
        if (!p->half && accept(p, TOKEN_COMMA)) {
            continue;
        }
        ok = close_angle(p);
        break;
    }
    p->angles--;
    if (!ok) {
        free(args.data);
        *count = 0;
        return NULL;
    }
    return list_finish(p, &args, count);
}

static struct type_expr *type_level(struct parser *p)
{
    const struct token *t = peek(p);
    struct type_expr *ty = node(p, sizeof *ty);

    ty->pos = pos_of(t);
    /* `?T` is a T or `none`, for any type. `?*T` is one token, and `?fn`
       stays the function type that may hold `none`, which the branches
       below read. `??T` is `?` twice. */
    if ((t->kind == TOKEN_QUESTION && peek_at(p, 1)->kind != TOKEN_FN) ||
        t->kind == TOKEN_QUESTION_QUESTION) {
        next(p);
        ty->kind = TYPEX_OPTIONAL;
        if ((ty->element = type(p)) == NULL) {
            return NULL;
        }
        if (t->kind == TOKEN_QUESTION_QUESTION) {
            struct type_expr *outer = node(p, sizeof *outer);
            outer->pos = ty->pos;
            outer->kind = TYPEX_OPTIONAL;
            outer->element = ty;
            ty = outer;
        }
        return ty;
    }
    if (is_builtin_type(t->kind)) {
        next(p);
        ty->kind = TYPEX_BUILTIN;
        ty->builtin = t->kind;
    } else if (t->kind == TOKEN_IDENT) {
        ty->kind = TYPEX_NAMED;
        expect_name(p, &ty->name);
        if (accept(p, TOKEN_DOT)) {
            ty->module = ty->name;
            if (!expect_name(p, &ty->name)) {
                return NULL;
            }
        }
        /* In a type, `<` after a name always opens type arguments. */
        if (check(p, TOKEN_LT) &&
            (ty->args = type_args(p, &ty->arg_count)) == NULL) {
            return NULL;
        }
    } else if (accept(p, TOKEN_CHAN)) {
        /* `chan T`, the channel of values of T. */
        ty->kind = TYPEX_CHAN;
        if ((ty->element = type(p)) == NULL) {
            return NULL;
        }
    } else if (check(p, TOKEN_STAR) || check(p, TOKEN_QUESTION_STAR)) {
        ty->nullable = accept(p, TOKEN_QUESTION_STAR);
        if (!ty->nullable) {
            next(p);
        }
        ty->kind = TYPEX_POINTER;
        if ((ty->element = type(p)) == NULL) {
            return NULL;
        }
    } else if (accept(p, TOKEN_LBRACKET)) {
        if (accept(p, TOKEN_RBRACKET)) {
            ty->kind = TYPEX_SLICE;
        } else {
            ty->kind = TYPEX_ARRAY;
            if ((ty->length = expression(p)) == NULL ||
                !expect(p, TOKEN_RBRACKET)) {
                return NULL;
            }
        }
        if ((ty->element = type(p)) == NULL) {
            return NULL;
        }
    } else if (accept(p, TOKEN_LPAREN)) {
        /* `(int, str)`, an anonymous struct with C layout. Two elements
           are the fewest that have no name of their own, so `()` and
           `(T)` are refused. */
        struct list elements = {NULL, 0, 0, sizeof(struct type_expr *)};

        ty->kind = TYPEX_TUPLE;
        while (!check(p, TOKEN_RPAREN)) {
            struct type_expr *element = type(p);
            if (element == NULL) {
                free(elements.data);
                return NULL;
            }
            list_push(&elements, &element);
            if (!accept(p, TOKEN_COMMA)) {
                break;
            }
        }
        ty->params = list_finish(p, &elements, &ty->param_count);
        if (ty->param_count < 2) {
            error_here(p, "a tuple has two or more elements");
            return NULL;
        }
        if (!expect(p, TOKEN_RPAREN)) {
            return NULL;
        }
    } else if (check(p, TOKEN_FN) ||
               (check(p, TOKEN_QUESTION) && peek_at(p, 1)->kind == TOKEN_FN)) {
        struct list params = {NULL, 0, 0, sizeof(struct type_expr *)};

        /* A function value follows the pointer rule, so `?fn(...)` may
           hold `none` and `fn(...)` may not. */
        ty->nullable = accept(p, TOKEN_QUESTION);
        next(p);
        ty->kind = TYPEX_FN;
        if (!expect(p, TOKEN_LPAREN)) {
            return NULL;
        }
        while (!check(p, TOKEN_RPAREN)) {
            bool keep = false;
            bool concurrent = false;
            bool owned = false;
            bool lent = false;
            struct type_expr *param;
            fn_param_marks(p, &keep, &concurrent, &owned);
            /* `fn(lent *T)`: `lent` is a contextual word, so a type of
               that name still stands alone in the list. */
            if (is_word(p, peek(p), "lent") &&
                peek_at(p, 1)->kind != TOKEN_COMMA &&
                peek_at(p, 1)->kind != TOKEN_RPAREN) {
                next(p);
                lent = true;
            }
            param = type(p);
            if (param == NULL) {
                free(params.data);
                return NULL;
            }
            param->keep = keep;
            param->concurrent = concurrent;
            param->owned = owned;
            param->lent = lent;
            list_push(&params, &param);
            if (!accept(p, TOKEN_COMMA)) {
                break;
            }
        }
        ty->params = list_finish(p, &params, &ty->param_count);
        if (!expect(p, TOKEN_RPAREN)) {
            return NULL;
        }
        if (accept(p, TOKEN_ARROW) && (ty->result = type(p)) == NULL) {
            return NULL;
        }
        /* DESIGN: `may fail` after a function type belongs to that type,
           the innermost one when a result is a function type in turn. A
           signature that fails itself and returns such a type writes the
           words twice. */
        if (is_word(p, peek(p), "may") && peek_at(p, 1)->kind == TOKEN_FAIL) {
            next(p);
            next(p);
            ty->may_fail = true;
        }
    } else {
        error_here(p, "expected a type");
        return NULL;
    }
    return ty;
}

static struct type_expr *type(struct parser *p)
{
    struct type_expr *ty;

    if (!descend(p)) {
        return NULL;
    }
    ty = type_level(p);
    ascend(p);
    return ty;
}

/* Expressions */

static struct expr *new_expr(struct parser *p, enum expr_kind kind,
                             const struct token *at)
{
    struct expr *e = node(p, sizeof *e);

    e->kind = kind;
    e->pos = pos_of(at);
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
    while (!check(p, TOKEN_RBRACE)) {
        struct field_init f;
        f.pos = pos_of(peek(p));
        if (!expect_name(p, &f.name) || !expect(p, TOKEN_COLON) ||
            (f.value = expression(p)) == NULL) {
            free(fields.data);
            p->no_struct_literal = saved;
            return NULL;
        }
        list_push(&fields, &f);
        if (!accept(p, TOKEN_COMMA)) {
            break;
        }
    }
    p->no_struct_literal = saved;
    if (!expect(p, TOKEN_RBRACE)) {
        free(fields.data);
        return NULL;
    }
    return list_finish(p, &fields, count);
}

/* A comma-separated list of expressions up to the closing token. */
static struct expr **expressions(struct parser *p, enum token_kind close,
                                 size_t *count)
{
    struct list items = {NULL, 0, 0, sizeof(struct expr *)};

    while (!check(p, close)) {
        struct expr *e = expression(p);
        if (e == NULL) {
            free(items.data);
            return NULL;
        }
        list_push(&items, &e);
        if (!accept(p, TOKEN_COMMA)) {
            break;
        }
    }
    if (!expect(p, close)) {
        free(items.data);
        return NULL;
    }
    return list_finish(p, &items, count);
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
   at the positions in the file. */
static struct expr *placeholder(struct parser *p,
                                const struct format_piece *piece)
{
    struct parser inner = *p;
    struct expr *e;

    inner.tokens = piece->tokens;
    inner.all = piece->tokens;
    inner.origin = NULL;
    inner.taken = NULL;
    inner.pos = 0;
    inner.panic = false;
    inner.ok = true;
    inner.no_struct_literal = false;
    e = expression(&inner);
    if (inner.ok && !check(&inner, TOKEN_EOF)) {
        error_here(&inner, "expected `}` or `:` after the expression");
    }
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
    struct expr *e = new_expr(p, EXPR_FORMAT, t);
    size_t n = t->value.format.count;
    struct format_part *parts = node(p, n * sizeof *parts);
    size_t i;

    next(p);
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

static void may_fail_after(struct parser *p, struct item *it);

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
    struct expr *e = new_expr(p, EXPR_FN, at);
    struct item *it = node(p, sizeof *it);
    bool saved = p->no_struct_literal;

    next(p);
    it->kind = ITEM_FN;
    it->pos = pos_of(at);
    it->name_pos = it->pos;
    it->name.text = p->source + at->offset;
    it->name.length = at->length;
    e->as.fn = it;
    if (!expect(p, TOKEN_LPAREN)) {
        return NULL;
    }
    while (!check(p, TOKEN_RPAREN)) {
        struct param param;
        memset(&param, 0, sizeof param);
        fn_param_marks(p, &param.keep, &param.concurrent, &param.owned);
        param.lent = lent_mark(p);
        param.pos = pos_of(peek(p));
        if (!expect_name(p, &param.name) ||
            (accept(p, TOKEN_COLON) && (param.type = type(p)) == NULL)) {
            free(list.data);
            return NULL;
        }
        list_push(&list, &param);
        if (!accept(p, TOKEN_COMMA)) {
            break;
        }
    }
    if (!expect(p, TOKEN_RPAREN)) {
        free(list.data);
        return NULL;
    }
    it->params = list_finish(p, &list, &it->param_count);
    if (!result_type(p, it)) {
        return NULL;
    }
    may_fail_after(p, it);
    p->no_struct_literal = false;
    it->body = block(p);
    p->no_struct_literal = saved;
    return it->body != NULL ? e : NULL;
}

static struct expr *primary(struct parser *p)
{
    const struct token *t = peek(p);
    struct expr *e;

    switch (t->kind) {
    case TOKEN_INT:
        next(p);
        e = new_expr(p, EXPR_INT, t);
        e->as.integer = t->value.integer;
        return e;
    case TOKEN_FLOAT:
    case TOKEN_STRING:
    case TOKEN_BYTES:
        next(p);
        e = new_expr(p, t->kind == TOKEN_FLOAT    ? EXPR_FLOAT
                        : t->kind == TOKEN_STRING ? EXPR_STRING
                                                  : EXPR_BYTES,
                     t);
        e->as.text = t->value.text;
        return e;
    case TOKEN_FORMAT:
        return format_literal(p, t);
    case TOKEN_PATTERN:
        next(p);
        e = new_expr(p, EXPR_PATTERN, t);
        e->as.text = t->value.text;
        return e;
    case TOKEN_CHAR:
        next(p);
        e = new_expr(p, EXPR_CHAR, t);
        e->as.character = t->value.character;
        return e;
    case TOKEN_TRUE:
    case TOKEN_FALSE:
        next(p);
        e = new_expr(p, EXPR_BOOL, t);
        e->as.boolean = t->kind == TOKEN_TRUE;
        return e;
    case TOKEN_NONE:
        next(p);
        return new_expr(p, EXPR_NONE, t);
    case TOKEN_HERE:
        next(p);
        return new_expr(p, EXPR_HERE, t);
    case TOKEN_FN:
        return anonymous_fn(p, t);
    /* `self` is the receiver of a function of a struct body. It reads as
       a name, and the checker gives it the type *T. */
    case TOKEN_SELF:
        next(p);
        e = new_expr(p, EXPR_NAME, t);
        e->as.name.text = p->source + t->offset;
        e->as.name.length = t->length;
        return e;
    case TOKEN_IDENT: {
        bool qualified;
        bool member;
        size_t end;
        /* `snapshot fn(...) { }` is an anonymous function that copies
           what it captures. `snapshot` is a contextual word. */
        if (is_word(p, t, "snapshot") && peek_at(p, 1)->kind == TOKEN_FN) {
            next(p);
            e = anonymous_fn(p, peek(p));
            if (e != NULL) {
                e->pos = pos_of(t);
                e->as.fn->snapshot = true;
            }
            return e;
        }
        /* `Pair<int, str> { }`, `max<int>(a, b)` and `List<int>.new()`
           name a generic with its type arguments, by the rule of C#. A
           variant's case follows its arguments,
           `Result<int, str>.Ok { }`. */
        end = generic_end(p, 1);
        if (end > 0) {
            if (!p->no_struct_literal &&
                (peek_at(p, end)->kind == TOKEN_LBRACE ||
                 (peek_at(p, end)->kind == TOKEN_DOT &&
                  peek_at(p, end + 1)->kind == TOKEN_IDENT &&
                  peek_at(p, end + 2)->kind == TOKEN_LBRACE))) {
                bool case_of = peek_at(p, end)->kind == TOKEN_DOT;
                e = new_expr(p, EXPR_STRUCT_LIT, t);
                expect_name(p, &e->as.struct_lit.name);
                e->type_args_pos = pos_of(peek(p));
                e->type_args = type_args(p, &e->type_arg_count);
                if (e->type_args == NULL) {
                    return NULL;
                }
                if (case_of) {
                    e->as.struct_lit.module = e->as.struct_lit.name;
                    next(p);
                    expect_name(p, &e->as.struct_lit.name);
                }
                next(p);
                e->as.struct_lit.fields =
                    field_inits(p, &e->as.struct_lit.field_count);
                return p->panic ? NULL : e;
            }
            e = new_expr(p, EXPR_NAME, t);
            expect_name(p, &e->as.name);
            e->type_args_pos = pos_of(peek(p));
            e->type_args = type_args(p, &e->type_arg_count);
            return e->type_args == NULL ? NULL : e;
        }
        /* `geo.Pair<int, str> { }` names a generic of another module. */
        if (!p->no_struct_literal && peek_at(p, 1)->kind == TOKEN_DOT &&
            peek_at(p, 2)->kind == TOKEN_IDENT &&
            (end = generic_end(p, 3)) > 0 &&
            peek_at(p, end)->kind == TOKEN_LBRACE) {
            e = new_expr(p, EXPR_STRUCT_LIT, t);
            expect_name(p, &e->as.struct_lit.module);
            next(p);
            expect_name(p, &e->as.struct_lit.name);
            e->type_args_pos = pos_of(peek(p));
            e->type_args = type_args(p, &e->type_arg_count);
            if (e->type_args == NULL) {
                return NULL;
            }
            next(p);
            e->as.struct_lit.fields =
                field_inits(p, &e->as.struct_lit.field_count);
            return p->panic ? NULL : e;
        }
        qualified = peek_at(p, 1)->kind == TOKEN_DOT &&
                    peek_at(p, 2)->kind == TOKEN_IDENT &&
                    peek_at(p, 3)->kind == TOKEN_LBRACE;
        /* `geo.Shape.Circle { }` names the case of a variant of another
           module. */
        member = peek_at(p, 1)->kind == TOKEN_DOT &&
                 peek_at(p, 2)->kind == TOKEN_IDENT &&
                 peek_at(p, 3)->kind == TOKEN_DOT &&
                 peek_at(p, 4)->kind == TOKEN_IDENT &&
                 peek_at(p, 5)->kind == TOKEN_LBRACE;
        if (!p->no_struct_literal &&
            (qualified || member || peek_at(p, 1)->kind == TOKEN_LBRACE)) {
            e = new_expr(p, EXPR_STRUCT_LIT, t);
            expect_name(p, &e->as.struct_lit.name);
            if (qualified || member) {
                e->as.struct_lit.module = e->as.struct_lit.name;
                next(p);
                expect_name(p, &e->as.struct_lit.name);
            }
            if (member) {
                next(p);
                expect_name(p, &e->as.struct_lit.member);
            }
            next(p);
            e->as.struct_lit.fields =
                field_inits(p, &e->as.struct_lit.field_count);
            return p->panic ? NULL : e;
        }
        e = new_expr(p, EXPR_NAME, t);
        expect_name(p, &e->as.name);
        return e;
    }
    case TOKEN_LPAREN: {
        bool saved = p->no_struct_literal;
        struct expr *first;
        next(p);
        p->no_struct_literal = false;
        first = expression(p);
        /* `(a)` groups and `(a, b)` builds a tuple. */
        if (first != NULL && check(p, TOKEN_COMMA)) {
            struct list elements = {NULL, 0, 0, sizeof(struct expr *)};
            e = new_expr(p, EXPR_TUPLE, t);
            list_push(&elements, &first);
            while (accept(p, TOKEN_COMMA)) {
                struct expr *element = expression(p);
                if (element == NULL) {
                    free(elements.data);
                    return NULL;
                }
                list_push(&elements, &element);
            }
            e->as.tuple.elements =
                list_finish(p, &elements, &e->as.tuple.count);
            p->no_struct_literal = saved;
            return expect(p, TOKEN_RPAREN) ? e : NULL;
        }
        p->no_struct_literal = saved;
        return first != NULL && expect(p, TOKEN_RPAREN) ? first : NULL;
    }
    case TOKEN_LBRACKET:
        if (peek_at(p, 1)->kind == TOKEN_RBRACKET) {
            if (p->no_struct_literal) {
                break;
            }
            e = new_expr(p, EXPR_SLICE_LIT, t);
            next(p);
            next(p);
            if ((e->as.slice_lit.element = type(p)) == NULL ||
                !expect(p, TOKEN_LBRACE)) {
                return NULL;
            }
            e->as.slice_lit.fields =
                field_inits(p, &e->as.slice_lit.field_count);
            return p->panic ? NULL : e;
        }
        next(p);
        {
            struct expr *first = expression(p);
            if (first == NULL) {
                return NULL;
            }
            if (accept(p, TOKEN_SEMICOLON)) {
                e = new_expr(p, EXPR_ARRAY_REPEAT, t);
                e->as.array_repeat.value = first;
                if ((e->as.array_repeat.count = expression(p)) == NULL ||
                    !expect(p, TOKEN_RBRACKET)) {
                    return NULL;
                }
                return e;
            }
            e = new_expr(p, EXPR_ARRAY_LIT, t);
            {
                struct list items = {NULL, 0, 0, sizeof(struct expr *)};
                list_push(&items, &first);
                while (accept(p, TOKEN_COMMA) && !check(p, TOKEN_RBRACKET)) {
                    struct expr *item = expression(p);
                    if (item == NULL) {
                        free(items.data);
                        return NULL;
                    }
                    list_push(&items, &item);
                }
                e->as.array_lit.elements =
                    list_finish(p, &items, &e->as.array_lit.count);
            }
            return expect(p, TOKEN_RBRACKET) ? e : NULL;
        }
    case TOKEN_ALLOC:
        next(p);
        e = new_expr(p, EXPR_ALLOC, t);
        /* DESIGN: `alloc T { ... }` puts one object on the heap and
           writes the literal into it. `alloc(T, n)` stays the raw form
           for any type and any count. */
        if (!check(p, TOKEN_LPAREN)) {
            if ((e->as.alloc.value = expression(p)) == NULL) {
                return NULL;
            }
            return e;
        }
        if (!expect(p, TOKEN_LPAREN) ||
            (e->as.alloc.type = type(p)) == NULL ||
            !expect(p, TOKEN_COMMA) ||
            (e->as.alloc.count = expression(p)) == NULL) {
            return NULL;
        }
        accept(p, TOKEN_COMMA);
        return expect(p, TOKEN_RPAREN) ? e : NULL;
    case TOKEN_DUP:
    case TOKEN_DELETE:
    case TOKEN_DESTROY:
        e = new_expr(p, EXPR_OBJECT, t);
        e->as.object.op = t->kind;
        next(p);
        if (!expect(p, TOKEN_LPAREN) ||
            (e->as.object.operand = expression(p)) == NULL) {
            return NULL;
        }
        if (accept(p, TOKEN_COMMA) && !check(p, TOKEN_RPAREN)) {
            if ((e->as.object.from = expression(p)) == NULL) {
                return NULL;
            }
            accept(p, TOKEN_COMMA);
        }
        return expect(p, TOKEN_RPAREN) ? e : NULL;
    case TOKEN_FREE:
        next(p);
        e = new_expr(p, EXPR_FREE, t);
        if (!expect(p, TOKEN_LPAREN) ||
            (e->as.free_pointer = expression(p)) == NULL) {
            return NULL;
        }
        accept(p, TOKEN_COMMA);
        return expect(p, TOKEN_RPAREN) ? e : NULL;
    case TOKEN_SIZE_OF:
        next(p);
        e = new_expr(p, EXPR_SIZE_OF, t);
        if (!expect(p, TOKEN_LPAREN) || (e->as.size_of = type(p)) == NULL) {
            return NULL;
        }
        accept(p, TOKEN_COMMA);
        return expect(p, TOKEN_RPAREN) ? e : NULL;
    case TOKEN_DISPATCH:
        /* DESIGN: `dispatch obj -> f(args)` reads as one expression, as
           `parallel` does. The object stands before the arrow and the
           worker after it, alone or called with the arguments that the
           worker receives after the object. */
        next(p);
        e = new_expr(p, EXPR_DISPATCH, t);
        if ((e->as.dispatch.object = expression(p)) == NULL ||
            !expect(p, TOKEN_ARROW) ||
            (e->as.dispatch.call = postfix(p)) == NULL) {
            return NULL;
        }
        return e;
    case TOKEN_JOIN:
    case TOKEN_JOIN_ALL:
        e = new_expr(p, EXPR_JOIN, t);
        e->as.join.all = t->kind == TOKEN_JOIN_ALL;
        next(p);
        if (!expect(p, TOKEN_LPAREN) ||
            (e->as.join.job = expression(p)) == NULL) {
            return NULL;
        }
        accept(p, TOKEN_COMMA);
        return expect(p, TOKEN_RPAREN) ? e : NULL;
    /* DESIGN: `chan T(n)` makes a channel of capacity n, and `send(c, v)`
       and `recv(c)` read as calls of their keyword. `close(c)` is a call
       of a name, which the checker takes as the built-in unless a
       function of that name is in scope. */
    case TOKEN_CHAN:
        next(p);
        e = new_expr(p, EXPR_SYNC_OP, t);
        e->as.sync_op.op = SYNC_CHAN_NEW;
        if ((e->as.sync_op.element = type(p)) == NULL ||
            !expect(p, TOKEN_LPAREN) ||
            (e->as.sync_op.value = expression(p)) == NULL) {
            return NULL;
        }
        accept(p, TOKEN_COMMA);
        return expect(p, TOKEN_RPAREN) ? e : NULL;
    case TOKEN_SEND:
    case TOKEN_RECV:
        next(p);
        e = new_expr(p, EXPR_SYNC_OP, t);
        e->as.sync_op.op = t->kind == TOKEN_SEND ? SYNC_SEND : SYNC_RECV;
        if (!expect(p, TOKEN_LPAREN) ||
            (e->as.sync_op.target = expression(p)) == NULL) {
            return NULL;
        }
        if (t->kind == TOKEN_SEND &&
            (!expect(p, TOKEN_COMMA) ||
             (e->as.sync_op.value = expression(p)) == NULL)) {
            return NULL;
        }
        accept(p, TOKEN_COMMA);
        return expect(p, TOKEN_RPAREN) ? e : NULL;
    case TOKEN_PARALLEL:
        /* DESIGN: `parallel a by n -> f(x)` reads as one expression. The
           array and the chunk count are expressions, and `by` is a
           contextual word rather than a keyword. After the arrow stands
           the worker, alone or called with the arguments that every
           chunk receives after its own. */
        next(p);
        e = new_expr(p, EXPR_PARALLEL, t);
        if ((e->as.parallel.array = expression(p)) == NULL) {
            return NULL;
        }
        if (is_word(p, peek(p), "by")) {
            next(p);
            if ((e->as.parallel.chunks = expression(p)) == NULL) {
                return NULL;
            }
        }
        if (!expect(p, TOKEN_ARROW) ||
            (e->as.parallel.call = postfix(p)) == NULL) {
            return NULL;
        }
        return e;
    default:
        break;
    }
    error_here(p, "expected an expression");
    return NULL;
}

/* The field name of a tuple element: `_` and the digits of the number,
   which is the name types.c gives the element. */
static bool element_name(struct parser *p, struct name *out)
{
    const struct token *t = peek(p);
    const char *digits = p->source + t->offset;
    char *text;
    size_t i;

    for (i = 0; i < t->length; i++) {
        if (digits[i] < '0' || digits[i] > '9') {
            error_here(p, "an element of a tuple is a decimal number");
            return false;
        }
    }
    text = arena_alloc(p->arena, t->length + 1);
    text[0] = '_';
    memcpy(text + 1, digits, t->length);
    out->text = text;
    out->length = t->length + 1;
    next(p);
    return true;
}

static struct expr *postfix(struct parser *p)
{
    struct expr *e = primary(p);

    while (e != NULL) {
        const struct token *t = peek(p);
        struct expr *outer;

        if (accept(p, TOKEN_LPAREN)) {
            bool saved = p->no_struct_literal;
            outer = new_expr(p, EXPR_CALL, t);
            outer->pos = e->pos;
            outer->as.call.callee = e;
            p->no_struct_literal = false;
            outer->as.call.args =
                expressions(p, TOKEN_RPAREN, &outer->as.call.arg_count);
            p->no_struct_literal = saved;
            if (p->panic) {
                return NULL;
            }
        } else if (accept(p, TOKEN_LBRACKET)) {
            bool saved = p->no_struct_literal;
            struct expr *first;
            p->no_struct_literal = false;
            first = expression(p);
            if (first != NULL && accept(p, TOKEN_DOT_DOT)) {
                outer = new_expr(p, EXPR_SLICE, t);
                outer->as.slice.base = e;
                outer->as.slice.low = first;
                outer->as.slice.high = expression(p);
            } else {
                outer = new_expr(p, EXPR_INDEX, t);
                outer->as.index.base = e;
                outer->as.index.index = first;
                /* `g[x, y]` gives the hooks every index. */
                if (first != NULL && check(p, TOKEN_COMMA)) {
                    struct list indices = {NULL, 0, 0,
                                           sizeof(struct expr *)};
                    struct expr *all = new_expr(p, EXPR_TUPLE, t);
                    all->pos = first->pos;
                    list_push(&indices, &first);
                    while (accept(p, TOKEN_COMMA)) {
                        struct expr *index = expression(p);
                        if (index == NULL) {
                            free(indices.data);
                            return NULL;
                        }
                        list_push(&indices, &index);
                    }
                    all->as.tuple.elements =
                        list_finish(p, &indices, &all->as.tuple.count);
                    outer->as.index.index = all;
                    outer->as.index.several = true;
                }
            }
            p->no_struct_literal = saved;
            outer->pos = e->pos;
            if (p->panic || !expect(p, TOKEN_RBRACKET)) {
                return NULL;
            }
        } else if (accept(p, TOKEN_DOT) || accept(p, TOKEN_QUESTION_DOT)) {
            outer = new_expr(p, EXPR_FIELD, t);
            outer->pos = e->pos;
            outer->as.field.base = e;
            outer->as.field.optional = t->kind == TOKEN_QUESTION_DOT;
            /* An element of a tuple is its number, and the field it
               names is `_0` upwards. */
            if (check(p, TOKEN_INT)) {
                outer->as.field.element = true;
                if (!element_name(p, &outer->as.field.name)) {
                    return NULL;
                }
            } else if (check(p, TOKEN_SUPER) || check(p, TOKEN_DESTROY) ||
                       check(p, TOKEN_SELECT)) {
                /* `self.super` names the base, `m.destroy()` the
                   function that releases a Mutex and `simd.select` the
                   choice of `anti.simd`. The built-in `destroy` and the
                   statement `select` never follow `.`. */
                outer->as.field.name.text = p->source + peek(p)->offset;
                outer->as.field.name.length = peek(p)->length;
                next(p);
            } else if (!expect_member_name(p, &outer->as.field.name)) {
                return NULL;
            }
            /* `geo.max<int>(a, b)` and `geo.List<int>.new()`. */
            if (!outer->as.field.element && generic_end(p, 0) > 0) {
                outer->type_args_pos = pos_of(peek(p));
                outer->type_args = type_args(p, &outer->type_arg_count);
                if (outer->type_args == NULL) {
                    return NULL;
                }
            }
        } else {
            return e;
        }
        e = outer;
    }
    return NULL;
}

static struct expr *unary(struct parser *p);

static struct expr *unary_level(struct parser *p)
{
    const struct token *t = peek(p);

    switch (t->kind) {
    case TOKEN_MINUS:
    case TOKEN_BANG:
    case TOKEN_TILDE:
    case TOKEN_STAR:
    case TOKEN_AMP: {
        struct expr *e;
        next(p);
        e = new_expr(p, EXPR_UNARY, t);
        e->as.unary.op = t->kind;
        e->as.unary.operand = unary(p);
        return e->as.unary.operand != NULL ? e : NULL;
    }
    /* DESIGN: `try f(args)` is `f(args) catch e { return e; }`, so it
       stands where the call stands and carries the same handler. */
    case TOKEN_TRY: {
        struct expr *e;
        next(p);
        e = postfix(p);
        if (e == NULL) {
            return NULL;
        }
        if (e->kind != EXPR_CALL) {
            error_here(p, "`try` stands before a call");
            return NULL;
        }
        e->as.call.handler.kind = HANDLE_TRY;
        e->as.call.handler.pos = pos_of(t);
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

    if (!descend(p)) {
        return NULL;
    }
    e = unary_level(p);
    ascend(p);
    return e;
}

/* unary { "as" type }: as binds tighter than the binary operators. */
static struct expr *cast(struct parser *p)
{
    struct expr *e = unary(p);

    while (e != NULL && (check(p, TOKEN_AS) || check(p, TOKEN_IS))) {
        bool test = check(p, TOKEN_IS);
        struct expr *c = new_expr(p, EXPR_CAST, next(p));
        c->pos = e->pos;
        c->as.cast.operand = e;
        c->as.cast.test = test;
        /* `as?` on a class pointer gives `none` where `as` traps. A `?`
           before `fn` opens a nullable function type, which `as?` never
           takes, so the type keeps it. */
        c->as.cast.checked = !test && peek_at(p, 1)->kind != TOKEN_FN &&
                             accept(p, TOKEN_QUESTION);
        if ((c->as.cast.type = type(p)) == NULL) {
            return NULL;
        }
        /* `v is geo.Shape.Circle` names the case of a variant of another
           module, one name more than a type has. */
        if (test && c->as.cast.type->kind == TYPEX_NAMED &&
            c->as.cast.type->module.length > 0 && check(p, TOKEN_DOT) &&
            peek_at(p, 1)->kind == TOKEN_IDENT) {
            next(p);
            expect_name(p, &c->as.cast.type->member);
        }
        e = c;
    }
    return e;
}

/* The precedence of a binary operator, from 1 for || to 11 for * / %.
   0 means the token is no binary operator. */
static int precedence(enum token_kind kind)
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
    struct expr *e = new_expr(p, EXPR_IN, next(p));
    int bound = precedence(TOKEN_LT) + 1;

    e->pos = value->pos;
    e->as.in.value = value;
    if ((e->as.in.low = binary(p, bound)) == NULL) {
        return NULL;
    }
    if (!accept(p, TOKEN_DOT_DOT)) {
        error_here(p, "`in` takes a range");
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
static struct expr *binary(struct parser *p, int min)
{
    struct expr *left = cast(p);

    while (left != NULL) {
        const struct token *op = peek(p);
        struct expr *e;
        /* `in` is a contextual word, as it is after `for`. A name right
           after an operand can be nothing else. */
        if (is_word(p, op, "in")) {
            if (precedence(TOKEN_LT) < min) {
                break;
            }
            left = in_range(p, left);
            continue;
        }
        if (precedence(op->kind) == 0 || precedence(op->kind) < min) {
            break;
        }
        next(p);
        e = new_expr(p, EXPR_BINARY, op);
        e->pos = left->pos;
        e->as.binary.op = op->kind;
        e->as.binary.left = left;
        /* The right operand of `??` is one level deeper, since `??`
           groups from the right. Any other operator's right operand
           takes a higher precedence, which bounds its nesting. */
        if (op->kind == TOKEN_QUESTION_QUESTION) {
            if (!descend(p)) {
                return NULL;
            }
            e->as.binary.right = binary(p, precedence(op->kind));
            ascend(p);
        } else {
            e->as.binary.right = binary(p, precedence(op->kind) + 1);
        }
        if (e->as.binary.right == NULL) {
            return NULL;
        }
        left = e;
    }
    return left;
}

static struct expr *expression(struct parser *p)
{
    return binary(p, 1);
}

/* A condition may not hold a struct or slice literal outside
   parentheses, because its '{' would be read as the body. */
static struct expr *condition(struct parser *p)
{
    bool saved = p->no_struct_literal;
    struct expr *e;

    p->no_struct_literal = true;
    e = expression(p);
    p->no_struct_literal = saved;
    return e;
}

/* Statements */

static bool is_assign_op(enum token_kind kind)
{
    return (kind >= TOKEN_ASSIGN && kind <= TOKEN_SHR_ASSIGN) ||
           (kind >= TOKEN_PLUS_WRAP_ASSIGN && kind <= TOKEN_STAR_SAT_ASSIGN);
}

static struct stmt *new_stmt(struct parser *p, enum stmt_kind kind,
                             const struct token *at)
{
    struct stmt *s = node(p, sizeof *s);

    s->kind = kind;
    s->pos = pos_of(at);
    return s;
}

static bool read_handler(struct parser *p, struct handler *out,
                         bool required);

/* The names of a destructuring, `(a, b)`. The opening parenthesis is
   read, and the list holds two names or more. */
static struct binding *bindings(struct parser *p, size_t *count)
{
    struct list names = {NULL, 0, 0, sizeof(struct binding)};

    while (!check(p, TOKEN_RPAREN)) {
        struct binding b;
        memset(&b, 0, sizeof b);
        b.pos = pos_of(peek(p));
        if (!expect_name(p, &b.name)) {
            free(names.data);
            return NULL;
        }
        list_push(&names, &b);
        if (!accept(p, TOKEN_COMMA)) {
            break;
        }
    }
    if (names.count < 2) {
        error_here(p, "a destructuring names two elements or more");
        free(names.data);
        return NULL;
    }
    if (!expect(p, TOKEN_RPAREN)) {
        free(names.data);
        return NULL;
    }
    return list_finish(p, &names, count);
}

static struct stmt *let_or_const(struct parser *p)
{
    const struct token *t = next(p);
    struct stmt *s = new_stmt(p, t->kind == TOKEN_LET ? STMT_LET : STMT_CONST,
                              t);

    s->as.let.name_pos = pos_of(peek(p));
    /* `let (a, b) = e;` takes a tuple apart. It binds no name of its
       own, and the type after a name is therefore the one-name form's. */
    if (t->kind == TOKEN_LET && accept(p, TOKEN_LPAREN)) {
        s->as.let.names = bindings(p, &s->as.let.name_count);
        if (s->as.let.names == NULL) {
            return NULL;
        }
        if (!expect(p, TOKEN_ASSIGN) ||
            (s->as.let.value = expression(p)) == NULL) {
            return NULL;
        }
        if (check(p, TOKEN_CATCH) &&
            !read_handler(p, s->as.let.value->kind == EXPR_CALL
                                 ? &s->as.let.value->as.call.handler
                                 : &s->as.let.guard,
                          false)) {
            return NULL;
        }
        return expect(p, TOKEN_SEMICOLON) ? s : NULL;
    }
    if (!expect_name(p, &s->as.let.name)) {
        return NULL;
    }
    if (t->kind == TOKEN_CONST ? !expect(p, TOKEN_COLON)
                               : !accept(p, TOKEN_COLON)) {
        if (p->panic) {
            return NULL;
        }
    } else {
        /* DESIGN: `atomic` before the type of a local makes it an
           atomic local, which the atomic operations alone reach. */
        if (t->kind == TOKEN_LET && check(p, TOKEN_ATOMIC)) {
            next(p);
            s->as.let.atomic = true;
        }
        if ((s->as.let.type = type(p)) == NULL) {
            return NULL;
        }
    }
    if (!expect(p, TOKEN_ASSIGN) || (s->as.let.value = expression(p)) == NULL) {
        return NULL;
    }
    /* `let n = f(args) catch e { ... };` handles the error of the call
       that gives n its value. `alloc T(args)` carries the handler of the
       `construct` it runs. */
    if (check(p, TOKEN_CATCH)) {
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
    if (accept(p, TOKEN_ELSE) &&
        (s->as.let.otherwise = block(p)) == NULL) {
        return NULL;
    }
    return expect(p, TOKEN_SEMICOLON) ? s : NULL;
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
    const struct token *t = peek(p);
    struct stmt *s;
    struct expr *e;

    if (check(p, TOKEN_LBRACE)) {
        s = new_stmt(p, STMT_BLOCK, t);
        return (s->as.block = block(p)) == NULL ? NULL : s;
    }
    if ((e = expression(p)) == NULL) {
        return NULL;
    }
    if (is_assign_op(peek(p)->kind)) {
        s = new_stmt(p, STMT_ASSIGN, t);
        s->as.assign.op = next(p)->kind;
        s->as.assign.target = e;
        return (s->as.assign.value = expression(p)) == NULL ? NULL : s;
    }
    if (e->kind != EXPR_CALL && e->kind != EXPR_SYNC_OP) {
        error_here(p, refusal);
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
    struct block *b = node(p, sizeof *b);

    b->pos = s->pos;
    b->end = s->pos;
    b->stmts = node(p, sizeof *b->stmts);
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
    struct switch_arm *arm = node(p, sizeof *arm);
    struct stmt *body;
    struct stmt *otherwise;

    next(p);
    s->as.switch_stmt.if_let = true;
    arm->pos = pos_of(peek(p));
    arm->value = new_expr(p, EXPR_NAME, peek(p));
    if (!expect_name(p, &arm->value->as.name)) {
        return NULL;
    }
    if (check(p, TOKEN_IDENT)) {
        arm->binds_pos = pos_of(peek(p));
        expect_name(p, &arm->binds);
    }
    if (!expect(p, TOKEN_ASSIGN)) {
        return NULL;
    }
    p->no_struct_literal = true;
    s->as.switch_stmt.value = expression(p);
    p->no_struct_literal = false;
    body = new_stmt(p, STMT_BLOCK, peek(p));
    if (s->as.switch_stmt.value == NULL ||
        (body->as.block = block(p)) == NULL) {
        return NULL;
    }
    arm->body = body;
    otherwise = new_stmt(p, STMT_BLOCK, peek(p));
    if (!accept(p, TOKEN_ELSE)) {
        otherwise->as.block = node(p, sizeof *otherwise->as.block);
        otherwise->as.block->pos = otherwise->pos;
        otherwise->as.block->end = otherwise->pos;
    } else if (check(p, TOKEN_IF)) {
        struct stmt *rest = if_statement(p);
        if (rest == NULL) {
            return NULL;
        }
        otherwise->as.block = block_of(p, rest);
    } else if ((otherwise->as.block = block(p)) == NULL) {
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
    const struct token *t = next(p);
    struct stmt *s;
    struct list branches = {NULL, 0, 0, sizeof(struct if_branch)};

    if (check(p, TOKEN_LET)) {
        return if_let(p, t);
    }
    s = new_stmt(p, STMT_IF, t);
    for (;;) {
        struct if_branch b;
        if ((b.cond = condition(p)) == NULL || (b.body = block(p)) == NULL) {
            free(branches.data);
            return NULL;
        }
        list_push(&branches, &b);
        if (!accept(p, TOKEN_ELSE)) {
            break;
        }
        /* `else if let` ends the chain with an `if let`, which holds the
           rest of it. */
        if (check(p, TOKEN_IF) && peek_at(p, 1)->kind == TOKEN_LET) {
            struct stmt *rest = if_statement(p);
            if (rest == NULL) {
                free(branches.data);
                return NULL;
            }
            s->as.if_chain.else_body = block_of(p, rest);
            break;
        }
        if (!accept(p, TOKEN_IF)) {
            if ((s->as.if_chain.else_body = block(p)) == NULL) {
                free(branches.data);
                return NULL;
            }
            break;
        }
    }
    s->as.if_chain.branches = list_finish(p, &branches, &s->as.if_chain.count);
    return s;
}

/* An `else if let` holds the rest of its chain, so each is one level. */
static struct stmt *if_statement(struct parser *p)
{
    struct stmt *s;

    if (!descend(p)) {
        return NULL;
    }
    s = if_level(p);
    ascend(p);
    return s;
}

/* `catch e { }`, `catch { }` or `catch fatal`. With required set the
   handler must be there. */
static bool read_handler(struct parser *p, struct handler *out, bool required)
{
    memset(out, 0, sizeof *out);
    out->pos = pos_of(peek(p));
    if (!accept(p, TOKEN_CATCH)) {
        if (required) {
            error_here(p, "expected `catch`");
            return false;
        }
        return true;
    }
    if (is_word(p, peek(p), "fatal")) {
        next(p);
        out->kind = HANDLE_FATAL;
        return true;
    }
    /* DESIGN: `catch none` counts a failure as `none`. It is the handler
       `catch { yield none; }`, which deletes the error on its way out as
       every handler does, so the parser writes that block and the
       checker refuses it where the result cannot be `none`. */
    if (check(p, TOKEN_NONE)) {
        const struct token *t = next(p);
        struct stmt *yield = new_stmt(p, STMT_YIELD, t);
        yield->as.yielded = new_expr(p, EXPR_NONE, t);
        out->kind = HANDLE_BLOCK;
        out->none = true;
        out->body = node(p, sizeof *out->body);
        out->body->pos = pos_of(t);
        out->body->end = pos_of(t);
        out->body->stmts = node(p, sizeof *out->body->stmts);
        out->body->stmts[0] = yield;
        out->body->count = 1;
        return true;
    }
    out->kind = HANDLE_BLOCK;
    if (peek(p)->kind == TOKEN_IDENT) {
        out->pos = pos_of(peek(p));
        if (!expect_name(p, &out->name)) {
            return false;
        }
    }
    return (out->body = block(p)) != NULL;
}

/* Clauses */

/* Whether the token is a word of a name, letters, digits and `_` with a
   letter first. A keyword is one as well, so `shadowed-catch` reads. */
static bool name_word(const struct parser *p, const struct token *t)
{
    const char *s = p->source + t->offset;
    size_t i;

    if (t->length == 0 || !isalpha((unsigned char)s[0])) {
        return false;
    }
    for (i = 1; i < t->length; i++) {
        if (!isalnum((unsigned char)s[i]) && s[i] != '_') {
            return false;
        }
    }
    return true;
}

/* Whether nothing stands between the two tokens. */
static bool adjacent(const struct token *a, const struct token *b)
{
    return a->offset + a->length == b->offset;
}

/* Whether `allow` or `unchecked` and a `(` stand at the token ahead. */
static bool clause_ahead(const struct parser *p, size_t ahead)
{
    const struct token *t = peek_at(p, ahead);

    return (is_word(p, t, "allow") || is_word(p, t, "unchecked")) &&
           peek_at(p, ahead + 1)->kind == TOKEN_LPAREN;
}

/* DESIGN: `allow` and `unchecked` are contextual words, so a function
   may still carry either name. Before a statement the clause is the
   word, its parentheses and a statement after them. A call of a
   function of that name ends with `;` or goes on as an expression, and
   the token after its `)` says which of the two stands. */
static bool statement_clause(const struct parser *p)
{
    size_t i = p->pos + 1;
    int depth = 0;
    enum token_kind after;

    if (!clause_ahead(p, 0)) {
        return false;
    }
    for (; p->tokens[i].kind != TOKEN_EOF; i++) {
        if (p->tokens[i].kind == TOKEN_LPAREN) {
            depth++;
        } else if (p->tokens[i].kind == TOKEN_RPAREN && --depth == 0) {
            break;
        }
    }
    if (p->tokens[i].kind == TOKEN_EOF) {
        return false;
    }
    after = p->tokens[i + 1].kind;
    return after != TOKEN_SEMICOLON && after != TOKEN_DOT &&
           after != TOKEN_QUESTION_DOT && after != TOKEN_LBRACKET &&
           after != TOKEN_LPAREN && after != TOKEN_CATCH &&
           after != TOKEN_AS && after != TOKEN_IS && !is_assign_op(after) &&
           precedence(after) == 0;
}

/* Read `allow(name, "reason")` or `unchecked(name, "reason")`. The
   clause waits for the source it covers, which close_clauses gives it.
   A name that is no warning of `allow`, or no safety check of
   `unchecked`, is refused: an error is never silenced. */
static bool read_clause(struct parser *p, enum clause_level level)
{
    const struct token *word = next(p);
    const struct token *first;
    const struct token *last;
    struct clause c;
    enum diag_name name;
    char message[160];

    memset(&c, 0, sizeof c);
    c.unchecked = is_word(p, word, "unchecked");
    c.level = level;
    c.block = p->block;
    c.pos = pos_of(word);
    next(p);
    first = last = peek(p);
    if (name_word(p, first)) {
        next(p);
        while (check(p, TOKEN_MINUS) && adjacent(last, peek(p)) &&
               adjacent(peek(p), peek_at(p, 1)) &&
               name_word(p, peek_at(p, 1))) {
            next(p);
            last = next(p);
        }
        c.name.text = p->source + first->offset;
        c.name.length = last->offset + last->length - first->offset;
    }
    if (c.name.length == 0 || !accept(p, TOKEN_COMMA) ||
        !check(p, TOKEN_STRING)) {
        snprintf(message, sizeof message,
                 "`%s` takes a name and a reason, `%s(name, \"reason\")`",
                 c.unchecked ? "unchecked" : "allow",
                 c.unchecked ? "unchecked" : "allow");
        error_here(p, message);
        return false;
    }
    c.reason = next(p)->value.text;
    if (!expect(p, TOKEN_RPAREN)) {
        return false;
    }
    name = warnings_find(c.name.text, c.name.length);
    if (!c.unchecked &&
        (name == NAME_NONE || warnings_kind(name) != KIND_WARNING)) {
        diagnostics_add(p->diags, first->line, first->column,
                        "`%.*s` is no warning, and `allow` silences a "
                        "warning alone", (int)c.name.length, c.name.text);
        p->ok = false;
    } else if (c.unchecked && (name == NAME_NONE ||
                               warnings_kind(name) != KIND_SAFETY_CHECK)) {
        diagnostics_add(p->diags, first->line, first->column,
                        "`%.*s` is no safety check, and `unchecked` "
                        "overrules a safety check alone",
                        (int)c.name.length, c.name.text);
        p->ok = false;
    } else if (c.reason.length == 0) {
        diagnostics_add(p->diags, c.pos.line, c.pos.column,
                        "the reason of `%s(%.*s)` is empty",
                        c.unchecked ? "unchecked" : "allow",
                        (int)c.name.length, c.name.text);
        p->ok = false;
    }
    list_push(p->clauses, &c);
    return true;
}

/* Give the clauses read since mark that still wait the source from
   `from` to the last token read. An inner statement closed its own
   clauses first, so only the ones of this level wait. */
static void close_clauses(struct parser *p, size_t mark, struct pos from)
{
    struct clause *all = p->clauses->data;
    struct pos to = pos_of(&p->tokens[p->pos > 0 ? p->pos - 1 : 0]);
    size_t i;

    for (i = mark; i < p->clauses->count; i++) {
        if (all[i].to.line == 0) {
            all[i].from = from;
            all[i].to = to;
        }
    }
}

/* The clauses last in a declaration's header, before its brace. */
static bool header_clauses(struct parser *p)
{
    while (clause_ahead(p, 0)) {
        if (!read_clause(p, CLAUSE_DECLARATION)) {
            return false;
        }
    }
    return true;
}

/* Where the source a declaration's clause covers starts: its doc
   comment, where a doc warning stands, or its first word. */
static struct pos declaration_start(const struct item *it)
{
    struct pos at = it->pos;

    if (it->doc.length > 0) {
        at.line = it->doc.line;
        at.column = it->doc.column;
    } else if (it->note.length > 0) {
        at.line = it->note.line;
        at.column = it->note.column;
    }
    return at;
}

static struct stmt *statement(struct parser *p);

static struct stmt *statement_level(struct parser *p)
{
    const struct token *t = peek(p);
    struct stmt *s;

    switch (t->kind) {
    case TOKEN_LET:
    case TOKEN_CONST:
        return let_or_const(p);
    case TOKEN_IF:
        return if_statement(p);
    case TOKEN_WHILE:
        next(p);
        s = new_stmt(p, STMT_WHILE, t);
        if ((s->as.loop.cond = condition(p)) == NULL || !expect(p, TOKEN_DO) ||
            (s->as.loop.body = block(p)) == NULL) {
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
        bool bound = peek_at(p, 1)->kind == TOKEN_IDENT &&
                     (is_word(p, peek_at(p, 2), "in") ||
                      (peek_at(p, 2)->kind == TOKEN_COMMA &&
                       peek_at(p, 3)->kind == TOKEN_IDENT &&
                       is_word(p, peek_at(p, 4), "in")));
        next(p);
        s = new_stmt(p, STMT_FOR, t);
        if (bound) {
            struct list names = {NULL, 0, 0, sizeof(struct binding)};
            do {
                struct binding b;
                memset(&b, 0, sizeof b);
                b.pos = pos_of(peek(p));
                if (!expect_name(p, &b.name)) {
                    free(names.data);
                    return NULL;
                }
                list_push(&names, &b);
            } while (accept(p, TOKEN_COMMA));
            s->as.for_loop.names =
                list_finish(p, &names, &s->as.for_loop.name_count);
            /* DESIGN: `in` is a contextual word, as `packed` and `align`
               are. The decision adds four keywords and `in` is not among
               them, so a program may still name a variable `in`. */
            next(p);
        }
        p->no_struct_literal = true;
        from = peek_at(p, check(p, TOKEN_AMP) ? 1 : 0)->offset;
        if (accept(p, TOKEN_AMP)) {
            s->as.for_loop.by_pointer = true;
            s->as.for_loop.over = expression(p);
        } else if ((s->as.for_loop.low = expression(p)) != NULL &&
                   accept(p, TOKEN_DOT_DOT)) {
            s->as.for_loop.high = expression(p);
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
        if (s->as.for_loop.high != NULL && is_word(p, peek(p), "by")) {
            next(p);
            s->as.for_loop.step_pos = pos_of(peek(p));
            s->as.for_loop.step = expression(p);
        }
        p->no_struct_literal = false;
        if (p->panic ||
            (s->as.for_loop.over == NULL && s->as.for_loop.high == NULL)) {
            return NULL;
        }
        if ((s->as.for_loop.body = block(p)) == NULL) {
            return NULL;
        }
        return s;
    }
    case TOKEN_DO:
        next(p);
        s = new_stmt(p, STMT_DO_WHILE, t);
        if ((s->as.loop.body = block(p)) == NULL ||
            !expect(p, TOKEN_WHILE) ||
            (s->as.loop.cond = condition(p)) == NULL) {
            return NULL;
        }
        return s;
    /* DESIGN: `assert(cond)` keeps the source of its condition, so the
       failure names what was asserted. A release build removes the whole
       statement, which is why a call in the condition warns. */
    case TOKEN_ASSERT: {
        size_t from;
        size_t to;
        next(p);
        s = new_stmt(p, STMT_ASSERT, t);
        if (!expect(p, TOKEN_LPAREN)) {
            return NULL;
        }
        from = peek(p)->offset;
        if ((s->as.assertion.cond = expression(p)) == NULL) {
            return NULL;
        }
        to = p->pos == 0 ? from : p->all[p->origin[p->pos - 1]].offset +
                                  p->all[p->origin[p->pos - 1]].length;
        s->as.assertion.text.bytes = p->source + from;
        s->as.assertion.text.length = to > from ? to - from : 0;
        if (accept(p, TOKEN_COMMA)) {
            if (!check(p, TOKEN_STRING)) {
                error_here(p, "the message of an `assert` is a string "
                              "literal");
                return NULL;
            }
            s->as.assertion.message = peek(p)->value.text;
            next(p);
        }
        return expect(p, TOKEN_RPAREN) && expect(p, TOKEN_SEMICOLON) ? s
                                                                     : NULL;
    }
    /* DESIGN: `switch e { A => stmt, else => stmt }` names one value per
       arm and runs one statement. An arm falls through only where it
       ends in `fallthrough;`, so no arm needs a break, and `else` takes
       the rest. The switch keeps the place the text gives `else`, since
       the arm after it is the one its `fallthrough` enters. */
    case TOKEN_SWITCH: {
        struct list arms = {NULL, 0, 0, sizeof(struct switch_arm)};
        next(p);
        s = new_stmt(p, STMT_SWITCH, t);
        p->no_struct_literal = true;
        s->as.switch_stmt.value = expression(p);
        p->no_struct_literal = false;
        if (s->as.switch_stmt.value == NULL || !expect(p, TOKEN_LBRACE)) {
            return NULL;
        }
        while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
            struct switch_arm arm;
            memset(&arm, 0, sizeof arm);
            arm.pos = pos_of(peek(p));
            if (accept(p, TOKEN_ELSE)) {
                if (s->as.switch_stmt.otherwise != NULL) {
                    error_here(p, "a `switch` has one `else`");
                    free(arms.data);
                    return NULL;
                }
                s->as.switch_stmt.otherwise_at = arms.count;
                if (!expect(p, TOKEN_FAT_ARROW) ||
                    (s->as.switch_stmt.otherwise =
                         arm_body(p, SWITCH_ARM)) == NULL) {
                    free(arms.data);
                    return NULL;
                }
            } else {
                /* `Circle c =>` binds the fields of a case of a variant
                   to c. The case is a name, which the checker reads. */
                if (check(p, TOKEN_IDENT) &&
                    peek_at(p, 1)->kind == TOKEN_IDENT &&
                    peek_at(p, 2)->kind == TOKEN_FAT_ARROW) {
                    arm.value = new_expr(p, EXPR_NAME, peek(p));
                    expect_name(p, &arm.value->as.name);
                    arm.binds_pos = pos_of(peek(p));
                    expect_name(p, &arm.binds);
                }
                if ((arm.value == NULL && (arm.value = expression(p)) == NULL) ||
                    !expect(p, TOKEN_FAT_ARROW) ||
                    (arm.body = arm_body(p, SWITCH_ARM)) == NULL) {
                    free(arms.data);
                    return NULL;
                }
                list_push(&arms, &arm);
            }
            if (!accept(p, TOKEN_COMMA)) {
                break;
            }
        }
        s->as.switch_stmt.arms = list_finish(p, &arms, &s->as.switch_stmt.count);
        return expect(p, TOKEN_RBRACE) ? s : NULL;
    }
    /* DESIGN: `sync m { }` holds the mutex m for the block. The operand
       is read as a condition is, so its `{` opens the block. */
    case TOKEN_SYNC:
        next(p);
        s = new_stmt(p, STMT_SYNC, t);
        if ((s->as.sync.mutex = condition(p)) == NULL ||
            (s->as.sync.body = block(p)) == NULL) {
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
        next(p);
        s = new_stmt(p, STMT_SELECT, t);
        if (!expect(p, TOKEN_LBRACE)) {
            return NULL;
        }
        if (check(p, TOKEN_RBRACE)) {
            error_here(p, "a `select` waits on one channel or more");
            return NULL;
        }
        while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
            struct switch_arm arm;
            memset(&arm, 0, sizeof arm);
            arm.pos = pos_of(peek(p));
            if (check(p, TOKEN_ELSE)) {
                error_here(p, "a `select` has no `else`");
                free(arms.data);
                return NULL;
            }
            if ((arm.value = expression(p)) == NULL) {
                free(arms.data);
                return NULL;
            }
            if (check(p, TOKEN_IDENT) &&
                peek_at(p, 1)->kind == TOKEN_FAT_ARROW) {
                arm.binds_pos = pos_of(peek(p));
                expect_name(p, &arm.binds);
            }
            if (!expect(p, TOKEN_FAT_ARROW) ||
                (arm.body = arm_body(p, SELECT_ARM)) == NULL) {
                free(arms.data);
                return NULL;
            }
            list_push(&arms, &arm);
            if (!accept(p, TOKEN_COMMA)) {
                break;
            }
        }
        s->as.select.arms = list_finish(p, &arms, &s->as.select.count);
        return expect(p, TOKEN_RBRACE) ? s : NULL;
    }
    /* DESIGN: `defer stmt;` records a statement that runs at every exit
       of the enclosing block, in reverse order of declaration. */
    case TOKEN_DEFER:
        next(p);
        s = new_stmt(p, STMT_DEFER, t);
        if ((s->as.deferred = statement(p)) == NULL) {
            return NULL;
        }
        if (s->as.deferred->kind == STMT_DEFER) {
            error_here(p, "a `defer` holds one statement, not another "
                          "`defer`");
            return NULL;
        }
        return s;
    /* DESIGN: `undo stmt;` records a statement that runs only when the
       enclosing block leaves through an error, before the `defer`
       statements of the same block. `defer` is always, `undo` is only if
       this block fails. */
    case TOKEN_UNDO:
        next(p);
        s = new_stmt(p, STMT_UNDO, t);
        if ((s->as.deferred = statement(p)) == NULL) {
            return NULL;
        }
        if (s->as.deferred->kind == STMT_UNDO) {
            error_here(p, "an `undo` holds one statement, not another "
                          "`undo`");
            return NULL;
        }
        return s;
    /* `fail e;` leaves on the error channel, and `fail "text";` is
       `fail Error.new(0, "text");`. */
    case TOKEN_FAIL:
        next(p);
        s = new_stmt(p, STMT_FAIL, t);
        if ((s->as.fail.value = expression(p)) == NULL) {
            return NULL;
        }
        return expect(p, TOKEN_SEMICOLON) ? s : NULL;
    case TOKEN_BREAK:
    case TOKEN_CONTINUE:
        next(p);
        s = new_stmt(p, t->kind == TOKEN_BREAK ? STMT_BREAK : STMT_CONTINUE, t);
        return expect(p, TOKEN_SEMICOLON) ? s : NULL;
    /* The checker decides where `fallthrough;` may stand, since the
       grammar of a block does not know that the block is an arm. */
    case TOKEN_FALLTHROUGH:
        next(p);
        s = new_stmt(p, STMT_FALLTHROUGH, t);
        return expect(p, TOKEN_SEMICOLON) ? s : NULL;
    case TOKEN_RETURN:
        next(p);
        s = new_stmt(p, STMT_RETURN, t);
        if (!check(p, TOKEN_SEMICOLON) &&
            (s->as.return_value = expression(p)) == NULL) {
            return NULL;
        }
        return expect(p, TOKEN_SEMICOLON) ? s : NULL;
    case TOKEN_LBRACE:
        s = new_stmt(p, STMT_BLOCK, t);
        return (s->as.block = block(p)) != NULL ? s : NULL;
    /* `yield v;` ends a handler with the value that takes the place of
       the result the failing call would have given. */
    case TOKEN_YIELD:
        next(p);
        s = new_stmt(p, STMT_YIELD, t);
        if (!check(p, TOKEN_SEMICOLON) &&
            (s->as.yielded = expression(p)) == NULL) {
            return NULL;
        }
        return expect(p, TOKEN_SEMICOLON) ? s : NULL;
    /* `try { } catch e { }` handles every failing call of the block. */
    case TOKEN_TRY:
        if (peek_at(p, 1)->kind != TOKEN_LBRACE) {
            break;
        }
        next(p);
        s = new_stmt(p, STMT_TRY, t);
        if ((s->as.try_block.body = block(p)) == NULL ||
            !read_handler(p, &s->as.try_block.handler, true)) {
            return NULL;
        }
        return s;
    default:
        break;
    }

    {
        struct expr *e = expression(p);
        if (e == NULL) {
            return NULL;
        }
        if (is_assign_op(peek(p)->kind)) {
            s = new_stmt(p, STMT_ASSIGN, t);
            s->as.assign.op = next(p)->kind;
            s->as.assign.target = e;
            if ((s->as.assign.value = expression(p)) == NULL) {
                return NULL;
            }
            /* `*p = f(args) catch e { ... };` handles the error of the
               call that fills the place, as the `let` of the same call
               does. The two forms of a failing call stand wherever the
               call stands, so neither `try` nor `catch` has a statement
               it is missing from. A `catch` on anything else guards a
               pointer and binds the one it proved, which an assignment
               has no name for. */
            if (check(p, TOKEN_CATCH)) {
                struct expr *v = s->as.assign.value;
                if (v->kind != EXPR_CALL) {
                    error_here(p, "`catch` stands after a call here, and a "
                                  "`catch` that guards a pointer stands in "
                                  "a `let`");
                    return NULL;
                }
                if (!read_handler(p, &v->as.call.handler, false)) {
                    return NULL;
                }
            }
        } else {
            /* A call that can fail carries its handler here. */
            if (e->kind == EXPR_CALL && check(p, TOKEN_CATCH) &&
                !read_handler(p, &e->as.call.handler, false)) {
                return NULL;
            }
            if (!check(p, TOKEN_SEMICOLON)) {
                expect(p, TOKEN_SEMICOLON);
                return NULL;
            }
            if (e->kind != EXPR_CALL && e->kind != EXPR_FREE &&
                e->kind != EXPR_OBJECT && e->kind != EXPR_JOIN &&
                e->kind != EXPR_SYNC_OP) {
                error_here(p, "expected a call or an assignment");
                return NULL;
            }
            s = new_stmt(p, STMT_EXPR, t);
            s->as.expr = e;
        }
        return expect(p, TOKEN_SEMICOLON) ? s : NULL;
    }
}

/* A nested block, `defer` and `undo` each hold a statement, so each
   statement is one level. */
static struct stmt *statement(struct parser *p)
{
    size_t mark = p->clauses->count;
    struct pos from = pos_of(peek(p));
    struct stmt *s = NULL;

    if (!descend(p)) {
        return NULL;
    }
    while (statement_clause(p)) {
        if (!read_clause(p, CLAUSE_STATEMENT)) {
            ascend(p);
            return NULL;
        }
    }
    if (p->clauses->count > mark &&
        (check(p, TOKEN_RBRACE) || check(p, TOKEN_EOF))) {
        error_here(p, "a clause of a statement stands before the statement");
    } else {
        s = statement_level(p);
    }
    ascend(p);
    close_clauses(p, mark, from);
    return s;
}

static struct block *block(struct parser *p)
{
    struct block *b = node(p, sizeof *b);
    struct list stmts = {NULL, 0, 0, sizeof(struct stmt *)};

    b->pos = pos_of(peek(p));
    if (!expect(p, TOKEN_LBRACE)) {
        return NULL;
    }
    while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
        struct stmt *s = statement(p);
        if (s != NULL) {
            list_push(&stmts, &s);
        } else {
            sync_statement(p);
        }
    }
    b->stmts = list_finish(p, &stmts, &b->count);
    b->end = pos_of(peek(p));
    return expect(p, TOKEN_RBRACE) ? b : NULL;
}

/* Items */

/* A parenthesised list of name: type pairs, each with an optional
   `= value`. When allow_variadic is set, an ellipsis token may end the
   list. */
/* DESIGN: `self` is written as the first parameter and carries no type,
   because its type is always a pointer to the struct that declares the
   function. It reaches the parameter list as has_self, not as a param. */
static struct param *params(struct parser *p, bool allow_variadic,
                            bool *variadic, bool *has_self, size_t *count)
{
    struct list list = {NULL, 0, 0, sizeof(struct param)};

    if (!expect(p, TOKEN_LPAREN)) {
        return NULL;
    }
    if (has_self != NULL && check(p, TOKEN_SELF)) {
        next(p);
        *has_self = true;
        if (!accept(p, TOKEN_COMMA)) {
            return expect(p, TOKEN_RPAREN) ? list_finish(p, &list, count)
                                           : NULL;
        }
    }
    while (!check(p, TOKEN_RPAREN)) {
        struct param param;
        memset(&param, 0, sizeof param);
        if (allow_variadic && list.count > 0 && accept(p, TOKEN_ELLIPSIS)) {
            *variadic = true;
            accept(p, TOKEN_COMMA);
            break;
        }
        /* `own name: T`: the function takes over the object. `own` is a
           contextual word, so a parameter may still carry that name. */
        if (is_word(p, peek(p), "own") && peek_at(p, 1)->kind == TOKEN_IDENT) {
            next(p);
            param.owned = true;
        }
        fn_param_marks(p, &param.keep, &param.concurrent, &param.owned);
        param.lent = lent_mark(p);
        param.pos = pos_of(peek(p));
        if (!expect_name(p, &param.name) || !expect(p, TOKEN_COLON) ||
            (param.type = type(p)) == NULL) {
            free(list.data);
            return NULL;
        }
        /* `name: T = value`: the default that a call which leaves the
           parameter out passes. */
        if (accept(p, TOKEN_ASSIGN) && (param.value = expression(p)) == NULL) {
            free(list.data);
            return NULL;
        }
        list_push(&list, &param);
        if (!accept(p, TOKEN_COMMA)) {
            break;
        }
    }
    if (!expect(p, TOKEN_RPAREN)) {
        free(list.data);
        return NULL;
    }
    return list_finish(p, &list, count);
}

/* Whether token t is the identifier word. */
static bool is_word(const struct parser *p, const struct token *t,
                    const char *word)
{
    return t->kind == TOKEN_IDENT && t->length == strlen(word) &&
           memcmp(p->source + t->offset, word, t->length) == 0;
}

/* DESIGN: `may fail` is two contextual words after a signature, not a
   keyword pair, so `may` stays an identifier everywhere else. The
   function then returns `?*Error` and writes its result through an out
   pointer, which is the convention a program used to write by hand. */
static void may_fail_after(struct parser *p, struct item *it)
{
    if (!is_word(p, peek(p), "may") || peek_at(p, 1)->kind != TOKEN_FAIL) {
        return;
    }
    it->may_fail_pos = pos_of(peek(p));
    next(p);
    next(p);
    it->may_fail = true;
}

/* Whether the next token opens a function or a constant of a body.
   DESIGN: `pub` and `protected` stand before a field as well as before a
   function, so the test looks past the marker. A field is a name and a
   colon, and everything else at that point opens a member. */
static bool starts_member(const struct parser *p)
{
    size_t i = 0;

    switch (peek(p)->kind) {
    case TOKEN_PUB:
    case TOKEN_PROTECTED:
    case TOKEN_EXPORT:
        i = 1;
        break;
    default:
        break;
    }
    switch (peek_at(p, i)->kind) {
    case TOKEN_FN:
    case TOKEN_EXPORT:
    case TOKEN_PUB:
    case TOKEN_ABSTRACT:
    case TOKEN_CONCRETE:
    case TOKEN_CONST:
    case TOKEN_STATIC:
        return true;
    default:
        return (is_word(p, peek_at(p, i), "final") ||
                is_word(p, peek_at(p, i), "operator")) &&
               peek_at(p, i + 1)->kind == TOKEN_FN;
    }
}

/* One function or constant declared between the braces of a struct, a
   union or an enum. An abstract function has no body and ends with `;`. */
/* The constraints after the `:` of a type parameter or the `=` of a
   `constraint`, joined by `+`. Each is a name, qualified by a module or
   not. */
static struct constraint_ref *constraint_list(struct parser *p,
                                              size_t *count)
{
    struct list refs = {NULL, 0, 0, sizeof(struct constraint_ref)};

    do {
        struct constraint_ref r;
        memset(&r, 0, sizeof r);
        r.pos = pos_of(peek(p));
        if (!expect_name(p, &r.name) ||
            (accept(p, TOKEN_DOT) &&
             (r.module = r.name, !expect_name(p, &r.name)))) {
            free(refs.data);
            *count = 0;
            return NULL;
        }
        /* A generic interface takes its type arguments, as a type does. */
        if (check(p, TOKEN_LT)) {
            r.type_args_pos = pos_of(peek(p));
            r.type_args = type_args(p, &r.type_arg_count);
            if (r.type_args == NULL) {
                free(refs.data);
                *count = 0;
                return NULL;
            }
        }
        list_push(&refs, &r);
    } while (accept(p, TOKEN_PLUS));
    return list_finish(p, &refs, count);
}

/* `<T: lt + eq, U, N: int>` after the name of a generic. A parameter
   without `:` is unconstrained, and `N: int` takes an integer constant. */
static bool type_params(struct parser *p, struct item *it)
{
    struct list list = {NULL, 0, 0, sizeof(struct type_param)};

    bool ok;

    if (!check(p, TOKEN_LT)) {
        return true;
    }
    next(p);
    /* The list counts as open, so that `>>` after the arguments of a
       constraint closes both. */
    p->angles++;
    do {
        struct type_param tp;
        memset(&tp, 0, sizeof tp);
        tp.pos = pos_of(peek(p));
        if (!expect_name(p, &tp.name)) {
            free(list.data);
            p->angles--;
            return false;
        }
        if (accept(p, TOKEN_COLON)) {
            if (accept(p, TOKEN_INT_TYPE)) {
                tp.constant = true;
            } else if (is_builtin_type(peek(p)->kind)) {
                error_here(p, "a constant parameter is written `N: int`");
                free(list.data);
                p->angles--;
                return false;
            } else if ((tp.constraints = constraint_list(
                            p, &tp.constraint_count)) == NULL) {
                free(list.data);
                p->angles--;
                return false;
            }
        }
        list_push(&list, &tp);
    } while (!p->half && accept(p, TOKEN_COMMA));
    it->type_params = list_finish(p, &list, &it->type_param_count);
    ok = close_angle(p);
    p->angles--;
    return ok;
}

static struct item *member_level(struct parser *p, const struct item *owner)
{
    struct item *m = node(p, sizeof *m);

    m->pos = pos_of(peek(p));
    m->doc = doc_before(p, TOKEN_DOC);
    m->note = doc_before(p, TOKEN_NOTE);
    m->owner = owner;
    m->exported = accept(p, TOKEN_EXPORT);
    m->pub = m->exported || accept(p, TOKEN_PUB);
    m->vis = m->pub ? VIS_PUB : VIS_PRIVATE;
    /* DESIGN: `protected` reaches the class and every class below it. The
       visibility table of the object model document gives it to a class
       member alone, so `internal` here names the level that fits. */
    if (!m->pub && accept(p, TOKEN_PROTECTED)) {
        m->vis = VIS_PROTECTED;
    } else if (check(p, TOKEN_INTERNAL)) {
        error_here(p, "`internal` marks a module item, and a class member "
                      "is `pub`, `protected` or neither");
        return NULL;
    }
    /* `final` before `fn` forbids replacement, as it forbids inheritance
       before `class`. It is a contextual word in both places. */
    if (is_word(p, peek(p), "final") && peek_at(p, 1)->kind == TOKEN_FN) {
        next(p);
        m->is_final = true;
    }
    /* `operator` before `fn` marks a function an operator calls. It is a
       contextual word and implies `pub`. */
    if (is_word(p, peek(p), "operator") && peek_at(p, 1)->kind == TOKEN_FN) {
        next(p);
        m->is_operator = true;
    }
    /* `trace` before `fn` marks one function of the class, as it marks
       every `pub` function before `class`. */
    if (is_word(p, peek(p), "trace") && peek_at(p, 1)->kind == TOKEN_FN) {
        next(p);
        m->trace = true;
    }
    if (accept(p, TOKEN_ABSTRACT)) {
        m->contract = FN_ABSTRACT;
    } else if (accept(p, TOKEN_CONCRETE)) {
        m->contract = FN_CONCRETE;
    }
    /* A function of a contract is public wherever its struct is, and so
       is one an operator calls. */
    if (m->contract != FN_PLAIN || m->is_operator) {
        m->pub = true;
        m->vis = VIS_PUB;
    }
    /* DESIGN: `static atomic n: T = e;` declares a field of the class,
       not of the object. It is a global with the class's symbol, and it
       must be atomic, so that `parallel` keeps its guarantee. */
    if (m->contract == FN_PLAIN && accept(p, TOKEN_STATIC)) {
        bool atomic = accept(p, TOKEN_ATOMIC);
        m->kind = ITEM_CONST;
        m->is_static = true;
        m->name_pos = pos_of(peek(p));
        if (!expect_name(p, &m->name)) {
            return NULL;
        }
        if (!atomic) {
            char message[96];
            snprintf(message, sizeof message,
                     "`%.*s` is `static` and must be `atomic`",
                     (int)m->name.length, m->name.text);
            error_here(p, message);
            return NULL;
        }
        m->atomic = true;
        if (!expect(p, TOKEN_COLON) || (m->type = type(p)) == NULL ||
            !expect(p, TOKEN_ASSIGN) || (m->value = expression(p)) == NULL ||
            !expect(p, TOKEN_SEMICOLON)) {
            return NULL;
        }
        return m;
    }
    if (m->contract == FN_PLAIN && accept(p, TOKEN_CONST)) {
        m->kind = ITEM_CONST;
        m->name_pos = pos_of(peek(p));
        if (!expect_name(p, &m->name) || !expect(p, TOKEN_COLON) ||
            (m->type = type(p)) == NULL || !expect(p, TOKEN_ASSIGN) ||
            (m->value = expression(p)) == NULL ||
            !expect(p, TOKEN_SEMICOLON)) {
            return NULL;
        }
        return m;
    }
    if (!expect(p, TOKEN_FN)) {
        return NULL;
    }
    m->kind = ITEM_FN;
    m->name_pos = pos_of(peek(p));
    if (!expect_member_name(p, &m->name)) {
        return NULL;
    }
    /* DESIGN: `concrete fn Serializable::f` fills the table of that base
       or interface alone. The qualifier is read as a name and a `::`, so
       the function name that follows it takes the place of the first. */
    if (check(p, TOKEN_COLON_COLON)) {
        if (m->contract != FN_CONCRETE) {
            error_here(p, "a `::` qualifier belongs to a `concrete fn`");
            return NULL;
        }
        next(p);
        m->qualifier = m->name;
        m->qualifier_pos = m->name_pos;
        m->name_pos = pos_of(peek(p));
        if (!expect_member_name(p, &m->name)) {
            return NULL;
        }
    }
    if (!type_params(p, m)) {
        return NULL;
    }
    m->params = params(p, false, &m->variadic, &m->has_self,
                       &m->param_count);
    if (p->panic ||
        !result_type(p, m)) {
        return NULL;
    }
    may_fail_after(p, m);
    if (!header_clauses(p)) {
        return NULL;
    }
    if (m->contract == FN_ABSTRACT) {
        return expect(p, TOKEN_SEMICOLON) ? m : NULL;
    }
    return (m->body = block(p)) == NULL ? NULL : m;
}

/* A member with the clauses of its header closed over it. */
static struct item *member(struct parser *p, const struct item *owner)
{
    size_t mark = p->clauses->count;
    struct pos from = pos_of(peek(p));
    struct item *m = member_level(p, owner);

    close_clauses(p, mark, m != NULL ? declaration_start(m) : from);
    return m;
}

/* DESIGN: `compatible 1.1;` in the body of an abstract class names the
   lowest version a plugin may have been built for. `compatible` is a
   contextual word in that position alone, so a field may still carry
   the name. The version is the source text of the number the lexer read
   as an integer or a float, with any further `.<integer>` parts after
   it, since `1.1.0` is no number of Anti. */
static bool compatible_line(struct parser *p, struct item *it)
{
    const struct token *start;
    size_t end;

    next(p);
    if (it->compatible.length > 0) {
        error_here(p, "a class has one `compatible` line");
        return false;
    }
    it->compatible_pos = pos_of(peek(p));
    start = peek(p);
    if (start->kind != TOKEN_INT && start->kind != TOKEN_FLOAT) {
        error_here(p, "`compatible` names a version, as `compatible 1.1;`");
        return false;
    }
    next(p);
    end = start->offset + start->length;
    while (check(p, TOKEN_DOT) && peek_at(p, 1)->kind == TOKEN_INT) {
        next(p);
        end = peek(p)->offset + peek(p)->length;
        next(p);
    }
    it->compatible.text = p->source + start->offset;
    it->compatible.length = end - start->offset;
    return expect(p, TOKEN_SEMICOLON);
}

/* `inherits` in a class body, where the base stood before it moved to
   the header. The message writes the header the programmer means, with
   the base as the body named it. The base and a comma after it are read
   and dropped, so the rest of the body parses and reports its own
   errors. Returns whether a comma followed. */
static bool inherits_in_body(struct parser *p, const struct item *it)
{
    const struct token *base = peek_at(p, 1);
    const struct token *dot = peek_at(p, 2);
    const struct token *name = peek_at(p, 3);
    size_t length = 0;
    char message[192];

    if (base->kind == TOKEN_IDENT) {
        length = base->length;
        if (dot->kind == TOKEN_DOT && name->kind == TOKEN_IDENT) {
            length = name->offset + name->length - base->offset;
        }
    }
    snprintf(message, sizeof message,
             "inherits belongs in the class header: class %.*s inherits %.*s",
             (int)it->name.length, it->name.text, (int)length,
             p->source + base->offset);
    error_here(p, message);
    next(p);
    if (accept(p, TOKEN_IDENT) && accept(p, TOKEN_DOT)) {
        accept(p, TOKEN_IDENT);
    }
    p->panic = false;
    return accept(p, TOKEN_COMMA);
}

static struct item *item(struct parser *p);

/* Whether the next tokens declare a type in a class body: the words that
   may stand before a type, then the word of its kind. */
static bool starts_nested(const struct parser *p)
{
    size_t i;

    for (i = 0; i < 4; i++) {
        const struct token *t = peek_at(p, i);
        switch (t->kind) {
        case TOKEN_STRUCT:
        case TOKEN_UNION:
        case TOKEN_ENUM:
        case TOKEN_CLASS:
        case TOKEN_VARIANT:
            return true;
        case TOKEN_PUB:
        case TOKEN_EXPORT:
        case TOKEN_INTERNAL:
        case TOKEN_PROTECTED:
        case TOKEN_ABSTRACT:
        case TOKEN_SINGLETON:
            continue;
        default:
            if (!is_word(p, t, "packed") && !is_word(p, t, "simd") &&
                !is_word(p, t, "final") && !is_word(p, t, "trace")) {
                return false;
            }
        }
    }
    return false;
}

/* DESIGN: a class body may declare a struct, an enum or a class, as
   "Nested types" in docs/anti-language-additions.md gives. The type is
   private to the class, so no visibility word stands before it. The
   union and the variant stay at module level, since the additions name
   the three kinds alone. */
static bool nested_type(struct parser *p, struct list *nested)
{
    const struct token *word = peek(p);
    struct item *inner;

    /* Both refusals leave the declaration whole, so the rest of the
       body parses and reports its own errors. */
    switch (word->kind) {
    case TOKEN_PUB:
    case TOKEN_EXPORT:
    case TOKEN_INTERNAL:
    case TOKEN_PROTECTED:
        diagnostics_add(p->diags, word->line, word->column,
                        "a type declared in a class is private to the "
                        "class, and takes no `pub`, `export`, `internal` "
                        "or `protected`");
        p->ok = false;
        next(p);
        break;
    default:
        break;
    }
    inner = item(p);
    if (inner == NULL) {
        return false;
    }
    if (inner->kind != ITEM_STRUCT && inner->kind != ITEM_ENUM &&
        inner->kind != ITEM_CLASS) {
        diagnostics_add(p->diags, inner->name_pos.line,
                        inner->name_pos.column,
                        "a class body declares a struct, an enum or a "
                        "class, and a %s stands at module level",
                        inner->kind == ITEM_UNION ? "union" : "variant");
        p->ok = false;
        return true;
    }
    list_push(nested, &inner);
    return true;
}

/* Read the functions and constants of a body into it->members, and the
   types of a class body into nested, which is NULL for an enum. */
static bool members_of(struct parser *p, struct item *it,
                       struct list *nested)
{
    struct list list = {NULL, 0, 0, sizeof(struct item *)};

    while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
        struct item *m;
        if (it->kind == ITEM_CLASS && check(p, TOKEN_INHERITS)) {
            inherits_in_body(p, it);
            continue;
        }
        if (nested != NULL && starts_nested(p)) {
            if (!nested_type(p, nested)) {
                free(list.data);
                return false;
            }
            continue;
        }
        m = member(p, it);
        if (m == NULL) {
            free(list.data);
            return false;
        }
        list_push(&list, &m);
    }
    it->members = list_finish(p, &list, &it->member_count);
    return true;
}

/* A struct body holds fields alone. Report what the programmer wrote and
   name the kind that takes it. */
static bool struct_field_only(struct parser *p, const struct item *it)
{
    const char *what = NULL;
    char message[96];

    switch (peek(p)->kind) {
    case TOKEN_FN:
    case TOKEN_PUB:
    case TOKEN_EXPORT:
    case TOKEN_ABSTRACT:
    case TOKEN_CONCRETE:
        what = "a function";
        break;
    case TOKEN_CONST:
        what = "a constant";
        break;
    case TOKEN_STATIC:
        what = "a static field";
        break;
    case TOKEN_USE:
        what = "a `use` field";
        break;
    case TOKEN_INHERITS:
        what = "a base";
        break;
    case TOKEN_IMPLEMENTS:
        what = "an interface";
        break;
    case TOKEN_PROTECTED:
        what = "a protected member";
        break;
    default:
        return true;
    }
    snprintf(message, sizeof message,
             "a %s holds fields alone, and %s belongs to a class",
             it->kind == ITEM_UNION ? "union" : "struct", what);
    error_here(p, message);
    return false;
}

/* DESIGN: `guarded by lock` after a field's type or its default names
   the Mutex that a `sync` holds wherever the field is reached, and
   `guarded by PeopleList.lock` names the lock of an enclosing object by
   its class. `guarded` and `by` are contextual words. */
static bool guard_clause(struct parser *p, struct param *field)
{
    if (field->guard.length > 0 || !is_word(p, peek(p), "guarded") ||
        !is_word(p, peek_at(p, 1), "by")) {
        return true;
    }
    field->guard_pos = pos_of(peek(p));
    next(p);
    next(p);
    if (!expect_name(p, &field->guard)) {
        return false;
    }
    if (accept(p, TOKEN_DOT)) {
        field->guard_class = field->guard;
        if (!expect_name(p, &field->guard)) {
            return false;
        }
    }
    return true;
}

/* Whether a clause read since mark is `unchecked(unguarded-field)`. */
static bool unchecks_guard(const struct parser *p, size_t mark)
{
    const struct clause *all = p->clauses->data;
    size_t i;

    for (i = mark; i < p->clauses->count; i++) {
        if (all[i].unchecked &&
            warnings_find(all[i].name.text, all[i].name.length) ==
                NAME_UNGUARDED_FIELD) {
            return true;
        }
    }
    return false;
}

/* `unchecked(name, "reason")` after a field's type overrules a safety
   check for that field alone. A warning of a field is one of its class,
   so `allow` stands in the class header. */
static bool field_clauses(struct parser *p)
{
    while (clause_ahead(p, 0)) {
        if (is_word(p, peek(p), "allow")) {
            error_here(p, "`allow` stands before a statement, last in a "
                          "declaration's header or at the top of the file");
            return false;
        }
        if (!read_clause(p, CLAUSE_FIELD)) {
            return false;
        }
    }
    return true;
}

/* `class Circle inherits Shape { implements ser: Serializable, ... }`.
   DESIGN: the base is no member. It sits at offset 0, has no name of its
   own, is reached as `self.super`, and a class has at most one, so the
   header names it. `implements` and `use` open the body, because each is
   a named sub-object with a place in the layout. Then come the fields,
   then the constants and the functions. */
static struct item *class_item(struct parser *p, struct item *it)
{
    struct list fields = {NULL, 0, 0, sizeof(struct param)};
    struct list nested = {NULL, 0, 0, sizeof(struct item *)};
    size_t mark;

    next(p);
    it->kind = ITEM_CLASS;
    if (!expect_name(p, &it->name) || !type_params(p, it)) {
        return NULL;
    }
    /* DESIGN: `align(N)` stays directly after the name, as on a struct,
       and the base follows it. */
    if (is_word(p, peek(p), "align")) {
        next(p);
        if (!expect(p, TOKEN_LPAREN) || (it->align = expression(p)) == NULL ||
            !expect(p, TOKEN_RPAREN)) {
            return NULL;
        }
    }
    /* The base is one name, qualified by its module or not, and no field
       of its own. The checker builds the `super` field from it. */
    if (check(p, TOKEN_INHERITS)) {
        it->base_pos = pos_of(peek(p));
        next(p);
        if (!expect_name(p, &it->base_name) ||
            (accept(p, TOKEN_DOT) &&
             (it->base_module = it->base_name,
              !expect_name(p, &it->base_name)))) {
            return NULL;
        }
        if (check(p, TOKEN_LT) &&
            (it->base_args = type_args(p, &it->base_arg_count)) == NULL) {
            return NULL;
        }
        if (check(p, TOKEN_COMMA) || check(p, TOKEN_INHERITS)) {
            error_here(p, "a class has one base");
            return NULL;
        }
    }
    mark = p->clauses->count;
    if (!header_clauses(p) || !expect(p, TOKEN_LBRACE)) {
        return NULL;
    }
    it->unchecked_fields = unchecks_guard(p, mark);
    /* Fields first, comma separated, then the constants and functions,
       each ended by its own `;` or block. */
    while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF) &&
           (!starts_member(p) || starts_nested(p))) {
        struct param field;
        if (starts_nested(p)) {
            if (!nested_type(p, &nested)) {
                free(fields.data);
                free(nested.data);
                return NULL;
            }
            continue;
        }
        memset(&field, 0, sizeof field);
        field.doc = doc_before(p, TOKEN_DOC);
        field.note = doc_before(p, TOKEN_NOTE);
        field.pos = pos_of(peek(p));
        if (check(p, TOKEN_INHERITS)) {
            if (!inherits_in_body(p, it)) {
                break;
            }
            continue;
        }
        if (is_word(p, peek(p), "compatible") &&
            (peek_at(p, 1)->kind == TOKEN_INT ||
             peek_at(p, 1)->kind == TOKEN_FLOAT)) {
            if (!compatible_line(p, it)) {
                break;
            }
            continue;
        }
        if (accept(p, TOKEN_IMPLEMENTS)) {
            field.form = FIELD_IMPL;
        } else if (accept(p, TOKEN_USE)) {
            field.form = FIELD_USE;
        }
        if (field.form == FIELD_PLAIN) {
            if (accept(p, TOKEN_PUB)) {
                field.vis = VIS_PUB;
            } else if (accept(p, TOKEN_PROTECTED)) {
                field.vis = VIS_PROTECTED;
            }
        }
        /* DESIGN: `inject` and `inject final` name a field a provider
           fills before `construct` runs. Both are contextual, so
           `inject: int` is still a field named `inject` and
           `inject final: *L` a field named `final` a provider fills.
           The `final` form is read first, because its second word is a
           name as well. */
        if (is_word(p, peek(p), "inject") &&
            is_word(p, peek_at(p, 1), "final") &&
            is_field_name(peek_at(p, 2))) {
            next(p);
            next(p);
            field.injected = true;
            field.inject_final = true;
        } else if (is_word(p, peek(p), "inject") &&
                   is_field_name(peek_at(p, 1))) {
            next(p);
            field.injected = true;
        }
        /* `transient`, `own`, `atomic` and `mutable` are contextual
           words before a field name, so a field may still carry one of
           those names. */
        if (is_word(p, peek(p), "transient") &&
            (peek_at(p, 1)->kind == TOKEN_IDENT ||
             peek_at(p, 1)->kind == TOKEN_ATOMIC)) {
            next(p);
            field.transient = true;
        }
        if (is_word(p, peek(p), "own") && peek_at(p, 1)->kind == TOKEN_IDENT) {
            next(p);
            field.owned = true;
        }
        if (is_word(p, peek(p), "mutable") &&
            peek_at(p, 1)->kind == TOKEN_IDENT) {
            next(p);
            field.writable = true;
        }
        if (check(p, TOKEN_ATOMIC) && peek_at(p, 1)->kind == TOKEN_IDENT) {
            next(p);
            field.atomic = true;
        }
        /* An `inject` field may be called `alloc` or `free`, as the
           example of `docs/anti-syntax-overview.md` writes it. A name
           must stand after `inject`, so the position has one reading. */
        mark = p->clauses->count;
        if (!(field.injected ? expect_member_name(p, &field.name)
                             : expect_name(p, &field.name)) ||
            !expect(p, TOKEN_COLON) ||
            (field.type = type(p)) == NULL || !guard_clause(p, &field) ||
            !field_clauses(p) ||
            (accept(p, TOKEN_COLON) &&
             (field.bits = expression(p)) == NULL) ||
            (accept(p, TOKEN_ASSIGN) &&
             (field.value = expression(p)) == NULL) ||
            !guard_clause(p, &field) || !field_clauses(p)) {
            free(fields.data);
            free(nested.data);
            return NULL;
        }
        field.unchecked = unchecks_guard(p, mark);
        close_clauses(p, mark, field.pos);
        list_push(&fields, &field);
        if (!accept(p, TOKEN_COMMA)) {
            break;
        }
    }
    it->params = list_finish(p, &fields, &it->param_count);
    if (!members_of(p, it, &nested)) {
        free(nested.data);
        return NULL;
    }
    it->nested = list_finish(p, &nested, &it->nested_count);
    return expect(p, TOKEN_RBRACE) ? it : NULL;
}

/* DESIGN: a variant body holds its cases and nothing else. A case is a
   name and, in braces, the fields it carries, each a name and a type. A
   variant is a struct, so no function, constant, default or bitfield
   stands in it. */
static struct item *variant_item(struct parser *p, struct item *it)
{
    struct list cases = {NULL, 0, 0, sizeof(struct variant_case)};

    next(p);
    it->kind = ITEM_VARIANT;
    if (!expect_name(p, &it->name) || !type_params(p, it)) {
        return NULL;
    }
    if (is_word(p, peek(p), "align")) {
        next(p);
        if (!expect(p, TOKEN_LPAREN) || (it->align = expression(p)) == NULL ||
            !expect(p, TOKEN_RPAREN)) {
            return NULL;
        }
    }
    if (!expect(p, TOKEN_LBRACE)) {
        return NULL;
    }
    while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
        struct variant_case one;
        memset(&one, 0, sizeof one);
        one.doc = doc_before(p, TOKEN_DOC);
        doc_before(p, TOKEN_NOTE);
        one.pos = pos_of(peek(p));
        if (!expect_name(p, &one.name)) {
            free(cases.data);
            return NULL;
        }
        if (accept(p, TOKEN_LBRACE)) {
            struct list fields = {NULL, 0, 0, sizeof(struct param)};
            while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
                struct param field;
                memset(&field, 0, sizeof field);
                field.doc = doc_before(p, TOKEN_DOC);
                field.note = doc_before(p, TOKEN_NOTE);
                field.pos = pos_of(peek(p));
                if (!expect_name(p, &field.name) || !expect(p, TOKEN_COLON) ||
                    (field.type = type(p)) == NULL) {
                    free(fields.data);
                    free(cases.data);
                    return NULL;
                }
                list_push(&fields, &field);
                if (!accept(p, TOKEN_COMMA)) {
                    break;
                }
            }
            one.fields = list_finish(p, &fields, &one.field_count);
            if (!expect(p, TOKEN_RBRACE)) {
                free(cases.data);
                return NULL;
            }
        }
        list_push(&cases, &one);
        if (!accept(p, TOKEN_COMMA)) {
            break;
        }
    }
    it->cases = list_finish(p, &cases, &it->case_count);
    return expect(p, TOKEN_RBRACE) ? it : NULL;
}

/* Whether `class` stands at the token ahead, or `synchronized class` or
   `concurrent class` does. */
static bool class_ahead(const struct parser *p, size_t ahead)
{
    const struct token *t = peek_at(p, ahead);

    return t->kind == TOKEN_CLASS ||
           ((is_word(p, t, "synchronized") || is_word(p, t, "concurrent")) &&
            peek_at(p, ahead + 1)->kind == TOKEN_CLASS);
}

static struct item *item_level(struct parser *p)
{
    const struct token *start = peek(p);
    struct item *it = node(p, sizeof *it);

    it->pos = pos_of(start);
    it->doc = doc_before(p, TOKEN_DOC);
    it->note = doc_before(p, TOKEN_NOTE);
    it->exported = accept(p, TOKEN_EXPORT);
    it->pub = it->exported || accept(p, TOKEN_PUB);
    it->vis = it->pub ? VIS_PUB : VIS_PRIVATE;
    /* DESIGN: `internal` reaches every module under the same package
       root. It is a level of a module item, and never of a class
       member. */
    if (!it->pub && accept(p, TOKEN_INTERNAL)) {
        it->vis = VIS_INTERNAL;
    } else if (check(p, TOKEN_PROTECTED)) {
        error_here(p, "`protected` marks a class member, and a module item "
                      "is `pub`, `internal` or neither");
        return NULL;
    }
    /* DESIGN: export marks items that Anti defines for C. An extern fn is
       defined in C already. */
    if (it->exported && peek(p)->kind == TOKEN_EXTERN) {
        error_here(p, "an `extern fn` cannot be exported");
        return NULL;
    }
    /* DESIGN: worker marks a function that a `parallel` may run on
       another thread. It stands before fn, after pub, and no other item
       takes it. */
    /* DESIGN: `abstract` marks a class with an open function, and the
       contextual `final` forbids inheritance. Both stand before `class`,
       after `pub`. */
    /* DESIGN: `trace` marks a class whose `pub` functions call the enter
       and leave hooks. It is a contextual word before `class`, and
       before `fn` in a class body, where it marks one function. A module
       function has no object to hook, so `trace` before one is refused. */
    if (is_word(p, peek(p), "trace") &&
        (class_ahead(p, 1) || peek_at(p, 1)->kind == TOKEN_FN)) {
        next(p);
        if (!class_ahead(p, 0)) {
            error_here(p, "`trace` marks a class or a function of one");
            return NULL;
        }
        it->trace = true;
    }
    if (peek(p)->kind == TOKEN_ABSTRACT && class_ahead(p, 1)) {
        next(p);
        it->is_abstract = true;
    } else if (is_word(p, peek(p), "final") && class_ahead(p, 1)) {
        next(p);
        it->is_final = true;
    } else if (peek(p)->kind == TOKEN_SINGLETON && class_ahead(p, 1)) {
        next(p);
        it->is_singleton = true;
    }
    /* DESIGN: `synchronized` and `concurrent` are contextual words
       directly before `class`, so a parameter or a field may still carry
       either name. */
    if (is_word(p, peek(p), "synchronized") &&
        peek_at(p, 1)->kind == TOKEN_CLASS) {
        next(p);
        it->synchronized = true;
    } else if (is_word(p, peek(p), "concurrent") &&
               peek_at(p, 1)->kind == TOKEN_CLASS) {
        next(p);
        it->concurrent = true;
    }
    /* `operator fn` at module level gives a struct its operators, since
       a struct holds no functions of its own. */
    if (is_word(p, peek(p), "operator") && peek_at(p, 1)->kind == TOKEN_FN) {
        next(p);
        it->is_operator = true;
    }
    it->worker = accept(p, TOKEN_WORKER);
    if (it->worker && peek(p)->kind != TOKEN_FN) {
        error_here(p, "`worker` stands before `fn`");
        return NULL;
    }
    /* DESIGN: packed and align are contextual words. packed is one only
       directly before struct, union or variant, and align only between
       the name of one of them and its opening brace. */
    if (is_word(p, peek(p), "packed") &&
        (peek_at(p, 1)->kind == TOKEN_STRUCT ||
         peek_at(p, 1)->kind == TOKEN_UNION ||
         peek_at(p, 1)->kind == TOKEN_CLASS ||
         peek_at(p, 1)->kind == TOKEN_VARIANT)) {
        next(p);
        it->packed = true;
    }
    /* DESIGN: simd is a contextual word as well, and one only directly
       before struct. */
    if (is_word(p, peek(p), "simd") && peek_at(p, 1)->kind == TOKEN_STRUCT) {
        next(p);
        it->simd = true;
    }
    it->name_pos = pos_of(peek_at(p, 1));
    if (peek(p)->kind == TOKEN_EXTERN) {
        it->name_pos = pos_of(peek_at(p, 2));
    }
    switch (peek(p)->kind) {
    case TOKEN_FN:
        next(p);
        it->kind = ITEM_FN;
        if (!expect_name(p, &it->name) || !type_params(p, it)) {
            return NULL;
        }
        it->params = params(p, false, &it->variadic, NULL,
                            &it->param_count);
        if (p->panic ||
            !result_type(p, it)) {
            return NULL;
        }
        may_fail_after(p, it);
        if (p->panic || !header_clauses(p) ||
            (it->body = block(p)) == NULL) {
            return NULL;
        }
        return it;
    case TOKEN_EXTERN:
        next(p);
        it->kind = ITEM_EXTERN_FN;
        if (!expect(p, TOKEN_FN) || !expect_name(p, &it->name)) {
            return NULL;
        }
        it->params = params(p, true, &it->variadic, NULL,
                            &it->param_count);
        if (p->panic ||
            !result_type(p, it)) {
            return NULL;
        }
        /* A binding declares what C declares, and C has no error
           channel. A C function that reports one returns `?*Error` by
           hand. */
        if (is_word(p, peek(p), "may") && peek_at(p, 1)->kind == TOKEN_FAIL) {
            error_here(p, "`may fail` belongs to an Anti function, and an "
                          "`extern fn` writes `-> ?*Error` by hand");
            return NULL;
        }
        if (!expect(p, TOKEN_SEMICOLON)) {
            return NULL;
        }
        return it;
    case TOKEN_STRUCT:
    case TOKEN_UNION: {
        struct list fields = {NULL, 0, 0, sizeof(struct param)};
        it->kind = next(p)->kind == TOKEN_UNION ? ITEM_UNION : ITEM_STRUCT;
        if (!expect_name(p, &it->name) ||
            (it->kind == ITEM_STRUCT && !type_params(p, it))) {
            return NULL;
        }
        if (is_word(p, peek(p), "align")) {
            next(p);
            if (!expect(p, TOKEN_LPAREN) ||
                (it->align = expression(p)) == NULL ||
                !expect(p, TOKEN_RPAREN)) {
                return NULL;
            }
        }
        if (!header_clauses(p) || !expect(p, TOKEN_LBRACE)) {
            return NULL;
        }
        /* DESIGN: a struct body holds fields and nothing else. It is
           exactly the bytes C declares, so a function, a constant, a
           default or a base belongs to a class. */
        do {
            struct param field;
            size_t mark;
            memset(&field, 0, sizeof field);
            field.doc = doc_before(p, TOKEN_DOC);
            field.note = doc_before(p, TOKEN_NOTE);
            field.pos = pos_of(peek(p));
            if (!struct_field_only(p, it)) {
                free(fields.data);
                return NULL;
            }
            mark = p->clauses->count;
            if (!expect_name(p, &field.name) || !expect(p, TOKEN_COLON) ||
                (field.type = type(p)) == NULL || !guard_clause(p, &field) ||
                !field_clauses(p) ||
                (accept(p, TOKEN_COLON) &&
                 (field.bits = expression(p)) == NULL)) {
                free(fields.data);
                return NULL;
            }
            field.unchecked = unchecks_guard(p, mark);
            close_clauses(p, mark, field.pos);
            if (check(p, TOKEN_ASSIGN)) {
                error_here(p, "a field of a struct has no default, which "
                              "belongs to a class");
                free(fields.data);
                return NULL;
            }
            list_push(&fields, &field);
        } while (accept(p, TOKEN_COMMA) && !check(p, TOKEN_RBRACE));
        it->params = list_finish(p, &fields, &it->param_count);
        return expect(p, TOKEN_RBRACE) ? it : NULL;
    }
    case TOKEN_CLASS:
        return class_item(p, it);
    case TOKEN_VARIANT:
        return variant_item(p, it);
    case TOKEN_ENUM: {
        struct list values = {NULL, 0, 0, sizeof(struct param)};
        next(p);
        it->kind = ITEM_ENUM;
        if (!expect_name(p, &it->name)) {
            return NULL;
        }
        /* `enum Mode: u8` names the underlying type. Without one the type
           is c_int, which the checker fills in. */
        if (accept(p, TOKEN_COLON) && (it->base = type(p)) == NULL) {
            return NULL;
        }
        if (!expect(p, TOKEN_LBRACE)) {
            return NULL;
        }
        while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF) &&
               !starts_member(p)) {
            struct param value;
            memset(&value, 0, sizeof value);
            value.doc = doc_before(p, TOKEN_DOC);
            value.note = doc_before(p, TOKEN_NOTE);
            value.pos = pos_of(peek(p));
            if (!expect_name(p, &value.name) ||
                (accept(p, TOKEN_ASSIGN) &&
                 (value.value = expression(p)) == NULL)) {
                free(values.data);
                return NULL;
            }
            list_push(&values, &value);
            if (!accept(p, TOKEN_COMMA)) {
                break;
            }
        }
        it->params = list_finish(p, &values, &it->param_count);
        if (!members_of(p, it, NULL)) {
            return NULL;
        }
        return expect(p, TOKEN_RBRACE) ? it : NULL;
    }
    case TOKEN_CONSTRAINT:
        /* `constraint Ordered = eq + lt;` names a set of constraints. C
           has none, so a constraint is never exported. */
        if (it->exported) {
            error_here(p, "a `constraint` is not exported, since C has no "
                          "generics");
            return NULL;
        }
        next(p);
        it->kind = ITEM_CONSTRAINT;
        if (!expect_name(p, &it->name) || !expect(p, TOKEN_ASSIGN) ||
            (it->constraints = constraint_list(
                 p, &it->constraint_count)) == NULL ||
            !expect(p, TOKEN_SEMICOLON)) {
            return NULL;
        }
        return it;
    case TOKEN_TYPE:
        /* `type People = List<Person>;` names a type, and
           `export type PersonList = List<Person>;` offers it to C. */
        next(p);
        it->kind = ITEM_TYPE;
        if (!expect_name(p, &it->name) || !expect(p, TOKEN_ASSIGN) ||
            (it->type = type(p)) == NULL || !expect(p, TOKEN_SEMICOLON)) {
            return NULL;
        }
        return it;
    case TOKEN_CONST:
        next(p);
        it->kind = ITEM_CONST;
        if (!expect_name(p, &it->name) || !expect(p, TOKEN_COLON) ||
            (it->type = type(p)) == NULL || !expect(p, TOKEN_ASSIGN) ||
            (it->value = expression(p)) == NULL ||
            !expect(p, TOKEN_SEMICOLON)) {
            return NULL;
        }
        return it;
    default:
        error_here(p, "expected an item");
        return NULL;
    }
}

/* An item with the clauses of its header closed over it. */
static struct item *item(struct parser *p)
{
    size_t mark = p->clauses->count;
    struct pos from = pos_of(peek(p));
    struct item *it = item_level(p);

    close_clauses(p, mark, it != NULL ? declaration_start(it) : from);
    return it;
}

/* DESIGN: a clause of the whole file stands at the top of the module,
   among the imports and before the first item, and ends with `;`. It
   covers every line of the file. */
static bool file_clause(struct parser *p)
{
    struct clause *c;

    if (!read_clause(p, CLAUSE_FILE)) {
        return false;
    }
    c = (struct clause *)p->clauses->data + (p->clauses->count - 1);
    c->from.line = 1;
    c->from.column = 1;
    c->to.line = INT_MAX;
    c->to.column = INT_MAX;
    return expect(p, TOKEN_SEMICOLON);
}

/* After an error in an import, skip the rest of its line up to and with
   its `;`. An import holds no braces, and a keyword in its path would
   stop sync_item inside it. */
static void sync_import(struct parser *p, int line)
{
    while (!check(p, TOKEN_EOF) && peek(p)->line == line) {
        if (next(p)->kind == TOKEN_SEMICOLON) {
            break;
        }
    }
    p->panic = false;
}

/* A module path: identifiers joined by dots. The name holds the path with
   its dots and without any space between the tokens. A dot before `{`
   opens the list of a direct import and ends the path. */
static bool module_path(struct parser *p, struct name *out)
{
    struct text path = {0};
    struct name segment;
    char *copy;

    do {
        if (!expect_name(p, &segment)) {
            text_free(&path);
            return false;
        }
        text_appendf(&path, "%s%.*s", path.length > 0 ? "." : "",
                     (int)segment.length, segment.text);
    } while (peek_at(p, 1)->kind != TOKEN_LBRACE && accept(p, TOKEN_DOT));
    copy = node(p, path.length + 1);
    memcpy(copy, text_cstr(&path), path.length + 1);
    out->text = copy;
    out->length = path.length;
    text_free(&path);
    return true;
}

/* The list of a direct import, `.{Builder, equal}`, after its path. The
   names stand in the order written, and `anti fmt` sorts them. */
static bool import_names(struct parser *p, struct import *imp)
{
    struct list names = {NULL, 0, 0, sizeof(struct import_name)};

    if (!check(p, TOKEN_DOT) || peek_at(p, 1)->kind != TOKEN_LBRACE) {
        return true;
    }
    next(p);
    next(p);
    if (check(p, TOKEN_RBRACE)) {
        error_here(p, "a direct import lists at least one name");
        return false;
    }
    do {
        struct import_name n;
        n.pos = pos_of(peek(p));
        if (!expect_name(p, &n.name)) {
            free(names.data);
            return false;
        }
        list_push(&names, &n);
    } while (accept(p, TOKEN_COMMA));
    imp->names = list_finish(p, &names, &imp->name_count);
    return expect(p, TOKEN_RBRACE);
}

/* An item of the module, after every type nested in it. A nested type
   takes its full name, the name of the class, a dot and the name as
   written. The name
   of the class is complete before the types inside it take theirs, so a
   type nested two deep is `A.B.C`. The nested types come first, so the
   checker has declared their fields when a default of the class names
   one. */
static void push_item(struct parser *p, struct list *items, struct item *it)
{
    size_t i;

    for (i = 0; i < it->nested_count; i++) {
        struct item *inner = it->nested[i];
        size_t length = it->name.length + 1 + inner->name.length;
        char *full = node(p, length + 1);
        memcpy(full, it->name.text, it->name.length);
        full[it->name.length] = '.';
        memcpy(full + it->name.length + 1, inner->name.text,
               inner->name.length);
        full[length] = '\0';
        inner->local_name = inner->name;
        inner->name.text = full;
        inner->name.length = length;
        inner->outer = it;
        push_item(p, items, inner);
    }
    list_push(items, &it);
}

/* DESIGN: `tests { }` and `fixtures { }` hold functions and nothing else.
   Each one becomes an ordinary module function carrying the block it was
   written in, so every pass after this one reads a module of functions.
   The build that is not `anti test` drops them before checking, which is
   how a dev build, a release build and a `.antl` contain none of it. */
static bool test_block(struct parser *p, struct list *items,
                       enum fn_block which)
{
    next(p);
    if (!expect(p, TOKEN_LBRACE)) {
        return false;
    }
    while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
        struct item *it;
        p->block = which;
        it = item(p);
        p->block = BLOCK_NONE;
        if (it == NULL) {
            return false;
        }
        if (it->kind != ITEM_FN) {
            diagnostics_add(p->diags, it->pos.line, it->pos.column,
                            "`%s` holds functions and nothing else",
                            which == BLOCK_TESTS ? "tests" : "fixtures");
            p->ok = false;
            return false;
        }
        it->block = which;
        list_push(items, &it);
    }
    return expect(p, TOKEN_RBRACE);
}

/* `provides Interface as Class;` at module level. The interface is one
   dotted path. The last name is the interface. The names before it are
   its module, written as an alias or as the whole path. */
static bool provides_line(struct parser *p, struct list *out)
{
    struct provides pr;
    struct name path;
    size_t dot;

    memset(&pr, 0, sizeof pr);
    pr.pos = pos_of(peek(p));
    next(p);
    pr.interface_pos = pos_of(peek(p));
    if (!module_path(p, &path)) {
        return false;
    }
    dot = path.length;
    while (dot > 0 && path.text[dot - 1] != '.') {
        dot--;
    }
    if (dot > 0) {
        pr.qualifier.text = path.text;
        pr.qualifier.length = dot - 1;
        pr.interface.text = path.text + dot;
        pr.interface.length = path.length - dot;
    } else {
        pr.interface = path;
    }
    if (!expect(p, TOKEN_AS)) {
        return false;
    }
    pr.class_pos = pos_of(peek(p));
    if (!expect_name(p, &pr.class_name) || !expect(p, TOKEN_SEMICOLON)) {
        return false;
    }
    list_push(out, &pr);
    return true;
}

/* DESIGN: `link framework "Name";` names a framework of Apple's SDK at
   module level, and `link linux "Name";` a library of the glibc sysroot.
   `link`, `framework` and `linux` are contextual words there alone, so
   each stays a name everywhere else. A binding carries the line and the
   library file records it, and `anti` passes the names to antic as
   --framework and --linux-lib. */
static bool link_line(struct parser *p, struct list *out, const char *kind)
{
    struct link_name line;
    const struct token *t;

    memset(&line, 0, sizeof line);
    line.pos = pos_of(peek(p));
    next(p);
    next(p);
    t = peek(p);
    if (t->kind != TOKEN_STRING) {
        error_here(p, strcmp(kind, "linux") == 0
                          ? "`link linux` takes the name of a library as a "
                            "string"
                          : "`link framework` takes the name of a framework "
                            "as a string");
        return false;
    }
    next(p);
    line.name.text = t->value.text.bytes;
    line.name.length = t->value.text.length;
    if (!expect(p, TOKEN_SEMICOLON)) {
        return false;
    }
    list_push(out, &line);
    return true;
}

bool parse(const char *source, const struct token_list *tokens,
           struct arena *arena, struct diagnostics *diags,
           struct module **out)
{
    struct list clauses = {NULL, 0, 0, sizeof(struct clause)};
    struct parser p = {source, NULL, tokens->items, NULL, NULL, 0, arena,
                       diags, false, true, false, 0, &clauses, BLOCK_NONE,
                       false, 0};
    struct module *m = arena_alloc(arena, sizeof *m);
    struct list imports = {NULL, 0, 0, sizeof(struct import)};
    struct list items = {NULL, 0, 0, sizeof(struct item *)};
    struct list provides = {NULL, 0, 0, sizeof(struct provides)};
    struct list frameworks = {NULL, 0, 0, sizeof(struct link_name)};
    struct list linux_libraries = {NULL, 0, 0, sizeof(struct link_name)};
    struct list dropped = {NULL, 0, 0, sizeof(struct dropped_doc)};
    struct token *kept = malloc(tokens->count * sizeof *kept);
    size_t *origin = malloc(tokens->count * sizeof *origin);
    bool *taken = calloc(tokens->count, sizeof *taken);
    size_t count = 0;
    size_t i;
    bool saw_tests = false;
    bool saw_fixtures = false;

    if (kept == NULL || origin == NULL || taken == NULL) {
        fputs("antic: out of memory\n", stderr);
        exit(70);
    }
    /* DESIGN: the grammar rules never see a doc comment. Items, fields and
       the module ask for the comments that precede them, so a comment in
       any other place is dropped. */
    for (i = 0; i < tokens->count; i++) {
        if (!is_doc(tokens->items[i].kind)) {
            origin[count] = i;
            kept[count++] = tokens->items[i];
        }
    }
    p.tokens = kept;
    p.origin = origin;
    p.taken = taken;
    m->doc = doc_before(&p, TOKEN_MODULE_DOC);
    m->note = doc_before(&p, TOKEN_MODULE_NOTE);

    while (check(&p, TOKEN_IMPORT) || clause_ahead(&p, 0)) {
        struct import imp;
        if (clause_ahead(&p, 0)) {
            if (!file_clause(&p)) {
                sync_import(&p, peek(&p)->line);
            }
            continue;
        }
        memset(&imp, 0, sizeof imp);
        imp.pos = pos_of(next(&p));
        imp.module_pos = pos_of(peek(&p));
        if (module_path(&p, &imp.module) && import_names(&p, &imp) &&
            (imp.name_count > 0 || !accept(&p, TOKEN_AS) ||
             expect_name(&p, &imp.alias)) &&
            expect(&p, TOKEN_SEMICOLON)) {
            list_push(&imports, &imp);
        } else {
            sync_import(&p, imp.pos.line);
        }
    }
    while (!check(&p, TOKEN_EOF)) {
        size_t before = p.pos;
        struct item *it;
        if (check(&p, TOKEN_TESTS) || check(&p, TOKEN_FIXTURES)) {
            bool is_tests = check(&p, TOKEN_TESTS);
            bool *seen = is_tests ? &saw_tests : &saw_fixtures;
            if (*seen) {
                diagnostics_add(diags, peek(&p)->line, peek(&p)->column,
                                "a module has at most one `%s` block",
                                is_tests ? "tests" : "fixtures");
                p.ok = false;
            }
            *seen = true;
            if (!test_block(&p, &items,
                            is_tests ? BLOCK_TESTS : BLOCK_FIXTURES)) {
                if (p.pos == before) {
                    next(&p);
                }
                sync_item(&p);
            }
            continue;
        }
        if (clause_ahead(&p, 0)) {
            error_here(&p, "a clause of the whole file stands at the top of "
                           "the module, and one of a declaration last in "
                           "its header");
            next(&p);
            sync_item(&p);
            continue;
        }
        if (check(&p, TOKEN_PROVIDES)) {
            if (!provides_line(&p, &provides)) {
                if (p.pos == before) {
                    next(&p);
                }
                sync_item(&p);
            }
            continue;
        }
        if (is_word(&p, peek(&p), "link") &&
            is_word(&p, peek_at(&p, 1), "framework")) {
            if (!link_line(&p, &frameworks, "framework")) {
                if (p.pos == before) {
                    next(&p);
                }
                sync_item(&p);
            }
            continue;
        }
        if (is_word(&p, peek(&p), "link") &&
            is_word(&p, peek_at(&p, 1), "linux")) {
            if (!link_line(&p, &linux_libraries, "linux")) {
                if (p.pos == before) {
                    next(&p);
                }
                sync_item(&p);
            }
            continue;
        }
        it = item(&p);
        if (it != NULL) {
            push_item(&p, &items, it);
        } else {
            if (p.pos == before) {
                next(&p);
            }
            sync_item(&p);
        }
    }
    m->imports = list_finish(&p, &imports, &m->import_count);
    m->items = list_finish(&p, &items, &m->item_count);
    m->provides = list_finish(&p, &provides, &m->provides_count);
    m->frameworks = list_finish(&p, &frameworks, &m->framework_count);
    m->linux_libraries = list_finish(&p, &linux_libraries,
                                     &m->linux_library_count);
    for (i = 0; i < tokens->count; i++) {
        const struct token *t = &tokens->items[i];
        struct dropped_doc d;
        if (!is_doc(t->kind) || taken[i]) {
            continue;
        }
        d.pos = pos_of(t);
        d.marker.text = source + t->offset;
        d.marker.length = marker_length(d.marker.text, t->length);
        d.module_form = t->kind == TOKEN_MODULE_DOC ||
                        t->kind == TOKEN_MODULE_NOTE;
        list_push(&dropped, &d);
    }
    m->dropped = list_finish(&p, &dropped, &m->dropped_count);
    m->clauses = list_finish(&p, &clauses, &m->clause_count);
    free(kept);
    free(origin);
    free(taken);
    *out = m;
    return p.ok;
}

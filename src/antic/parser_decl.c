/* The parts of a declaration: the marks, the parameters and the result
   of a signature, `may fail`, the type parameters with their constraints,
   and the members, nested types and fields of a body. */

#include <stdlib.h>
#include <string.h>

#include "parser_parser.h"
#include "warnings.h"

/* DESIGN: `keep` and `concurrent` are contextual words before a
   parameter of function type, in a parameter list and in the list of a
   function type. A mark stands before a name followed by a colon, or
   before `fn` or `?fn`, so a parameter or a type may still carry either
   name. `own` after `keep` makes `keep own`, a parameter that keeps and
   owns what it is given. */
bool parser_is_fn_mark(const struct parser *p, size_t at)
{
    const struct token *t = parser_peek_at(p, at);
    const struct token *after = parser_peek_at(p, at + 1);

    if (!parser_is_word(p, t, "keep") && !parser_is_word(p, t, "concurrent")) {
        return false;
    }
    return after->kind == TOKEN_FN ||
           (after->kind == TOKEN_QUESTION &&
            parser_peek_at(p, at + 2)->kind == TOKEN_FN) ||
           (after->kind == TOKEN_IDENT &&
            parser_peek_at(p, at + 2)->kind == TOKEN_COLON) ||
           parser_is_word(p, after, "keep") ||
           parser_is_word(p, after, "concurrent") ||
           (parser_is_word(p, after, "own") &&
            (parser_peek_at(p, at + 2)->kind == TOKEN_IDENT ||
             parser_peek_at(p, at + 2)->kind == TOKEN_FN ||
             parser_peek_at(p, at + 2)->kind == TOKEN_QUESTION));
}

/* Read the marks before a parameter. Both may stand, and the checker
   refuses the pair. `own` is read after a mark. */
void parser_fn_param_marks(struct parser *p, bool *keep, bool *concurrent,
                           bool *owned)
{
    while (parser_is_fn_mark(p, 0)) {
        if (parser_is_word(p, parser_peek(p), "keep")) {
            *keep = true;
        } else {
            *concurrent = true;
        }
        parser_next(p);
        if (parser_is_word(p, parser_peek(p), "own") &&
            (parser_peek_at(p, 1)->kind == TOKEN_IDENT ||
             parser_peek_at(p, 1)->kind == TOKEN_FN ||
             parser_peek_at(p, 1)->kind == TOKEN_QUESTION)) {
            parser_next(p);
            *owned = true;
        }
    }
}

/* `-> R` after the parameters of the function it, where R may be written
   `lent *T` or `lent ?*T`. It reports false after an error. `lent` is a
   contextual word, and no type name is followed by `*` or `?`. */
bool parser_result_type(struct parser *p, struct item *it)
{
    if (!parser_accept(p, TOKEN_ARROW)) {
        return true;
    }
    if (parser_is_word(p, parser_peek(p), "lent") &&
        (parser_peek_at(p, 1)->kind == TOKEN_STAR ||
         parser_peek_at(p, 1)->kind == TOKEN_QUESTION ||
         parser_peek_at(p, 1)->kind == TOKEN_QUESTION_STAR)) {
        it->result_lent = true;
        it->result_lent_pos = parser_pos_of(parser_peek(p));
        parser_next(p);
    }
    return (it->result = parser_type(p)) != NULL;
}

/* `lent name`: the pointer the parameter takes is valid only during the
   call. `lent` is a contextual word, so a parameter may carry that name. */
bool parser_lent_mark(struct parser *p)
{
    if (parser_is_word(p, parser_peek(p), "lent") &&
        parser_peek_at(p, 1)->kind == TOKEN_IDENT) {
        parser_next(p);
        return true;
    }
    return false;
}

/* A parenthesised list of name: type pairs, each with an optional
   `= value`. When allow_variadic is set, an ellipsis token may end the
   list. */
/* DESIGN: `self` is written as the first parameter and carries no type,
   because its type is always a pointer to the struct that declares the
   function. It reaches the parameter list as has_self, not as a param. */
struct param *parser_params(struct parser *p, bool allow_variadic,
                            bool *variadic, bool *has_self, size_t *count)
{
    struct list list = {NULL, 0, 0, sizeof(struct param)};

    if (!parser_expect(p, TOKEN_LPAREN)) {
        return NULL;
    }
    if (has_self != NULL && parser_check(p, TOKEN_SELF)) {
        parser_next(p);
        *has_self = true;
        if (!parser_accept(p, TOKEN_COMMA)) {
            return parser_expect(p, TOKEN_RPAREN)
                       ? parser_list_finish(p, &list, count)
                       : NULL;
        }
    }
    while (!parser_check(p, TOKEN_RPAREN)) {
        struct param param;
        memset(&param, 0, sizeof param);
        if (allow_variadic && list.count > 0 &&
            parser_accept(p, TOKEN_ELLIPSIS)) {
            *variadic = true;
            parser_accept(p, TOKEN_COMMA);
            break;
        }
        /* `own name: T`: the function takes over the object. `own` is a
           contextual word, so a parameter may still carry that name. */
        if (parser_is_word(p, parser_peek(p), "own") &&
            parser_peek_at(p, 1)->kind == TOKEN_IDENT) {
            parser_next(p);
            param.owned = true;
        }
        parser_fn_param_marks(p, &param.keep, &param.concurrent, &param.owned);
        param.lent = parser_lent_mark(p);
        param.pos = parser_pos_of(parser_peek(p));
        if (!parser_expect_name(p, &param.name) ||
            !parser_expect(p, TOKEN_COLON) ||
            (param.type = parser_type(p)) == NULL) {
            free(list.data);
            return NULL;
        }
        /* `name: T = value`: the default that a call which leaves the
           parameter out passes. */
        if (parser_accept(p, TOKEN_ASSIGN) &&
            (param.value = parser_expression(p)) == NULL) {
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
    return parser_list_finish(p, &list, count);
}

/* DESIGN: `may fail` is two contextual words after a signature, not a
   keyword pair, so `may` stays an identifier everywhere else. The
   function then returns `?*Error` and writes its result through an out
   pointer, which is the convention a program used to write by hand. */
void parser_may_fail_after(struct parser *p, struct item *it)
{
    if (!parser_is_word(p, parser_peek(p), "may") ||
        parser_peek_at(p, 1)->kind != TOKEN_FAIL) {
        return;
    }
    it->may_fail_pos = parser_pos_of(parser_peek(p));
    parser_next(p);
    parser_next(p);
    it->may_fail = true;
}

/* Whether the next token opens a function or a constant of a body.
   DESIGN: `pub` and `protected` stand before a field as well as before a
   function, so the test looks past the marker. A field is a name and a
   colon, and everything else at that point opens a member. */
bool parser_starts_member(const struct parser *p)
{
    size_t i = 0;

    switch (parser_peek(p)->kind) {
    case TOKEN_PUB:
    case TOKEN_PROTECTED:
    case TOKEN_EXPORT:
        i = 1;
        break;
    default:
        break;
    }
    switch (parser_peek_at(p, i)->kind) {
    case TOKEN_FN:
    case TOKEN_EXPORT:
    case TOKEN_PUB:
    case TOKEN_ABSTRACT:
    case TOKEN_CONCRETE:
    case TOKEN_CONST:
    case TOKEN_STATIC:
        return true;
    default:
        return (parser_is_word(p, parser_peek_at(p, i), "final") ||
                parser_is_word(p, parser_peek_at(p, i), "operator")) &&
               parser_peek_at(p, i + 1)->kind == TOKEN_FN;
    }
}

/* The constraints after the `:` of a type parameter or the `=` of a
   `constraint`, joined by `+`. Each is a name, qualified by a module or
   not. */
struct constraint_ref *parser_constraint_list(struct parser *p,
                                              size_t *count)
{
    struct list refs = {NULL, 0, 0, sizeof(struct constraint_ref)};

    do {
        struct constraint_ref r;
        memset(&r, 0, sizeof r);
        r.pos = parser_pos_of(parser_peek(p));
        if (!parser_expect_name(p, &r.name) ||
            (parser_accept(p, TOKEN_DOT) &&
             (r.module = r.name, !parser_expect_name(p, &r.name)))) {
            free(refs.data);
            *count = 0;
            return NULL;
        }
        /* A generic interface takes its type arguments, as a type does. */
        if (parser_check(p, TOKEN_LT)) {
            r.type_args_pos = parser_pos_of(parser_peek(p));
            r.type_args = parser_type_args(p, &r.type_arg_count);
            if (r.type_args == NULL) {
                free(refs.data);
                *count = 0;
                return NULL;
            }
        }
        parser_list_push(&refs, &r);
    } while (parser_accept(p, TOKEN_PLUS));
    return parser_list_finish(p, &refs, count);
}

/* `<T: lt + eq, U, N: int>` after the name of a generic. A parameter
   without `:` is unconstrained, and `N: int` takes an integer constant. */
bool parser_type_params(struct parser *p, struct item *it)
{
    struct list list = {NULL, 0, 0, sizeof(struct type_param)};

    bool ok;

    if (!parser_check(p, TOKEN_LT)) {
        return true;
    }
    parser_next(p);
    /* The list counts as open, so that `>>` after the arguments of a
       constraint closes both. */
    p->angles++;
    do {
        struct type_param tp;
        memset(&tp, 0, sizeof tp);
        tp.pos = parser_pos_of(parser_peek(p));
        if (!parser_expect_name(p, &tp.name)) {
            free(list.data);
            p->angles--;
            return false;
        }
        if (parser_accept(p, TOKEN_COLON)) {
            if (parser_accept(p, TOKEN_INT_TYPE)) {
                tp.constant = true;
            } else if (lexer_token_is_builtin_type(parser_peek(p)->kind)) {
                parser_error_here(p,
                                  "a constant parameter is written `N: int`");
                free(list.data);
                p->angles--;
                return false;
            } else if ((tp.constraints = parser_constraint_list(
                            p, &tp.constraint_count)) == NULL) {
                free(list.data);
                p->angles--;
                return false;
            }
        }
        parser_list_push(&list, &tp);
    } while (!p->half && parser_accept(p, TOKEN_COMMA));
    it->type_params = parser_list_finish(p, &list, &it->type_param_count);
    ok = parser_close_angle(p);
    p->angles--;
    return ok;
}

/* One function or constant declared between the braces of a struct, a
   union or an enum. An abstract function has no body and ends with `;`. */
static struct item *member_level(struct parser *p, const struct item *owner)
{
    struct item *m = parser_node(p, sizeof *m);

    m->pos = parser_pos_of(parser_peek(p));
    m->doc = parser_doc_before(p, TOKEN_DOC);
    m->note = parser_doc_before(p, TOKEN_NOTE);
    m->owner = owner;
    m->exported = parser_accept(p, TOKEN_EXPORT);
    m->pub = m->exported || parser_accept(p, TOKEN_PUB);
    m->vis = m->pub ? VIS_PUB : VIS_PRIVATE;
    /* DESIGN: `protected` reaches the class and every class below it. The
       visibility table of the object model document gives it to a class
       member alone, so `internal` here names the level that fits. */
    if (!m->pub && parser_accept(p, TOKEN_PROTECTED)) {
        m->vis = VIS_PROTECTED;
    } else if (parser_check(p, TOKEN_INTERNAL)) {
        parser_error_here(p, "`internal` marks a module item, and a class "
                             "member is `pub`, `protected` or neither");
        return NULL;
    }
    /* `final` before `fn` forbids replacement, as it forbids inheritance
       before `class`. It is a contextual word in both places. */
    if (parser_is_word(p, parser_peek(p), "final") &&
        parser_peek_at(p, 1)->kind == TOKEN_FN) {
        parser_next(p);
        m->is_final = true;
    }
    /* `operator` before `fn` marks a function an operator calls. It is a
       contextual word and implies `pub`. */
    if (parser_is_word(p, parser_peek(p), "operator") &&
        parser_peek_at(p, 1)->kind == TOKEN_FN) {
        parser_next(p);
        m->is_operator = true;
    }
    /* `trace` before `fn` marks one function of the class, as it marks
       every `pub` function before `class`. */
    if (parser_is_word(p, parser_peek(p), "trace") &&
        parser_peek_at(p, 1)->kind == TOKEN_FN) {
        parser_next(p);
        m->trace = true;
    }
    if (parser_accept(p, TOKEN_ABSTRACT)) {
        m->contract = FN_ABSTRACT;
    } else if (parser_accept(p, TOKEN_CONCRETE)) {
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
    if (m->contract == FN_PLAIN && parser_accept(p, TOKEN_STATIC)) {
        bool atomic = parser_accept(p, TOKEN_ATOMIC);
        m->kind = ITEM_CONST;
        m->is_static = true;
        m->name_pos = parser_pos_of(parser_peek(p));
        if (!parser_expect_name(p, &m->name)) {
            return NULL;
        }
        if (!atomic) {
            parser_error_here(p,
                              "`%.*s` is `static` and must be `atomic`",
                              (int)m->name.length, m->name.text);
            return NULL;
        }
        m->atomic = true;
        if (!parser_expect(p, TOKEN_COLON) ||
            (m->type = parser_type(p)) == NULL ||
            !parser_expect(p, TOKEN_ASSIGN) ||
            (m->value = parser_expression(p)) == NULL ||
            !parser_expect(p, TOKEN_SEMICOLON)) {
            return NULL;
        }
        return m;
    }
    if (m->contract == FN_PLAIN && parser_accept(p, TOKEN_CONST)) {
        m->kind = ITEM_CONST;
        m->name_pos = parser_pos_of(parser_peek(p));
        if (!parser_expect_name(p, &m->name) ||
            !parser_expect(p, TOKEN_COLON) ||
            (m->type = parser_type(p)) == NULL ||
            !parser_expect(p, TOKEN_ASSIGN) ||
            (m->value = parser_expression(p)) == NULL ||
            !parser_expect(p, TOKEN_SEMICOLON)) {
            return NULL;
        }
        return m;
    }
    if (!parser_expect(p, TOKEN_FN)) {
        return NULL;
    }
    m->kind = ITEM_FN;
    m->name_pos = parser_pos_of(parser_peek(p));
    if (!parser_expect_function_name(p, &m->name)) {
        return NULL;
    }
    /* DESIGN: `concrete fn Serializable::f` fills the table of that base
       or interface alone. The qualifier is read as a name and a `::`, so
       the function name that follows it takes the place of the first. */
    if (parser_check(p, TOKEN_COLON_COLON)) {
        if (m->contract != FN_CONCRETE) {
            parser_error_here(p, "a `::` qualifier belongs to a `concrete fn`");
            return NULL;
        }
        parser_next(p);
        m->qualifier = m->name;
        m->qualifier_pos = m->name_pos;
        m->name_pos = parser_pos_of(parser_peek(p));
        if (!parser_expect_function_name(p, &m->name)) {
            return NULL;
        }
    }
    if (!parser_type_params(p, m)) {
        return NULL;
    }
    m->params = parser_params(p, false, &m->variadic, &m->has_self,
                              &m->param_count);
    if (p->panic ||
        !parser_result_type(p, m)) {
        return NULL;
    }
    parser_may_fail_after(p, m);
    if (!parser_header_clauses(p)) {
        return NULL;
    }
    if (m->contract == FN_ABSTRACT) {
        return parser_expect(p, TOKEN_SEMICOLON) ? m : NULL;
    }
    return (m->body = parser_block(p)) == NULL ? NULL : m;
}

/* A member with the clauses of its header closed over it. */
static struct item *member(struct parser *p, const struct item *owner)
{
    size_t mark = p->clauses->count;
    struct pos from = parser_pos_of(parser_peek(p));
    struct item *m = member_level(p, owner);

    parser_close_clauses(p, mark,
                         m != NULL ? parser_declaration_start(m) : from);
    return m;
}

/* DESIGN: `compatible 1.1;` in the body of an abstract class names the
   lowest version a plugin may have been built for. `compatible` is a
   contextual word in that position alone, so a field may still carry
   the name. The version is the source text of the number the lexer read
   as an integer or a float, with any further `.<integer>` parts after
   it, since `1.1.0` is no number of Anti. */
bool parser_compatible_line(struct parser *p, struct item *it)
{
    const struct token *start;
    size_t end;

    parser_next(p);
    if (it->compatible.length > 0) {
        parser_error_here(p, "a class has one `compatible` line");
        return false;
    }
    it->compatible_pos = parser_pos_of(parser_peek(p));
    start = parser_peek(p);
    if (start->kind != TOKEN_INT && start->kind != TOKEN_FLOAT) {
        parser_error_here(p,
                          "`compatible` names a version, as `compatible 1.1;`");
        return false;
    }
    parser_next(p);
    end = start->offset + start->length;
    while (parser_check(p, TOKEN_DOT) &&
           parser_peek_at(p, 1)->kind == TOKEN_INT) {
        parser_next(p);
        end = parser_peek(p)->offset + parser_peek(p)->length;
        parser_next(p);
    }
    it->compatible.text = p->source + start->offset;
    it->compatible.length = end - start->offset;
    return parser_expect(p, TOKEN_SEMICOLON);
}

/* `inherits` in a class body, where the base stood before it moved to
   the header. The message writes the header the programmer means, with
   the base as the body named it. The base and a comma after it are read
   and dropped, so the rest of the body parses and reports its own
   errors. Returns whether a comma followed. */
bool parser_inherits_in_body(struct parser *p, const struct item *it)
{
    const struct token *base = parser_peek_at(p, 1);
    const struct token *dot = parser_peek_at(p, 2);
    const struct token *name = parser_peek_at(p, 3);
    size_t length = 0;

    if (base->kind == TOKEN_IDENT) {
        length = base->length;
        if (dot->kind == TOKEN_DOT && name->kind == TOKEN_IDENT) {
            length = name->offset + name->length - base->offset;
        }
    }
    parser_error_here(p, "inherits belongs in the class header: class %.*s "
                         "inherits %.*s",
                      (int)it->name.length, it->name.text, (int)length,
                      p->source + base->offset);
    parser_next(p);
    if (parser_accept(p, TOKEN_IDENT) && parser_accept(p, TOKEN_DOT)) {
        parser_accept(p, TOKEN_IDENT);
    }
    p->panic = false;
    return parser_accept(p, TOKEN_COMMA);
}

/* Whether the next tokens declare a type in a class body: the words that
   may stand before a type, then the word of its kind. */
bool parser_starts_nested(const struct parser *p)
{
    size_t i;

    for (i = 0; i < 4; i++) {
        const struct token *t = parser_peek_at(p, i);
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
            if (!parser_is_word(p, t, "packed") &&
                !parser_is_word(p, t, "simd") &&
                !parser_is_word(p, t, "final") &&
                !parser_is_word(p, t, "trace")) {
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
bool parser_nested_type(struct parser *p, struct list *nested)
{
    const struct token *word = parser_peek(p);
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
        parser_next(p);
        break;
    default:
        break;
    }
    /* Each type nested in a class body is one level, since the item
       reaches class_item and this function again. */
    if (!parser_descend(p)) {
        return false;
    }
    inner = parser_item(p);
    parser_ascend(p);
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
    parser_list_push(nested, &inner);
    return true;
}

/* Read the functions and constants of a body into it->members, and the
   types of a class body into nested, which is NULL for an enum. */
bool parser_members_of(struct parser *p, struct item *it,
                       struct list *nested)
{
    struct list list = {NULL, 0, 0, sizeof(struct item *)};

    while (!parser_check(p, TOKEN_RBRACE) && !parser_check(p, TOKEN_EOF)) {
        struct item *m;
        if (it->kind == ITEM_CLASS && parser_check(p, TOKEN_INHERITS)) {
            parser_inherits_in_body(p, it);
            continue;
        }
        if (nested != NULL && parser_starts_nested(p)) {
            if (!parser_nested_type(p, nested)) {
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
        parser_list_push(&list, &m);
    }
    it->members = parser_list_finish(p, &list, &it->member_count);
    return true;
}

/* A struct body holds fields alone. Report what the programmer wrote and
   name the kind that takes it. */
bool parser_struct_field_only(struct parser *p, const struct item *it)
{
    const char *what = NULL;

    switch (parser_peek(p)->kind) {
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
    parser_error_here(p,
                      "a %s holds fields alone, and %s belongs to a class",
                      it->kind == ITEM_UNION ? "union" : "struct", what);
    return false;
}

/* DESIGN: `guarded by lock` after a field's type or its default names
   the Mutex that a `sync` holds wherever the field is reached, and
   `guarded by PeopleList.lock` names the lock of an enclosing object by
   its class. `guarded` and `by` are contextual words. */
bool parser_guard_clause(struct parser *p, struct param *field)
{
    if (field->guard.length > 0 ||
        !parser_is_word(p, parser_peek(p), "guarded") ||
        !parser_is_word(p, parser_peek_at(p, 1), "by")) {
        return true;
    }
    field->guard_pos = parser_pos_of(parser_peek(p));
    parser_next(p);
    parser_next(p);
    if (!parser_expect_name(p, &field->guard)) {
        return false;
    }
    if (parser_accept(p, TOKEN_DOT)) {
        field->guard_class = field->guard;
        if (!parser_expect_name(p, &field->guard)) {
            return false;
        }
    }
    return true;
}

/* Whether a clause read since mark is `unchecked(unguarded-field)`. */
bool parser_unchecks_guard(const struct parser *p, size_t mark)
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
bool parser_field_clauses(struct parser *p)
{
    while (parser_clause_ahead(p, 0)) {
        if (parser_is_word(p, parser_peek(p), "allow")) {
            parser_error_here(p, "`allow` stands before a statement, last in "
                                 "a declaration's header or at the top of the "
                                 "file");
            return false;
        }
        if (!parser_read_clause(p, CLAUSE_FIELD)) {
            return false;
        }
    }
    return true;
}

/* The items of a module: a class with its body, a variant with its
   cases and every other item, each with the clauses of its header. */

#include <stdlib.h>
#include <string.h>

#include "parser_parser.h"

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

    parser_next(p);
    it->kind = ITEM_CLASS;
    if (!parser_expect_name(p, &it->name) || !parser_type_params(p, it)) {
        return NULL;
    }
    /* DESIGN: `align(N)` stays directly after the name, as on a struct,
       and the base follows it. */
    if (parser_is_word(p, parser_peek(p), "align")) {
        parser_next(p);
        if (!parser_expect(p, TOKEN_LPAREN) ||
            (it->align = parser_expression(p)) == NULL ||
            !parser_expect(p, TOKEN_RPAREN)) {
            return NULL;
        }
    }
    /* The base is one name, qualified by its module or not, and no field
       of its own. The checker builds the `super` field from it. */
    if (parser_check(p, TOKEN_INHERITS)) {
        it->base_pos = parser_pos_of(parser_peek(p));
        parser_next(p);
        if (!parser_expect_name(p, &it->base_name) ||
            (parser_accept(p, TOKEN_DOT) &&
             (it->base_module = it->base_name,
              !parser_expect_name(p, &it->base_name)))) {
            return NULL;
        }
        if (parser_check(p, TOKEN_LT) &&
            (it->base_args =
                 parser_type_args(p, &it->base_arg_count)) == NULL) {
            return NULL;
        }
        if (parser_check(p, TOKEN_COMMA) || parser_check(p, TOKEN_INHERITS)) {
            parser_error_here(p, "a class has one base");
            return NULL;
        }
    }
    mark = p->clauses->count;
    if (!parser_header_clauses(p) || !parser_expect(p, TOKEN_LBRACE)) {
        return NULL;
    }
    it->unchecked_fields = parser_unchecks_guard(p, mark);
    /* Fields first, comma separated, then the constants and functions,
       each ended by its own `;` or block. */
    while (!parser_check(p, TOKEN_RBRACE) && !parser_check(p, TOKEN_EOF) &&
           (!parser_starts_member(p) || parser_starts_nested(p))) {
        struct param field;
        if (parser_starts_nested(p)) {
            if (!parser_nested_type(p, &nested)) {
                free(fields.data);
                free(nested.data);
                return NULL;
            }
            continue;
        }
        memset(&field, 0, sizeof field);
        field.doc = parser_doc_before(p, TOKEN_DOC);
        field.note = parser_doc_before(p, TOKEN_NOTE);
        field.pos = parser_pos_of(parser_peek(p));
        if (parser_check(p, TOKEN_INHERITS)) {
            if (!parser_inherits_in_body(p, it)) {
                break;
            }
            continue;
        }
        if (parser_is_word(p, parser_peek(p), "compatible") &&
            (parser_peek_at(p, 1)->kind == TOKEN_INT ||
             parser_peek_at(p, 1)->kind == TOKEN_FLOAT)) {
            if (!parser_compatible_line(p, it)) {
                break;
            }
            continue;
        }
        if (parser_accept(p, TOKEN_IMPLEMENTS)) {
            field.form = FIELD_IMPL;
        } else if (parser_accept(p, TOKEN_USE)) {
            field.form = FIELD_USE;
        }
        if (field.form == FIELD_PLAIN) {
            if (parser_accept(p, TOKEN_PUB)) {
                field.vis = VIS_PUB;
            } else if (parser_accept(p, TOKEN_PROTECTED)) {
                field.vis = VIS_PROTECTED;
            }
        }
        /* DESIGN: `inject` and `inject final` name a field a provider
           fills before `construct` runs. Both are contextual, so
           `inject: int` is still a field named `inject` and
           `inject final: *L` a field named `final` a provider fills.
           The `final` form is read first, because its second word is a
           name as well. */
        if (parser_is_word(p, parser_peek(p), "inject") &&
            parser_is_word(p, parser_peek_at(p, 1), "final") &&
            parser_is_field_name(parser_peek_at(p, 2))) {
            parser_next(p);
            parser_next(p);
            field.injected = true;
            field.inject_final = true;
        } else if (parser_is_word(p, parser_peek(p), "inject") &&
                   parser_is_field_name(parser_peek_at(p, 1))) {
            parser_next(p);
            field.injected = true;
        }
        /* `transient`, `own`, `atomic` and `mutable` are contextual
           words before a field name, so a field may still carry one of
           those names. */
        if (parser_is_word(p, parser_peek(p), "transient") &&
            (parser_peek_at(p, 1)->kind == TOKEN_IDENT ||
             parser_peek_at(p, 1)->kind == TOKEN_ATOMIC)) {
            parser_next(p);
            field.transient = true;
        }
        if (parser_is_word(p, parser_peek(p), "own") &&
            parser_peek_at(p, 1)->kind == TOKEN_IDENT) {
            parser_next(p);
            field.owned = true;
        }
        if (parser_is_word(p, parser_peek(p), "mutable") &&
            parser_peek_at(p, 1)->kind == TOKEN_IDENT) {
            parser_next(p);
            field.writable = true;
        }
        if (parser_check(p, TOKEN_ATOMIC) &&
            parser_peek_at(p, 1)->kind == TOKEN_IDENT) {
            parser_next(p);
            field.atomic = true;
        }
        /* An `inject` field may be called `alloc` or `free`, as the
           example of `docs/anti-syntax-overview.md` writes it. A name
           must stand after `inject`, so the position has one reading. */
        mark = p->clauses->count;
        if (!(field.injected ? parser_expect_member_name(p, &field.name)
                             : parser_expect_name(p, &field.name)) ||
            !parser_expect(p, TOKEN_COLON) ||
            (field.type = parser_type(p)) == NULL ||
            !parser_guard_clause(p, &field) ||
            !parser_field_clauses(p) ||
            (parser_accept(p, TOKEN_COLON) &&
             (field.bits = parser_expression(p)) == NULL) ||
            (parser_accept(p, TOKEN_ASSIGN) &&
             (field.value = parser_expression(p)) == NULL) ||
            !parser_guard_clause(p, &field) || !parser_field_clauses(p)) {
            free(fields.data);
            free(nested.data);
            return NULL;
        }
        field.unchecked = parser_unchecks_guard(p, mark);
        parser_close_clauses(p, mark, field.pos);
        parser_list_push(&fields, &field);
        if (!parser_accept(p, TOKEN_COMMA)) {
            break;
        }
    }
    it->params = parser_list_finish(p, &fields, &it->param_count);
    if (!parser_members_of(p, it, &nested)) {
        free(nested.data);
        return NULL;
    }
    it->nested = parser_list_finish(p, &nested, &it->nested_count);
    return parser_expect(p, TOKEN_RBRACE) ? it : NULL;
}

/* DESIGN: a variant body holds its cases and nothing else. A case is a
   name and, in braces, the fields it carries, each a name and a type. A
   variant is a struct, so no function, constant, default or bitfield
   stands in it. */
static struct item *variant_item(struct parser *p, struct item *it)
{
    struct list cases = {NULL, 0, 0, sizeof(struct variant_case)};

    parser_next(p);
    it->kind = ITEM_VARIANT;
    if (!parser_expect_name(p, &it->name) || !parser_type_params(p, it)) {
        return NULL;
    }
    if (parser_is_word(p, parser_peek(p), "align")) {
        parser_next(p);
        if (!parser_expect(p, TOKEN_LPAREN) ||
            (it->align = parser_expression(p)) == NULL ||
            !parser_expect(p, TOKEN_RPAREN)) {
            return NULL;
        }
    }
    if (!parser_expect(p, TOKEN_LBRACE)) {
        return NULL;
    }
    while (!parser_check(p, TOKEN_RBRACE) && !parser_check(p, TOKEN_EOF)) {
        struct variant_case one;
        memset(&one, 0, sizeof one);
        one.doc = parser_doc_before(p, TOKEN_DOC);
        parser_doc_before(p, TOKEN_NOTE);
        one.pos = parser_pos_of(parser_peek(p));
        if (!parser_expect_name(p, &one.name)) {
            free(cases.data);
            return NULL;
        }
        if (parser_accept(p, TOKEN_LBRACE)) {
            struct list fields = {NULL, 0, 0, sizeof(struct param)};
            while (!parser_check(p, TOKEN_RBRACE) &&
                   !parser_check(p, TOKEN_EOF)) {
                struct param field;
                memset(&field, 0, sizeof field);
                field.doc = parser_doc_before(p, TOKEN_DOC);
                field.note = parser_doc_before(p, TOKEN_NOTE);
                field.pos = parser_pos_of(parser_peek(p));
                if (!parser_expect_name(p, &field.name) ||
                    !parser_expect(p, TOKEN_COLON) ||
                    (field.type = parser_type(p)) == NULL) {
                    free(fields.data);
                    free(cases.data);
                    return NULL;
                }
                parser_list_push(&fields, &field);
                if (!parser_accept(p, TOKEN_COMMA)) {
                    break;
                }
            }
            one.fields = parser_list_finish(p, &fields, &one.field_count);
            if (!parser_expect(p, TOKEN_RBRACE)) {
                free(cases.data);
                return NULL;
            }
        }
        parser_list_push(&cases, &one);
        if (!parser_accept(p, TOKEN_COMMA)) {
            break;
        }
    }
    it->cases = parser_list_finish(p, &cases, &it->case_count);
    return parser_expect(p, TOKEN_RBRACE) ? it : NULL;
}

/* Whether `class` stands at the token ahead, or `synchronized class` or
   `concurrent class` does. */
static bool class_ahead(const struct parser *p, size_t ahead)
{
    const struct token *t = parser_peek_at(p, ahead);

    return t->kind == TOKEN_CLASS ||
           ((parser_is_word(p, t, "synchronized") ||
             parser_is_word(p, t, "concurrent")) &&
            parser_peek_at(p, ahead + 1)->kind == TOKEN_CLASS);
}

static struct item *item_level(struct parser *p)
{
    const struct token *start = parser_peek(p);
    struct item *it = parser_node(p, sizeof *it);

    it->pos = parser_pos_of(start);
    it->doc = parser_doc_before(p, TOKEN_DOC);
    it->note = parser_doc_before(p, TOKEN_NOTE);
    it->exported = parser_accept(p, TOKEN_EXPORT);
    it->pub = it->exported || parser_accept(p, TOKEN_PUB);
    it->vis = it->pub ? VIS_PUB : VIS_PRIVATE;
    /* DESIGN: `internal` reaches every module under the same package
       root. It is a level of a module item, and never of a class
       member. */
    if (!it->pub && parser_accept(p, TOKEN_INTERNAL)) {
        it->vis = VIS_INTERNAL;
    } else if (parser_check(p, TOKEN_PROTECTED)) {
        parser_error_here(p, "`protected` marks a class member, and a module "
                             "item is `pub`, `internal` or neither");
        return NULL;
    }
    /* DESIGN: export marks items that Anti defines for C. An extern fn is
       defined in C already. */
    if (it->exported && parser_peek(p)->kind == TOKEN_EXTERN) {
        parser_error_here(p, "an `extern fn` cannot be exported");
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
    if (parser_is_word(p, parser_peek(p), "trace") &&
        (class_ahead(p, 1) || parser_peek_at(p, 1)->kind == TOKEN_FN)) {
        parser_next(p);
        if (!class_ahead(p, 0)) {
            parser_error_here(p, "`trace` marks a class or a function of one");
            return NULL;
        }
        it->trace = true;
    }
    if (parser_peek(p)->kind == TOKEN_ABSTRACT && class_ahead(p, 1)) {
        parser_next(p);
        it->is_abstract = true;
    } else if (parser_is_word(p, parser_peek(p), "final") &&
               class_ahead(p, 1)) {
        parser_next(p);
        it->is_final = true;
    } else if (parser_peek(p)->kind == TOKEN_SINGLETON && class_ahead(p, 1)) {
        parser_next(p);
        it->is_singleton = true;
    }
    /* DESIGN: `synchronized` and `concurrent` are contextual words
       directly before `class`, so a parameter or a field may still carry
       either name. */
    if (parser_is_word(p, parser_peek(p), "synchronized") &&
        parser_peek_at(p, 1)->kind == TOKEN_CLASS) {
        parser_next(p);
        it->synchronized = true;
    } else if (parser_is_word(p, parser_peek(p), "concurrent") &&
               parser_peek_at(p, 1)->kind == TOKEN_CLASS) {
        parser_next(p);
        it->concurrent = true;
    }
    /* `operator fn` at module level gives a struct its operators, since
       a struct holds no functions of its own. */
    if (parser_is_word(p, parser_peek(p), "operator") &&
        parser_peek_at(p, 1)->kind == TOKEN_FN) {
        parser_next(p);
        it->is_operator = true;
    }
    it->worker = parser_accept(p, TOKEN_WORKER);
    if (it->worker && parser_peek(p)->kind != TOKEN_FN) {
        parser_error_here(p, "`worker` stands before `fn`");
        return NULL;
    }
    /* DESIGN: packed and align are contextual words. packed is one only
       directly before struct, union or variant, and align only between
       the name of one of them and its opening brace. */
    if (parser_is_word(p, parser_peek(p), "packed") &&
        (parser_peek_at(p, 1)->kind == TOKEN_STRUCT ||
         parser_peek_at(p, 1)->kind == TOKEN_UNION ||
         parser_peek_at(p, 1)->kind == TOKEN_CLASS ||
         parser_peek_at(p, 1)->kind == TOKEN_VARIANT)) {
        parser_next(p);
        it->packed = true;
    }
    /* DESIGN: simd is a contextual word as well, and one only directly
       before struct. */
    if (parser_is_word(p, parser_peek(p), "simd") &&
        parser_peek_at(p, 1)->kind == TOKEN_STRUCT) {
        parser_next(p);
        it->simd = true;
    }
    it->name_pos = parser_pos_of(parser_peek_at(p, 1));
    if (parser_peek(p)->kind == TOKEN_EXTERN) {
        it->name_pos = parser_pos_of(parser_peek_at(p, 2));
    }
    switch (parser_peek(p)->kind) {
    case TOKEN_FN:
        parser_next(p);
        it->kind = ITEM_FN;
        if (!parser_expect_name(p, &it->name) || !parser_type_params(p, it)) {
            return NULL;
        }
        it->params = parser_params(p, false, &it->variadic, NULL,
                                   &it->param_count);
        if (p->panic ||
            !parser_result_type(p, it)) {
            return NULL;
        }
        parser_may_fail_after(p, it);
        if (p->panic || !parser_header_clauses(p) ||
            (it->body = parser_block(p)) == NULL) {
            return NULL;
        }
        return it;
    case TOKEN_EXTERN:
        parser_next(p);
        it->kind = ITEM_EXTERN_FN;
        if (!parser_expect(p, TOKEN_FN) || !parser_expect_name(p, &it->name)) {
            return NULL;
        }
        it->params = parser_params(p, true, &it->variadic, NULL,
                                   &it->param_count);
        if (p->panic ||
            !parser_result_type(p, it)) {
            return NULL;
        }
        /* A binding declares what C declares, and C has no error
           channel. A C function that reports one returns `?*Error` by
           hand. */
        if (parser_is_word(p, parser_peek(p), "may") &&
            parser_peek_at(p, 1)->kind == TOKEN_FAIL) {
            parser_error_here(p, "`may fail` belongs to an Anti function, "
                                 "and an `extern fn` writes `-> ?*Error` by "
                                 "hand");
            return NULL;
        }
        if (!parser_expect(p, TOKEN_SEMICOLON)) {
            return NULL;
        }
        return it;
    case TOKEN_STRUCT:
    case TOKEN_UNION: {
        struct list fields = {NULL, 0, 0, sizeof(struct param)};
        it->kind = parser_next(p)->kind == TOKEN_UNION ? ITEM_UNION
                                                       : ITEM_STRUCT;
        if (!parser_expect_name(p, &it->name) ||
            (it->kind == ITEM_STRUCT && !parser_type_params(p, it))) {
            return NULL;
        }
        if (parser_is_word(p, parser_peek(p), "align")) {
            parser_next(p);
            if (!parser_expect(p, TOKEN_LPAREN) ||
                (it->align = parser_expression(p)) == NULL ||
                !parser_expect(p, TOKEN_RPAREN)) {
                return NULL;
            }
        }
        if (!parser_header_clauses(p) || !parser_expect(p, TOKEN_LBRACE)) {
            return NULL;
        }
        /* DESIGN: a struct body holds fields and nothing else. It is
           exactly the bytes C declares, so a function, a constant, a
           default or a base belongs to a class. */
        do {
            struct param field;
            size_t mark;
            memset(&field, 0, sizeof field);
            field.doc = parser_doc_before(p, TOKEN_DOC);
            field.note = parser_doc_before(p, TOKEN_NOTE);
            field.pos = parser_pos_of(parser_peek(p));
            if (!parser_struct_field_only(p, it)) {
                free(fields.data);
                return NULL;
            }
            mark = p->clauses->count;
            if (!parser_expect_name(p, &field.name) ||
                !parser_expect(p, TOKEN_COLON) ||
                (field.type = parser_type(p)) == NULL ||
                !parser_guard_clause(p, &field) ||
                !parser_field_clauses(p) ||
                (parser_accept(p, TOKEN_COLON) &&
                 (field.bits = parser_expression(p)) == NULL)) {
                free(fields.data);
                return NULL;
            }
            field.unchecked = parser_unchecks_guard(p, mark);
            parser_close_clauses(p, mark, field.pos);
            if (parser_check(p, TOKEN_ASSIGN)) {
                parser_error_here(p, "a field of a struct has no default, "
                                     "which belongs to a class");
                free(fields.data);
                return NULL;
            }
            parser_list_push(&fields, &field);
        } while (parser_accept(p, TOKEN_COMMA) &&
                 !parser_check(p, TOKEN_RBRACE));
        it->params = parser_list_finish(p, &fields, &it->param_count);
        return parser_expect(p, TOKEN_RBRACE) ? it : NULL;
    }
    case TOKEN_CLASS:
        return class_item(p, it);
    case TOKEN_VARIANT:
        return variant_item(p, it);
    case TOKEN_ENUM: {
        struct list values = {NULL, 0, 0, sizeof(struct param)};
        parser_next(p);
        it->kind = ITEM_ENUM;
        if (!parser_expect_name(p, &it->name)) {
            return NULL;
        }
        /* `enum Mode: u8` names the underlying type. Without one the type
           is c_int, which the checker fills in. */
        if (parser_accept(p, TOKEN_COLON) &&
            (it->base = parser_type(p)) == NULL) {
            return NULL;
        }
        if (!parser_expect(p, TOKEN_LBRACE)) {
            return NULL;
        }
        while (!parser_check(p, TOKEN_RBRACE) && !parser_check(p, TOKEN_EOF) &&
               !parser_starts_member(p)) {
            struct param value;
            memset(&value, 0, sizeof value);
            value.doc = parser_doc_before(p, TOKEN_DOC);
            value.note = parser_doc_before(p, TOKEN_NOTE);
            value.pos = parser_pos_of(parser_peek(p));
            if (!parser_expect_name(p, &value.name) ||
                (parser_accept(p, TOKEN_ASSIGN) &&
                 (value.value = parser_expression(p)) == NULL)) {
                free(values.data);
                return NULL;
            }
            parser_list_push(&values, &value);
            if (!parser_accept(p, TOKEN_COMMA)) {
                break;
            }
        }
        it->params = parser_list_finish(p, &values, &it->param_count);
        if (!parser_members_of(p, it, NULL)) {
            return NULL;
        }
        return parser_expect(p, TOKEN_RBRACE) ? it : NULL;
    }
    case TOKEN_CONSTRAINT:
        /* `constraint Ordered = eq + lt;` names a set of constraints. C
           has none, so a constraint is never exported. */
        if (it->exported) {
            parser_error_here(p, "a `constraint` is not exported, since C "
                                 "has no generics");
            return NULL;
        }
        parser_next(p);
        it->kind = ITEM_CONSTRAINT;
        if (!parser_expect_name(p, &it->name) ||
            !parser_expect(p, TOKEN_ASSIGN) ||
            (it->constraints = parser_constraint_list(
                 p, &it->constraint_count)) == NULL ||
            !parser_expect(p, TOKEN_SEMICOLON)) {
            return NULL;
        }
        return it;
    case TOKEN_TYPE:
        /* `type People = List<Person>;` names a type, and
           `export type PersonList = List<Person>;` offers it to C. */
        parser_next(p);
        it->kind = ITEM_TYPE;
        if (!parser_expect_name(p, &it->name) ||
            !parser_expect(p, TOKEN_ASSIGN) ||
            (it->type = parser_type(p)) == NULL ||
            !parser_expect(p, TOKEN_SEMICOLON)) {
            return NULL;
        }
        return it;
    case TOKEN_CONST:
        parser_next(p);
        it->kind = ITEM_CONST;
        if (!parser_expect_name(p, &it->name) ||
            !parser_expect(p, TOKEN_COLON) ||
            (it->type = parser_type(p)) == NULL ||
            !parser_expect(p, TOKEN_ASSIGN) ||
            (it->value = parser_expression(p)) == NULL ||
            !parser_expect(p, TOKEN_SEMICOLON)) {
            return NULL;
        }
        return it;
    default:
        parser_error_here(p, "expected an item");
        return NULL;
    }
}

/* An item with the clauses of its header closed over it. */
struct item *parser_item(struct parser *p)
{
    size_t mark = p->clauses->count;
    struct pos from = parser_pos_of(parser_peek(p));
    struct item *it = item_level(p);

    parser_close_clauses(p, mark,
                         it != NULL ? parser_declaration_start(it) : from);
    return it;
}

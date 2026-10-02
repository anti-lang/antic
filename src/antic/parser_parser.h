#ifndef ANTIC_PARSER_PARSER_H
#define ANTIC_PARSER_PARSER_H

/* The state of the parser and the functions its files share. parser.c
   holds the helpers every file uses and parser_parse, which reads a
   module and the lines that stand at module level. parser_type.c reads
   types, parser_expr.c expressions and parser_stmt.c statements and
   blocks. parser_clause.c reads the clauses `allow` and `unchecked`.
   parser_decl.c reads the parts of a declaration: its signature, its
   type parameters and the members and fields of a body. parser_item.c
   reads the items. A function the files share takes the prefix
   `parser_`, and one used in a single file stays `static`. */

#include <stdbool.h>
#include <stddef.h>

#include "arena.h"
#include "ast.h"
#include "attributes.h"
#include "diagnostic.h"
#include "lexer.h"

/* DESIGN: recursive descent, one function per grammar rule of chapter 2,
   with precedence climbing for the binary operators. After an error the
   parser is in panic mode and reports nothing more. It skips to the start
   of a statement or an item, so one mistake produces one message. */

struct parser {
    const char *source;
    const struct token *tokens;     /* without doc comments */
    const struct token *all;        /* with doc comments */
    size_t *origin;                 /* index in all of each token */
    bool *taken;                    /* the doc comments that were read */
    size_t pos;
    size_t last;                    /* index of the TOKEN_EOF of tokens */
    struct arena *arena;
    struct diagnostics *diags;
    bool panic;
    bool ok;
    bool no_struct_literal;         /* inside a condition */
    int depth;                      /* levels entered by parser_descend */
    int reach;                      /* deepest level of the chain's tree */
    struct list *clauses;           /* every `allow` and `unchecked` */
    struct list *dropped;           /* the doc comments no item took */
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

/* DESIGN: a chain that the parser builds in a loop, `a + b + c`,
   `a.b.c`, `f()()` or `x as T as U`, nests its tree one level per link
   with no call of parser_descend, and the walks over the tree recurse along it.
   The reach is the deepest level the tree built since the chain began
   stands at. Each link lifts it one level above everything in the chain,
   and a reach past PARSE_DEPTH_MAX is refused as parser_descend refuses a
   depth. So the tree, and not only the parser's own recursion, stays
   within the limit. */
struct chain {
    int saved;                      /* the reach around the chain */
};

/* parser.c */

void parser_list_push(struct list *l, const void *element);
void *parser_list_finish(struct parser *p, struct list *l, size_t *count);
const struct token *parser_peek(const struct parser *p);
const struct token *parser_peek_at(const struct parser *p, size_t ahead);
bool parser_check(const struct parser *p, enum token_kind kind);
const struct token *parser_next(struct parser *p);
bool parser_accept(struct parser *p, enum token_kind kind);
struct pos parser_pos_of(const struct token *t);
void parser_error_here(struct parser *p, const char *format, ...)
    ATTRIBUTE_PRINTF(2, 3);
bool parser_descend(struct parser *p);
void parser_ascend(struct parser *p);
void parser_chain_begin(struct parser *p, struct chain *c);
bool parser_chain_link(struct parser *p);
void parser_chain_end(struct parser *p, const struct chain *c);
bool parser_expect(struct parser *p, enum token_kind kind);
bool parser_expect_name(struct parser *p, struct name *name);
bool parser_is_field_name(const struct token *t);
bool parser_expect_member_name(struct parser *p, struct name *name);
bool parser_expect_function_name(struct parser *p, struct name *name);
void *parser_node(struct parser *p, size_t size);
size_t parser_keep_tokens(const struct token *all, size_t count,
                          struct token *kept, size_t *origin);
void parser_drop_untaken(struct parser *p, size_t count);
struct doc_text parser_doc_before(struct parser *p, enum token_kind kind);
void parser_sync_statement(struct parser *p);
bool parser_is_word(const struct parser *p, const struct token *t,
                    const char *word);

/* parser_type.c */

size_t parser_generic_end(const struct parser *p, size_t ahead);
bool parser_close_angle(struct parser *p);
struct type_expr **parser_type_args(struct parser *p, size_t *count);
struct type_expr *parser_type(struct parser *p);

/* parser_expr.c */

struct expr *parser_new_expr(struct parser *p, enum expr_kind kind,
                             const struct token *at);
int parser_precedence(enum token_kind kind);
struct expr *parser_expression(struct parser *p);
struct expr *parser_condition(struct parser *p);

/* parser_stmt.c */

bool parser_is_assign_op(enum token_kind kind);
struct block *parser_block(struct parser *p);

/* parser_clause.c */

bool parser_clause_ahead(const struct parser *p, size_t ahead);
bool parser_statement_clause(const struct parser *p);
bool parser_read_clause(struct parser *p, enum clause_level level);
void parser_close_clauses(struct parser *p, size_t mark, struct pos from);
bool parser_header_clauses(struct parser *p);
struct pos parser_declaration_start(const struct item *it);

/* parser_decl.c */

bool parser_is_fn_mark(const struct parser *p, size_t at);
void parser_fn_param_marks(struct parser *p, bool *keep, bool *concurrent,
                           bool *owned);
bool parser_result_type(struct parser *p, struct item *it);
bool parser_lent_mark(struct parser *p);
struct param *parser_params(struct parser *p, bool allow_variadic,
                            bool *variadic, bool *has_self, size_t *count);
void parser_may_fail_after(struct parser *p, struct item *it);
bool parser_starts_member(const struct parser *p);
struct constraint_ref *parser_constraint_list(struct parser *p,
                                              size_t *count);
bool parser_type_params(struct parser *p, struct item *it);
bool parser_compatible_line(struct parser *p, struct item *it);
bool parser_inherits_in_body(struct parser *p, const struct item *it);
bool parser_starts_nested(const struct parser *p);
bool parser_nested_type(struct parser *p, struct list *nested);
bool parser_members_of(struct parser *p, struct item *it,
                       struct list *nested);
bool parser_struct_field_only(struct parser *p, const struct item *it);
bool parser_guard_clause(struct parser *p, struct param *field);
bool parser_unchecks_guard(const struct parser *p, size_t mark);
bool parser_field_clauses(struct parser *p);

/* parser_item.c */

struct item *parser_item(struct parser *p);

#endif

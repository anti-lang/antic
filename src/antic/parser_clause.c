/* The clauses `allow(name, "reason")` and `unchecked(name, "reason")`
   of a statement, a declaration and the whole file, and the source each
   covers. */

#include <ctype.h>
#include <string.h>

#include "parser_parser.h"
#include "warnings.h"

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
bool parser_clause_ahead(const struct parser *p, size_t ahead)
{
    const struct token *t = parser_peek_at(p, ahead);

    return (parser_is_word(p, t, "allow") ||
            parser_is_word(p, t, "unchecked")) &&
           parser_peek_at(p, ahead + 1)->kind == TOKEN_LPAREN;
}

/* DESIGN: `allow` and `unchecked` are contextual words, so a function
   may still carry either name. Before a statement the clause is the
   word, its parentheses and a statement after them. A call of a
   function of that name ends with `;` or goes on as an expression, and
   the token after its `)` says which of the two stands. */
bool parser_statement_clause(const struct parser *p)
{
    size_t i = p->pos + 1;
    int depth = 0;
    enum token_kind after;

    if (!parser_clause_ahead(p, 0)) {
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
           after != TOKEN_AS && after != TOKEN_IS &&
           !parser_is_assign_op(after) && parser_precedence(after) == 0;
}

/* Read `allow(name, "reason")` or `unchecked(name, "reason")`. The
   clause waits for the source it covers, which parser_close_clauses gives it.
   A name that is no warning of `allow`, or no safety check of
   `unchecked`, is refused: an error is never silenced. */
bool parser_read_clause(struct parser *p, enum clause_level level)
{
    const struct token *word = parser_next(p);
    const struct token *first;
    const struct token *last;
    struct clause c;
    enum diag_name name;

    memset(&c, 0, sizeof c);
    c.unchecked = parser_is_word(p, word, "unchecked");
    c.level = level;
    c.block = p->block;
    c.pos = parser_pos_of(word);
    parser_next(p);
    first = last = parser_peek(p);
    if (name_word(p, first)) {
        parser_next(p);
        while (parser_check(p, TOKEN_MINUS) && adjacent(last, parser_peek(p)) &&
               adjacent(parser_peek(p), parser_peek_at(p, 1)) &&
               name_word(p, parser_peek_at(p, 1))) {
            parser_next(p);
            last = parser_next(p);
        }
        c.name.text = p->source + first->offset;
        c.name.length = last->offset + last->length - first->offset;
    }
    if (c.name.length == 0 || !parser_accept(p, TOKEN_COMMA) ||
        !parser_check(p, TOKEN_STRING)) {
        parser_error_here(p, "`%s` takes a name and a reason, "
                             "`%s(name, \"reason\")`",
                          c.unchecked ? "unchecked" : "allow",
                          c.unchecked ? "unchecked" : "allow");
        return false;
    }
    c.reason = parser_next(p)->value.text;
    if (!parser_expect(p, TOKEN_RPAREN)) {
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
    parser_list_push(p->clauses, &c);
    return true;
}

/* Give the clauses read since mark that still wait the source from
   `from` to the last token read. An inner statement closed its own
   clauses first, so only the ones of this level wait. */
void parser_close_clauses(struct parser *p, size_t mark, struct pos from)
{
    struct clause *all = p->clauses->data;
    struct pos to = parser_pos_of(&p->tokens[p->pos > 0 ? p->pos - 1 : 0]);
    size_t i;

    for (i = mark; i < p->clauses->count; i++) {
        if (all[i].to.line == 0) {
            all[i].from = from;
            all[i].to = to;
        }
    }
}

/* The clauses last in a declaration's header, before its brace. */
bool parser_header_clauses(struct parser *p)
{
    while (parser_clause_ahead(p, 0)) {
        if (!parser_read_clause(p, CLAUSE_DECLARATION)) {
            return false;
        }
    }
    return true;
}

/* Where the source a declaration's clause covers starts: its doc
   comment, where a doc warning stands, or its first word. */
struct pos parser_declaration_start(const struct item *it)
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

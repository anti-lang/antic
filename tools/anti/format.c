/* The formatting class of `anti check`.

   DESIGN: the class reads the rules of "Formatter rules" in
   docs/tooling-addendum.md against the token stream of the compiler's own
   lexer, and reports the line of every finding. It writes no canonical
   text, so `anti fmt` is no dependency of `anti check`. The rules it reads
   are the ones the tokens settle. The placement of a brace is not among
   them, because the canonical form keeps the one-line body that
   `concrete fn joined(self, o: *Object) { }` and
   `pub enum Mode: u8 { Read, Write }` are written in, and a token stream
   does not say which of the two forms an item asked for. */
#include "format.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "lexer.h"
#include "text.h"

static bool read_source(const char *path, struct text *out)
{
    FILE *f = fopen(path, "rb");
    char buffer[8192];
    size_t n;

    if (f == NULL) {
        fprintf(stderr, "anti: cannot read %s\n", path);
        return false;
    }
    while ((n = fread(buffer, 1, sizeof buffer, f)) > 0) {
        text_append_bytes(out, buffer, n);
    }
    fclose(f);
    return true;
}

/* The state of one file under the rules. */
struct layout {
    const char *source;
    size_t length;
    const struct token_list *tokens;
    unsigned char *code;        /* one byte per source byte, 1 in a token */
    struct diagnostics *out;
    /* The open braces. A brace knows whether `do` opened it, so that the
       `while` of a `do` block is told from a `while` that stands after
       another block. */
    struct brace {
        int line;
        bool from_do;
    } *braces;
    size_t depth;
    size_t capacity;
};

static void out_of_memory(void)
{
    fputs("anti: out of memory\n", stderr);
    exit(70);
}

static void push_brace(struct layout *l, int line, bool from_do)
{
    if (l->depth == l->capacity) {
        size_t capacity = l->capacity == 0 ? 32 : l->capacity * 2;
        struct brace *braces = realloc(l->braces, capacity * sizeof *braces);
        if (braces == NULL) {
            out_of_memory();
        }
        l->braces = braces;
        l->capacity = capacity;
    }
    l->braces[l->depth].line = line;
    l->braces[l->depth].from_do = from_do;
    l->depth++;
}

/* The bytes every token covers. A rule of the layout then passes over
   the inside of a literal and of a doc comment. An ordinary comment is no
   token at all and is passed over as well. */
static void mark_code(struct layout *l)
{
    size_t i;

    l->code = calloc(l->length + 1, 1);
    if (l->code == NULL) {
        out_of_memory();
    }
    for (i = 0; i < l->tokens->count; i++) {
        const struct token *t = &l->tokens->items[i];
        size_t end = t->offset + t->length;
        size_t b;
        for (b = t->offset; b < end && b < l->length; b++) {
            l->code[b] = 1;
        }
    }
}

/* The indent of the line that starts at byte `from`: the number of tabs,
   with spaces counted separately. */
struct indent {
    size_t tabs;
    size_t spaces;
    size_t bytes;               /* the whole leading whitespace */
};

static struct indent indent_of(const struct layout *l, size_t from)
{
    struct indent in = {0, 0, 0};

    while (from + in.bytes < l->length) {
        char c = l->source[from + in.bytes];
        if (c == '\t') {
            in.tabs++;
        } else if (c == ' ') {
            in.spaces++;
        } else {
            break;
        }
        in.bytes++;
    }
    return in;
}

/* The lines of the file, one start offset each, counted from 1. */
static size_t *line_starts(const struct layout *l, size_t *count)
{
    size_t lines = 2;
    size_t *starts;
    size_t i;

    for (i = 0; i < l->length; i++) {
        lines += l->source[i] == '\n';
    }
    starts = calloc(lines + 1, sizeof *starts);
    if (starts == NULL) {
        out_of_memory();
    }
    starts[1] = 0;
    *count = 1;
    for (i = 0; i < l->length; i++) {
        if (l->source[i] == '\n') {
            starts[++(*count)] = i + 1;
        }
    }
    return starts;
}

/* The indent of every line that starts with code. It is tabs alone, and
   no tab stands past it, which is what "Nothing is aligned past the
   indent" asks. A line that starts inside a comment or a literal is no
   concern of the rule. A tab before a trailing comment keeps its
   position. */
static void check_indents(struct layout *l)
{
    size_t count = 0;
    size_t *starts = line_starts(l, &count);
    size_t line;

    for (line = 1; line <= count; line++) {
        size_t from = starts[line];
        struct indent in = indent_of(l, from);
        size_t first = from + in.bytes;
        size_t last = first;
        size_t b;
        if (first >= l->length || l->source[first] == '\n' ||
            l->code[first] == 0) {
            continue;
        }
        if (in.spaces > 0) {
            diagnostics_add(l->out, (int)line, 1,
                            "the indent is tabs, and this line starts with a "
                            "space");
        }
        for (b = first; b < l->length && l->source[b] != '\n'; b++) {
            if (l->code[b] != 0) {
                last = b;
            }
        }
        for (b = first; b < last; b++) {
            if (l->source[b] == '\t' && l->code[b] == 0) {
                diagnostics_add(l->out, (int)line, (int)(b - from + 1),
                                "a tab stands past the indent, and nothing is "
                                "aligned");
                break;
            }
        }
    }
    free(starts);
}

/* Whether the token at index is the first of its line. */
static bool first_on_line(const struct layout *l, size_t index)
{
    return index == 0 ||
           l->tokens->items[index - 1].line != l->tokens->items[index].line;
}

/* The line of the token, or 0 past the end. */
static int line_of(const struct layout *l, size_t index)
{
    return index < l->tokens->count ? l->tokens->items[index].line : 0;
}

static enum token_kind kind_of(const struct layout *l, size_t index)
{
    return index < l->tokens->count ? l->tokens->items[index].kind : TOKEN_EOF;
}

/* The tokens of one file against the rules that a token stream settles. */
static void check_tokens(struct layout *l)
{
    size_t depth_of_condition = 0;
    int condition_line = 0;
    size_t brackets = 0;
    bool in_condition = false;
    size_t i;

    for (i = 0; i < l->tokens->count; i++) {
        const struct token *t = &l->tokens->items[i];
        switch (t->kind) {
        case TOKEN_IF:
        case TOKEN_WHILE:
            /* Parentheses around a whole condition are dropped. */
            if (kind_of(l, i + 1) == TOKEN_LPAREN) {
                size_t j = i + 2;
                size_t open = 1;
                while (j < l->tokens->count && open > 0) {
                    open += kind_of(l, j) == TOKEN_LPAREN;
                    open -= kind_of(l, j) == TOKEN_RPAREN;
                    j++;
                }
                if (open == 0 && (kind_of(l, j) == TOKEN_LBRACE ||
                                  kind_of(l, j) == TOKEN_DO)) {
                    diagnostics_add(l->out, t->line, t->column,
                                    "the parentheses around the condition are "
                                    "dropped");
                }
            }
            in_condition = true;
            condition_line = t->line;
            depth_of_condition = l->depth;
            break;
        case TOKEN_ELSE:
            if (i > 0 && kind_of(l, i - 1) == TOKEN_RBRACE &&
                line_of(l, i - 1) != t->line) {
                diagnostics_add(l->out, t->line, t->column,
                                "`} else {` stands on one line");
            }
            break;
        case TOKEN_LBRACE:
            /* A statement block opens its brace on the line of the
               statement, which the condition of an `if` or a `while` may
               have wrapped. `do {` and `else {` are the two that follow
               their keyword directly. */
            if ((kind_of(l, i - 1) == TOKEN_DO ||
                 kind_of(l, i - 1) == TOKEN_ELSE) &&
                line_of(l, i - 1) != t->line) {
                diagnostics_add(l->out, t->line, t->column,
                                "the brace of a statement block opens on the "
                                "line of the statement");
            }
            if (in_condition && l->depth == depth_of_condition &&
                t->line != condition_line && first_on_line(l, i)) {
                diagnostics_add(l->out, t->line, t->column,
                                "the brace of a statement block opens on the "
                                "line of the statement");
            }
            in_condition = false;
            /* `do {` of a `do` block starts its statement, and the `do {`
               of `while cond do {` stands after the condition. Only the
               first takes a `while` on the line of its closing brace. */
            push_brace(l, t->line,
                       kind_of(l, i - 1) == TOKEN_DO && i >= 2 &&
                           (kind_of(l, i - 2) == TOKEN_LBRACE ||
                            kind_of(l, i - 2) == TOKEN_RBRACE ||
                            kind_of(l, i - 2) == TOKEN_SEMICOLON ||
                            kind_of(l, i - 2) == TOKEN_COLON));
            break;
        case TOKEN_RBRACE:
            /* `} while cond` of a `do` block is one line. A `while` after
               another block starts a statement of its own. */
            if (l->depth > 0) {
                struct brace opened = l->braces[--l->depth];
                if (opened.from_do && kind_of(l, i + 1) == TOKEN_WHILE &&
                    line_of(l, i + 1) != t->line) {
                    diagnostics_add(l->out, line_of(l, i + 1),
                                    l->tokens->items[i + 1].column,
                                    "`} while` stands on one line");
                }
            }
            break;
        case TOKEN_LPAREN:
        case TOKEN_LBRACKET:
            brackets++;
            break;
        case TOKEN_RPAREN:
        case TOKEN_RBRACKET:
            brackets -= brackets > 0;
            break;
        case TOKEN_SEMICOLON:
            /* The `;` of `[0; 8]` separates the parts of one expression
               and ends no statement. */
            if (brackets == 0 && line_of(l, i + 1) == t->line &&
                kind_of(l, i + 1) != TOKEN_RBRACE &&
                kind_of(l, i + 1) != TOKEN_EOF) {
                diagnostics_add(l->out, line_of(l, i + 1),
                                l->tokens->items[i + 1].column,
                                "one statement per line");
            }
            break;
        default:
            break;
        }
    }
}

/* One tab per level, with one more for a wrapped line. The level of a
   line is the number of open braces before its first token. A line that
   starts with a closing brace stands one level out. */
static void check_levels(struct layout *l)
{
    size_t count = 0;
    size_t *starts = line_starts(l, &count);
    size_t i;

    l->depth = 0;
    for (i = 0; i < l->tokens->count; i++) {
        const struct token *t = &l->tokens->items[i];
        size_t level = l->depth;
        if (t->kind == TOKEN_RBRACE && level > 0) {
            level--;
        }
        if (first_on_line(l, i) && (size_t)t->line <= count) {
            struct indent in = indent_of(l, starts[t->line]);
            if (in.tabs != level && in.tabs != level + 1) {
                diagnostics_add(l->out, t->line, 1,
                                "the line stands %zu tab%s in, and its level "
                                "is %zu",
                                in.tabs, in.tabs == 1 ? "" : "s", level);
            }
        }
        l->depth += t->kind == TOKEN_LBRACE;
        l->depth -= t->kind == TOKEN_RBRACE && l->depth > 0;
    }
    free(starts);
}

bool format_check(const char *path, struct diagnostics *out)
{
    struct arena arena = {0};
    struct diagnostics lexed = {0};
    struct token_list tokens = {0};
    struct text source = {0};
    struct layout l;

    if (!read_source(path, &source)) {
        text_free(&source);
        return false;
    }
    memset(&l, 0, sizeof l);
    if (lex(text_cstr(&source), source.length, &arena, &lexed, &tokens)) {
        l.source = text_cstr(&source);
        l.length = source.length;
        l.tokens = &tokens;
        l.out = out;
        mark_code(&l);
        check_indents(&l);
        check_tokens(&l);
        l.depth = 0;
        check_levels(&l);
    }
    free(l.code);
    free(l.braces);
    token_list_free(&tokens);
    diagnostics_free(&lexed);
    arena_free(&arena);
    text_free(&source);
    return true;
}

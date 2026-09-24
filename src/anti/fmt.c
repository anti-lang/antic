/* `anti fmt`, the canonical form of an Anti source.

   DESIGN: the formatter reads the compiler's own lexer, so the canonical
   form cannot disagree with the language. The lexer drops an ordinary
   comment, which the rules keep. The gaps between two tokens are scanned
   for one, and the two streams are merged.

   DESIGN: the line breaks of the author stand, and the rules move the
   ones they name. The brace of an item body goes onto its own line and
   the brace of a statement block onto the line of its statement.
   `} else {` and `} while` each come to one line, and a second statement
   of a line goes onto its own. The rules of docs/tooling-addendum.md name the
   indent, the braces, the parentheses of a condition, one statement per
   line and the width of a doc comment. None of them re-flows an
   expression, so a wrapped line stays where it was written and takes one
   extra tab. See docs/notes/fmt.md. */
#include "fmt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "diagnostic.h"
#include "files.h"
#include "lexer.h"
#include "text.h"

/* The width a doc comment is re-wrapped to, and the columns of one tab,
   which `anti html` and `anti tex` render as four spaces. */
#define WRAP_COLUMNS 80
#define TAB_COLUMNS 4

/* One token or one ordinary comment of the source, in the order the
   bytes stand. */
enum piece_kind { PIECE_TOKEN, PIECE_COMMENT };

struct piece {
    enum piece_kind kind;
    const struct token *token;  /* null in a comment. */
    size_t offset;
    size_t length;
    int line;                   /* the line the piece starts on. */
    int end_line;               /* the line the piece ends on. */
    bool own_line;              /* nothing but whitespace before it. */
    bool blank_before;          /* an empty line stands before it. */
    bool dropped;               /* a parenthesis of a condition. */
    size_t match;               /* of a `{`, the index of its `}`. */
};

struct piece_list {
    struct piece *items;
    size_t count;
    size_t capacity;
};

static struct piece *add_piece(struct piece_list *l)
{
    if (l->count == l->capacity) {
        l->items = files_grow(l->items, &l->capacity, sizeof *l->items);
    }
    memset(&l->items[l->count], 0, sizeof l->items[0]);
    return &l->items[l->count++];
}

static int newlines_in(const char *src, size_t from, size_t to)
{
    int n = 0;

    while (from < to) {
        n += src[from++] == '\n';
    }
    return n;
}

/* Whether nothing but whitespace stands before the byte on its line. */
static bool starts_line(const char *src, size_t offset)
{
    while (offset > 0) {
        char c = src[offset - 1];
        if (c == '\n') {
            return true;
        }
        if (c != ' ' && c != '\t' && c != '\r') {
            return false;
        }
        offset--;
    }
    return true;
}

/* The comments in the bytes between two tokens. The lexer leaves nothing
   else there but whitespace, so a slash opens one of the two forms. */
static void scan_gap(const char *src, size_t from, size_t to, int *line,
                     size_t *seen, struct piece_list *out)
{
    size_t i = from;

    while (i < to) {
        struct piece *p;
        size_t start = i;
        int at;
        if (src[i] != '/') {
            i++;
            continue;
        }
        at = *line + newlines_in(src, *seen, start);
        if (i + 1 < to && src[i + 1] == '/') {
            while (i < to && src[i] != '\n') {
                i++;
            }
        } else {
            i += 2;
            while (i + 1 < to && !(src[i] == '*' && src[i + 1] == '/')) {
                i++;
            }
            i = i + 2 <= to ? i + 2 : to;
        }
        p = add_piece(out);
        p->kind = PIECE_COMMENT;
        p->offset = start;
        p->length = i - start;
        p->line = at;
        p->end_line = at + newlines_in(src, start, i);
        p->own_line = starts_line(src, start);
        *line = at;
        *seen = start;
    }
}

/* The tokens and the ordinary comments of the source in one stream. Each
   carries the line it stands on and whether an empty line comes first. */
static void collect(const char *src, size_t length,
                    const struct token_list *tokens, struct piece_list *out)
{
    size_t pos = 0;
    size_t seen = 0;
    int line = 1;
    size_t i;

    for (i = 0; i < tokens->count; i++) {
        const struct token *t = &tokens->items[i];
        struct piece *p;
        if (t->kind == TOKEN_EOF) {
            break;
        }
        scan_gap(src, pos, t->offset, &line, &seen, out);
        p = add_piece(out);
        p->kind = PIECE_TOKEN;
        p->token = t;
        p->offset = t->offset;
        p->length = t->length;
        p->line = t->line;
        p->end_line = t->line + newlines_in(src, t->offset,
                                            t->offset + t->length);
        p->own_line = starts_line(src, t->offset);
        line = t->line;
        seen = t->offset;
        pos = t->offset + t->length;
    }
    scan_gap(src, pos, length, &line, &seen, out);
    for (i = 0; i < out->count; i++) {
        struct piece *p = &out->items[i];
        size_t before = i == 0 ? 0 : out->items[i - 1].offset +
                                     out->items[i - 1].length;
        p->blank_before = newlines_in(src, before, p->offset) >= 2;
    }
}

static enum token_kind kind_at(const struct piece_list *l, size_t i)
{
    if (i >= l->count || l->items[i].kind != PIECE_TOKEN) {
        return TOKEN_EOF;
    }
    return l->items[i].token->kind;
}

/* The `}` of every `{`, and the parentheses that stand around a whole
   condition, which the canonical form drops. */
static void pair_braces(struct piece_list *l)
{
    size_t *open = files_array(l->count + 1, sizeof *open);
    size_t depth = 0;
    size_t i;

    for (i = 0; i < l->count; i++) {
        enum token_kind kind = kind_at(l, i);
        if (kind == TOKEN_LBRACE) {
            open[depth++] = i;
        } else if (kind == TOKEN_RBRACE && depth > 0) {
            l->items[open[--depth]].match = i;
        }
    }
    for (i = 0; i < l->count; i++) {
        size_t j;
        size_t nesting = 1;
        if (kind_at(l, i) != TOKEN_IF && kind_at(l, i) != TOKEN_WHILE) {
            continue;
        }
        if (kind_at(l, i + 1) != TOKEN_LPAREN) {
            continue;
        }
        for (j = i + 2; j < l->count && nesting > 0; j++) {
            nesting += kind_at(l, j) == TOKEN_LPAREN;
            nesting -= kind_at(l, j) == TOKEN_RPAREN;
        }
        if (nesting > 0 || (kind_at(l, j) != TOKEN_LBRACE &&
                            kind_at(l, j) != TOKEN_DO)) {
            continue;
        }
        l->items[i + 1].dropped = true;
        l->items[j - 1].dropped = true;
    }
    free(open);
}

/* What a brace belongs to, which decides where it opens. */
enum brace_kind { BRACE_ITEM, BRACE_BLOCK, BRACE_LITERAL, BRACE_ANONYMOUS };

/* The statement a brace interrupts, kept so that the one after it reads
   the same as the one before. */
struct statement {
    bool open;                  /* a token of it stands already */
    bool item;                  /* it declares an item with a body */
    bool governed;              /* a word of it governs a block */
    size_t colons;              /* the `:` of it outside brackets */
    enum token_kind first;
};

struct frame {
    enum brace_kind kind;
    bool inlined;               /* the `}` stands on the line of the `{` */
    bool from_do;               /* `do {`, so `} while` joins */
    struct statement saved;
    /* The body of an anonymous function interrupts an expression. The
       indent, the continuation and the open brackets of the line it
       stands in come back at its `}`. */
    size_t depth;
    bool cont;
    size_t brackets;
    bool *bracket_type;
};

struct emitter {
    const char *src;
    struct text *out;
    struct text line;           /* the current line without its indent */
    size_t depth;
    bool cont;                  /* the line continues a statement */
    bool blank;                 /* an empty line waits for the next line */
    bool after_do;              /* a `}` that a `do` opened stands last */
    bool do_tail;               /* the `while` of a `do` block is open */
    size_t inlined;             /* open braces that stay on one line */
    size_t brackets;            /* open `(` and `[` */
    bool *bracket_type;         /* of each, whether a `[` opened a type */
    size_t bracket_capacity;
    bool type_bracket;          /* the last `]` closed a type */
    const struct piece *prev;   /* the last piece on the line */
    const struct piece *prev2;
    bool prev_prefix;           /* the last piece is a prefix operator */
    struct frame *frames;
    size_t frame_count;
    size_t frame_capacity;
    struct statement stmt;
    const struct piece *stmt_start; /* the first piece of the statement */
    /* The `(` of an `allow` or `unchecked` clause, as a count of open
       brackets with it, and 0 outside one. The name before its comma is
       written without spaces, `shadowed-catch`. */
    size_t clause_depth;
    bool clause_name;
    bool clause_statement;      /* the clause opens its statement */
    /* An anonymous function whose body has not opened yet, and the open
       brackets its `fn` stood in. */
    bool anonymous;
    size_t anonymous_brackets;
};

static void text_clear(struct text *t)
{
    t->length = 0;
    if (t->data != NULL) {
        t->data[0] = '\0';
    }
}

static void push_frame(struct emitter *e, struct frame f)
{
    if (e->frame_count == e->frame_capacity) {
        e->frames = files_grow(e->frames,
                               &e->frame_capacity, sizeof *e->frames);
    }
    e->frames[e->frame_count++] = f;
}

static void push_bracket(struct emitter *e, bool type)
{
    if (e->brackets == e->bracket_capacity) {
        e->bracket_type = files_grow(e->bracket_type,
                                     &e->bracket_capacity,
                                     sizeof *e->bracket_type);
    }
    e->bracket_type[e->brackets++] = type;
}

static void flush(struct emitter *e)
{
    size_t tabs = e->depth + (e->cont ? 1 : 0);
    size_t i;

    if (e->line.length == 0) {
        return;
    }
    if (e->blank && e->out->length > 0) {
        text_append(e->out, "\n");
    }
    e->blank = false;
    for (i = 0; i < tabs; i++) {
        text_append(e->out, "\t");
    }
    text_append_bytes(e->out, e->line.data, e->line.length);
    text_append(e->out, "\n");
    text_clear(&e->line);
    e->prev = NULL;
    e->prev2 = NULL;
    e->prev_prefix = false;
    e->type_bracket = false;
}

/* The words that read as a keyword in their place and are identifiers to
   the lexer. A value never ends with one, so the operator after it opens
   an expression: `for i in 0..10 by -3` steps by minus three. Every other
   contextual word stands before a name, where nothing turns on it. */
static bool contextual_word(const char *s, size_t n)
{
    static const char *const words[] = { "by", "in" };
    size_t i;

    for (i = 0; i < sizeof words / sizeof words[0]; i++) {
        if (strlen(words[i]) == n && memcmp(words[i], s, n) == 0) {
            return true;
        }
    }
    return false;
}

/* Whether the piece is the word of an `allow` or `unchecked` clause,
   which the `(` after it opens. */
static bool clause_word(const struct emitter *e, const struct piece *p)
{
    const char *s;

    if (p == NULL || p->kind != PIECE_TOKEN ||
        p->token->kind != TOKEN_IDENT) {
        return false;
    }
    s = e->src + p->offset;
    return (p->length == 5 && memcmp(s, "allow", 5) == 0) ||
           (p->length == 9 && memcmp(s, "unchecked", 9) == 0);
}

/* Whether the kind names a type, so that `[8]int` and `chan int(16)`
   close up and a call of one does too. */
static bool type_word(enum token_kind kind)
{
    switch (kind) {
    case TOKEN_BOOL_TYPE:
    case TOKEN_BYTE_TYPE:
    case TOKEN_CHAR_TYPE:
    case TOKEN_F16:
    case TOKEN_F32:
    case TOKEN_F64:
    case TOKEN_FLOAT_TYPE:
    case TOKEN_I8:
    case TOKEN_I16:
    case TOKEN_I32:
    case TOKEN_I64:
    case TOKEN_INT_TYPE:
    case TOKEN_STR_TYPE:
    case TOKEN_U8:
    case TOKEN_U16:
    case TOKEN_U32:
    case TOKEN_U64:
    case TOKEN_UINT_TYPE:
    case TOKEN_C_CHAR:
    case TOKEN_C_DOUBLE:
    case TOKEN_C_FLOAT:
    case TOKEN_C_INT:
    case TOKEN_C_LONG:
    case TOKEN_C_LONGLONG:
    case TOKEN_C_SHORT:
    case TOKEN_C_SIZE_T:
    case TOKEN_C_UCHAR:
    case TOKEN_C_UINT:
    case TOKEN_C_ULONG:
    case TOKEN_C_ULONGLONG:
    case TOKEN_C_USHORT:
    case TOKEN_C_WCHAR:
        return true;
    default:
        return false;
    }
}

/* Whether the piece ends a value. The operator after one is binary, and
   a `(` after one opens a call. */
static bool ends_value(const struct piece *p)
{
    if (p == NULL || p->kind != PIECE_TOKEN) {
        return false;
    }
    switch (p->token->kind) {
    case TOKEN_IDENT:
    case TOKEN_INT:
    case TOKEN_FLOAT:
    case TOKEN_CHAR:
    case TOKEN_STRING:
    case TOKEN_BYTES:
    case TOKEN_FORMAT:
    case TOKEN_PATTERN:
    case TOKEN_RPAREN:
    case TOKEN_RBRACKET:
    case TOKEN_RBRACE:
    case TOKEN_SELF:
    case TOKEN_SUPER:
    case TOKEN_TRUE:
    case TOKEN_FALSE:
    case TOKEN_NONE:
    case TOKEN_HERE:
        return true;
    default:
        return type_word(p->token->kind);
    }
}

/* Whether the piece written last ends a value. A name behind a dot is
   one whatever the lexer calls the word, so `simd.select(m, a, b)` calls
   a function of a module and `chan.recv` reads a field. */
static bool last_ends_value(const struct emitter *e)
{
    if (e->prev2 != NULL && e->prev2->kind == PIECE_TOKEN &&
        (e->prev2->token->kind == TOKEN_DOT ||
         e->prev2->token->kind == TOKEN_QUESTION_DOT ||
         e->prev2->token->kind == TOKEN_COLON_COLON) &&
        e->prev != NULL && e->prev->kind == PIECE_TOKEN) {
        return true;
    }
    return ends_value(e->prev);
}

/* Whether a value opens after the piece written last. The word before an
   operator may read as a keyword, and no value ends with one. The step of
   `for i in 0..10 by -3` is minus three. `by[0..1]` indexes a slice that
   a variable of that name holds. */
static bool last_opens_value(const struct emitter *e)
{
    if (e->prev2 != NULL && e->prev2->kind == PIECE_TOKEN &&
        (e->prev2->token->kind == TOKEN_DOT ||
         e->prev2->token->kind == TOKEN_QUESTION_DOT ||
         e->prev2->token->kind == TOKEN_COLON_COLON)) {
        return false;
    }
    if (e->prev != NULL && e->prev->kind == PIECE_TOKEN &&
        e->prev->token->kind == TOKEN_IDENT &&
        contextual_word(e->src + e->prev->offset, e->prev->length)) {
        return true;
    }
    return !last_ends_value(e);
}

/* The words that take their arguments in parentheses, so that no space
   stands between the word and the `(`. */
static bool call_word(enum token_kind kind)
{
    switch (kind) {
    case TOKEN_ALLOC:
    case TOKEN_ASSERT:
    case TOKEN_DELETE:
    case TOKEN_DESTROY:
    case TOKEN_DUP:
    case TOKEN_FN:
    case TOKEN_FREE:
    case TOKEN_JOIN:
    case TOKEN_JOIN_ALL:
    case TOKEN_RECV:
    case TOKEN_SEND:
    case TOKEN_SIZE_OF:
        return true;
    default:
        return false;
    }
}

/* Whether the kind opens a type, so that the `]` of `[]byte` closes up
   while the one of `[1, 2] as ...` does not. */
static bool opens_type(enum token_kind kind)
{
    return kind == TOKEN_IDENT || kind == TOKEN_STAR ||
           kind == TOKEN_QUESTION_STAR || kind == TOKEN_LBRACKET ||
           type_word(kind);
}

/* Whether a `+ - * &` in this place is a prefix, which binds to the value
   after it. */
static bool prefix_operator(const struct emitter *e, enum token_kind kind)
{
    switch (kind) {
    case TOKEN_BANG:
    case TOKEN_TILDE:
    case TOKEN_QUESTION:
    case TOKEN_QUESTION_STAR:
        return true;
    case TOKEN_STAR:
        /* The `*` of `[]*Object` points, and the `]` before it closed a
           type rather than an index. */
        return e->type_bracket || last_opens_value(e);
    case TOKEN_PLUS:
    case TOKEN_MINUS:
    case TOKEN_AMP:
        return last_opens_value(e);
    default:
        return false;
    }
}

/* One space, or none, between the last piece of the line and this one. */
static bool space_before(const struct emitter *e, const struct piece *p)
{
    enum token_kind cur;
    enum token_kind prev;

    if (e->line.length == 0 || e->prev == NULL) {
        return false;
    }
    if (p->kind == PIECE_COMMENT) {
        return true;
    }
    cur = p->token->kind;
    if (e->prev->kind == PIECE_COMMENT) {
        return true;
    }
    prev = e->prev->token->kind;
    /* The name of a clause joins its words with `-`, as it is written. */
    if (e->clause_name && (cur == TOKEN_MINUS || prev == TOKEN_MINUS) &&
        e->prev->offset + e->prev->length == p->offset) {
        return false;
    }
    switch (prev) {
    case TOKEN_LPAREN:
    case TOKEN_LBRACKET:
    case TOKEN_DOT:
    case TOKEN_DOT_DOT:
    case TOKEN_COLON_COLON:
    case TOKEN_QUESTION_DOT:
        return false;
    case TOKEN_QUESTION:
        /* `p as? *Circle` keeps its space, and `?fn(int)` has none. */
        return e->prev2 != NULL && e->prev2->kind == PIECE_TOKEN &&
               e->prev2->token->kind == TOKEN_AS;
    default:
        break;
    }
    if (e->prev_prefix) {
        return false;
    }
    switch (cur) {
    case TOKEN_COLON:
        /* The width of a bitfield stands behind a colon of its own,
           `layer: u32 : 4`, which the second colon of the field opens. */
        return e->brackets == 0 && e->stmt.colons > 0;
    case TOKEN_COMMA:
    case TOKEN_SEMICOLON:
    case TOKEN_RPAREN:
    case TOKEN_RBRACKET:
    case TOKEN_COLON_COLON:
    case TOKEN_DOT:
    case TOKEN_DOT_DOT:
    case TOKEN_QUESTION_DOT:
        return false;
    case TOKEN_QUESTION:
        return prev != TOKEN_AS;
    case TOKEN_LPAREN:
        return !last_ends_value(e) && !call_word(prev);
    case TOKEN_LBRACKET:
        return !last_ends_value(e);
    default:
        break;
    }
    /* `guarded by` follows the default of a field, which may be an
       array literal, and stands apart from it. */
    if (prev == TOKEN_RBRACKET && e->type_bracket && opens_type(cur) &&
        !(p->length == 7 && memcmp(e->src + p->offset, "guarded", 7) == 0)) {
        return false;
    }
    return true;
}

static void append_piece(struct emitter *e, const struct piece *p, bool space)
{
    if (space) {
        text_append(&e->line, " ");
    }
    text_append_bytes(&e->line, e->src + p->offset, p->length);
    e->prev_prefix = p->kind == PIECE_TOKEN &&
                     prefix_operator(e, p->token->kind);
    e->prev2 = e->prev;
    e->prev = p;
    e->type_bracket = false;
}

static void reset_statement(struct emitter *e)
{
    e->stmt.open = false;
    e->stmt.item = false;
    e->stmt.governed = false;
    e->stmt.colons = 0;
    e->stmt.first = TOKEN_EOF;
}

/* The words that declare an item with a body, and the ones that govern a
   statement block. A brace that follows neither builds a value. */
static bool item_word(enum token_kind kind)
{
    switch (kind) {
    case TOKEN_CLASS:
    case TOKEN_ENUM:
    case TOKEN_FIXTURES:
    case TOKEN_FN:
    case TOKEN_STRUCT:
    case TOKEN_TESTS:
    case TOKEN_UNION:
    case TOKEN_VARIANT:
        return true;
    default:
        return false;
    }
}

static bool governing_word(enum token_kind kind)
{
    switch (kind) {
    case TOKEN_CATCH:
    case TOKEN_DEFER:
    case TOKEN_DO:
    case TOKEN_ELSE:
    case TOKEN_FAT_ARROW:
    case TOKEN_FOR:
    case TOKEN_IF:
    case TOKEN_PARALLEL:
    case TOKEN_SELECT:
    case TOKEN_SWITCH:
    case TOKEN_SYNC:
    case TOKEN_UNDO:
    case TOKEN_WHILE:
        return true;
    default:
        return false;
    }
}

static void note_token(struct emitter *e, enum token_kind kind)
{
    if (!e->stmt.open) {
        e->stmt.first = kind;
    }
    e->stmt.open = true;
    if (e->brackets == 0) {
        e->stmt.item = e->stmt.item || item_word(kind);
        e->stmt.governed = e->stmt.governed || governing_word(kind);
        e->stmt.colons += kind == TOKEN_COLON;
    }
}

static enum brace_kind classify(const struct emitter *e)
{
    if (e->stmt.item) {
        return BRACE_ITEM;
    }
    if (e->stmt.governed || !e->stmt.open) {
        return BRACE_BLOCK;
    }
    return BRACE_LITERAL;
}

/* The columns n bytes cover. A byte of a character past the first counts
   none, and a tab counts what `anti html` renders. */
static size_t columns_of(const char *s, size_t n)
{
    size_t columns = 0;
    size_t i;

    for (i = 0; i < n; i++) {
        if (s[i] == '\t') {
            columns += TAB_COLUMNS;
        } else if (((unsigned char)s[i] & 0xC0) != 0x80) {
            columns++;
        }
    }
    return columns;
}

/* One paragraph of a doc comment, filled to the width. The text of first
   opens its first line and the text of rest opens every line after it. */
static void wrap_paragraph(struct text *out, const char *first,
                           const char *rest, const char *body, size_t length,
                           size_t columns)
{
    const char *open = first;
    size_t column = 0;
    size_t i = 0;
    bool fresh = true;

    while (i < length) {
        size_t start;
        size_t width;
        while (i < length && body[i] == ' ') {
            i++;
        }
        if (i == length) {
            break;
        }
        start = i;
        while (i < length && body[i] != ' ') {
            i++;
        }
        width = columns_of(body + start, i - start);
        if (fresh) {
            text_append(out, open);
            column = columns_of(open, strlen(open));
            fresh = false;
        } else if (column + 1 + width <= columns) {
            text_append(out, " ");
            column++;
        } else {
            text_append(out, "\n");
            text_append(out, rest);
            column = columns_of(rest, strlen(rest));
        }
        text_append_bytes(out, body + start, i - start);
        column += width;
        open = rest;
    }
    if (fresh) {
        text_append(out, first);
    }
}

/* The leading spaces of a line of doc text. */
static size_t doc_indent(const char *s, size_t length)
{
    size_t n = 0;

    while (n < length && s[n] == ' ') {
        n++;
    }
    return n;
}

static bool starts_with(const char *s, size_t length, const char *prefix)
{
    size_t n = strlen(prefix);

    return length >= n && memcmp(s, prefix, n) == 0;
}

/* Re-wrap the text of a doc comment to the width, with used columns
   already taken by the indent and the marker. A fenced block is written
   as it stands, a blank line separates two paragraphs, and a `- ` item
   carries its continuation two columns in. */
static void wrap_doc(const char *text, size_t length, size_t used,
                     struct text *out)
{
    size_t columns = used < WRAP_COLUMNS ? WRAP_COLUMNS - used : 1;
    size_t i = 0;
    bool fenced = false;
    bool first = true;

    while (i <= length) {
        size_t end = i;
        size_t indent;
        size_t body;
        size_t stop;
        bool bullet;
        char lead[80];
        char rest[80];
        struct text unit = {0};
        while (end < length && text[end] != '\n') {
            end++;
        }
        if (!first) {
            text_append(out, "\n");
        }
        first = false;
        indent = doc_indent(text + i, end - i);
        if (starts_with(text + i + indent, end - i - indent, "```")) {
            fenced = !fenced;
            text_append_bytes(out, text + i, end - i);
            i = end + 1;
            continue;
        }
        if (fenced || indent == end - i) {
            text_append_bytes(out, text + i, end - i);
            i = end + 1;
            continue;
        }
        bullet = starts_with(text + i + indent, end - i - indent, "- ");
        body = indent + (bullet ? 2 : 0);
        if (indent + 2 >= sizeof lead) {
            text_append_bytes(out, text + i, end - i);
            i = end + 1;
            continue;
        }
        memset(lead, ' ', indent);
        lead[indent] = '\0';
        memcpy(rest, lead, indent + 1);
        if (bullet) {
            memcpy(lead + indent, "- ", 3);
            memset(rest + indent, ' ', 2);
            rest[indent + 2] = '\0';
        }
        text_append_bytes(&unit, text + i + body, end - i - body);
        /* The lines that carry on the paragraph, which stand at the
           indent of its text and open no item of their own. */
        stop = end;
        while (stop < length) {
            size_t next = stop + 1;
            size_t tail = next;
            size_t lead_in;
            while (tail < length && text[tail] != '\n') {
                tail++;
            }
            lead_in = doc_indent(text + next, tail - next);
            if (lead_in == tail - next ||
                starts_with(text + next + lead_in, tail - next - lead_in,
                            "```") ||
                starts_with(text + next + lead_in, tail - next - lead_in,
                            "- ") ||
                lead_in != columns_of(rest, strlen(rest))) {
                break;
            }
            text_append(&unit, " ");
            text_append_bytes(&unit, text + next + lead_in,
                              tail - next - lead_in);
            stop = tail;
        }
        wrap_paragraph(out, lead, rest, unit.data == NULL ? "" : unit.data,
                       unit.length, columns);
        text_free(&unit);
        i = stop + 1;
    }
}

/* The bytes of the marker that opens a doc comment, three or four. */
static size_t marker_length(const char *s)
{
    return s[2] == '#' && s[3] == '!' ? 4 : 3;
}

/* Remove the ` * ` gutter of a block comment, which every non-empty line
   of the block carries when it has one. */
static void strip_gutter(struct text *text)
{
    const char *s = text_cstr(text);
    struct text out = {0};
    size_t i;
    bool any = false;

    for (i = 0; i <= text->length;) {
        size_t end = i;
        while (end < text->length && s[end] != '\n') {
            end++;
        }
        if (end > i) {
            if (s[i] != '*' || (end > i + 1 && s[i + 1] != ' ')) {
                return;
            }
            any = true;
        }
        i = end + 1;
    }
    if (!any) {
        return;
    }
    for (i = 0; i <= text->length;) {
        size_t end = i;
        size_t from = i;
        while (end < text->length && s[end] != '\n') {
            end++;
        }
        if (end > i) {
            from = i + (end > i + 1 ? 2 : 1);
        }
        if (i > 0) {
            text_append(&out, "\n");
        }
        text_append_bytes(&out, s + from, end - from);
        i = end + 1;
    }
    text_clear(text);
    text_append_bytes(text, out.data, out.length);
    text_free(&out);
}

/* One line of a doc comment, which stands even when it is empty: a
   blank line of the text separates two paragraphs. */
static void flush_doc_line(struct emitter *e)
{
    if (e->line.length > 0) {
        flush(e);
        return;
    }
    if (e->blank && e->out->length > 0) {
        text_append(e->out, "\n");
    }
    e->blank = false;
    text_append(e->out, "\n");
}

/* A doc comment in the form its author chose, re-wrapped to 80 columns
   and standing on the lines of its own. */
static void emit_doc(struct emitter *e, const struct piece *p)
{
    const char *src = e->src + p->offset;
    size_t marker = marker_length(src);
    bool block = src[1] == '*';
    size_t used = e->depth * TAB_COLUMNS;
    struct text text = {0};
    struct text wrapped = {0};
    const char *s;
    size_t i;

    text_append_bytes(&text, p->token->value.text.bytes,
                      p->token->value.text.length);
    if (block) {
        strip_gutter(&text);
        wrap_doc(text_cstr(&text), text.length, used, &wrapped);
    } else {
        wrap_doc(text_cstr(&text), text.length, used + marker + 1, &wrapped);
    }
    s = text_cstr(&wrapped);
    if (block && p->line == p->end_line &&
        newlines_in(s, 0, wrapped.length) == 0 &&
        used + marker + 1 + columns_of(s, wrapped.length) + 3 <=
            WRAP_COLUMNS) {
        text_append_bytes(&e->line, src, marker);
        if (wrapped.length > 0) {
            text_append(&e->line, " ");
            text_append_bytes(&e->line, s, wrapped.length);
        }
        text_append(&e->line, " */");
        flush(e);
        text_free(&text);
        text_free(&wrapped);
        return;
    }
    if (block) {
        text_append_bytes(&e->line, src, marker);
        flush(e);
    }
    for (i = 0; i <= wrapped.length;) {
        size_t end = i;
        while (end < wrapped.length && s[end] != '\n') {
            end++;
        }
        if (!block) {
            text_append_bytes(&e->line, src, marker);
            if (end > i) {
                text_append(&e->line, " ");
            }
        }
        text_append_bytes(&e->line, s + i, end - i);
        flush_doc_line(e);
        i = end + 1;
    }
    if (block) {
        text_append(&e->line, "*/");
        flush(e);
    }
    text_free(&text);
    text_free(&wrapped);
}

static bool is_doc(enum token_kind kind)
{
    return kind == TOKEN_DOC || kind == TOKEN_MODULE_DOC ||
           kind == TOKEN_NOTE || kind == TOKEN_MODULE_NOTE;
}

/* An ordinary comment of its own line, written at the indent of the
   code around it, or one after code, which keeps its place. */
static void emit_comment(struct emitter *e, const struct piece *p)
{
    const char *s = e->src + p->offset;
    size_t i;

    if (!p->own_line) {
        append_piece(e, p, e->line.length > 0);
        return;
    }
    flush(e);
    for (i = 0; i <= p->length;) {
        size_t end = i;
        while (end < p->length && s[end] != '\n') {
            end++;
        }
        if (i == 0) {
            text_append_bytes(&e->line, s, end);
            flush(e);
        } else {
            /* The lines below the first of a block comment stand as
               they were written, because nothing says what they align
               to. */
            if (e->blank && e->out->length > 0) {
                text_append(e->out, "\n");
                e->blank = false;
            }
            text_append_bytes(e->out, s + i, end - i);
            text_append(e->out, "\n");
        }
        i = end + 1;
    }
}

/* Whether the `while` of a `do` block follows the brace that closed it,
   so that the two stand on one line. */
static bool joins_do(const struct emitter *e, size_t i)
{
    return e->after_do && e->prev != NULL && e->prev->kind == PIECE_TOKEN &&
           e->prev->token->kind == TOKEN_RBRACE && i > 0;
}

/* Whether the `fn` about to be written opens an anonymous function: one
   that stands where a value starts, inside a statement already open. A
   `fn` after `:`, `->`, `as` or `?` writes a type. A `fn` after the word
   `snapshot` stands where the word stood. */
static bool anonymous_fn(const struct emitter *e)
{
    const struct piece *before = e->prev;

    if (!e->stmt.open || before == NULL || before->kind != PIECE_TOKEN) {
        return false;
    }
    if (before->token->kind == TOKEN_IDENT && before->length == 8 &&
        memcmp(e->src + before->offset, "snapshot", 8) == 0) {
        before = e->prev2;
        if (before == NULL || before->kind != PIECE_TOKEN) {
            return false;
        }
    }
    switch (before->token->kind) {
    case TOKEN_ASSIGN:
    case TOKEN_LPAREN:
    case TOKEN_LBRACKET:
    case TOKEN_COMMA:
    case TOKEN_RETURN:
    case TOKEN_YIELD:
    case TOKEN_FAT_ARROW:
    case TOKEN_QUESTION_QUESTION:
        return true;
    default:
        return false;
    }
}

/* DESIGN: the body of an anonymous function opens on the line of its
   signature. It indents one tab past that line, as a statement block
   does. Its `}` opens the line that goes on with the expression around
   it, at the indent of that line. A body written on one line stays. */
static void emit_anonymous_open(struct emitter *e, const struct piece_list *l,
                                const struct piece *p)
{
    bool inlined = e->inlined > 0 ||
                   (p->match > 0 && l->items[p->match].end_line == p->line);
    struct frame f;

    memset(&f, 0, sizeof f);
    e->anonymous = false;
    f.kind = BRACE_ANONYMOUS;
    f.inlined = inlined;
    f.saved = e->stmt;
    f.depth = e->depth;
    f.cont = e->cont;
    f.brackets = e->brackets;
    if (e->brackets > 0) {
        f.bracket_type = files_array(e->brackets, sizeof *f.bracket_type);
        memcpy(f.bracket_type, e->bracket_type,
               e->brackets * sizeof *f.bracket_type);
    }
    append_piece(e, p, space_before(e, p));
    push_frame(e, f);
    e->brackets = 0;
    reset_statement(e);
    if (inlined) {
        e->inlined++;
        return;
    }
    flush(e);
    e->depth = f.depth + (f.cont ? 1 : 0) + 1;
    e->cont = false;
}

static void emit_anonymous_close(struct emitter *e, const struct piece *p,
                                 struct frame *f)
{
    if (f->inlined) {
        e->inlined--;
        append_piece(e, p, true);
    } else {
        flush(e);
        e->depth = f->depth;
        e->cont = f->cont;
        append_piece(e, p, false);
    }
    e->stmt = f->saved;
    e->brackets = 0;
    while (e->brackets < f->brackets) {
        push_bracket(e, f->bracket_type[e->brackets]);
    }
    free(f->bracket_type);
}

static void emit_brace_open(struct emitter *e, const struct piece_list *l,
                            const struct piece *p)
{
    enum brace_kind kind = classify(e);
    bool inlined = e->inlined > 0 ||
                   (p->match > 0 && l->items[p->match].end_line == p->line);
    struct frame f;

    if (e->anonymous && e->brackets == e->anonymous_brackets) {
        emit_anonymous_open(e, l, p);
        return;
    }
    memset(&f, 0, sizeof f);
    if (!inlined && (kind == BRACE_ITEM || !e->stmt.open)) {
        flush(e);
        e->cont = false;
    }
    f.kind = kind;
    f.inlined = inlined;
    f.from_do = e->stmt.first == TOKEN_DO;
    f.saved = e->stmt;
    append_piece(e, p, space_before(e, p));
    push_frame(e, f);
    if (inlined) {
        e->inlined++;
        e->stmt.colons = 0;
        return;
    }
    flush(e);
    e->cont = false;
    e->depth++;
    reset_statement(e);
}

static void emit_brace_close(struct emitter *e, const struct piece *p)
{
    struct frame f;

    if (e->frame_count == 0) {
        append_piece(e, p, space_before(e, p));
        return;
    }
    f = e->frames[--e->frame_count];
    if (f.kind == BRACE_ANONYMOUS) {
        emit_anonymous_close(e, p, &f);
        return;
    }
    if (f.inlined) {
        e->inlined--;
        append_piece(e, p, true);
        if (f.kind == BRACE_LITERAL) {
            e->stmt = f.saved;
        } else {
            reset_statement(e);
        }
        return;
    }
    flush(e);
    e->cont = false;
    if (e->depth > 0) {
        e->depth--;
    }
    append_piece(e, p, false);
    if (f.kind == BRACE_LITERAL) {
        e->stmt = f.saved;
    } else {
        reset_statement(e);
    }
    e->after_do = f.from_do;
}

static void emit_token(struct emitter *e, const struct piece_list *l,
                       size_t index, const struct piece *p)
{
    enum token_kind kind = p->token->kind;
    bool space;

    switch (kind) {
    case TOKEN_LBRACE:
        emit_brace_open(e, l, p);
        return;
    case TOKEN_RBRACE:
        emit_brace_close(e, p);
        return;
    case TOKEN_LPAREN: {
        /* `fn allow(...)` declares a function of that name, and
           `x.allow(...)` calls one. */
        bool clause = clause_word(e, e->prev) &&
                      !(e->prev2 != NULL && e->prev2->kind == PIECE_TOKEN &&
                        (e->prev2->token->kind == TOKEN_FN ||
                         e->prev2->token->kind == TOKEN_DOT ||
                         e->prev2->token->kind == TOKEN_QUESTION_DOT));
        note_token(e, kind);
        append_piece(e, p, space_before(e, p));
        push_bracket(e, false);
        if (clause) {
            e->clause_depth = e->brackets;
            e->clause_name = true;
            e->clause_statement = e->stmt_start == e->prev2;
        }
        return;
    }
    case TOKEN_LBRACKET: {
        /* `[8]int` and `[]byte` open a type, and `a[i]` an index. The
           second bracket of `[][]i32` opens one as well. */
        bool type = e->type_bracket || !last_ends_value(e);
        note_token(e, kind);
        append_piece(e, p, space_before(e, p));
        push_bracket(e, type);
        return;
    }
    case TOKEN_RPAREN:
    case TOKEN_RBRACKET:
        note_token(e, kind);
        append_piece(e, p, space_before(e, p));
        if (e->brackets > 0) {
            e->brackets--;
            e->type_bracket = kind == TOKEN_RBRACKET &&
                              e->bracket_type[e->brackets];
        }
        /* A clause before a statement ends at its `)`, and the statement
           it covers opens the next line, which continues nothing. */
        if (e->clause_depth > 0 && e->brackets < e->clause_depth) {
            e->clause_depth = 0;
            e->clause_name = false;
            if (e->clause_statement && e->brackets == 0) {
                reset_statement(e);
            }
        }
        return;
    case TOKEN_SEMICOLON:
        append_piece(e, p, space_before(e, p));
        if (e->brackets == 0 && e->inlined == 0) {
            /* One statement per line ends the line here, unless an
               ordinary comment stands behind the `;`. A comment keeps
               its position, so the line ends after it. */
            if (index + 1 >= l->count ||
                l->items[index + 1].kind != PIECE_COMMENT ||
                l->items[index + 1].line != p->end_line) {
                flush(e);
                e->cont = false;
            }
            reset_statement(e);
            e->do_tail = false;
            e->after_do = false;
        }
        return;
    case TOKEN_COMMA:
        append_piece(e, p, space_before(e, p));
        if (e->clause_depth > 0 && e->brackets == e->clause_depth) {
            e->clause_name = false;
        }
        /* A comma at the level of a brace ends a field, an arm or a
           case wherever it stands. The colons of the next one then count
           from none. The word that governs the statement stands over the
           comma. The second name of `for i, x in items` opens no
           statement of its own, and the arm after `Circle c => { },` is
           a block as well. */
        if (e->brackets == 0) {
            e->stmt.colons = 0;
            if (e->inlined == 0) {
                e->stmt.open = false;
                e->stmt.first = TOKEN_EOF;
            }
        }
        return;
    default:
        break;
    }
    if (kind == TOKEN_WHILE && joins_do(e, index)) {
        e->do_tail = true;
    }
    e->after_do = false;
    /* The space is read before the token joins the statement, because a
       colon reads the colons the statement holds already. */
    space = space_before(e, p);
    if (!e->stmt.open) {
        e->stmt_start = p;
    }
    /* An anonymous function declares no item, so its `fn` leaves the
       statement as it is. */
    if (kind == TOKEN_FN && anonymous_fn(e)) {
        e->anonymous = true;
        e->anonymous_brackets = e->brackets;
        append_piece(e, p, space);
        return;
    }
    note_token(e, kind);
    append_piece(e, p, space);
}

/* Whether a line break of the source stands before the piece and the
   rules keep it. */
static bool breaks_line(struct emitter *e, const struct piece_list *l,
                        size_t index, const struct piece *p)
{
    enum token_kind kind;

    if (e->inlined > 0 || e->prev == NULL) {
        return false;
    }
    if (p->line <= e->prev->end_line) {
        return false;
    }
    if (p->kind != PIECE_TOKEN) {
        return true;
    }
    kind = p->token->kind;
    /* `} else {` and `} while cond` each stand on one line, and the
       brace of a statement block opens on the line of its statement. */
    if (kind == TOKEN_ELSE && e->prev->kind == PIECE_TOKEN &&
        e->prev->token->kind == TOKEN_RBRACE) {
        return false;
    }
    if (kind == TOKEN_WHILE && joins_do(e, index)) {
        return false;
    }
    if (kind == TOKEN_LBRACE) {
        bool inlined = p->match > 0 && l->items[p->match].end_line == p->line;
        if (e->anonymous && e->brackets == e->anonymous_brackets) {
            return false;
        }
        /* An item body and a block that stands on its own open a line,
           and every other brace joins the statement before it. */
        return !inlined && (classify(e) == BRACE_ITEM || !e->stmt.open);
    }
    return true;
}

bool fmt_source(const char *source, size_t length, struct text *out)
{
    struct arena arena = {0};
    struct diagnostics diags = {0};
    struct token_list tokens = {0};
    struct piece_list pieces = {0};
    struct emitter e;
    size_t i;

    if (!lex(source, length, &arena, &diags, &tokens)) {
        token_list_free(&tokens);
        diagnostics_free(&diags);
        arena_free(&arena);
        return false;
    }
    collect(source, length, &tokens, &pieces);
    pair_braces(&pieces);
    memset(&e, 0, sizeof e);
    e.src = source;
    e.out = out;
    reset_statement(&e);
    for (i = 0; i < pieces.count; i++) {
        const struct piece *p = &pieces.items[i];
        if (p->dropped) {
            continue;
        }
        if (breaks_line(&e, &pieces, i, p)) {
            flush(&e);
            e.cont = e.stmt.open && !e.do_tail;
            if (e.do_tail) {
                reset_statement(&e);
                e.do_tail = false;
            }
        }
        if (p->blank_before && e.line.length == 0) {
            e.blank = true;
        }
        if (p->kind == PIECE_COMMENT) {
            emit_comment(&e, p);
        } else if (is_doc(p->token->kind)) {
            flush(&e);
            e.cont = false;
            emit_doc(&e, p);
        } else {
            emit_token(&e, &pieces, i, p);
        }
    }
    flush(&e);
    free(e.frames);
    free(e.bracket_type);
    text_free(&e.line);
    free(pieces.items);
    token_list_free(&tokens);
    diagnostics_free(&diags);
    arena_free(&arena);
    return true;
}

int fmt_run(const char *const *paths, size_t count, bool check)
{
    size_t changed = 0;
    int status = 0;
    size_t i;

    for (i = 0; i < count; i++) {
        struct text source = {0};
        struct text formed = {0};
        if (!files_read_reported(paths[i], &source)) {
            text_free(&source);
            status = 1;
            continue;
        }
        /* A source the lexer refuses is the front end's to report, and
           the canonical form of it is unknown, so it stays as it is. */
        if (!fmt_source(text_cstr(&source), source.length, &formed)) {
            fprintf(stderr, "anti: %s does not lex and stays as it is\n",
                    paths[i]);
            text_free(&source);
            text_free(&formed);
            status = 1;
            continue;
        }
        if (formed.length != source.length ||
            memcmp(text_cstr(&formed), text_cstr(&source), formed.length) !=
                0) {
            changed++;
            printf("%s\n", paths[i]);
            if (!check && !files_write(paths[i], &formed)) {
                status = 1;
            }
        }
        text_free(&source);
        text_free(&formed);
    }
    if (check && changed > 0) {
        status = 1;
    }
    return status;
}

/* The parser of a module and the helpers its files share: the stream of
   tokens, the errors and the depth of nesting, the lists, the doc comments
   and the recovery after an error. parser_parse reads the imports, the
   clauses of the whole file, the `provides` and `link` lines and the
   `tests` and `fixtures` blocks, and hands every other item to
   parser_item. */

#include "parser.h"

#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "parser_parser.h"
#include "text.h"

void parser_list_push(struct list *l, const void *element)
{
    l->data = alloc_grow(l->data, &l->capacity, l->count, l->size);
    memcpy((char *)l->data + l->count * l->size, element, l->size);
    l->count++;
}

void *parser_list_finish(struct parser *p, struct list *l, size_t *count)
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

const struct token *parser_peek(const struct parser *p)
{
    return &p->tokens[p->pos];
}

/* The token ahead places after the current one, or the TOKEN_EOF at the
   end. PERF DECISION: one step, since the lookaheads and the scan of `<`
   ask for tokens far ahead once per token they pass. */
const struct token *parser_peek_at(const struct parser *p, size_t ahead)
{
    return &p->tokens[ahead < p->last - p->pos ? p->pos + ahead : p->last];
}

bool parser_check(const struct parser *p, enum token_kind kind)
{
    return parser_peek(p)->kind == kind;
}

const struct token *parser_next(struct parser *p)
{
    const struct token *t = parser_peek(p);

    if (t->kind != TOKEN_EOF) {
        p->pos++;
    }
    return t;
}

bool parser_accept(struct parser *p, enum token_kind kind)
{
    if (parser_check(p, kind)) {
        parser_next(p);
        return true;
    }
    return false;
}

struct pos parser_pos_of(const struct token *t)
{
    struct pos pos = {t->line, t->column};
    return pos;
}

void parser_error_here(struct parser *p, const char *format, ...)
{
    va_list args;
    char message[sizeof p->diags->items->message];

    p->ok = false;
    if (p->panic) {
        return;
    }
    p->panic = true;
    va_start(args, format);
    text_vformat(message, sizeof message, format, args);
    va_end(args);
    diagnostics_add(p->diags, parser_peek(p)->line, parser_peek(p)->column,
                    "%s", message);
}

static void too_deep(struct parser *p)
{
    p->ok = false;
    if (!p->panic) {
        p->panic = true;
        diagnostics_add(p->diags, parser_peek(p)->line, parser_peek(p)->column,
                        "nesting deeper than %d levels", PARSE_DEPTH_MAX);
    }
}

/* Enter one level of nesting. Past PARSE_DEPTH_MAX it reports the
   source and returns false, and the caller returns NULL without
   entering. Each true return is paired with one parser_ascend. */
bool parser_descend(struct parser *p)
{
    if (p->depth >= PARSE_DEPTH_MAX) {
        too_deep(p);
        return false;
    }
    p->depth++;
    if (p->reach < p->depth) {
        p->reach = p->depth;
    }
    return true;
}

void parser_ascend(struct parser *p)
{
    p->depth--;
}

void parser_chain_begin(struct parser *p, struct chain *c)
{
    c->saved = p->reach;
    p->reach = p->depth;
}

/* One link, a node that holds everything the chain built so far. */
bool parser_chain_link(struct parser *p)
{
    if (p->reach >= PARSE_DEPTH_MAX) {
        too_deep(p);
        return false;
    }
    p->reach++;
    return true;
}

void parser_chain_end(struct parser *p, const struct chain *c)
{
    if (p->reach < c->saved) {
        p->reach = c->saved;
    }
}

bool parser_expect(struct parser *p, enum token_kind kind)
{

    if (parser_accept(p, kind)) {
        return true;
    }
    parser_error_here(p, "expected %s", lexer_token_kind_name(kind));
    return false;
}

bool parser_expect_name(struct parser *p, struct name *name)
{
    const struct token *t = parser_peek(p);

    if (!parser_expect(p, TOKEN_IDENT)) {
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
bool parser_is_field_name(const struct token *t)
{
    return t->kind == TOKEN_IDENT || t->kind == TOKEN_ALLOC ||
           t->kind == TOKEN_FREE;
}

bool parser_expect_member_name(struct parser *p, struct name *name)
{
    const struct token *t = parser_peek(p);

    if (t->kind == TOKEN_ALLOC || t->kind == TOKEN_FREE) {
        parser_next(p);
        name->text = p->source + t->offset;
        name->length = t->length;
        return true;
    }
    return parser_expect_name(p, name);
}

/* DESIGN: `union` names a function of a struct or class and follows
   `.` as well, so that a set has `union(o)`. The declaration of a union
   type stands at module level, before a name, and never after `fn` or
   `.`. An `inject` field keeps to `alloc` and `free`. */
bool parser_expect_function_name(struct parser *p, struct name *name)
{
    const struct token *t = parser_peek(p);

    if (t->kind == TOKEN_UNION) {
        parser_next(p);
        name->text = p->source + t->offset;
        name->length = t->length;
        return true;
    }
    return parser_expect_member_name(p, name);
}

void *parser_node(struct parser *p, size_t size)
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

/* DESIGN: the grammar rules never see a doc comment. Items, fields and
   the module ask for the comments that precede them, so a comment in
   any other place is dropped. Copies the tokens of all but the doc
   comments into kept, with the index in all of each into origin, and
   returns how many it copied. The last token of all is TOKEN_EOF, which
   is kept. */
size_t parser_keep_tokens(const struct token *all, size_t count,
                          struct token *kept, size_t *origin)
{
    size_t n = 0;
    size_t i;

    for (i = 0; i < count; i++) {
        if (!is_doc(all[i].kind)) {
            origin[n] = i;
            kept[n++] = all[i];
        }
    }
    return n;
}

/* Record each doc comment of the count tokens of p->all that no item or
   field took. */
void parser_drop_untaken(struct parser *p, size_t count)
{
    size_t i;

    for (i = 0; i < count; i++) {
        const struct token *t = &p->all[i];
        struct dropped_doc d;
        if (!is_doc(t->kind) || p->taken[i]) {
            continue;
        }
        d.pos = parser_pos_of(t);
        d.marker.text = p->source + t->offset;
        d.marker.length = marker_length(d.marker.text, t->length);
        d.module_form = t->kind == TOKEN_MODULE_DOC ||
                        t->kind == TOKEN_MODULE_NOTE;
        parser_list_push(p->dropped, &d);
    }
}

/* The text of the doc comments of one kind between the previous token and
   the current one. DESIGN: two comments of one kind join with a blank
   line, the paragraph break of the doc markup, so no text is lost. */
struct doc_text parser_doc_before(struct parser *p, enum token_kind kind)
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
        copy = parser_node(p, joined.length + 1);
        memcpy(copy, text_cstr(&joined), joined.length + 1);
        doc.text = copy;
        doc.length = joined.length;
    }
    text_free(&joined);
    return doc;
}

/* Skip to the start of the next statement. That is past a semicolon, or
   up to a closing brace or a keyword that starts a statement. */
void parser_sync_statement(struct parser *p)
{
    p->half = false;
    p->angles = 0;
    for (;;) {
        switch (parser_peek(p)->kind) {
        case TOKEN_SEMICOLON:
            parser_next(p);
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
            parser_next(p);
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
        switch (parser_peek(p)->kind) {
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
        parser_next(p);
    }
}

/* Whether token t is the identifier word. */
bool parser_is_word(const struct parser *p, const struct token *t,
                    const char *word)
{
    return t->kind == TOKEN_IDENT && t->length == strlen(word) &&
           memcmp(p->source + t->offset, word, t->length) == 0;
}

/* DESIGN: a clause of the whole file stands at the top of the module,
   among the imports and before the first item, and ends with `;`. It
   covers every line of the file. */
static bool file_clause(struct parser *p)
{
    struct clause *c;

    if (!parser_read_clause(p, CLAUSE_FILE)) {
        return false;
    }
    c = (struct clause *)p->clauses->data + (p->clauses->count - 1);
    c->from.line = 1;
    c->from.column = 1;
    c->to.line = INT_MAX;
    c->to.column = INT_MAX;
    return parser_expect(p, TOKEN_SEMICOLON);
}

/* After an error in an import, skip the rest of its line up to and with
   its `;`. An import holds no braces, and a keyword in its path would
   stop sync_item inside it. */
static void sync_import(struct parser *p, int line)
{
    while (!parser_check(p, TOKEN_EOF) && parser_peek(p)->line == line) {
        if (parser_next(p)->kind == TOKEN_SEMICOLON) {
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
        if (!parser_expect_name(p, &segment)) {
            text_free(&path);
            return false;
        }
        text_appendf(&path, "%s%.*s", path.length > 0 ? "." : "",
                     (int)segment.length, segment.text);
    } while (parser_peek_at(p, 1)->kind != TOKEN_LBRACE &&
             parser_accept(p, TOKEN_DOT));
    copy = parser_node(p, path.length + 1);
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

    if (!parser_check(p, TOKEN_DOT) ||
        parser_peek_at(p, 1)->kind != TOKEN_LBRACE) {
        return true;
    }
    parser_next(p);
    parser_next(p);
    if (parser_check(p, TOKEN_RBRACE)) {
        parser_error_here(p, "a direct import lists at least one name");
        return false;
    }
    do {
        struct import_name n;
        n.pos = parser_pos_of(parser_peek(p));
        if (!parser_expect_name(p, &n.name)) {
            free(names.data);
            return false;
        }
        parser_list_push(&names, &n);
    } while (parser_accept(p, TOKEN_COMMA));
    imp->names = parser_list_finish(p, &names, &imp->name_count);
    return parser_expect(p, TOKEN_RBRACE);
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
        char *full = parser_node(p, length + 1);
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
    parser_list_push(items, &it);
}

/* DESIGN: `tests { }` and `fixtures { }` hold functions and nothing else.
   Each one becomes an ordinary module function carrying the block it was
   written in, so every pass after this one reads a module of functions.
   The build that is not `anti test` drops them before checking, which is
   how a dev build, a release build and a `.antl` contain none of it. */
static bool test_block(struct parser *p, struct list *items,
                       enum fn_block which)
{
    parser_next(p);
    if (!parser_expect(p, TOKEN_LBRACE)) {
        return false;
    }
    while (!parser_check(p, TOKEN_RBRACE) && !parser_check(p, TOKEN_EOF)) {
        struct item *it;
        p->block = which;
        it = parser_item(p);
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
        parser_list_push(items, &it);
    }
    return parser_expect(p, TOKEN_RBRACE);
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
    pr.pos = parser_pos_of(parser_peek(p));
    parser_next(p);
    pr.interface_pos = parser_pos_of(parser_peek(p));
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
    if (!parser_expect(p, TOKEN_AS)) {
        return false;
    }
    pr.class_pos = parser_pos_of(parser_peek(p));
    if (!parser_expect_name(p, &pr.class_name) ||
        !parser_expect(p, TOKEN_SEMICOLON)) {
        return false;
    }
    parser_list_push(out, &pr);
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
    line.pos = parser_pos_of(parser_peek(p));
    parser_next(p);
    parser_next(p);
    t = parser_peek(p);
    if (t->kind != TOKEN_STRING) {
        parser_error_here(p, "%s",
                          strcmp(kind, "linux") == 0
                       ? "`link linux` takes the name of a library as a "
                         "string"
                       : "`link framework` takes the name of a framework "
                         "as a string");
        return false;
    }
    parser_next(p);
    line.name.text = t->value.text.bytes;
    line.name.length = t->value.text.length;
    if (!parser_expect(p, TOKEN_SEMICOLON)) {
        return false;
    }
    parser_list_push(out, &line);
    return true;
}

bool parser_parse(const char *source, const struct token_list *tokens,
                  struct arena *arena, struct diagnostics *diags,
                  struct module **out)
{
    struct list clauses = {NULL, 0, 0, sizeof(struct clause)};
    struct list dropped = {NULL, 0, 0, sizeof(struct dropped_doc)};
    struct parser p = {source, NULL, tokens->items, NULL, NULL, 0, 0, arena,
                       diags, false, true, false, 0, 0, &clauses, &dropped,
                       BLOCK_NONE, false, 0};
    struct module *m = arena_alloc(arena, sizeof *m);
    struct list imports = {NULL, 0, 0, sizeof(struct import)};
    struct list items = {NULL, 0, 0, sizeof(struct item *)};
    struct list provides = {NULL, 0, 0, sizeof(struct provides)};
    struct list frameworks = {NULL, 0, 0, sizeof(struct link_name)};
    struct list linux_libraries = {NULL, 0, 0, sizeof(struct link_name)};
    struct token *kept = alloc_zeroed(tokens->count, sizeof *kept);
    size_t *origin = alloc_zeroed(tokens->count, sizeof *origin);
    bool *taken = alloc_zeroed(tokens->count, sizeof *taken);
    size_t count;
    bool saw_tests = false;
    bool saw_fixtures = false;

    count = parser_keep_tokens(tokens->items, tokens->count, kept, origin);
    p.tokens = kept;
    p.last = count - 1;
    p.origin = origin;
    p.taken = taken;
    m->doc = parser_doc_before(&p, TOKEN_MODULE_DOC);
    m->note = parser_doc_before(&p, TOKEN_MODULE_NOTE);

    while (parser_check(&p, TOKEN_IMPORT) || parser_clause_ahead(&p, 0)) {
        struct import imp;
        if (parser_clause_ahead(&p, 0)) {
            if (!file_clause(&p)) {
                sync_import(&p, parser_peek(&p)->line);
            }
            continue;
        }
        memset(&imp, 0, sizeof imp);
        imp.pos = parser_pos_of(parser_next(&p));
        imp.module_pos = parser_pos_of(parser_peek(&p));
        if (module_path(&p, &imp.module) && import_names(&p, &imp) &&
            (imp.name_count > 0 || !parser_accept(&p, TOKEN_AS) ||
             parser_expect_name(&p, &imp.alias)) &&
            parser_expect(&p, TOKEN_SEMICOLON)) {
            parser_list_push(&imports, &imp);
        } else {
            sync_import(&p, imp.pos.line);
        }
    }
    while (!parser_check(&p, TOKEN_EOF)) {
        size_t before = p.pos;
        struct item *it;
        if (parser_check(&p, TOKEN_TESTS) || parser_check(&p, TOKEN_FIXTURES)) {
            bool is_tests = parser_check(&p, TOKEN_TESTS);
            bool *seen = is_tests ? &saw_tests : &saw_fixtures;
            if (*seen) {
                diagnostics_add(diags, parser_peek(&p)->line,
                                parser_peek(&p)->column,
                                "a module has at most one `%s` block",
                                is_tests ? "tests" : "fixtures");
                p.ok = false;
            }
            *seen = true;
            if (!test_block(&p, &items,
                            is_tests ? BLOCK_TESTS : BLOCK_FIXTURES)) {
                if (p.pos == before) {
                    parser_next(&p);
                }
                sync_item(&p);
            }
            continue;
        }
        if (parser_clause_ahead(&p, 0)) {
            parser_error_here(&p, "a clause of the whole file stands at the "
                                  "top of the module, and one of a "
                                  "declaration last in its header");
            parser_next(&p);
            sync_item(&p);
            continue;
        }
        if (parser_check(&p, TOKEN_PROVIDES)) {
            if (!provides_line(&p, &provides)) {
                if (p.pos == before) {
                    parser_next(&p);
                }
                sync_item(&p);
            }
            continue;
        }
        if (parser_is_word(&p, parser_peek(&p), "link") &&
            parser_is_word(&p, parser_peek_at(&p, 1), "framework")) {
            if (!link_line(&p, &frameworks, "framework")) {
                if (p.pos == before) {
                    parser_next(&p);
                }
                sync_item(&p);
            }
            continue;
        }
        if (parser_is_word(&p, parser_peek(&p), "link") &&
            parser_is_word(&p, parser_peek_at(&p, 1), "linux")) {
            if (!link_line(&p, &linux_libraries, "linux")) {
                if (p.pos == before) {
                    parser_next(&p);
                }
                sync_item(&p);
            }
            continue;
        }
        it = parser_item(&p);
        if (it != NULL) {
            push_item(&p, &items, it);
        } else {
            if (p.pos == before) {
                parser_next(&p);
            }
            sync_item(&p);
        }
    }
    m->imports = parser_list_finish(&p, &imports, &m->import_count);
    m->items = parser_list_finish(&p, &items, &m->item_count);
    m->provides = parser_list_finish(&p, &provides, &m->provides_count);
    m->frameworks = parser_list_finish(&p, &frameworks, &m->framework_count);
    m->linux_libraries = parser_list_finish(&p, &linux_libraries,
                                            &m->linux_library_count);
    parser_drop_untaken(&p, tokens->count);
    m->dropped = parser_list_finish(&p, &dropped, &m->dropped_count);
    m->clauses = parser_list_finish(&p, &clauses, &m->clause_count);
    free(kept);
    free(origin);
    free(taken);
    *out = m;
    return p.ok;
}

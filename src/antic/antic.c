/* The side of antic behind antic.h: the walks over a source and a library
   file that the commands of anti ask for. The page of `anti doc` is
   docpage.c. */
#include "antic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "antl.h"
#include "antl_io.h"
#include "ast.h"
#include "diagnostic.h"
#include "notice.h"
#include "parser.h"

bool antic_library_header(const uint8_t *data, size_t size,
                          struct arena *arena, struct package *package,
                          const char **module, char *error,
                          size_t error_size)
{
    struct interface iface;

    memset(&iface, 0, sizeof iface);
    if (!antl_header(data, size, arena, &iface, error, error_size)) {
        return false;
    }
    *package = iface.package;
    *module = iface.module;
    return true;
}

void antic_notice_lines(struct text *out,
                        const struct package *const *packages, size_t count)
{
    struct text marked = {0};
    size_t begin = sizeof ANTI_NOTICE_BEGIN - 1;
    size_t end = sizeof ANTI_NOTICE_END - 1;

    notice_text(&marked, packages, count);
    text_append_bytes(out, marked.data + begin, marked.length - begin - end);
    text_free(&marked);
}

void antic_component_license(const char *component, struct text *out)
{
    notice_component_license(component, out);
}

bool antic_library_starts(const uint8_t *data, size_t size)
{
    return size >= sizeof antl_magic &&
           memcmp(data, antl_magic, sizeof antl_magic) == 0;
}

bool antic_tokens(const char *source, size_t length, struct arena *arena,
                  struct token_list *out)
{
    struct diagnostics diags = {0};
    bool ok = lexer_lex(source, length, arena, &diags, out);

    diagnostics_free(&diags);
    return ok;
}

bool antic_is_identifier(const char *name)
{
    struct arena arena = {0};
    struct token_list tokens = {0};
    bool word = antic_tokens(name, strlen(name), &arena, &tokens) &&
                tokens.count >= 1 && tokens.items[0].kind == TOKEN_IDENT &&
                (tokens.count == 1 || tokens.items[1].kind == TOKEN_EOF);

    lexer_token_list_free(&tokens);
    arena_free(&arena);
    return word;
}

static bool named(const struct name *a, const char *text)
{
    size_t n = strlen(text);

    return a->length == n && memcmp(a->text, text, n) == 0;
}

/* A copy of the name in the memory pool, ending in a zero byte. */
static const char *pool_name(struct arena *arena, const struct name *name)
{
    char *copy = arena_alloc(arena, alloc_sum(name->length, 1));

    memcpy(copy, name->text, name->length);
    copy[name->length] = '\0';
    return copy;
}

/* The comments of an outline grow in an array of their own, which the
   memory pool takes when the walk ends. */
struct comment_list {
    struct antic_doc_comment *items;
    size_t count;
    size_t capacity;
};

static void add_comment(struct comment_list *list, const struct doc_text *doc,
                        bool dev, const char *owner)
{
    struct antic_doc_comment *one;

    if (doc->text == NULL || doc->length == 0) {
        return;
    }
    list->items = alloc_grow(list->items, &list->capacity, list->count + 1,
                             sizeof *list->items);
    one = &list->items[list->count++];
    one->text = doc->text;
    one->length = doc->length;
    one->line = doc->line;
    one->dev = dev;
    one->owner = owner;
}

static void item_comments(struct comment_list *list, const struct item *it,
                          struct arena *arena)
{
    const char *owner = pool_name(arena, &it->name);
    size_t i;

    add_comment(list, &it->doc, false, owner);
    add_comment(list, &it->note, true, owner);
    for (i = 0; i < it->param_count; i++) {
        const char *param = pool_name(arena, &it->params[i].name);
        add_comment(list, &it->params[i].doc, false, param);
        add_comment(list, &it->params[i].note, true, param);
    }
    for (i = 0; i < it->case_count; i++) {
        add_comment(list, &it->cases[i].doc, false,
                    pool_name(arena, &it->cases[i].name));
    }
    for (i = 0; i < it->member_count; i++) {
        item_comments(list, it->members[i], arena);
    }
}

/* The functions of the blocks of a module: `fn main`, the tests of its
   `tests` block and the setup and teardown of its `fixtures` block. */
static void outline_functions(const struct module *tree, struct arena *arena,
                              struct antic_outline *out)
{
    const char **tests =
        arena_alloc(arena, alloc_product(tree->item_count + 1, sizeof *tests));
    size_t i;

    for (i = 0; i < tree->item_count; i++) {
        const struct item *it = tree->items[i];
        if (it->kind != ITEM_FN) {
            continue;
        }
        if (it->block == BLOCK_NONE) {
            out->has_main = out->has_main || named(&it->name, "main");
        } else if (it->block == BLOCK_FIXTURES) {
            out->setup = out->setup || named(&it->name, "setup");
            out->teardown = out->teardown || named(&it->name, "teardown");
        } else {
            tests[out->test_count++] = pool_name(arena, &it->name);
        }
    }
    out->tests = tests;
}

bool antic_outline(const char *path, const char *source, size_t length,
                   bool report, struct arena *arena,
                   struct antic_outline *out)
{
    struct diagnostics diags = {0};
    struct token_list tokens = {0};
    struct comment_list comments = {0};
    struct module *tree = NULL;
    const char **imports;
    size_t i;
    bool ok = false;

    memset(out, 0, sizeof *out);
    if (!lexer_lex(source, length, arena, &diags, &tokens) ||
        !parser_parse(source, &tokens, arena, &diags, &tree)) {
        for (i = 0; report && i < diags.count; i++) {
            fprintf(stderr, "%s:%d:%d: error: %s\n", path,
                    diags.items[i].line, diags.items[i].column,
                    diags.items[i].message);
        }
        goto done;
    }
    outline_functions(tree, arena, out);
    imports = arena_alloc(arena, alloc_product(tree->import_count + 1,
                                               sizeof *imports));
    for (i = 0; i < tree->import_count; i++) {
        imports[i] = pool_name(arena, &tree->imports[i].module);
    }
    out->imports = imports;
    out->import_count = tree->import_count;
    add_comment(&comments, &tree->doc, false, NULL);
    add_comment(&comments, &tree->note, true, NULL);
    for (i = 0; i < tree->item_count; i++) {
        item_comments(&comments, tree->items[i], arena);
    }
    if (comments.count > 0) {
        size_t bytes = alloc_product(comments.count, sizeof *comments.items);
        out->comments = arena_alloc(arena, bytes);
        memcpy(out->comments, comments.items, bytes);
        out->comment_count = comments.count;
    }
    ok = true;
done:
    free(comments.items);
    lexer_token_list_free(&tokens);
    diagnostics_free(&diags);
    return ok;
}

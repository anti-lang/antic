/* The walks of src/antic/antic.h, which the commands of anti call in
   place of the lexer, the parser and the library reader, and the doc
   page of src/antic/docpage.c that `anti doc` renders. */

#include "../binary_stdio.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../src/antic/platform.h"
#include "antic.h"
#include "check.h"
#include "cpu.h"
#include "driver.h"
#include "target.h"

static void identifiers(void)
{
    CHECK(antic_is_identifier("count"));
    CHECK(antic_is_identifier("_x9"));
    CHECK(!antic_is_identifier("fn"));
    CHECK(!antic_is_identifier("int"));
    CHECK(!antic_is_identifier("null"));
    CHECK(!antic_is_identifier("a b"));
    CHECK(!antic_is_identifier("9a"));
    CHECK(!antic_is_identifier(""));
}

static void builtin_types(void)
{
    CHECK(lexer_token_is_builtin_type(TOKEN_BOOL_TYPE));
    CHECK(lexer_token_is_builtin_type(TOKEN_INT_TYPE));
    CHECK(lexer_token_is_builtin_type(TOKEN_F16));
    CHECK(lexer_token_is_builtin_type(TOKEN_C_WCHAR));
    CHECK(!lexer_token_is_builtin_type(TOKEN_SIZE_OF));
    CHECK(!lexer_token_is_builtin_type(TOKEN_WORKER));
    CHECK(!lexer_token_is_builtin_type(TOKEN_IDENT));
}

static const char outline_source[] =
    "//! Guide.\n"
    "//#! Notes.\n"
    "import geometry;\n"
    "import anti.text;\n"
    "/// Adds.\n"
    "//# Fast.\n"
    "pub fn f(a: int) {}\n"
    "/// A point.\n"
    "struct P {\n"
    "    /// Across.\n"
    "    x: int,\n"
    "}\n"
    "fn main() -> int { return 0; }\n"
    "fixtures {\n"
    "    fn setup() {}\n"
    "}\n"
    "tests {\n"
    "    fn adds() {}\n"
    "    fn subtracts() {}\n"
    "}\n";

/* The comment at index, its text and its owner, NULL for the module. */
static void comment_is(const struct antic_outline *o, size_t index,
                       const char *text, bool dev, const char *owner)
{
    const struct antic_doc_comment *c;

    CHECK(index < o->comment_count);
    if (index >= o->comment_count) {
        return;
    }
    c = &o->comments[index];
    CHECK(c->length == strlen(text) && memcmp(c->text, text, c->length) == 0);
    CHECK(c->dev == dev);
    if (owner == NULL) {
        CHECK(c->owner == NULL);
    } else {
        CHECK(c->owner != NULL && strcmp(c->owner, owner) == 0);
    }
}

static void outlines(void)
{
    struct arena arena = {0};
    struct antic_outline o;
    const char broken[] = "fn f( {";

    CHECK(antic_outline("m.anti", outline_source, sizeof outline_source - 1,
                        false, &arena, &o));
    CHECK(o.import_count == 2);
    if (o.import_count == 2) {
        CHECK_STR(o.imports[0], "geometry");
        CHECK_STR(o.imports[1], "anti.text");
    }
    CHECK(o.has_main);
    CHECK(o.setup);
    CHECK(!o.teardown);
    CHECK(o.test_count == 2);
    if (o.test_count == 2) {
        CHECK_STR(o.tests[0], "adds");
        CHECK_STR(o.tests[1], "subtracts");
    }
    CHECK(o.comment_count == 6);
    comment_is(&o, 0, "Guide.", false, NULL);
    comment_is(&o, 1, "Notes.", true, NULL);
    comment_is(&o, 2, "Adds.", false, "f");
    comment_is(&o, 3, "Fast.", true, "f");
    comment_is(&o, 4, "A point.", false, "P");
    comment_is(&o, 5, "Across.", false, "x");
    arena_free(&arena);

    /* A source the parser refuses has no outline. */
    CHECK(!antic_outline("m.anti", broken, sizeof broken - 1, false, &arena,
                         &o));
    arena_free(&arena);
}

/* Bytes that are no library file are refused with a message, whether
   empty, cut short or of another kind. */
static void library_headers(void)
{
    static const uint8_t junk[] = { 'A', 'N', 'T', 'L', 0xff, 0xff, 0xff };
    static const uint8_t none[] = { 0 };
    struct arena arena = {0};
    struct package package;
    const char *module = NULL;
    char error[200];

    error[0] = '\0';
    CHECK(!antic_library_header(none, 0, &arena, &package, &module, error,
                                sizeof error));
    CHECK(error[0] != '\0');
    error[0] = '\0';
    CHECK(!antic_library_header(junk, sizeof junk, &arena, &package, &module,
                                error, sizeof error));
    CHECK(error[0] != '\0');
    arena_free(&arena);
}

/* The page of a source: the doc text of the module and of each public
   item, and the private items only where the page asks for them. */
static void doc_pages(void)
{
    static const char path[] = "doc_page.anti";
    static const char source[] = "//! The answers.\n"
                                 "/// The answer.\n"
                                 "pub fn answer() -> int { return 42; }\n"
                                 "fn hidden() -> int { return 1; }\n";
    struct options o;
    struct doc_page page = {0};
    FILE *f = platform_open(path, true);

    CHECK(f != NULL);
    if (f == NULL) {
        return;
    }
    CHECK(fwrite(source, 1, sizeof source - 1, f) == sizeof source - 1);
    CHECK(fclose(f) == 0);
    memset(&o, 0, sizeof o);
    o.input = path;
    CHECK(target_host(&o.target));
    o.cpu = cpu_default(o.target);
    CHECK(antic_doc_page(&o, DOC_ITEMS_PUBLIC, &page));
    CHECK_STR(text_cstr(&page.module), "doc_page");
    CHECK_STR(text_cstr(&page.doc), "The answers.");
    CHECK(page.item_count == 1);
    if (page.item_count == 1) {
        CHECK_STR(text_cstr(&page.items[0].name), "answer");
        CHECK_STR(text_cstr(&page.items[0].doc), "The answer.");
    }
    antic_doc_page_free(&page);
    CHECK(page.item_count == 0);
    CHECK(antic_doc_page(&o, DOC_ITEMS_PRIVATE, &page));
    CHECK(page.item_count == 2);
    antic_doc_page_free(&page);
}

void test_interface(void)
{
    identifiers();
    builtin_types();
    outlines();
    library_headers();
    doc_pages();
}

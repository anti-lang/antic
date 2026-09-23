/* The TOML subset of src/rt/toml.c, and the inline tables that `anti.toml`
   writes a dependency and a version of an index with. */
#include <string.h>

#include "../binary_stdio.h"
#include "check.h"
#include "toml.h"

/* Whether the document holds key at all. */
static int has_key(const struct anti_toml *doc, const char *key)
{
    return anti_rt_toml_find(doc, (const unsigned char *)key,
                             (int64_t)strlen(key)) >= 0;
}

/* The value of key in the document. A key the document does not hold
   gives this text, which no check expects, so a missing key reports
   rather than ends the run. */
static const char *value_of(const struct anti_toml *doc, const char *key)
{
    int64_t at = anti_rt_toml_find(doc, (const unsigned char *)key,
                                   (int64_t)strlen(key));
    struct anti_text value;

    if (at < 0) {
        return "<no such key>";
    }
    value = anti_rt_toml_value(doc, at);
    return value.ptr == NULL ? "" : (const char *)value.ptr;
}

static struct anti_toml *read(const char *text)
{
    return anti_rt_toml_read((const unsigned char *)text,
                             (int64_t)strlen(text));
}

/* An inline table writes one key per pair under the path of the table. */
static void inline_table(void)
{
    static const char text[] =
        "[dependencies]\n"
        "\"com.example.tree\" = { version = \"1.2.4\", repo = \"ff\" }\n"
        "\"com.example.matrix\" = { path = \"../libs\" }\n";
    struct anti_toml *doc = read(text);

    CHECK(doc != NULL);
    if (doc == NULL) {
        return;
    }
    CHECK_STR(value_of(doc, "dependencies.com.example.tree.version"), "1.2.4");
    CHECK_STR(value_of(doc, "dependencies.com.example.tree.repo"), "ff");
    CHECK_STR(value_of(doc, "dependencies.com.example.matrix.path"),
              "../libs");
    CHECK(!has_key(doc, "dependencies.com.example.matrix.version"));
    anti_rt_toml_free(doc);
}

/* An array of inline tables numbers its elements, as an array does, so
   the modules of a version of an index stand at `.0.` upwards. */
static void inline_table_array(void)
{
    static const char text[] =
        "[[version]]\n"
        "version = \"1.2.4\"\n"
        "modules = [\n"
        "    { path = \"com.example.tree\", sha256 = \"aa\" },\n"
        "    { path = \"com.example.tree.balance\", sha256 = \"bb\" },\n"
        "]\n"
        "dependencies = [{ name = \"com.example.geometry\", "
        "version = \"2.0\", repo = \"https://example.com/repo\" }]\n";
    struct anti_toml *doc = read(text);

    CHECK(doc != NULL);
    if (doc == NULL) {
        return;
    }
    CHECK_STR(value_of(doc, "version.0.version"), "1.2.4");
    CHECK_STR(value_of(doc, "version.0.modules.0.path"), "com.example.tree");
    CHECK_STR(value_of(doc, "version.0.modules.0.sha256"), "aa");
    CHECK_STR(value_of(doc, "version.0.modules.1.path"),
              "com.example.tree.balance");
    CHECK_STR(value_of(doc, "version.0.modules.1.sha256"), "bb");
    CHECK_STR(value_of(doc, "version.0.dependencies.0.name"),
              "com.example.geometry");
    CHECK_STR(value_of(doc, "version.0.dependencies.0.repo"),
              "https://example.com/repo");
    anti_rt_toml_free(doc);
}

/* An inline table nests, and a comment and a line break inside one are
   read as they are outside it. */
static void inline_table_nested(void)
{
    static const char text[] =
        "a = { b = { c = 1 }, d = [2, 3] }   # a note\n"
        "e = true\n";
    struct anti_toml *doc = read(text);

    CHECK(doc != NULL);
    if (doc == NULL) {
        return;
    }
    CHECK_STR(value_of(doc, "a.b.c"), "1");
    CHECK_STR(value_of(doc, "a.d.0"), "2");
    CHECK_STR(value_of(doc, "a.d.1"), "3");
    CHECK_STR(value_of(doc, "e"), "true");
    anti_rt_toml_free(doc);
}

/* An inline table that no `}` closes is no document of the subset. */
static void inline_table_unclosed(void)
{
    struct anti_toml *doc = read("a = { b = 1\n");

    CHECK(doc == NULL);
    anti_rt_toml_free(doc);
}

/* The count of a `[[name]]` table follows the largest number under that
   name. A number at the limit of `int64_t` leaves no next count, so the
   document is refused. */
static void table_repeat_limit(void)
{
    struct anti_toml *doc = read("[[t]]\nx = 1\n[[t]]\nx = 2\n");

    CHECK(doc != NULL);
    if (doc != NULL) {
        CHECK_STR(value_of(doc, "t.0.x"), "1");
        CHECK_STR(value_of(doc, "t.1.x"), "2");
    }
    anti_rt_toml_free(doc);

    doc = read("[a]\n9223372036854775806 = 1\n[[a]]\nx = 2\n");
    CHECK(doc != NULL);
    if (doc != NULL) {
        CHECK_STR(value_of(doc, "a.9223372036854775807.x"), "2");
    }
    anti_rt_toml_free(doc);

    doc = read("[a]\n9223372036854775807 = 1\n[[a]]\n");
    CHECK(doc == NULL);
    anti_rt_toml_free(doc);
    doc = read("[a]\n99999999999999999999 = 1\n[[a]]\n");
    CHECK(doc == NULL);
    anti_rt_toml_free(doc);
}

/* A key is at most 319 bytes, the room of a path less its NUL. The
   bound holds bare or in quotes, and at the top level or inside an
   inline table. */
static void key_length(void)
{
    char text[1200];
    struct anti_toml *doc;
    size_t n;

    memset(text, 'k', 319);
    memcpy(text + 319, " = 1\n", 6);
    doc = read(text);
    CHECK(doc != NULL && anti_rt_toml_count(doc) == 1);
    anti_rt_toml_free(doc);

    memset(text, 'k', 320);
    memcpy(text + 320, " = 1\n", 6);
    doc = read(text);
    CHECK(doc == NULL);
    anti_rt_toml_free(doc);

    text[0] = '"';
    memset(text + 1, 'k', 1000);
    memcpy(text + 1001, "\" = 1\n", 7);
    doc = read(text);
    CHECK(doc == NULL);
    anti_rt_toml_free(doc);

    memcpy(text, "a = { ", 6);
    memset(text + 6, 'k', 1000);
    n = 1006;
    memcpy(text + n, " = 1 }\n", 8);
    doc = read(text);
    CHECK(doc == NULL);
    anti_rt_toml_free(doc);
}

void test_toml(void)
{
    inline_table();
    inline_table_array();
    inline_table_nested();
    inline_table_unclosed();
    table_repeat_limit();
    key_length();
}

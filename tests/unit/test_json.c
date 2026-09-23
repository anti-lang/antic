/* The JSON scanner of src/rt/json.c on malformed input. Every case must end
   in false, with the position inside the input, and never read past it.
   The input of each case is a heap copy without a NUL, so AddressSanitizer
   reports a read past the end. */
#include <stdlib.h>
#include <string.h>

#include "../binary_stdio.h"
#include "check.h"
#include "json.h"

/* The text as bytes of their own, with nothing after the last. */
static unsigned char *copy(const char *text, size_t *length)
{
    unsigned char *bytes;

    *length = strlen(text);
    bytes = malloc(*length > 0 ? *length : 1);
    if (bytes != NULL) {
        memcpy(bytes, text, *length);
    }
    return bytes;
}

/* Whether skipping one value of text succeeds and stays inside it. */
static int skips(const char *text)
{
    size_t length;
    unsigned char *bytes = copy(text, &length);
    struct anti_json s;
    int ok;

    if (bytes == NULL) {
        return -1;
    }
    s.at = bytes;
    s.end = bytes + length;
    ok = anti_rt_json_skip(&s, 0);
    CHECK(s.at >= bytes && s.at <= bytes + length);
    free(bytes);
    return ok;
}

/* Whether text reads as one string into a buffer of room bytes. */
static int reads_string(const char *text, size_t room)
{
    size_t length;
    unsigned char *bytes = copy(text, &length);
    unsigned char out[16];
    size_t decoded;
    struct anti_json s;
    int ok;

    if (bytes == NULL) {
        return -1;
    }
    s.at = bytes;
    s.end = bytes + length;
    ok = anti_rt_json_string(&s, out, room, &decoded);
    CHECK(s.at >= bytes && s.at <= bytes + length);
    CHECK(decoded <= room || !ok);
    free(bytes);
    return ok;
}

static int integer(const char *text, int64_t *value)
{
    return anti_rt_json_integer((const unsigned char *)text,
                                (int64_t)strlen(text), value);
}

/* A value cut short anywhere is refused. */
static void truncated(void)
{
    static const char whole[] = "{\"a\":[1,2,{\"b\":\"c\\u00e9\"}],\"d\":true}";
    size_t n;

    CHECK(skips(whole) == 1);
    for (n = 0; n + 1 < sizeof whole - 1; n++) {
        char cut[sizeof whole];
        memcpy(cut, whole, n);
        cut[n] = '\0';
        CHECK(skips(cut) == 0);
    }
    CHECK(skips("tru") == 0);
    CHECK(skips("nul") == 0);
    CHECK(skips("{\"a\"") == 0);
    CHECK(skips("{\"a\":") == 0);
    CHECK(skips("[1,") == 0);
}

static void unterminated_strings(void)
{
    CHECK(reads_string("\"abc", 16) == 0);
    CHECK(reads_string("\"abc\\", 16) == 0);
    CHECK(reads_string("\"abc\\\"", 16) == 0);
    CHECK(reads_string("\"", 16) == 0);
    CHECK(reads_string("\"a\nb\"", 16) == 0);
    CHECK(skips("[\"abc]") == 0);
}

static void bad_escapes(void)
{
    CHECK(reads_string("\"\\x41\"", 16) == 0);
    CHECK(reads_string("\"\\u12\"", 16) == 0);
    CHECK(reads_string("\"\\u12", 16) == 0);
    CHECK(reads_string("\"\\u00g0\"", 16) == 0);
    CHECK(reads_string("\"\\ud800\"", 16) == 0);
    CHECK(reads_string("\"\\udfff\"", 16) == 0);
    CHECK(reads_string("\"\\u00e9\"", 16) == 1);
    /* Two bytes of UTF-8 do not fit in a room of one. */
    CHECK(reads_string("\"\\u00e9\"", 1) == 0);
    CHECK(reads_string("\"abcd\"", 3) == 0);
}

/* levels arrays, one inside the other, as a C string in text. */
static void nest(char *text, size_t levels)
{
    size_t i;

    for (i = 0; i < levels; i++) {
        text[i] = '[';
        text[levels + i] = ']';
    }
    text[2 * levels] = '\0';
}

/* Nesting past the limit is refused, however deep, without running out
   of stack. */
static void deep_nesting(void)
{
    char text[4 * ANTI_JSON_DEPTH + 8];
    char *big;
    size_t i;
    size_t n = 1000000;

    /* A value at depth 0 may hold values down to ANTI_JSON_DEPTH, which
       is ANTI_JSON_DEPTH + 1 levels of arrays. */
    nest(text, ANTI_JSON_DEPTH + 1);
    CHECK(skips(text) == 1);
    nest(text, ANTI_JSON_DEPTH + 2);
    CHECK(skips(text) == 0);
    big = malloc(n + 1);
    if (big != NULL) {
        memset(big, '[', n);
        big[n] = '\0';
        CHECK(skips(big) == 0);
        for (i = 0; i < n; i += 2) {
            memcpy(big + i, "{\"", 2);
        }
        CHECK(skips(big) == 0);
        free(big);
    }
}

static void numbers(void)
{
    int64_t value = 0;

    CHECK(integer("9223372036854775807", &value) == 1);
    CHECK(value == INT64_MAX);
    CHECK(integer("-9223372036854775808", &value) == 1);
    CHECK(value == INT64_MIN);
    CHECK(integer("9223372036854775808", &value) == 0);
    CHECK(integer("-9223372036854775809", &value) == 0);
    CHECK(integer("99999999999999999999999", &value) == 0);
    CHECK(integer("1.5", &value) == 0);
    CHECK(integer("1e3", &value) == 0);
    CHECK(integer("-", &value) == 0);
    CHECK(integer("01", &value) == 0);
    CHECK(integer("+1", &value) == 0);
    CHECK(integer("", &value) == 0);
    CHECK(anti_rt_json_valid_number((const unsigned char *)"-0.5e+10", 8));
    CHECK(!anti_rt_json_valid_number((const unsigned char *)"1.", 2));
    CHECK(!anti_rt_json_valid_number((const unsigned char *)".5", 2));
    CHECK(!anti_rt_json_valid_number((const unsigned char *)"1e", 2));
    CHECK(!anti_rt_json_valid_number((const unsigned char *)"1-2", 3));
    CHECK(!anti_rt_json_valid_number(NULL, 0));
}

static void garbage(void)
{
    CHECK(skips("") == 0);
    CHECK(skips("   ") == 0);
    CHECK(skips("}") == 0);
    CHECK(skips("{1:2}") == 0);
    CHECK(skips("{\"a\" 1}") == 0);
    CHECK(skips("[1 2]") == 0);
    CHECK(skips("[,]") == 0);
    CHECK(skips("@") == 0);
}

void test_json(void)
{
    truncated();
    unterminated_strings();
    bad_escapes();
    deep_nesting();
    numbers();
    garbage();
}

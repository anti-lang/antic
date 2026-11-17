/* The rewriter of a byte pattern of src/rt/regex.c on every prefix of one
   pattern, and the lookup of a group by a name that holds a NUL. Each
   prefix is a heap copy without a NUL, so AddressSanitizer reports a read
   past its end. The message of a refusal of a byte pattern in a buffer
   shorter than it. */

#include <stdlib.h>
#include <string.h>

#include "../binary_stdio.h"
#include "check.h"
#include "alloc.h"
#include "regex.h"

/* A byte pattern with every construct the rewriter reads ahead in: a
   group name, `\Q...\E` outside a class and inside one, a class with a
   character outside ASCII, an escaped one, a set, a POSIX class, the
   escapes `\x`, `\x{}`, `\c` and an octal one, a range, a comment and
   characters outside ASCII under a quantifier. Cut anywhere, it holds a
   class without its `]`, a `\Q` without `\E`, a `\` at the end and a
   UTF-8 sequence cut off. */
static const char every_construct[] =
    "(?<n>a)\\Q\xC3\xA9.\\E[a\xC3\xA9\\\xC3\xB3\\d[:alpha:]\\x41\\x{42}"
    "\\cA\\101\\Q]\xC3\xB3\\E0-9]\\\xC3\xA9+\xC3\xA9*\\cB(?#note)[a-z]x";

/* Compile the first length bytes of text as a byte pattern, and check
   that a refusal names a byte inside them. Returns whether it compiled. */
static int compiles(const char *text, size_t length)
{
    unsigned char *bytes = alloc_zeroed(length > 0 ? length : 1, 1);
    int32_t code = 0;
    int64_t offset = -1;
    void *compiled;

    memcpy(bytes, text, length);
    compiled = anti_rt_regex_compile_bytes(bytes, (int64_t)length, &code,
                                           &offset);
    if (compiled == NULL) {
        CHECK(code != 0);
        CHECK(offset >= 0 && offset <= (int64_t)length);
    }
    anti_rt_regex_free(compiled);
    free(bytes);
    return compiled != NULL;
}

static void every_prefix(void)
{
    size_t length = strlen(every_construct);
    size_t cut;

    CHECK(compiles(every_construct, length) == 1);
    for (cut = 0; cut < length; cut++) {
        (void)compiles(every_construct, cut);
    }
    CHECK(compiles("[a\xC3", 4) == 0);
    CHECK(compiles("\\Q\xC3\xA9", 4) == 1);
    CHECK(compiles("a\\", 2) == 0);
    CHECK(compiles("\xC3", 1) == 1);
}

/* A group name is the bytes it is given. A NUL inside it cuts nothing. */
static void name_with_a_nul(void)
{
    const unsigned char pattern[] = "(?<a>x)(?<b>y)";
    int32_t code = 0;
    int64_t offset = 0;
    void *compiled = anti_rt_regex_compile(pattern, 14, &code, &offset);

    CHECK(compiled != NULL);
    if (compiled == NULL) {
        return;
    }
    CHECK(anti_rt_regex_group_number(compiled, (const unsigned char *)"a",
                                     1) == 1);
    CHECK(anti_rt_regex_group_number(compiled, (const unsigned char *)"b",
                                     1) == 2);
    CHECK(anti_rt_regex_group_number(compiled,
                                     (const unsigned char *)"a\0b", 3) == -1);
    anti_rt_regex_free(compiled);
}

/* The message of a refusal of a byte pattern comes back cut to the room
   it is given and ended by a NUL, and no room takes no byte. */
static void message_cut(void)
{
    unsigned char out[8];

    memset(out, 'x', sizeof out);
    anti_rt_regex_message_into(ANTI_RT_REGEX_WIDE_NEGATED, out, 0);
    CHECK(out[0] == 'x');
    anti_rt_regex_message_into(ANTI_RT_REGEX_WIDE_NEGATED, out, sizeof out);
    CHECK(strcmp((const char *)out, "a byte ") == 0);
    anti_rt_regex_message_into(ANTI_RT_REGEX_WIDE_RANGE, out, 3);
    CHECK(strcmp((const char *)out, "a ") == 0);
}

/* Compile text as a byte pattern and give the code of its refusal, or 0
   when it compiled. */
static int32_t refusal(const char *text)
{
    unsigned char *bytes = alloc_zeroed(strlen(text) + 1, 1);
    int32_t code = 0;
    int64_t offset = -1;
    void *compiled;

    memcpy(bytes, text, strlen(text));
    compiled = anti_rt_regex_compile_bytes(bytes, (int64_t)strlen(text),
                                           &code, &offset);
    anti_rt_regex_free(compiled);
    free(bytes);
    return compiled != NULL ? 0 : code;
}

/* Every escape of a class that the rewriter reads to its end, beside a
   character outside ASCII, which makes the class a choice. `[[:a]` is
   no POSIX class and holds `[`, `:` and `a`. */
static void class_escapes(void)
{
    static const char *const fine[] = {
        "[\\d\\D\\w\\W\\s\\S\\h\\H\\v\\V\xC3\xA9]",
        "[\\o{101}\\x{41}\\pL\\p{Lu}\xC3\xA9]",
        "[\\x4 \xC3\xA9]", "[\\x41\xC3\xA9]", "[\\.\xC3\xA9]",
        "[\\9\xC3\xA9]", "[\\1a\xC3\xA9]", "[\\1 \xC3\xA9]",
        "[\\101\xC3\xA9]", "[[:alpha:]\xC3\xA9]", "[[x\xC3\xA9]",
        "[]a\xC3\xA9]", "[a-]\xC3\xA9", "[\xC3\xA9]", "[[:a]\xC3\xA9",
        "[\\Qab.\\b\\E\xC3\xA9]", "\\Qab.Z9\\b\\E", "\\Qa\\",
    };
    /* PCRE2 refuses these after the rewrite: \x, \o and \N without what
       follows them, POSIX collating elements and a range up to a set. */
    static const char *const refused[] = {
        "[\\x\xC3\xA9]", "[\\o\xC3\xA9]", "[\\N\xC3\xA9]",
        "[[.a.]\xC3\xA9]", "[[=a=]\xC3\xA9]", "[a-\\d\xC3\xA9]",
        "[[:al", "[[:a:x", "[\\Qa\\",
    };
    size_t i;

    for (i = 0; i < sizeof fine / sizeof fine[0]; i++) {
        CHECK(refusal(fine[i]) == 0);
    }
    for (i = 0; i < sizeof refused / sizeof refused[0]; i++) {
        CHECK(compiles(refused[i], strlen(refused[i])) == 0);
    }
}

/* The first refusal of a class names the class, and a second wide member
   or range after it changes nothing. */
static void class_refusals(void)
{
    CHECK(refusal("[^\\Q\xC3\xA9\xC3\xB3\\E]") == ANTI_RT_REGEX_WIDE_NEGATED);
    CHECK(refusal("[^\xC3\xA9\xC3\xB3]") == ANTI_RT_REGEX_WIDE_NEGATED);
    CHECK(refusal("[^\xC3\xA9" "a-\xC3\xB3]") == ANTI_RT_REGEX_WIDE_NEGATED);
}

/* A group name of no bytes and one past the room of a name name no
   group. */
static void name_lengths(void)
{
    const unsigned char pattern[] = "(?<a>x)";
    unsigned char long_name[300];
    int32_t code = 0;
    int64_t offset = 0;
    void *compiled = anti_rt_regex_compile(pattern, 7, &code, &offset);

    CHECK(compiled != NULL);
    if (compiled == NULL) {
        return;
    }
    memset(long_name, 'a', sizeof long_name);
    CHECK(anti_rt_regex_group_number(compiled, long_name, 0) == -1);
    CHECK(anti_rt_regex_group_number(compiled, long_name,
                                     (int64_t)sizeof long_name) == -1);
    anti_rt_regex_free(compiled);
}

/* The kind of the first piece of a template, and the offset after it. */
static enum anti_rt_piece_kind first_piece(const char *text, int64_t *after)
{
    struct anti_rt_piece piece;
    int64_t offset = 0;

    memset(&piece, 0, sizeof piece);
    CHECK(anti_rt_regex_piece((const unsigned char *)text,
                              (int64_t)strlen(text), &offset, &piece) == 1);
    *after = offset;
    return piece.kind;
}

/* `${` takes a name of letters, `_` and digits, and a number, each closed
   by `}`. Any other `${` is a `$` written as it stands. */
static void template_names(void)
{
    int64_t after = 0;

    CHECK(first_piece("${A}", &after) == ANTI_RT_PIECE_NAME && after == 4);
    CHECK(first_piece("${_a1}", &after) == ANTI_RT_PIECE_NAME && after == 6);
    CHECK(first_piece("${ab", &after) == ANTI_RT_PIECE_TEXT && after == 1);
    CHECK(first_piece("${ab-}", &after) == ANTI_RT_PIECE_TEXT && after == 1);
    CHECK(first_piece("${12", &after) == ANTI_RT_PIECE_TEXT && after == 1);
    CHECK(first_piece("${12x}", &after) == ANTI_RT_PIECE_TEXT && after == 1);
    CHECK(first_piece("${", &after) == ANTI_RT_PIECE_TEXT && after == 1);
    CHECK(first_piece("${-}", &after) == ANTI_RT_PIECE_TEXT && after == 1);
    CHECK(first_piece("${~}", &after) == ANTI_RT_PIECE_TEXT && after == 1);
}

void test_regex(void)
{
    every_prefix();
    name_with_a_nul();
    message_cut();
    class_escapes();
    class_refusals();
    name_lengths();
    template_names();
}

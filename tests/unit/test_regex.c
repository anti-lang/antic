/* The rewriter of a byte pattern of src/rt/regex.c on every prefix of one
   pattern, and the lookup of a group by a name that holds a NUL. Each
   prefix is a heap copy without a NUL, so AddressSanitizer reports a read
   past its end. */

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

void test_regex(void)
{
    every_prefix();
    name_with_a_nul();
}

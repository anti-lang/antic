/* Three parts. The text buffer of src/antic/text.c, the cut of a long
   line of a message of src/antic/diagnostic.c, and the room of a
   generated name. */

#include <stdint.h>
#include <wchar.h>

#include "../binary_stdio.h"
#include "check.h"
#include "diagnostic.h"
#include "text.h"

void test_text(void)
{
    struct text t = {0};
    char small[3];
    char written[16];
    char formatted[16];
    int i;
    int n;

    CHECK_STR(text_cstr(&t), "");
    text_append(&t, "mov");
    text_appendf(&t, " x%d, #%s", 0, "42");
    CHECK_STR(text_cstr(&t), "mov x0, #42");
    for (i = 0; i < 1000; i++) {
        text_append(&t, "0123456789");
    }
    CHECK(t.length == 11 + 10000);
    text_free(&t);
    CHECK(t.data == NULL && t.length == 0);

    /* Binary data may hold NUL bytes. */
    text_append_bytes(&t, "A\0B", 3);
    CHECK(t.length == 3 && memcmp(t.data, "A\0B", 4) == 0);
    text_free(&t);

    /* No bytes from no buffer append nothing. glibc declares the source of
       memcpy non-null, so UBSan on Linux refuses it even for zero bytes. */
    text_append_bytes(&t, NULL, 0);
    CHECK(t.length == 0);
    CHECK_STR(text_cstr(&t), "");
    text_free(&t);

    /* A buffer of no bytes takes nothing, and one of fewer than four
       takes the start of a text too long for it, without the dots. */
    memcpy(small, "abc", 3);
    text_format(small, 0, "%s", "xyz");
    CHECK(memcmp(small, "abc", 3) == 0);
    text_format(small, sizeof small, "%s", "xyz");
    CHECK_STR(small, "xy");

    /* U+D800 is no character, so the C library of the Mac writes no
       bytes for it and gives -1. The buffer is then empty, and the text
       stays as it was. Each part holds what the C library gives. */
    n = snprintf(written, sizeof written, "%lc", (wint_t)0xD800);
    text_format(formatted, sizeof formatted, "%lc", (wint_t)0xD800);
    CHECK_STR(formatted, n < 0 ? "" : written);
    text_append(&t, "a");
    text_appendf(&t, "%lc", (wint_t)0xD800);
    CHECK(t.length == (n < 0 ? 1 : 1 + (size_t)n));
    text_free(&t);
}

/* A diagnostic too long for its message ends in three dots, so the cut
   is visible. One that fits stands as written. */
void test_diagnostic_cut(void)
{
    struct diagnostics d = {0};
    char long_name[400];
    size_t n;

    memset(long_name, 'x', sizeof long_name - 1);
    long_name[sizeof long_name - 1] = '\0';
    diagnostics_add(&d, 1, 1, "unknown name `%s`", long_name);
    diagnostics_add(&d, 2, 1, "unknown name `%s`", "y");
    CHECK(d.count == 2);
    n = strlen(d.items[0].message);
    CHECK(n == sizeof d.items[0].message - 1);
    CHECK(n >= 3 && memcmp(d.items[0].message + n - 3, "...", 3) == 0);
    CHECK_STR(d.items[1].message, "unknown name `y`");
    diagnostics_free(&d);
}

/* The longest name lowering and the whole-program pass format, with the
   largest count, fits a buffer of TEXT_NAME_SIZE whole. text_format_name
   stops the test where it does not. */
void test_name_size(void)
{
    char name[TEXT_NAME_SIZE];
    char expected[TEXT_NAME_SIZE];
    int n;

    n = snprintf(expected, sizeof expected, "injectable.%zu.library",
                 (size_t)SIZE_MAX);
    CHECK(n > 0 && (size_t)n < sizeof expected);
    text_format_name(name, sizeof name, "injectable.%zu.library",
                     (size_t)SIZE_MAX);
    CHECK_STR(name, expected);
    text_format_name(name, sizeof name, "trampolines.%zu", (size_t)SIZE_MAX);
    CHECK(strlen(name) == 12 + 20);
}

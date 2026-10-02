#include "../binary_stdio.h"
#include "check.h"
#include "diagnostic.h"
#include "text.h"

void test_text(void)
{
    struct text t = {0};
    int i;

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

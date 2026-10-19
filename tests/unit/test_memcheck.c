/* The checks of AddressSanitizer of src/antic/memcheck.c: the targets
   whose runtime archive has them, the declarations a module takes, and a
   check before each load and store that reaches outside the frame. */

#include <string.h>

#include "../binary_stdio.h"
#include "check.h"
#include "pipeline.h"
#include "layout.h"
#include "memcheck.h"
#include "target.h"

/* Lower source, declare the checks for target t as the module that
   links when links is set, lay out each function and put the checks in.
   out receives the IR. */
static void checked_ir(const char *source, enum target t, bool links,
                       struct text *out)
{
    struct lowered l;
    struct layouts layouts;
    char error[160] = "";
    size_t i;

    lowered_run(&l, source);
    if (l.ok) {
        memcheck_declare(&l.ir, "main", links, t);
        CHECK(layout_init(&layouts, t, &l.ir, error, sizeof error));
        CHECK_STR(error, "");
        for (i = 0; i < l.ir.function_count; i++) {
            struct ir_function *f = l.ir.functions[i];
            bool resolved = false;
            CHECK(layout_resolve(&layouts, f, &resolved));
            if (!f->is_extern) {
                memcheck_function(&l.ir, f, &layouts);
            }
        }
        layout_free(&layouts);
        ir_print_own(out, &l.ir);
    }
    lowered_release(&l);
}

/* Count a failure unless text holds line. */
static void holds(const struct text *text, const char *line)
{
    if (strstr(text_cstr(text), line) == NULL) {
        check_failures++;
        fprintf(stderr, "no `%s` in\n%s", line, text_cstr(text));
    }
}

/* Count a failure when text holds line. */
static void lacks(const struct text *text, const char *line)
{
    if (strstr(text_cstr(text), line) != NULL) {
        check_failures++;
        fprintf(stderr, "`%s` in\n%s", line, text_cstr(text));
    }
}

void test_memcheck(void)
{
    static const char source[] = "fn f(p: *int) -> int {\n"
                                 "    let n = 1;\n"
                                 "    *p = n;\n"
                                 "    return *p;\n"
                                 "}\n";
    struct text out = {0};

    /* compiler-rt builds no runtime of AddressSanitizer for Windows on
       ARM64. */
    CHECK(memcheck_available(TARGET_MACOS_ARM64));
    CHECK(memcheck_available(TARGET_LINUX_X86_64));
    CHECK(memcheck_available(TARGET_WINDOWS_X86_64));
    CHECK(!memcheck_available(TARGET_WINDOWS_ARM64));

    /* The store and the load through p take a check of their 8 bytes. The
       slot of n stands in the frame and takes none. A module that does
       not link gets neither the options hook nor the marking. */
    checked_ir(source, TARGET_LINUX_X86_64, false, &out);
    holds(&out, "extern fn " MEMCHECK_LOAD "(ptr, i64)\n");
    holds(&out, "extern fn " MEMCHECK_STORE "(ptr, i64)\n");
    holds(&out, "    call void @" MEMCHECK_STORE "(%0, 8)\n");
    holds(&out, "    call void @" MEMCHECK_LOAD "(%0, 8)\n");
    lacks(&out, MEMCHECK_OPTIONS_HOOK);
    lacks(&out, MEMCHECK_KEPT);
    text_free(&out);

    /* The module that links gets the hook and the marking on Linux, and
       neither on Windows, whose runtime has no leak check. */
    checked_ir(source, TARGET_LINUX_X86_64, true, &out);
    holds(&out, MEMCHECK_OPTIONS_HOOK);
    holds(&out, "extern fn " MEMCHECK_IGNORE "(ptr)\n");
    holds(&out, MEMCHECK_KEPT);
    text_free(&out);
    checked_ir(source, TARGET_WINDOWS_X86_64, true, &out);
    lacks(&out, MEMCHECK_OPTIONS_HOOK);
    lacks(&out, MEMCHECK_KEPT);
    text_free(&out);
}

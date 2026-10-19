/* The run of opt and llc of src/antic/llvm_run.c, with the tools of the
   runtime archive: an object in dev mode, assembly and bitcode in
   release mode, and a failed run on text llc refuses. */

#include <stdio.h>
#include <string.h>

#include "../../src/antic/platform.h"
#include "../binary_stdio.h"
#include "check.h"
#include "llvm_run.h"
#include "llvm_target.h"
#include "target.h"
#include "text.h"

/* Write the LLVM text of one function for target t, or text that no tool
   reads when broken is set, to path. */
static bool write_text(const char *path, enum target t, bool broken)
{
    struct text text = {0};
    FILE *f = platform_open(path, true);
    bool ok = f != NULL;

    text_append(&text, "target triple = \"");
    llvm_triple(&text, t);
    text_appendf(&text, "\"\n\ndefine i64 @answer() {\n  ret i64 %s\n}\n",
                 broken ? "nothing" : "42");
    if (ok) {
        ok = fwrite(text.data, 1, text.length, f) == text.length;
        ok = fclose(f) == 0 && ok;
    }
    text_free(&text);
    return ok;
}

/* The bytes of the file at path, or an empty text when there is none. */
static void read_back(const char *path, struct text *out)
{
    FILE *f = platform_open(path, false);
    char buffer[4096];
    size_t n;

    if (f == NULL) {
        return;
    }
    while ((n = fread(buffer, 1, sizeof buffer, f)) > 0) {
        text_append_bytes(out, buffer, n);
    }
    fclose(f);
}

/* Whether the first bytes of t hold the magic of LLVM bitcode, raw or
   inside the wrapper that Apple's targets put around it. */
static bool is_bitcode(const struct text *t)
{
    static const unsigned char magic[] = {'B', 'C', 0xc0, 0xde};
    size_t at;

    for (at = 0; at + sizeof magic <= t->length && at <= 20; at++) {
        if (memcmp(t->data + at, magic, sizeof magic) == 0) {
            return true;
        }
    }
    return false;
}

void test_llvm_run(void)
{
    struct llvm_run r;
    struct text out = {0};
    enum target host;

    CHECK(target_host(&host));
    memset(&r, 0, sizeof r);
    r.opt = ANTIC_TEST_OPT;
    r.llc = ANTIC_TEST_LLC;
    r.target = host;
    r.lto = LTO_NONE;
    CHECK(write_text("answer.ll", host, false));

    /* Dev mode: llc alone writes the object from the text. */
    CHECK(llvm_run(&r, "answer.ll", "answer.bc", "answer.o"));
    read_back("answer.o", &out);
    CHECK(out.length > 0);
    text_free(&out);

    /* Release mode: opt writes the bitcode, and llc the assembly from
       it. */
    r.optimize = true;
    r.assembly = true;
    CHECK(llvm_run(&r, "answer.ll", "answer.bc", "answer.s"));
    read_back("answer.bc", &out);
    CHECK(is_bitcode(&out));
    text_free(&out);
    read_back("answer.s", &out);
    CHECK(strstr(text_cstr(&out), "answer") != NULL);
    text_free(&out);

    /* A text the tools refuse fails the run, after their own message. */
    r.optimize = false;
    CHECK(write_text("broken.ll", host, true));
    CHECK(!llvm_run(&r, "broken.ll", "broken.bc", "broken.o"));
}

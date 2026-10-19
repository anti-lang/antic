/* The readers of `anti license` in src/anti/licensing.c: the build id,
   the version and the text of the licence notice in the bytes of a
   binary, and the record of upstream sources of the runtime archive. */

#include <string.h>

#include "../binary_stdio.h"
#include "check.h"
#include "files.h"
#include "license.h"
#include "licensing.h"
#include "shared.h"

/* A build id of the notice: 64 lowercase hex digits. */
#define BUILD_ID "0123456789abcdef0123456789abcdef" \
                 "0123456789abcdef0123456789abcdef"

/* The notice between bytes of a binary. The first begin marker carries
   no id and is passed over. */
static const char binary[] =
    "\x7f" "ELF" ANTI_NOTICE_BEGIN ANTI_NOTICE_BUILD "none\n"
    "code" ANTI_NOTICE_BEGIN ANTI_NOTICE_BUILD BUILD_ID "\n"
    "package lib 3.0 MIT\n"
    "attribution Copyright L\n"
    "package app 1.2.0 0BSD\n"
    "text for lib\n"
    "the text of MIT\n" ANTI_NOTICE_END "data";

/* The id, the version of the last package and the text of the notice,
   and no notice in bytes that hold none. */
static void notices(void)
{
    struct text bytes = {0};
    struct text id = {0};
    struct text version = {0};
    struct text text = {0};

    text_append_bytes(&bytes, binary, sizeof binary - 1);
    CHECK(licensing_notice(&bytes, &id, &version));
    CHECK_STR(text_cstr(&id), BUILD_ID);
    CHECK_STR(text_cstr(&version), "1.2.0");
    CHECK(licensing_text(&bytes, &text));
    CHECK_STR(text_cstr(&text), "package lib 3.0 MIT\n"
                                "attribution Copyright L\n"
                                "package app 1.2.0 0BSD\n"
                                "text for lib\n"
                                "the text of MIT\n");
    text_free(&text);
    CHECK(files_write("binary", &bytes));
    CHECK(licensing_text_of("binary", &text));
    CHECK(text.length > 0);
    text_free(&text);
    bytes.length = 0;
    text_append(&bytes, "no notice " ANTI_NOTICE_END);
    CHECK(!licensing_text(&bytes, &text));
    CHECK(!licensing_notice(&bytes, &id, &version));
    text_free(&bytes);
    text_free(&id);
    text_free(&version);
    text_free(&text);
}

/* The record of upstream sources: the version of a component, and the
   line of each component of a `text for` line after its text. */
static void sources(void)
{
    static const char record[] =
        "# name version url\n"
        "musl 1.2.5 https://musl.libc.org/releases/musl-1.2.5.tar.gz\n"
        "mimalloc 3.1.5 https://github.com/microsoft/mimalloc\n";
    struct text text = {0};
    struct text out = {0};

    text_append(&text, record);
    CHECK(licensing_source_version(&text, "musl", &out));
    CHECK_STR(text_cstr(&out), "1.2.5");
    text_free(&out);
    CHECK(!licensing_source_version(&text, "mus", &out));
    CHECK(!licensing_source_version(&text, "name", &out));
    CHECK(files_make_dirs("runtime/" RUNTIME_LICENSES_DIR));
    CHECK(files_write("runtime/" RUNTIME_LICENSES_DIR "/" RUNTIME_SOURCES_FILE,
                      &text));
    text_free(&text);
    text_append(&text, "text for musl mimalloc\nthe text of MIT");
    CHECK(licensing_add_sources(&text, "runtime"));
    CHECK_STR(text_cstr(&text),
              "text for musl mimalloc\n"
              "the text of MIT\n"
              "source musl 1.2.5 "
              "https://musl.libc.org/releases/musl-1.2.5.tar.gz\n"
              "source mimalloc 3.1.5 https://github.com/microsoft/mimalloc\n");
    text_free(&text);
    CHECK(!licensing_add_sources(&text, "no-runtime"));
}

void test_licensing(void)
{
    notices();
    sources();
}

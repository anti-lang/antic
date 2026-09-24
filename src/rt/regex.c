#include "regex.h"

/* The 8-bit library, linked statically, as src/native/ builds it. */
#define PCRE2_CODE_UNIT_WIDTH 8
#define PCRE2_STATIC
#include <pcre2.h>

/* DESIGN: a pattern of text compiles in UTF mode and without PCRE2_UCP, so
   `\d`, `\w` and `\s` mean their ASCII sets, as "Regular expressions" in
   docs/anti-language-additions.md asks. A pattern that starts with
   `(*UCP)` gives the three their Unicode meaning, which PCRE2 reads
   itself. The flags `i`, `m`, `s` and `x` are PCRE2's inline flags, so no
   option of the compile stands for one. */
#define TEXT_OPTIONS PCRE2_UTF

void *anti_rt_regex_compile(const unsigned char *bytes, int64_t length,
                            int32_t *code, int64_t *offset)
{
    int error = 0;
    PCRE2_SIZE at = 0;
    pcre2_code *compiled = pcre2_compile(bytes, (PCRE2_SIZE)length,
                                         TEXT_OPTIONS, &error, &at, NULL);

    if (compiled == NULL) {
        *code = (int32_t)error;
        *offset = (int64_t)at;
    }
    return compiled;
}

void anti_rt_regex_free(void *compiled)
{
    pcre2_code_free(compiled);
}

void anti_rt_regex_message_into(int32_t code, unsigned char *out,
                                size_t room)
{
    if (room == 0) {
        return;
    }
    out[0] = '\0';
    /* A message longer than room comes back cut and ended by a NUL, with
       PCRE2_ERROR_NOMEMORY, and a number PCRE2 does not know leaves out
       empty. Both are what the caller shows. */
    (void)pcre2_get_error_message(code, out, room);
}

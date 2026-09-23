/* The C half of the tests pcre2_match and pcre2_link_<target>. It compiles
   a pattern with the PCRE2 of the runtime tree and matches it, and
   pcre2_match.anti prints what it returns. The functions take and give
   plain integers and C strings, so the Anti half needs no binding of
   PCRE2. */
#define PCRE2_CODE_UNIT_WIDTH 8
#define PCRE2_STATIC
#include "../binary_stdio.h"
#include <pcre2.h>
#include <stddef.h>
#include <stdint.h>

int32_t pcre2_probe(const char *pattern, const char *subject, int32_t group,
                    int32_t end);
int32_t pcre2_probe_config(int32_t what);

/* The byte offset where group <group> of the first match of <pattern> in
   <subject> starts, or where it ends when <end> is not 0. The pattern
   compiles in UTF mode. -1 is no match, and a pattern that does not
   compile gives -1000 minus the offset that PCRE2 names. */
int32_t pcre2_probe(const char *pattern, const char *subject, int32_t group,
                    int32_t end)
{
    int error;
    PCRE2_SIZE offset;
    pcre2_code *code = pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED,
                                     PCRE2_UTF, &error, &offset, NULL);
    pcre2_match_data *data;
    int count;
    int32_t result = -1;

    if (code == NULL) {
        return -1000 - (int32_t)offset;
    }
    data = pcre2_match_data_create_from_pattern(code, NULL);
    if (data == NULL) {
        pcre2_code_free(code);
        return -2;
    }
    count = pcre2_match(code, (PCRE2_SPTR)subject, PCRE2_ZERO_TERMINATED, 0, 0,
                        data, NULL);
    if (count > group) {
        PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(data);
        size_t slot = 2 * (size_t)group + (end != 0 ? 1u : 0u);
        result = (int32_t)ovector[slot];
    }
    pcre2_match_data_free(data);
    pcre2_code_free(code);
    return result;
}

/* 1 when the library was built with the option, 0 when not. <what> 0
   asks for Unicode support and 1 for JIT. */
int32_t pcre2_probe_config(int32_t what)
{
    uint32_t value = 0;

    if (pcre2_config(what == 0 ? PCRE2_CONFIG_UNICODE : PCRE2_CONFIG_JIT,
                     &value) < 0) {
        return -1;
    }
    return (int32_t)value;
}

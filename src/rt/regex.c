#include "regex.h"

#include <string.h>

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

int64_t anti_rt_regex_group_count(const void *compiled)
{
    uint32_t count = 0;

    (void)pcre2_pattern_info(compiled, PCRE2_INFO_CAPTURECOUNT, &count);
    return (int64_t)count;
}

/* The longest group name PCRE2 accepts, and room for its NUL. */
#define NAME_ROOM 256

int64_t anti_rt_regex_group_number(const void *compiled,
                                   const unsigned char *name, int64_t length)
{
    unsigned char text[NAME_ROOM];
    int number;

    if (length <= 0 || length >= NAME_ROOM) {
        return -1;
    }
    memcpy(text, name, (size_t)length);
    text[length] = '\0';
    number = pcre2_substring_number_from_name(compiled, text);
    if (number == PCRE2_ERROR_NOUNIQUESUBSTRING) {
        PCRE2_SPTR first = NULL;
        PCRE2_SPTR last = NULL;
        (void)pcre2_substring_nametable_scan(compiled, text, &first, &last);
        /* An entry starts with its number in two bytes, high first. */
        return first == NULL ? -1 : (int64_t)((first[0] << 8) | first[1]);
    }
    return number < 0 ? -1 : (int64_t)number;
}

static int is_digit(unsigned char c)
{
    return c >= '0' && c <= '9';
}

static int is_name_start(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

/* The largest group number a template reads. A larger one names a group
   no pattern has, and stays above every count. */
#define NUMBER_CAP 1000000000

/* The number of the digits from at to end, capped. */
static int64_t digits_value(const unsigned char *bytes, int64_t at,
                            int64_t end)
{
    int64_t value = 0;

    for (; at < end; at++) {
        value = value * 10 + (bytes[at] - '0');
        if (value > NUMBER_CAP) {
            value = NUMBER_CAP;
        }
    }
    return value;
}

/* DESIGN: `$` reads every digit that follows it, so `$10` is group 10,
   and `${1}0` is group 1 and a `0`. `${name}` names a group by its name
   and `$$` writes one `$`. A `$` that starts none of these forms is
   written as it stands, as the smallest reading that refuses nothing. */
int anti_rt_regex_piece(const unsigned char *bytes, int64_t length,
                        int64_t *offset, struct anti_rt_piece *piece)
{
    int64_t at = *offset;
    int64_t end;

    if (at >= length) {
        return 0;
    }
    piece->kind = ANTI_RT_PIECE_TEXT;
    piece->number = 0;
    if (bytes[at] != '$') {
        end = at;
        while (end < length && bytes[end] != '$') {
            end++;
        }
        piece->start = at;
        piece->length = end - at;
        *offset = end;
        return 1;
    }
    /* `$$`, and a `$` that ends the template. */
    piece->start = at + (at + 1 < length && bytes[at + 1] == '$' ? 1 : 0);
    piece->length = 1;
    *offset = piece->start + 1;
    if (at + 1 >= length) {
        return 1;
    }
    if (is_digit(bytes[at + 1])) {
        end = at + 1;
        while (end < length && is_digit(bytes[end])) {
            end++;
        }
        piece->kind = ANTI_RT_PIECE_NUMBER;
        piece->number = digits_value(bytes, at + 1, end);
        *offset = end;
        return 1;
    }
    if (bytes[at + 1] != '{') {
        return 1;
    }
    end = at + 2;
    if (end < length && is_digit(bytes[end])) {
        while (end < length && is_digit(bytes[end])) {
            end++;
        }
        if (end < length && bytes[end] == '}') {
            piece->kind = ANTI_RT_PIECE_NUMBER;
            piece->number = digits_value(bytes, at + 2, end);
            *offset = end + 1;
        }
        return 1;
    }
    if (end < length && is_name_start(bytes[end])) {
        while (end < length &&
               (is_name_start(bytes[end]) || is_digit(bytes[end]))) {
            end++;
        }
        if (end < length && bytes[end] == '}') {
            piece->kind = ANTI_RT_PIECE_NAME;
            piece->start = at + 2;
            piece->length = end - (at + 2);
            *offset = end + 1;
        }
    }
    return 1;
}

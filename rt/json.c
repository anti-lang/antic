/* The JSON scanner that Object.deserialize and anti bind share.

   DESIGN: the runtime holds the scanner because Object.deserialize runs
   inside a program. anti links the same file, as it links rt/toml.c, so
   one scanner reads the JSON of both. It reads text from outside the
   program in both uses. Every read checks at against end first. Malformed
   input is refused with false, never with an abort. */
#include <string.h>

#include "json.h"

void anti_rt_json_space(struct anti_json *s)
{
    while (s->at < s->end && (*s->at == ' ' || *s->at == '\t' ||
                              *s->at == '\n' || *s->at == '\r')) {
        s->at++;
    }
}

bool anti_rt_json_take(struct anti_json *s, unsigned char c)
{
    anti_rt_json_space(s);
    if (s->at < s->end && *s->at == c) {
        s->at++;
        return true;
    }
    return false;
}

bool anti_rt_json_word(struct anti_json *s, const char *word)
{
    size_t n = strlen(word);

    anti_rt_json_space(s);
    if ((size_t)(s->end - s->at) >= n && memcmp(s->at, word, n) == 0) {
        s->at += n;
        return true;
    }
    return false;
}

static int hex_digit(unsigned char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

/* Append one byte to out, or fail when out holds room bytes already. A
   NULL out keeps nothing and counts the bytes. */
static bool put_byte(unsigned char *out, size_t room, size_t *length,
                     unsigned char b)
{
    if (out != NULL) {
        if (*length >= room) {
            return false;
        }
        out[*length] = b;
    }
    (*length)++;
    return true;
}

/* DESIGN: a \u escape of a surrogate half is refused, since the
   serializer writes none and clang writes UTF-8. */
bool anti_rt_json_string(struct anti_json *s, unsigned char *out,
                         size_t room, size_t *length)
{
    *length = 0;
    if (!anti_rt_json_take(s, '"')) {
        return false;
    }
    while (s->at < s->end && *s->at != '"') {
        unsigned char c = *s->at++;
        if (c < 0x20) {
            return false;
        }
        if (c != '\\') {
            if (!put_byte(out, room, length, c)) {
                return false;
            }
            continue;
        }
        if (s->at >= s->end) {
            return false;
        }
        c = *s->at++;
        switch (c) {
        case '"': case '\\': case '/':
            break;
        case 'b': c = '\b'; break;
        case 'f': c = '\f'; break;
        case 'n': c = '\n'; break;
        case 'r': c = '\r'; break;
        case 't': c = '\t'; break;
        case 'u': {
            unsigned long code = 0;
            int k;
            for (k = 0; k < 4; k++) {
                int d = s->at < s->end ? hex_digit(*s->at++) : -1;
                if (d < 0) {
                    return false;
                }
                code = code * 16 + (unsigned long)d;
            }
            if (code >= 0xD800 && code <= 0xDFFF) {
                return false;
            }
            if (code < 0x80) {
                c = (unsigned char)code;
                break;
            }
            if (code < 0x800) {
                if (!put_byte(out, room, length,
                              (unsigned char)(0xC0 | (code >> 6)))) {
                    return false;
                }
            } else if (!put_byte(out, room, length,
                                 (unsigned char)(0xE0 | (code >> 12))) ||
                       !put_byte(out, room, length, (unsigned char)(
                           0x80 | ((code >> 6) & 0x3F)))) {
                return false;
            }
            c = (unsigned char)(0x80 | (code & 0x3F));
            break;
        }
        default:
            return false;
        }
        if (!put_byte(out, room, length, c)) {
            return false;
        }
    }
    return s->at < s->end && *s->at++ == '"';
}

bool anti_rt_json_number(struct anti_json *s, const unsigned char **start,
                         int64_t *length)
{
    anti_rt_json_space(s);
    *start = s->at;
    while (s->at < s->end &&
           ((*s->at >= '0' && *s->at <= '9') || *s->at == '-' ||
            *s->at == '+' || *s->at == '.' || *s->at == 'e' ||
            *s->at == 'E')) {
        s->at++;
    }
    *length = s->at - *start;
    return *length > 0;
}

/* The digits from i, and whether there was at least one. */
static bool digits(const unsigned char *p, int64_t length, int64_t *i)
{
    int64_t from = *i;

    while (*i < length && p[*i] >= '0' && p[*i] <= '9') {
        (*i)++;
    }
    return *i > from;
}

bool anti_rt_json_valid_number(const unsigned char *start, int64_t length)
{
    int64_t i = 0;

    if (start == NULL || length <= 0) {
        return false;
    }
    if (start[i] == '-') {
        i++;
    }
    if (i < length && start[i] == '0') {
        i++;
    } else if (!digits(start, length, &i)) {
        return false;
    }
    if (i < length && start[i] == '.') {
        i++;
        if (!digits(start, length, &i)) {
            return false;
        }
    }
    if (i < length && (start[i] == 'e' || start[i] == 'E')) {
        i++;
        if (i < length && (start[i] == '+' || start[i] == '-')) {
            i++;
        }
        if (!digits(start, length, &i)) {
            return false;
        }
    }
    return i == length;
}

bool anti_rt_json_integer(const unsigned char *start, int64_t length,
                          int64_t *value)
{
    bool negative;
    uint64_t magnitude = 0;
    uint64_t limit;
    int64_t i;

    if (!anti_rt_json_valid_number(start, length)) {
        return false;
    }
    negative = start[0] == '-';
    limit = negative ? (uint64_t)INT64_MAX + 1 : (uint64_t)INT64_MAX;
    for (i = negative ? 1 : 0; i < length; i++) {
        uint64_t d;
        if (start[i] < '0' || start[i] > '9') {
            return false;
        }
        d = (uint64_t)(start[i] - '0');
        if (magnitude > (limit - d) / 10) {
            return false;
        }
        magnitude = magnitude * 10 + d;
    }
    if (!negative) {
        *value = (int64_t)magnitude;
    } else if (magnitude == (uint64_t)INT64_MAX + 1) {
        *value = INT64_MIN;
    } else {
        *value = -(int64_t)magnitude;
    }
    return true;
}

bool anti_rt_json_skip(struct anti_json *s, int depth)
{
    const unsigned char *number;
    int64_t digits_length;
    size_t length;

    if (depth > ANTI_JSON_DEPTH) {
        return false;
    }
    anti_rt_json_space(s);
    if (s->at >= s->end) {
        return false;
    }
    switch (*s->at) {
    case '"':
        return anti_rt_json_string(s, NULL, 0, &length);
    case '{':
        s->at++;
        if (anti_rt_json_take(s, '}')) {
            return true;
        }
        do {
            if (!anti_rt_json_string(s, NULL, 0, &length) ||
                !anti_rt_json_take(s, ':') ||
                !anti_rt_json_skip(s, depth + 1)) {
                return false;
            }
        } while (anti_rt_json_take(s, ','));
        return anti_rt_json_take(s, '}');
    case '[':
        s->at++;
        if (anti_rt_json_take(s, ']')) {
            return true;
        }
        do {
            if (!anti_rt_json_skip(s, depth + 1)) {
                return false;
            }
        } while (anti_rt_json_take(s, ','));
        return anti_rt_json_take(s, ']');
    default:
        return anti_rt_json_word(s, "true") ||
               anti_rt_json_word(s, "false") ||
               anti_rt_json_word(s, "null") ||
               anti_rt_json_number(s, &number, &digits_length);
    }
}

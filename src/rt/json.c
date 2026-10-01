/* The JSON scanner that Object.deserialize and anti bind share.

   DESIGN: the runtime holds the scanner because Object.deserialize runs
   inside a program. anti links the same file, as it links src/rt/toml.c, so
   one scanner reads the JSON of both. It reads text from outside the
   program in both uses. Every read checks at against end first. Malformed
   input is refused with false, never with an abort. */
#include <string.h>

#include "json.h"
#include "utf.h"

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

/* Where the decoded bytes of a string go. out takes them when it is not
   NULL, and refuses the byte past room. against, when it is not NULL,
   holds room bytes that are compared with them, and same turns false at
   the first difference. length counts the bytes in every case. */
struct sink {
    unsigned char *out;
    const unsigned char *against;
    size_t room;
    size_t length;
    bool same;
};

static bool put_byte(struct sink *k, unsigned char b)
{
    if (k->out != NULL) {
        if (k->length >= k->room) {
            return false;
        }
        k->out[k->length] = b;
    }
    if (k->against != NULL &&
        (k->length >= k->room || k->against[k->length] != b)) {
        k->same = false;
    }
    k->length++;
    return true;
}

/* DESIGN: a \u escape of a surrogate half is refused, since the
   serializer writes none and clang writes UTF-8. */
static bool read_string(struct anti_json *s, struct sink *k)
{
    if (!anti_rt_json_take(s, '"')) {
        return false;
    }
    while (s->at < s->end && *s->at != '"') {
        unsigned char c = *s->at++;
        if (c < 0x20) {
            return false;
        }
        if (c != '\\') {
            if (!put_byte(k, c)) {
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
            int j;
            for (j = 0; j < 4; j++) {
                int d = s->at < s->end ? hex_digit(*s->at++) : -1;
                if (d < 0) {
                    return false;
                }
                code = code * 16 + (unsigned long)d;
            }
            if (code >= 0xD800 && code <= 0xDFFF) {
                return false;
            }
            {
                unsigned char utf8[4];
                size_t count = anti_rt_utf8_encode((uint32_t)code, utf8);
                size_t i;
                for (i = 0; i + 1 < count; i++) {
                    if (!put_byte(k, utf8[i])) {
                        return false;
                    }
                }
                c = utf8[count - 1];
            }
            break;
        }
        default:
            return false;
        }
        if (!put_byte(k, c)) {
            return false;
        }
    }
    return s->at < s->end && *s->at++ == '"';
}

bool anti_rt_json_string(struct anti_json *s, unsigned char *out,
                         size_t room, size_t *length)
{
    struct sink k = {out, NULL, room, 0, true};
    bool ok = read_string(s, &k);

    *length = k.length;
    return ok;
}

bool anti_rt_json_key(struct anti_json *s, const unsigned char *name,
                      size_t name_length, bool *same)
{
    struct sink k = {NULL, name, name_length, 0, true};
    bool ok = read_string(s, &k);

    *same = ok && k.same && k.length == name_length;
    return ok;
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

/* The value of the digits after the sign of a JSON integer, refused when
   the bytes are no integer of the grammar or the value passes limit. */
static bool magnitude_of(const unsigned char *start, int64_t length,
                         uint64_t limit, uint64_t *magnitude)
{
    int64_t i;

    if (!anti_rt_json_valid_number(start, length)) {
        return false;
    }
    *magnitude = 0;
    for (i = start[0] == '-' ? 1 : 0; i < length; i++) {
        uint64_t d;
        if (start[i] < '0' || start[i] > '9') {
            return false;
        }
        d = (uint64_t)(start[i] - '0');
        if (*magnitude > (limit - d) / 10) {
            return false;
        }
        *magnitude = *magnitude * 10 + d;
    }
    return true;
}

bool anti_rt_json_integer(const unsigned char *start, int64_t length,
                          int64_t *value)
{
    uint64_t magnitude;
    bool negative = length > 0 && start != NULL && start[0] == '-';

    if (!magnitude_of(start, length,
                      negative ? (uint64_t)INT64_MAX + 1 : (uint64_t)INT64_MAX,
                      &magnitude)) {
        return false;
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

/* DESIGN: -0 is an integer of the grammar whose value is 0, so it reads
   as 0. Any other minus sign gives a value below 0, which is refused. */
bool anti_rt_json_unsigned(const unsigned char *start, int64_t length,
                           uint64_t *value)
{
    uint64_t magnitude;

    if (!magnitude_of(start, length, UINT64_MAX, &magnitude) ||
        (start[0] == '-' && magnitude != 0)) {
        return false;
    }
    *value = magnitude;
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

bool anti_rt_json_value(struct anti_json *s)
{
    if (!anti_rt_json_skip(s, 0)) {
        return false;
    }
    anti_rt_json_space(s);
    return s->at == s->end;
}

bool anti_rt_json_member(const unsigned char *source, int64_t length,
                         const unsigned char *name, int64_t name_length,
                         int64_t *start, int64_t *value_length)
{
    struct anti_json s;
    bool same;

    if (source == NULL || length < 0 || name_length < 0 ||
        (name == NULL && name_length > 0)) {
        return false;
    }
    s.at = source;
    s.end = source + length;
    {
        struct anti_json whole = s;
        anti_rt_json_space(&whole);
        if (whole.at >= whole.end || *whole.at != '{' ||
            !anti_rt_json_value(&whole)) {
            return false;
        }
    }
    if (!anti_rt_json_take(&s, '{') || anti_rt_json_take(&s, '}')) {
        return false;
    }
    do {
        const unsigned char *from;
        if (!anti_rt_json_key(&s, name, (size_t)name_length, &same) ||
            !anti_rt_json_take(&s, ':')) {
            return false;
        }
        anti_rt_json_space(&s);
        from = s.at;
        if (!anti_rt_json_skip(&s, 1)) {
            return false;
        }
        if (same) {
            *start = from - source;
            *value_length = s.at - from;
            return true;
        }
    } while (anti_rt_json_take(&s, ','));
    return false;
}

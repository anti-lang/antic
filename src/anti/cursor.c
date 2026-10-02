/* The cursor over one line of text that comes from another machine. */
#include "cursor.h"

bool cursor_is_blank(char c)
{
    return c == ' ' || c == '\t' || c == '\r';
}

bool cursor_blank(struct cursor *c)
{
    const char *from = c->at;

    while (c->at < c->end && cursor_is_blank(*c->at)) {
        c->at++;
    }
    return c->at > from;
}

static int digit_of(char c, unsigned base)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (base == 16 && c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (base == 16 && c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

bool cursor_number(struct cursor *c, unsigned base, uint64_t *out)
{
    uint64_t value = 0;
    const char *from = c->at;
    int d;

    while (c->at < c->end && (d = digit_of(*c->at, base)) >= 0) {
        if (value > (UINT64_MAX - (uint64_t)d) / base) {
            return false;
        }
        value = value * base + (uint64_t)d;
        c->at++;
    }
    *out = value;
    return c->at > from;
}

bool cursor_hex(struct cursor *c, uint64_t *out)
{
    if (c->end - c->at > 2 && c->at[0] == '0' &&
        (c->at[1] == 'x' || c->at[1] == 'X')) {
        c->at += 2;
    }
    return cursor_number(c, 16, out);
}

bool cursor_char(struct cursor *c, char want)
{
    if (c->at < c->end && *c->at == want) {
        c->at++;
        return true;
    }
    return false;
}

bool cursor_token(struct cursor *c, const char **token, size_t *length)
{
    const char *from = c->at;

    while (c->at < c->end && !cursor_is_blank(*c->at)) {
        c->at++;
    }
    *token = from;
    *length = (size_t)(c->at - from);
    return *length > 0;
}

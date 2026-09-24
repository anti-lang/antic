/* The TOML subset that anti.toml reads and the logger configures itself
   from. It is one pass over the text that writes a flat list of keys.

   DESIGN: the runtime holds the parser because the logger reads its file
   before `main` runs, where no Anti code has started. One parser then
   serves both, and anti.toml is a face over this list. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "toml.h"

/* The room of a dotted path, the table and the key, with its NUL. A key
   longer than that is refused where it is read, so its length also fits
   the precision of a format. */
#define PATH_ROOM 320

struct pair {
    char *key;
    char *value;
    int64_t key_length;
    int64_t value_length;
    int64_t line;
};

struct anti_toml {
    struct pair *pairs;
    size_t count;
    size_t capacity;
};

struct reader {
    const unsigned char *bytes;
    int64_t len;
    int64_t pos;
    int64_t line;
    char table[256];
    size_t table_length;
    int failed;
};

static int at_end(const struct reader *r)
{
    return r->pos >= r->len;
}

static int peek(const struct reader *r)
{
    return at_end(r) ? -1 : r->bytes[r->pos];
}

static void skip_spaces(struct reader *r)
{
    while (!at_end(r) && (r->bytes[r->pos] == ' ' || r->bytes[r->pos] == '\t')) {
        r->pos++;
    }
}

/* Past the spaces, the comments and the line breaks. */
static void skip_blank(struct reader *r)
{
    while (!at_end(r)) {
        unsigned char c = r->bytes[r->pos];
        if (c == ' ' || c == '\t' || c == '\r') {
            r->pos++;
        } else if (c == '\n') {
            r->line++;
            r->pos++;
        } else if (c == '#') {
            while (!at_end(r) && r->bytes[r->pos] != '\n') {
                r->pos++;
            }
        } else {
            return;
        }
    }
}

static int is_bare(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '-';
}

static char *keep(const char *text, int64_t length)
{
    char *copy = malloc((size_t)length + 1);

    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, text, (size_t)length);
    copy[length] = '\0';
    return copy;
}

static void add(struct anti_toml *doc, struct reader *r, const char *key,
                int64_t key_length, const char *value, int64_t value_length)
{
    struct pair *pair;

    if (doc->count == doc->capacity) {
        size_t capacity = doc->capacity == 0 ? 16 : doc->capacity * 2;
        struct pair *grown = realloc(doc->pairs, capacity * sizeof *grown);
        if (grown == NULL) {
            r->failed = 1;
            return;
        }
        doc->pairs = grown;
        doc->capacity = capacity;
    }
    pair = &doc->pairs[doc->count];
    pair->key = keep(key, key_length);
    pair->value = keep(value, value_length);
    pair->key_length = key_length;
    pair->value_length = value_length;
    pair->line = r->line;
    if (pair->key == NULL || pair->value == NULL) {
        /* The pair is not counted, so anti_rt_toml_free never sees
           the half that was copied. */
        free(pair->key);
        free(pair->value);
        r->failed = 1;
        return;
    }
    doc->count++;
}

/* The count of `[[name]]` tables seen so far under that name. It is -1
   when the largest number under the name is the limit of `int64_t`,
   which leaves no next count. */
static int64_t repeats(const struct anti_toml *doc, const char *name,
                       size_t length)
{
    int64_t seen = -1;
    size_t i;

    for (i = 0; i < doc->count; i++) {
        const char *key = doc->pairs[i].key;
        if (strncmp(key, name, length) == 0 && key[length] == '.') {
            int64_t number = strtoll(key + length + 1, NULL, 10);
            if (number > seen) {
                seen = number;
            }
        }
    }
    return seen == INT64_MAX ? -1 : seen + 1;
}

static void read_header(struct reader *r, struct anti_toml *doc)
{
    int array = 0;
    int64_t start;
    int64_t length;
    int64_t next;
    int written;

    r->pos++;
    if (peek(r) == '[') {
        array = 1;
        r->pos++;
    }
    skip_spaces(r);
    start = r->pos;
    while (!at_end(r) && (is_bare(peek(r)) || peek(r) == '.')) {
        r->pos++;
    }
    length = r->pos - start;
    if (length <= 0 || (size_t)length >= sizeof r->table - 24) {
        r->failed = 1;
        return;
    }
    memcpy(r->table, r->bytes + start, (size_t)length);
    r->table_length = (size_t)length;
    r->table[length] = '\0';
    skip_spaces(r);
    if (peek(r) != ']') {
        r->failed = 1;
        return;
    }
    r->pos++;
    if (array) {
        if (peek(r) != ']') {
            r->failed = 1;
            return;
        }
        r->pos++;
        next = repeats(doc, r->table, r->table_length);
        if (next < 0) {
            r->failed = 1;
            return;
        }
        written = snprintf(r->table + r->table_length,
                           sizeof r->table - r->table_length, ".%lld",
                           (long long)next);
        if (written <= 0 ||
            (size_t)written >= sizeof r->table - r->table_length) {
            r->failed = 1;
            return;
        }
        r->table_length += (size_t)written;
    }
}

/* The bytes in quotes at the read position, without them. */
static void read_quoted(struct reader *r, const char **text, int64_t *length)
{
    int quote = peek(r);
    int64_t start;

    r->pos++;
    start = r->pos;
    while (!at_end(r) && peek(r) != quote && peek(r) != '\n') {
        r->pos++;
    }
    if (peek(r) != quote) {
        r->failed = 1;
        return;
    }
    *text = (const char *)r->bytes + start;
    *length = r->pos - start;
    r->pos++;
}

/* The key before `=`, bare or in quotes. A quoted key carries the dots
   of an interface name, so the path it writes holds them as well. */
static void read_key(struct reader *r, const char **key, int64_t *length)
{
    int64_t start;

    if (peek(r) == '"' || peek(r) == '\'') {
        read_quoted(r, key, length);
    } else {
        start = r->pos;
        while (!at_end(r) && is_bare(peek(r))) {
            r->pos++;
        }
        *key = (const char *)r->bytes + start;
        *length = r->pos - start;
        if (*length == 0) {
            r->failed = 1;
        }
    }
    if (!r->failed && *length >= PATH_ROOM) {
        r->failed = 1;
    }
}

/* Whether c is one of the bytes that end a value here. Inside an array
   that is a comma and `]`, and inside an inline table a comma and `}`. */
static int stops_value(const char *stops, int c)
{
    return c >= 0 && strchr(stops, c) != NULL;
}

/* One value, with the quotes of a string removed. stops holds the bytes
   that end it beside a line break and a comment. */
static void read_value(struct reader *r, const char *stops, const char **value,
                       int64_t *length)
{
    int64_t start;

    skip_spaces(r);
    if (peek(r) == '"' || peek(r) == '\'') {
        read_quoted(r, value, length);
        return;
    }
    start = r->pos;
    while (!at_end(r) && peek(r) != '\n' && peek(r) != '#' &&
           !stops_value(stops, peek(r))) {
        r->pos++;
    }
    *length = r->pos - start;
    while (*length > 0 && (r->bytes[start + *length - 1] == ' ' ||
                           r->bytes[start + *length - 1] == '\t' ||
                           r->bytes[start + *length - 1] == '\r')) {
        (*length)--;
    }
    *value = (const char *)r->bytes + start;
    if (*length == 0) {
        r->failed = 1;
    }
}

static void read_array(struct reader *r, struct anti_toml *doc,
                       const char *path);

/* The pairs of an inline table, each under the path of the table. The
   value of a pair is a plain value, an array or another inline table, so
   `{ version = "1.2.4", repo = "ff" }` at `dependencies.a` writes
   `dependencies.a.version` and `dependencies.a.repo`. */
static void read_inline_table(struct reader *r, struct anti_toml *doc,
                              const char *path)
{
    r->pos++;
    while (!r->failed) {
        char key[PATH_ROOM];
        const char *name = NULL;
        const char *value = NULL;
        int64_t value_length = 0;
        int64_t name_length = 0;
        int length;
        skip_blank(r);
        if (peek(r) == '}') {
            r->pos++;
            return;
        }
        if (at_end(r)) {
            r->failed = 1;
            return;
        }
        read_key(r, &name, &name_length);
        if (r->failed) {
            return;
        }
        skip_spaces(r);
        if (peek(r) != '=') {
            r->failed = 1;
            return;
        }
        r->pos++;
        length = snprintf(key, sizeof key, "%s.%.*s", path, (int)name_length,
                          name);
        if (length <= 0 || (size_t)length >= sizeof key) {
            r->failed = 1;
            return;
        }
        skip_spaces(r);
        if (peek(r) == '[') {
            read_array(r, doc, key);
        } else if (peek(r) == '{') {
            read_inline_table(r, doc, key);
        } else {
            read_value(r, ",}", &value, &value_length);
            if (r->failed) {
                return;
            }
            add(doc, r, key, length, value, value_length);
        }
        if (r->failed) {
            return;
        }
        skip_blank(r);
        if (peek(r) == ',') {
            r->pos++;
        } else if (peek(r) != '}') {
            r->failed = 1;
        }
    }
}

/* The elements of an array, as the keys path.0, path.1, path.2. The
   array runs over lines, and a comma after its last element is allowed.
   An element that opens with `{` is an inline table, so an array of them
   writes path.0.name. An array of arrays is outside the subset. */
static void read_array(struct reader *r, struct anti_toml *doc,
                       const char *path)
{
    int64_t index = 0;

    r->pos++;
    while (!r->failed) {
        char key[PATH_ROOM];
        const char *value = NULL;
        int64_t value_length = 0;
        int length;
        skip_blank(r);
        if (peek(r) == ']') {
            r->pos++;
            return;
        }
        if (at_end(r) || peek(r) == '[') {
            r->failed = 1;
            return;
        }
        length = snprintf(key, sizeof key, "%s.%lld", path, (long long)index);
        if (length <= 0 || (size_t)length >= sizeof key) {
            r->failed = 1;
            return;
        }
        if (peek(r) == '{') {
            read_inline_table(r, doc, key);
        } else {
            read_value(r, ",]", &value, &value_length);
            if (r->failed) {
                return;
            }
            add(doc, r, key, length, value, value_length);
        }
        if (r->failed) {
            return;
        }
        index++;
        skip_blank(r);
        if (peek(r) == ',') {
            r->pos++;
        } else if (peek(r) != ']') {
            r->failed = 1;
        }
    }
}

static void read_pair(struct reader *r, struct anti_toml *doc)
{
    char path[PATH_ROOM];
    const char *key = NULL;
    const char *value = NULL;
    int64_t value_length = 0;
    int64_t length = 0;
    int written;

    read_key(r, &key, &length);
    if (r->failed) {
        return;
    }
    skip_spaces(r);
    if (peek(r) != '=') {
        r->failed = 1;
        return;
    }
    r->pos++;
    written = r->table_length == 0
                  ? snprintf(path, sizeof path, "%.*s", (int)length, key)
                  : snprintf(path, sizeof path, "%s.%.*s", r->table,
                             (int)length, key);
    if (written <= 0 || (size_t)written >= sizeof path) {
        r->failed = 1;
        return;
    }
    skip_spaces(r);
    if (peek(r) == '[') {
        read_array(r, doc, path);
        return;
    }
    if (peek(r) == '{') {
        read_inline_table(r, doc, path);
        return;
    }
    read_value(r, "", &value, &value_length);
    if (r->failed) {
        return;
    }
    add(doc, r, path, written, value, value_length);
}

struct anti_toml *anti_rt_toml_read(const unsigned char *bytes, int64_t len)
{
    struct anti_toml *doc = calloc(1, sizeof *doc);
    struct reader r;

    if (doc == NULL) {
        return NULL;
    }
    memset(&r, 0, sizeof r);
    r.bytes = bytes;
    r.len = len;
    r.line = 1;
    while (!r.failed) {
        skip_blank(&r);
        if (at_end(&r)) {
            return doc;
        }
        if (peek(&r) == '[') {
            read_header(&r, doc);
        } else if (is_bare(peek(&r)) || peek(&r) == '"' || peek(&r) == '\'') {
            read_pair(&r, doc);
        } else {
            r.failed = 1;
        }
    }
    anti_rt_toml_free(doc);
    return NULL;
}

int64_t anti_rt_toml_count(const struct anti_toml *doc)
{
    return doc == NULL ? 0 : (int64_t)doc->count;
}

static struct anti_text text_of(const char *bytes, int64_t length)
{
    struct anti_text text;

    text.ptr = (const unsigned char *)(bytes == NULL ? "" : bytes);
    text.len = bytes == NULL ? 0 : length;
    return text;
}

struct anti_text anti_rt_toml_key(const struct anti_toml *doc, int64_t index)
{
    if (doc == NULL || index < 0 || index >= (int64_t)doc->count) {
        return text_of(NULL, 0);
    }
    return text_of(doc->pairs[index].key, doc->pairs[index].key_length);
}

struct anti_text anti_rt_toml_value(const struct anti_toml *doc, int64_t index)
{
    if (doc == NULL || index < 0 || index >= (int64_t)doc->count) {
        return text_of(NULL, 0);
    }
    return text_of(doc->pairs[index].value, doc->pairs[index].value_length);
}

int64_t anti_rt_toml_find(const struct anti_toml *doc,
                          const unsigned char *path, int64_t len)
{
    size_t i;

    if (doc == NULL) {
        return -1;
    }
    for (i = 0; i < doc->count; i++) {
        if (doc->pairs[i].key_length == len &&
            memcmp(doc->pairs[i].key, path, (size_t)len) == 0) {
            return (int64_t)i;
        }
    }
    return -1;
}

int64_t anti_rt_toml_line(const struct anti_toml *doc, int64_t index)
{
    if (doc == NULL || index < 0 || index >= (int64_t)doc->count) {
        return 0;
    }
    return doc->pairs[index].line;
}

void anti_rt_toml_free(struct anti_toml *doc)
{
    size_t i;

    if (doc == NULL) {
        return;
    }
    for (i = 0; i < doc->count; i++) {
        free(doc->pairs[i].key);
        free(doc->pairs[i].value);
    }
    free(doc->pairs);
    free(doc);
}

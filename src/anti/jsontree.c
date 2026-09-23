/* A tree of a JSON document over the scanner of src/rt/json.c. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json.h"
#include "jsontree.h"

struct builder {
    struct anti_json scan;
    const unsigned char *start;
    struct arena *arena;
    char *error;
    size_t error_size;
};

/* The message names the line and the column of the failure, counted
   from 1, as an editor shows them. */
static bool fail(struct builder *b, const char *what)
{
    const unsigned char *p;
    long long line = 1;
    long long column = 1;

    for (p = b->start; p < b->scan.at; p++) {
        if (*p == '\n') {
            line++;
            column = 1;
        } else {
            column++;
        }
    }
    snprintf(b->error, b->error_size, "%s at line %lld, column %lld", what,
             line, column);
    return false;
}

/* A string of the input as text of the memory pool. The scan counts the bytes
   first, so the copy needs one allocation of the right size. */
static bool read_string(struct builder *b, const char **text, size_t *length)
{
    struct anti_json count = b->scan;
    size_t n;
    char *out;

    if (!anti_rt_json_string(&count, NULL, 0, &n)) {
        return fail(b, "a malformed string");
    }
    out = arena_alloc(b->arena, n + 1);
    if (!anti_rt_json_string(&b->scan, (unsigned char *)out, n, &n)) {
        return fail(b, "a malformed string");
    }
    if (memchr(out, '\0', n) != NULL) {
        return fail(b, "a string that holds a NUL");
    }
    *text = out;
    *length = n;
    return true;
}

/* A growable list of the values of one array or object, which moves to
   the memory pool once it is complete. */
struct list {
    const struct json_value **items;
    const char **keys;
    size_t count;
    size_t room;
};

static bool list_add(struct list *l, const char *key,
                     const struct json_value *item)
{
    if (l->count == l->room) {
        size_t room = l->room == 0 ? 8 : l->room * 2;
        const struct json_value **items =
            realloc((void *)l->items, room * sizeof *items);
        const char **keys;
        if (items == NULL) {
            return false;
        }
        l->items = items;
        keys = realloc((void *)l->keys, room * sizeof *keys);
        if (keys == NULL) {
            return false;
        }
        l->keys = keys;
        l->room = room;
    }
    l->items[l->count] = item;
    l->keys[l->count] = key;
    l->count++;
    return true;
}

static void list_move(struct builder *b, struct list *l, struct json_value *v,
                      bool keyed)
{
    v->count = l->count;
    if (l->count > 0) {
        const struct json_value **items =
            arena_alloc(b->arena, l->count * sizeof *items);
        memcpy((void *)items, (void *)l->items, l->count * sizeof *items);
        v->items = items;
        if (keyed) {
            const char **keys = arena_alloc(b->arena, l->count * sizeof *keys);
            memcpy((void *)keys, (void *)l->keys, l->count * sizeof *keys);
            v->keys = keys;
        }
    }
    free((void *)l->items);
    free((void *)l->keys);
}

static const struct json_value *read_value(struct builder *b, int depth);

static bool read_members(struct builder *b, struct json_value *v, int depth)
{
    struct list l = {0};
    bool ok = true;

    if (!anti_rt_json_take(&b->scan, '}')) {
        do {
            const char *key;
            size_t length;
            const struct json_value *item;
            anti_rt_json_space(&b->scan);
            if (!read_string(b, &key, &length)) {
                ok = false;
                break;
            }
            if (!anti_rt_json_take(&b->scan, ':')) {
                ok = fail(b, "no `:` after a member name");
                break;
            }
            item = read_value(b, depth + 1);
            if (item == NULL) {
                ok = false;
                break;
            }
            if (!list_add(&l, key, item)) {
                ok = fail(b, "out of memory");
                break;
            }
        } while (anti_rt_json_take(&b->scan, ','));
        if (ok && !anti_rt_json_take(&b->scan, '}')) {
            ok = fail(b, "an object without its `}`");
        }
    }
    list_move(b, &l, v, true);
    return ok;
}

static bool read_elements(struct builder *b, struct json_value *v, int depth)
{
    struct list l = {0};
    bool ok = true;

    if (!anti_rt_json_take(&b->scan, ']')) {
        do {
            const struct json_value *item = read_value(b, depth + 1);
            if (item == NULL) {
                ok = false;
                break;
            }
            if (!list_add(&l, NULL, item)) {
                ok = fail(b, "out of memory");
                break;
            }
        } while (anti_rt_json_take(&b->scan, ','));
        if (ok && !anti_rt_json_take(&b->scan, ']')) {
            ok = fail(b, "an array without its `]`");
        }
    }
    list_move(b, &l, v, false);
    return ok;
}

static const struct json_value *read_value(struct builder *b, int depth)
{
    struct json_value *v;
    const unsigned char *start;
    int64_t length;

    if (depth > JSON_TREE_DEPTH) {
        fail(b, "values nested too deep");
        return NULL;
    }
    anti_rt_json_space(&b->scan);
    if (b->scan.at >= b->scan.end) {
        fail(b, "the end of the input where a value belongs");
        return NULL;
    }
    v = arena_alloc(b->arena, sizeof *v);
    switch (*b->scan.at) {
    case '{':
        b->scan.at++;
        v->kind = JSON_OBJECT;
        return read_members(b, v, depth) ? v : NULL;
    case '[':
        b->scan.at++;
        v->kind = JSON_ARRAY;
        return read_elements(b, v, depth) ? v : NULL;
    case '"':
        v->kind = JSON_STRING;
        return read_string(b, &v->text, &v->length) ? v : NULL;
    default:
        break;
    }
    if (anti_rt_json_word(&b->scan, "null")) {
        v->kind = JSON_NULL;
        return v;
    }
    if (anti_rt_json_word(&b->scan, "true")) {
        v->kind = JSON_BOOL;
        v->truth = true;
        return v;
    }
    if (anti_rt_json_word(&b->scan, "false")) {
        v->kind = JSON_BOOL;
        return v;
    }
    if (!anti_rt_json_number(&b->scan, &start, &length) ||
        !anti_rt_json_valid_number(start, length)) {
        fail(b, "no JSON value");
        return NULL;
    }
    {
        char *text = arena_alloc(b->arena, (size_t)length + 1);
        memcpy(text, start, (size_t)length);
        v->kind = JSON_NUMBER;
        v->text = text;
        v->length = (size_t)length;
    }
    return v;
}

bool json_read(const unsigned char *bytes, size_t length,
               struct json_tree *out, char *error, size_t error_size)
{
    struct builder b;

    memset(out, 0, sizeof *out);
    b.scan.at = bytes;
    b.scan.end = bytes + length;
    b.start = bytes;
    b.arena = &out->arena;
    b.error = error;
    b.error_size = error_size;
    out->root = read_value(&b, 0);
    if (out->root == NULL) {
        json_free(out);
        return false;
    }
    anti_rt_json_space(&b.scan);
    if (b.scan.at != b.scan.end) {
        fail(&b, "bytes after the value");
        json_free(out);
        return false;
    }
    return true;
}

void json_free(struct json_tree *tree)
{
    arena_free(&tree->arena);
    tree->root = NULL;
}

const struct json_value *json_get(const struct json_value *value,
                                  const char *key)
{
    size_t i;

    if (value == NULL || value->kind != JSON_OBJECT) {
        return NULL;
    }
    for (i = 0; i < value->count; i++) {
        if (strcmp(value->keys[i], key) == 0) {
            return value->items[i];
        }
    }
    return NULL;
}

const char *json_string(const struct json_value *value)
{
    return value != NULL && value->kind == JSON_STRING ? value->text : NULL;
}

const char *json_member_string(const struct json_value *value,
                               const char *key)
{
    return json_string(json_get(value, key));
}

bool json_integer(const struct json_value *value, int64_t *out)
{
    return value != NULL && value->kind == JSON_NUMBER &&
           anti_rt_json_integer((const unsigned char *)value->text,
                                (int64_t)value->length, out);
}

bool json_member_true(const struct json_value *value, const char *key)
{
    const struct json_value *v = json_get(value, key);

    return v != NULL && v->kind == JSON_BOOL && v->truth;
}

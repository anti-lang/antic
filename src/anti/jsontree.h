#ifndef ANTI_JSONTREE_H
#define ANTI_JSONTREE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "arena.h"

/* A tree of a JSON document, built over the scanner of src/rt/json.c. anti
   bind reads the description of raylib and the AST that clang dumps
   with it. */
enum json_kind {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
};

struct json_value {
    enum json_kind kind;
    bool truth;                 /* JSON_BOOL */
    const char *text;           /* the decoded string, or the number as written */
    size_t length;              /* of text */
    const char **keys;          /* JSON_OBJECT: the member names, in order */
    const struct json_value **items;    /* the elements or the member values */
    size_t count;
};

struct json_tree {
    struct arena arena;
    const struct json_value *root;
};

/* How deep objects and arrays may nest in a document json_read takes.
   The AST of raymath nests 41 levels, and an expression of many operands
   nests one level per operand. */
#define JSON_TREE_DEPTH 1024

/* Read one JSON value that fills the bytes, up to white space. Every
   string is decoded, with a NUL after it, and every number is checked
   against the grammar. Returns false with a message in error that names
   the line and the column, and out then holds no allocation. A string
   that holds a NUL byte is refused, since a name of C holds none. The
   caller frees a tree read with json_free. */
bool json_read(const unsigned char *bytes, size_t length,
               struct json_tree *out, char *error, size_t error_size);

void json_free(struct json_tree *tree);

/* The value of the member key of an object, or NULL when value is no
   object or has no such member. */
const struct json_value *json_get(const struct json_value *value,
                                  const char *key);

/* The string of the member key, or NULL. */
const char *json_member_string(const struct json_value *value,
                               const char *key);

/* Whether value is a number that is an integer an int64_t holds, which
   then goes to out. */
bool json_integer(const struct json_value *value, int64_t *out);

/* Whether the member key is true. */
bool json_member_true(const struct json_value *value, const char *key);

#endif

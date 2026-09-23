#ifndef ANTI_BINDMODEL_H
#define ANTI_BINDMODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "arena.h"
#include "text.h"

/* The declarations of a C API that anti bind writes as an Anti module.
   The reader of raylib_api.json and the reader of clang's AST fill it,
   and one writer turns it into the binding, the shim and the probe. It
   records C types and never a size: antic lays out every struct. */

enum bind_kind {
    BIND_VOID,
    BIND_SCALAR,        /* name is the Anti type, as c_int or u8 */
    BIND_BOOL,
    BIND_POINTER,       /* to */
    BIND_ARRAY,         /* to, length */
    BIND_FUNCTION,      /* to is the result, params, variadic */
    BIND_RECORD,        /* record */
    BIND_ENUM,          /* name */
    BIND_VALIST,
    BIND_OPAQUE,        /* a struct or union the API never defines */
    BIND_UNSUPPORTED    /* name is the C spelling */
};

struct bind_record;

struct bind_type {
    enum bind_kind kind;
    const char *name;
    const struct bind_type *to;
    int64_t length;
    const struct bind_type **params;
    size_t param_count;
    bool variadic;
    struct bind_record *record;
};

struct bind_field {
    const char *name;           /* NULL for an unnamed bitfield */
    const struct bind_type *type;
    const char *c_type;         /* the C spelling, NULL where none is known */
    int64_t bits;               /* the width of a bitfield, or -1 */
};

struct bind_record {
    const char *name;           /* the Anti name */
    const char *c_name;         /* how C names the type: `Vector2`, `struct node` */
    bool is_union;
    bool complete;
    bool packed;
    int64_t align;              /* raised alignment, or 0 */
    struct bind_field *fields;
    size_t field_count;
    const char *doc;
    bool bound;                 /* written into the module */
};

struct bind_value {
    const char *name;
    int64_t value;
    const char *doc;
};

struct bind_enum {
    const char *name;
    const char *c_name;
    const char *base;           /* NULL for c_int */
    struct bind_value *values;
    size_t value_count;
    const char *doc;
};

/* How a function reaches a symbol. A `static` function of a header gets a
   wrapper that calls it under another name. A C99 `inline` definition
   gets an external definition from an `extern` declaration. */
enum bind_shim { SHIM_NONE, SHIM_STATIC, SHIM_INLINE };

struct bind_param {
    const char *name;
    const struct bind_type *type;
    const char *c_type;         /* the C spelling, which the shim writes */
};

struct bind_function {
    const char *name;
    const struct bind_type *result;
    const char *c_result;
    struct bind_param *params;
    size_t param_count;
    bool variadic;
    enum bind_shim shim;
    const char *doc;
    bool bound;
};

struct bind_eval;

struct bind_const {
    const char *name;
    const char *type;           /* the Anti type */
    const char *value;          /* the Anti expression */
    const struct bind_record *record;   /* of a struct literal, or NULL */
    const struct bind_eval *eval;       /* the value it was written from */
    const char *doc;
    bool bound;
};

/* A growable array of pointers, for the declarations of each kind. */
struct bind_list {
    void **items;
    size_t count;
    size_t room;
};

struct bind_module {
    struct arena arena;
    const char *module;         /* anti.raylib */
    const char *library;        /* raylib, the last segment */
    const char *source;         /* the name of the file it was read from */
    const char *header;         /* the header the shim and the probe include */
    struct bind_list records;
    struct bind_list enums;
    struct bind_list functions;
    struct bind_list consts;
    struct bind_list defines;   /* NAME or NAME=VALUE, which the shim repeats */
    size_t warnings;
};

void bind_list_add(struct bind_list *list, void *item);

/* A copy of s in the memory pool of b. */
const char *bind_strdup(struct bind_module *b, const char *s);
const char *bind_strndup(struct bind_module *b, const char *s, size_t n);

/* Print a warning that names the source of the binding. */
void bind_warn(struct bind_module *b, const char *format, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;

/* A type in the memory pool of b. */
struct bind_type *bind_type_new(struct bind_module *b, enum bind_kind kind);

/* The scalar Anti type of a C type name, `unsigned int` or `uint8_t`, or
   NULL when the name is none of the fixed mappings. */
const struct bind_type *bind_known_name(struct bind_module *b, const char *name);

/* The names a C type spelling refers to, which a reader resolves. */
struct bind_names {
    /* The type a typedef name stands for, or NULL. */
    const struct bind_type *(*typedef_named)(void *context, const char *name);
    /* The record of a tag, or NULL. */
    struct bind_record *(*record_named)(void *context, const char *tag,
                                        bool is_union);
    /* Whether the enum of a tag exists, whose Anti name goes to out. */
    const char *(*enum_named)(void *context, const char *tag);
    void *context;
};

/* The type that a C spelling names, as clang prints a type and as
   raylib_api.json writes one: `const char *`, `float[4]`,
   `void (*)(int, const char *)`. Qualifiers go, since Anti has none.
   Returns NULL when the spelling does not parse. */
const struct bind_type *bind_parse_type(struct bind_module *b, const char *c,
                                        const struct bind_names *names);

/* Whether name is a word the lexer of Anti reserves. */
bool bind_is_keyword(const char *name);

/* The last segment of a module path. */
const char *bind_last_segment(const char *module);

/* The frameworks of Apple's SDK that a library links against on macOS,
   from the table of anti bind. Returns the number of names. */
size_t bind_frameworks(const char *library, const char *const **names);

/* Decide what the module can hold. A record is bound when the type of
   every field has an Anti spelling. A function or a constant is bound
   when every type it names has one. Each declaration left out gets a warning
   that says why. */
void bind_settle(struct bind_module *b);

/* Write the binding module, the shim and the probe. */
void bind_write_module(const struct bind_module *b, struct text *out);
bool bind_needs_shim(const struct bind_module *b);
void bind_write_shim(const struct bind_module *b, struct text *out);
void bind_write_probe_c(const struct bind_module *b, struct text *out);
void bind_write_probe_anti(const struct bind_module *b, struct text *out);

/* Read raylib_api.json, or the AST and the macros of a header through
   clang, into b. Both return false after a message. */
bool bind_read_api(struct bind_module *b, const unsigned char *bytes,
                   size_t length);

struct bind_clang_request {
    const char *header;
    const char *runtime;
    const char *target;         /* NULL for the host */
    const char *const *includes;
    size_t include_count;
    const char *const *defines;
    size_t define_count;
};

bool bind_read_clang(struct bind_module *b, const struct bind_clang_request *r);

#endif

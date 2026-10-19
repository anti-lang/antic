#ifndef ANTI_BINDMODEL_H
#define ANTI_BINDMODEL_H

/* The model of a binding and the functions the files of anti bind give
   each other. The model holds the declarations of a C API that anti bind
   writes as an Anti module. The reader of raylib_api.json in bindapi.c
   and the reader of clang's AST in bindclang.c fill it, and the writer of
   bindwrite.c turns it into the binding, the shim and the probes. It
   records C types and never a size: antic lays out every struct.
   bindmodel.c holds its memory, its warnings, the words of Anti and the
   libraries each bundled library links, and bindtype.c the parser of a
   C type spelling. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "arena.h"
#include "attributes.h"
#include "text.h"

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
    /* The reader left it out and gave its warning, as for a `static`
       function without a body. */
    bool left_out;
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
    const char *const *defines; /* NAME or NAME=VALUE, which the shim repeats */
    size_t define_count;
    size_t warnings;
};

/* Append item to list, whose array grows on the heap and exits through
   files_out_of_memory when it cannot. The item is not copied. The caller
   frees the array, items, with free once the list is done, and owns
   nothing else of it: the items live in the memory pool of a module. */
void bind_list_add(struct bind_list *list, void *item);

/* A copy of s, NUL-terminated, in the memory pool of b. */
const char *bind_strdup(struct bind_module *b, const char *s);

/* A copy of the first n bytes of s in the memory pool of b, with a NUL
   after them. s need not end within n bytes. */
const char *bind_strndup(struct bind_module *b, const char *s, size_t n);

/* Print a warning that names the source of the binding. */
void bind_warn(struct bind_module *b, const char *format, ...)
    ATTRIBUTE_PRINTF(2, 3);

/* A type in the memory pool of b. */
struct bind_type *bind_type_new(struct bind_module *b, enum bind_kind kind);

/* The names a C type spelling refers to, which a reader resolves. */
struct bind_names {
    /* The type a typedef name stands for, or NULL. */
    const struct bind_type *(*typedef_named)(void *context, const char *name);
    /* The record of a tag, or NULL. */
    struct bind_record *(*record_named)(void *context, const char *tag,
                                        bool is_union);
    /* The Anti name of the enum of a tag, or NULL when there is none. */
    const char *(*enum_named)(void *context, const char *tag);
    void *context;
};

/* The type that a C spelling names, as clang prints a type and as
   raylib_api.json writes one: `const char *`, `float[4]`,
   `void (*)(int, const char *)`. Qualifiers go, since Anti has none.
   Returns NULL when the spelling does not parse. */
const struct bind_type *bind_parse_type(struct bind_module *b, const char *c,
                                        const struct bind_names *names);

/* Whether name is one identifier of C: a letter or `_`, then letters,
   digits and `_`. Otherwise say that b names the kind with it, for each
   reader to refuse its input after. The binding writes every name the
   readers take into Anti and C as it stands. */
bool bind_named(const struct bind_module *b, const char *kind,
                const char *name);

/* Whether name, an identifier of C, is a word of Anti: one the lexer of
   Anti reads as no identifier, as `fn`, `type` and `none`. */
bool bind_is_anti_word(const char *name);

/* Append the name of C as the binding writes it in Anti. A word of Anti
   takes a trailing `_`, which changes no layout and no call. A record, an
   enum, an enumerator, a constant, a field and a parameter take it. */
void bind_anti_name(const char *name, struct text *out);

/* The frameworks of Apple's SDK that a library links against on macOS,
   from the table of anti bind. Returns the number of names. */
size_t bind_frameworks(const char *library, const char *const **names);
/* The libraries of the glibc sysroot that library needs on Linux, as
   bind_frameworks gives the frameworks. */
size_t bind_linux_libraries(const char *library, const char *const **names);

/* Decide what the module can hold. A record is bound when the type of
   every field has an Anti spelling. A function or a constant is bound
   when every type it names has one. Each declaration left out gets a warning
   that says why. */
void bind_settle(struct bind_module *b);

/* Append the binding module to out: its `//!` lines, the `link` lines of
   the bundled library, the constants that bind_settle bound, every enum, and
   the bound records and functions, each kind in the order the reader
   added it. */
void bind_write_module(const struct bind_module *b, struct text *out);

/* Whether a bound function of b needs the shim: an inline or `static`
   function of the header, which has no symbol of its own. */
bool bind_needs_shim(const struct bind_module *b);

/* Append the shim to out: the C file that gives every inline and
   `static` function a bound function of b names a symbol. It repeats the
   defines of b before it includes the header. Call it when
   bind_needs_shim says so. */
void bind_write_shim(const struct bind_module *b, struct text *out);

/* Append the ABI probe in C to out. For every bound record that C can
   name it prints the size, the alignment and the bytes of an object
   cleared and given one field, through the header of b. */
void bind_write_probe_c(const struct bind_module *b, struct text *out);

/* Append the ABI probe in Anti to out, which imports the binding and
   prints what bind_write_probe_c prints. */
void bind_write_probe_anti(const struct bind_module *b, struct text *out);

/* Read raylib_api.json, the length bytes at bytes, into b. Returns false
   after a message for a text that is no JSON or a declaration it cannot
   read. */
bool bind_read_api(struct bind_module *b, const unsigned char *bytes,
                   size_t length);

/* What bind_read_clang reads: the header, the runtime archive that holds
   the sysroot of its target, and the -I and -D options of the command. */
struct bind_clang_request {
    const char *header;
    const char *runtime;
    const char *target;         /* NULL for the host */
    const char *const *includes;
    size_t include_count;
    const char *const *defines;
    size_t define_count;
};

/* Read the AST and the macros of the header of r into b through clang.
   clang reads it for the target of r against the headers of that
   target's sysroot in the runtime archive. Returns false after a message
   for an unknown target, a request without runtime or a runtime archive
   that holds no such sysroot, a clang it cannot run or of another major
   version, and an AST it cannot read. */
bool bind_read_clang(struct bind_module *b, const struct bind_clang_request *r);

#endif

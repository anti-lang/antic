#ifndef ANTIC_SEMA_H
#define ANTIC_SEMA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "arena.h"
#include "ast.h"
#include "diagnostic.h"
#include "text.h"
#include "types.h"

enum symbol_kind {
    SYMBOL_LOCAL,
    SYMBOL_PARAM,
    SYMBOL_CONST,
    SYMBOL_FN,
    SYMBOL_EXTERN_FN,
    SYMBOL_STRUCT,
    SYMBOL_MODULE,  /* the name an import declares */
    SYMBOL_GLOBAL   /* a static atomic field of a class */
};

enum const_kind {
    CONST_INT,
    CONST_FLOAT,
    CONST_BOOL,
    CONST_CHAR,
    CONST_NULL,
    CONST_TEXT,     /* a string or byte string literal */
    CONST_ARRAY,
    CONST_STRUCT,
    CONST_SYMBOLIC  /* an integer or bool computed from size_of */
};

/* The value of a constant, computed at compile time. */
struct const_value {
    enum const_kind kind;
    struct type *type;
    union {
        uint64_t integer;           /* the bits of the value in its type */
        double floating;
        bool boolean;
        uint32_t character;
        struct token_text text;
        struct {
            struct const_value *items;  /* elements, or fields in order */
            size_t count;
        } aggregate;
        const struct symbolic *symbolic;
    } as;
};

enum eval_state { EVAL_NONE, EVAL_BUSY, EVAL_DONE };

struct interface;

/* A dependency in the package header of a library file. */
struct package_dependency {
    const char *name;               /* a module path */
    const char *constraint;         /* a version constraint, such as ^1.2 */
    const char *url;                /* the repository */
};

/* The version of a package whose build names none. It stands in the
   package header of a library file and in every class descriptor. */
#define PACKAGE_VERSION_DEFAULT "0.0.0"

/* The package header of a library file. antic alone writes the module
   path as the name, version 0.0.0 and empty licence fields. */
struct package {
    const char *name;
    const char *version;
    const struct package_dependency *dependencies;
    size_t dependency_count;
    const char *license;            /* an SPDX identifier */
    const char *license_text;       /* the full text */
    const char *const *attribution; /* lines copied verbatim */
    size_t attribution_count;
};

/* The default of one parameter: a constant, `here`, or neither when the
   parameter has none. */
struct param_default {
    const struct const_value *value;
    bool here;                      /* the position of each call */
};

struct symbol {
    enum symbol_kind kind;
    struct name name;
    struct pos pos;
    struct type *type;
    struct item *item;              /* a module-level item */
    struct stmt *stmt;              /* a const in a block */
    struct const_value *value;      /* SYMBOL_CONST */
    enum eval_state state;          /* SYMBOL_CONST */
    bool address_taken;             /* SYMBOL_LOCAL, SYMBOL_PARAM */
    bool read_only;                 /* the variable of a `for` */
    bool variadic;                  /* SYMBOL_EXTERN_FN */
    bool worker;                    /* SYMBOL_FN written `worker fn` */
    bool may_fail;                  /* SYMBOL_FN written `may fail` */
    bool internal;                  /* `internal`: the package alone sees it */
    bool caught;                    /* the error a `catch` binds */
    bool atomic;                    /* a local declared `atomic T` */
    /* The error a `catch` binds: the loops its handler stands in, the
       function it moved into, and whether a `defer` or an `undo` of the
       handler names it. */
    int caught_loops;
    const struct symbol *moved_into;
    bool deferred;
    /* A `keep own` parameter that moved into an owner, and where. */
    bool snapshot_moved;
    struct pos snapshot_move;
    const struct name *params;      /* a function of an interface */
    /* DESIGN: the defaults of a function's parameters, one per parameter
       the program writes, `self` included, in the order of the type.
       NULL when no parameter has one. They belong to the function and
       not to its type, so a call through a function value passes every
       argument. */
    const struct param_default *defaults;
    size_t default_count;
    /* DESIGN: the `own` parameters of a function, which take over the
       object passed to them. One flag per parameter, `self` included, as
       for the defaults, and NULL when none is `own`. They belong to the
       function and not to its type, so a call through a function value
       moves nothing. */
    const bool *owned;
    size_t owned_count;
    /* DESIGN: the fields of a local or parameter of type Flags that its
       function reads. Each field has one bit, in the order of the struct.
       A use of the whole value reads all four. The flags form computes
       these alone, so a field that nothing reads costs nothing. */
    uint8_t flags_read;

    /* DESIGN: a local or a parameter belongs to the frame of one
       function. An anonymous function that names one of another frame
       captures it. depth counts the blocks around its declaration. A
       closure assigned to another local of the frame is checked against
       it. A local that holds a closure names the anonymous function for
       the messages. It names the captured variable declared deepest as
       well, since the closure is held in no block outside that one. */
    const struct item *frame;
    int depth;
    const struct item *closure;
    const struct symbol *holds;
    /* An export item, whose function has the C symbol of its name. */
    bool exported;
    struct doc_text doc;            /* the /// text of a pub item */
    const struct interface *home;   /* the library of an imported item */
    uint32_t ir;                    /* set by lowering, see lower.h */
};

/* flags_read of a Flags value whose every field is read. */
#define FLAGS_READ_ALL 15

/* What other modules see of a module: its imports and its pub items. A
   library file stores it, and an import reads it. */
struct interface {
    struct package package;
    const char *doc;                /* the `//!` text of the module */
    const char *module;
    const char **imports;           /* module names, not aliases */
    size_t import_count;
    struct symbol **items;          /* pub fn, extern fn, struct and const */
    size_t item_count;
    const char **frameworks;        /* of its `link framework` lines */
    size_t framework_count;
    const char **linux_libraries;   /* of its `link linux` lines */
    size_t linux_library_count;
};

/* Check one module against the rules of chapter 2: resolve every name,
   give every expression its type and compute every constant. The method
   syntax v.f(args) and qualified names are rewritten into plain names.
   libraries holds the interface of every loaded library. program says
   that the build writes a program and not a library, which decides the
   report of an abstract class that nothing fills. Returns true when no
   error occurred. */
bool sema_check(struct module *module, const char *module_name,
                const char *package,
                const struct interface *const *libraries,
                size_t library_count, struct types *types,
                struct arena *arena, struct diagnostics *diags,
                bool program);

/* The `fallthrough;` that ends the body of a switch arm, the last
   statement of its block, or NULL when the arm ends otherwise. */
struct stmt *sema_arm_fallthrough(const struct stmt *body);

/* Whether a field without a written default takes `T { }`: an inline
   class value whose class a literal may write with no field named. */
bool sema_field_takes_literal(const struct struct_field *f);

/* The doc warnings of `anti check`: markup outside the doc subset, a
   backtick name that resolves nowhere, a `pub` item with a `//#` note and
   no `///` comment, a doc comment that documents nothing and, with
   undocumented set, every `pub` item without a `///` comment. module_name
   is the module path of the compilation, libraries holds the interface of
   every loaded library and types holds the root, so a name resolves
   against all three. */
void sema_doc_warnings(const struct module *module, const char *module_name,
                       const struct interface *const *libraries,
                       size_t library_count, struct types *types,
                       bool undocumented, struct diagnostics *diags);

/* Fill out with the interface of a checked module. The items are new
   symbols whose home is out. */
void sema_interface(const struct module *module, const char *module_name,
                    struct arena *arena, struct interface *out);

/* The syntax tree as ast_dump prints it, with the type of every
   expression, parameter, variable and function on the right. */
void ast_dump_typed(struct text *out, const struct module *module);

#endif

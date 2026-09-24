#ifndef ANTIC_SEMA_CHECKER_H
#define ANTIC_SEMA_CHECKER_H

/* The state of the checker and the functions its files share. sema.c
   declares the items of a module and runs the passes over them.
   sema_expr.c checks expressions, sema_call.c calls and members, and
   sema_const.c evaluates constants. sema_stmt.c checks statements and
   function bodies and walks what a worker reaches. sema_export.c checks
   what crosses to C and the doc comments. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "arena.h"
#include "attributes.h"
#include "ast.h"
#include "diagnostic.h"
#include "lexer.h"
#include "sema.h"
#include "text.h"
#include "types.h"

struct scope_entry {
    struct name name;
    struct symbol *symbol;
};

/* DESIGN: narrowing is per block. A block that a check opened records
   the names the check proved are not `none`. The record dies with the
   block, so after it the name is `?*T` again. An assignment to the name
   puts the declared type back in the record that holds it, wherever that
   block is. The value the check proved is gone. */
struct narrowing {
    struct symbol *symbol;
    struct type *type;
};

struct scope {
    struct scope *parent;
    struct scope_entry *entries;
    size_t count;
    size_t capacity;
    struct narrowing *narrowed;
    size_t narrowed_count;
    size_t narrowed_capacity;
};

/* One `sync` that the statement being checked stands in, innermost
   first. */
struct held_mutex {
    const struct expr *mutex;
    const struct held_mutex *outer;
};

struct checker {
    struct types *types;
    struct arena *arena;
    struct diagnostics *diags;
    struct module *module;
    struct name module_name;
    const char *package;        /* the package this compilation builds */
    const struct interface *const *libraries;
    size_t library_count;
    struct scope module_scope;
    struct scope *scope;
    struct item *function;      /* the function whose body is checked */
    /* The item whose nested types, and those of the classes around it,
       are named as written. NULL at module level. */
    const struct item *within;
    int loop_depth;
    struct stmt *fallthrough;   /* the one that ends the arm checked now */
    bool target_sized;          /* a symbolic array length is allowed */
    bool atomic_place;          /* the place of an atomic operation */
    bool program;               /* the build writes a program, not a library */
    struct type *yields;        /* the type `yield` gives in a handler */
    int handler_depth;          /* above 0, a `yield` has a place to go */
    struct block *try_block;    /* the body of the enclosing `try` block */
    struct type *error_type;    /* `*Error` of the first failing call */
    bool saw_fail;              /* the body holds a `fail` or a `try` */
    const struct expr *top_call; /* the first statement's call, or NULL */
    int quiet;                  /* above 0, errors are not reported */
    int deferring;              /* above 0, a `defer` or `undo` is checked */
    const struct expr *field_base; /* the base of the field checked now */
    const struct held_mutex *held; /* the `sync` blocks around it */
    int const_depth;            /* constants evaluated inside each other */
    bool ok;
};

/* The most names one condition proves. */
#define PROVED_MAX 8

/* Copy the spelling of an operator without its backticks into buffer.
   The longest is `mul_high`. */
#define OP_TEXT 16

/* A set of pointers, open addressed and at most half full. The owner
   frees slots with free(). */
struct ptr_set {
    const void **slots;
    size_t capacity;
    size_t count;
};

/* sema.c */

int64_t sema_signed_bits(uint64_t v);
void sema_format_to(char *out, size_t size, const char *format, ...)
    ATTRIBUTE_PRINTF(3, 4);
void sema_error_at(struct checker *c, struct pos pos, const char *format,
                   ...)
    ATTRIBUTE_PRINTF(3, 4);
const char *sema_tn(const struct type *t);
struct type *sema_builtin(struct checker *c, enum type_kind kind);
bool sema_is_error(const struct type *t);
bool sema_same_name(const struct name *a, const struct name *b);
bool sema_name_is(const struct name *a, const char *text);
struct symbol *sema_scope_find_local(const struct scope *s,
                                     const struct name *name);
struct symbol *sema_lookup(const struct checker *c, const struct name *name);
const struct item *sema_within(const struct item *it);
struct symbol *sema_module_find(const struct checker *c,
                                const struct name *name);
const struct interface *sema_find_library(const struct checker *c,
                                          const struct name *module);
struct symbol *sema_library_item(const struct checker *c,
                                 const struct interface *lib,
                                 const struct name *name);
struct symbol *sema_declare(struct checker *c, enum symbol_kind kind,
                            const struct name *name, struct pos pos,
                            const char *duplicate_message);
void sema_warn_catch_shadow(struct checker *c, const struct name *name,
                            struct pos pos);
void sema_enter_scope(struct checker *c, struct scope *s);
void sema_leave_scope(struct checker *c, struct scope *s);
struct type *sema_narrowed_type(const struct checker *c,
                                const struct symbol *sym);
void sema_narrow(struct checker *c, struct symbol *sym, struct type *t);
void sema_end_narrowing(struct checker *c, const struct symbol *sym);
struct type *sema_array_of(struct checker *c, struct expr *e,
                           struct type *element);
struct type *sema_imported_struct(struct checker *c,
                                  const struct name *module,
                                  const struct name *name, struct pos pos);
bool sema_refuses_half(struct checker *c, struct pos pos,
                       const struct type *t);
struct type *sema_chan_element(struct checker *c, struct type_expr *t);
struct type *sema_resolve_type(struct checker *c, struct type_expr *t);
struct symbol *sema_std_item(struct checker *c, const struct name *module,
                             const struct name *name, bool own);
struct type *sema_error_class(struct checker *c, struct pos pos);
struct type *sema_location_type(struct checker *c, struct pos pos);
struct type *sema_deserialize_type(struct checker *c, struct pos pos,
                                   const struct type *declared);
bool sema_check_object_from(struct checker *c, struct expr *e,
                            const char *what);

/* sema_expr.c */

bool sema_is_place(const struct expr *e);
void sema_mark_address_taken(struct expr *e);
bool sema_spell(struct text *out, const struct expr *e);
struct type *sema_usable_pointer(struct checker *c, const struct expr *e,
                                 struct type *t);
bool sema_require(struct checker *c, struct expr *e, struct type *got,
                  struct type *expected);
bool sema_simd_numeric(const struct type *lane);
const char *sema_op_text(enum token_kind op, char buffer[OP_TEXT]);
bool sema_operator_named(const struct name *name);
struct type *sema_check_binary(struct checker *c, struct expr *e,
                               struct type *expected);
bool sema_descends_from(const struct type *a, const struct type *b);
extern const struct name sema_hidden_value;
const char *sema_format_name(const struct expr *e);
struct type *sema_check_expr(struct checker *c, struct expr *e,
                             struct type *expected);
struct type *sema_check_storage(struct checker *c, struct expr *e);

/* sema_call.c */

struct type *sema_struct_of(struct type *t);
const struct struct_field *sema_find_field(const struct type *s,
                                           const struct name *name);
struct expr *sema_new_node(struct checker *c, enum expr_kind kind,
                           struct pos pos);
bool sema_refuse_abstract_value(struct checker *c, struct pos pos,
                                const char *what, const struct type *t);
const struct type *sema_inherited(const struct type *t);
const struct type *sema_declaring_class(const struct item *m);
const struct type *sema_checking_class(const struct checker *c);
bool sema_singleton_type(const struct type *t);
struct symbol *sema_method_symbol(const struct checker *c,
                                  const struct type *s,
                                  const struct name *name);
bool sema_implemented_in(const struct type *t, const struct type *iface);
struct symbol *sema_null_pointer_maker(struct checker *c, struct pos pos);
struct symbol *sema_error_maker(struct checker *c, struct pos pos);
void sema_resolve_origin(struct checker *c, struct stmt *s);
struct type *sema_caught_error(struct checker *c, struct type *result);
void sema_declare_caught(struct checker *c, struct handler *h,
                         struct type *t);
bool sema_in_failing_function(const struct checker *c);
void sema_refuse_escaping_error(struct checker *c, const struct expr *e);
struct type *sema_check_sync_op(struct checker *c, struct expr *e);
const struct type *sema_interface_named(struct checker *c,
                                        const struct name *qualifier,
                                        const struct name *name,
                                        struct pos pos);
struct type *sema_check_call(struct checker *c, struct expr *e,
                             struct type *expected);
struct item *sema_find_member(const struct type *t, const struct name *name);
struct name sema_dotted(struct checker *c, const struct name *a,
                        const struct name *b);
const struct name *sema_case_name(const struct type *v, size_t index);
struct type *sema_variant_literal(struct checker *c, struct expr *e,
                                  struct type *v, const struct name *name);
struct type *sema_check_field(struct checker *c, struct expr *e);
size_t sema_chain_fields(const struct type *t, struct struct_field *out);
bool sema_check_field_inits(struct checker *c, struct expr *e,
                            struct field_init *inits, size_t count,
                            const struct struct_field *fields,
                            size_t field_count, const char *type_name,
                            bool skip_missing);
struct type *sema_check_parallel(struct checker *c, struct expr *e);
struct type *sema_check_dispatch(struct checker *c, struct expr *e);
struct type *sema_check_join(struct checker *c, struct expr *e);

/* sema_const.c */

bool sema_undefined_on_constants(struct checker *c, struct expr *e,
                                 const struct type *result);
bool sema_eval_const(struct checker *c, struct expr *e,
                     struct const_value *out);
bool sema_const_symbol(struct checker *c, struct symbol *sym,
                       struct pos use);

/* sema_stmt.c */

bool sema_ptr_set_add(struct ptr_set *s, const void *p);
void sema_walk_worker(struct checker *c, const struct item *worker);
bool sema_fills(const struct type *t, const struct type *abstract);
bool sema_filled_somewhere(const struct checker *c, const struct type *t);
size_t sema_proved_names(const struct expr *cond, bool want_true,
                         struct symbol **out, size_t count);
struct type *sema_proved_type(struct checker *c, const struct symbol *sym);
bool sema_type_owns(const struct type *t);
void sema_refuse_owned_copy(struct checker *c, const struct expr *value,
                            struct type *t);
void sema_check_block(struct checker *c, struct block *b);
void sema_check_function(struct checker *c, struct item *it);
void sema_check_main(struct checker *c, struct item *it);
void sema_check_test_block(struct checker *c, struct item *it);

/* sema_export.c */

void sema_check_extern_fn(struct checker *c, struct item *it);
void sema_check_export(struct checker *c, struct item *it);

#endif

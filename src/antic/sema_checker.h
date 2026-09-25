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
    int depth;                  /* the blocks around it, 0 for a module */
};

/* A field of a concurrent class of the module that a write outside
   `construct` made none of guarded, atomic or fixed. */
struct written_field {
    const struct struct_field *field;
};

/* One `sync` that the statement being checked stands in, innermost
   first. */
struct held_mutex {
    const struct expr *mutex;
    const struct held_mutex *outer;
};

/* Where a `lent` pointer is refused, for the message. LENT_TO_C is an
   argument of an `extern fn`, where it is not refused. */
enum lent_use { LENT_STORED, LENT_RETURNED, LENT_PASSED, LENT_TO_C };

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
    /* Above 0, a function type holds one C function pointer in every
       place, a parameter of it included. The signature of an `extern fn`
       is checked so. */
    int plain_fns;
    /* Set while the result of an `operator fn value` resolves, where a
       part of a tuple may be written `lent *T`. */
    bool value_result;
    /* What a `lent` pointer refused where it stands would be: stored,
       returned or passed on. The message of the refusal names it. */
    enum lent_use lent_use;
    /* The function whose signature is resolved now, whose type
       parameters its types name. NULL outside a signature. */
    const struct item *signature;
    /* The callee of the call checked now, which may name a generic
       function without its arguments. */
    const struct expr *callee;
    /* The copies named before the functions had their types, whose
       constraints are checked once they do. */
    struct pending_check *pending;
    size_t pending_count;
    size_t pending_capacity;
    bool pending_done;
    /* The copies being filled, one inside another, and whether a chain
       too deep was refused. */
    int copy_depth;
    bool copy_refused;
    const struct type *copy_root;   /* the generic the chain began with */
    /* The fields of concurrent classes that the module writes after
       `construct`, reported at their declarations at the end. */
    struct written_field *written;
    size_t written_count;
    size_t written_capacity;
    bool ok;
};

/* A copy of a generic whose arguments are checked against the
   constraints of type parameter param once every function has its type. */
struct pending_check {
    struct type *type;
    const struct type *param;
    struct name generic;
    struct pos pos;
};

/* What a call gives the inference of a generic. It holds the type
   arguments written after the callee's name, the expression that wrote
   them and the name as written. It holds the type before the `.` of
   `List<int>.new()` or the type of the receiver. It has one slot per
   argument for the type of each argument checked while inferring. */
struct generic_call {
    struct type_expr *const *written;
    size_t count;
    const struct expr *written_at;
    const struct name *name;
    struct type *owner;
    struct type **prechecked;
};

/* The most names one condition proves. */
#define PROVED_MAX 8

/* Copy the spelling of an operator without its backticks into buffer.
   The longest is `mul_high`. */
#define OP_TEXT 16

/* The language hooks of the operator table. `for x in e` calls the
   first three, `e[i]` and `e[i] = v` the next two, and a hashing
   collection `hash`. */
#define LANG_HOOK_ITER "iter"
#define LANG_HOOK_NEXT "next"
#define LANG_HOOK_VALUE "value"
#define LANG_HOOK_INDEX "index"
#define LANG_HOOK_SET_INDEX "set_index"
#define LANG_HOOK_HASH "hash"
/* Not a hook: the function every iterator has, which collects it. */
#define LANG_HOOK_TO_SLICE "to_slice"

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
/* A safety check at pos, which `unchecked(name, "reason")` overrules. It
   stops the build unless a clause covers it, so it leaves c->ok. */
void sema_check_at(struct checker *c, enum diag_name name, struct pos pos,
                   const char *format, ...)
    ATTRIBUTE_PRINTF(4, 5);
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
bool sema_direct_item(const struct symbol *sym);
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
struct type *sema_param_form(struct checker *c, struct type *t, bool keep,
                             bool concurrent, bool owned, struct pos pos);
struct type *sema_lent_form(struct checker *c, struct type *t, bool lent,
                            bool owned, struct pos pos);
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
void sema_mark_address_taken(struct checker *c, struct expr *e);
bool sema_spell(struct text *out, const struct expr *e);
struct type *sema_usable_pointer(struct checker *c, const struct expr *e,
                                 struct type *t);
struct type *sema_whole_optional(struct type *t, struct expr *e);
void sema_refuse_lent_tuple(struct checker *c, const struct expr *e);
bool sema_require(struct checker *c, struct expr *e, struct type *got,
                  struct type *expected);
bool sema_simd_numeric(const struct type *lane);
const char *sema_op_text(enum token_kind op, char buffer[OP_TEXT]);
bool sema_operator_named(const struct name *name);
struct symbol *sema_hook(struct checker *c, struct type *t, const char *text);
struct type *sema_member_type_in(struct checker *c, struct type *fn,
                                 const struct item *owner, struct type *s);
struct symbol *sema_operator_symbol(struct checker *c, struct type *t,
                                    const char *text);
bool sema_is_iterator(struct checker *c, struct type *t);
struct expr *sema_hook_call(struct checker *c, struct expr *base,
                            const char *name, struct expr **args,
                            size_t count);
bool sema_iterate(struct checker *c, struct expr *e, struct type *t,
                  struct iteration *it, struct type **element);
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
/* How a message names what an owning type owns: its `own` fields for a
   class, its parts for a struct or a tuple. */
const char *sema_owns_phrase(const struct type *t);
bool sema_reads_existing(const struct expr *e);
struct name sema_place_name(const struct expr *e);
bool sema_move_local(struct checker *c, struct expr *e,
                     const struct name *into, const struct name *by);
void sema_refuse_owned_copy(struct checker *c, const struct expr *value,
                            struct type *t);
bool sema_holds_mutex(const struct type *t);
void sema_refuse_lock_copy(struct checker *c, const struct expr *value,
                           const struct type *t);
void sema_check_block(struct checker *c, struct block *b);
const struct item *sema_named_function(const struct checker *c);
bool sema_thread_safe(const struct type *t);
bool sema_thread_safe_symbol(const struct symbol *sym);

/* sema_safety.c */

void sema_safety_declare(struct item *it);
void sema_safety_base(struct checker *c, const struct item *it,
                      const struct type *base);
bool sema_needs_hidden_lock(const struct item *it);
void sema_hidden_lock(struct checker *c, const struct item *it,
                      struct struct_field *f);
void sema_check_guards(struct checker *c);
bool sema_check_reach(struct checker *c, const struct expr *e,
                      const struct struct_field *f);
void sema_note_field_write(struct checker *c, const struct expr *e);
void sema_report_unfixed(struct checker *c);
bool sema_points_into_fields(const struct expr *e, const struct item *fn,
                             const struct type *to);
void sema_check_leak_return(struct checker *c, const struct expr *value,
                            const struct type *result);
void sema_check_leak_arg(struct checker *c, const struct expr *callee,
                         const struct expr *arg, const struct type *param);
void sema_capture(struct checker *c, struct symbol *sym);
void sema_note_write(struct checker *c, const struct expr *e);
void sema_note_call(struct checker *c, const struct expr *callee);
void sema_refuse_worker_closure(struct checker *c, const struct expr *arg,
                                const struct type *param);
struct type *sema_check_anonymous(struct checker *c, struct expr *e,
                                  struct type *expected);
void sema_check_function(struct checker *c, struct item *it);
void sema_check_main(struct checker *c, struct item *it);
void sema_check_test_block(struct checker *c, struct item *it);

/* sema_generic.c */

struct symbol *sema_type_param_find(const struct checker *c,
                                    const struct name *name);
void sema_declare_generics(struct checker *c);
void sema_resolve_generics(struct checker *c);
struct type *sema_alias_type(struct checker *c, struct symbol *sym);
void sema_run_pending(struct checker *c);
bool sema_has_params(const struct type *t);
/* The parameters of a generic and what stands in their place, a type or
   a constant, and the generic type whose copy it is. */
struct generic_map {
    struct types *types;
    const struct type *from;
    struct type *to;
    struct type *const *params;
    struct type **args;
    const struct symbolic **values;
    size_t count;
};
/* t with the arguments of map in place of its parameters. */
struct type *sema_subst(struct checker *c, struct type *t,
                        const struct generic_map *map);
const struct symbolic *sema_subst_symbolic(struct checker *c,
                                           const struct symbolic *s,
                                           const struct generic_map *map);
/* The map of the generic of copy to the arguments of copy. */
struct generic_map sema_copy_map(const struct type *copy);
/* The type parameters t declares itself: all of them, but those of the
   class around a type nested in a generic class. */
size_t sema_nested_own(const struct type *t);
/* The copy of generic with these arguments, made on its first use. */
struct type *sema_copy_named(struct checker *c, struct type *generic,
                             struct type **args,
                             const struct symbolic **values);
void sema_generic_ready(struct checker *c, struct type *generic);
struct type *sema_copy_of(struct checker *c, struct type *generic,
                          struct type_expr *const *written, size_t count,
                          struct pos pos);
struct type *sema_member_type(struct checker *c, struct type *fn,
                              const struct type *copy);
void sema_refuse_type_args(struct checker *c, const struct expr *e,
                           const struct name *name);
struct type *sema_generic_named(struct checker *c, struct expr *e,
                                struct type *t, const struct name *name,
                                struct type *expected);
struct type *sema_generic_call(struct checker *c, struct expr *e,
                               struct type *fn, const struct symbol *sym,
                               size_t fixed, const struct generic_call *g);
/* The signature of the copy of the generic worker fn named sym that
   call of `parallel` or `dispatch` reaches, whose chunk or object has
   the type first. NULL after an error. */
struct type *sema_worker_copy(struct checker *c, struct expr *call,
                              struct type *fn, const struct symbol *sym,
                              struct type *first);
/* The signature of the `operator fn` sym that call makes of an operator
   with operands of the types left and right. It is that of its copy when
   sym is generic or a function of a copy. NULL after an error. */
struct type *sema_operator_copy(struct checker *c, struct expr *call,
                                struct type *fn, const struct symbol *sym,
                                struct type *left, struct type *right);
bool sema_param_operator(struct checker *c, struct expr *e,
                         enum token_kind op, const char *hook,
                         struct type *operand);
bool sema_param_iterate(struct checker *c, struct expr *e, struct type *p,
                        struct type **element);
struct type *sema_param_index(struct checker *c, struct expr *e,
                              struct type *p, bool write);
const struct type *sema_param_iface(const struct type *p,
                                    const struct name *name);
/* Whether the constraints of the parameter p give the hook named hook. */
bool sema_param_has(const struct type *p, const char *hook);
bool sema_param_hash(struct checker *c, struct expr *e, const struct type *p);
void sema_check_generic_item(struct checker *c, const struct item *it);

/* sema_copies.c */

/* Make a compiled copy of every generic the module uses with concrete
   arguments, and add each to the items of the module. */
void sema_compile_copies(struct checker *c);
/* e[i] = v in the statement s, which a copy reads again with the type of
   its argument. Returns false when the base has no hook, and s is then a
   plain assignment. */
bool sema_set_index(struct checker *c, struct stmt *s);

/* sema_hash.c */

/* Whether the call e, `x.hash()` with x of type t, is the default hash
   of x or the hook of a type parameter. The call then has its type in
   *result. Returns false when `hash` is a function of t, which the call
   of a method reaches. */
bool sema_hash_call(struct checker *c, struct expr *e, struct type *t,
                    struct type **result);
/* Whether e is `x.hash()` in the form that sema_hash_call gives. */
bool sema_is_hash_call(const struct expr *e);

/* sema_pattern.c */

/* Whether the call e names a method of `str` with a pattern or a function
   of a match. It then checks the receiver and every argument and writes
   the call of `anti.regex` over e, and *fn holds its function type or an
   error. */
bool sema_pattern_call(struct checker *c, struct expr *e, struct type **fn);
/* `m.1` and `m.name` of the match t of a pattern literal, the call of
   `group` written over e. */
struct type *sema_match_field(struct checker *c, struct expr *e,
                              struct type *t);
/* Check e where a condition stands. A match there is its test. */
struct type *sema_check_test(struct checker *c, struct expr *e);
/* `if let m = e { }` on the match t of the checked value. */
void sema_if_let_none(struct checker *c, struct stmt *s, struct type *t);

/* sema_export.c */

void sema_check_extern_fn(struct checker *c, struct item *it);
void sema_check_export(struct checker *c, struct item *it);

#endif

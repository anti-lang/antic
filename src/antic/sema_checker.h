#ifndef ANTIC_SEMA_CHECKER_H
#define ANTIC_SEMA_CHECKER_H

/* The state of the checker and the functions its files share. sema.c
   holds the helpers every file uses, scopes and lookup, and the
   resolution of types and signatures, and runs the passes over the
   items. sema_declare.c declares the items of a module and checks the
   `provides` and `link` lines and the declaration of a simd struct.
   sema_class.c checks the class model: the
   tables, the contracts and the bodies that fill them, the interfaces,
   the hooks and the operators. sema_chain.c takes chains of
   declarations apart, and sema_const.c evaluates constants. sema_expr.c
   checks expressions, and sema_call.c calls and members. sema_operator.c
   checks the binary operators and finds the `operator fn` functions of
   a type, sema_simd.c checks simd structs, and sema_thread.c the
   atomics, the locks, the channels and the workers. sema_stmt.c
   checks statements and function bodies, and walks what a worker
   reaches. sema_closure.c checks the anonymous functions and what they
   capture. sema_value.c holds the value rules: what a value owns, and
   when it moves, when it is copied and when a copy is refused.
   sema_safety.c checks the thread-safe classes and says which types are
   thread-safe. sema_escape.c refuses a reference to a local that would
   outlive the local, and sema_free.c a use of a value on a path after
   `delete`, `destroy` or a move of it. sema_uninit.c refuses a read of
   an element of `alloc(T, n)` before a path wrote it, and sema_index.c
   an index out of range on every path that reaches it. sema_lock.c
   refuses a cycle in the graph of lock orders. sema_generic.c
   checks generics, and sema_copies.c compiles their copies. sema_hash.c
   checks hashing and the default `==`, and sema_pattern.c the pattern
   literals and their calls. sema_export.c checks what crosses to C and
   writes the interface of a module, and sema_doc.c the doc warnings,
   which read the module alone.

   DESIGN: every node, type, name and list a function of the checker
   makes lies in the arena of the checker, c->arena, which the pool of
   types c->types allocates from as well. The arena owns it, and
   arena_free frees all of it at once. No function here returns memory
   that its caller frees alone. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "arena.h"
#include "attributes.h"
#include "ast.h"
#include "diagnostic.h"
#include "lexer.h"
#include "ptrset.h"
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

/* One `try` block being checked. Each failing call of its body without a
   handler of its own reaches the handler of the block, and the first one
   gives the error that handler binds. */
struct try_record {
    struct type *error_type;    /* `?*Error` of the first failing call */
};

/* DESIGN: the context of the code being checked is where it stands and
   how it is checked. sema_enter makes one current and sema_leave puts the
   one before it back, and nothing else writes it. A switch written by
   hand saved some fields and not others, so a constant first evaluated in
   a signature resolved its names there and a `try` lost the error type of
   the one around it (S04 of the audit). A declaration that is resolved on
   its first use, a constant, an alias or a constraint set, is resolved in
   the context of the declaration, which sema_declaration_context gives.
   That context is never quiet, so a probe caches no result a loud check
   would not, and the declaration reports its own errors at its place. */
struct context {
    struct scope *scope;        /* the innermost block, or the module's */
    struct item *function;      /* the function whose body is checked */
    /* The item whose nested types, and those of the classes around it,
       are named as written. NULL at module level. */
    const struct item *within;
    /* The function whose signature is resolved now, whose type
       parameters its types name. NULL outside a signature. */
    const struct item *signature;
    struct try_record *in_try;  /* the enclosing `try` block, or NULL */
    int quiet;                  /* above 0, errors are not reported */
    bool target_sized;          /* a symbolic array length is allowed */
    /* A function type holds one C function pointer in every place, a
       parameter of it included. The parameters of an `extern fn` are
       checked so. */
    bool plain_fns;
    /* The result of an `operator fn value` resolves, where a part of a
       tuple may be written `lent *T`. */
    bool value_result;
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
    struct context outer;       /* the context sema_leave_scope puts back */
};

/* A field of a concurrent class of the module that a write outside
   `construct` made none of guarded, atomic or fixed. */
struct written_field {
    const struct struct_field *field;
};

/* The loops inside each other that the checker names, see loops_open. */
#define LOOPS_OPEN_MAX 16

/* One `sync` that the statement being checked stands in, innermost
   first. */
struct held_mutex {
    const struct expr *mutex;
    const struct held_mutex *outer;
};

/* Where a `lent` pointer is refused, for the message. LENT_TO_C is an
   argument of an `extern fn`, where it is not refused. */
enum lent_use { LENT_STORED, LENT_RETURNED, LENT_PASSED, LENT_TO_C };

/* The handler a `yield` belongs to: the innermost `catch` handler that
   holds it, by R015 of "Rules of 2026-10-13". */
struct handler_state {
    struct type *yields;    /* the type `yield` gives */
    int depth;              /* above 0, a `yield` has a place to go */
    int deferring;          /* the checker's deferring where it began */
    bool of_try;            /* the handler of a `try` block */
};

/* The place and the function of a move the checker marked, which the
   message of the safety check `use-after-free` names. */
struct move_note {
    struct name into;
    struct name by;
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
    struct context ctx;         /* written by sema_enter and sema_leave */
    /* The move_note of each name the checker marked as moving. */
    struct ptr_map moves;
    int loop_depth;
    /* The loops around the statement checked now, outermost first, as
       far as LOOPS_OPEN_MAX. */
    const struct stmt *loops_open[LOOPS_OPEN_MAX];
    struct stmt *fallthrough;   /* the one that ends the arm checked now */
    /* The enum whose value checked now is enum_value, so a name of a
       later value of it gets a message of its own. */
    const struct item *enum_item;
    size_t enum_value;
    /* The type of each field value of the literal checked next that
       inference checked already, or NULL. sema_check_field_inits takes
       it and clears it. */
    struct type **field_given;
    bool atomic_place;          /* the place of an atomic operation */
    /* The operand of `delete` or `destroy`, whose address a field of a
       singleton gives there. R083 of "Answers of 2026-10-18" then refuses
       the part of the singleton with a message of its own. */
    bool ending_object;
    bool program;               /* the build writes a program, not a library */
    struct handler_state handler;   /* the handler under check */
    bool saw_fail;              /* the body holds a `fail` or a `try` */
    const struct expr *top_call; /* the first statement's call, or NULL */
    /* The call whose `catch` the `let` checked now takes over when the
       call cannot fail and gives a `?*T`, or NULL. */
    const struct expr *guard_taker;
    /* The call of the expression statement checked now, whose value
       nothing takes, or NULL. */
    const struct expr *discarded;
    int deferring;              /* above 0, a `defer` or `undo` is checked */
    const struct stmt *deferred;    /* the innermost of them, or NULL */
    int deferred_loops;         /* loop_depth where deferred began */
    const struct expr *field_base; /* the base of the field checked now */
    const struct held_mutex *held; /* the `sync` blocks around it */
    /* The innermost `sync` around the anonymous function checked now,
       where the blocks of its own body end, or NULL. */
    const struct held_mutex *held_outside;
    int const_depth;            /* constants evaluated inside each other */
    int alias_depth;            /* `type` lines resolved inside each other */
    int set_depth;              /* `constraint` sets resolved likewise */
    /* What a `lent` pointer refused where it stands would be: stored,
       returned or passed on. The message of the refusal names it. */
    enum lent_use lent_use;
    /* The callee of the call checked now, which may name a generic
       function without its arguments. */
    const struct expr *callee;
    /* The name that the `=` checked now gives a new value, which is no
       use of the value it held. */
    const struct expr *assigned;
    /* The operation of the flags form checked now, whose result wraps
       and sets the flags rather than leave its range, or NULL. */
    const struct expr *flags_value;
    /* The copies named before the functions had their types, whose
       constraints are checked once they do. */
    struct pending_check *pending;
    size_t pending_count;
    size_t pending_capacity;
    bool pending_done;
    /* The copies being filled, one inside another, and whether a chain
       too deep was refused. */
    int copy_depth;
    size_t copy_count;          /* the copies of generic types made */
    bool copy_refused;
    bool copy_name_refused;         /* a cut copy name was reported */
    const struct type *copy_root;   /* the generic the chain began with */
    const struct type *copy_cut;    /* a copy in the chain has a cut name */
    /* The answers of the question of a default `==` being asked, which
       sema_default_eq keeps while its parts ask again. */
    struct eq_answers *eq_answers;
    /* The fields of concurrent classes that the module writes after
       `construct`, reported at their declarations at the end. */
    struct written_field *written;
    size_t written_count;
    size_t written_capacity;
    /* The functions of thread-safe classes that return a pointer into
       the fields, and the returns of a call of one on self, which the
       end of the module reports from a public function. */
    const struct item **leaking;
    size_t leaking_count;
    size_t leaking_capacity;
    struct leak_call *leak_calls;
    size_t leak_call_count;
    size_t leak_call_capacity;
    /* The functions and the functions of class bodies have their types,
       so a call in a constant finds its callee. */
    bool fns_typed;
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
/* The operators of the table a struct and a class have by default, and
   the one they never have by default. */
#define LANG_HOOK_EQ "eq"
#define LANG_HOOK_LT "lt"

/* The bounds of the copies of generics, in sema_generic.c and
   sema_copies.c: how deep a chain of copies goes, how many copies of
   types and of functions a module names, and how long the name of a copy
   is. */
#define COPY_DEPTH_MAX 64
#define COPY_COUNT_MAX 16384
#define COPY_NAME_MAX 65536

/* sema.c */

int64_t sema_signed_bits(uint64_t v);
void sema_error_at(struct checker *c, struct pos pos, const char *format,
                   ...)
    ATTRIBUTE_PRINTF(3, 4);
/* A safety check at pos, which `unchecked(name, "reason")` overrules. It
   stops the build unless a clause covers it, so it leaves c->ok. */
void sema_check_at(struct checker *c, enum diag_name name, struct pos pos,
                   const char *format, ...)
    ATTRIBUTE_PRINTF(4, 5);
/* Whether a safety check named name stands at pos already. A
   statement that the walk of a check reads at more than one exit gives
   its message once. */
bool sema_reported_at(const struct checker *c, struct pos pos,
                      enum diag_name name);
const char *sema_tn(const struct type *t);
struct type *sema_builtin(struct checker *c, enum type_kind kind);
bool sema_is_error(const struct type *t);
bool sema_name_is(const struct name *a, const char *text);
/* The place e names with its `&` and `*` taken off, so `m`, `&m` and
   `*&m` name one place. */
const struct expr *sema_bare_place(const struct expr *e);
/* Whether a and b name one place: the same variable, or the same path of
   fields from it, each after sema_bare_place. */
bool sema_same_place(const struct expr *a, const struct expr *b);
/* A node of kind at pos, in the arena of the checker, which owns it. */
struct expr *sema_new_node(struct checker *c, enum expr_kind kind,
                           struct pos pos);
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
/* Put name with its symbol sym into the scope s. */
void sema_scope_put(struct scope *s, const struct name *name,
                    struct symbol *sym);
/* Declare name of kind at pos in the current scope. A name the scope
   holds already is refused with duplicate_message, a format that holds
   exactly one `%.*s`, which takes the name. */
struct symbol *sema_declare(struct checker *c, enum symbol_kind kind,
                            const struct name *name, struct pos pos,
                            const char *duplicate_message);
void sema_warn_catch_shadow(struct checker *c, const struct name *name,
                            struct pos pos);
void sema_enter_scope(struct checker *c, struct scope *s);
void sema_leave_scope(struct checker *c, struct scope *s);
/* The only writers of c->ctx. sema_enter makes next the context and
   keeps the one it replaces in saved, and sema_leave puts saved back. */
void sema_enter(struct checker *c, const struct context *next,
                struct context *saved);
void sema_leave(struct checker *c, const struct context *saved);
/* sema_enter with the context that differs from the current one in the
   one field each names. */
void sema_enter_quiet(struct checker *c, struct context *saved);
void sema_enter_sized(struct checker *c, struct context *saved);
void sema_enter_within(struct checker *c, const struct item *within,
                       struct context *saved);
/* The context of a declaration at module level, or in the body of the
   class within: the module scope, no function and no signature, and
   never quiet. */
struct context sema_declaration_context(struct checker *c,
                                        const struct item *within);
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
/* Refuse a channel at pos whose element holds a pointer, and give
   whether it did. */
bool sema_refuse_chan_element(struct checker *c, struct pos pos,
                              const struct type *element);
struct type *sema_chan_element(struct checker *c, struct type_expr *t);
struct type *sema_resolve_type(struct checker *c, struct type_expr *t);
struct type *sema_param_form(struct checker *c, struct type *t, bool keep,
                             bool concurrent, bool owned, struct pos pos);
struct type *sema_lent_form(struct checker *c, struct type *t, bool lent,
                            bool owned, struct pos pos);
struct symbol *sema_std_function(struct checker *c, struct pos pos,
                                 const char *what, const struct name *module,
                                 const char *text, size_t count);
struct symbol *sema_std_item(struct checker *c, const struct name *module,
                             const struct name *name, bool own);
struct type *sema_error_class(struct checker *c, struct pos pos);
struct type *sema_location_type(struct checker *c, struct pos pos);
/* The class anti.lang.Error, or NULL when the compilation lacks
   anti.lang. */
struct type *sema_lang_error_or_null(struct checker *c);
struct type *sema_deserialize_type(struct checker *c, struct pos pos,
                                   const struct type *declared);
bool sema_check_object_from(struct checker *c, struct expr *e,
                            const char *what);
/* The type of the function item it, fn(params) -> result. */
struct type *sema_function_type(struct checker *c, struct item *it);

/* sema_declare.c */
bool sema_later_enum_value(struct checker *c, const struct expr *e);

/* The passes of sema_check that declare the items, in the order it runs
   them. */
void sema_declare_root(struct checker *c);
void sema_declare_import(struct checker *c, const struct import *imp);
void sema_declare_items(struct checker *c);
void sema_declare_direct_names(struct checker *c, const struct import *imp);
void sema_resolve_bases(struct checker *c);
void sema_declare_enums_and_variants(struct checker *c);
void sema_declare_body_constants(struct checker *c);
void sema_declare_all_fields(struct checker *c);
void sema_declare_function_types(struct checker *c);
void sema_declare_all_members(struct checker *c);
void sema_check_signatures(struct checker *c);
/* The checks of the `provides` lines and of the names of one kind of
   `link` line of a module, which check_boundary of sema.c runs. */
void sema_check_provides(struct checker *c, struct module *module);
void sema_check_link_names(struct checker *c, const struct link_name *names,
                           size_t count, const char *kind, const char *what);
void sema_check_nesting(struct checker *c, struct type *t, struct pos pos);
/* The shared name `op:Type` of a module-level `operator fn` whose name
   another one of its module has, in arena, which owns it. */
struct name sema_shared_name(struct arena *arena, const struct name *op,
                             const struct name *type);

/* sema_expr.c */

bool sema_is_place(const struct expr *e);
const struct struct_field *sema_bitfield_of(const struct expr *e);
void sema_mark_address_taken(struct checker *c, struct expr *e);
bool sema_spell(struct text *out, const struct expr *e);
struct type *sema_usable_pointer(struct checker *c, const struct expr *e,
                                 struct type *t);
struct type *sema_whole_optional(struct type *t, struct expr *e);
void sema_refuse_lent_tuple(struct checker *c, const struct expr *e);
bool sema_require(struct checker *c, struct expr *e, struct type *got,
                  struct type *expected);
/* The checked call of the hook name on base with count arguments, in the
   arena of the checker, which owns it. */
struct expr *sema_hook_call(struct checker *c, struct expr *base,
                            const char *name, struct expr **args,
                            size_t count);
bool sema_iterate(struct checker *c, struct expr *e, struct type *t,
                  struct iteration *it, struct type **element);
extern const struct name sema_hidden_value;
const char *sema_format_name(const struct expr *e);
struct type *sema_check_expr(struct checker *c, struct expr *e,
                             struct type *expected);
struct type *sema_check_storage(struct checker *c, struct expr *e);
bool sema_is_untyped(const struct expr *e);
void sema_error_may_be_none(struct checker *c, const struct expr *e,
                            const struct type *t);

/* sema_operator.c */

const char *sema_op_text(enum token_kind op, char buffer[OP_TEXT]);
/* The operator function `name` that the type t declares, or that the
   module of t declares for a struct. */
struct symbol *sema_operator_symbol(struct checker *c, struct type *t,
                                    const char *text);
/* Whether the module of t, or t itself, gives it `operator fn text`, a
   free function of the module that takes t or a pointer to it first. */
bool sema_module_operator(struct checker *c, struct type *t,
                          const char *text);
struct symbol *sema_hook(struct checker *c, struct type *t, const char *text);
bool sema_is_iterator(struct checker *c, struct type *t);
bool sema_hook_well_formed(struct checker *c, const struct symbol *hook,
                           const char *name);
/* The value of type t that a hidden operand or a value the caller holds
   stands for, `*p`, in the arena of the checker. It is checked already
   and never lowered. */
struct expr *sema_stand_in(struct checker *c, struct pos pos,
                           struct type *t);
/* The checked call of the `operator fn hash` that the module of the class
   t declares for it, on a value that stands for the object, or NULL when
   the module declares none or t does not meet its constraints. The call
   lies in the arena of the checker, which owns it. */
struct expr *sema_class_hash_operator(struct checker *c, struct type *t,
                                      struct pos pos);
/* The same for `operator fn eq`, on two values. */
struct expr *sema_class_eq_operator(struct checker *c, struct type *t,
                                    struct pos pos);
struct type *sema_check_binary(struct checker *c, struct expr *e,
                               struct type *expected);
struct type *sema_check_in(struct checker *c, struct expr *e);

/* sema_simd.c */

bool sema_simd_lane_refused(const struct type *lane);
struct type *sema_check_simd_unary(struct checker *c, struct expr *e,
                                   struct type *t);
void sema_check_simd_compound(struct checker *c, struct pos pos,
                              enum token_kind op, struct type *t);
struct type *sema_check_simd_binary(struct checker *c, struct expr *e,
                                    struct type *left, struct type *right);
struct type *sema_check_simd_cast(struct checker *c, struct expr *e,
                                  struct type *from, struct type *to);
bool sema_simd_module_call(const struct checker *c, const struct expr *e);
struct type *sema_check_simd_module(struct checker *c, struct expr *e);
bool sema_simd_static_name(const struct name *name);
struct type *sema_check_simd_static(struct checker *c, struct expr *e,
                                    struct type *t);
bool sema_simd_value_name(const struct name *name);
struct type *sema_check_simd_value(struct checker *c, struct expr *e,
                                   struct type *base, struct type *s);
bool sema_simd_method_declared(struct checker *c, const struct type *s,
                               const struct name *name);

/* sema_call.c */
/* The refusal of the member name, private or protected by vis, that the
   class owner declares, where the code that names it may not see it. */
void sema_refuse_hidden(struct checker *c, struct pos pos,
                        const struct name *name, enum visibility vis,
                        const struct type *owner);
/* The refusal of the atomic field or local name where it is named
   outside its own calls. */
void sema_refuse_atomic(struct checker *c, struct pos pos,
                        const struct name *name);
/* The defaults of the parameters of sym. sema_required_params gives the
   fewest parameters a call of it writes, of count in all.
   sema_filled_by_defaults gives how many parameters after the first
   `given` the defaults fill, and sema_append_defaults appends their
   arguments to the call e. Each counts in the parameters of the type. */
size_t sema_required_params(const struct symbol *sym, size_t count);
size_t sema_filled_by_defaults(const struct symbol *sym, size_t given);
void sema_append_defaults(struct checker *c, struct expr *e,
                          const struct symbol *sym, const struct type *fn,
                          size_t given, size_t filled);

struct type *sema_struct_of(struct type *t);
bool sema_refuse_abstract_value(struct checker *c, struct pos pos,
                                const char *what, const struct type *t);
const struct type *sema_inherited(const struct type *t);
const struct type *sema_declaring_class(const struct item *m);
const struct type *sema_checking_class(const struct checker *c);
bool sema_singleton_type(const struct type *t);
struct symbol *sema_method_symbol(const struct checker *c,
                                  const struct type *s,
                                  const struct name *name);
/* The free function name of the module of the type s, a shared `operator
   fn` among them, without the functions of the body of s. */
struct symbol *sema_module_function(const struct checker *c,
                                    const struct type *s,
                                    const struct name *name);
bool sema_implemented_in(const struct type *t, const struct type *iface);
struct symbol *sema_null_pointer_maker(struct checker *c, struct pos pos,
                                       const struct type *guarded);
struct symbol *sema_error_maker(struct checker *c, struct pos pos);
struct symbol *sema_error_text_maker(struct checker *c, struct pos pos,
                                     bool takes);
void sema_resolve_origin(struct checker *c, struct stmt *s);
struct type *sema_caught_error(struct checker *c, struct type *result);
void sema_refuse_try_on_optional(struct checker *c, struct pos at);
void sema_check_handler(struct checker *c, struct handler *h,
                        struct type *yields, bool of_try, bool value);
void sema_declare_caught(struct checker *c, struct handler *h,
                         struct type *t);
bool sema_in_failing_function(const struct checker *c);
void sema_refuse_escaping_error(struct checker *c, const struct expr *e);
struct type *sema_interface_named(struct checker *c,
                                  const struct name *qualifier,
                                  const struct name *name, struct pos pos);
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
                            const struct struct_field *fields,
                            size_t field_count, const char *literal_name,
                            bool skip_missing);
const struct symbol *sema_qualifier(const struct checker *c,
                                    const struct expr *field);

/* sema_thread.c */

bool sema_atomic_call(struct checker *c, struct expr *e, struct type **out);
struct type *sema_check_close(struct checker *c, struct expr *e);
struct type *sema_check_mutex_new(struct checker *c, struct expr *e);
struct type *sema_check_mutex_destroy(struct checker *c, struct expr *e,
                                      struct type *base);
struct type *sema_check_sync_op(struct checker *c, struct expr *e);
struct type *sema_check_parallel(struct checker *c, struct expr *e);
struct type *sema_check_dispatch(struct checker *c, struct expr *e);
/* Refuse an atomic field at pos whose type t no atomic operation
   moves. */
void sema_refuse_atomic_field(struct checker *c, struct pos pos,
                              const struct type *t);
struct type *sema_check_join(struct checker *c, struct expr *e);
/* The jobs a block has joined, which a second `join` of one of them
   is refused against. sema_join_flow_stmt reads each statement of the
   block once it is checked, in the order of the text, and
   sema_join_flow_free ends the record with the block. */
struct join_flow {
    struct joined *done;
    size_t count;
    size_t capacity;
    /* The locals the block declares, the only ones it follows, and
       whether each was named apart from a `join` since. */
    const struct symbol **locals;
    bool *named;
    size_t local_count;
    size_t local_capacity;
};
void sema_join_flow_stmt(struct checker *c, struct join_flow *flow,
                         const struct stmt *s);
void sema_join_flow_free(struct join_flow *flow);

/* sema_class.c */
/* Whether a is b or a class below b, so a pointer to a converts to a
   pointer to b without a check. */
bool sema_descends_from(const struct type *a, const struct type *b);
/* The type of one parameter of a function of anti.lang.Object, after
   `self`. */
enum root_param { ROOT_P_OBJECT, ROOT_P_STR, ROOT_P_ERROR, ROOT_P_FIELD };
/* One function of anti.lang.Object: its name, its parameters besides
   `self`, its result and the types of those parameters. */
struct root_fn {
    const char *name;
    size_t params;
    enum type_kind result;
    enum root_param kinds[2];
};
/* The sixteen functions of anti.lang.Object in the order of every table:
   the seven of the runtime, then the nine hooks from
   SEMA_ROOT_FIRST_HOOK. sema_declare_root of sema_declare.c builds
   their items from it, and the tables of sema_class.c take their first
   entries from it. */
#define SEMA_ROOT_COUNT 16
#define SEMA_ROOT_FIRST_HOOK 7
extern const struct root_fn sema_root_fns[SEMA_ROOT_COUNT];

/* Check the class model of every type with fields of the module. */
void sema_check_classes(struct checker *c);
/* Refuse a `construct` with arguments of the class of it that can end
   without setting a field that has no default. */
void sema_check_construct_sets(struct checker *c, const struct item *it);

/* sema_chain.c */

/* The declarations that one depends on, collected on one stack. */
struct chain_deps {
    struct checker *c;
    struct symbol **items;
    size_t count;
    size_t capacity;
};

/* Add sym to d when it is not resolved yet. */
void sema_chain_add(struct chain_deps *d, struct symbol *sym);
/* Add to d the declarations that the declaration sym names. */
typedef void sema_chain_deps_fn(struct chain_deps *d, struct symbol *sym);
/* Resolve the declaration sym, which is not resolved yet. */
typedef void sema_chain_resolve_fn(struct checker *c, struct symbol *sym);
/* Resolve the declarations root depends on, each after the ones it
   names, when the chain below root is deeper than a few links. The
   caller resolves root. */
void sema_chain_prepare(struct checker *c, struct symbol *root,
                        sema_chain_deps_fn *deps_of,
                        sema_chain_resolve_fn *resolve);

/* sema_const.c */

bool sema_undefined_on_constants(struct checker *c, struct expr *e,
                                 const struct type *result);
/* Report a constant index e below zero or past the end on a base of the
   fixed array type array. True when it reported. */
bool sema_index_outside(struct checker *c, struct expr *e,
                        const struct type *array);
/* Report a constant value that does not fit the width of the bitfield
   field. True when it reported. */
bool sema_bitfield_constant(struct checker *c, struct expr *value,
                            const struct struct_field *field);
bool sema_eval_const(struct checker *c, struct expr *e,
                     struct const_value *out);
/* s, or the number it is once the operands of its operation are
   numbers. */
const struct symbolic *sema_fold_symbolic(struct checker *c,
                                          const struct symbolic *s);
/* Refuse the first call or `??` in e, a constant expression, in the order
   the evaluator meets them. True when it refused. */
bool sema_refuse_runtime_parts(struct checker *c, const struct expr *e);
bool sema_const_symbol(struct checker *c, struct symbol *sym,
                       struct pos use);

/* sema_stmt.c */
bool sema_refuse_in_deferred(struct checker *c, struct pos pos,
                             const char *word);
/* Refuse a write to the field target of a singleton that is read-only
   after creation, or its address, at pos. Returns whether it refused. */
bool sema_refuse_singleton_write(struct checker *c, const struct expr *target,
                                 struct pos pos);
bool sema_refuse_count_write(struct checker *c, const struct expr *target,
                             struct pos pos);

void sema_walk_worker(struct checker *c, const struct item *worker);
bool sema_fills(const struct type *t, const struct type *abstract);
bool sema_filled_somewhere(const struct checker *c, const struct type *t);
size_t sema_proved_names(const struct expr *cond, bool want_true,
                         struct symbol **out, size_t count);
struct type *sema_proved_type(struct checker *c, const struct symbol *sym);
void sema_check_block(struct checker *c, struct block *b);
/* e[i] = v in the statement s, which a copy reads again with the type of
   its argument. Returns false when the base has no hook, and s is then a
   plain assignment. */
bool sema_set_index(struct checker *c, struct stmt *s);
void sema_check_function(struct checker *c, struct item *it);
void sema_check_main(struct checker *c, struct item *it);
void sema_check_test_block(struct checker *c, struct item *it);
/* Whether the last statement of b returns on every path, the form the
   rule of the missing `return` reads. */
bool sema_block_returns(const struct block *b);
/* Whether s is a call of a function that returns `never`. */
bool sema_calls_never(const struct stmt *s);
struct type *sema_declared_result(struct checker *c);

/* sema_closure.c */
const struct item *sema_named_function(const struct checker *c);
void sema_capture(struct checker *c, struct symbol *sym);
void sema_note_write(struct checker *c, const struct expr *e);
void sema_note_call(struct checker *c, const struct expr *callee);
void sema_refuse_worker_closure(struct checker *c, const struct expr *arg,
                                const struct type *param);
const struct symbol *sema_held_deepest(const struct expr *e);
void sema_check_closure_lifetime(struct checker *c,
                                 const struct expr *target,
                                 const struct expr *value);
struct type *sema_check_anonymous(struct checker *c, struct expr *e,
                                  struct type *expected);

/* sema_value.c */

bool sema_type_owns(const struct type *t);
/* How a message names what an owning type owns: its `own` fields for a
   class, its parts for a struct or a tuple. */
const char *sema_owns_phrase(const struct type *t);
bool sema_reads_existing(const struct expr *e);
struct name sema_place_name(const struct expr *e);
bool sema_move_local(struct checker *c, struct expr *e,
                     const struct name *into, const struct name *by);
/* Bind value of type t to the place named into, as `=`, `let` and `if
   let` do: an `own` parameter moves into it, and any other existing
   value that owns memory is refused. */
void sema_bind_value(struct checker *c, struct expr *value, struct name into,
                     const struct type *t);
void sema_refuse_owned_copy(struct checker *c, const struct expr *value,
                            const struct type *t);
/* Refuse value, an `own` field, as the value of the `own` field into,
   which target names when an assignment writes it. */
void sema_refuse_own_field_copy(struct checker *c,
                                const struct struct_field *into,
                                const struct expr *value,
                                const struct expr *target);
/* Whether value, a part of a tuple, struct or variant literal, moves into
   it, and the move of such a part once the literal has read every part.
   literal names the type of the literal. */
bool sema_literal_moves(const struct expr *value);
void sema_move_into_literal(struct checker *c, struct expr *value,
                            const char *literal);
/* Refuse value when it names a parameter that is not `own` and whose
   value owns something or whose type holds a type parameter, and give
   whether it did. */
bool sema_refuse_caller_value(struct checker *c, const struct expr *value);
bool sema_holds_mutex(const struct type *t);
void sema_refuse_lock_copy(struct checker *c, const struct expr *value,
                           const struct type *t);
/* Refuse value where `=` or `let` would copy a concurrent class out of
   the place it lives in. */
void sema_refuse_concurrent_copy(struct checker *c, const struct expr *value,
                                 const struct type *t);
/* The move of arg, argument index of a call of callee, into an `own`
   parameter. receiver is the object of a call of a function of a class,
   or NULL. */
void sema_note_move(struct checker *c, const struct symbol *callee,
                    size_t index, const struct expr *receiver,
                    struct expr *arg);

/* sema_escape.c */

/* The safety check `escaping-local` over the body of it, which the
   checker checked. It reads nothing when a diagnostic from index first
   on is an error. */
void sema_check_escapes(struct checker *c, const struct item *it,
                        size_t first);

/* sema_free.c */

/* The safety check `use-after-free` over the body of it, which the
   checker checked. It reads nothing when a diagnostic from index first
   on is an error of the checker. */
void sema_check_frees(struct checker *c, const struct item *it,
                      size_t first);

/* sema_uninit.c */

/* The safety check `uninitialized-read` over the body of it, which the
   checker checked. It reads nothing when a diagnostic from index first
   on is an error of the checker. */
void sema_check_fills(struct checker *c, const struct item *it,
                      size_t first);

/* sema_index.c */

/* The safety check `index-out-of-range` over the body of it, which the
   checker checked. It reads nothing when a diagnostic from index first
   on is an error of the checker. */
void sema_check_indices(struct checker *c, const struct item *it,
                        size_t first);

/* sema_lock.c */

/* The safety check `lock-order` over the functions of the module, the
   copies of its generics among them, and the edges of its libraries,
   and the warning `blocking-under-lock` over the same functions. It
   keeps what the modules that import it need in c->module->locks. */
void sema_check_lock_order(struct checker *c);

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
/* Report each public function of a thread-safe class that returns what
   a function of the class gave, when that is a pointer into the fields,
   and free the record. */
void sema_report_leak_calls(struct checker *c);
bool sema_points_into_fields(const struct expr *e, const struct item *fn,
                             const struct type *to);
void sema_check_leak_return(struct checker *c, const struct expr *value,
                            const struct type *result);
void sema_check_leak_arg(struct checker *c, const struct expr *callee,
                         const struct expr *arg, const struct type *param);
void sema_check_leak_store(struct checker *c, const struct expr *target,
                           const struct expr *value, const struct type *to);
void sema_loop_opens(struct checker *c, const struct stmt *loop);
void sema_note_guarded_pointer(struct checker *c, struct symbol *sym,
                               const struct expr *value);
void sema_check_guarded_pointer(struct checker *c, const struct expr *e,
                                struct symbol *sym);
bool sema_thread_safe(const struct type *t);
bool sema_thread_safe_symbol(const struct symbol *sym);

/* sema_generic.c */
bool sema_generic_open(struct checker *c, const struct expr *e,
                       const struct type *t, struct type *expected);
struct type *sema_infer_literal(struct checker *c, struct expr *e,
                                struct type *t, struct type ***given);
/* The end of a refusal of the hook named hook on type t: what a struct
   or a class without `lt` declares, and "" for any other. */
const char *sema_no_order(const struct type *t, const char *hook);
/* Whether name is one of the twenty of the hook table: the fourteen
   operators, the five language hooks and `hash`. */
bool sema_operator_named(const struct name *name);

struct symbol *sema_type_param_find(const struct checker *c,
                                    const struct name *name);
void sema_declare_generics(struct checker *c);
void sema_resolve_generics(struct checker *c);
struct type *sema_alias_type(struct checker *c, struct symbol *sym);
void sema_run_pending(struct checker *c);
bool sema_has_params(const struct type *t);
bool sema_names_param(const struct type *t, const struct type *param);
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
/* The constant v with the arguments of map in place. */
const struct const_value *sema_subst_const(struct checker *c,
                                           const struct const_value *v,
                                           const struct generic_map *map);
/* The map of the generic of copy to the arguments of copy. */
struct generic_map sema_copy_map(const struct type *copy);
struct type *sema_copy_in_chain(struct type *t, const struct type *g);
struct name sema_copy_name(struct checker *c, struct text *out, bool *cut);
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
struct type *sema_member_type_in(struct checker *c, struct type *fn,
                                 const struct item *owner, struct type *s);
/* Whether t meets the constraints of the type parameter p, reporting
   nothing. */
bool sema_meets_param(struct checker *c, struct type *t, const struct type *p);
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
/* The type of the second parameter of the operator sym, with what the
   left operand left gives the type parameters, for a literal in its
   place. A parameter that the left operand leaves open stays in it. */
struct type *sema_operator_right(struct checker *c, const struct symbol *sym,
                                 struct type *left);
bool sema_param_operator(struct checker *c, struct expr *e,
                         enum token_kind op, const char *hook,
                         struct type *operand);
bool sema_param_iterate(struct checker *c, struct expr *e, struct type *p,
                        struct type **element);
struct type *sema_param_index(struct checker *c, struct expr *e,
                              struct type *p, bool write);
struct type *sema_param_iface(const struct type *p, const struct name *name);
/* Whether the constraints of the parameter p give the hook named hook. */
bool sema_param_has(const struct type *p, const char *hook);
bool sema_param_hash(struct checker *c, struct expr *e, const struct type *p);
/* Whether the type t meets the hook named hook, as a constraint asks. */
bool sema_meets_hook(struct checker *c, struct type *t, const char *hook);
void sema_check_generic_item(struct checker *c, const struct item *it);

/* sema_copies.c */

/* Make a compiled copy of every generic the module uses with concrete
   arguments, and add each to the items of the module. */
void sema_compile_copies(struct checker *c);

/* sema_hash.c */

/* Whether the call e, `x.hash()` with x of type t, is the default hash
   of x or the hook of a type parameter. The call then has its type in
   *result. Returns false when `hash` is a function of t, which the call
   of a method reaches. */
bool sema_hash_call(struct checker *c, struct expr *e, struct type *t,
                    struct type **result);
/* Whether e is `x.hash()` in the form that sema_hash_call gives. */
bool sema_is_hash_call(const struct expr *e);
/* Whether the checked call e is the default hash of a value whose parts
   reach no function of the program and no function of the runtime that
   has effects, so that the call has none. */
bool sema_hash_without_effects(const struct expr *e);
/* Whether a value of type t has the default `==`. A class value has it,
   and so has a struct, a tuple, a variant or a `?T` whose every part has
   `==`, its own or the default. */
bool sema_default_eq(struct checker *c, struct type *t);
/* The first field of the struct, tuple or case t without `==`, or
   NULL. */
const struct struct_field *sema_eq_gap(struct checker *c, struct type *t);
/* Make e, `a == b` or `a != b` on two values of type t, the default
   equality. Returns false when t has none. */
bool sema_equals(struct checker *c, struct expr *e, struct type *t);
/* The first field of the chain of the class t that holds a union in
   place, which the `equals` of `Object` cannot compare, or NULL. A class
   that replaces `equals` has none. */
const struct struct_field *sema_class_gap(struct checker *c, struct type *t);
bool sema_concurrent_lacks(struct checker *c, struct type *t,
                           const char *hook);
/* Give the class it the checked `==` and `x.hash()` of the default
   `equals` and `hash` the compiler writes for it, where its chain declares
   neither. */
void sema_class_defaults(struct checker *c, struct item *it);

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
struct type *sema_check_pattern(struct checker *c, struct expr *e,
                                struct type *expected);
struct type *sema_check_regex_compile(struct checker *c, struct expr *e,
                                      struct type *expected);

/* sema_export.c */

void sema_check_extern_fn(struct checker *c, struct item *it);
void sema_check_export(struct checker *c, struct item *it);

#endif

#ifndef ANTIC_LOWER_LOWERER_H
#define ANTIC_LOWER_LOWERER_H

/* The state of lowering and the functions its files share. lower.c holds
   the helpers every part uses and lowers the items of a module, with the
   functions a class carries and the hooks of tracing. lower_desc.c writes
   the descriptors and the tables of classes, structs and interfaces.
   lower_expr.c lowers expressions and calls, lower_simd.c the operations
   of simd structs, and lower_stmt.c statements, loops and the exits of a
   block. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ast.h"
#include "ir.h"
#include "sema.h"
#include "text.h"
#include "types.h"

/* Lowering walks the checked tree of one function. Expressions become
   instructions in the current block. if, while and do while become new
   blocks joined by jumps and branches. */

struct defers;

struct loop {
    struct ir_block *continue_to;
    struct ir_block *break_to;
    struct loop *outer;
    struct defers *defers_at;   /* the block scope the loop started in */
};

/* DESIGN: the statements of `defer` are recorded per block and run at
   every exit of it, in reverse order of declaration. A `return` runs the
   scopes of the whole function, and `break` or `continue` those the loop
   encloses. Nothing unwinds. */
/* One exit action of a block: a deferred statement, the statement of an
   `undo`, the end of a local whose class has to be torn down, or the
   delete of the error a handler binds. */
struct exit_action {
    const struct stmt *stmt;
    const struct symbol *local; /* the local, or the name of the error */
    bool undo;                  /* `undo`: the error exits alone run it */
    bool error;                 /* delete the error in error_temp */
    uint32_t error_temp;
    const struct type *error_type;
    bool unlock;                /* `sync`: unlock the mutex in mutex */
    uint32_t mutex;             /* the address of the lock */
    const char *unlock_fn;      /* the function of the runtime that does */
    /* DESIGN: the `leave` hook of an instrumented function is an exit
       action of a scope around its body. Every exit therefore runs it,
       after the locals of the body are gone. An exit that gives an error
       runs `failed` before it. */
    bool leave;
    /* A `keep own` parameter in local frees its snapshot at every exit,
       unless it moved and holds `none`. */
    bool snapshot;
};

struct defers {
    struct exit_action *items;
    size_t count;
    size_t capacity;
    struct defers *outer;
};

/* Where a handler sends control, and where `yield` puts its value. */
struct handling {
    struct ir_block *join;      /* the block after the call */
    struct ir_operand out;      /* where the result is written */
    uint32_t error;             /* the error the handler binds */
    const struct type *error_type;  /* its class, or NULL */
    bool has_out;
    const struct defers *defers_at; /* the scope around the handler */
    struct handling *outer;
};

/* The one handler of a `try` block, which every failing call inside it
   reaches. */
struct try_scope {
    struct ir_block *handler;
    uint32_t error;             /* the temporary the error lands in */
    const struct defers *defers_at; /* the scope around the `try` block */
    struct try_scope *outer;
};

struct lowerer {
    struct ir_module *m;
    const char *file;           /* the source path, for an assertion */
    uint32_t file_index;        /* the same path in the module's table */
    const char *module_name;
    struct ir_function *f;
    struct ir_block *b;         /* NULL after a terminator */
    struct loop *loop;
    struct defers *defers;
    uint32_t loop_depth;        /* the loops the next block sits inside */
    struct handling *handling;  /* the handler being lowered */
    struct try_scope *try_scope;
    struct ir_operand out_address;  /* the out parameter of a failing call */
    /* The out parameter of the `may fail` function being lowered, where
       `return v` puts what the function computed. */
    struct ir_operand result_out;
    const struct symbol *moved;     /* the local a `return` hands over */
    bool may_fail;              /* the function was written `may fail` */
    bool no_reflect;            /* --no-reflect: no field list */
    bool dev;                   /* --dev: every dispatch checks its table */
    bool hooks;                 /* the hook sites are written. */
    bool trace_marked;          /* code marked `trace` is instrumented. */
    bool trace_writes;          /* --trace writes: the changed hook. */
    const char *const *patterns;    /* --trace <pattern>. */
    size_t pattern_count;
    const char *version;        /* --package-version, of the descriptors. */
    /* The function being lowered when it is instrumented: the literal of
       its full name and the `self` it hooks. NULL where it is not. */
    const struct ir_global *trace_name;
    int64_t trace_name_length;
    struct ir_operand trace_self;
    /* The error of the exit that runs the deferred statements, which the
       `failed` hook takes. */
    struct ir_operand failing_error;
    /* DESIGN: an anonymous function is lowered after the function it
       stands in, as a function of the module of its own. The list holds
       the ones met and not lowered yet, and count numbers each, which
       names it `<enclosing>.<count>`. */
    struct item **anonymous;
    size_t anonymous_count;
    size_t anonymous_capacity;
    size_t anonymous_named;
};

/* A place is where an assignment writes: a variable that lives in a
   temporary, or an address in memory. */
struct place {
    bool in_temp;
    uint32_t temp;
    struct ir_operand address;      /* of a bitfield: of its aggregate */
    enum ir_type type;
    bool bitfield;
    uint32_t agg;                   /* bitfield */
    uint32_t field;                 /* bitfield */
    /* A field of a struct or a class: the start of the value that holds
       it and its type. The `changed` hook takes both. */
    struct ir_operand object;
    const struct type *owner;
};

/* The kinds of enum anti_check in src/rt/std.h, in its order. The unit test
   records_check_kinds pins the two together. */
enum check_kind {
    CHECK_BOUNDS, CHECK_OVERFLOW, CHECK_VALUE, CHECK_VALUE_U, CHECK_LEFT,
    CHECK_LEFT_U, CHECK_SHIFT
};

/* The nine hooks of enum anti_hook in src/rt/object.h, in its order. The
   unit test records_hook_entries pins the two together, and pins the
   entries they take in the table of every class. */
enum hook_kind {
    HOOK_CREATED, HOOK_DESTROYED, HOOK_COPIED, HOOK_DISPATCHED, HOOK_JOINED,
    HOOK_ENTER, HOOK_LEAVE, HOOK_FAILED, HOOK_CHANGED, HOOK_COUNT
};

/* DESIGN: the table of a class holds the class descriptor at entry 0 and
   one entry per public function of its chain. The entries of the base
   come first, in declaration order. The class's own entries follow. One
   name therefore keeps one index in every class of a chain. A call
   through a base pointer reads the entry the derived class filled. A
   `concrete fn` takes the entry of the function it replaces. An
   `abstract fn` leaves its entry zero until a class fills it. */

/* One entry of a table. It holds the name a call names, the count of its
   parameters with `self` among them, and the function of the class that
   fills it. A function of the root has no source, so it holds the
   runtime symbol instead. */
/* DESIGN: an entry is keyed by its name and its parameter count. Anti
   has no overloading, so two functions of one name in a chain have one
   signature everywhere but the nine hooks. `anti.lang.TraceHandler`
   declares each of them again with the object after `self`, and those
   take entries of their own after the root's. */
struct entry {
    struct name name;
    size_t params;
    const struct item *fn;
    const char *runtime;
};

struct table {
    struct entry *entries;
    size_t count;
    size_t capacity;
};

/* lower.c */

struct ir_operand lower_none(void);
struct ir_operand lower_temp(const struct lowerer *l, uint32_t t);
enum ir_type lower_ir_type_of(const struct type *t);
bool lower_is_aggregate(const struct type *t);
uint32_t lower_agg_of(struct lowerer *l, const struct type *t);
struct ir_vtype lower_vtype_of(struct lowerer *l, const struct type *t);
struct ir_operand lower_size_operand(struct lowerer *l, const struct type *t);
bool lower_narrows(enum ir_type from, enum ir_type to);
struct ir_block *lower_new_block(struct lowerer *l);
char *lower_cstr(const struct name *name);
struct ir_function *lower_find_function(const struct ir_module *m,
                                        const char *module, const char *name);
struct ir_global *lower_find_global(const struct ir_module *m,
                                    const char *module, const char *name);
char *lower_copy_text(const struct text *t);
uint32_t lower_result_agg(struct lowerer *l, const struct type *t);
void lower_add_param(struct lowerer *l, struct ir_function *f,
                     const struct type *t);
struct ir_function *lower_c_function(struct lowerer *l, const char *name,
                                     enum ir_type result, enum ir_type param);
struct ir_function *lower_calloc_function(struct lowerer *l);
struct ir_function *lower_callee_function(struct lowerer *l,
                                          const struct symbol *sym);
const struct ir_function *lower_signature(struct lowerer *l,
                                          const struct type *t);
const struct ir_function *lower_fatal_signature(struct lowerer *l);
const struct ir_function *lower_provider_signature(struct lowerer *l);
bool lower_is_context(const struct type *t);
const struct ir_function *lower_context_signature(struct lowerer *l,
                                                  const struct type *t);
void lower_push_argument(struct lowerer *l, struct ir_operand *args,
                         size_t *count, struct ir_operand value,
                         const struct type *param);
struct ir_function *lower_anonymous_function(struct lowerer *l,
                                             struct item *it);
uint32_t lower_captures_agg(struct lowerer *l, const struct item *it);
uint32_t lower_snapshot_agg(struct lowerer *l, const struct item *it);
struct ir_operand lower_context_word(struct lowerer *l, const struct type *t,
                                     struct ir_operand pair);
void lower_free_snapshot(struct lowerer *l, const struct type *t,
                         struct ir_operand pair);
void lower_dup_snapshot(struct lowerer *l, const struct type *t,
                        struct ir_operand from, struct ir_operand into);
const struct ir_function *lower_bound_signature(struct lowerer *l,
                                                const struct type *t);
double lower_float_literal(const struct expr *literal, enum ir_type type);
struct ir_operand lower_constant(struct lowerer *l,
                                 const struct const_value *v,
                                 enum ir_type type);
struct ir_operand lower_offset_address(struct lowerer *l,
                                       struct ir_operand address,
                                       struct ir_operand offset);
struct ir_operand lower_zero(void);
const struct struct_field *lower_field_of(const struct type *s,
                                          const struct name *name);
const struct type *lower_field_owner(const struct type *t,
                                     const struct name *name);
bool lower_name_is(const struct name *name, const char *text);
void lower_zero_lock(struct lowerer *l, const struct type *t,
                     struct ir_operand at);
struct ir_operand lower_object_lock_address(struct lowerer *l,
                                            const struct type *t,
                                            struct ir_operand object);
void lower_hold_lock(struct lowerer *l, struct ir_operand at, bool object,
                     int line);
extern const struct name lower_len_name;
extern const struct name lower_entry_name;
struct ir_operand lower_field_offset(struct lowerer *l, const struct type *s,
                                     const struct name *name);
struct ir_operand lower_element_offset(struct lowerer *l,
                                       const struct type *t, uint64_t index);
const struct ir_global *lower_literal_global(struct lowerer *l,
                                             const struct token_text *text);
struct ir_operand lower_literal_address(struct lowerer *l,
                                        const struct token_text *text);
void lower_hook_object(struct lowerer *l, enum hook_kind hook,
                       struct ir_operand object);
bool lower_traced_class(const struct lowerer *l, const struct type *t);
void lower_hook_call(struct lowerer *l, enum hook_kind hook);
void lower_hook_failed(struct lowerer *l, struct ir_operand err);
void lower_hook_copied(struct lowerer *l, struct ir_operand made,
                       struct ir_operand from);
const struct ir_global *lower_check_text(struct lowerer *l, int line,
                                         const char *operation);
struct ir_operand lower_widen_operand(struct lowerer *l, struct ir_operand v,
                                      const struct type *t);
void lower_check_branch(struct lowerer *l, struct ir_operand cond, bool bad,
                        const struct ir_global *text, enum check_kind kind,
                        struct ir_operand a, struct ir_operand b,
                        const struct type *widen);
struct ir_operand lower_binary_checks(struct lowerer *l, enum token_kind op,
                                      const struct type *t,
                                      struct ir_operand left,
                                      struct ir_operand right, int line);
struct ir_operand lower_field_address(struct lowerer *l,
                                      const struct expr *e);
struct ir_operand lower_first_element(struct lowerer *l, const struct expr *e,
                                      struct ir_operand *length);
struct ir_operand lower_element_address(struct lowerer *l,
                                        const struct expr *e);
struct ir_global *lower_static_global(struct lowerer *l,
                                      const struct symbol *sym);
bool lower_place(struct lowerer *l, const struct expr *e,
                 struct place *p);
struct ir_operand lower_const_address(struct lowerer *l,
                                      const struct const_value *v,
                                      const struct type *t);
const struct const_value *lower_location_value(struct lowerer *l,
                                               struct pos pos,
                                               const struct type *t);
uint32_t lower_array_agg(struct lowerer *l, const char *element_name,
                         struct ir_vtype element, size_t n);
uint32_t lower_table_agg(struct lowerer *l, size_t n);
const struct type *lower_struct_of_expr(const struct expr *e);
bool lower_bound_is_direct(const struct expr *e, const struct type *s);
struct ir_block *lower_when_made(struct lowerer *l, struct ir_operand p);
struct ir_operand lower_rt_call(struct lowerer *l, const char *name,
                                enum ir_type result,
                                const enum ir_type *params,
                                struct ir_operand *args, size_t count);
struct ir_operand lower_slice_length(struct lowerer *l, struct ir_operand p,
                                     const struct type *slice);

/* lower_desc.c */

bool lower_same_name(const struct name *a, const struct name *b);
size_t lower_table_index(const struct type *t, const struct name *name,
                         size_t params);
struct ir_operand lower_entry_offset(struct lowerer *l, size_t index);
uint32_t lower_fields_agg(struct lowerer *l, size_t n);
uint32_t lower_descriptor_agg(struct lowerer *l);
uint32_t lower_class_depth(const struct type *t);
struct ir_global *lower_class_global(struct lowerer *l, const struct type *t,
                                     const char *suffix, char **module_out,
                                     char **name_out);
bool lower_listed_field(const struct struct_field *f);
size_t lower_own_fields(const struct type *t);
struct ir_global *lower_class_fields(struct lowerer *l,
                                     const struct type *t);
struct ir_global *lower_class_descriptor(struct lowerer *l,
                                         const struct type *t);
struct ir_global *lower_struct_descriptor(struct lowerer *l,
                                          const struct type *t);
bool lower_has_body(const struct item *fn);
struct ir_function *lower_class_function(struct lowerer *l,
                                         const struct type *t,
                                         const char *part);
const struct item *lower_level_fn(const struct type *t,
                                  const struct name *name);
const struct item *lower_declared_copy(const struct type *t);
struct ir_global *lower_class_table(struct lowerer *l, const struct type *t);
struct ir_global *lower_interface_table(struct lowerer *l,
                                        const struct type *t,
                                        const struct struct_field *sub);
struct ir_function *lower_reach_thunk(struct lowerer *l,
                                      const struct struct_field *sub,
                                      const struct symbol *sym);
void lower_run_construct_bodies(struct lowerer *l, const struct type *t,
                                struct ir_operand dest);
void lower_run_construct(struct lowerer *l, const struct type *t,
                         struct ir_operand dest);
void lower_store_interface_tables(struct lowerer *l, const struct type *t,
                                  struct ir_operand dest);
bool lower_hook_name(const struct name *name);

/* lower_expr.c */

void lower_store_value(struct lowerer *l, const struct type *t,
                       const struct expr *e, struct ir_operand address);
bool lower_has_default(const struct struct_field *field);
void lower_init_name(const struct type *t, bool exported, struct text *out);
struct ir_function *lower_init_function(struct lowerer *l,
                                        const struct type *t);
void lower_store_default(struct lowerer *l, const struct struct_field *field,
                         struct ir_operand address);
void lower_store_field_default(struct lowerer *l, const struct type *owner,
                               size_t i, struct ir_operand object);
struct ir_operand lower_load_tag(struct lowerer *l, const struct type *v,
                                 struct ir_operand address);
struct ir_operand lower_case_address(struct lowerer *l, const struct type *v,
                                     struct ir_operand address);
void lower_build_into(struct lowerer *l, const struct expr *e,
                      struct ir_operand dest);
void lower_bind_value(struct lowerer *l, struct symbol *sym,
                      struct ir_operand v);
void lower_bind_cursor(struct lowerer *l, const struct iteration *it);
struct ir_operand lower_address(struct lowerer *l,
                                const struct expr *e);
struct ir_operand lower_read_place(struct lowerer *l, const struct place *p);
enum ir_op lower_binary_op(enum token_kind op, const struct type *t);
bool lower_is_comparison(enum token_kind op);
struct ir_operand lower_shift_wrap(struct lowerer *l, const struct type *t,
                                   struct ir_operand value,
                                   struct ir_operand count);
struct ir_operand lower_flag_operation(struct lowerer *l,
                                       const struct expr *e,
                                       uint8_t want,
                                       struct ir_operand flags[4]);
struct ir_operand lower_load_table(struct lowerer *l, struct ir_operand p,
                                   const struct type *t);
void lower_check_table(struct lowerer *l, struct ir_operand table,
                       const struct type *t);
uint32_t lower_sym_of(struct lowerer *l, const struct symbolic *s);
struct ir_operand lower_argument(struct lowerer *l,
                                 const struct expr *arg);
struct ir_operand lower_call(struct lowerer *l, const struct expr *e);
struct ir_function *lower_rt_function_giving(struct lowerer *l,
                                             const char *name,
                                             enum ir_type result,
                                             const enum ir_type *params,
                                             size_t count);
struct ir_function *lower_rt_function(struct lowerer *l, const char *name,
                                      const enum ir_type *params,
                                      size_t count);
struct ir_operand lower_static_descriptor(struct lowerer *l,
                                          const struct type *t);
struct ir_operand lower_object_call(struct lowerer *l, const char *name,
                                    struct ir_operand object,
                                    const struct type *t);
struct ir_operand lower_load_handle(struct lowerer *l, const struct expr *e);
struct ir_operand lower_sync_call(struct lowerer *l, const char *name,
                                  enum ir_type result,
                                  const enum ir_type *params,
                                  const struct ir_operand *args,
                                  size_t count);
struct ir_operand lower_expr(struct lowerer *l, const struct expr *e);
void lower_branch(struct lowerer *l, const struct expr *e,
                  struct ir_block *then_block,
                  struct ir_block *else_block);

/* lower_simd.c */

struct ir_operand lower_simd_binary(struct lowerer *l,
                                    const struct expr *e);
struct ir_operand lower_simd_unary(struct lowerer *l,
                                   const struct expr *e);
struct ir_operand lower_simd_cast(struct lowerer *l,
                                  const struct expr *e);
struct ir_operand lower_simd(struct lowerer *l, const struct expr *e);

/* lower_stmt.c */

void lower_push_leave_action(struct lowerer *l);
void lower_push_snapshot_action(struct lowerer *l, const struct symbol *param);
bool lower_type_needs_destruct(const struct type *t);
void lower_clear_tables(struct lowerer *l, struct ir_operand base,
                        const struct type *t);
void lower_handle_error(struct lowerer *l, const struct expr *call,
                        struct ir_operand err, struct ir_operand out,
                        bool has_out, struct ir_operand release);
struct ir_operand lower_construct(struct lowerer *l,
                                  const struct expr *e,
                                  struct ir_operand dest);
bool lower_is_handled_call(const struct expr *e);
void lower_run_defers(struct lowerer *l, const struct defers *scope,
                      bool failing);
void lower_block(struct lowerer *l, const struct block *b);
void lower_reserve_slots(struct lowerer *l, struct ir_block *entry,
                         const struct block *b);

#endif

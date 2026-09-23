#ifndef ANTIC_AST_H
#define ANTIC_AST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lexer.h"
#include "text.h"

/* The syntax tree of one module. Every node lives in the compilation's
   memory pool, and names point into the source text, which outlives the
   tree. */

/* A name as written in the source. */
struct name {
    const char *text;
    size_t length;
};

struct pos {
    int line;
    int column;
};

struct type;    /* a checked type, filled in by semantic analysis */
struct symbol;  /* what a name refers to, filled in by semantic analysis */
struct struct_field;    /* a field of a checked type */
struct expr;

enum type_expr_kind {
    TYPEX_BUILTIN,  /* int, f32, str and the other type keywords */
    TYPEX_NAMED,    /* Vec2 or geometry.Vec2 */
    TYPEX_POINTER,  /* *T */
    TYPEX_ARRAY,    /* [N]T */
    TYPEX_SLICE,    /* []T */
    TYPEX_FN,       /* fn(T, U) -> R */
    TYPEX_TUPLE,    /* (int, str) */
    TYPEX_CHAN      /* chan T */
};

struct type_expr {
    enum type_expr_kind kind;
    struct pos pos;
    enum token_kind builtin;        /* TYPEX_BUILTIN */
    struct name module;             /* TYPEX_NAMED, empty when unqualified */
    struct name name;               /* TYPEX_NAMED */
    /* TYPEX_NAMED after `is`: the case of `v is geo.Shape.Circle`, whose
       module and variant stand in module and name. Empty otherwise. */
    struct name member;
    struct type_expr *element;      /* TYPEX_POINTER, TYPEX_ARRAY, TYPEX_SLICE,
                                       TYPEX_CHAN */
    bool nullable;                  /* TYPEX_POINTER: `?*T` */
    struct expr *length;            /* TYPEX_ARRAY */
    struct type_expr **params;      /* TYPEX_FN, TYPEX_TUPLE */
    size_t param_count;
    struct type_expr *result;       /* TYPEX_FN, NULL without a result */
    bool may_fail;                  /* TYPEX_FN: `fn(T) -> R may fail` */
    struct type *type;              /* set by semantic analysis */
};

/* The operations of an atomic field. Each is a call on the field, and
   the language offers no other way to read or write one. */
enum atomic_op {
    ATOMIC_LOAD,
    ATOMIC_STORE,
    ATOMIC_SWAP,
    ATOMIC_ADD,
    ATOMIC_SUB,
    ATOMIC_AND,
    ATOMIC_OR,
    ATOMIC_CAS
};

/* The operations of a mutex and a channel. The parser writes the three
   that have a keyword of their own. The checker writes the others from
   the calls `Mutex.new()`, `m.destroy()` and `close(c)` and from
   `delete(c)` of a channel. */
enum sync_op {
    SYNC_MUTEX_NEW,                 /* Mutex.new() */
    SYNC_MUTEX_DESTROY,             /* m.destroy() */
    SYNC_CHAN_NEW,                  /* chan T(n) */
    SYNC_SEND,                      /* send(c, v) */
    SYNC_RECV,                      /* recv(c) */
    SYNC_CLOSE,                     /* close(c) */
    SYNC_CHAN_DELETE                /* delete(c) */
};

/* The built-ins of a simd struct and the functions of `anti.simd`. The
   checker writes each from a call. */
enum simd_op {
    SIMD_OP_SPLAT,                  /* T.splat(v) */
    SIMD_OP_LOAD,                   /* T.load(slice, i) */
    SIMD_OP_STORE,                  /* v.store(slice, i) */
    SIMD_OP_SHUFFLE,                /* v.shuffle(i, j, k, l), constant */
    SIMD_OP_SUM,                    /* v.sum() */
    SIMD_OP_MIN,                    /* v.min() */
    SIMD_OP_MAX,                    /* v.max() */
    SIMD_OP_DOT,                    /* a.dot(b) */
    SIMD_OP_SELECT,                 /* simd.select(mask, a, b) */
    SIMD_OP_ANY,                    /* simd.any(mask) */
    SIMD_OP_ALL                     /* simd.all(mask) */
};

enum expr_kind {
    EXPR_INT,
    EXPR_FLOAT,
    EXPR_CHAR,
    EXPR_STRING,
    EXPR_BYTES,
    EXPR_BOOL,
    EXPR_NONE,
    EXPR_NAME,
    EXPR_UNARY,
    EXPR_BINARY,
    EXPR_CAST,
    EXPR_CALL,
    EXPR_INDEX,
    EXPR_SLICE,
    EXPR_FIELD,
    EXPR_STRUCT_LIT,
    EXPR_TUPLE,                     /* (a, b) */
    EXPR_SLICE_LIT,
    EXPR_ARRAY_LIT,
    EXPR_ARRAY_REPEAT,
    EXPR_ALLOC,
    EXPR_FREE,
    EXPR_OBJECT,                    /* dup, delete or destroy */
    EXPR_ATOMIC,                    /* an operation on an atomic place */
    EXPR_SIZE_OF,
    EXPR_PARALLEL,
    EXPR_DISPATCH,                  /* dispatch obj -> f(args) */
    EXPR_JOIN,                      /* join(job) and join_all(jobs) */
    EXPR_HERE,                      /* `here`, the position it stands at */
    EXPR_FORMAT,                    /* `f"..."` and `rf"..."` */
    EXPR_IN,                        /* `x in lo..hi` */
    EXPR_OPTIONAL,                  /* `p?.x` and `p?.f(args)`, checked */
    EXPR_SYNC_OP,                   /* an operation of a mutex or a channel */
    EXPR_SIMD,                      /* a built-in of a simd struct */
    EXPR_DESCRIPTOR                 /* the descriptor of a class, as `*byte` */
};

/* The format specification after the colon of an `{expr}`, as the
   parser read it. */
struct format_spec {
    char align;                     /* '<', '>', '^', or 0 when not written */
    bool zero;                      /* `0` before the width */
    int32_t width;                  /* -1 when not written */
    int32_t precision;              /* -1 when not written */
    char kind;                      /* 'x', 'X', 'b', 'o', 'e', 'f', or 0 */
};

/* One `{expr}` of an `f"..."` with the text before it. The last part
   holds the text after the last `{expr}` and no value. */
struct format_part {
    struct token_text text;         /* the bytes before the `{` */
    struct expr *value;             /* NULL in the last part */
    struct format_spec spec;
    struct pos pos;                 /* the `{` */
    struct token_text source;       /* `{x:q}` as written, for messages */
    /* Set by the checker. The value is bound to a local of its own.
       The two calls append the text and then the value to the builder
       of the literal. */
    struct symbol *bound;
    struct expr *text_call;         /* NULL for empty text */
    struct expr *value_call;        /* NULL in the last part */
};

/* name: value inside a struct or slice literal. */
struct field_init {
    struct name name;
    struct pos pos;
    struct expr *value;
};

/* How a failing call hands its error on. A handler is a block, `fatal`
   prints and exits, and `try` returns the error to the caller. */
enum handler_kind {
    HANDLE_NONE,
    HANDLE_BLOCK,
    HANDLE_FATAL,
    HANDLE_TRY,
    HANDLE_ENCLOSING            /* the `try` block around it */
};

/* `catch e { }` on a call, on a `let`, or around a block. */
struct handler {
    enum handler_kind kind;
    struct name name;               /* the error, empty in `catch { }` */
    struct pos pos;
    struct block *body;             /* HANDLE_BLOCK */
    struct symbol *symbol;          /* the error the handler binds */
};

struct expr {
    enum expr_kind kind;
    struct pos pos;
    struct token_text spelling;     /* source text of a literal */
    struct type *type;              /* set by semantic analysis */
    struct symbol *symbol;          /* EXPR_NAME, set by semantic analysis. */
    /* EXPR_NAME: an argument that moves the error a `catch` binds into
       an `own` parameter, so the handler holds it no longer. */
    bool moves;
    /* DESIGN: a pointer to a class converts to a pointer to one of its
       interfaces by adding the offset of the sub-object. The checker
       records the field here and lowering adds the offset, so every
       place a value flows into an interface slot is covered once. */
    const struct struct_field *to_iface;
    union {
        uint64_t integer;           /* EXPR_INT */
        uint32_t character;         /* EXPR_CHAR */
        bool boolean;               /* EXPR_BOOL */
        struct token_text text;     /* EXPR_FLOAT digits, EXPR_STRING, EXPR_BYTES */
        struct name name;           /* EXPR_NAME */
        struct {
            enum token_kind op;
            struct expr *operand;
        } unary;
        struct {
            enum token_kind op;
            struct expr *left;
            struct expr *right;
            /* `a + f.carry` and `a - f.carry`: the right operand is the
               `carry` of a Flags value, which goes in as a carry or a
               borrow. Set by the checker. */
            bool carry;
        } binary;
        struct {
            struct expr *operand;
            struct type_expr *type;
            bool checked;           /* `as?`, which gives `none` on a mismatch */
            bool test;              /* `is`, which gives a bool */
            bool from_sub;          /* the source may be a sub-object */
            bool promoted;          /* the checker's read of an f16 */
            const struct type *target;  /* the class of `is` and `as` */
            /* `v is Shape.Circle`: the index of the case plus 1, set by
               the checker, and 0 for every other `is` and `as`. */
            uint32_t variant_case;
        } cast;
        struct {
            struct expr *callee;
            struct expr **args;
            size_t arg_count;
            /* The class whose table holds the entry, when the call goes
               through one, and the name of that entry. dispatch is NULL
               for a direct call. */
            const struct type *dispatch;
            struct name entry;
            /* DESIGN: a call that can fail carries its handler. The
               checker refuses one that has none, so no program drops an
               error by writing nothing. */
            struct handler handler;
            struct expr *out;       /* the place the result is written to */
            /* `T(args)` and `alloc T(args)` build a value and run its
               `construct` with the arguments. */
            const struct type *builds;
            bool on_heap;
            /* The call cannot fail and gives a `?*T`, so its `catch`
               guards the pointer and the `let` takes it over. */
            bool guards_pointer;
            /* The call stands after `?.`, which gives a `?*T` whatever
               the call gives, so a `catch` may guard that. */
            bool optional;
        } call;
        struct {
            struct expr *base;
            struct expr *index;
        } index;
        struct {
            struct expr *base;
            struct expr *low;
            struct expr *high;
        } slice;
        struct {
            struct expr *base;
            struct name name;
            /* The sub-object whose table `T.f` reaches, for a body
               qualified by an interface. NULL for every other field. */
            const struct struct_field *through;
            uint32_t enum_value;    /* the index of an enum value, plus 1 */
            bool promoted;          /* the checker wrote it, not the program */
            bool element;           /* `t.0`, which names the field `_0` */
            bool optional;          /* `p?.x`, until the checker reads it */
        } field;
        struct {
            struct name module;     /* empty when unqualified */
            struct name name;
            /* The case of `geo.Shape.Circle { }`, whose module and
               variant stand in module and name. Empty otherwise. */
            struct name member;
            struct field_init *fields;
            size_t field_count;
            /* A literal of a variant: the index of its case plus 1, set
               by the checker, and 0 for a struct or a class. */
            uint32_t variant_case;
        } struct_lit;
        struct {
            struct type_expr *element;
            struct field_init *fields;
            size_t field_count;
        } slice_lit;
        struct {
            struct expr **elements;
            size_t count;
        } array_lit;
        struct {
            struct expr **elements;
            size_t count;
        } tuple;                    /* EXPR_TUPLE */
        struct {
            struct expr *value;
            struct expr *count;
        } array_repeat;
        struct {
            struct type_expr *type;
            struct expr *count;     /* NULL in the literal form */
            struct expr *value;     /* `alloc T { ... }`, or NULL */
        } alloc;
        struct expr *free_pointer;  /* EXPR_FREE */
        struct {
            enum token_kind op;     /* dup, delete or destroy */
            struct expr *operand;
        } object;
        struct {
            struct expr *job;       /* EXPR_JOIN: the job or the slice */
            bool all;               /* join_all */
        } join;
        struct {
            enum atomic_op op;
            struct expr *place;     /* the atomic field */
            struct expr *a;         /* the value, or the expected one */
            struct expr *b;         /* compare_swap: the new value */
        } atomic;
        struct type_expr *size_of;  /* EXPR_SIZE_OF */
        struct {
            struct expr *array;     /* the array to split */
            struct expr *chunks;    /* the count after `by`, or NULL */
            struct expr *call;      /* the worker, called or named */
        } parallel;
        struct {
            struct expr *object;    /* the object to submit */
            struct expr *call;      /* the worker, called or named */
        } dispatch;
        struct {
            struct format_part *parts;
            size_t count;
            bool raw;               /* `rf"..."` */
            /* `f"..."(from)`: the arguments after the literal, which the
               checker takes as the allocator of the text. */
            bool has_from;
            struct expr **from;
            size_t from_count;
            /* Set by the checker: the local `anti.text.Builder` that
               collects the text, the call that makes it and the call
               that gives its bytes. */
            struct symbol *builder;
            struct expr *start;
            struct expr *take;
        } format;                   /* EXPR_FORMAT */
        /* `value in low..high`. The checker binds the value to a local
           of its own and writes `value >= low && value < high` over
           it. */
        struct {
            struct expr *value;
            struct expr *low;
            struct expr *high;
            struct symbol *bound;
            struct expr *test;
        } in;                       /* EXPR_IN */
        /* The checker's form of `p?.x` and `p?.f(args)`. bound holds the
           value of base, and access is the field or the call that reads
           it when it is not `none`. */
        struct {
            struct expr *base;
            struct symbol *bound;
            struct expr *access;
        } optional;                 /* EXPR_OPTIONAL */
        /* The mutex or the channel an operation reads, the value that
           `send` puts and the capacity of `chan T(n)`, and the element
           type that `chan T(n)` names. */
        struct {
            enum sync_op op;
            struct expr *target;
            struct expr *value;
            struct type_expr *element;
        } sync_op;                  /* EXPR_SYNC_OP */
        /* The operands of a built-in of a simd struct in the order the
           enum lists them. The value a built-in is called on comes
           first, and it may be a pointer to one. simd is the simd struct
           the operation works on, and lanes holds the index of each lane
           of a shuffle. */
        struct {
            enum simd_op op;
            struct expr **args;
            size_t arg_count;
            const struct type *simd;
            uint32_t *lanes;
        } simd;                     /* EXPR_SIMD */
        /* EXPR_DESCRIPTOR: the class whose descriptor the expression
           gives. `lib.instance(I)` and `lib.supports(I, n)` write one,
           and no source text does. */
        const struct type *descriptor_of;
    } as;
};

/* DESIGN: one name of a destructuring. `let (a, b) = e;` and
   `for i, x in items` are the two places a program writes one. Both take
   the elements of a tuple in order, so both carry this list and the
   checker binds them by one rule. */
struct binding {
    struct name name;
    struct pos pos;
    struct symbol *symbol;
    /* The flags name of `let (result, flags) = e;` that names a Flags
       variable in scope, which the statement assigns. symbol is then
       that variable. */
    bool assigns;
};

struct stmt;

struct block {
    struct pos pos;
    struct pos end;             /* the closing brace */
    struct stmt **stmts;
    size_t count;
};

struct switch_arm {
    struct expr *value;
    struct pos pos;
    struct stmt *body;
    /* `Circle c =>` on a variant: the name that binds the fields of the
       case, empty without one. The checker sets the symbol and the index
       of the case plus 1. */
    struct name binds;
    struct pos binds_pos;
    struct symbol *bound;
    uint32_t variant_case;
    /* Set by the checker on a `switch` over a `str`: the call of
       `anti.text.equal` on the value and this arm's. */
    struct expr *test;
};

struct if_branch {
    struct expr *cond;
    struct block *body;
};

enum stmt_kind {
    STMT_LET,
    STMT_CONST,
    STMT_EXPR,
    STMT_ASSIGN,
    STMT_IF,
    STMT_WHILE,
    STMT_DO_WHILE,
    STMT_FOR,
    STMT_DEFER,
    STMT_UNDO,
    STMT_FAIL,
    STMT_SWITCH,
    STMT_ASSERT,
    STMT_BREAK,
    STMT_CONTINUE,
    STMT_RETURN,
    STMT_YIELD,
    STMT_TRY,
    STMT_FALLTHROUGH,
    STMT_BLOCK,
    STMT_SYNC,
    STMT_SELECT
};



struct stmt {
    enum stmt_kind kind;
    struct pos pos;
    /* DESIGN: whether the statement leaves the enclosing blocks through
       an error, which is what `undo` runs on. `fail` always does, and so
       does a `return` of an `*Error` in a function that returns
       `?*Error`. The checker knows the types and decides it once, so
       lowering reads one flag. */
    bool error_exit;
    union {
        struct {
            struct name name;
            struct pos name_pos;
            struct type_expr *type; /* NULL when a let omits it */
            struct expr *value;
            struct symbol *symbol;
            /* `let m = p else { }`: the block that runs when p is
               `none`, and which leaves the block the `let` stands in. */
            struct block *otherwise;
            /* `let m = p catch fatal` and `let m = p catch e { }`: the
               handler that runs when p is `none`, with the error
               `anti.lang.NoneDereference`. */
            struct handler guard;
            /* `anti.lang.NoneDereference.new`, which the guard calls to
               build the error it hands the handler. */
            struct symbol *guard_make;
            /* `let (a, b) = e;`: the names that take the elements of the
               tuple apart. The list is empty in the one-name form, and
               the `let` then binds `name` itself. A destructuring binds
               no name of its own, so `symbol` is the value the names
               come from and stands in no scope. */
            struct binding *names;
            size_t name_count;
        } let;                      /* STMT_LET, STMT_CONST */
        struct expr *expr;          /* STMT_EXPR */
        struct {
            enum token_kind op;     /* TOKEN_ASSIGN or a compound op */
            struct expr *target;
            struct expr *value;
        } assign;
        struct {
            struct if_branch *branches;
            size_t count;
            struct block *else_body; /* NULL without else */
        } if_chain;
        struct {
            struct expr *cond;
            struct block *body;
        } loop;                     /* STMT_WHILE, STMT_DO_WHILE */
        /* `for i in lo..hi` and `for x in slice`. over is set for the
           second form, and by_pointer for `for x in &slice`. The binding
           is optional in the range form, and step holds the constant of
           `by k`, which walks the set backwards when it is negative. */
        struct {
            /* The names the loop binds, the element last. A range names
               its counter or nothing, `for x in items` names its element,
               and `for i, x in items` destructures the `(int, T)` of each
               element into the index and the element. */
            struct binding *names;
            size_t name_count;
            struct expr *low;
            struct expr *high;
            struct expr *over;
            struct expr *step;
            struct pos step_pos;
            int64_t step_value;         /* the folded `by k`, or 1 */
            bool by_pointer;
            struct block *body;
        } for_loop;
        struct stmt *deferred;      /* STMT_DEFER, STMT_UNDO */
        /* `fail e;` and `fail "text";`. The second form names the
           `anti.lang.Error.new` that builds the error from the text,
           and value is then the text. */
        struct {
            struct expr *value;
            struct symbol *make;
            /* DESIGN: the class `anti.lang.Error`, whose `at` the
               statement writes when it holds no position yet, and
               `StackTrace.capture`, which fills `frames` there when
               backtraces are on. The checker resolves both, so lowering
               reads the fields and calls the function directly. */
            const struct type *error;
            struct symbol *capture;
        } fail;
        struct expr *yielded;       /* STMT_YIELD, NULL without a value */
        /* `try { } catch e { }`: every failing call of the body reaches
           the handler, and the first error abandons the rest. */
        struct {
            struct block *body;
            struct handler handler;
        } try_block;
        /* `assert(cond)` and `assert(cond, "message")`. text is the
           source of the condition, for the failure message. */
        struct {
            struct expr *cond;
            struct token_text message;
            struct token_text text;
        } assertion;
        /* `switch e { A => stmt, else => stmt }`. An arm holds one value
           and one statement, and the else arm has no value. otherwise_at
           is the number of arms the text writes before `else`. */
        struct {
            struct expr *value;
            struct switch_arm *arms;
            size_t count;
            struct stmt *otherwise;
            size_t otherwise_at;
            /* The local that holds the value of a `switch` on a `str`,
               which each arm's test reads. Set by the checker. */
            struct symbol *bound;
            /* DESIGN: `if let Circle c = s { } else { }` is a switch on
               s with one arm and an `else`, which is empty when the text
               writes none. The flag names the form for the messages and
               for the missing-return rule, which reads it as an `if`. */
            bool if_let;
        } switch_stmt;
        struct expr *return_value;  /* STMT_RETURN, NULL for return; */
        struct block *block;        /* STMT_BLOCK */
        /* `sync m { }`: the mutex, a `Mutex` or a pointer to one, and the
           block that holds it. */
        struct {
            struct expr *mutex;
            struct block *body;
        } sync;                     /* STMT_SYNC */
        /* `select { a x => stmt, b => stmt }`. The value of an arm is its
           channel, and binds names what the channel gives, a `?*T`. */
        struct {
            struct switch_arm *arms;
            size_t count;
        } select;                   /* STMT_SELECT */
    } as;
};

/* The text of the doc comments before a node, empty without one. The
   line form and the block form of a marker give the same text. */
struct doc_text {
    const char *text;
    size_t length;
};

/* How a field joins its class. A plain field is its own name. A `use`
   field promotes the names of its type onto the class that holds it, and
   an `implements` field holds the sub-object of an interface. The checker
   adds two more that no declaration writes. One is the base of a class,
   named by `inherits` and carried as the field `super`. The other is the
   table pointer of the root `anti.lang.Object`. */
enum field_form { FIELD_PLAIN, FIELD_USE, FIELD_BASE, FIELD_TABLE,
                  FIELD_IMPL };

/* DESIGN: the four levels of the object model document. A module item is
   private, internal or public. A class member is private, protected or
   public. The two ends of the range are shared, and the middle level
   differs, so one enumeration serves both. */
enum visibility { VIS_PRIVATE, VIS_PROTECTED, VIS_INTERNAL, VIS_PUB };

/* A parameter, or a field of a struct declaration. */
struct param {
    struct name name;
    struct pos pos;
    struct type_expr *type;
    struct symbol *symbol;
    struct doc_text doc;            /* fields only */
    struct doc_text note;           /* fields only */
    struct expr *bits;              /* the width of a bitfield, or NULL */
    /* A field default, an enum value or the default of a parameter. */
    struct expr *value;
    enum field_form form;           /* fields only */
    enum visibility vis;            /* fields only */
    bool owned;                     /* `own`: the object frees the memory */
    bool transient;                 /* `transient`: derived state */
    bool atomic;                    /* `atomic`: read and written by calls */
    bool writable;                  /* `mutable`: a singleton field to write */
    bool injected;                  /* `inject`: a provider fills the field */
    bool inject_final;              /* `inject final`: no replacement */
};

/* One case of a variant: its name and its fields, none for a case such
   as `Empty`. */
struct variant_case {
    struct name name;
    struct pos pos;
    struct doc_text doc;            /* the /// text */
    struct param *fields;
    size_t field_count;
};

enum item_kind {
    ITEM_FN,
    ITEM_EXTERN_FN,
    ITEM_STRUCT,
    ITEM_UNION,
    ITEM_CONST,
    ITEM_ENUM,
    ITEM_CLASS,
    ITEM_VARIANT
};

/* A function of a struct body against the contracts of its chain. */
enum fn_contract { FN_PLAIN, FN_ABSTRACT, FN_CONCRETE };

/* The module-level block an ITEM_FN was written in. `anti test`
   compiles both, and every other build drops them. */
enum fn_block { BLOCK_NONE, BLOCK_TESTS, BLOCK_FIXTURES };

struct item {
    enum item_kind kind;
    struct pos pos;
    bool pub;
    bool exported;                  /* export, which implies pub */
    struct name name;
    struct pos name_pos;
    struct param *params;           /* parameters, or fields of a struct or union */
    size_t param_count;
    bool variadic;                  /* ITEM_EXTERN_FN */
    bool worker;                    /* ITEM_FN, may run on a worker */
    struct type_expr *result;       /* NULL without a result */
    /* `may fail` after the signature. The ABI is then `?*Error f(args,
       R *out)`, with `out` absent without a result. */
    bool may_fail;
    struct pos may_fail_pos;
    struct block *body;             /* ITEM_FN */
    struct type_expr *type;         /* ITEM_CONST */
    struct expr *value;             /* ITEM_CONST */
    bool packed;                    /* ITEM_STRUCT, ITEM_UNION, ITEM_VARIANT */
    bool simd;                      /* ITEM_STRUCT: a `simd struct` */
    struct expr *align;             /* the same three, or NULL */
    struct variant_case *cases;     /* ITEM_VARIANT */
    size_t case_count;
    struct symbol *symbol;
    struct doc_text doc;            /* the /// text */
    struct doc_text note;           /* the //# text */
    struct item **members;          /* the functions and constants of a body */
    size_t member_count;
    enum fn_contract contract;      /* ITEM_FN */
    enum fn_block block;            /* ITEM_FN: `tests` or `fixtures` */
    bool has_self;                  /* ITEM_FN: self is its first parameter */
    struct symbol *self;            /* ITEM_FN: the symbol of self */
    const char *runtime;            /* ITEM_FN of the root: its C symbol. */
    const struct item *owner;       /* the class or enum that declares it. */
    struct type_expr *base;         /* ITEM_ENUM: the underlying type, or NULL */
    struct name base_name;          /* ITEM_CLASS: the base after `inherits` */
    struct name base_module;        /* ITEM_CLASS: the module of the base,
                                       empty when unqualified. */
    struct pos base_pos;
    bool is_abstract;               /* ITEM_CLASS, or ITEM_FN in a body */
    bool is_final;                  /* ITEM_CLASS, ITEM_FN */
    bool is_static;                 /* a static atomic field of a class. */
    bool atomic;                    /* ITEM_CONST with is_static */
    bool is_singleton;              /* ITEM_CLASS with one instance */
    bool singleton_get;             /* the generated `get` of a singleton */
    bool is_operator;               /* ITEM_FN that an operator calls */
    /* DESIGN: the contextual `trace` marks a class whose `pub` functions
       want the call hooks, or one function of a class. It follows the
       `assert` rule: on in dev mode and off in release, with `--trace`
       and `--no-trace` deciding instead. */
    bool trace;                     /* ITEM_CLASS, ITEM_FN in a body */
    enum visibility vis;
    struct name qualifier;          /* `concrete fn X::f`, the X */
    struct pos qualifier_pos;
};

struct import {
    struct pos pos;
    struct pos module_pos;
    struct name module;
    struct name alias;              /* empty without as */
};

/* `provides Interface as Class;` at module level: one interface a
   library offers and the class it gives for it. The interface is one
   path, qualified by a module alias, by a whole module path, or by
   nothing. */
struct provides {
    struct pos pos;
    struct name qualifier;          /* empty when the path has one name */
    struct name interface;
    struct pos interface_pos;
    struct name class_name;
    struct pos class_pos;
    /* Filled by the checker: the interface and the class it names. */
    const struct type *type;
    const struct type *class_type;
};

/* A doc comment that no item, field or module took. Its text is lost, so
   `--doc-warnings` reports it. */
struct dropped_doc {
    struct pos pos;
    struct name marker;             /* the marker, as the reader wrote it */
    bool module_form;               /* `//!` or `//#!`, which a module takes */
};

struct module {
    const char *file;               /* the source path, for a message */
    struct doc_text doc;            /* the `//!` text */
    struct doc_text note;           /* the `//#!` text */
    struct import *imports;
    size_t import_count;
    struct item **items;
    size_t item_count;
    struct provides *provides;
    size_t provides_count;
    struct dropped_doc *dropped;
    size_t dropped_count;
};

/* Append the tree of module to out, one node per line, indented by two
   spaces per level. Chapter 1 shows this output for the function scale. */
void ast_dump(struct text *out, const struct module *module);

#endif

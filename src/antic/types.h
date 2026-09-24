#ifndef ANTIC_TYPES_H
#define ANTIC_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "arena.h"
#include "ast.h"
#include "text.h"

/* The checked types of a compilation. Every type exists once, so two
   types are the same type exactly when their pointers are equal. Structs
   are the exception chapter 2 makes: each declaration is a new type. */

enum type_kind {
    TYPE_VOID,      /* the result of a function without -> R */
    TYPE_BOOL,
    TYPE_CHAR,
    TYPE_I8,
    TYPE_I16,
    TYPE_I32,
    TYPE_I64,
    TYPE_CLONG,     /* 32 bits on Windows, 64 bits elsewhere */
    TYPE_U8,
    TYPE_U16,
    TYPE_U32,
    TYPE_U64,
    TYPE_CULONG,    /* 32 bits on Windows, 64 bits elsewhere */
    TYPE_CWCHAR,    /* 16 bits on Windows, 32 bits elsewhere, unsigned */
    /* DESIGN: f16 is storage. It is sixteen bits in memory, a read gives
       an f32 and `as f16` makes one from an f32. It has no arithmetic,
       so no predicate of the float types holds for it. */
    TYPE_F16,
    TYPE_F32,
    TYPE_F64,
    TYPE_STR,
    TYPE_NONE,      /* the type of `none` before a context gives it one */
    TYPE_ERROR,     /* an expression that already produced a diagnostic */
    TYPE_BUILTIN_COUNT,
    TYPE_POINTER = TYPE_BUILTIN_COUNT,
    TYPE_ARRAY,
    TYPE_SLICE,
    TYPE_FN,
    TYPE_STRUCT,
    TYPE_ENUM,      /* a named integer type with its own namespace */
    TYPE_CLASS,     /* a table pointer, a base at offset 0, then fields. */
    /* DESIGN: a tuple is an anonymous struct with C layout. It is a type
       with fields, and every rule of layout, passing and returning is
       the struct's. It carries no module and no name. Two tuples of the
       same elements in the same order are one type, so the elements
       intern it. */
    TYPE_TUPLE,
    /* DESIGN: a variant is a struct of a tag and a union of its cases,
       with C layout. Its fields are the ones C sees. `tag` has the enum
       of its cases, and `u` is a union with one struct per case that has
       fields, named by the case. A variant whose cases have no fields
       has no `u`. base is the enum of the tags. Its values number the
       cases from 0 in the order of the declaration. params holds the
       struct of each case in that order, NULL for a case without fields,
       and param_count is the number of cases. Every rule of layout,
       passing and returning is the struct's, so the passes after the
       checker see a struct. */
    TYPE_VARIANT
};

struct struct_field {
    struct name name;
    struct pos pos;
    struct type *type;
    uint8_t bits;                   /* the width of a bitfield, or 0 */
    struct doc_text doc;            /* the /// text */
    enum field_form form;           /* plain, use, base, table or impl */
    enum visibility vis;            /* the level the declaration gave it. */
    const struct type *home;        /* the struct or class that declares it */
    bool owned;                     /* `own`: the object frees the memory */
    bool transient;                 /* `transient`: derived state */
    bool atomic;                    /* `atomic`: read and written by calls */
    bool writable;                  /* `mutable`: a singleton field to write. */
    /* DESIGN: `inject` fills the field from the provider of its
       interface before `construct` runs. No literal writes it, and the
       class never builds what it points at. `inject final` keeps the
       run-time configuration from replacing the provider. */
    bool injected;
    bool inject_final;
    /* DESIGN: `guarded by lock` names the Mutex field that a `sync`
       holds wherever the field is reached. guard_class is NULL for a
       lock of the same object, and the enclosing class for
       `guarded by PeopleList.lock`. unchecked marks a field that
       `unchecked(unguarded-field)` follows, which the check passes over.
       hidden marks the lock of a synchronized class, which no program
       names. */
    struct name guard;
    const struct type *guard_class;
    bool unchecked;
    bool hidden;
    const struct expr *value;       /* a field default or an enum value */
    /* DESIGN: the value of a field default, which the checker evaluates.
       A library file carries it, so a module that builds a class of
       another module writes the defaults that module declared. */
    const struct const_value *constant;
    uint64_t number;                /* TYPE_ENUM: the value of the name */
};

enum layout_state { LAYOUT_NONE, LAYOUT_BUSY, LAYOUT_DONE };

enum thread_safety { SAFETY_NONE, SAFETY_SYNCHRONIZED, SAFETY_CONCURRENT };

enum symbolic_kind {
    SYMBOLIC_INT,
    SYMBOLIC_SIZE_OF,
    SYMBOLIC_UNARY,
    SYMBOLIC_BINARY,
    SYMBOLIC_CAST
};

/* A constant integer or bool whose value depends on the target, because
   it is computed from size_of. Nodes are interned, so two equal values
   are one pointer. */
struct symbolic {
    enum symbolic_kind kind;
    struct type *type;
    uint64_t value;                 /* SYMBOLIC_INT */
    struct type *of;                /* SYMBOLIC_SIZE_OF */
    enum token_kind op;             /* SYMBOLIC_UNARY, SYMBOLIC_BINARY */
    const struct symbolic *a;       /* the operand of an operation or cast */
    const struct symbolic *b;       /* SYMBOLIC_BINARY */
    struct symbolic *next;          /* the list of interned nodes */
};

struct type {
    enum type_kind kind;
    struct type *element;           /* TYPE_POINTER, TYPE_ARRAY, TYPE_SLICE */
    /* DESIGN: `?*T` is a pointer that may hold `none` and `*T` is one
       that never does. The two are distinct types, so the difference is
       a pointer comparison like every other, and the flag belongs to the
       key that interns them. They have the same layout: nothing is
       emitted for the difference, and the checks are the ones the
       program wrote. */
    bool nullable;                  /* TYPE_POINTER and TYPE_FN: `?*T` */
    uint64_t length;                /* TYPE_ARRAY, 0 when symbolic */
    const struct symbolic *length_of; /* TYPE_ARRAY, a symbolic length */
    struct type **params;           /* TYPE_FN */
    bool bound;                     /* TYPE_FN: an object and an entry */
    /* DESIGN: a parameter of function type that does not keep its
       argument holds two words, the code and a context pointer. Its
       type is the function type with context set, so a value of it
       converts to nothing that keeps it: a field, a global, a result
       and a `keep` parameter hold the one C function pointer. A named
       function converts to it with the context `none`. concurrent marks
       the form of a `concurrent` parameter, which a closure reaches only
       when it writes no captured variable of a type that is not
       thread-safe. */
    bool context;                   /* TYPE_FN: the code and a context */
    bool concurrent;                /* TYPE_FN, context: `concurrent` */
    /* DESIGN: `own fn(...)` is the form of two words that owns its
       context, the address of a snapshot on the heap, and frees it with
       its owner. An `own` field and a `keep own` parameter hold it. It is
       read-only, so it is `concurrent` as well, and it lends itself to a
       parameter of the form of two words. Nothing else converts to it
       but a snapshot, a function that captures nothing and `dup`. */
    bool owned;                     /* TYPE_FN, context: `own fn` */
    /* DESIGN: `fn(A) -> R may fail` is a type of its own. It holds the
       ABI form that the checker gives a `may fail` function: `?*Error` as
       the result, and the out pointer last when the type names a result.
       The passes after the checker then see an ordinary signature. Both
       flags belong to the key that interns the type, so
       `fn(A, *R) may fail` and `fn(A) -> R may fail` are two types. */
    bool may_fail;                  /* TYPE_FN: written `may fail` */
    bool has_out;                   /* TYPE_FN, may_fail: the out pointer */
    size_t param_count;
    struct type *result;            /* TYPE_FN, TYPE_VOID without a result */
    struct name module;             /* TYPE_STRUCT */
    struct name name;               /* TYPE_STRUCT */
    struct struct_field *fields;    /* TYPE_STRUCT */
    size_t field_count;

    /* The modifiers of a struct or union: union, packed, the N of
       align(N) or 0, and export. */
    bool is_union;
    bool packed;
    uint64_t align;
    bool item_exported;

    /* The object model. A struct or an enum owns the functions and
       constants of its body. A struct that declares or inherits an
       abstract function is reached through a pointer of two words. */
    struct item **members;
    size_t member_count;
    struct type *base;              /* TYPE_ENUM: the underlying integer,
                                       TYPE_CLASS: the class it inherits */
    bool has_abstract;              /* TYPE_CLASS: an open function */
    bool is_final;                  /* TYPE_CLASS: no class inherits it */
    /* DESIGN: the contextual `trace` on the class. A library file carries
       it, so a write to a field of a class of another module is
       instrumented where the write stands. */
    bool traced;                    /* TYPE_CLASS: written `trace class` */
    /* DESIGN: a thread-safe class. A synchronized class runs every
       function that is not private under one hidden lock. A concurrent
       class has every field guarded, atomic or fixed, and a type nested
       in one carries the mark so its fields are checked the same way.
       unchecked_fields marks `unchecked(unguarded-field)` in the class
       header, which the check of every field passes over. */
    enum thread_safety safety;      /* TYPE_CLASS, TYPE_STRUCT */
    bool unchecked_fields;
    /* The TYPE_U32 that is the word of a Mutex, which lowering gives the
       IR type of the lock word of each target. */
    bool lock_word;
    /* DESIGN: `compatible 1.1;` in the body of an abstract class names
       the lowest version a plugin may have been built for. The
       descriptor of the class carries it, so the loader reads it where
       the program runs, and a library file carries it, so every module
       writes the same descriptor. It is empty where the body has no
       such line. */
    struct name compatible;         /* TYPE_CLASS: `compatible <version>` */

    /* DESIGN: a `simd struct` is a struct whose fields are its lanes, of
       one primitive type. The checker gives its operators, and the back
       end its alignment and its registers. A comparison of two of them
       gives a mask, a simd struct of `bool` with the same fields, which
       no program declares. mask holds it, made on the first comparison. */
    bool simd;                      /* TYPE_STRUCT: a simd struct or a mask */
    struct type *mask;              /* TYPE_STRUCT, simd: its mask, or NULL */

    enum layout_state layout;       /* TYPE_STRUCT, for the cycle check */
    struct type *next;              /* the list of derived types */
};

struct types {
    struct arena *arena;
    struct type builtins[TYPE_BUILTIN_COUNT];
    struct type *derived;
    struct symbolic *symbolics;
    struct type *object;            /* anti.lang.Object, the class root */
    struct type *flags;             /* anti.lang.Flags */
    struct type *mutex;             /* anti.lang.Mutex */
    struct type *object_lock;       /* the hidden lock of a class */
    struct type *field_record;      /* anti.lang.FieldDescriptor */
};

void types_init(struct types *types, struct arena *arena);
/* Memory for count items of size bytes each, zeroed, in the pool, which
   frees it. antic stops when the product does not fit in a size_t, as on
   any failed allocation. */
void *types_alloc_array(struct arena *arena, size_t count, size_t size);
struct type *types_builtin(struct types *types, enum type_kind kind);
/* `*T`, the pointer that never holds `none`. */
struct type *types_pointer(struct types *types, struct type *element);
/* `?*T`, the pointer that may hold `none`. */
struct type *types_pointer_nullable(struct types *types,
                                    struct type *element);
/* Either of the two, for a caller that carries the answer in a value. */
struct type *types_pointer_of(struct types *types, struct type *element,
                              bool nullable);
/* Whether t is `?*T` or `?fn(...)`. */
bool type_is_nullable(const struct type *t);
/* The same type without `none`: `*T` of a `?*T`, `fn()` of a `?fn()`. */
struct type *types_without_none(struct types *types, struct type *t);
/* The same type with `none`, for a pointer or a function type. */
struct type *types_with_none(struct types *types, struct type *t);
struct type *types_array(struct types *types, struct type *element,
                         uint64_t length);
struct type *types_slice(struct types *types, struct type *element);
/* An array whose length is a symbolic value. */
struct type *types_array_symbolic(struct types *types, struct type *element,
                                  const struct symbolic *length);
/* The interned node equal to key. */
const struct symbolic *types_symbolic(struct types *types,
                                      const struct symbolic *key);
/* Append a symbolic value as a program writes it. With qualified set, a
   struct name carries its module. */
void symbolic_print(struct text *out, const struct symbolic *s,
                    bool qualified);
struct type *types_fn(struct types *types, struct type *const *params,
                      size_t param_count, struct type *result);
/* A function type in the ABI form of `may fail`: params end with the out
   pointer when has_out is set, and result is `?*Error`. */
struct type *types_fn_failing(struct types *types, struct type *const *params,
                              size_t param_count, struct type *result,
                              bool has_out);
/* A function type with each flag given, as a library file records it. */
struct type *types_fn_flagged(struct types *types, struct type *const *params,
                              size_t param_count, struct type *result,
                              bool bound, bool may_fail, bool has_out);
/* The same function type in another form. With context set it is the
   form of a parameter that does not keep its argument, `concurrent` or
   not. Without, it is the plain form of one C function pointer. The `?`
   stays. */
struct type *types_fn_form(struct types *types, struct type *fn, bool context,
                           bool concurrent);
/* The same function type as `own fn(...)`, the form of two words that
   owns its snapshot. */
struct type *types_fn_owned(struct types *types, struct type *fn);

/* DESIGN: a bound function is a value of two words, the object and the
   entry of the table. Its type is the function type without `self`. It
   is a distinct type from the plain function pointer of that signature,
   because the two have different layouts. It is made from the type of
   the method, whose first parameter is `self`, and keeps its `may fail`. */
struct type *types_bound_of(struct types *types, const struct type *fn);

/* A new struct type without fields. Each call returns a distinct type. */
struct type *types_object(struct types *types);

/* DESIGN: `anti.lang` is the root of the standard library and imports
   nothing, so every module can name its classes without a cycle. The
   compiler knows these of them by name: `Error`, which a failing function
   returns, `NoneDereference`, the error of a `catch` on a `?*T`,
   `SourceLocation`, the value of `here`, and `StackTrace`, whose
   `capture` a `fail` calls. It declares two more of the module itself:
   `Object`, the root of every class chain, and `Job`, which `dispatch`
   gives. The names are defined here and nowhere else, and so are the
   fields of them that the compiler writes. */
#define LANG_MODULE "anti.lang"
#define LANG_OBJECT "Object"
#define LANG_JOB "Job"
#define LANG_ERROR "Error"
#define LANG_NONE_DEREFERENCE "NoneDereference"
#define LANG_SOURCE_LOCATION "SourceLocation"
#define LANG_STACK_TRACE "StackTrace"
#define LANG_TRACE_CAPTURE "capture"
#define LANG_ERROR_AT "at"
#define LANG_ERROR_FRAMES "frames"
#define LANG_LOCATION_FILE "file"
#define LANG_LOCATION_LINE "line"
#define LANG_LOCATION_COLUMN "column"
#define LANG_LOCATION_FUNCTION "function"
#define LANG_LOCATION_MODULE "module"

/* DESIGN: `Flags` is the built-in struct of the flags form, which the
   compiler declares in `anti.lang` as it does `Object`. Its four bools
   stand in this order, which lowering follows when it reads a flag. A
   type of that name in the module wins over it, as a class named
   `Object` does. It carries no descriptor, since no module declares it. */
#define LANG_FLAGS "Flags"
#define FLAGS_OVERFLOW "overflow"
#define FLAGS_CARRY "carry"
#define FLAGS_ZERO "zero"
#define FLAGS_NEGATIVE "negative"

/* DESIGN: `Mutex` is the built-in struct of `sync`, which the compiler
   declares in `anti.lang` as it does `Flags`. A channel is `chan T`, a
   struct of `anti.lang` per element type, whose name is the keyword, so
   no module declares one. A Mutex holds one field, the lock word of the
   system, and cannot be copied. A channel holds the handle of the object
   the runtime makes, so a copy names the same channel. A type
   named `Mutex` in the module wins over the built-in one, and so does a
   function named `close` over the built-in `close(c)`. Neither carries a
   descriptor, since no module declares them. */
#define LANG_MUTEX "Mutex"
#define LANG_CHAN "chan"
#define SYNC_HANDLE "handle"
#define MUTEX_WORD "word"
/* The struct of the hidden lock of a synchronized class, and the name of
   its field. Neither is a name the lexer reads, so no program spells
   them. */
#define LANG_OBJECT_LOCK "Object lock"
#define HIDDEN_LOCK "(lock)"
#define MUTEX_NEW "new"
#define MUTEX_DESTROY "destroy"
#define CHAN_CLOSE "close"

/* DESIGN: `FieldDescriptor` is the built-in struct that the `changed`
   hook takes, which the compiler declares in `anti.lang` as it does
   `Flags`. Its five fields are one record of a class's field list, in
   the order `struct anti_field` of `src/rt/object.h` holds them, so a hook
   reads the record the descriptor already carries. The compiler declares
   it, so no import is needed where a class replaces `changed`, and it
   carries no descriptor, since no module declares it. */
#define LANG_FIELD_DESCRIPTOR "FieldDescriptor"
#define FIELD_RECORD_NAME "name"
#define FIELD_RECORD_OFFSET "offset"
#define FIELD_RECORD_TYPE "type_id"
#define FIELD_RECORD_OWNED "owned"
#define FIELD_RECORD_DESCRIPTOR "descriptor"

/* DESIGN: an `f"..."` builds its text with `anti.text.Builder`, which
   the compiler knows by name, with the enum of its alignments and the
   functions the literal calls. The names are defined here and nowhere
   else. `to_text` of the root class writes an object. */
#define TEXT_MODULE "anti.text"
#define TEXT_BUILDER "Builder"
#define TEXT_NEW "new"
#define TEXT_APPEND "append"
#define TEXT_APPEND_INT "append_int"
#define TEXT_APPEND_UINT "append_uint"
#define TEXT_APPEND_FLOAT "append_float"
#define TEXT_APPEND_F32 "append_f32"
#define TEXT_APPEND_BOOL "append_bool"
#define TEXT_APPEND_CHAR "append_char"
#define TEXT_APPEND_TEXT "append_text"
#define TEXT_TAKE "take"
#define TEXT_ALIGN "Align"
#define TEXT_ALIGN_LEFT "Left"
#define TEXT_ALIGN_RIGHT "Right"
#define TEXT_ALIGN_CENTER "Center"
/* A `switch` on a `str` compares the value with each arm by this
   function of the same module. */
#define TEXT_EQUAL "equal"
#define ROOT_TO_TEXT "to_text"
/* `Object.deserialize` takes its memory from an allocator of this
   class, and `delete(p, from)` and `destroy(p, from)` give memory back
   to one. */
#define MEM_MODULE "anti.mem"
#define MEM_ALLOCATOR "Allocator"
#define ROOT_DESERIALIZE "deserialize"

/* DESIGN: `anti.lang.Object` declares nine hooks with empty bodies after
   its seven functions. They take the entries after those in the table of
   every class, in this order, which `enum anti_hook` of `src/rt/object.h`
   repeats and the unit test `records_hook_entries` pins. The names are
   defined here and nowhere else. */
#define ROOT_CREATED "created"
#define ROOT_DESTROYED "destroyed"
#define ROOT_COPIED "copied"
#define ROOT_DISPATCHED "dispatched"
#define ROOT_JOINED "joined"
#define ROOT_ENTER "enter"
#define ROOT_LEAVE "leave"
#define ROOT_FAILED "failed"
#define ROOT_CHANGED "changed"
/* `anti.lang.TraceHandler` declares the same nine, each taking the
   object after `self`. `anti.lang.Trace` installs one handler. */
#define LANG_TRACE_HANDLER "TraceHandler"
#define LANG_TRACE "Trace"
#define TRACE_INSTALL "install"

/* Whether t is the class `anti.lang.Error` itself. */
bool types_is_lang_error(const struct type *t);

/* DESIGN: a `concrete fn` fills the tables its qualifier names. One
   without a qualifier, or qualified by its own class, is a plain body. It
   fills the primary table. It also fills each interface table of its name
   that no qualified body of its level fills. The nearest body wins, so a
   plain body replaces the qualified bodies of the levels above it. A body
   qualified by a class of the base chain fills the primary table alone.
   It wins there over a plain body of the same level. Any other qualifier
   names an interface, and the body fills its table alone. The checker,
   lowering and the header read the tables through the functions below,
   so the three agree. */
enum body_table {
    BODY_PLAIN,
    BODY_BASE,
    BODY_INTERFACE
};

/* The tables that the member m of the level t of a chain fills. */
enum body_table types_body_table(const struct type *t, const struct item *m);
/* The function of the chain of t that the primary table holds under
   name. It sits at the level nearest to t that declares one, and there a
   body qualified by a base comes before a plain one. NULL when no level
   declares the name outside the table of an interface. */
const struct item *types_primary_member(const struct type *t,
                                        const struct name *name);
/* The level of the chain of t that declares the member m, or NULL. */
const struct type *types_member_level(const struct type *t,
                                      const struct item *m);
/* Whether m holds its entry in the primary table of its level of the
   chain of t. A plain body beside one qualified by a base holds none. */
bool types_holds_entry(const struct type *t, const struct item *m);
/* The public function of the chain of t that fills the entry name of the
   table of iface. It sits at the level nearest to t that declares one.
   There a body qualified by a class of the chain of iface comes before a
   plain one. */
const struct item *types_interface_member(const struct type *t,
                                          const struct type *iface,
                                          const struct name *name);
/* The symbol name of the member m of the class named owner. It is `T.f`,
   and `T.Q.f` for a body qualified by another class, so that two bodies
   of one name have two symbols. The text lies in the memory pool and ends
   in a NUL, and the pool frees it. */
struct name types_member_symbol(struct arena *arena, const struct name *owner,
                                const struct item *m);

/* DESIGN: `dispatch` gives a Job back, and `join` of it gives the result
   of the worker. The result therefore belongs to the type. A Job of one
   result type is a distinct type from a Job of another, and every one
   has the layout of one pointer. */
struct type *types_job(struct types *types, struct type *result);
/* Whether t is a Job that types_job made. */
bool types_is_job(const struct type *t);
/* The struct `anti.lang.Flags`, one for the compilation. */
struct type *types_flags(struct types *types);
/* Whether t is the struct that types_flags made. */
bool types_is_flags(const struct type *t);
/* The struct `anti.lang.FieldDescriptor`, one for the compilation. */
struct type *types_field_descriptor(struct types *types);
/* Whether t is the struct that types_field_descriptor made. */
bool types_is_field_descriptor(const struct type *t);
/* The struct `anti.lang.Mutex`, one for the compilation. */
struct type *types_mutex(struct types *types);
struct type *types_object_lock(struct types *types);
bool types_is_object_lock(const struct type *t);
/* Whether t is the struct that types_mutex made. */
bool types_is_mutex(const struct type *t);
/* `chan T`, one per element type. */
struct type *types_chan(struct types *types, struct type *element);
/* Whether t is a channel that types_chan made. */
bool types_is_chan(const struct type *t);
/* DESIGN: `simd.select`, `simd.any` and `simd.all` take any simd
   struct, which no function of Anti can, so the compiler knows them by
   name in `anti.simd`. A module that calls one imports that module. The
   names are defined here and nowhere else, and so are the names of the
   built-ins on a simd struct and its values. */
#define SIMD_MODULE "anti.simd"
#define SIMD_SELECT "select"
#define SIMD_ANY "any"
#define SIMD_ALL "all"
#define SIMD_SPLAT "splat"
#define SIMD_LOAD "load"
#define SIMD_STORE "store"
#define SIMD_SHUFFLE "shuffle"
#define SIMD_SUM "sum"
#define SIMD_MIN "min"
#define SIMD_MAX "max"
#define SIMD_DOT "dot"

/* The mask of the simd struct s: a simd struct of `bool` with the names
   of the fields of s, one for the compilation. */
struct type *types_mask(struct types *types, struct type *s);
/* Whether t is a simd struct, a mask among them. */
bool type_is_simd(const struct type *t);
/* Whether t is a simd struct of `bool`, which a comparison gives. */
bool type_is_mask(const struct type *t);
/* The type of the lanes of the simd struct t. */
struct type *type_simd_lane(const struct type *t);
/* The bytes of a lane of type t, or 0 for a type that is no lane.
   DESIGN: a lane has one width on every target, so the size of a simd
   struct is a property of its declaration. c_long, c_ulong and c_wchar,
   whose width the target decides, are no lanes. */
uint64_t type_lane_bytes(const struct type *t);
/* The bytes of the simd struct t, its lanes without padding. */
uint64_t type_simd_bytes(const struct type *t);

/* The tuple of the element types, interned. Its fields are `_0`, `_1`
   and on, in the order the elements were written. */
struct type *types_tuple(struct types *types, struct type *const *elements,
                         size_t count);
struct type *types_struct(struct types *types, struct name module,
                          struct name name);
/* The name of the field of a variant that holds its tag, and of the one
   that holds the union of its cases. */
#define VARIANT_TAG "tag"
#define VARIANT_UNION "u"

/* Give the variant v its tag of type tag and the union of the structs
   of its cases. payloads[i] is the struct of case i, or NULL for a case
   without fields. */
void types_set_cases(struct types *types, struct type *v, struct type *tag,
                     struct type *const *payloads, size_t count);
/* Read base and params of the variant v back from its fields, as a
   library file carries them. False when the fields are not those of a
   variant. */
bool types_cases_from_fields(struct types *types, struct type *v);
/* Whether the variant v has the case name. If so, index receives the
   index of the case. */
bool types_case_index(const struct type *v, const struct name *name,
                      size_t *index);

/* A named integer type over base. Each call returns a distinct type. */
struct type *types_enum(struct types *types, struct name module,
                        struct name name, struct type *base);
void types_set_fields(struct types *types, struct type *s,
                      const struct struct_field *fields, size_t count);

/* DESIGN: the front end computes no layout, because the back end lays out
   types for its target. The front end checks only that no struct contains
   itself by value, directly or through size_of in an array length.
   Returns NULL, or the struct that contains itself. */
struct type *types_find_cycle(struct type *s);

/* DESIGN: a struct that contains itself has been reported, and the
   checker goes on to find more. Every field of s, or of a struct inside
   it, that closes a cycle takes the type error. No later walk over fields
   then recurses without end, and each use of such a field is quiet. */
void types_break_cycles(struct type *s, struct type *error);

/* The name of t as a program writes it, with int, float and byte for the
   aliased types. */
void type_name(struct text *out, const struct type *t);
/* The name of t with the module of every struct in it, as main.Vec2. */
void type_name_qualified(struct text *out, const struct type *t);

bool type_is_integer(const struct type *t);
/* Whether f is a zero-width bitfield, written `_: T : 0`, which breaks the
   unit of the bitfields and holds no value. */
bool type_field_is_unit_break(const struct struct_field *f);
/* c_long, c_ulong and c_wchar, whose width the target decides. */
bool type_is_target_sized(const struct type *t);
bool type_is_signed(const struct type *t);
bool type_is_float(const struct type *t);
bool type_is_numeric(const struct type *t);
/* The width in bits. A target-sized type has the narrower of its widths,
   the range a value must fit on every target. */
int type_bits(const struct type *t);

/* A type with no pointer inside it, the property the threading chapter
   needs. str counts as pointer-free, because its bytes never change. A
   Mutex and a channel count as well, because each is made to be shared
   between threads. */
bool type_pointer_free(const struct type *t);

/* Whether t declares fields, which a struct, a union and a class do. */
bool type_has_fields(const struct type *t);

#endif

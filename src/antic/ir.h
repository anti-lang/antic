#ifndef ANTIC_IR_H
#define ANTIC_IR_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "arena.h"
#include "attributes.h"
#include "text.h"

/* The intermediate representation of antic: typed three-address code in
   functions of basic blocks. It is the same for all six targets. It holds
   no register, no calling convention and no symbol mangling, because the
   back end decides those per target.

   DESIGN: the IR records types, never sizes, offsets or register classes.
   A size, an offset or an array stride is a symbolic value that names the
   type. The back end folds it after it lays out the types for its target,
   so a library file holds the same bytes on every host. */

/* Scalar value types. bool is i8, char is i32, and every pointer and
   function pointer is ptr. Signedness lives in the operations. */
enum ir_type {
    IR_VOID,
    IR_I8,
    IR_I16,
    IR_I32,
    IR_I64,
    IR_F32,
    IR_F64,
    IR_PTR,
    IR_AGG,     /* a struct, union, array, str or slice passed by value */
    IR_CLONG,   /* c_long and c_ulong: 32 bits on Windows, else 64 */
    IR_CWCHAR,  /* c_wchar: 16 bits on Windows, else 32 */
    IR_LOCK     /* the word of a Mutex: 64 bits on Windows, else 32 */
};

#define IR_NO_AGG UINT32_MAX

/* The type of a value in memory: a scalar, or IR_AGG with the index of an
   aggregate in the module's type table. */
struct ir_vtype {
    enum ir_type type;
    uint32_t agg;
};

enum ir_agg_kind { IR_AGG_STRUCT, IR_AGG_UNION, IR_AGG_ARRAY };

/* How a parameter of 8 or 16 bits extends to 32 bits. The IR keeps
   signedness in operations, and a parameter has none, so the signature
   records it for the conventions whose callers extend. */
enum ir_ext { IR_EXT_NONE, IR_EXT_SIGN, IR_EXT_ZERO };

/* A field of a struct or union, or the element of an array. A bitfield
   has a width in bits and extends by its signedness when it is read. */
struct ir_field {
    const char *name;               /* NULL for an array element */
    struct ir_vtype type;
    uint8_t bits;                   /* 0 for a field that is not a bitfield */
    enum ir_ext ext;
};

/* An aggregate type. A struct or union lists its fields, and an array has
   one field for its element and a symbolic length. */
struct ir_aggtype {
    enum ir_agg_kind kind;
    const char *name;               /* main.Vec2, str, []i32 or [4]i32 */
    struct ir_field *fields;
    size_t field_count;
    bool packed;
    /* DESIGN: a simd struct is a struct of lanes of one scalar type. The
       back end aligns it to its size or to sixteen, whichever is less.
       It passes it in vector registers where its convention passes the
       vector type of C. */
    bool simd;
    uint64_t align;                 /* the N of align(N), or 0 */
    uint32_t length;                /* IR_AGG_ARRAY: a symbolic value */
    const char *length_text;        /* IR_AGG_ARRAY: the length as written */
};

enum ir_sym_kind {
    IR_SYM_INT,         /* a number */
    IR_SYM_SIZE_OF,     /* size_of T */
    IR_SYM_OFFSET_OF,   /* offset_of T.f */
    IR_SYM_OP           /* an operation on one or two symbolic values */
};

/* A symbolic value, an integer that depends on the target. */
struct ir_sym {
    enum ir_sym_kind kind;
    enum ir_type type;
    uint64_t value;                 /* IR_SYM_INT */
    struct ir_vtype of;             /* IR_SYM_SIZE_OF, IR_SYM_OFFSET_OF */
    uint32_t field;                 /* IR_SYM_OFFSET_OF */
    int op;                         /* IR_SYM_OP, an enum ir_op */
    uint32_t a;                     /* IR_SYM_OP */
    uint32_t b;                     /* IR_SYM_OP, IR_NO_AGG for one operand */
};

enum ir_op {
    /* result = op a, b: both operands and the result have the same type */
    IR_ADD, IR_SUB, IR_MUL, IR_SDIV, IR_UDIV, IR_SREM, IR_UREM,
    IR_AND, IR_OR, IR_XOR, IR_SHL, IR_SHR_S, IR_SHR_U,
    IR_FADD, IR_FSUB, IR_FMUL, IR_FDIV,
    /* result = op a */
    IR_NEG, IR_NOT, IR_FNEG, IR_COPY,
    /* result i8 = op a, b: 1 when the comparison holds, else 0 */
    IR_EQ, IR_NE, IR_SLT, IR_SLE, IR_SGT, IR_SGE,
    IR_ULT, IR_ULE, IR_UGT, IR_UGE,
    IR_FEQ, IR_FNE, IR_FLT, IR_FLE, IR_FGT, IR_FGE,
    /* result = op a, b: the signed operation, which also records whether
       it left the range of the type. IR_BRANCH_OV reads that. The back
       end emits one instruction where it sets the overflow flag, and a
       multiply into twice the width where none does. */
    IR_ADD_OV, IR_SUB_OV, IR_MUL_OV,
    /* result = op a, b: the upper half of the full product, signed or
       unsigned. */
    IR_MULH_S, IR_MULH_U,
    /* result = op a, b: the operation clamped at the minimum or the
       maximum of the type, signed or unsigned. The back end expands each
       into plain operations once it knows the width. */
    IR_ADD_SAT_S, IR_ADD_SAT_U, IR_SUB_SAT_S, IR_SUB_SAT_U, IR_MUL_SAT_S,
    IR_MUL_SAT_U,
    /* DESIGN: a flag operation gives the result of its arithmetic and
       leaves the four flags of it, which IR_FLAG reads. The reads follow
       the operation with nothing between, which the verifier checks, so a
       target whose instruction sets a flag reads it there. c of + and -
       is a carry or a borrow of type i8, or none. */
    IR_ADD_FL, IR_SUB_FL, IR_MUL_FL, IR_SHL_FL, IR_SHR_S_FL, IR_SHR_U_FL,
    IR_NEG_FL,
    /* result i8 = the flag field of the flag operation that gave a */
    IR_FLAG,
    /* result = op a, with the result type named by the instruction */
    IR_TRUNC, IR_SEXT, IR_ZEXT, IR_SITOF, IR_UITOF, IR_FTOSI, IR_FTOUI,
    IR_FEXT, IR_FTRUNC,
    /* DESIGN: an f16 is its sixteen bits in an i16, since nothing but
       these two operations reads it as a number. hext widens the bits in a
       to an f32 and htrunc rounds the f32 in a to the bits, to nearest
       with ties to even. */
    IR_HEXT, IR_HTRUNC,
    /* Memory. */
    IR_SLOT,        /* Result ptr: a stack slot for a value of type of. */
    IR_LOAD,        /* Result: load type from a. */
    IR_STORE,       /* Store a of type into b. */
    IR_PTRADD,      /* Result ptr: a plus b bytes. */
    IR_MEMCOPY,     /* Copy a value of type of from b to a. */
    IR_ADDR,        /* Result ptr: address of a. */
    IR_BITLOAD,     /* Result: load bitfield field of aggregate of from a. */
    IR_BITSTORE,    /* Store a into bitfield field of aggregate of at b. */
    /* DESIGN: an operation on simd structs works on whole values in
       memory, as memcopy does, so the IR holds no vector type and no
       width. of is the simd aggregate of the operands, type the type of
       one lane and field the operation of one lane, an ir_op. The back
       end maps each to the native width of its target and level. It
       expands one it has no instruction for into an operation per
       lane. */
    IR_VBINARY,     /* The lane operation field of b and c into a. A
                       comparison writes a mask, one i8 per lane. */
    IR_VUNARY,      /* The lane operation field of b into a. */
    IR_VSPLAT,      /* The scalar b into every lane of a. */
    IR_VSELECT,     /* Into a the lane of c where the mask b holds, and
                       the lane of args[0] where it does not. */
    IR_VSHUFFLE,    /* Into lane i of a the lane args[i] of b. */
    IR_VREDUCE,     /* Result: the lanes at a folded by field. IR_ADD or
                       IR_FADD sums, a less-than keeps the least, a
                       greater-than the greatest, and IR_OR and IR_AND
                       fold a mask. */
    /* Calls. */
    IR_CALL,        /* Result: call a with args, as signature b if set.
                       A call through a table names in c the descriptor
                       of the class whose table it reads and in field
                       the slot. */
    /* Terminators, the last instruction of a block. */
    IR_JUMP,        /* Go to block a. */
    IR_BRANCH,      /* Go to b when a is nonzero, else c. */
    IR_BRANCH_OV,   /* Go to b when the operation that gave a left the
                       range of its type, else c. a is the result of an
                       IR_ADD_OV, IR_SUB_OV or IR_MUL_OV, which is the
                       instruction right before this one. */
    IR_RET          /* Return a, or nothing. */
};

/* The flags IR_FLAG reads, in the order of the fields of Flags. */
enum ir_flag {
    IR_FLAG_OVERFLOW,   /* the signed result does not fit */
    IR_FLAG_CARRY,      /* the carry of +, the borrow of -, the bit a shift
                           moved out last, the unsigned product too wide */
    IR_FLAG_ZERO,
    IR_FLAG_NEGATIVE    /* the top bit of the result */
};

enum ir_operand_kind {
    IR_NONE,
    IR_TEMP,        /* A temporary, %n. */
    IR_INT,         /* An integer constant. */
    IR_FLOAT,       /* A float constant. */
    IR_GLOBAL,      /* a global data item, by index */
    IR_FUNC,        /* a function, by index */
    IR_BLOCK,       /* a basic block, by index */
    IR_SYM          /* a symbolic value, by index */
};

struct ir_operand {
    enum ir_operand_kind kind;
    enum ir_type type;
    union {
        uint32_t temp;
        uint64_t integer;
        double floating;
        uint32_t index;
    } as;
};

#define IR_NO_RESULT UINT32_MAX

/* A global or a function that a class record does not have. */
#define IR_NO_INDEX UINT32_MAX

struct ir_inst {
    enum ir_op op;
    enum ir_type type;              /* the result type, or the stored type */
    uint32_t line;                  /* the source line, or 0 for none */
    uint32_t result;                /* a temporary, or IR_NO_RESULT */
    struct ir_operand a;
    struct ir_operand b;
    struct ir_operand c;
    struct ir_vtype of;             /* IR_SLOT, IR_MEMCOPY, the bitfield ops */
    uint32_t field;                 /* IR_BITLOAD, IR_BITSTORE, IR_FLAG */
    struct ir_operand *args;        /* IR_CALL */
    size_t arg_count;
};

/* DESIGN: the failure arm of an assertion and the failure arm of a
   dev-mode check carry which one they are. The build that compiles the
   program drops each under its own switch. A library file holds both,
   and the build that links decides. */
enum ir_fail {
    IR_FAIL_NONE,
    IR_FAIL_ASSERT,     /* the failure arm of an assertion */
    IR_FAIL_CHECK       /* the failure arm of a dev-mode check */
};

struct ir_block {
    uint32_t index;
    struct ir_inst *insts;
    size_t count;
    size_t capacity;
    enum ir_fail fail;
    uint32_t loop_depth;        /* the loops this block sits inside */
};

/* A parameter. An aggregate parameter's temporary holds a pointer to the
   value, and the back end copies it in as its ABI requires. */
struct ir_param {
    enum ir_type type;
    enum ir_ext ext;
    uint32_t agg;                   /* IR_AGG: the aggregate, else IR_NO_AGG */
    uint32_t temp;
};

/* DESIGN: a source position is one line and one file per function. The
   file is an index into the module's table, so a function names its
   source in four bytes and the IR holds each path once. at_line is the
   cursor that lowering moves from statement to statement, and every
   instruction the appenders add takes it. A pass that adds an
   instruction of its own leaves it at 0, and no `.loc` names it. */
struct ir_function {
    uint32_t index;
    uint32_t file;                  /* the source, or IR_NO_INDEX */
    uint32_t decl_line;             /* the line of `fn`, or 0 */
    uint32_t at_line;               /* the line the appenders stamp */
    const char *module;             /* NULL for a C function */
    const char *name;
    struct ir_param *params;
    size_t param_count;
    size_t param_capacity;
    enum ir_type result;
    uint32_t result_agg;            /* IR_AGG: the aggregate, else IR_NO_AGG */
    bool is_extern;                 /* no body: C or another module */
    bool variadic;
    bool exported;                  /* an export fn, with a C symbol */
    bool worker;                    /* a worker fn */
    struct ir_block **blocks;
    size_t block_count;
    size_t block_capacity;
    enum ir_type *temps;            /* the type of each temporary */
    uint32_t temp_count;
    size_t temp_capacity;
};

/* An address the linker writes into a global. It names another global,
   or a function when fn is set, as a table entry does. */
struct ir_reloc {
    uint64_t offset;
    uint32_t global;
    bool fn;
};

enum ir_const_kind {
    IR_CONST_NONE,      /* no value, such as the break of a bitfield unit */
    IR_CONST_INT,       /* an integer, a bitfield among them */
    IR_CONST_FLOAT,
    IR_CONST_SYM,       /* a symbolic value, such as size_of T */
    IR_CONST_ADDR,      /* the address of another global. */
    IR_CONST_FUNC,      /* the address of a function, as a table entry */
    IR_CONST_AGG        /* a struct, a union or an array */
};

/* DESIGN: an aggregate constant reaches the back end as a typed tree and
   not as bytes. The IR holds no sizes. A field's offset, its padding and
   the bits of a bitfield belong to the back end, which lays the type out
   for its target. */
struct ir_const {
    enum ir_const_kind kind;
    struct ir_vtype type;           /* IR_CONST_AGG: the aggregate */
    enum ir_type scalar;            /* the value types */
    uint64_t integer;               /* IR_CONST_INT */
    double floating;                /* IR_CONST_FLOAT */
    uint32_t sym;                   /* IR_CONST_SYM */
    uint32_t global;                /* IR_CONST_ADDR, IR_CONST_FUNC */
    struct ir_const *items;         /* IR_CONST_AGG, one per field */
    size_t item_count;
};

/* Constant data, such as the bytes of a string literal. A global with a
   value holds an aggregate constant instead. The back end writes its
   bytes, its size and its alignment before anything reads them. */
struct ir_global {
    uint32_t index;
    const char *module;
    const char *name;
    uint8_t *bytes;
    uint64_t size;
    uint64_t align;
    struct ir_reloc *relocs;
    size_t reloc_count;
    struct ir_const *value;
    bool mutable;                   /* a static field, which the program
                                       writes. */
    bool exported;                  /* a C symbol that C code reads */
    /* DESIGN: a global the runtime defines, which every module refers to
       and none writes out. The root's descriptor is the one of them, so
       that two modules of one program share it. */
    bool is_extern;
};

/* DESIGN: a class record tells the passes over the whole program what
   the functions and globals of the IR do not say. It names the tables a
   class fills, its base, its interfaces and the `mutable` fields of a
   singleton. The module that declares a class writes its record. Every
   reference in it is a global, a function or an aggregate of the module,
   so a library file maps it like the rest. */
enum ir_class_flag {
    IR_CLASS_ABSTRACT = 1,
    IR_CLASS_FINAL = 2,
    IR_CLASS_SINGLETON = 4,
    IR_CLASS_ARGS = 8,              /* its construct takes arguments */
    IR_CLASS_REQUIRED = 16          /* an inline class field no default fills */
};

/* The table of one interface sub-object of a concrete class, the ones it
   inherits among them.

   DESIGN: the sub-object of an interface sits at a field of the class
   that declares it. The aggregate and the index give its offset. A pass
   over the whole program then converts a pointer to the class into a
   pointer to the interface, without the types. */
struct ir_subtable {
    uint32_t interface;             /* the global of the interface's
                                       descriptor */
    uint32_t table;                 /* the global of the table */
    uint32_t agg;                   /* the aggregate of the sub-object */
    uint32_t field;                 /* its field of that aggregate */
};

/* One `inject` field of a class: the path of its interface, the name of
   the field and whether `inject final` keeps the run-time configuration
   from replacing the provider. */
struct ir_inject {
    const char *interface;
    const char *field;
    uint32_t descriptor;            /* the interface's descriptor */
    bool final;
};

/* One `provides Interface as Class;` line of the module that declares
   the class: the path of the interface and the global of its
   descriptor. The factory is written by the pass over the whole
   program, so the record carries what that pass needs and no more. */
struct ir_provides {
    const char *interface;
    uint32_t descriptor;            /* the interface's descriptor */
};

/* A class record. The descriptor, the base's descriptor and the tables
   are globals. The table of an abstract class is IR_NO_INDEX. The init
   function sets the tables of an object, writes the defaults and runs
   construct. */
struct ir_class {
    const char *module;
    const char *name;
    unsigned flags;                 /* enum ir_class_flag */
    uint32_t descriptor;
    uint32_t base;
    uint32_t table;
    uint32_t init;                  /* a function, or IR_NO_INDEX */
    uint32_t agg;                   /* its aggregate */
    struct ir_subtable *subtables;
    size_t subtable_count;
    uint32_t *mutable_fields;       /* fields of agg */
    size_t mutable_count;
    struct ir_inject *injects;      /* the `inject` fields it declares */
    size_t inject_count;
    struct ir_provides *provides;   /* the interfaces it is provided for */
    size_t provides_count;
};

struct ir_module {
    struct arena *arena;
    const char *name;
    /* The source files of the module, each once. A path is the one the
       search root gives, so a library file holds the same bytes on every
       host. `-g` writes a `.file` directive per entry. */
    const char **files;
    size_t file_count;
    size_t file_capacity;
    struct ir_aggtype **aggs;
    size_t agg_count;
    size_t agg_capacity;
    struct ir_sym *syms;
    size_t sym_count;
    size_t sym_capacity;
    struct ir_function **functions;
    size_t function_count;
    size_t function_capacity;
    struct ir_global **globals;
    size_t global_count;
    size_t global_capacity;
    struct ir_class **classes;
    size_t class_count;
    size_t class_capacity;
    /* DESIGN: the object of a plugin, whose functions and data of other
       modules the host defines. On ELF the code reaches each of them
       through the GOT, and on COFF through the __imp_ entry the import
       library of the host gives. */
    bool plugin;
};

/* Checked heap memory for the back end. The compiler cannot continue
   after an allocation fails. These functions then write "antic: out of
   memory" and end the run with status 70, and never return NULL. A size
   whose product overflows size_t fails the same way. */
_Noreturn void ir_out_of_memory(void);

/* Return a * b, and end the run as an allocation failure does when the
   product overflows size_t. */
size_t ir_product(size_t a, size_t b);

/* Return zeroed memory for count items of size bytes, or for one item when
   count is 0, so the result is never NULL. The caller frees it with
   free. */
void *ir_alloc(size_t count, size_t size);

/* Resize items to count items of size bytes, as realloc does. The caller
   frees the result with free. */
void *ir_resize(void *items, size_t count, size_t size);

/* Return items with room for at least count + 1 items. *capacity starts
   at 8 and doubles. The caller frees the result with free. */
void *ir_grow(void *items, size_t *capacity, size_t count, size_t size);

/* Format a message into buffer of size bytes, as vsnprintf does. A
   message cut to fit ends in three dots, so a reader sees that the rest
   is missing. */
void ir_vformat(char *buffer, size_t size, const char *format, va_list args)
    ATTRIBUTE_PRINTF(3, 0);
void ir_format(char *buffer, size_t size, const char *format, ...)
    ATTRIBUTE_PRINTF(3, 4);

void ir_module_init(struct ir_module *m, struct arena *arena,
                    const char *name);
void ir_module_free(struct ir_module *m);

/* Release the heap memory of a block, of the blocks and temporaries of a
   function, or of both and the parameters. The two for a function leave
   its counts and capacities at 0, so it takes new blocks and temporaries
   afterwards. The optimizer uses them for blocks and functions it
   removes. */
void ir_block_free(struct ir_block *b);
void ir_function_free_body(struct ir_function *f);
/* Release the lists of a class record and leave them empty. */
void ir_class_free(struct ir_class *c);
void ir_function_free(struct ir_function *f);

struct ir_vtype ir_scalar(enum ir_type type);
struct ir_vtype ir_aggregate(uint32_t agg);

/* Add a source file to the module's table, or find the one of that path.
   Returns its index. */
uint32_t ir_file_add(struct ir_module *m, const char *path);

/* Add a struct or union to the type table, or find the one of that name.
   Returns its index. */
uint32_t ir_struct_add(struct ir_module *m, enum ir_agg_kind kind,
                       const char *name, const struct ir_field *fields,
                       size_t count, bool packed, uint64_t align);
/* Add a simd struct of count lanes, or find the one of that name. */
uint32_t ir_simd_add(struct ir_module *m, const char *name,
                     const struct ir_field *fields, size_t count);
/* Add an array of a symbolic length, or find the one of that name. */
uint32_t ir_array_add(struct ir_module *m, const char *name,
                      struct ir_vtype element, uint32_t length,
                      const char *length_text);
/* The index of the aggregate named name, or IR_NO_AGG. */
uint32_t ir_agg_find(const struct ir_module *m, const char *name);

/* Add a symbolic value, or find an equal one. Returns its index. */
uint32_t ir_sym_int(struct ir_module *m, enum ir_type type, uint64_t value);
uint32_t ir_sym_size_of(struct ir_module *m, struct ir_vtype of);
uint32_t ir_sym_offset_of(struct ir_module *m, uint32_t agg, uint32_t field);
uint32_t ir_sym_op(struct ir_module *m, enum ir_op op, enum ir_type type,
                   uint32_t a, uint32_t b);
/* The operand of symbolic value sym: an integer constant for a number,
   else a symbolic operand. */
struct ir_operand ir_sym_operand(const struct ir_module *m, uint32_t sym);

/* DESIGN: the function of a module that compiles its pattern literals
   before main. The back end writes it as a constructor of the object, and
   the removal of unused code keeps it, since nothing calls it. The name
   holds a dot, so no function of a program has it. */
#define IR_PATTERNS_START "patterns.start"

/* Whether f is the IR_PATTERNS_START of its module, defined here. */
bool ir_is_patterns_start(const struct ir_function *f);

struct ir_function *ir_function_add(struct ir_module *m, const char *module,
                                    const char *name, enum ir_type result,
                                    uint32_t result_agg);
struct ir_function *ir_extern_add(struct ir_module *m, const char *name,
                                  enum ir_type result, bool variadic);
/* A function of another Anti module, which that module's IR defines. */
struct ir_function *ir_declare_add(struct ir_module *m, const char *module,
                                   const char *name, enum ir_type result,
                                   uint32_t result_agg);
uint32_t ir_param_add(struct ir_function *f, enum ir_type type, uint32_t agg);
struct ir_block *ir_block_add(struct ir_function *f);
uint32_t ir_temp(struct ir_function *f, enum ir_type type);

struct ir_global *ir_global_add(struct ir_module *m, const char *module,
                                const char *name, const uint8_t *bytes,
                                uint64_t size, uint64_t align);
void ir_global_reloc(struct ir_module *m, struct ir_global *g,
                     uint64_t offset, uint32_t target);
void ir_global_reloc_fn(struct ir_module *m, struct ir_global *g,
                        uint64_t offset, uint32_t function);

/* A global that holds the aggregate constant value. Its bytes stay empty
   until the back end lays the type out. */
struct ir_global *ir_global_add_value(struct ir_module *m, const char *module,
                                      const char *name,
                                      struct ir_const *value);

/* Add the record of a class with no tables, no base and no fields. */
struct ir_class *ir_class_add(struct ir_module *m, const char *module,
                              const char *name);
void ir_class_subtable(struct ir_class *c, uint32_t interface,
                       uint32_t table, uint32_t agg, uint32_t field);
void ir_class_mutable(struct ir_class *c, uint32_t field);
void ir_class_provides(struct ir_module *m, struct ir_class *c,
                       const char *interface, uint32_t descriptor);
void ir_class_inject(struct ir_module *m, struct ir_class *c,
                     const char *interface, const char *field,
                     uint32_t descriptor, bool final);

/* Room for an aggregate constant of count items, in the memory pool. */
struct ir_const *ir_const_agg(struct ir_module *m, struct ir_vtype type,
                              size_t count);

/* Whether the two constants hold the same value. */
bool ir_const_equal(const struct ir_const *a, const struct ir_const *b);

struct ir_operand ir_temp_op(const struct ir_function *f, uint32_t temp);
struct ir_operand ir_int_op(enum ir_type type, uint64_t value);
struct ir_operand ir_float_op(enum ir_type type, double value);
struct ir_operand ir_func_op(const struct ir_function *f);
struct ir_operand ir_global_op(const struct ir_global *g);

/* Append a copy of inst, with a copy of its argument list. The library
   file reader uses it for instructions whose operands it has checked. */
void ir_inst_add(struct ir_block *b, const struct ir_inst *inst);

/* Append an instruction. The emitters with a result create a new
   temporary and return it. ir_assign writes to an existing temporary,
   because the IR is not in static single assignment form. */
uint32_t ir_binary(struct ir_function *f, struct ir_block *b, enum ir_op op,
                   enum ir_type type, struct ir_operand x,
                   struct ir_operand y);
uint32_t ir_unary(struct ir_function *f, struct ir_block *b, enum ir_op op,
                  enum ir_type type, struct ir_operand x);
/* A flag operation of x and y, with the carry or borrow c of an add or a
   subtract, or none. */
uint32_t ir_flag_op(struct ir_function *f, struct ir_block *b, enum ir_op op,
                    enum ir_type type, struct ir_operand x,
                    struct ir_operand y, struct ir_operand c);
/* Read flag of the flag operation whose result is of, right after it. */
uint32_t ir_flag(struct ir_function *f, struct ir_block *b, enum ir_flag flag,
                 struct ir_operand of);
void ir_assign(struct ir_function *f, struct ir_block *b, uint32_t dst,
               struct ir_operand src);
uint32_t ir_slot(struct ir_function *f, struct ir_block *b,
                 struct ir_vtype of);
/* Add a slot to the entry block, after the slots already there. Lowering
   uses it for a value that it needs in the middle of a function. */
uint32_t ir_entry_slot(struct ir_function *f, struct ir_vtype of);
uint32_t ir_load(struct ir_function *f, struct ir_block *b, enum ir_type type,
                 struct ir_operand pointer);
void ir_store(struct ir_function *f, struct ir_block *b, enum ir_type type,
              struct ir_operand value, struct ir_operand pointer);
uint32_t ir_ptradd(struct ir_function *f, struct ir_block *b,
                   struct ir_operand pointer, struct ir_operand offset);
void ir_memcopy(struct ir_function *f, struct ir_block *b,
                struct ir_operand dst, struct ir_operand src,
                struct ir_vtype of);
uint32_t ir_addr(struct ir_function *f, struct ir_block *b,
                 struct ir_operand target);
/* Read or write bitfield field of aggregate agg in the aggregate at
   pointer. The back end lowers both to shifts and masks. */
uint32_t ir_bitload(struct ir_function *f, struct ir_block *b,
                    enum ir_type type, struct ir_operand pointer, uint32_t agg,
                    uint32_t field);
void ir_bitstore(struct ir_function *f, struct ir_block *b, enum ir_type type,
                 struct ir_operand value, struct ir_operand pointer,
                 uint32_t agg, uint32_t field);
/* The operations on simd structs. lane is the type of one lane of agg,
   and dst, x, y and mask are addresses of values. */
void ir_vbinary(struct ir_function *f, struct ir_block *b, enum ir_op op,
                enum ir_type lane, struct ir_operand dst, struct ir_operand x,
                struct ir_operand y, uint32_t agg);
void ir_vunary(struct ir_function *f, struct ir_block *b, enum ir_op op,
               enum ir_type lane, struct ir_operand dst, struct ir_operand x,
               uint32_t agg);
void ir_vsplat(struct ir_function *f, struct ir_block *b, enum ir_type lane,
               struct ir_operand dst, struct ir_operand value, uint32_t agg);
void ir_vselect(struct ir_function *f, struct ir_block *b, enum ir_type lane,
                struct ir_operand dst, struct ir_operand mask,
                struct ir_operand x, struct ir_operand y, uint32_t agg);
void ir_vshuffle(struct ir_function *f, struct ir_block *b, enum ir_type lane,
                 struct ir_operand dst, struct ir_operand x,
                 const uint32_t *lanes, size_t count, uint32_t agg);
uint32_t ir_vreduce(struct ir_function *f, struct ir_block *b, enum ir_op op,
                    enum ir_type type, struct ir_operand x, uint32_t agg);
uint32_t ir_call(struct ir_function *f, struct ir_block *b, enum ir_type type,
                 struct ir_operand callee, const struct ir_operand *args,
                 size_t arg_count);
/* A call of the address in target. Operand b names the declared function
   whose parameters and result the call follows. */
uint32_t ir_call_indirect(struct ir_function *f, struct ir_block *b,
                          enum ir_type type, struct ir_operand target,
                          const struct ir_function *signature,
                          const struct ir_operand *args, size_t arg_count);
void ir_jump(struct ir_function *f, struct ir_block *b,
             const struct ir_block *target);
void ir_branch_ov(struct ir_function *f, struct ir_block *b,
                  struct ir_operand value, const struct ir_block *then_block,
                  const struct ir_block *else_block);

void ir_branch(struct ir_function *f, struct ir_block *b,
               struct ir_operand cond, const struct ir_block *then_block,
               const struct ir_block *else_block);
void ir_ret(struct ir_function *f, struct ir_block *b, enum ir_type type,
            struct ir_operand value);

/* The names of types and operations in the text form. */
const char *ir_type_name(enum ir_type type);
const char *ir_op_name(enum ir_op op);

/* Append the text form of m to out. */
void ir_print(struct text *out, const struct ir_module *m);

/* Check the structural and type rules of the IR. Every violation goes to
   errors, one per line. Returns true when there is none. */
bool ir_verify(const struct ir_module *m, struct text *errors);

#endif

#ifndef ANTIC_ANTL_IO_H
#define ANTIC_ANTL_IO_H

/* The inside of the library file, which antl.c, antl_tree.c and
   antl_verify.c share. antl.c writes and reads the header, the type
   table, the items and the IR. antl_tree.c writes and reads the section
   of the generics: their declarations and the checked tree of each body.
   antl_verify.c holds what the reader built to the rules a file must
   meet. No other file includes this one. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "antl.h"
#include "attributes.h"

struct writer {
    struct text *out;
    const struct interface *iface;
    bool strip_docs;
    const struct type **types;      /* the type table in index order */
    size_t type_count;
    size_t type_capacity;
    /* A count or an index did not fit in 32 bits. */
    bool failed;
    /* The symbols of other items that the bodies of the generics name,
       written once each in the section of the generics. */
    const struct symbol **externs;
    size_t extern_count;
    size_t extern_capacity;
    /* The trees of the bodies of the generics, collected before the type
       table is written, which antl_tree.c owns. */
    void *trees;
};

struct reader {
    const uint8_t *data;
    size_t size;
    size_t pos;
    bool failed;
    char *error;
    size_t error_size;
    struct arena *arena;
    struct types *types;
    const struct interface *const *libraries;
    size_t library_count;
    struct interface *iface;
    struct type **table;
    uint32_t table_count;
    /* Per item of the interface: bit 5 of its marks, a generic function
       whose declaration the section of the generics holds. */
    bool *marked_generic;
};

/* The type reference that stands for no type in the section of the
   generics. The type table itself never names one. */
#define ANTL_NO_TYPE UINT32_MAX

/* antl_io.c: the primitives of the format. */

/* Write v in one byte, in four and in eight, little-endian. */
void antl_put_u8(struct writer *w, uint8_t v);
void antl_put_u32(struct writer *w, uint32_t v);
void antl_put_u64(struct writer *w, uint64_t v);
/* Write a count or an index, which the file holds in 32 bits. A larger
   one marks the writer failed, and the file is refused. */
void antl_put_count(struct writer *w, size_t n);
/* Write length bytes of s after their count. */
void antl_put_bytes(struct writer *w, const char *s, size_t length);

/* Refuse the file with the message format gives, unless it is refused
   already. The first message stands. */
void antl_fail(struct reader *r, const char *format, ...)
    ATTRIBUTE_PRINTF(2, 3);
/* Refuse the file as damaged at the byte the reader stands at. */
void antl_damaged(struct reader *r);
/* Whether n more bytes remain. The file is damaged when they do not. */
bool antl_take(struct reader *r, size_t n);
/* Read one byte, four and eight, little-endian. Past the end of the
   file each gives 0 and refuses the file. */
uint8_t antl_get_u8(struct reader *r);
uint32_t antl_get_u32(struct reader *r);
uint64_t antl_get_u64(struct reader *r);
/* Read a signed number of four and eight bytes, two's complement. */
int32_t antl_get_i32(struct reader *r);
int64_t antl_get_i64(struct reader *r);
/* Read a number the compiler keeps in an int and never makes negative,
   such as a line, and refuse one above INT_MAX. */
int antl_get_int(struct reader *r);
/* Read a count of records that each take at least min bytes, and refuse
   one the rest of the file cannot hold. This keeps a damaged count from
   causing a huge allocation. */
uint32_t antl_get_count(struct reader *r, size_t min);
/* count zeroed elements of size bytes from the memory pool of the
   reader, and one more, so a count of 0 gives memory as well. The pool
   frees it with everything else it holds. */
void *antl_allocate(struct reader *r, size_t count, size_t size);
/* Read a string as a name, at most INT_MAX bytes. Its text is memory of
   the pool of the reader, and the pool frees it. */
struct name antl_get_name(struct reader *r);

/* antl.c: the types, the values and the libraries, which the section
   of the generics names as the tables do. */

/* Give t and every type inside it an index in the type table. */
void antl_visit_type(struct writer *w, const struct type *t);
/* Give the types that v, the default values of the parameters of sym
   and the symbolic value s name their indices. */
void antl_visit_value(struct writer *w, const struct const_value *v);
void antl_visit_defaults(struct writer *w, const struct symbol *sym);
void antl_visit_symbolic(struct writer *w, const struct symbolic *s);
/* Write the index of t, which antl_visit_type gave it. The second form
   writes ANTL_NO_TYPE for a NULL t. */
void antl_put_type_ref(struct writer *w, const struct type *t);
void antl_put_type_or_none(struct writer *w, const struct type *t);
/* Write a constant, a symbolic value, and the default values and the
   `own` marks of the parameters of sym. */
void antl_put_value(struct writer *w, const struct const_value *v);
void antl_put_symbolic(struct writer *w, const struct symbolic *s);
void antl_put_param_defaults(struct writer *w, const struct symbol *sym);
void antl_put_param_owned(struct writer *w, const struct symbol *sym);

/* The type of the index the file holds next, which lies below limit. */
struct type *antl_type_ref(struct reader *r, uint32_t limit);
/* The same below the count of the type table, or NULL for ANTL_NO_TYPE. */
struct type *antl_type_or_none(struct reader *r);
/* Read a constant of type t into v. depth counts the values around it,
   0 for one that stands alone. Its parts are memory of the pool. */
bool antl_read_value(struct reader *r, struct type *t, struct const_value *v,
                     int depth);
/* Read a symbolic value whose types lie below limit, at depth as for
   antl_read_value. It is memory of the pool. */
const struct symbolic *antl_read_symbolic(struct reader *r, uint32_t limit,
                                          int depth);
/* Read the default values and the `own` marks of the parameters of sym,
   into memory of the pool. */
void antl_read_param_defaults(struct reader *r, struct symbol *sym);
void antl_read_param_owned(struct reader *r, struct symbol *sym);
/* The interface of module among the libraries the reader was given, or
   NULL. */
const struct interface *antl_library(const struct reader *r,
                                     const struct name *module);

/* antl_tree.c */

/* Give every type that the generics of the interface name an index, and
   collect the symbols of other items their bodies name. Runs before the
   type table is written. */
void antl_visit_generics(struct writer *w);
/* Write the section of the generics. */
void antl_put_generics(struct writer *w);
/* Read the section of the generics into the interface, and link each
   generic of its items to its declaration. */
void antl_read_generics(struct reader *r);

/* antl_verify.c */

/* The records of the tree of one body as the reader made them, each
   table one array in the order of the file. fn is the function the tree
   belongs to, which stands outside the tables. */
struct antl_tree {
    struct item *fn;
    struct symbol *syms;
    size_t sym_count;
    struct item *fns;               /* the anonymous functions */
    size_t fn_count;
    struct block *blocks;
    size_t block_count;
    struct stmt *stmts;
    size_t stmt_count;
    struct expr *exprs;
    size_t expr_count;
    struct type_expr *typexes;
    size_t typex_count;
};

/* Whether the tree t keeps the rules the checker keeps for a tree it
   checked. The copy pass and lowering rely on each of them. */
bool antl_verify_tree(struct reader *r, const struct antl_tree *t);
/* Whether sym, a symbol of another item that a tree names, is one the
   checker makes: its kind agrees with its type and its value. */
bool antl_verify_extern(const struct symbol *sym);
/* Whether t, a struct of the type table marked simd, has the shape the
   checker gives a `simd struct`. */
bool antl_verify_simd_type(const struct type *t);
/* Whether t, an aggregate of the IR marked simd, has the shape lowering
   gives a `simd struct`. */
bool antl_verify_simd_agg(const struct ir_aggtype *t);
/* Whether the constant c of the IR has a type the back end lays out as
   the value it holds. Its items are held one by one as they are read. */
bool antl_verify_const(const struct ir_const *c);

#endif

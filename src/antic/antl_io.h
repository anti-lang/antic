#ifndef ANTIC_ANTL_IO_H
#define ANTIC_ANTL_IO_H

/* The inside of the library file, which antl.c and antl_tree.c share.
   antl.c writes and reads the header, the type table, the items and the
   IR. antl_tree.c writes and reads the section of the generics: their
   declarations and the checked tree of each body. No other file includes
   this one. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "antl.h"

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

void antl_put_u8(struct writer *w, uint8_t v);
void antl_put_u32(struct writer *w, uint32_t v);
void antl_put_u64(struct writer *w, uint64_t v);
void antl_put_count(struct writer *w, size_t n);
void antl_put_bytes(struct writer *w, const char *s, size_t length);
void antl_put_type_ref(struct writer *w, const struct type *t);
void antl_visit_type(struct writer *w, const struct type *t);
void antl_visit_value(struct writer *w, const struct const_value *v);
void antl_visit_defaults(struct writer *w, const struct symbol *sym);
void antl_put_value(struct writer *w, const struct const_value *v);
void antl_visit_symbolic(struct writer *w, const struct symbolic *s);
void antl_put_symbolic(struct writer *w, const struct symbolic *s);
void antl_put_param_defaults(struct writer *w, const struct symbol *sym);
void antl_put_param_owned(struct writer *w, const struct symbol *sym);
uint64_t antl_float_bits(double d);

void antl_damaged(struct reader *r);
void antl_fail_needs(struct reader *r, const char *what, const char *module,
                     const struct name *name);
uint8_t antl_get_u8(struct reader *r);
uint32_t antl_get_u32(struct reader *r);
uint64_t antl_get_u64(struct reader *r);
uint32_t antl_get_count(struct reader *r, size_t min);
void *antl_allocate(struct reader *r, size_t count, size_t size);
struct name antl_get_name(struct reader *r);
struct type *antl_type_ref(struct reader *r, uint32_t limit);
bool antl_read_value(struct reader *r, struct type *t, struct const_value *v);
const struct symbolic *antl_read_symbolic(struct reader *r);
void antl_read_param_defaults(struct reader *r, struct symbol *sym);
void antl_read_param_owned(struct reader *r, struct symbol *sym);
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

#endif

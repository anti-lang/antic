/* The root class of the object model and the data behind reflection. The
   compiler builds a descriptor and a table for every class. It lays both
   out with the C rules, so these declarations mirror what it writes. */
#ifndef ANTI_OBJECT_H
#define ANTI_OBJECT_H

#include <stdint.h>

#include "std.h"

/* DESIGN: a field record names the type of its field by a type id and
   never by a width. One number then serves every target, and the runtime
   knows the size of each type on the host it runs on. The low byte names
   the type. A pointer, a slice, an array and an enum hold the id of the
   type they are built on in the byte above. The id of `[]i32` is
   ANTI_TYPE_SLICE | ANTI_TYPE_I32 << 8. src/lower.c writes these numbers,
   and the unit test records_type_ids pins the two together. */
enum anti_type {
    ANTI_TYPE_NONE,     /* a bitfield, which no walk reads */
    ANTI_TYPE_BOOL,
    ANTI_TYPE_CHAR,
    ANTI_TYPE_I8,
    ANTI_TYPE_I16,
    ANTI_TYPE_I32,
    ANTI_TYPE_I64,
    ANTI_TYPE_CLONG,
    ANTI_TYPE_U8,
    ANTI_TYPE_U16,
    ANTI_TYPE_U32,
    ANTI_TYPE_U64,
    ANTI_TYPE_CULONG,
    ANTI_TYPE_CWCHAR,
    ANTI_TYPE_F32,
    ANTI_TYPE_F64,
    ANTI_TYPE_STR,
    ANTI_TYPE_PTR,
    ANTI_TYPE_FN,
    ANTI_TYPE_SLICE,
    ANTI_TYPE_ARRAY,
    ANTI_TYPE_STRUCT,
    ANTI_TYPE_UNION,
    ANTI_TYPE_ENUM,
    ANTI_TYPE_CLASS
};

/* The type a type id names, and the type it is built on. An enum in the
   byte above is written as the integer it is built on. */
#define ANTI_TYPE_OF(id) ((int64_t)(id) & 0xFF)
#define ANTI_TYPE_ELEMENT(id) (((int64_t)(id) >> 8) & 0xFF)

/* The type id of a scalar type, with an enum replaced by its integer. */
int64_t anti_rt_type_scalar(int64_t type);

/* The bytes that a value of the type id takes on this host. A struct, a
   union, an array and a class give 0, since their id holds no size. */
size_t anti_rt_type_size(int64_t type);

/* Whether the type id is a signed integer, an enum over one among them. */
int anti_rt_type_signed(int64_t type);

/* The integer of the type id at bytes, widened to 64 bits by its sign. */
uint64_t anti_rt_load_integer(const void *bytes, int64_t type);

/* Store the low bytes of value as an integer of the type id. */
void anti_rt_store_integer(void *bytes, int64_t type, uint64_t value);

struct anti_descriptor;

/* The bytes of one element that a pointer or a slice of the type id
   reaches, or 0 when the id does not give them. d is the descriptor of
   a struct or a class element. */
size_t anti_rt_element_size(int64_t type, const struct anti_descriptor *d);

/* Whether serialize and deserialize walk the elements of a slice or the
   value behind a pointer of the type id. A class value and a slice have
   no walk there, nor does an element without a size. */
int anti_rt_element_walked(int64_t type, const struct anti_descriptor *d);

struct anti_descriptor;
struct anti_object;

/* One field a class or a struct declares. A field of class or struct
   type carries the descriptor of that type, and so does a pointer or a
   slice of one. A walk then reaches the whole object. */
struct anti_field {
    const unsigned char *name;
    int64_t name_length;
    int64_t offset;
    int64_t type;       /* a type id of enum anti_type */
    int64_t owned;
    const struct anti_descriptor *descriptor;
};

/* One public function of the chain of a class, with the entry of the
   table that holds it. A program reads the list and calls through the
   table, so reflection needs no name of its own. The signature names
   the result and the parameters after self, as `i64.i32.str`. It is
   NULL where a reflect.Value cannot carry one of them. */
struct anti_function {
    const unsigned char *name;
    int64_t name_length;
    int64_t slot;
    int64_t param_count;
    const unsigned char *signature;
};

/* The record at entry 0 of the table of a class. The destruct entry is the
   body the class declares, not the one it inherits, because delete runs
   one body per level of the chain. */
struct anti_descriptor {
    const unsigned char *name;
    int64_t name_length;
    const struct anti_descriptor *parent;
    int64_t size;
    int64_t depth;
    const struct anti_descriptor *const *ancestors;
    int64_t field_count;
    const struct anti_field *fields;
    void (*destruct)(struct anti_object *self);
    /* How far the sub-object of an interface sits from the start of the
       object. The descriptor of a class has zero here, and the one an
       interface table points at has the offset of its sub-object. */
    int64_t offset;
    int64_t function_count;
    const struct anti_function *functions;
};

/* DESIGN: the seven functions of the root take the entries after the
   descriptor, in this order, in the table of every class. The order is
   root_names in src/lower.c, and a unit test pins the two together. */
enum anti_entry {
    ANTI_ENTRY_DESCRIPTOR,
    ANTI_ENTRY_TYPE_NAME,
    ANTI_ENTRY_TO_TEXT,
    ANTI_ENTRY_EQUALS,
    ANTI_ENTRY_HASH,
    ANTI_ENTRY_SERIALIZE,
    ANTI_ENTRY_DROP,
    ANTI_ENTRY_COPY
};

/* Every object starts with the address of its table, and entry 0 of a
   table is the descriptor of its class. */
struct anti_object {
    const struct anti_descriptor *const *table;
};

/* The descriptor of an object, or NULL when its table is not set. */
const struct anti_descriptor *anti_rt_descriptor(const void *object);

/* DESIGN: the three take the descriptor of the class the program holds
   the object as. It names that class when the table of the object is
   zero. A null object is left alone. */

/* A copy of the object on the heap, of its concrete size, made by the
   copy entry of its table. */
void *anti_rt_dup(void *object, const struct anti_descriptor *type);

/* Run the destruct body of each class of the chain, destroy every object
   the chain owns, free every buffer it owns, and free the object. */
void anti_rt_delete(void *object, const struct anti_descriptor *type);

/* The same without the final free, for an object that is not on the
   heap of its own. */
void anti_rt_destroy(void *object, const struct anti_descriptor *type);

/* The descriptor of the root and its one ancestor, which every module
   of a program shares. */
extern const struct anti_descriptor anti_rt_Object_descriptor;
extern const struct anti_descriptor *const anti_rt_Object_ancestors[1];

/* The start of the object that object points into. A pointer to an
   interface sub-object moves back by the offset its descriptor holds. */
void *anti_rt_object_of(void *object);

/* The seven functions of anti.rt.Object. A class replaces any of them
   with a concrete function of the same name. */
struct anti_text anti_rt_Object_type_name(struct anti_object *self);
struct anti_text anti_rt_Object_to_text(struct anti_object *self);
int8_t anti_rt_Object_equals(struct anti_object *self,
                             struct anti_object *other);
uint64_t anti_rt_Object_hash(struct anti_object *self);
void anti_rt_Object_serialize(struct anti_object *self, void *out);
void anti_rt_Object_destruct(struct anti_object *self);
void anti_rt_Object_copy(struct anti_object *self, struct anti_object *to);

#endif

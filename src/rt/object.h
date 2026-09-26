/* The root class of the object model and the data behind reflection. The
   compiler builds a descriptor and a table for every class. It lays both
   out with the C rules, so these declarations mirror what it writes. */
#ifndef ANTI_OBJECT_H
#define ANTI_OBJECT_H

#include <stddef.h>
#include <stdint.h>

#include "std.h"

/* DESIGN: a field record names the type of its field by a type id and
   never by a width. One number then serves every target, and the runtime
   knows the size of each type on the host it runs on. The low byte names
   the type. A pointer, a slice, an array and an enum hold the id of the
   type they are built on in the byte above. The id of `[]i32` is
   ANTI_TYPE_SLICE | ANTI_TYPE_I32 << 8. An array holds two more. The id
   of its element through every level of it stands in the third byte. The
   count of those elements stands from the fourth byte up, and is 0 where
   a length is symbolic. src/antic/lower_desc.c writes these numbers, and the unit
   test records_type_ids pins the two together. */
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
    ANTI_TYPE_CLASS,
    ANTI_TYPE_F16,      /* after the others, so their ids stay. */
    ANTI_TYPE_VARIANT,  /* carries its descriptor: the tag, then the cases. */
    ANTI_TYPE_OPTIONAL, /* a `?T` of a value: the value, then the flag. */
    ANTI_TYPE_HANDLE,   /* compared by identity, of the words above. */
    ANTI_TYPE_REGEX,    /* the handle of a compiled pattern. */
    ANTI_TYPE_TUPLE     /* carries its descriptor, one record per part. */
};

/* The type a type id names, and the type it is built on. An enum in the
   byte above is written as the integer it is built on. */
#define ANTI_TYPE_OF(id) ((int64_t)(id) & 0xFF)
#define ANTI_TYPE_ELEMENT(id) (((int64_t)(id) >> 8) & 0xFF)
/* The element of an array through every level of it, and their count. */
#define ANTI_TYPE_INNER(id) (((int64_t)(id) >> 16) & 0xFF)
#define ANTI_TYPE_COUNT(id) ((int64_t)((uint64_t)(id) >> 24))

/* The type id of a scalar type, with an enum replaced by its integer. */
int64_t anti_rt_type_scalar(int64_t type);

/* The bytes that a value of the type id takes on this host. A struct, a
   union, an array and a class give 0, since their id holds no size. */
size_t anti_rt_type_size(int64_t type);

struct anti_descriptor;

/* The bytes of one element of the array of type id type through every
   level of it, or 0 where the record does not give them. d is the
   descriptor the record of the array carries. */
size_t anti_rt_array_element_size(int64_t type,
                                  const struct anti_descriptor *d);

/* The descriptor of the element of the array of type id type, through
   every level of it. An array of arrays carries a descriptor of its own
   in d, and its last record carries the one of the element. An array of
   one level carries the one of the element itself. */
const struct anti_descriptor *
anti_rt_array_element_descriptor(int64_t type, const struct anti_descriptor *d);

/* The most levels of an array a walk writes as nested JSON arrays. */
#define ANTI_ARRAY_LEVELS 16

/* Write the length of each level of the array of type id type into
   lengths, the outermost first, and give the count of levels. It gives 0
   where the record does not give them or there are more than
   ANTI_ARRAY_LEVELS. */
int64_t anti_rt_array_levels(int64_t type, const struct anti_descriptor *d,
                             int64_t lengths[ANTI_ARRAY_LEVELS]);

/* Whether the type id is a signed integer, an enum over one among them. */
int anti_rt_type_signed(int64_t type);

/* The integer of the type id at bytes, widened to 64 bits by its sign. */
uint64_t anti_rt_load_integer(const void *bytes, int64_t type);

/* Store the low bytes of value as an integer of the type id. */
void anti_rt_store_integer(void *bytes, int64_t type, uint64_t value);

struct anti_descriptor;

/* Whether the two compiled patterns a and b have the same text and the
   same mode. The hash of p takes its text and its mode, so it agrees. The
   default `==` and hash of a Regex call the two. */
int8_t anti_rt_pattern_same(const void *a, const void *b);
uint64_t anti_rt_pattern_hash(const void *p);

/* The record of the case that the tag of the variant at bytes names,
   among the records of its descriptor d, or NULL. */
const struct anti_field *anti_rt_variant_case(const unsigned char *bytes,
                                              const struct anti_descriptor *d);

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

/* DESIGN: the chain of an abstract class holds one hash per prefix of
   its table, the empty prefix first. Entry k is the hash of the version
   whose table held k entries. A plugin records the chain of the
   interface it was built against, and the loader compares the two at the
   length of the shorter. Equal there, the plugin's structure is the one
   this program carries. floor is the version a `compatible` line names,
   the lowest a plugin may have been built for. It is NULL without
   one. */
struct anti_versions {
    const int64_t *chain;
    int64_t chain_length;
    const unsigned char *floor;
    int64_t floor_length;
};

/* The record at entry 0 of the table of a class. The destruct entry is the
   body the class declares, not the one it inherits. The teardown that the
   compiler writes for a class calls the body of each level. */
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
    /* The version of the package that declared the class, which the
       module that declares it writes. */
    const unsigned char *version;
    int64_t version_length;
    /* The chain and the floor of an abstract class, NULL for every
       other. */
    const struct anti_versions *versions;
    /* The type arguments of a copy of a generic class, one record each,
       whose offset holds the size of the argument. A constant argument
       has type id none. None for every other class. */
    int64_t type_arg_count;
    const struct anti_field *type_args;
};

/* DESIGN: the seven functions of the root take the entries after the
   descriptor, in this order, in the table of every class. The nine hooks
   take the entries after them. The order is root_names in
   src/antic/lower_desc.c, and a unit test pins the two together. The
   destruct entry of a class holds the teardown the compiler writes for
   it. The copy entry holds its copy unless the chain declares one. */
enum anti_entry {
    ANTI_ENTRY_DESCRIPTOR,
    ANTI_ENTRY_TYPE_NAME,
    ANTI_ENTRY_TO_TEXT,
    ANTI_ENTRY_EQUALS,
    ANTI_ENTRY_HASH,
    ANTI_ENTRY_SERIALIZE,
    ANTI_ENTRY_DROP,
    ANTI_ENTRY_COPY,
    ANTI_ENTRY_HOOK     /* the first of the nine hooks */
};

/* DESIGN: anti.mem.Allocator declares alloc and free as its first two
   functions. They take the two entries after the seven of the root and
   the nine hooks. That holds for the table of every allocator and of the
   sub-object of an interface. The runtime calls them there, as a call through a
   `*Allocator` does. src/std/anti/mem.anti keeps the order. */
#define ANTI_ENTRY_ALLOC ((int)ANTI_ENTRY_HOOK + (int)ANTI_HOOK_COUNT)
#define ANTI_ENTRY_FREE (ANTI_ENTRY_ALLOC + 1)

/* DESIGN: the nine hooks of anti.lang.Object, in the order of
   root_names. Three are lifecycle and two are threads, which every
   build compiles. Three are the calls of an instrumented class, which
   tracing compiles. One is a write, which `--trace writes` compiles. */
enum anti_hook {
    ANTI_HOOK_CREATED,
    ANTI_HOOK_DESTROYED,
    ANTI_HOOK_COPIED,
    ANTI_HOOK_DISPATCHED,
    ANTI_HOOK_JOINED,
    ANTI_HOOK_ENTER,
    ANTI_HOOK_LEAVE,
    ANTI_HOOK_FAILED,
    ANTI_HOOK_CHANGED,
    ANTI_HOOK_COUNT
};

/* The entry of the hook h in the table of a class. The second gives its
   entry in the table of an anti.lang.TraceHandler, which declares the
   same nine again with the object after `self`. The two enumerations are
   added as int, since -Wconversion of the host build of a runtime refuses
   arithmetic between them. */
#define ANTI_ENTRY_OF_HOOK(h) ((int)ANTI_ENTRY_HOOK + (h))
#define ANTI_ENTRY_OF_HANDLER(h) \
    ((int)ANTI_ENTRY_HOOK + (int)ANTI_HOOK_COUNT + (h))

/* Every object starts with the address of its table, and entry 0 of a
   table is the descriptor of its class. */
struct anti_object {
    const struct anti_descriptor *const *table;
};

/* The descriptor of an object, or NULL when its table is not set. */
const struct anti_descriptor *anti_rt_descriptor(const void *object);

/* DESIGN: a table holds the descriptor and the functions of a class in
   one array of object pointers. ISO C does not define the conversion
   between an object pointer and a function pointer. Every target gives
   it, as POSIX dlsym needs. The two functions below are the one place
   the runtime makes it, so the extension stands in one file. */

/* A function of no signature. The caller casts it to the type the entry
   declares, which C defines between function pointer types. */
typedef void (*anti_rt_body)(void);

/* The function in entry `entry` of the table of object, or NULL when the
   object or its table is NULL. */
anti_rt_body anti_rt_entry_body(const void *object, int entry);

/* The function body as an entry of a table. */
const void *anti_rt_body_entry(anti_rt_body body);

/* DESIGN: the three take the descriptor of the class the program holds
   the object as. It names that class when the table of the object is
   zero. A null object is left alone. */

/* A copy of the object on the heap of the C library, of its concrete
   size, or NULL. The copy entry of its table makes it. The caller frees
   it with anti_rt_delete, which `delete` calls. */
void *anti_rt_dup(void *object, const struct anti_descriptor *type);

/* DESIGN: memory goes back to the allocator it came from. The teardown
   in the destruct entry takes the object and the anti.mem.Allocator of
   the memory it owns. NULL there is the C library, where `alloc` takes
   it, and the two forms without an allocator pass NULL. */

/* Give p back to the allocator from, or to the C library when from is
   NULL. */
void anti_rt_give(struct anti_object *from, void *p);

/* The allocator that takes nothing back. A teardown given it runs every
   destruct and leaves the memory where it is. */
extern struct anti_object anti_rt_give_nothing;

/* Run the destruct body of each class of the chain, destroy every object
   the chain owns, free every buffer it owns, and free the object. */
void anti_rt_delete(void *object, const struct anti_descriptor *type);

/* The same without the final free, for an object that is not on the
   heap of its own. */
void anti_rt_destroy(void *object, const struct anti_descriptor *type);

/* delete and destroy that give every piece of memory back to from. */
void anti_rt_delete_from(void *object, const struct anti_descriptor *type,
                         struct anti_object *from);
void anti_rt_destroy_from(void *object, const struct anti_descriptor *type,
                          struct anti_object *from);

/* The teardown of each of count class values of the type in a row, last
   to first, as an `own` slice of them holds. What they own goes back to
   from. */
void anti_rt_destroy_elements(void *elements, int64_t count,
                              const struct anti_descriptor *type,
                              struct anti_object *from);

/* The copy of each of count class values of the type from one row into
   another. */
void anti_rt_copy_elements(void *from, void *into, int64_t count,
                           const struct anti_descriptor *type);

/* DESIGN: a generic collection reaches the type of its elements through
   the record of its type argument, which the descriptor of its copy
   holds. The four functions below write, copy and tear down one element
   as a walk of the fields treats a field of that type. */

/* The record of the type argument index of the class at depth of the
   chain of object, or NULL when it has none. */
const struct anti_field *anti_rt_type_arg(const void *object, int64_t depth,
                                          int64_t index);

/* Append the element at bytes to the anti.text.Builder out as JSON, as
   `serialize` writes a field of its type. A class writes the
   `serialize` of its own table. */
void anti_rt_element_serialize(void *out, void *bytes,
                               const struct anti_field *arg);

/* Append the element at bytes to out as `to_text` of a collection writes
   it, a class through its own `to_text`. */
void anti_rt_element_text(void *out, void *bytes, const struct anti_field *arg);

/* A new buffer on the heap with the bytes at from, or NULL when there are
   none. The caller frees it with free. */
void *anti_rt_copy_buffer(const void *from, int64_t bytes);

/* A snapshot on the heap of size bytes, the size in its first eight.
   It aborts when the memory is not there. */
void *anti_rt_snapshot_new(int64_t size);

/* Copy length bytes of a captured str into the snapshot at offset at. */
void anti_rt_snapshot_text(void *snapshot, int64_t at,
                           const unsigned char *bytes, int64_t length);

/* Free a snapshot. NULL, the snapshot of a function that captures
   nothing, frees nothing. */
void anti_rt_snapshot_free(void *snapshot);

/* A copy of a snapshot, byte for byte, or NULL for NULL. */
void *anti_rt_snapshot_dup(const void *snapshot);

/* The count of the snapshots on the heap that are not freed. */
int64_t anti_rt_snapshots_alive(void);

/* The descriptor of the root and its one ancestor, which every module
   of a program shares. */
extern const struct anti_descriptor anti_lang_Object_descriptor;
extern const struct anti_descriptor *const anti_lang_Object_ancestors[1];

/* The start of the object that object points into. A pointer to an
   interface sub-object moves back by the offset its descriptor holds. */
void *anti_rt_object_of(void *object);

/* The seven functions of anti.lang.Object. A class replaces any of them
   with a concrete function of the same name. */
struct anti_text anti_lang_Object_type_name(struct anti_object *self);
struct anti_text anti_lang_Object_to_text(struct anti_object *self);
int8_t anti_lang_Object_equals(struct anti_object *self,
                               struct anti_object *other);
uint64_t anti_lang_Object_hash(struct anti_object *self);
void anti_lang_Object_serialize(struct anti_object *self, void *out);
void anti_lang_Object_destruct(struct anti_object *self);
void anti_lang_Object_copy(struct anti_object *self, struct anti_object *to);

/* DESIGN: the nine hooks of the root have empty bodies. A hook site
   dispatches the object's own hook, and one that reaches a body here
   does nothing. A class that replaces none pays a compare. */
void anti_lang_Object_created(struct anti_object *self);
void anti_lang_Object_destroyed(struct anti_object *self);
void anti_lang_Object_copied(struct anti_object *self,
                             struct anti_object *from);
void anti_lang_Object_dispatched(struct anti_object *self);
void anti_lang_Object_joined(struct anti_object *self);
void anti_lang_Object_enter(struct anti_object *self, struct anti_text name);
void anti_lang_Object_leave(struct anti_object *self, struct anti_text name);
void anti_lang_Object_failed(struct anti_object *self, struct anti_text name,
                             struct anti_object *e);
void anti_lang_Object_changed(struct anti_object *self,
                              const struct anti_field *field);

#endif

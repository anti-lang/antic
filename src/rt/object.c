#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "f16.h"
#include "object.h"
#include "regex.h"
#include "std.h"
#include "utf.h"

/* DESIGN: the default bodies of anti.lang.Object walk the field list of the
   descriptor. They are slow by design: a class that needs speed replaces
   the one it cares about with a concrete function. A build with
   --no-reflect has no field list, and these bodies then fall back to the
   class name and the address. */

/* DESIGN: the descriptor of the root belongs to the runtime. Every
   module of a program refers to this one, so `p is *Object` compares the
   same address wherever it is written, and no two modules define it. */
static const unsigned char object_name[] = "Object";

const struct anti_descriptor *const anti_lang_Object_ancestors[1] = {
    &anti_lang_Object_descriptor
};

const struct anti_descriptor anti_lang_Object_descriptor = {
    object_name, 6, NULL, (int64_t)sizeof(struct anti_object), 0,
    anti_lang_Object_ancestors, 0, NULL, NULL, 0, 0, NULL, NULL, 0, NULL, 0, NULL
};

const struct anti_descriptor *anti_rt_descriptor(const void *object)
{
    const struct anti_object *o = object;

    return o == NULL || o->table == NULL ? NULL : o->table[0];
}

/* DESIGN: a pointer to an interface sub-object points into the middle of
   an object. The descriptor of that sub-object's table records how far,
   so every operation that reads the whole object starts here. A class
   pointer has zero there and comes back unchanged. */
void *anti_rt_object_of(void *object)
{
    const struct anti_descriptor *d = anti_rt_descriptor(object);

    return d == NULL ? object : (char *)object - d->offset;
}

/* The start of the object, once its table is known to be set. type is
   the class the program holds it as, and the trap names it. */
static void *checked_object(void *object, const struct anti_descriptor *type)
{
    const struct anti_object *o = object;

    if (o != NULL && o->table == NULL) {
        if (type == NULL) {
            type = &anti_lang_Object_descriptor;
        }
        anti_rt_table_unset(type->name, type->name_length);
    }
    return anti_rt_object_of(object);
}

struct anti_text anti_lang_Object_type_name(struct anti_object *self)
{
    const struct anti_descriptor *d = anti_rt_descriptor(self);
    struct anti_text text;

    text.ptr = d == NULL ? (const unsigned char *)"" : d->name;
    text.len = d == NULL ? 0 : d->name_length;
    return text;
}

struct anti_text anti_lang_Object_to_text(struct anti_object *self)
{
    return anti_lang_Object_type_name(self);
}

int64_t anti_rt_type_scalar(int64_t type)
{
    return ANTI_TYPE_OF(type) == ANTI_TYPE_ENUM ? ANTI_TYPE_ELEMENT(type)
                                               : ANTI_TYPE_OF(type);
}

/* DESIGN: the size of each type is the size on the host the program runs
   on, which is the host its objects live on. The record holds none. */
size_t anti_rt_type_size(int64_t type)
{
    switch (anti_rt_type_scalar(type)) {
    case ANTI_TYPE_BOOL:
    case ANTI_TYPE_I8:
    case ANTI_TYPE_U8: return 1;
    case ANTI_TYPE_I16:
    case ANTI_TYPE_U16:
    case ANTI_TYPE_F16: return 2;
    case ANTI_TYPE_CHAR:
    case ANTI_TYPE_I32:
    case ANTI_TYPE_U32:
    case ANTI_TYPE_F32: return 4;
    case ANTI_TYPE_I64:
    case ANTI_TYPE_U64:
    case ANTI_TYPE_F64: return 8;
    case ANTI_TYPE_CLONG: return sizeof(long);
    case ANTI_TYPE_CULONG: return sizeof(unsigned long);
    case ANTI_TYPE_CWCHAR: return sizeof(wchar_t);
    case ANTI_TYPE_PTR:
    case ANTI_TYPE_FN: return sizeof(void *);
    case ANTI_TYPE_STR:
    case ANTI_TYPE_SLICE: return sizeof(struct anti_text);
    default: return 0;
    }
}

int anti_rt_type_signed(int64_t type)
{
    switch (anti_rt_type_scalar(type)) {
    case ANTI_TYPE_I8:
    case ANTI_TYPE_I16:
    case ANTI_TYPE_I32:
    case ANTI_TYPE_I64:
    case ANTI_TYPE_CLONG: return 1;
    default: return 0;
    }
}

uint64_t anti_rt_load_integer(const void *bytes, int64_t type)
{
    int sign = anti_rt_type_signed(type);

    switch (anti_rt_type_size(type)) {
    case 1: {
        uint8_t v;
        memcpy(&v, bytes, sizeof v);
        return sign ? (uint64_t)(int64_t)(int8_t)v : v;
    }
    case 2: {
        uint16_t v;
        memcpy(&v, bytes, sizeof v);
        return sign ? (uint64_t)(int64_t)(int16_t)v : v;
    }
    case 4: {
        uint32_t v;
        memcpy(&v, bytes, sizeof v);
        return sign ? (uint64_t)(int64_t)(int32_t)v : v;
    }
    default: {
        uint64_t v;
        memcpy(&v, bytes, sizeof v);
        return v;
    }
    }
}

void anti_rt_store_integer(void *bytes, int64_t type, uint64_t value)
{
    switch (anti_rt_type_size(type)) {
    case 1: {
        uint8_t v = (uint8_t)value;
        memcpy(bytes, &v, sizeof v);
        return;
    }
    case 2: {
        uint16_t v = (uint16_t)value;
        memcpy(bytes, &v, sizeof v);
        return;
    }
    case 4: {
        uint32_t v = (uint32_t)value;
        memcpy(bytes, &v, sizeof v);
        return;
    }
    default:
        memcpy(bytes, &value, sizeof value);
        return;
    }
}

/* DESIGN: equals and hash take each field by its kind, as the default
   `==` and hash of a struct do. The two then always agree, and a class
   and a struct with the same fields compare alike. A scalar, an enum, a
   pointer and a function compare by their bytes, so a pointer compares
   by its address. A str compares by its bytes. An own slice compares
   element by element, and a plain slice by its address and its length.
   A struct compares field by field under the same rule, in place, in an
   array or in an own slice. A class value compares through the equals
   of its own table. An array compares element by element through every
   level of it. A bitfield and a field of type id none are passed over,
   and the checker refuses the default `==` of a class with a union. */

static int same_value(const unsigned char *a, const unsigned char *b,
                      int64_t type, const struct anti_descriptor *d,
                      int64_t owned);
static uint64_t hash_value(uint64_t h, const unsigned char *p, int64_t type,
                           const struct anti_descriptor *d, int64_t owned);

/* FNV-1a of count bytes at p into h. */
static uint64_t hash_run(uint64_t h, const unsigned char *p, size_t count)
{
    size_t k;

    for (k = 0; k < count; k++) {
        h = (h ^ p[k]) * 1099511628211u;
    }
    return h;
}

int8_t anti_rt_pattern_same(const void *a, const void *b)
{
    const struct anti_pattern *x = a;
    const struct anti_pattern *y = b;

    if (x == y) {
        return 1;
    }
    if (x == NULL || y == NULL) {
        return 0;
    }
    return x->bytes == y->bytes && x->length == y->length &&
           (x->length == 0 ||
            memcmp(x->text, y->text, (size_t)x->length) == 0);
}

uint64_t anti_rt_pattern_hash(const void *p)
{
    const struct anti_pattern *x = p;
    uint64_t h = 1469598103934665603u;

    if (x == NULL) {
        return h;
    }
    h = hash_run(h, (const unsigned char *)&x->bytes, sizeof x->bytes);
    return x->length > 0 ? hash_run(h, x->text, (size_t)x->length) : h;
}

const struct anti_field *anti_rt_variant_case(const unsigned char *bytes,
                                              const struct anti_descriptor *d)
{
    uint64_t tag;
    int64_t i;

    if (d == NULL || d->field_count < 1) {
        return NULL;
    }
    tag = anti_rt_load_integer(bytes + d->fields[0].offset, d->fields[0].type);
    for (i = 1; i < d->field_count; i++) {
        if ((uint64_t)d->fields[i].owned == tag) {
            return &d->fields[i];
        }
    }
    return NULL;
}

/* Whether the `?T` of descriptor d at bytes holds a value. A descriptor
   without its list, under `--no-reflect`, gives 0. */
static int optional_has(const unsigned char *bytes,
                        const struct anti_descriptor *d)
{
    return d != NULL && d->field_count >= 2 && bytes[d->fields[1].offset] != 0;
}

/* Whether the fields that d itself declares are the same at a and b. */
static int same_fields(const unsigned char *a, const unsigned char *b,
                       const struct anti_descriptor *d)
{
    int64_t i;

    for (i = 0; i < d->field_count; i++) {
        const struct anti_field *f = &d->fields[i];
        if (!same_value(a + f->offset, b + f->offset, f->type, f->descriptor,
                        f->owned)) {
            return 0;
        }
    }
    return 1;
}

/* The fields that d itself declares at p, into h. */
static uint64_t hash_fields(uint64_t h, const unsigned char *p,
                            const struct anti_descriptor *d)
{
    int64_t i;

    for (i = 0; i < d->field_count; i++) {
        const struct anti_field *f = &d->fields[i];
        h = hash_value(h, p + f->offset, f->type, f->descriptor, f->owned);
    }
    return h;
}

const struct anti_descriptor *
anti_rt_array_element_descriptor(int64_t type, const struct anti_descriptor *d)
{
    if (ANTI_TYPE_ELEMENT(type) != ANTI_TYPE_ARRAY) {
        return d;
    }
    return d != NULL && d->field_count > 0
               ? d->fields[d->field_count - 1].descriptor
               : NULL;
}

int64_t anti_rt_array_levels(int64_t type, const struct anti_descriptor *d,
                             int64_t lengths[ANTI_ARRAY_LEVELS])
{
    int64_t i;

    if (ANTI_TYPE_ELEMENT(type) != ANTI_TYPE_ARRAY) {
        lengths[0] = ANTI_TYPE_COUNT(type);
        return lengths[0] > 0 ? 1 : 0;
    }
    if (d == NULL || d->field_count == 0 ||
        d->field_count > ANTI_ARRAY_LEVELS) {
        return 0;
    }
    for (i = 0; i < d->field_count; i++) {
        lengths[i] = d->fields[i].owned;
    }
    return d->field_count;
}

size_t anti_rt_array_element_size(int64_t type,
                                  const struct anti_descriptor *d)
{
    int64_t inner = ANTI_TYPE_INNER(type);

    d = anti_rt_array_element_descriptor(type, d);

    if (inner == ANTI_TYPE_STRUCT || inner == ANTI_TYPE_CLASS ||
        inner == ANTI_TYPE_VARIANT || inner == ANTI_TYPE_OPTIONAL ||
        inner == ANTI_TYPE_TUPLE) {
        return d != NULL ? (size_t)d->size : 0;
    }
    return anti_rt_type_size(inner);
}

/* Whether a class field of descriptor d is the sub-object of an
   interface, a view of the object that holds it. No value of an abstract
   class stands in place otherwise, and only an abstract class carries
   versions. Its table leads back to that object, so it is passed over. */
static int sub_object(const struct anti_descriptor *d)
{
    return d != NULL && d->versions != NULL;
}

/* Whether the element of type id element at a and b is the same, in an
   own slice, an array or in place. */
static int same_element(const unsigned char *a, const unsigned char *b,
                        int64_t element, const struct anti_descriptor *d)
{
    /* A tuple has the id of a struct and no descriptor, and is passed
       over. */
    if (element == ANTI_TYPE_STRUCT) {
        return d == NULL || same_fields(a, b, d);
    }
    if (element == ANTI_TYPE_CLASS && sub_object(d)) {
        return 1;
    }
    if (element == ANTI_TYPE_CLASS) {
        int8_t (*equals)(struct anti_object *, struct anti_object *) =
            (int8_t(*)(struct anti_object *, struct anti_object *))
                anti_rt_entry_body(a, ANTI_ENTRY_EQUALS);
        return equals != NULL &&
               equals((struct anti_object *)a, (struct anti_object *)b) != 0;
    }
    return same_value(a, b, element, NULL, 0);
}

static uint64_t hash_element(uint64_t h, const unsigned char *p,
                             int64_t element, const struct anti_descriptor *d)
{
    if (element == ANTI_TYPE_STRUCT) {
        return d != NULL ? hash_fields(h, p, d) : h;
    }
    if (element == ANTI_TYPE_CLASS && sub_object(d)) {
        return h;
    }
    if (element == ANTI_TYPE_CLASS) {
        uint64_t (*hash)(struct anti_object *) =
            (uint64_t(*)(struct anti_object *))anti_rt_entry_body(
                p, ANTI_ENTRY_HASH);
        uint64_t v = hash != NULL ? hash((struct anti_object *)p) : 0;
        return hash_run(h, (const unsigned char *)&v, sizeof v);
    }
    return hash_value(h, p, element, NULL, 0);
}

static int same_value(const unsigned char *a, const unsigned char *b,
                      int64_t type, const struct anti_descriptor *d,
                      int64_t owned)
{
    int64_t t = anti_rt_type_scalar(type);
    struct anti_text x;
    struct anti_text y;
    int64_t i;
    size_t size;

    switch (t) {
    case ANTI_TYPE_STRUCT:
    case ANTI_TYPE_CLASS:
        return same_element(a, b, t, d);
    /* A channel is its handle, and an own fn its code and its snapshot,
       each compared as a pointer compares. */
    case ANTI_TYPE_HANDLE:
        return memcmp(a, b, (size_t)ANTI_TYPE_ELEMENT(type) * sizeof(void *)) ==
               0;
    case ANTI_TYPE_REGEX: {
        const void *x;
        const void *y;
        memcpy(&x, a, sizeof x);
        memcpy(&y, b, sizeof y);
        return anti_rt_pattern_same(x, y);
    }
    case ANTI_TYPE_TUPLE:
        return d == NULL || same_fields(a, b, d);
    case ANTI_TYPE_VARIANT: {
        const struct anti_field *x = anti_rt_variant_case(a, d);
        if (x != anti_rt_variant_case(b, d)) {
            return 0;
        }
        return x == NULL || x->descriptor == NULL ||
               same_fields(a + x->offset, b + x->offset, x->descriptor);
    }
    case ANTI_TYPE_OPTIONAL:
        if (optional_has(a, d) != optional_has(b, d)) {
            return 0;
        }
        return !optional_has(a, d) ||
               same_value(a + d->fields[0].offset, b + d->fields[0].offset,
                          d->fields[0].type, d->fields[0].descriptor, 0);
    case ANTI_TYPE_ARRAY:
        size = anti_rt_array_element_size(type, d);
        for (i = 0; size > 0 && i < ANTI_TYPE_COUNT(type); i++) {
            if (!same_element(a + (size_t)i * size, b + (size_t)i * size,
                              ANTI_TYPE_INNER(type),
                              anti_rt_array_element_descriptor(type, d))) {
                return 0;
            }
        }
        return 1;
    case ANTI_TYPE_STR:
        memcpy(&x, a, sizeof x);
        memcpy(&y, b, sizeof y);
        return x.len == y.len &&
               (x.len == 0 || memcmp(x.ptr, y.ptr, (size_t)x.len) == 0);
    case ANTI_TYPE_SLICE:
        memcpy(&x, a, sizeof x);
        memcpy(&y, b, sizeof y);
        if (x.len != y.len) {
            return 0;
        }
        if (!owned || x.ptr == y.ptr) {
            return x.ptr == y.ptr;
        }
        size = anti_rt_element_size(type, d);
        for (i = 0; size > 0 && i < x.len; i++) {
            if (!same_element(x.ptr + (size_t)i * size,
                              y.ptr + (size_t)i * size,
                              ANTI_TYPE_ELEMENT(type), d)) {
                return 0;
            }
        }
        return 1;
    default:
        size = anti_rt_type_size(t);
        return size == 0 || memcmp(a, b, size) == 0;
    }
}

static uint64_t hash_value(uint64_t h, const unsigned char *p, int64_t type,
                           const struct anti_descriptor *d, int64_t owned)
{
    int64_t t = anti_rt_type_scalar(type);
    struct anti_text x;
    int64_t i;
    size_t size;

    switch (t) {
    case ANTI_TYPE_STRUCT:
    case ANTI_TYPE_CLASS:
        return hash_element(h, p, t, d);
    case ANTI_TYPE_HANDLE:
        return hash_run(h, p, (size_t)ANTI_TYPE_ELEMENT(type) * sizeof(void *));
    case ANTI_TYPE_REGEX: {
        const void *x;
        uint64_t v;
        memcpy(&x, p, sizeof x);
        v = anti_rt_pattern_hash(x);
        return hash_run(h, (const unsigned char *)&v, sizeof v);
    }
    case ANTI_TYPE_TUPLE:
        return d == NULL ? h : hash_fields(h, p, d);
    case ANTI_TYPE_VARIANT: {
        const struct anti_field *x = anti_rt_variant_case(p, d);
        uint64_t tag = x != NULL ? (uint64_t)x->owned : 0;
        h = hash_run(h, (const unsigned char *)&tag, sizeof tag);
        return x == NULL || x->descriptor == NULL
                   ? h
                   : hash_fields(h, p + x->offset, x->descriptor);
    }
    case ANTI_TYPE_OPTIONAL: {
        unsigned char has = (unsigned char)optional_has(p, d);
        h = hash_run(h, &has, 1);
        return has == 0 ? h
                        : hash_value(h, p + d->fields[0].offset,
                                     d->fields[0].type,
                                     d->fields[0].descriptor, 0);
    }
    case ANTI_TYPE_ARRAY:
        size = anti_rt_array_element_size(type, d);
        for (i = 0; size > 0 && i < ANTI_TYPE_COUNT(type); i++) {
            h = hash_element(h, p + (size_t)i * size, ANTI_TYPE_INNER(type),
                             anti_rt_array_element_descriptor(type, d));
        }
        return h;
    case ANTI_TYPE_STR:
        memcpy(&x, p, sizeof x);
        h = hash_run(h, (const unsigned char *)&x.len, sizeof x.len);
        return x.len > 0 ? hash_run(h, x.ptr, (size_t)x.len) : h;
    case ANTI_TYPE_SLICE:
        memcpy(&x, p, sizeof x);
        h = hash_run(h, (const unsigned char *)&x.len, sizeof x.len);
        if (!owned) {
            return hash_run(h, (const unsigned char *)&x.ptr, sizeof x.ptr);
        }
        size = anti_rt_element_size(type, d);
        for (i = 0; size > 0 && i < x.len; i++) {
            h = hash_element(h, x.ptr + (size_t)i * size,
                             ANTI_TYPE_ELEMENT(type), d);
        }
        return h;
    default:
        return hash_run(h, p, anti_rt_type_size(t));
    }
}

/* Compare the fields the chain declares, from the class up to the root.
   Two objects of different classes are never equal. */
int8_t anti_lang_Object_equals(struct anti_object *self,
                               struct anti_object *other)
{
    const struct anti_descriptor *d = anti_rt_descriptor(self);

    if (self == other) {
        return 1;
    }
    if (self == NULL || other == NULL ||
        d != anti_rt_descriptor(other)) {
        return 0;
    }
    for (; d != NULL; d = d->parent) {
        if (!same_fields((const unsigned char *)self,
                         (const unsigned char *)other, d)) {
            return 0;
        }
    }
    return 1;
}

/* FNV-1a over the fields that the level d of the chain of self declares,
   and those of every level above it first, into h. */
static uint64_t hash_level(const struct anti_object *self,
                           const struct anti_descriptor *d, uint64_t h)
{
    if (d == NULL) {
        return h;
    }
    h = hash_level(self, d->parent, h);
    return hash_fields(h, (const unsigned char *)self, d);
}

/* FNV-1a over the same fields that equals compares, so two equal objects
   hash alike. The fields go in the order of the object, those of the root
   of the chain first. */
uint64_t anti_lang_Object_hash(struct anti_object *self)
{
    return hash_level(self, anti_rt_descriptor(self), 1469598103934665603u);
}

/* Append the bytes of a C string. */
static void put(struct anti_builder *b, const char *text)
{
    anti_rt_builder_append(b, (const unsigned char *)text,
                           (int64_t)strlen(text));
}

/* DESIGN: serialize writes every number itself and calls no printf. A
   float takes the fewest digits that read back as the same value. They
   come from src/rt/text.c, so the text is the same on every target. */

/* An integer in decimal, from its magnitude and its sign. */
static void put_integer(struct anti_builder *b, uint64_t magnitude,
                        bool negative)
{
    unsigned char digits[21];
    int n = (int)sizeof digits;

    do {
        digits[--n] = (unsigned char)('0' + magnitude % 10);
        magnitude /= 10;
    } while (magnitude != 0);
    if (negative) {
        digits[--n] = '-';
    }
    anti_rt_builder_append(b, digits + n, (int64_t)sizeof digits - n);
}

static void put_signed(struct anti_builder *b, int64_t value)
{
    put_integer(b, value < 0 ? 0 - (uint64_t)value : (uint64_t)value,
                value < 0);
}

/* Append bytes as a JSON string, with the escapes that JSON requires.
   Every other byte goes out as it is, so the text keeps the bytes of a
   str. */
static void put_text(struct anti_builder *b, const unsigned char *bytes,
                     int64_t len)
{
    static const char hex[] = "0123456789abcdef";
    int64_t i;

    put(b, "\"");
    for (i = 0; i < len; i++) {
        unsigned char c = bytes[i];
        if (c == '"' || c == '\\') {
            anti_rt_builder_append(b, (const unsigned char *)"\\", 1);
            anti_rt_builder_append(b, &c, 1);
        } else if (c == '\n') {
            put(b, "\\n");
        } else if (c == '\r') {
            put(b, "\\r");
        } else if (c == '\t') {
            put(b, "\\t");
        } else if (c < 0x20) {
            unsigned char escape[6] = {'\\', 'u', '0', '0', 0, 0};
            escape[4] = (unsigned char)hex[c >> 4];
            escape[5] = (unsigned char)hex[c & 15];
            anti_rt_builder_append(b, escape, 6);
        } else {
            anti_rt_builder_append(b, &c, 1);
        }
    }
    put(b, "\"");
}

size_t anti_rt_element_size(int64_t type, const struct anti_descriptor *d)
{
    int64_t element = ANTI_TYPE_ELEMENT(type);

    if (element == ANTI_TYPE_STRUCT || element == ANTI_TYPE_CLASS ||
        element == ANTI_TYPE_VARIANT || element == ANTI_TYPE_OPTIONAL ||
        element == ANTI_TYPE_TUPLE) {
        return d != NULL ? (size_t)d->size : 0;
    }
    return anti_rt_type_size(element);
}

int anti_rt_element_walked(int64_t type, const struct anti_descriptor *d)
{
    int64_t element = ANTI_TYPE_ELEMENT(type);

    return element != ANTI_TYPE_CLASS && element != ANTI_TYPE_SLICE &&
           anti_rt_element_size(type, d) > 0;
}

static void serialize_into(struct anti_builder *b, const void *object,
                           const struct anti_descriptor *d);
static void put_value(struct anti_builder *b, const void *bytes,
                      int64_t type, const struct anti_descriptor *d,
                      int64_t owned);

/* Level k of an array at bytes, of levels levels of the lengths lengths,
   as a JSON array. The elements of the last level have size bytes, the
   type id inner and the descriptor e. */
static void put_level(struct anti_builder *b, const void *bytes,
                      const int64_t *lengths, int64_t levels, int64_t k,
                      size_t size, int64_t inner,
                      const struct anti_descriptor *e)
{
    size_t block = size;
    int64_t i;

    for (i = k + 1; i < levels; i++) {
        block *= (size_t)lengths[i];
    }
    put(b, "[");
    for (i = 0; i < lengths[k]; i++) {
        const char *at = (const char *)bytes + (size_t)i * block;
        put(b, i == 0 ? "" : ",");
        if (k + 1 == levels) {
            put_value(b, at, inner, e, 0);
        } else {
            put_level(b, at, lengths, levels, k + 1, size, inner, e);
        }
    }
    put(b, "]");
}

/* The fields of the struct d describes, as a JSON object. */
static void put_struct(struct anti_builder *b, const void *bytes,
                       const struct anti_descriptor *d)
{
    int64_t i;

    put(b, "{");
    for (i = 0; i < d->field_count; i++) {
        const struct anti_field *f = &d->fields[i];
        put(b, i == 0 ? "" : ",");
        put_text(b, f->name, f->name_length);
        put(b, ":");
        put_value(b, (const char *)bytes + f->offset, f->type, f->descriptor,
                  f->owned);
    }
    put(b, "}");
}

/* A slice. One that the object owns is written as its elements, and any
   other as null, as a pointer is. */
static void put_slice(struct anti_builder *b, const void *bytes,
                      int64_t type, const struct anti_descriptor *d,
                      int64_t owned)
{
    struct anti_text s;
    size_t size = anti_rt_element_size(type, d);
    int64_t i;

    memcpy(&s, bytes, sizeof s);
    if (!owned || s.ptr == NULL || !anti_rt_element_walked(type, d)) {
        put(b, "null");
        return;
    }
    put(b, "[");
    for (i = 0; i < s.len; i++) {
        put(b, i == 0 ? "" : ",");
        put_value(b, s.ptr + (size_t)i * size, ANTI_TYPE_ELEMENT(type), d, 0);
    }
    put(b, "]");
}

/* One value of the type id at bytes. d is the descriptor of the struct
   or the class that the type reaches. owned says that a pointer or a
   slice owns what it points at. A type no walk reads is null. */
static void put_value(struct anti_builder *b, const void *bytes,
                      int64_t type, const struct anti_descriptor *d,
                      int64_t owned)
{
    int64_t t = anti_rt_type_scalar(type);

    switch (t) {
    case ANTI_TYPE_BOOL:
        put(b, *(const unsigned char *)bytes != 0 ? "true" : "false");
        return;
    case ANTI_TYPE_CHAR: {
        uint32_t c;
        unsigned char utf8[4];
        memcpy(&c, bytes, sizeof c);
        put_text(b, utf8, (int64_t)anti_rt_utf8_encode(c, utf8));
        return;
    }
    /* An f16 is written as the f32 a read gives, which reads back to
       the same sixteen bits. */
    case ANTI_TYPE_F16: {
        uint16_t h;
        memcpy(&h, bytes, sizeof h);
        anti_rt_builder_float(b, (double)anti_rt_f16_widen(h), -1, 0, 1);
        return;
    }
    case ANTI_TYPE_F32: {
        float v;
        memcpy(&v, bytes, sizeof v);
        anti_rt_builder_float(b, (double)v, -1, 0, 1);
        return;
    }
    case ANTI_TYPE_F64: {
        double v;
        memcpy(&v, bytes, sizeof v);
        anti_rt_builder_float(b, v, -1, 0, 0);
        return;
    }
    case ANTI_TYPE_STR: {
        struct anti_text s;
        memcpy(&s, bytes, sizeof s);
        put_text(b, s.ptr, s.len);
        return;
    }
    case ANTI_TYPE_PTR:
    case ANTI_TYPE_FN: {
        void *value;
        memcpy(&value, bytes, sizeof value);
        if (value == NULL) {
            put(b, "null");
        } else if (owned && ANTI_TYPE_ELEMENT(type) == ANTI_TYPE_CLASS) {
            value = anti_rt_object_of(value);
            serialize_into(b, value, anti_rt_descriptor(value));
        } else if (owned && t == ANTI_TYPE_PTR &&
                   anti_rt_element_walked(type, d)) {
            put_value(b, value, ANTI_TYPE_ELEMENT(type), d, 0);
        } else {
            /* It refers to something outside the object. */
            put(b, "null");
        }
        return;
    }
    case ANTI_TYPE_SLICE:
        put_slice(b, bytes, type, d, owned);
        return;
    case ANTI_TYPE_STRUCT:
        if (d != NULL) {
            put_struct(b, bytes, d);
        } else {
            put(b, "null");
        }
        return;
    case ANTI_TYPE_CLASS:
        serialize_into(b, bytes, d);
        return;
    /* A variant is an object whose one member names the case and holds
       its fields, `{"Circle":{"r":2}}`, and a `?T` is its value or null. */
    case ANTI_TYPE_VARIANT: {
        const struct anti_field *c = anti_rt_variant_case(bytes, d);
        if (c == NULL) {
            put(b, "null");
            return;
        }
        put(b, "{");
        put_text(b, c->name, c->name_length);
        put(b, ":");
        if (c->descriptor != NULL) {
            put_struct(b, (const char *)bytes + c->offset, c->descriptor);
        } else {
            put(b, "{}");
        }
        put(b, "}");
        return;
    }
    /* A tuple is a JSON array of its parts, and an array one of its
       elements through every level of it, in order. */
    case ANTI_TYPE_TUPLE: {
        int64_t i;
        if (d == NULL) {
            put(b, "null");
            return;
        }
        put(b, "[");
        for (i = 0; i < d->field_count; i++) {
            const struct anti_field *f = &d->fields[i];
            put(b, i == 0 ? "" : ",");
            put_value(b, (const char *)bytes + f->offset, f->type,
                      f->descriptor, 0);
        }
        put(b, "]");
        return;
    }
    case ANTI_TYPE_ARRAY: {
        int64_t lengths[ANTI_ARRAY_LEVELS];
        int64_t levels = anti_rt_array_levels(type, d, lengths);
        size_t size = anti_rt_array_element_size(type, d);
        if (size == 0 || levels == 0 || ANTI_TYPE_COUNT(type) == 0) {
            put(b, "null");
            return;
        }
        put_level(b, bytes, lengths, levels, 0, size, ANTI_TYPE_INNER(type),
                  anti_rt_array_element_descriptor(type, d));
        return;
    }
    case ANTI_TYPE_OPTIONAL:
        if (!optional_has(bytes, d)) {
            put(b, "null");
        } else {
            put_value(b, (const char *)bytes + d->fields[0].offset,
                      d->fields[0].type, d->fields[0].descriptor, 0);
        }
        return;
    default:
        if (anti_rt_type_size(t) == 0) {
            put(b, "null");
        } else if (anti_rt_type_signed(t)) {
            put_signed(b, (int64_t)anti_rt_load_integer(bytes, t));
        } else {
            put_integer(b, anti_rt_load_integer(bytes, t), false);
        }
        return;
    }
}

/* The object as a JSON object: the class name under "type", then one
   member per field of the chain, the root's first. */
static void serialize_into(struct anti_builder *b, const void *object,
                           const struct anti_descriptor *d)
{
    const struct anti_descriptor *up;
    int64_t depth;
    int64_t i;

    if (object == NULL || d == NULL) {
        put(b, "null");
        return;
    }
    put(b, "{\"type\":");
    put_text(b, d->name, d->name_length);
    /* The ancestor list is indexed by depth, so the root comes first and
       the class itself last. */
    for (depth = 0; depth <= d->depth; depth++) {
        up = d->ancestors == NULL ? NULL : d->ancestors[depth];
        if (up == NULL) {
            continue;
        }
        for (i = 0; i < up->field_count; i++) {
            const struct anti_field *f = &up->fields[i];
            put(b, ",");
            put_text(b, f->name, f->name_length);
            put(b, ":");
            put_value(b, (const char *)object + f->offset, f->type,
                      f->descriptor, f->owned);
        }
    }
    put(b, "}");
}

/* DESIGN: the default `serialize` writes the JSON that `anti.json`
   defines. Each field goes out by the type its record names. A bool is
   true or false, a char a string of one character and a str a string.
   An inline struct or class is an object, and so is what an `own`
   pointer points at. An `own` slice is an array of its elements. A
   pointer, a slice and a function pointer the object does not own are
   null, since each refers to something outside the object. A union, an
   array, a bitfield and an `own` slice of class values or of slices are
   null, and a class that wants them replaces the body. */
void anti_lang_Object_serialize(struct anti_object *self, void *out)
{
    serialize_into(out, self, anti_rt_descriptor(self));
}

/* Elements of a generic collection */

const struct anti_field *anti_rt_type_arg(const void *object, int64_t depth,
                                          int64_t index)
{
    const struct anti_descriptor *d = anti_rt_descriptor(object);
    const struct anti_descriptor *up;

    if (d == NULL || depth < 0 || depth > d->depth || d->ancestors == NULL) {
        return NULL;
    }
    up = d->ancestors[depth];
    if (up == NULL || index < 0 || index >= up->type_arg_count) {
        return NULL;
    }
    return &up->type_args[index];
}

/* The number of elements of an array argument. Its record holds the
   size of the whole array and the descriptor of a struct or a class
   element. */
static int64_t array_length(const struct anti_field *arg)
{
    size_t size = anti_rt_element_size(arg->type, arg->descriptor);

    return size == 0 ? 0 : arg->offset / (int64_t)size;
}

/* A class value in place, written by the `serialize` of its table, so a
   class that replaces it, a collection among them, writes its own form. */
static void serialize_object(struct anti_builder *b, void *object)
{
    void (*write)(struct anti_object *, void *) =
        (void (*)(struct anti_object *, void *))anti_rt_entry_body(
            object, ANTI_ENTRY_SERIALIZE);

    if (write == NULL) {
        put(b, "null");
        return;
    }
    write(object, b);
}

void anti_rt_element_serialize(void *out, void *bytes,
                               const struct anti_field *arg)
{
    struct anti_builder *b = out;
    int64_t t;
    int64_t i;

    if (arg == NULL) {
        put(b, "null");
        return;
    }
    t = anti_rt_type_scalar(arg->type);
    if (t == ANTI_TYPE_CLASS) {
        serialize_object(b, bytes);
        return;
    }
    if (t == ANTI_TYPE_ARRAY) {
        size_t size = anti_rt_element_size(arg->type, arg->descriptor);
        put(b, "[");
        for (i = 0; i < array_length(arg); i++) {
            char *at = (char *)bytes + (size_t)i * size;
            put(b, i == 0 ? "" : ",");
            if (ANTI_TYPE_ELEMENT(arg->type) == ANTI_TYPE_CLASS) {
                serialize_object(b, at);
            } else {
                put_value(b, at, ANTI_TYPE_ELEMENT(arg->type),
                          arg->descriptor, 0);
            }
        }
        put(b, "]");
        return;
    }
    put_value(b, bytes, arg->type, arg->descriptor, 0);
}

static void show_value(struct anti_builder *b, void *bytes, int64_t type,
                       const struct anti_descriptor *d, int64_t size);

/* A class value in place, written by the `to_text` of its table. */
static void show_object(struct anti_builder *b, void *object)
{
    struct anti_text (*text)(struct anti_object *) =
        (struct anti_text(*)(struct anti_object *))anti_rt_entry_body(
            object, ANTI_ENTRY_TO_TEXT);
    struct anti_text shown;

    if (text == NULL) {
        put(b, "none");
        return;
    }
    shown = text(object);
    anti_rt_builder_append(b, shown.ptr, shown.len);
}

/* count elements of the type id in a row, as `[a, b]`. */
static void show_row(struct anti_builder *b, char *at, int64_t count,
                     int64_t type, const struct anti_descriptor *d)
{
    size_t size = anti_rt_element_size(type << 8, d);
    int64_t i;

    put(b, "[");
    for (i = 0; i < count && size > 0; i++) {
        put(b, i == 0 ? "" : ", ");
        show_value(b, at + (size_t)i * size, type, d, (int64_t)size);
    }
    put(b, "]");
}

/* DESIGN: the text of an element, as `to_text` of a collection writes
   it. A number, a bool and `none` stand as they are, and a str and a
   char as a quoted JSON string. A class writes its own `to_text`, in
   place or through a pointer. A struct is `{"x": 1, "y": 2}`, and an
   array and a slice are `[1, 2]`. A pointer to anything else writes what
   it points at, and a type that no walk reads is `?`. size is the bytes
   of an array, which its type id does not give. */
static void show_value(struct anti_builder *b, void *bytes, int64_t type,
                       const struct anti_descriptor *d, int64_t size)
{
    int64_t t = anti_rt_type_scalar(type);
    int64_t i;

    switch (t) {
    case ANTI_TYPE_CLASS:
        show_object(b, bytes);
        return;
    case ANTI_TYPE_PTR:
    case ANTI_TYPE_FN: {
        void *value;
        memcpy(&value, bytes, sizeof value);
        if (value == NULL) {
            put(b, "none");
        } else if (t == ANTI_TYPE_PTR &&
                   ANTI_TYPE_ELEMENT(type) == ANTI_TYPE_CLASS) {
            show_object(b, anti_rt_object_of(value));
        } else if (t == ANTI_TYPE_PTR && anti_rt_element_walked(type, d)) {
            show_value(b, value, ANTI_TYPE_ELEMENT(type), d, 0);
        } else {
            put(b, "?");
        }
        return;
    }
    case ANTI_TYPE_SLICE: {
        struct anti_text s;
        memcpy(&s, bytes, sizeof s);
        if (anti_rt_element_size(type, d) == 0) {
            put(b, "?");
        } else {
            show_row(b, (char *)s.ptr, s.len, ANTI_TYPE_ELEMENT(type), d);
        }
        return;
    }
    case ANTI_TYPE_ARRAY: {
        size_t one = anti_rt_element_size(type, d);
        if (one == 0 || size == 0) {
            put(b, "?");
        } else {
            show_row(b, bytes, size / (int64_t)one, ANTI_TYPE_ELEMENT(type), d);
        }
        return;
    }
    case ANTI_TYPE_STRUCT:
        if (d == NULL) {
            put(b, "?");
            return;
        }
        put(b, "{");
        for (i = 0; i < d->field_count; i++) {
            const struct anti_field *f = &d->fields[i];
            put(b, i == 0 ? "" : ", ");
            put_text(b, f->name, f->name_length);
            put(b, ": ");
            show_value(b, (char *)bytes + f->offset, f->type, f->descriptor,
                       0);
        }
        put(b, "}");
        return;
    case ANTI_TYPE_NONE:
    case ANTI_TYPE_UNION:
    case ANTI_TYPE_HANDLE:
    case ANTI_TYPE_REGEX:
        put(b, "?");
        return;
    default:
        put_value(b, bytes, type, d, 0);
        return;
    }
}

void anti_rt_element_text(void *out, void *bytes, const struct anti_field *arg)
{
    if (arg == NULL) {
        put(out, "?");
        return;
    }
    show_value(out, bytes, arg->type, arg->descriptor, arg->offset);
}

/* The root frees nothing. The teardown the compiler writes for a class
   runs each destruct body of its chain and destroys what it owns. */
void anti_lang_Object_destruct(struct anti_object *self)
{
    (void)self;
}

/* DESIGN: the nine hooks of the root do nothing. A class that wants one
   replaces it with a concrete function of the same name. src/rt/hooks.c
   compares the entry with the body here before it dispatches, so a class
   that replaced none pays a load and a compare. */
void anti_lang_Object_created(struct anti_object *self)
{
    (void)self;
}

void anti_lang_Object_destroyed(struct anti_object *self)
{
    (void)self;
}

void anti_lang_Object_copied(struct anti_object *self,
                             struct anti_object *from)
{
    (void)self;
    (void)from;
}

void anti_lang_Object_dispatched(struct anti_object *self)
{
    (void)self;
}

void anti_lang_Object_joined(struct anti_object *self)
{
    (void)self;
}

void anti_lang_Object_enter(struct anti_object *self, struct anti_text name)
{
    (void)self;
    (void)name;
}

void anti_lang_Object_leave(struct anti_object *self, struct anti_text name)
{
    (void)self;
    (void)name;
}

void anti_lang_Object_failed(struct anti_object *self, struct anti_text name,
                             struct anti_object *e)
{
    (void)self;
    (void)name;
    (void)e;
}

void anti_lang_Object_changed(struct anti_object *self,
                              const struct anti_field *field)
{
    (void)self;
    (void)field;
}

/* The union carries a table entry across between the two kinds of
   pointer. */
union entry_cast {
    const void *entry;
    anti_rt_body body;
};

anti_rt_body anti_rt_entry_body(const void *object, int entry)
{
    const struct anti_object *o = object;
    union entry_cast cast;

    if (o == NULL || o->table == NULL) {
        return NULL;
    }
    cast.entry = o->table[entry];
    return cast.body;
}

const void *anti_rt_body_entry(anti_rt_body body)
{
    union entry_cast cast;

    cast.body = body;
    return cast.entry;
}

/* The bytes of the object. The copy the compiler writes for a class
   replaces this body in its table and copies what the object owns. */
void anti_lang_Object_copy(struct anti_object *self, struct anti_object *to)
{
    const struct anti_descriptor *d = anti_rt_descriptor(self);

    if (d != NULL) {
        memcpy(to, self, (size_t)d->size);
    }
}

/* DESIGN: delete, destroy and dup call the teardown and the copy that
   the compiler writes for every class. The destruct and copy entries of
   the table hold them. Ownership therefore never depends on the field
   list, which --no-reflect drops. A copy keeps the place a pointer to an
   interface points at, in the new object. */
void *anti_rt_dup(void *object, const struct anti_descriptor *type)
{
    char *start = checked_object(object, type);
    const struct anti_descriptor *d = anti_rt_descriptor(start);
    void (*copy)(struct anti_object *, struct anti_object *) =
        (void (*)(struct anti_object *, struct anti_object *))
            anti_rt_entry_body(start, ANTI_ENTRY_COPY);
    char *made;

    if (d == NULL || copy == NULL) {
        return NULL;
    }
    made = malloc((size_t)d->size);
    if (made == NULL) {
        return NULL;
    }
    copy((struct anti_object *)start, (struct anti_object *)made);
    return made + ((char *)object - start);
}

struct anti_object anti_rt_give_nothing = {NULL};

void anti_rt_give(struct anti_object *from, void *p)
{
    void (*give)(struct anti_object *, void *);

    if (from == &anti_rt_give_nothing) {
        return;
    }
    if (from == NULL) {
        free(p);
        return;
    }
    give = (void (*)(struct anti_object *, void *))anti_rt_entry_body(
        from, ANTI_ENTRY_FREE);
    give(from, p);
}

void anti_rt_destroy_from(void *object, const struct anti_descriptor *type,
                          struct anti_object *from)
{
    void *start = checked_object(object, type);
    void (*teardown)(struct anti_object *, struct anti_object *) =
        (void (*)(struct anti_object *, struct anti_object *))
            anti_rt_entry_body(start, ANTI_ENTRY_DROP);

    if (teardown != NULL) {
        teardown(start, from);
    }
}

void anti_rt_delete_from(void *object, const struct anti_descriptor *type,
                         struct anti_object *from)
{
    void *start = checked_object(object, type);

    if (start == NULL) {
        return;
    }
    anti_rt_destroy_from(start, type, from);
    anti_rt_give(from, start);
}

void anti_rt_destroy(void *object, const struct anti_descriptor *type)
{
    anti_rt_destroy_from(object, type, NULL);
}

void anti_rt_delete(void *object, const struct anti_descriptor *type)
{
    anti_rt_delete_from(object, type, NULL);
}

/* DESIGN: every sequence of class values is torn down last to first, a
   local array and an `own` slice alike. */
void anti_rt_destroy_elements(void *elements, int64_t count,
                              const struct anti_descriptor *type,
                              struct anti_object *from)
{
    int64_t i;

    for (i = count - 1; elements != NULL && type != NULL && i >= 0; i--) {
        anti_rt_destroy_from((char *)elements + i * type->size, type, from);
    }
}

void anti_rt_copy_elements(void *from, void *into, int64_t count,
                           const struct anti_descriptor *type)
{
    int64_t i;

    for (i = 0; from != NULL && into != NULL && type != NULL && i < count;
         i++) {
        char *at = checked_object((char *)from + i * type->size, type);
        void (*copy)(struct anti_object *, struct anti_object *) =
            (void (*)(struct anti_object *, struct anti_object *))
                anti_rt_entry_body(at, ANTI_ENTRY_COPY);
        if (copy != NULL) {
            copy((struct anti_object *)at,
                 (struct anti_object *)((char *)into + i * type->size));
        }
    }
}

void *anti_rt_copy_buffer(const void *from, int64_t bytes)
{
    void *made;

    if (from == NULL || bytes <= 0) {
        return NULL;
    }
    made = malloc((size_t)bytes);
    if (made != NULL) {
        memcpy(made, from, (size_t)bytes);
    }
    return made;
}

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "utf.h"

/* DESIGN: the default bodies of anti.rt.Object walk the field list of the
   descriptor. They are slow by design: a class that needs speed replaces
   the one it cares about with a concrete function. A build with
   --no-reflect has no field list, and these bodies then fall back to the
   class name and the address. */

/* DESIGN: the descriptor of the root belongs to the runtime. Every
   module of a program refers to this one, so `p is *Object` compares the
   same address wherever it is written, and no two modules define it. */
static const unsigned char object_name[] = "Object";

const struct anti_descriptor *const anti_rt_Object_ancestors[1] = {
    &anti_rt_Object_descriptor
};

const struct anti_descriptor anti_rt_Object_descriptor = {
    object_name, 6, NULL, (int64_t)sizeof(struct anti_object), 0,
    anti_rt_Object_ancestors, 0, NULL, NULL, 0, 0, NULL
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

struct anti_text anti_rt_Object_type_name(struct anti_object *self)
{
    const struct anti_descriptor *d = anti_rt_descriptor(self);
    struct anti_text text;

    text.ptr = d == NULL ? (const unsigned char *)"" : d->name;
    text.len = d == NULL ? 0 : d->name_length;
    return text;
}

struct anti_text anti_rt_Object_to_text(struct anti_object *self)
{
    return anti_rt_Object_type_name(self);
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
    case ANTI_TYPE_U16: return 2;
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

/* The bytes of a field that equals and hash compare: a scalar, a pointer
   and an enum. A str, a slice and an inline value give 0. */
static size_t compared_size(int64_t type)
{
    int64_t t = anti_rt_type_scalar(type);

    return t == ANTI_TYPE_STR || t == ANTI_TYPE_SLICE ? 0
                                                      : anti_rt_type_size(t);
}

/* Compare the fields the chain declares, from the root down. Two objects
   of different classes are never equal. */
int8_t anti_rt_Object_equals(struct anti_object *self,
                             struct anti_object *other)
{
    const struct anti_descriptor *d = anti_rt_descriptor(self);
    int64_t i;

    if (self == other) {
        return 1;
    }
    if (self == NULL || other == NULL ||
        d != anti_rt_descriptor(other)) {
        return 0;
    }
    for (; d != NULL; d = d->parent) {
        for (i = 0; i < d->field_count; i++) {
            const struct anti_field *f = &d->fields[i];
            size_t size = compared_size(f->type);
            if (size == 0) {
                continue;
            }
            if (memcmp((const char *)self + f->offset,
                       (const char *)other + f->offset, size) != 0) {
                return 0;
            }
        }
    }
    return 1;
}

/* FNV-1a over the same fields that equals compares, so two equal objects
   hash alike. */
uint64_t anti_rt_Object_hash(struct anti_object *self)
{
    const struct anti_descriptor *d = anti_rt_descriptor(self);
    uint64_t h = 1469598103934665603u;
    int64_t i;
    size_t k;

    for (; d != NULL; d = d->parent) {
        for (i = 0; i < d->field_count; i++) {
            const struct anti_field *f = &d->fields[i];
            const unsigned char *bytes =
                (const unsigned char *)self + f->offset;
            size_t size = compared_size(f->type);
            for (k = 0; k < size; k++) {
                h = (h ^ bytes[k]) * 1099511628211u;
            }
        }
    }
    return h;
}

/* Append the bytes of a C string. */
static void put(struct anti_builder *b, const char *text)
{
    anti_rt_builder_append(b, (const unsigned char *)text,
                           (int64_t)strlen(text));
}

/* Append bytes as a JSON string, with the escapes that JSON requires.
   Every other byte goes out as it is, so the text keeps the bytes of a
   str. */
static void put_text(struct anti_builder *b, const unsigned char *bytes,
                     int64_t len)
{
    char escape[8];
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
            snprintf(escape, sizeof escape, "\\u%04x", (unsigned)c);
            put(b, escape);
        } else {
            anti_rt_builder_append(b, &c, 1);
        }
    }
    put(b, "\"");
}

size_t anti_rt_element_size(int64_t type, const struct anti_descriptor *d)
{
    int64_t element = ANTI_TYPE_ELEMENT(type);

    if (element == ANTI_TYPE_STRUCT || element == ANTI_TYPE_CLASS) {
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
   other as its address and its length, as a pointer is. */
static void put_slice(struct anti_builder *b, const void *bytes,
                      int64_t type, const struct anti_descriptor *d,
                      int64_t owned)
{
    struct anti_text s;
    char number[80];
    size_t size = anti_rt_element_size(type, d);
    int64_t i;

    memcpy(&s, bytes, sizeof s);
    if (!owned) {
        snprintf(number, sizeof number, "{\"address\":%llu,\"length\":%lld}",
                 (unsigned long long)(uintptr_t)s.ptr, (long long)s.len);
        put(b, number);
        return;
    }
    if (s.ptr == NULL || !anti_rt_element_walked(type, d)) {
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
    char number[48];
    int64_t t = anti_rt_type_scalar(type);

    switch (t) {
    case ANTI_TYPE_BOOL:
        put(b, *(const unsigned char *)bytes != 0 ? "true" : "false");
        return;
    case ANTI_TYPE_CHAR: {
        uint32_t c;
        unsigned char utf8[4];
        memcpy(&c, bytes, sizeof c);
        put_text(b, utf8, (int64_t)anti_utf8_encode(c, utf8));
        return;
    }
    case ANTI_TYPE_F32: {
        float v;
        memcpy(&v, bytes, sizeof v);
        snprintf(number, sizeof number, "%.9g", (double)v);
        put(b, number);
        return;
    }
    case ANTI_TYPE_F64: {
        double v;
        memcpy(&v, bytes, sizeof v);
        snprintf(number, sizeof number, "%.17g", v);
        put(b, number);
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
            snprintf(number, sizeof number, "%llu",
                     (unsigned long long)(uintptr_t)value);
            put(b, number);
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
    default:
        if (anti_rt_type_size(t) == 0) {
            put(b, "null");
        } else if (anti_rt_type_signed(t)) {
            snprintf(number, sizeof number, "%lld",
                     (long long)(int64_t)anti_rt_load_integer(bytes, t));
            put(b, number);
        } else {
            snprintf(number, sizeof number, "%llu",
                     (unsigned long long)anti_rt_load_integer(bytes, t));
            put(b, number);
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
   pointer the object does not own is its address, and a slice it does
   not own is an object of its address and its length. A union, an
   array, a bitfield and an `own` slice of class values or of slices are
   null, and a class that wants them replaces the body. */
void anti_rt_Object_serialize(struct anti_object *self, void *out)
{
    serialize_into(out, self, anti_rt_descriptor(self));
}

/* The root frees nothing. delete frees the `own` fields of each class of
   the chain and then the object itself. */
void anti_rt_Object_destruct(struct anti_object *self)
{
    (void)self;
}

/* The address of the memory an `own` field points at, and its size in
   bytes. A pointer owns one element of its type, and a slice its length
   in elements. */
static void **owned_at(void *object, const struct anti_field *f,
                       int64_t *size)
{
    void **slot = (void **)((char *)object + f->offset);
    int64_t count = 1;

    if (ANTI_TYPE_OF(f->type) == ANTI_TYPE_SLICE) {
        memcpy(&count, (char *)slot + sizeof(void *), sizeof count);
    }
    *size = count * (int64_t)anti_rt_element_size(f->type, f->descriptor);
    return slot;
}

/* A copy of every byte of the object, then a fresh copy of the memory
   behind each `own` field of the chain. An object behind an `own`
   pointer is copied by its own copy entry, at the size of its own class.
   Its `own` fields are then copied too. */
void anti_rt_Object_copy(struct anti_object *self, struct anti_object *to)
{
    const struct anti_descriptor *d = anti_rt_descriptor(self);
    int64_t i;

    if (d == NULL) {
        return;
    }
    memcpy(to, self, (size_t)d->size);
    for (; d != NULL; d = d->parent) {
        for (i = 0; i < d->field_count; i++) {
            const struct anti_field *f = &d->fields[i];
            int64_t size = 0;
            void **from;
            void **into;
            if (!f->owned) {
                continue;
            }
            from = owned_at(self, f, &size);
            into = owned_at(to, f, &size);
            if (*from == NULL) {
                continue;
            }
            if (ANTI_TYPE_OF(f->type) == ANTI_TYPE_PTR &&
                ANTI_TYPE_ELEMENT(f->type) == ANTI_TYPE_CLASS) {
                /* A pointer to an interface points into its object, and
                   the copy keeps the same place in the new one. */
                size_t inside = (size_t)((char *)*from -
                                         (char *)anti_rt_object_of(*from));
                char *made = anti_rt_dup(*from);
                *into = made == NULL ? NULL : made + inside;
                continue;
            }
            if (size <= 0) {
                continue;
            }
            *into = malloc((size_t)size);
            if (*into != NULL) {
                memcpy(*into, *from, (size_t)size);
            }
        }
    }
}

/* The entry of a table, which the compiler fills with the function the
   concrete class ended with. */
static void *table_entry(const void *object, enum anti_entry entry)
{
    const struct anti_object *o = object;

    return o == NULL || o->table == NULL ? NULL : (void *)o->table[entry];
}

void *anti_rt_dup(void *object)
{
    const struct anti_descriptor *d;

    object = anti_rt_object_of(object);
    d = anti_rt_descriptor(object);
    void (*copy)(struct anti_object *, struct anti_object *) =
        (void (*)(struct anti_object *, struct anti_object *))
            table_entry(object, ANTI_ENTRY_COPY);
    void *made;

    if (d == NULL || copy == NULL) {
        return NULL;
    }
    made = malloc((size_t)d->size);
    if (made != NULL) {
        copy(object, made);
    }
    return made;
}

/* DESIGN: the destruct body of the concrete class runs first and the root's
   last. A class therefore tears down what it added before its base does.
   The `own` fields of each level are freed after every body has run, so
   a body still reads what it owns. */
void anti_rt_destroy(void *object)
{
    const struct anti_descriptor *d;
    const struct anti_descriptor *level;
    int64_t i;

    object = anti_rt_object_of(object);
    d = anti_rt_descriptor(object);

    if (d == NULL) {
        return;
    }
    for (level = d; level != NULL; level = level->parent) {
        if (level->destruct != NULL) {
            level->destruct(object);
        }
    }
    for (level = d; level != NULL; level = level->parent) {
        for (i = 0; i < level->field_count; i++) {
            const struct anti_field *f = &level->fields[i];
            int64_t size = 0;
            void **slot;
            if (!f->owned) {
                continue;
            }
            slot = owned_at(object, f, &size);
            /* A pointer to an interface points into its object, and the
               memory to free starts at the object. */
            if (ANTI_TYPE_OF(f->type) == ANTI_TYPE_PTR &&
                ANTI_TYPE_ELEMENT(f->type) == ANTI_TYPE_CLASS) {
                free(anti_rt_object_of(*slot));
            } else {
                free(*slot);
            }
            *slot = NULL;
        }
    }
}

void anti_rt_delete(void *object)
{
    object = anti_rt_object_of(object);
    anti_rt_destroy(object);
    free(object);
}

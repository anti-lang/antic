#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "f16.h"
#include "object.h"
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
    anti_lang_Object_ancestors, 0, NULL, NULL, 0, 0, NULL
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
int8_t anti_lang_Object_equals(struct anti_object *self,
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
uint64_t anti_lang_Object_hash(struct anti_object *self)
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

/* DESIGN: serialize writes every number itself and calls no printf. A
   float takes the fewest digits that read back as the same value. They
   come from rt/text.c, so the text is the same on every target. */

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
    size_t size = anti_rt_element_size(type, d);
    int64_t i;

    memcpy(&s, bytes, sizeof s);
    if (!owned) {
        put(b, "{\"address\":");
        put_integer(b, (uint64_t)(uintptr_t)s.ptr, false);
        put(b, ",\"length\":");
        put_signed(b, s.len);
        put(b, "}");
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
    /* An f16 is written as the f32 a read gives, which reads back to
       the same sixteen bits. */
    case ANTI_TYPE_F16: {
        uint16_t h;
        memcpy(&h, bytes, sizeof h);
        anti_rt_builder_float(b, (double)anti_f16_widen(h), -1, 0, 1);
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
            put_integer(b, (uint64_t)(uintptr_t)value, false);
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
   pointer the object does not own is its address, and a slice it does
   not own is an object of its address and its length. A union, an
   array, a bitfield and an `own` slice of class values or of slices are
   null, and a class that wants them replaces the body. */
void anti_lang_Object_serialize(struct anti_object *self, void *out)
{
    serialize_into(out, self, anti_rt_descriptor(self));
}

/* The root frees nothing. The teardown the compiler writes for a class
   runs each destruct body of its chain and destroys what it owns. */
void anti_lang_Object_destruct(struct anti_object *self)
{
    (void)self;
}

/* DESIGN: the nine hooks of the root do nothing. A class that wants one
   replaces it with a concrete function of the same name. rt/hooks.c
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

/* The entry of a table, which the compiler fills with the function the
   concrete class ended with. */
static void *table_entry(const void *object, enum anti_entry entry)
{
    const struct anti_object *o = object;

    return o == NULL || o->table == NULL ? NULL : (void *)o->table[entry];
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
            table_entry(start, ANTI_ENTRY_COPY);
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

void anti_rt_destroy(void *object, const struct anti_descriptor *type)
{
    void *start = checked_object(object, type);
    void (*teardown)(struct anti_object *) =
        (void (*)(struct anti_object *))table_entry(start, ANTI_ENTRY_DROP);

    if (teardown != NULL) {
        teardown(start);
    }
}

void anti_rt_delete(void *object, const struct anti_descriptor *type)
{
    void *start = checked_object(object, type);

    anti_rt_destroy(start, type);
    free(start);
}

/* DESIGN: every sequence of class values is torn down last to first, a
   local array and an `own` slice alike. */
void anti_rt_destroy_elements(void *elements, int64_t count,
                              const struct anti_descriptor *type)
{
    int64_t i;

    for (i = count - 1; elements != NULL && type != NULL && i >= 0; i--) {
        anti_rt_destroy((char *)elements + i * type->size, type);
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
                table_entry(at, ANTI_ENTRY_COPY);
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

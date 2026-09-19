/* reflect.new and Object.deserialize, which read the registry of the
   classes of a program. */
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "registry.h"
#include "utf.h"

static bool same_bytes(const unsigned char *a, int64_t a_length,
                       const unsigned char *b, int64_t b_length)
{
    return a_length == b_length &&
           (a_length == 0 || memcmp(a, b, (size_t)a_length) == 0);
}

/* DESIGN: a name without a dot is the name the descriptor holds, which
   two modules may both declare. It finds a class when one class alone
   has it. `module.Class` names the class of one module and is never in
   doubt. */
const struct anti_class *anti_rt_registry_find(const unsigned char *name,
                                               int64_t length)
{
    const struct anti_class *found = NULL;
    int64_t dot = length;
    int64_t i;

    while (dot > 0 && name[dot - 1] != '.') {
        dot--;
    }
    for (i = 0; i < anti_rt_registry.count; i++) {
        const struct anti_class *c = &anti_rt_registry.classes[i];
        if (!same_bytes(c->descriptor->name, c->descriptor->name_length,
                        name + dot, length - dot)) {
            continue;
        }
        if (dot > 0) {
            if (same_bytes(c->module, c->module_length, name, dot - 1)) {
                return c;
            }
            continue;
        }
        if (found != NULL) {
            return NULL;
        }
        found = c;
    }
    return found;
}

/* A zeroed object of the class, prepared as a literal of it would be. */
static void *build(const struct anti_class *c)
{
    void *object = calloc(1, (size_t)c->descriptor->size);

    if (object != NULL) {
        c->init(object);
    }
    return object;
}

void *anti_rt_reflect_new(const unsigned char *name, int64_t length)
{
    const struct anti_class *c = anti_rt_registry_find(name, length);

    /* A construct with arguments has none to take here, and a required
       class field has no value. */
    if (c == NULL ||
        (c->flags & (ANTI_CLASS_ARGS | ANTI_CLASS_REQUIRED)) != 0) {
        return NULL;
    }
    return build(c);
}

/* Reading JSON */

/* The longest member name or class name the reader keeps. */
#define NAME_ROOM 256
/* How deep objects and arrays may nest. */
#define DEPTH_LIMIT 64

struct reader {
    const unsigned char *at;
    const unsigned char *end;
};

static void skip_space(struct reader *r)
{
    while (r->at < r->end && (*r->at == ' ' || *r->at == '\t' ||
                              *r->at == '\n' || *r->at == '\r')) {
        r->at++;
    }
}

/* Skip white space and take c when it comes next. */
static bool take(struct reader *r, unsigned char c)
{
    skip_space(r);
    if (r->at < r->end && *r->at == c) {
        r->at++;
        return true;
    }
    return false;
}

static bool take_word(struct reader *r, const char *word)
{
    size_t n = strlen(word);

    skip_space(r);
    if ((size_t)(r->end - r->at) >= n && memcmp(r->at, word, n) == 0) {
        r->at += n;
        return true;
    }
    return false;
}

static int hex_digit(unsigned char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

/* Append one byte to out, or fail when out holds room bytes already. A
   NULL out keeps nothing and counts the bytes. */
static bool put_byte(unsigned char *out, size_t room, size_t *length,
                     unsigned char b)
{
    if (out != NULL) {
        if (*length >= room) {
            return false;
        }
        out[*length] = b;
    }
    (*length)++;
    return true;
}

/* A JSON string, decoded into out when out is not NULL, and refused when
   it takes more than room bytes there. A \u escape of a surrogate half is
   refused, since the serializer writes none. */
static bool read_string(struct reader *r, unsigned char *out, size_t room,
                        size_t *length)
{
    *length = 0;
    if (!take(r, '"')) {
        return false;
    }
    while (r->at < r->end && *r->at != '"') {
        unsigned char c = *r->at++;
        if (c < 0x20) {
            return false;
        }
        if (c != '\\') {
            if (!put_byte(out, room, length, c)) {
                return false;
            }
            continue;
        }
        if (r->at >= r->end) {
            return false;
        }
        c = *r->at++;
        switch (c) {
        case '"': case '\\': case '/':
            break;
        case 'b': c = '\b'; break;
        case 'f': c = '\f'; break;
        case 'n': c = '\n'; break;
        case 'r': c = '\r'; break;
        case 't': c = '\t'; break;
        case 'u': {
            unsigned long code = 0;
            int k;
            for (k = 0; k < 4; k++) {
                int d = r->at < r->end ? hex_digit(*r->at++) : -1;
                if (d < 0) {
                    return false;
                }
                code = code * 16 + (unsigned long)d;
            }
            if (code >= 0xD800 && code <= 0xDFFF) {
                return false;
            }
            if (code < 0x80) {
                c = (unsigned char)code;
                break;
            }
            if (code < 0x800) {
                if (!put_byte(out, room, length,
                              (unsigned char)(0xC0 | (code >> 6)))) {
                    return false;
                }
            } else if (!put_byte(out, room, length,
                                 (unsigned char)(0xE0 | (code >> 12))) ||
                       !put_byte(out, room, length, (unsigned char)(
                           0x80 | ((code >> 6) & 0x3F)))) {
                return false;
            }
            c = (unsigned char)(0x80 | (code & 0x3F));
            break;
        }
        default:
            return false;
        }
        if (!put_byte(out, room, length, c)) {
            return false;
        }
    }
    return r->at < r->end && *r->at++ == '"';
}

/* Copy a JSON number into out as a C string. */
static bool read_number(struct reader *r, char *out, size_t size)
{
    size_t n = 0;

    skip_space(r);
    while (r->at < r->end &&
           ((*r->at >= '0' && *r->at <= '9') || *r->at == '-' ||
            *r->at == '+' || *r->at == '.' || *r->at == 'e' ||
            *r->at == 'E')) {
        if (n + 1 >= size) {
            return false;
        }
        out[n++] = (char)*r->at++;
    }
    out[n] = '\0';
    return n > 0;
}

static bool skip_value(struct reader *r, int depth)
{
    char number[64];
    size_t length;

    if (depth > DEPTH_LIMIT) {
        return false;
    }
    skip_space(r);
    if (r->at >= r->end) {
        return false;
    }
    switch (*r->at) {
    case '"':
        return read_string(r, NULL, 0, &length);
    case '{':
        r->at++;
        if (take(r, '}')) {
            return true;
        }
        do {
            if (!read_string(r, NULL, 0, &length) || !take(r, ':') ||
                !skip_value(r, depth + 1)) {
                return false;
            }
        } while (take(r, ','));
        return take(r, '}');
    case '[':
        r->at++;
        if (take(r, ']')) {
            return true;
        }
        do {
            if (!skip_value(r, depth + 1)) {
                return false;
            }
        } while (take(r, ','));
        return take(r, ']');
    default:
        return take_word(r, "true") || take_word(r, "false") ||
               take_word(r, "null") || read_number(r, number, sizeof number);
    }
}

/* The value of the member "type" of the object that r stands before,
   read without moving r. */
static bool type_of(struct reader r, unsigned char *out, size_t *length)
{
    unsigned char name[NAME_ROOM];
    size_t name_length;

    if (!take(&r, '{') || take(&r, '}')) {
        return false;
    }
    do {
        if (!read_string(&r, name, NAME_ROOM, &name_length) || !take(&r, ':')) {
            return false;
        }
        if (name_length == 4 && memcmp(name, "type", 4) == 0) {
            return read_string(&r, out, NAME_ROOM, length);
        }
        if (!skip_value(&r, 1)) {
            return false;
        }
    } while (take(&r, ','));
    return false;
}

/* The field of the chain of d with the name, or NULL. */
static const struct anti_field *field_named(const struct anti_descriptor *d,
                                            const unsigned char *name,
                                            size_t length)
{
    int64_t i;

    for (; d != NULL; d = d->parent) {
        for (i = 0; i < d->field_count; i++) {
            if (same_bytes(d->fields[i].name, d->fields[i].name_length, name,
                           (int64_t)length)) {
                return &d->fields[i];
            }
        }
    }
    return NULL;
}

/* Whether a class is the class of expected or a class below it. */
static bool descends(const struct anti_descriptor *d,
                     const struct anti_descriptor *expected)
{
    return expected == NULL ||
           (d->depth >= expected->depth && d->ancestors != NULL &&
            d->ancestors[expected->depth] == expected);
}

static void *read_object(struct reader *r,
                         const struct anti_descriptor *expected, int depth);

static bool fill(struct reader *r, void *object,
                 const struct anti_descriptor *d, bool typed, int depth);
static bool read_value(struct reader *r, void *bytes, int64_t type,
                       const struct anti_descriptor *d, int64_t owned,
                       int depth);

/* An integer of the type id, refused when the type cannot hold it. */
static bool read_integer(struct reader *r, void *bytes, int64_t type)
{
    char number[64];
    char *end;
    unsigned bits = (unsigned)(anti_rt_type_size(type) * 8);

    if (!read_number(r, number, sizeof number)) {
        return false;
    }
    errno = 0;
    if (anti_rt_type_signed(type)) {
        long long high = bits >= 64 ? LLONG_MAX : (1LL << (bits - 1)) - 1;
        long long value = strtoll(number, &end, 10);
        if (*end != '\0' || errno != 0 || value > high || value < -high - 1) {
            return false;
        }
        anti_rt_store_integer(bytes, type, (uint64_t)value);
        return true;
    }
    {
        unsigned long long high =
            bits >= 64 ? ULLONG_MAX : (1ULL << bits) - 1;
        unsigned long long value = strtoull(number, &end, 10);
        if (number[0] == '-' || *end != '\0' || errno != 0 || value > high) {
            return false;
        }
        anti_rt_store_integer(bytes, type, value);
        return true;
    }
}

/* DESIGN: a str that deserialize reads gets bytes of its own on the
   heap, which nothing frees. A str never owns its bytes, as the rule of
   `own` says, so no `destruct` could free them. They come from libc
   until `anti.mem` exists. deserialize then takes an Allocator, which
   gives every string and every owned object it makes. The caller frees
   that memory at once when the object's life ends. */
static bool read_text(struct reader *r, struct anti_text *out)
{
    struct reader scan = *r;
    unsigned char *bytes;
    size_t length;

    if (!read_string(&scan, NULL, 0, &length)) {
        return false;
    }
    bytes = malloc(length > 0 ? length : 1);
    if (bytes == NULL || !read_string(r, bytes, length, &length)) {
        free(bytes);
        return false;
    }
    out->ptr = bytes;
    out->len = (int64_t)length;
    return true;
}

/* A string of one character, as the scalar value of a char. */
static bool read_char(struct reader *r, void *bytes)
{
    unsigned char text[8];
    size_t length;
    size_t used;
    uint32_t c;

    if (!read_string(r, text, sizeof text, &length)) {
        return false;
    }
    c = anti_utf8_decode(text, length, &used);
    if (used == 0 || used != length) {
        return false;
    }
    memcpy(bytes, &c, sizeof c);
    return true;
}

/* The address and the length of a slice that the object does not own. */
static bool read_view(struct reader *r, struct anti_text *out)
{
    unsigned char name[NAME_ROOM];
    char number[64];
    char *end;
    size_t length;
    int64_t found = 0;

    if (!take(r, '{')) {
        return false;
    }
    do {
        unsigned long long value;
        if (!read_string(r, name, NAME_ROOM, &length) || !take(r, ':') ||
            !read_number(r, number, sizeof number) || number[0] == '-') {
            return false;
        }
        value = strtoull(number, &end, 10);
        if (*end != '\0') {
            return false;
        }
        if (length == 7 && memcmp(name, "address", 7) == 0) {
            out->ptr = (const unsigned char *)(uintptr_t)value;
            found |= 1;
        } else if (length == 6 && memcmp(name, "length", 6) == 0) {
            out->len = (int64_t)value;
            found |= 2;
        } else {
            return false;
        }
    } while (take(r, ','));
    return found == 3 && take(r, '}');
}

/* The elements of a slice that the object owns, in new memory. */
static bool read_elements(struct reader *r, struct anti_text *out,
                          int64_t type, const struct anti_descriptor *d,
                          int depth)
{
    size_t size = anti_rt_element_size(type, d);
    struct reader scan;
    unsigned char *items;
    int64_t count = 0;
    int64_t i;

    if (take_word(r, "null")) {
        out->ptr = NULL;
        out->len = 0;
        return true;
    }
    if (!take(r, '[')) {
        return false;
    }
    scan = *r;
    if (!take(&scan, ']')) {
        do {
            if (!skip_value(&scan, depth + 1)) {
                return false;
            }
            count++;
        } while (take(&scan, ','));
    }
    items = calloc(count > 0 ? (size_t)count : 1, size);
    if (items == NULL) {
        return false;
    }
    for (i = 0; i < count; i++) {
        if ((i > 0 && !take(r, ',')) ||
            !read_value(r, items + (size_t)i * size, ANTI_TYPE_ELEMENT(type),
                        d, 0, depth + 1)) {
            free(items);
            return false;
        }
    }
    if (!take(r, ']')) {
        free(items);
        return false;
    }
    out->ptr = count > 0 ? items : NULL;
    out->len = count;
    if (count == 0) {
        free(items);
    }
    return true;
}

/* What an `own` pointer points at, in new memory. An object of a class
   names its class, which is the field's class or one below it. */
static bool read_owned(struct reader *r, void **out, int64_t type,
                       const struct anti_descriptor *d, int depth)
{
    size_t size = anti_rt_element_size(type, d);
    void *value;

    if (ANTI_TYPE_ELEMENT(type) == ANTI_TYPE_CLASS) {
        if (d == NULL) {
            return false;
        }
        value = read_object(r, d, depth + 1);
    } else {
        value = calloc(1, size);
        if (value != NULL &&
            !read_value(r, value, ANTI_TYPE_ELEMENT(type), d, 0, depth + 1)) {
            free(value);
            value = NULL;
        }
    }
    *out = value;
    return value != NULL;
}

/* Read one value of the type id into bytes, as serialize writes it. A
   value the type cannot hold fails the whole text. d and owned are the
   descriptor and the `own` bit of the field, as in put_value of
   object.c. */
static bool read_value(struct reader *r, void *bytes, int64_t type,
                       const struct anti_descriptor *d, int64_t owned,
                       int depth)
{
    int64_t t = anti_rt_type_scalar(type);
    char number[64];
    char *end;

    if (depth > DEPTH_LIMIT) {
        return false;
    }
    skip_space(r);
    switch (t) {
    case ANTI_TYPE_BOOL: {
        unsigned char value = 0;
        if (take_word(r, "true")) {
            value = 1;
        } else if (!take_word(r, "false")) {
            return false;
        }
        memcpy(bytes, &value, sizeof value);
        return true;
    }
    case ANTI_TYPE_CHAR:
        return read_char(r, bytes);
    case ANTI_TYPE_F32:
    case ANTI_TYPE_F64: {
        double value;
        if (!read_number(r, number, sizeof number)) {
            return false;
        }
        value = strtod(number, &end);
        if (*end != '\0') {
            return false;
        }
        if (t == ANTI_TYPE_F32) {
            float narrow = (float)value;
            memcpy(bytes, &narrow, sizeof narrow);
        } else {
            memcpy(bytes, &value, sizeof value);
        }
        return true;
    }
    case ANTI_TYPE_STR: {
        struct anti_text text;
        if (!read_text(r, &text)) {
            return false;
        }
        memcpy(bytes, &text, sizeof text);
        return true;
    }
    case ANTI_TYPE_PTR:
    case ANTI_TYPE_FN: {
        void *value = NULL;
        if (take_word(r, "null")) {
            value = NULL;
        } else if (owned && t == ANTI_TYPE_PTR &&
                   (ANTI_TYPE_ELEMENT(type) == ANTI_TYPE_CLASS ||
                    anti_rt_element_walked(type, d))) {
            if (!read_owned(r, &value, type, d, depth)) {
                return false;
            }
        } else {
            unsigned long long address;
            if (!read_number(r, number, sizeof number) || number[0] == '-') {
                return false;
            }
            address = strtoull(number, &end, 10);
            if (*end != '\0') {
                return false;
            }
            value = (void *)(uintptr_t)address;
        }
        memcpy(bytes, &value, sizeof value);
        return true;
    }
    case ANTI_TYPE_SLICE: {
        struct anti_text slice;
        if (owned && !anti_rt_element_walked(type, d)) {
            /* The serializer wrote null, and the field keeps its
               default. */
            return skip_value(r, depth + 1);
        }
        if (!(owned ? read_elements(r, &slice, type, d, depth)
                    : read_view(r, &slice))) {
            return false;
        }
        memcpy(bytes, &slice, sizeof slice);
        return true;
    }
    case ANTI_TYPE_STRUCT:
    case ANTI_TYPE_CLASS:
        if (d != NULL && r->at < r->end && *r->at == '{') {
            return fill(r, bytes, d, t == ANTI_TYPE_CLASS, depth + 1);
        }
        /* A struct without a descriptor was written as null, and keeps
           its default. */
        return skip_value(r, depth + 1);
    default:
        if (anti_rt_type_size(t) == 0) {
            return skip_value(r, depth + 1);
        }
        return read_integer(r, bytes, t);
    }
}

/* Read the members of the object that r stands before into the object
   of the class or the struct d. The member "type" of a class was read
   before. A member that names no field of the chain is skipped. */
static bool fill(struct reader *r, void *object,
                 const struct anti_descriptor *d, bool typed, int depth)
{
    unsigned char name[NAME_ROOM];
    size_t length;

    if (depth > DEPTH_LIMIT || !take(r, '{')) {
        return false;
    }
    if (take(r, '}')) {
        return true;
    }
    do {
        const struct anti_field *f;
        if (!read_string(r, name, NAME_ROOM, &length) || !take(r, ':')) {
            return false;
        }
        f = typed && length == 4 && memcmp(name, "type", 4) == 0
                ? NULL
                : field_named(d, name, length);
        if (f != NULL ? !read_value(r, (unsigned char *)object + f->offset,
                                    f->type, f->descriptor, f->owned, depth)
                      : !skip_value(r, depth + 1)) {
            return false;
        }
    } while (take(r, ','));
    return take(r, '}');
}

/* DESIGN: an object comes back as a new object of the class that its
   "type" member names. It is prepared as a literal of the class would be
   and then filled from the other members. A construct with arguments
   does not run, because the fields come from the text. A failure deletes
   what was built. */
static void *read_object(struct reader *r,
                         const struct anti_descriptor *expected, int depth)
{
    unsigned char name[NAME_ROOM];
    size_t length;
    const struct anti_class *c;
    void *object;

    if (!type_of(*r, name, &length)) {
        return NULL;
    }
    c = anti_rt_registry_find(name, (int64_t)length);
    if (c == NULL || !descends(c->descriptor, expected) ||
        (c->flags & ANTI_CLASS_REQUIRED) != 0) {
        return NULL;
    }
    object = build(c);
    if (object != NULL && !fill(r, object, c->descriptor, true, depth)) {
        anti_rt_delete(object, c->descriptor);
        return NULL;
    }
    return object;
}

void *anti_rt_Object_deserialize(struct anti_text input)
{
    struct reader r;
    void *object;

    r.at = input.ptr;
    r.end = input.ptr + (input.len > 0 ? input.len : 0);
    object = read_object(&r, NULL, 0);
    skip_space(&r);
    if (object != NULL && r.at != r.end) {
        anti_rt_delete(object, NULL);
        return NULL;
    }
    return object;
}

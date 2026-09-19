/* reflect.new and Object.deserialize, which read the registry of the
   classes of a program. */
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "registry.h"

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

    /* A construct with arguments has none to take here. */
    if (c == NULL || (c->flags & ANTI_CLASS_ARGS) != 0) {
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

/* Append one byte to out, or fail when out has no room. A NULL out keeps
   nothing and never fails. */
static bool put_byte(unsigned char *out, size_t *length, unsigned char b)
{
    if (out == NULL) {
        return true;
    }
    if (*length + 1 >= NAME_ROOM) {
        return false;
    }
    out[(*length)++] = b;
    return true;
}

/* A JSON string, decoded into out when out is not NULL. A \u escape of a
   surrogate half is refused, since no name the serializer writes holds
   one. */
static bool read_string(struct reader *r, unsigned char *out, size_t *length)
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
            if (!put_byte(out, length, c)) {
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
                if (!put_byte(out, length,
                              (unsigned char)(0xC0 | (code >> 6)))) {
                    return false;
                }
            } else if (!put_byte(out, length,
                                 (unsigned char)(0xE0 | (code >> 12))) ||
                       !put_byte(out, length, (unsigned char)(
                           0x80 | ((code >> 6) & 0x3F)))) {
                return false;
            }
            c = (unsigned char)(0x80 | (code & 0x3F));
            break;
        }
        default:
            return false;
        }
        if (!put_byte(out, length, c)) {
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
        return read_string(r, NULL, &length);
    case '{':
        r->at++;
        if (take(r, '}')) {
            return true;
        }
        do {
            if (!read_string(r, NULL, &length) || !take(r, ':') ||
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
        if (!read_string(&r, name, &name_length) || !take(&r, ':')) {
            return false;
        }
        if (name_length == 4 && memcmp(name, "type", 4) == 0) {
            return read_string(&r, out, length);
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

/* An integer of the width of kind, written at bytes. */
static void store_integer(void *bytes, int64_t kind, long long value)
{
    switch (kind) {
    case ANTI_I8: {
        int8_t v = (int8_t)value;
        memcpy(bytes, &v, sizeof v);
        return;
    }
    case ANTI_I16: {
        int16_t v = (int16_t)value;
        memcpy(bytes, &v, sizeof v);
        return;
    }
    case ANTI_I32: {
        int32_t v = (int32_t)value;
        memcpy(bytes, &v, sizeof v);
        return;
    }
    case ANTI_CWCHAR: {
        int v = (int)value;
        memcpy(bytes, &v, sizeof v);
        return;
    }
    case ANTI_CLONG: {
        long v = (long)value;
        memcpy(bytes, &v, sizeof v);
        return;
    }
    default: {
        int64_t v = (int64_t)value;
        memcpy(bytes, &v, sizeof v);
        return;
    }
    }
}

static bool fill(struct reader *r, void *object,
                 const struct anti_descriptor *d, int depth);

/* Read the value of field f of the object. A value the field cannot hold
   fails the whole text. */
static bool read_field(struct reader *r, void *object,
                       const struct anti_field *f, int depth)
{
    unsigned char *bytes = (unsigned char *)object + f->offset;
    char number[64];
    char *end;

    switch (f->kind) {
    case ANTI_F32:
    case ANTI_F64: {
        double value;
        if (!read_number(r, number, sizeof number)) {
            return false;
        }
        value = strtod(number, &end);
        if (*end != '\0') {
            return false;
        }
        if (f->kind == ANTI_F32) {
            float narrow = (float)value;
            memcpy(bytes, &narrow, sizeof narrow);
        } else {
            memcpy(bytes, &value, sizeof value);
        }
        return true;
    }
    case ANTI_PTR: {
        void *value = NULL;
        skip_space(r);
        if (take_word(r, "null")) {
            value = NULL;
        } else if (r->at < r->end && *r->at == '{') {
            /* An owned object comes back as a new one. Any other pointer
               was written as an address. */
            if (!f->owned || f->descriptor == NULL) {
                return false;
            }
            value = read_object(r, f->descriptor, depth + 1);
            if (value == NULL) {
                return false;
            }
        } else {
            unsigned long long address;
            if (!read_number(r, number, sizeof number)) {
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
    case ANTI_AGG:
        skip_space(r);
        if (f->descriptor != NULL && r->at < r->end && *r->at == '{') {
            return fill(r, bytes, f->descriptor, depth + 1);
        }
        /* A str, a slice or a struct without a descriptor was written as
           null, and keeps its default. */
        return skip_value(r, depth + 1);
    case ANTI_VOID:
        return skip_value(r, depth + 1);
    default: {
        long long value;
        if (take_word(r, "true")) {
            value = 1;
        } else if (take_word(r, "false")) {
            value = 0;
        } else {
            if (!read_number(r, number, sizeof number)) {
                return false;
            }
            value = strtoll(number, &end, 10);
            if (*end != '\0') {
                return false;
            }
        }
        store_integer(bytes, f->kind, value);
        return true;
    }
    }
}

/* Read the members of the object that r stands before into the object
   of the class d. The member "type" was read before. A member that names
   no field of the chain is skipped. */
static bool fill(struct reader *r, void *object,
                 const struct anti_descriptor *d, int depth)
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
        if (!read_string(r, name, &length) || !take(r, ':')) {
            return false;
        }
        f = length == 4 && memcmp(name, "type", 4) == 0
                ? NULL
                : field_named(d, name, length);
        if (f != NULL ? !read_field(r, object, f, depth)
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
    if (c == NULL || !descends(c->descriptor, expected)) {
        return NULL;
    }
    object = build(c);
    if (object != NULL && !fill(r, object, c->descriptor, depth)) {
        anti_rt_delete(object);
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
        anti_rt_delete(object);
        return NULL;
    }
    return object;
}

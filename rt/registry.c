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

/* DESIGN: anti.mem.Allocator declares alloc and free as its first two
   functions. They take the two entries after the seven of the root and
   the nine hooks, in the table of every allocator and of the sub-object
   of an interface. The runtime calls them there, as a call through a
   `*Allocator` does. std/anti/mem.anti keeps the order.
   std_deserialize_alloc counts every call. */
enum {
    ENTRY_ALLOC = ANTI_ENTRY_HOOK + ANTI_HOOK_COUNT,
    ENTRY_FREE
};

/* DESIGN: an object and a buffer of elements take the alignment that
   malloc gives on every target of the runtime archive. calloc gave them
   that before deserialize took an allocator. A string takes one. */
#define OBJECT_ALIGN 16

/* The blocks that one deserialize took from its allocator, which a
   failure gives back. The list is memory of the runtime and goes before
   deserialize returns. */
struct made {
    void **blocks;
    int64_t count;
    int64_t room;
};

struct reader {
    const unsigned char *at;
    const unsigned char *end;
    struct anti_object *from;   /* the anti.mem.Allocator */
    struct made *made;
};

/* size bytes at align from the allocator of the reader, zeroed and
   remembered for a failure, or NULL. */
static void *lend(struct reader *r, size_t size, int64_t align)
{
    void *(*alloc)(struct anti_object *, int64_t, int64_t) =
        (void *(*)(struct anti_object *, int64_t, int64_t))(
            void *)r->from->table[ENTRY_ALLOC];
    struct made *m = r->made;
    void *p;

    if ((uint64_t)size > (uint64_t)INT64_MAX) {
        return NULL;
    }
    if (m->count == m->room) {
        int64_t room = m->room == 0 ? 16 : m->room * 2;
        void **blocks = realloc(m->blocks, (size_t)room * sizeof *blocks);
        if (blocks == NULL) {
            return NULL;
        }
        m->blocks = blocks;
        m->room = room;
    }
    p = alloc(r->from, (int64_t)size, align);
    if (p == NULL) {
        return NULL;
    }
    memset(p, 0, size);
    m->blocks[m->count++] = p;
    return p;
}

/* Give every block the reader took back to its allocator, the last
   first. */
static void give_back(struct reader *r)
{
    void (*give)(struct anti_object *, void *) =
        (void (*)(struct anti_object *, void *))(
            void *)r->from->table[ENTRY_FREE];
    struct made *m = r->made;

    while (m->count > 0) {
        give(r->from, m->blocks[--m->count]);
    }
}

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

/* The bytes of a JSON number, which stay in the input. A float has any
   number of digits. */
static bool number_span(struct reader *r, const unsigned char **start,
                        int64_t *length)
{
    skip_space(r);
    *start = r->at;
    while (r->at < r->end &&
           ((*r->at >= '0' && *r->at <= '9') || *r->at == '-' ||
            *r->at == '+' || *r->at == '.' || *r->at == 'e' ||
            *r->at == 'E')) {
        r->at++;
    }
    *length = r->at - *start;
    return *length > 0;
}

/* Copy a JSON number into out as a C string. */
static bool read_number(struct reader *r, char *out, size_t size)
{
    const unsigned char *start;
    int64_t length;

    if (!number_span(r, &start, &length) || (size_t)length >= size) {
        return false;
    }
    memcpy(out, start, (size_t)length);
    out[length] = '\0';
    return true;
}

static bool skip_value(struct reader *r, int depth)
{
    const unsigned char *number;
    int64_t digits;
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
               take_word(r, "null") || number_span(r, &number, &digits);
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

/* DESIGN: a str that deserialize reads gets bytes of its own from the
   allocator, with the NUL that every str has after them. A str never owns
   its bytes, as the rule of `own` says, so no `destruct` could free them.
   The caller gives them back through the allocator with the object. */
static bool read_text(struct reader *r, struct anti_text *out)
{
    struct reader scan = *r;
    unsigned char *bytes;
    size_t length;

    if (!read_string(&scan, NULL, 0, &length)) {
        return false;
    }
    bytes = lend(r, length + 1, 1);
    if (bytes == NULL || !read_string(r, bytes, length, &length)) {
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
    unsigned char *items = NULL;
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
    if (count > 0) {
        if (size > 0 && (size_t)count > SIZE_MAX / size) {
            return false;
        }
        items = lend(r, (size_t)count * size, OBJECT_ALIGN);
        if (items == NULL) {
            return false;
        }
    }
    for (i = 0; i < count; i++) {
        if ((i > 0 && !take(r, ',')) ||
            !read_value(r, items + (size_t)i * size, ANTI_TYPE_ELEMENT(type),
                        d, 0, depth + 1)) {
            return false;
        }
    }
    if (!take(r, ']')) {
        return false;
    }
    out->ptr = items;
    out->len = count;
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
        value = lend(r, size, OBJECT_ALIGN);
        if (value != NULL &&
            !read_value(r, value, ANTI_TYPE_ELEMENT(type), d, 0, depth + 1)) {
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
    /* The text rounds once, to the width of the field. */
    case ANTI_TYPE_F16:
    case ANTI_TYPE_F32:
    case ANTI_TYPE_F64: {
        const unsigned char *start;
        int64_t length;
        uint64_t value;
        if (!number_span(r, &start, &length)) {
            return false;
        }
        if (t == ANTI_TYPE_F16) {
            uint16_t half;
            if (!anti_rt_read_float(start, length, 10, 5, &value)) {
                return false;
            }
            half = (uint16_t)value;
            memcpy(bytes, &half, sizeof half);
        } else if (t == ANTI_TYPE_F32) {
            uint32_t single;
            if (!anti_rt_read_float(start, length, 23, 8, &value)) {
                return false;
            }
            single = (uint32_t)value;
            memcpy(bytes, &single, sizeof single);
        } else {
            if (!anti_rt_read_float(start, length, 52, 11, &value)) {
                return false;
            }
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
   "type" member names, in memory of the allocator. It is prepared as a
   literal of the class would be and then filled from the other members.
   A construct with arguments does not run, because the fields come from
   the text. A failure leaves what was built to deserialize, which gives
   every block back. No destruct runs, as none runs when the caller gives
   the memory back. */
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
    object = lend(r, (size_t)c->descriptor->size, OBJECT_ALIGN);
    if (object == NULL) {
        return NULL;
    }
    c->init(object);
    return fill(r, object, c->descriptor, true, depth) ? object : NULL;
}

void *anti_lang_Object_deserialize(struct anti_text input,
                                   struct anti_object *from)
{
    struct made made = {NULL, 0, 0};
    struct reader r;
    void *object;

    if (from == NULL) {
        return NULL;
    }
    r.at = input.ptr;
    r.end = input.ptr + (input.len > 0 ? input.len : 0);
    r.from = from;
    r.made = &made;
    object = read_object(&r, NULL, 0);
    skip_space(&r);
    if (object == NULL || r.at != r.end) {
        give_back(&r);
        object = NULL;
    }
    free(made.blocks);
    return object;
}

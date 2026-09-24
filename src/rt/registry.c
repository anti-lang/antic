/* reflect.new and Object.deserialize, which read the registry of the
   classes of a program. */
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "json.h"
#include "plugin.h"
#include "registry.h"
#include "std.h"
#include "utf.h"

/* The class of one table whose name matches, or NULL. A bare name that
   two classes of the table carry sets two. */
static const struct anti_class *find_in(const struct anti_registry *r,
                                        const unsigned char *name,
                                        int64_t length, int64_t dot,
                                        bool *two)
{
    const struct anti_class *found = NULL;
    int64_t i;

    for (i = 0; i < r->count; i++) {
        const struct anti_class *c = &r->classes[i];
        if (!anti_rt_same_bytes(c->descriptor->name, c->descriptor->name_length,
                        name + dot, length - dot)) {
            continue;
        }
        if (dot > 0) {
            if (anti_rt_same_bytes(c->module, c->module_length, name, dot - 1)) {
                return c;
            }
            continue;
        }
        if (found != NULL) {
            *two = true;
            return NULL;
        }
        found = c;
    }
    return found;
}

/* The class named `Class` or `module.Class`, or NULL when the registry
   holds none or holds two of the bare name.

   DESIGN: a name without a dot is the name the descriptor holds, which
   two modules may both declare. It finds a class when one class alone
   has it. `module.Class` names the class of one module and is never in
   doubt. */
static const struct anti_class *registry_find(const unsigned char *name,
                                             int64_t length)
{
    const struct anti_class *found;
    bool two = false;
    int64_t dot = length;
    int64_t i;

    while (dot > 0 && name[dot - 1] != '.') {
        dot--;
    }
    found = find_in(&anti_rt_registry, name, length, dot, &two);
    if (two || anti_rt_plugin_open() == 0) {
        return two ? NULL : found;
    }
    /* DESIGN: a loaded library brings classes of its own, and
       `reflect.new` finds one by name as it finds a class of the
       program. The loader holds the table of each open library. A lookup
       walks them after the program's own, under the lock of the slots.
       No unload takes a table away during the walk. The class it finds
       stays mapped while the program keeps its library open, as every
       object of the library does. */
    anti_rt_plugin_hold();
    for (i = 0; i < ANTI_PLUGIN_MAX && !two; i++) {
        const struct anti_registry *r = anti_rt_plugin_registry(i);
        const struct anti_class *c;
        if (r == NULL) {
            continue;
        }
        c = find_in(r, name, length, dot, &two);
        if (c == NULL) {
            continue;
        }
        if (dot > 0) {
            found = c;
            break;
        }
        if (found != NULL) {
            two = true;
            break;
        }
        found = c;
    }
    anti_rt_plugin_release();
    return two ? NULL : found;
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
    const struct anti_class *c = registry_find(name, length);

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
#define DEPTH_LIMIT ANTI_JSON_DEPTH

/* DESIGN: an object and a buffer of elements take the alignment that
   malloc gives on every target of the runtime archive. calloc gave them
   that before deserialize took an allocator. A string takes one. */
#define OBJECT_ALIGN 16

/* An object whose construct ran, and the field of the object that owns
   it, or NULL before the reader stores it there. */
struct built {
    void *object;
    void **owner;
};

/* The blocks that one deserialize took from its allocator, which a
   failure gives back, and the objects it prepared, which a failure
   destroys first. The lists are memory of the runtime and go before
   deserialize returns. */
struct made {
    void **blocks;
    int64_t count;
    int64_t room;
    struct built *objects;
    int64_t object_count;
    int64_t object_room;
};

struct reader {
    struct anti_json scan;
    struct anti_object *from;   /* the anti.mem.Allocator */
    struct made *made;
};

/* size bytes at align from the allocator of the reader, zeroed and
   remembered for a failure, or NULL. */
static void *lend(struct reader *r, size_t size, int64_t align)
{
    void *(*alloc)(struct anti_object *, int64_t, int64_t) =
        (void *(*)(struct anti_object *, int64_t, int64_t))
            anti_rt_entry_body(r->from, ANTI_ENTRY_ALLOC);
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

/* Room for one more prepared object, taken before its construct runs,
   so that every object whose construct ran is on the list. */
static bool room_for_object(struct made *m)
{
    if (m->object_count == m->object_room) {
        int64_t room = m->object_room == 0 ? 8 : m->object_room * 2;
        struct built *objects =
            realloc(m->objects, (size_t)room * sizeof *objects);
        if (objects == NULL) {
            return false;
        }
        m->objects = objects;
        m->object_room = room;
    }
    return true;
}

/* Record that owner, a field of another object, holds object. */
static void owned_by(struct made *m, void *object, void **owner)
{
    int64_t i;

    for (i = m->object_count - 1; i >= 0; i--) {
        if (m->objects[i].object == object) {
            m->objects[i].owner = owner;
            return;
        }
    }
}

/* DESIGN: a text that fails undoes what the reader built. Every object
   whose construct ran is destroyed, the newest first, and only then does
   every block go back to the allocator. Each owning field is cleared
   first. The teardown of an object then reaches none of the objects the
   reader made for it, which have their own place on the list. The
   teardowns give nothing back, since the blocks go back afterwards. */
static void give_back(struct reader *r)
{
    void (*give)(struct anti_object *, void *) =
        (void (*)(struct anti_object *, void *))anti_rt_entry_body(
            r->from, ANTI_ENTRY_FREE);
    struct made *m = r->made;
    int64_t i;

    for (i = m->object_count - 1; i >= 0; i--) {
        if (m->objects[i].owner != NULL) {
            *m->objects[i].owner = NULL;
        }
    }
    while (m->object_count > 0) {
        anti_rt_destroy_from(m->objects[--m->object_count].object, NULL,
                             &anti_rt_give_nothing);
    }
    while (m->count > 0) {
        give(r->from, m->blocks[--m->count]);
    }
}

/* The scanner of src/rt/json.c, over the input of the reader. */

static void skip_space(struct reader *r)
{
    anti_rt_json_space(&r->scan);
}

static bool take(struct reader *r, unsigned char c)
{
    return anti_rt_json_take(&r->scan, c);
}

static bool take_word(struct reader *r, const char *word)
{
    return anti_rt_json_word(&r->scan, word);
}

static bool read_string(struct reader *r, unsigned char *out, size_t room,
                        size_t *length)
{
    return anti_rt_json_string(&r->scan, out, room, length);
}

static bool number_span(struct reader *r, const unsigned char **start,
                        int64_t *length)
{
    return anti_rt_json_number(&r->scan, start, length);
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
    return anti_rt_json_skip(&r->scan, depth);
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
                                            size_t length, int64_t *index)
{
    int64_t i;

    *index = 0;
    for (; d != NULL; d = d->parent) {
        for (i = 0; i < d->field_count; i++) {
            if (anti_rt_same_bytes(d->fields[i].name, d->fields[i].name_length, name,
                           (int64_t)length)) {
                *index += i;
                return &d->fields[i];
            }
        }
        *index += d->field_count;
    }
    return NULL;
}

/* The fields of the chain of d, which field_named numbers. */
static int64_t fields_of(const struct anti_descriptor *d)
{
    int64_t count = 0;

    for (; d != NULL; d = d->parent) {
        count += d->field_count;
    }
    return count;
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
    c = anti_rt_utf8_decode(text, length, &used);
    if (used == 0 || used != length) {
        return false;
    }
    memcpy(bytes, &c, sizeof c);
    return true;
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
    /* DESIGN: the text never gives an address. A pointer or a function
       pointer that the object does not own was written as null, and null
       is all the reader takes there. An address from text means nothing
       in another run. From untrusted text it would let the input choose
       what memory the program reaches. */
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
            owned_by(r->made, value, (void **)bytes);
        } else {
            return false;
        }
        memcpy(bytes, &value, sizeof value);
        return true;
    }
    case ANTI_TYPE_SLICE: {
        struct anti_text slice = {NULL, 0};
        if (owned && !anti_rt_element_walked(type, d)) {
            /* The serializer wrote null, and the field keeps its
               default. */
            return skip_value(r, depth + 1);
        }
        /* A slice the object does not own is null, as a pointer is. */
        if (owned ? !read_elements(r, &slice, type, d, depth)
                  : !take_word(r, "null")) {
            return false;
        }
        memcpy(bytes, &slice, sizeof slice);
        return true;
    }
    case ANTI_TYPE_STRUCT:
    case ANTI_TYPE_CLASS:
        if (d != NULL && r->scan.at < r->scan.end && *r->scan.at == '{') {
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
   before. A member that names no field of the chain is skipped.

   DESIGN: a member that names a field read before fails the text. The
   first value would otherwise stay behind, an object whose construct
   ran or a string, which no field holds and nothing gives back. seen
   holds one bit per field of the chain. */
static bool fill(struct reader *r, void *object,
                 const struct anti_descriptor *d, bool typed, int depth)
{
    unsigned char name[NAME_ROOM];
    unsigned char *seen = NULL;
    int64_t count = fields_of(d);
    bool ok = false;
    size_t length;

    if (depth > DEPTH_LIMIT || !take(r, '{')) {
        return false;
    }
    if (take(r, '}')) {
        return true;
    }
    if (count > 0) {
        seen = calloc((size_t)(count + 7) / 8, 1);
        if (seen == NULL) {
            return false;
        }
    }
    do {
        const struct anti_field *f;
        int64_t index = 0;
        unsigned char bit;
        if (!read_string(r, name, NAME_ROOM, &length) || !take(r, ':')) {
            goto done;
        }
        f = typed && length == 4 && memcmp(name, "type", 4) == 0
                ? NULL
                : field_named(d, name, length, &index);
        if (f == NULL) {
            if (!skip_value(r, depth + 1)) {
                goto done;
            }
            continue;
        }
        bit = (unsigned char)(1u << (index % 8));
        if ((seen[index / 8] & bit) != 0) {
            goto done;
        }
        seen[index / 8] = (unsigned char)(seen[index / 8] | bit);
        if (!read_value(r, (unsigned char *)object + f->offset, f->type,
                        f->descriptor, f->owned, depth)) {
            goto done;
        }
    } while (take(r, ','));
    ok = take(r, '}');
done:
    free(seen);
    return ok;
}

/* DESIGN: an object comes back as a new object of the class that its
   "type" member names, in memory of the allocator. It is prepared as a
   literal of the class would be and then filled from the other members.
   A construct with arguments does not run, because the fields come from
   the text. A failure leaves what was built to deserialize, which
   destroys it and gives every block back. On success the caller runs the
   destructs with destroy(p, from), which gives the owned memory back to
   the same allocator. */
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
    c = registry_find(name, (int64_t)length);
    if (c == NULL || !descends(c->descriptor, expected) ||
        (c->flags & ANTI_CLASS_REQUIRED) != 0) {
        return NULL;
    }
    if (!room_for_object(r->made)) {
        return NULL;
    }
    object = lend(r, (size_t)c->descriptor->size, OBJECT_ALIGN);
    if (object == NULL) {
        return NULL;
    }
    c->init(object);
    r->made->objects[r->made->object_count].object = object;
    r->made->objects[r->made->object_count].owner = NULL;
    r->made->object_count++;
    return fill(r, object, c->descriptor, true, depth) ? object : NULL;
}

void *anti_lang_Object_deserialize(struct anti_text input,
                                   struct anti_object *from)
{
    struct made made = {NULL, 0, 0, NULL, 0, 0};
    struct reader r;
    void *object;

    if (from == NULL) {
        return NULL;
    }
    r.scan.at = input.ptr;
    r.scan.end = input.ptr + (input.len > 0 ? input.len : 0);
    r.from = from;
    r.made = &made;
    object = read_object(&r, NULL, 0);
    skip_space(&r);
    if (object == NULL || r.scan.at != r.scan.end) {
        give_back(&r);
        object = NULL;
    }
    free(made.blocks);
    free(made.objects);
    return object;
}

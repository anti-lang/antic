/* Loading a shared library that provides interfaces.

   DESIGN: a plugin is bound against the host at load. The runtime, the
   heap, the worker pool, the descriptors and the standard library it
   names are the host's. An object of a loaded class is then an object
   of the program like any other. The library carries one table,
   `anti_rt_provides`, which names what it offers. The loader reads that
   table and opens nothing else. It checks every count, length, pointer
   and offset of the table before it uses one. A damaged or mismatched
   library would otherwise send it outside the table. */
#include "plugin.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atomic.h"
#include "digest.h"
#include "platform.h"
#include "std.h"
#include "toml.h"


enum { ANTI_PLUGIN_PATH = 4096 };

/* DESIGN: the reason of the last failure is kept per thread, because
   any thread may load, and each reads the reason of its own call. */
static _Thread_local char message[512];

/* The text snprintf wrote into out, of size bytes, when it gave
   written. A text cut short ends in `...`, and a failure leaves one that
   says so. */
static const char *fitted(char *out, size_t size, int written)
{
    static const char failed[] = "a message that cannot be written";
    size_t kept = sizeof failed - 1;

    if (written < 0 && size > 0) {
        kept = kept < size - 1 ? kept : size - 1;
        memcpy(out, failed, kept);
        out[kept] = '\0';
    } else if (written >= 0 && (size_t)written >= size && size > 3) {
        memcpy(out + size - 4, "...", 4);
    }
    return out;
}

/* Keep the reason the last call gave, which a caller may print. */
static void fail(const char *format, ...)
{
    va_list args;
    int written;

    va_start(args, format);
    written = vsnprintf(message, sizeof message, format, args);
    va_end(args);
    fitted(message, sizeof message, written);
}

struct anti_text anti_rt_plugin_message(void)
{
    struct anti_text text;

    text.ptr = (const unsigned char *)message;
    text.len = (int64_t)strlen(message);
    return text;
}

/* Copy a str into a NUL-terminated path, or give 0 for one too long. */
static int path_of(char *out, size_t size, const unsigned char *path,
                   int64_t length)
{
    if (length < 0 || (size_t)length + 1 > size) {
        return 0;
    }
    memcpy(out, path, (size_t)length);
    out[length] = '\0';
    return 1;
}

/* The slot of the handle, or NULL. The caller holds the lock of the
   slots. */
static struct anti_plugin *slot_of(void *handle)
{
    int64_t i;

    for (i = 0; i < ANTI_PLUGIN_MAX; i++) {
        struct anti_plugin *p = anti_rt_plugin_at(i);
        if (p == handle && p->used != 0) {
            return p;
        }
    }
    fail("the library is not open");
    return NULL;
}

/* The entry of the library for the interface, by its descriptor or by
   its path. NULL when the library provides none. */
static const struct anti_provides *entry_of(const struct anti_plugin *p,
                                            const struct anti_descriptor *d,
                                            const unsigned char *path,
                                            int64_t length)
{
    int64_t i;

    for (i = 0; i < p->table->count; i++) {
        const struct anti_provides *e = &p->table->entries[i];
        if (d != NULL ? e->descriptor == d
                      : anti_rt_same_bytes(e->path, e->path_length, path,
                                   length)) {
            return e;
        }
    }
    return NULL;
}

/* The version checks of "Versions" in docs/anti-language-additions.md.
   Each runs per provided interface, at load, and compiles nothing. */

/* The digits of the next dotted part of a version without its leading
   zeros, and the rest after it. A part that is no number counts as zero,
   and a missing part as zero as well, which is no digits.

   DESIGN: a part is compared by its digits rather than read into an
   integer, so a part of any length compares exactly and never overflows.
   The table of a library is not trusted to hold short parts. */
static void version_part(const unsigned char **at, int64_t *left,
                         const unsigned char **digits, int64_t *count)
{
    while (*left > 0 && **at == '0') {
        (*at)++;
        (*left)--;
    }
    *digits = *at;
    *count = 0;
    while (*left > 0 && **at >= '0' && **at <= '9') {
        (*at)++;
        (*left)--;
        (*count)++;
    }
    while (*left > 0 && **at != '.') {
        (*at)++;
        (*left)--;
    }
    if (*left > 0) {
        (*at)++;
        (*left)--;
    }
}

/* Whether version a is below version b, part by part. */
static int version_below(const unsigned char *a, int64_t a_length,
                         const unsigned char *b, int64_t b_length)
{
    int i;

    for (i = 0; i < 8; i++) {
        const unsigned char *one;
        const unsigned char *two;
        int64_t one_count;
        int64_t two_count;
        int order;
        version_part(&a, &a_length, &one, &one_count);
        version_part(&b, &b_length, &two, &two_count);
        if (one_count != two_count) {
            return one_count < two_count;
        }
        order = one_count > 0 ? memcmp(one, two, (size_t)one_count) : 0;
        if (order != 0) {
            return order < 0;
        }
        if (a_length <= 0 && b_length <= 0) {
            break;
        }
    }
    return 0;
}

const struct anti_slots *anti_rt_plugin_slots(const struct anti_descriptor *d)
{
    int64_t i;

    for (i = 0; i < anti_rt_slots.count; i++) {
        if (anti_rt_slots.interfaces[i].descriptor == d) {
            return &anti_rt_slots.interfaces[i];
        }
    }
    return NULL;
}

/* The name of the function at the slot of d, or `slot N`. */
static const char *function_at(const struct anti_descriptor *d, int64_t slot,
                               char *out, size_t size)
{
    int64_t i;

    for (i = 0; i < d->function_count; i++) {
        if (d->functions[i].slot == slot) {
            return fitted(out, size,
                          snprintf(out, size, "%.*s",
                                   (int)d->functions[i].name_length,
                                   d->functions[i].name));
        }
    }
    return fitted(out, size,
                  snprintf(out, size, "slot %lld", (long long)slot));
}

/* DESIGN: the three checks of one provided interface. The plugin's hash
   stands in the program's chain. No field was added between the two
   versions. Every slot the program reaches by a call is in the plugin's
   table. A newer plugin in an older program passes by the same rules,
   since the two chains agree wherever both of them reach. */
static int checked(const char *name, const struct anti_provides *e)
{
    const struct anti_descriptor *d = e->descriptor;
    const struct anti_versions *v = d != NULL ? d->versions : NULL;
    const struct anti_slots *reached;
    int64_t entries = e->chain_length - 1;
    int64_t at;

    if (v == NULL || v->chain_length < 1 || e->chain_length < 1) {
        fail("%s provides `%.*s`, and this program carries no version of "
             "it", name, (int)e->path_length, e->path);
        return 0;
    }
    at = v->chain_length < e->chain_length ? v->chain_length : e->chain_length;
    if (v->chain[at - 1] != e->chain[at - 1]) {
        fail("`%.*s` of %s is not the interface this program carries",
             (int)e->path_length, e->path, name);
        return 0;
    }
    if (v->floor != NULL &&
        version_below(e->built, e->built_length, v->floor, v->floor_length)) {
        fail("%s was built for `%.*s` %.*s, and this program takes %.*s and "
             "above", name, (int)e->path_length, e->path,
             (int)e->built_length, e->built, (int)v->floor_length, v->floor);
        return 0;
    }
    if (e->fields != d->field_count || e->size != d->size) {
        fail("a field was added to `%.*s` after %s was built",
             (int)e->path_length, e->path, name);
        return 0;
    }
    reached = anti_rt_plugin_slots(d);
    for (at = entries + 1; reached != NULL && at < reached->slot_count;
         at++) {
        char found[128];
        if ((reached->bits[at / 8] & (1u << (at % 8))) == 0) {
            continue;
        }
        fail("this program calls `%s` of `%.*s`, and %s carries no such "
             "function", function_at(d, at, found, sizeof found),
             (int)e->path_length, e->path, name);
        return 0;
    }
    return 1;
}

/* DESIGN: a slot that `reflect.call` alone may reach and the library
   does not carry is filled with a stub. The loader writes a table of
   its own for the sub-object, with the library's entries and the stub
   in the rest. The stub reads that table back through the object it was
   called with, so one function serves every load. */
struct anti_stubbed {
    uint64_t magic;
    const struct anti_provides *entry;
    int64_t filled;             /* the library's entries are copied in */
};

#define ANTI_STUBBED_MAGIC 0x616e746973747562ULL

static void stub(struct anti_object *self)
{
    const struct anti_stubbed *head =
        self != NULL && self->table != NULL
            ? (const struct anti_stubbed *)(const void *)self->table - 1
            : NULL;

    if (head != NULL && head->magic == ANTI_STUBBED_MAGIC) {
        const struct anti_descriptor *c = head->entry->class_of;
        anti_rt_fail_abort("anti: `%.*s` %.*s carries no such function of "
                           "`%.*s`", (int)c->name_length, c->name,
                           (int)c->version_length, c->version,
                           (int)head->entry->path_length, head->entry->path);
    }
    anti_rt_fail_abort("anti: a library carries no such function of its "
                       "interface");
}

/* Whether the program may reach a slot through reflection that the
   library does not carry. */
static int needs_stubs(const struct anti_provides *e)
{
    return anti_rt_slots.reflect != 0 &&
           e->descriptor->function_count > e->chain_length - 1;
}

/* The table of the sub-object an object of the entry gets, with a stub
   in every slot the library lacks. NULL when there is no memory for
   it. */
static const void **stubs_of(const struct anti_provides *e)
{
    const struct anti_descriptor *d = e->descriptor;
    int64_t entries = e->chain_length - 1;
    struct anti_stubbed *head;
    const void **table;
    int64_t i;

    head = calloc(1, sizeof *head +
                         (size_t)(d->function_count + 1) * sizeof(void *));
    if (head == NULL) {
        return NULL;
    }
    head->magic = ANTI_STUBBED_MAGIC;
    head->entry = e;
    table = (const void **)(head + 1);
    for (i = entries + 1; i <= d->function_count; i++) {
        table[i] = anti_rt_body_entry((anti_rt_body)stub);
    }
    return table;
}

/* Give back the tables the loader wrote for the count entries. */
static void free_stubs(struct anti_plugin *p, int64_t count)
{
    int64_t i;

    for (i = 0; p->stubbed != NULL && i < count; i++) {
        if (p->stubbed[i] != NULL) {
            free((struct anti_stubbed *)(void *)p->stubbed[i] - 1);
        }
    }
    free(p->stubbed);
    p->stubbed = NULL;
}

/* An object of the class of the entry, as a pointer to its interface
   sub-object. The table of that sub-object is the loader's where the
   library lacks a slot the program may reach through reflection. */
static void *build(const struct anti_plugin *p,
                   const struct anti_provides *e)
{
    const void **table = p->stubbed != NULL
                             ? p->stubbed[e - p->table->entries]
                             : NULL;
    struct anti_object *sub;
    void *object;

    if ((e->flags & (ANTI_CLASS_ARGS | ANTI_CLASS_REQUIRED)) != 0) {
        fail("`%.*s` of the library takes arguments, and the host has none "
             "to give", (int)e->class_of->name_length, e->class_of->name);
        return NULL;
    }
    object = calloc(1, (size_t)e->class_of->size);
    if (object == NULL) {
        fail("out of memory");
        return NULL;
    }
    e->init(object);
    sub = (struct anti_object *)(void *)((unsigned char *)object + e->offset);
    if (table != NULL) {
        struct anti_stubbed *head = (struct anti_stubbed *)(void *)table - 1;
        int64_t i;
        /* Two threads may build the first two objects at once. */
        anti_rt_plugin_hold();
        if (head->filled == 0) {
            for (i = 0; i < e->chain_length; i++) {
                table[i] = (const void *)sub->table[i];
            }
            head->filled = 1;
        }
        anti_rt_plugin_release();
        sub->table = (const struct anti_descriptor *const *)(const void *)
                         table;
    }
    return sub;
}

/* Take a slot for the library that anti_rt_library_open gave. The caller holds
   the lock of the slots. Gives the slot, or NULL with the reason in
   message. *same is 1 when the library is open already, in the slot it
   gives, and the caller then closes the handle it opened. */
static struct anti_plugin *claim(const char *name, void *handle,
                                 const struct anti_provided *table,
                                 const void *base, int *same)
{
    struct anti_plugin *p = NULL;
    int64_t i;

    *same = 0;
    for (i = 0; i < ANTI_PLUGIN_MAX && p == NULL; i++) {
        struct anti_plugin *slot = anti_rt_plugin_at(i);
        if (slot->used == 0) {
            p = slot;
        }
    }
    if (p == NULL) {
        fail("%d libraries are open, which is all a program may hold",
             ANTI_PLUGIN_MAX);
        return NULL;
    }
    /* The platform gives one handle per file, so a second load of the
       same library is the library that is open. */
    for (i = 0; i < ANTI_PLUGIN_MAX; i++) {
        struct anti_plugin *open = anti_rt_plugin_at(i);
        if (open->used != 0 && open->table == table) {
            *same = 1;
            return open;
        }
    }
    for (i = 0; i < table->count; i++) {
        const struct anti_provides *e = &table->entries[i];
        /* The interface is the host's, because the library was bound
           against the host. One that brought its own would give objects
           that no `is` of the program answers for. */
        if (e->descriptor == NULL ||
            anti_rt_plugin_image(e->descriptor) == base) {
            fail("%s carries an `%.*s` of its own, and the host's is the one "
                 "it provides", name, (int)e->path_length, e->path);
            return NULL;
        }
        if (!checked(name, e)) {
            return NULL;
        }
    }
    p->stubbed = NULL;
    for (i = 0; i < table->count; i++) {
        const void **one;
        if (!needs_stubs(&table->entries[i])) {
            continue;
        }
        if (p->stubbed == NULL) {
            p->stubbed = calloc((size_t)table->count, sizeof *p->stubbed);
        }
        one = p->stubbed != NULL ? stubs_of(&table->entries[i]) : NULL;
        if (one == NULL) {
            free_stubs(p, table->count);
            fail("out of memory");
            return NULL;
        }
        p->stubbed[i] = one;
    }
    p->handle = handle;
    p->base = base;
    p->table = table;
    p->classes.count = table->class_count;
    p->classes.classes = table->classes;
    p->live = 0;
    p->used = 1;
    anti_rt_plugin_opened();
    return p;
}

/* Whether a text of the table has a length that a message may print
   and bytes where it has any. */
static int text_sound(const unsigned char *text, int64_t length)
{
    return length >= 0 && length <= INT_MAX && (length == 0 || text != NULL);
}

/* Whether a descriptor of a class lies in an image of the process. Its
   name, its version and its size must be ones the loader may use. */
static int class_sound(const struct anti_descriptor *d)
{
    return d != NULL && anti_rt_plugin_image(d) != NULL &&
           text_sound(d->name, d->name_length) &&
           text_sound(d->version, d->version_length) && d->size > 0;
}

/* What is damaged in the table of a library, or NULL when every value
   the loader reads is sound. */
static const char *damage_of(const struct anti_provided *table)
{
    int64_t i;

    if (!text_sound(table->version, table->version_length)) {
        return "the runtime version";
    }
    if (table->count < 0 || (table->count > 0 && table->entries == NULL) ||
        table->class_count < 0 ||
        (table->class_count > 0 && table->classes == NULL)) {
        return "a count";
    }
    for (i = 0; i < table->count; i++) {
        const struct anti_provides *e = &table->entries[i];
        if (!text_sound(e->path, e->path_length) ||
            !text_sound(e->built, e->built_length)) {
            return "a text of an entry";
        }
        if (e->descriptor == NULL ||
            anti_rt_plugin_image(e->descriptor) == NULL) {
            return "the interface of an entry";
        }
        if (!class_sound(e->class_of) || e->init == NULL) {
            return "the class of an entry";
        }
        /* The loader writes the table pointer of the sub-object. */
        if (e->offset < 0 ||
            e->offset > e->class_of->size -
                            (int64_t)sizeof(struct anti_object)) {
            return "the offset of an entry";
        }
        if (e->chain_length < 1 || e->chain == NULL) {
            return "the version chain of an entry";
        }
    }
    for (i = 0; i < table->class_count; i++) {
        const struct anti_class *c = &table->classes[i];
        if (!class_sound(c->descriptor) || c->init == NULL ||
            !text_sound(c->module, c->module_length)) {
            return "a class";
        }
    }
    return NULL;
}

/* DESIGN: the library is opened and closed outside the lock of the
   slots. A constructor or a destructor of a library may then make an
   object, whose `created` hook takes the lock, without waiting for
   itself. */
void *anti_rt_plugin_load(const unsigned char *path, int64_t length)
{
    struct anti_text version = anti_rt_runtime_version();
    char name[ANTI_PLUGIN_PATH];
    char why[64];
    const struct anti_provided *table;
    const char *damage;
    struct anti_plugin *p = NULL;
    void *handle;
    int same = 0;

    message[0] = '\0';
    if (!path_of(name, sizeof name, path, length)) {
        fail("the path of a library is at most %d bytes",
             ANTI_PLUGIN_PATH - 1);
        return NULL;
    }
    handle = anti_rt_library_open(name);
    if (handle == NULL) {
        fail("%s: %s", name, anti_rt_library_error(why, sizeof why));
        return NULL;
    }
    table = anti_rt_library_symbol(handle, "anti_rt_provides");
    if (table == NULL) {
        fail("%s provides nothing and is no plugin", name);
        goto close;
    }
    damage = damage_of(table);
    if (damage != NULL) {
        fail("%s carries a damaged table of what it provides: %s", name,
             damage);
        goto close;
    }
    if (!anti_rt_same_bytes(table->version, table->version_length, version.ptr,
                    version.len)) {
        fail("%s was built for runtime %.*s, and this program carries %.*s",
             name, (int)table->version_length, table->version,
             (int)version.len, version.ptr);
        goto close;
    }
    anti_rt_plugin_hold();
    p = claim(name, handle, table, anti_rt_plugin_image(table), &same);
    anti_rt_plugin_release();
    if (p != NULL && !same) {
        return p;
    }
    /* A refusal gives NULL. A library open already gives the slot it
       has, and the handle this call opened goes back. */
close:
    anti_rt_library_close(handle);
    return p;
}

void *anti_rt_plugin_instance(void *handle, const struct anti_descriptor *d)
{
    const struct anti_provides *e = NULL;
    struct anti_plugin *p;

    message[0] = '\0';
    anti_rt_plugin_hold();
    p = slot_of(handle);
    if (p != NULL) {
        e = entry_of(p, d, NULL, 0);
    }
    anti_rt_plugin_release();
    if (p == NULL) {
        fail("the library is not open");
        return NULL;
    }
    if (e == NULL) {
        fail("the library provides no `%.*s`", (int)d->name_length, d->name);
        return NULL;
    }
    return build(p, e);
}

int8_t anti_rt_plugin_supports(void *handle, const struct anti_descriptor *d,
                               const unsigned char *name, int64_t length)
{
    const struct anti_provides *e = NULL;
    struct anti_plugin *p;
    int64_t i;

    anti_rt_plugin_hold();
    p = slot_of(handle);
    if (p != NULL) {
        e = entry_of(p, d, NULL, 0);
    }
    anti_rt_plugin_release();
    if (e == NULL) {
        return 0;
    }
    for (i = 0; i < e->class_of->function_count; i++) {
        const struct anti_function *f = &e->class_of->functions[i];
        if (anti_rt_same_bytes(f->name, f->name_length, name, length)) {
            return 1;
        }
    }
    return 0;
}

int64_t anti_rt_plugin_live(void *handle)
{
    struct anti_plugin *p;
    int64_t live = 0;

    anti_rt_plugin_hold();
    p = slot_of(handle);
    if (p != NULL) {
        live = anti_rt_atomic_load(&p->live, (int64_t)sizeof p->live);
    }
    anti_rt_plugin_release();
    return live;
}

/* DESIGN: the check of live and the release of the slot stand under one
   hold of the lock. A `created` hook counts under the same lock. An
   object made while the check runs is then either counted before it, or
   finds no library open. */
int8_t anti_rt_plugin_unload(void *handle)
{
    struct anti_plugin gone;
    struct anti_plugin *p;
    int8_t closed = 0;
    int64_t live;

    message[0] = '\0';
    anti_rt_plugin_hold();
    p = slot_of(handle);
    if (p != NULL) {
        live = anti_rt_atomic_load(&p->live, (int64_t)sizeof p->live);
        if (live != 0) {
            fail("%lld object%s of the library %s alive", (long long)live,
                 live == 1 ? "" : "s", live == 1 ? "is" : "are");
        } else {
            gone = *p;
            memset(p, 0, sizeof *p);
            anti_rt_plugin_closed();
            closed = 1;
        }
    }
    anti_rt_plugin_release();
    if (closed) {
        free_stubs(&gone, gone.table->count);
        anti_rt_library_close(gone.handle);
    }
    return closed;
}

/* Discovery */

/* Join a directory and a file name with the separator of the host. */
static int joined(char *out, size_t size, const char *dir, size_t dir_length,
                  const char *name)
{
    int written = snprintf(out, size, "%.*s%s%s", (int)dir_length, dir,
                           dir_length > 0 ? "/" : "", name);

    return written > 0 && (size_t)written < size;
}

/* Write the digest of the file at path into hex. Gives 0 when the file
   cannot be read. */
static int digest_file(const char *path, char hex[65])
{
    FILE *f = anti_rt_fs_open((const unsigned char *)path,
                              (int64_t)strlen(path), 0);
    int ok;

    if (f == NULL) {
        return 0;
    }
    ok = anti_rt_sha256_stream(f, hex);
    fclose(f);
    return ok;
}

/* The value of `library.<n>.<field>` of the index, or an empty text. */
static struct anti_text index_field(const struct anti_toml *doc, int64_t n,
                                    const char *field)
{
    char key[64];
    int written = snprintf(key, sizeof key, "library.%lld.%s", (long long)n,
                           field);
    int64_t at = -1;

    if (written > 0 && (size_t)written < sizeof key) {
        at = anti_rt_toml_find(doc, (const unsigned char *)key, written);
    }
    if (at < 0) {
        struct anti_text empty;
        empty.ptr = (const unsigned char *)"";
        empty.len = 0;
        return empty;
    }
    return anti_rt_toml_value(doc, at);
}

/* Whether entry n of the index lists the interface. */
static int index_lists(const struct anti_toml *doc, int64_t n,
                       const unsigned char *path, int64_t length)
{
    int64_t k;

    for (k = 0; k < 64; k++) {
        char field[32];
        struct anti_text value;
        int written = snprintf(field, sizeof field, "interfaces.%lld",
                               (long long)k);
        if (written <= 0 || (size_t)written >= sizeof field) {
            return 0;
        }
        value = index_field(doc, n, field);
        if (value.len == 0) {
            return 0;
        }
        if (anti_rt_same_bytes(value.ptr, value.len, path, length)) {
            return 1;
        }
    }
    return 0;
}

/* The library of the directory that provides the interface, checked
   against its digest. Writes its path into out. Gives 0 when the
   directory has none, and -1 when one is broken or two answer. */
static int discover_in(const char *dir, size_t dir_length,
                       const unsigned char *path, int64_t length, char *out,
                       size_t size)
{
    struct anti_text version = anti_rt_runtime_version();
    char index[ANTI_PLUGIN_PATH];
    struct anti_toml *doc;
    unsigned char *bytes;
    int64_t count;
    int64_t bytes_length = 0;
    int found = 0;
    int64_t n;

    if (!joined(index, sizeof index, dir, dir_length, "anti-plugins.toml")) {
        return 0;
    }
    bytes = anti_rt_fs_read(index, &bytes_length);
    if (bytes == NULL) {
        return 0;
    }
    doc = anti_rt_toml_read(bytes, bytes_length);
    free(bytes);
    if (doc == NULL) {
        fail("%s is no index of plugins", index);
        return -1;
    }
    count = anti_rt_toml_count(doc);
    for (n = 0; n < count; n++) {
        struct anti_text name = index_field(doc, n, "path");
        struct anti_text digest = index_field(doc, n, "digest");
        struct anti_text built = index_field(doc, n, "runtime");
        char library[ANTI_PLUGIN_PATH];
        char file[ANTI_PLUGIN_PATH];
        char hex[65];
        if (name.len == 0 || !index_lists(doc, n, path, length)) {
            continue;
        }
        /* A library built for another runtime is logged and passed
           over, as the specification asks. */
        if (!anti_rt_same_bytes(built.ptr, built.len, version.ptr, version.len)) {
            anti_rt_note("anti: %.*s was built for runtime %.*s, and this "
                         "program carries %.*s", (int)name.len, name.ptr,
                         (int)built.len, built.ptr, (int)version.len,
                         version.ptr);
            continue;
        }
        if (!path_of(file, sizeof file, name.ptr, name.len) ||
            !joined(library, sizeof library, dir, dir_length, file)) {
            continue;
        }
        if (!digest_file(library, hex) ||
            !anti_rt_same_bytes((const unsigned char *)hex, 64, digest.ptr,
                        digest.len)) {
            fail("%s does not match the digest of %s", library, index);
            anti_rt_toml_free(doc);
            return -1;
        }
        if (found) {
            fail("%s and %s both provide `%.*s`", out, library, (int)length,
                 path);
            anti_rt_toml_free(doc);
            return -1;
        }
        if (strlen(library) + 1 > size) {
            continue;
        }
        memcpy(out, library, strlen(library) + 1);
        found = 1;
    }
    anti_rt_toml_free(doc);
    return found;
}

/* Search every directory of dirs, which are separated by `:`, in
   order. */
static int discover(const char *dirs, const unsigned char *path,
                    int64_t length, char *out, size_t size)
{
    const char *at = dirs != NULL ? dirs : "";

    while (*at != '\0') {
        size_t n = strcspn(at, ":");
        int found = discover_in(at, n, path, length, out, size);
        if (found != 0) {
            return found;
        }
        at += n;
        at += *at == ':';
    }
    return 0;
}

void *anti_rt_plugin_provider(const unsigned char *path, int64_t path_length,
                              const unsigned char *library,
                              int64_t library_length, const char *dirs)
{
    char found[ANTI_PLUGIN_PATH];
    const struct anti_provides *e;
    struct anti_plugin *p;
    void *handle;

    message[0] = '\0';
    if (library_length == 0) {
        int answer = discover(dirs, path, path_length, found,
                              sizeof found);
        if (answer <= 0) {
            if (answer == 0) {
                fail("no library of the search directories provides `%.*s`",
                     (int)path_length, path);
            }
            return NULL;
        }
        library = (const unsigned char *)found;
        library_length = (int64_t)strlen(found);
    }
    handle = anti_rt_plugin_load(library, library_length);
    if (handle == NULL) {
        return NULL;
    }
    p = handle;
    anti_rt_plugin_hold();
    e = entry_of(p, NULL, path, path_length);
    anti_rt_plugin_release();
    if (e == NULL) {
        /* The unload clears the reason, so it comes first. */
        anti_rt_plugin_unload(handle);
        fail("%.*s provides no `%.*s`", (int)library_length, library,
             (int)path_length, path);
        return NULL;
    }
    return build(p, e);
}

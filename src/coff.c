#include "coff.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* DESIGN: lld-link and link.exe write no relocatable object, and the
   pinned LLVM release holds no other tool that joins COFF objects. antic
   joins them itself, for --bundle-runtime, so that a Windows archive holds
   the library and its runtime in one object as the other targets do.

   Every section keeps its bytes and its relocations. Only the numbers
   that point at sections and symbols change. Five things differ from a
   plain concatenation, because a linker reads one object otherwise than
   many.
   - A symbol that one object references and another defines becomes the
     defined symbol, and an undefined symbol stays once.
   - lld-link reads one .drectve per object, so the directives of all go
     into one, each once.
   - link.exe refuses two COMDATs of one name in one object. The first
     COMDAT of a name stays, and the sections associated with a dropped
     one go with it. Only the selection `any` may repeat.
   - .llvm_addrsig lists symbols by index, and a join renumbers them. It
     goes, as `ld.lld -r` drops it, and every symbol of the joined object
     is then significant.
   - One @feat.00 stays, with the features every object claims.
   A join that would change the meaning of the objects fails instead.
   That is a symbol defined twice, a CodeView line table or type stream
   in two objects, or an auxiliary record it does not read. */

enum {
    HEADER_SIZE = 20,
    SECTION_SIZE = 40,
    SYMBOL_SIZE = 18,
    RELOC_SIZE = 10,
    /* The characteristics of a section. */
    SCN_UNINITIALIZED = 0x80,
    SCN_LNK_INFO = 0x200,
    SCN_LNK_COMDAT = 0x1000,
    SCN_LNK_NRELOC_OVFL = 0x01000000,
    /* The storage classes of a symbol. */
    CLASS_EXTERNAL = 2,
    CLASS_STATIC = 3,
    CLASS_FILE = 103,
    CLASS_WEAK_EXTERNAL = 105,
    /* The COMDAT selections. */
    SELECT_ANY = 2,
    SELECT_ASSOCIATIVE = 5,
    /* The CodeView subsections of a line table. */
    CV_SIGNATURE = 4,
    CV_STRINGTABLE = 0xF3,
    CV_FILECHECKSUMS = 0xF4,
    /* The largest offset that a section name of 8 bytes can hold, as
       `/` and 7 decimal digits. */
    LONG_NAME_LIMIT = 9999999
};

/* A symbol with no index in the joined object, and one that takes the
   index of the definition of its name. */
#define DROPPED UINT32_MAX
#define BY_NAME (UINT32_MAX - 1)

#define FEATURES "@feat.00"

struct section {
    const unsigned char *header;
    const char *name;
    size_t name_length;
    uint32_t flags;
    bool kept;
    uint32_t out;               /* the number in the joined object */
    uint8_t selection;          /* of a COMDAT */
    uint32_t parent;            /* of an associative COMDAT */
    uint32_t leader;            /* the symbol of a COMDAT, or DROPPED */
};

struct object {
    const struct coff_input *in;
    size_t section_count;
    struct section *sections;
    size_t symbols_at;
    uint32_t symbol_count;
    const char *strings;
    size_t strings_size;
    uint32_t *map;              /* the index of each symbol when joined */
    bool *emits;                /* whether the join writes the symbol */
};

/* A name and where it stands: the object and the symbol, or the section
   of a COMDAT. */
struct entry {
    const char *name;
    size_t length;
    size_t object;
    uint32_t index;
};

struct table {
    struct entry *items;
    size_t capacity;
    size_t count;
};

struct join {
    struct object *objects;
    size_t count;
    struct table defined;
    struct table undefined;
    struct table comdats;
    struct text directives;
    uint32_t directive_flags;
    struct text strings;
    struct text *error;
};

static uint32_t get(const unsigned char *p, int n)
{
    uint32_t v = 0;
    int i;

    for (i = n - 1; i >= 0; i--) {
        v = v << 8 | p[i];
    }
    return v;
}

static void put(unsigned char *p, uint32_t v, int n)
{
    int i;

    for (i = 0; i < n; i++) {
        p[i] = (unsigned char)(v >> (8 * i));
    }
}

static void *allocate(size_t count, size_t size)
{
    void *p = calloc(count > 0 ? count : 1, size);

    if (p == NULL) {
        fputs("antic: out of memory\n", stderr);
        exit(70);
    }
    return p;
}

static uint64_t hash(const char *name, size_t length)
{
    uint64_t h = 1469598103934665603u;
    size_t i;

    for (i = 0; i < length; i++) {
        h = (h ^ (unsigned char)name[i]) * 1099511628211u;
    }
    return h;
}

/* The entry of name, or with insert a new empty one, which has no name. */
static struct entry *find(struct table *t, const char *name, size_t length,
                          bool insert)
{
    size_t i;

    if (insert && 2 * (t->count + 1) > t->capacity) {
        struct table grown = {0};
        grown.capacity = t->capacity > 0 ? 2 * t->capacity : 64;
        grown.items = allocate(grown.capacity, sizeof *grown.items);
        for (i = 0; i < t->capacity; i++) {
            if (t->items[i].name != NULL) {
                *find(&grown, t->items[i].name, t->items[i].length, true) =
                    t->items[i];
                grown.count++;
            }
        }
        free(t->items);
        *t = grown;
    }
    if (t->capacity == 0) {
        return NULL;
    }
    for (i = hash(name, length) & (t->capacity - 1);; i = (i + 1) & (t->capacity - 1)) {
        struct entry *e = &t->items[i];
        if (e->name == NULL) {
            return insert ? e : NULL;
        }
        if (e->length == length && memcmp(e->name, name, length) == 0) {
            return e;
        }
    }
}

static const unsigned char *symbol_at(const struct object *o, uint32_t index)
{
    return o->in->data + o->symbols_at + (size_t)SYMBOL_SIZE * index;
}

/* The name of a symbol, from its field or from the string table. */
static bool symbol_name(const struct object *o, uint32_t index,
                        const char **name, size_t *length)
{
    const unsigned char *s = symbol_at(o, index);

    if (get(s, 4) == 0) {
        uint32_t offset = get(s + 4, 4);
        const char *end;
        if (offset < 4 || offset >= o->strings_size) {
            return false;
        }
        end = memchr(o->strings + offset, '\0', o->strings_size - offset);
        if (end == NULL) {
            return false;
        }
        *name = o->strings + offset;
        *length = (size_t)(end - *name);
        return true;
    }
    *name = (const char *)s;
    *length = 0;
    while (*length < 8 && s[*length] != '\0') {
        (*length)++;
    }
    return true;
}

static bool is_named(const char *name, size_t length, const char *wanted)
{
    return length == strlen(wanted) && memcmp(name, wanted, length) == 0;
}

static bool fail(struct join *j, const char *what, const struct object *o,
                 const char *name, size_t length)
{
    text_appendf(j->error, "%s %.*s in %s", what, (int)length, name,
                 o->in->name);
    return false;
}

/* Whether a .debug$S section holds a line table: a table of file names or
   the strings it names. */
static bool holds_lines(const unsigned char *p, size_t size)
{
    size_t at = 4;

    if (size < 4 || get(p, 4) != CV_SIGNATURE) {
        return false;
    }
    while (at + 8 <= size) {
        uint32_t kind = get(p + at, 4) & 0x7FFFFFFF;
        uint32_t length = get(p + at + 4, 4);
        if (kind == CV_STRINGTABLE || kind == CV_FILECHECKSUMS) {
            return true;
        }
        if (length > size - at - 8) {
            return false;
        }
        at += 8 + ((length + 3) & ~(size_t)3);
    }
    return false;
}

/* Whether the directives hold the token already. */
static bool holds_token(const struct text *all, const char *token, size_t n)
{
    const char *p = text_cstr(all);

    while ((p = strstr(p, " ")) != NULL) {
        p++;
        if (strncmp(p, token, n) == 0 && (p[n] == ' ' || p[n] == '\0')) {
            return true;
        }
    }
    return false;
}

/* Add each directive of a .drectve that the joined one lacks. A quoted
   text is one token. */
static void add_directives(struct text *all, const char *p, size_t n)
{
    size_t i = 0;

    while (i < n) {
        size_t start;
        bool quoted = false;
        while (i < n && (p[i] == ' ' || p[i] == '\t' || p[i] == '\0')) {
            i++;
        }
        start = i;
        while (i < n && p[i] != '\0' &&
               (quoted || (p[i] != ' ' && p[i] != '\t'))) {
            quoted = p[i] == '"' ? !quoted : quoted;
            i++;
        }
        if (i > start && !holds_token(all, p + start, i - start)) {
            text_append(all, " ");
            text_append_bytes(all, p + start, i - start);
        }
    }
}

/* Read the headers of an object and check that every part lies inside
   it. */
static bool parse(struct join *j, struct object *o, uint16_t *machine)
{
    const unsigned char *d = o->in->data;
    size_t size = o->in->size;
    size_t i;

    if (size < HEADER_SIZE) {
        text_appendf(j->error, "%s is no COFF object", o->in->name);
        return false;
    }
    if (get(d, 2) == 0 && get(d + 2, 2) == 0xFFFF) {
        text_appendf(j->error, "%s is a big object or an import object, "
                               "which a join does not read", o->in->name);
        return false;
    }
    *machine = (uint16_t)get(d, 2);
    o->section_count = get(d + 2, 2);
    o->symbols_at = get(d + 8, 4);
    o->symbol_count = get(d + 12, 4);
    if (get(d + 16, 2) != 0 ||
        HEADER_SIZE + (size_t)SECTION_SIZE * o->section_count > size ||
        o->symbols_at > size ||
        o->symbol_count > (size - o->symbols_at) / SYMBOL_SIZE ||
        size - o->symbols_at - (size_t)SYMBOL_SIZE * o->symbol_count < 4) {
        text_appendf(j->error, "%s is no COFF object that a join reads",
                     o->in->name);
        return false;
    }
    o->strings = (const char *)d + o->symbols_at +
                 (size_t)SYMBOL_SIZE * o->symbol_count;
    o->strings_size = get((const unsigned char *)o->strings, 4);
    if (o->strings_size < 4 ||
        o->strings_size > size - (size_t)((const unsigned char *)o->strings - d)) {
        text_appendf(j->error, "%s has a damaged string table", o->in->name);
        return false;
    }
    o->sections = allocate(o->section_count, sizeof *o->sections);
    o->map = allocate(o->symbol_count, sizeof *o->map);
    o->emits = allocate(o->symbol_count, sizeof *o->emits);
    for (i = 0; i < o->section_count; i++) {
        struct section *s = &o->sections[i];
        const unsigned char *h = d + HEADER_SIZE + SECTION_SIZE * i;
        uint32_t raw = get(h + 16, 4);
        uint32_t relocs = get(h + 32, 2);
        s->header = h;
        s->flags = get(h + 36, 4);
        s->kept = true;
        s->leader = DROPPED;
        if (h[0] == '/') {
            unsigned long offset = strtoul((const char *)h + 1, NULL, 10);
            const char *end;
            if (offset < 4 || offset >= o->strings_size ||
                (end = memchr(o->strings + offset, '\0',
                              o->strings_size - offset)) == NULL) {
                text_appendf(j->error, "%s names a section it lacks",
                             o->in->name);
                return false;
            }
            s->name = o->strings + offset;
            s->name_length = (size_t)(end - s->name);
        } else {
            s->name = (const char *)h;
            while (s->name_length < 8 && h[s->name_length] != '\0') {
                s->name_length++;
            }
        }
        if (s->flags & SCN_LNK_NRELOC_OVFL && relocs == 0xFFFF &&
            get(h + 24, 4) + (size_t)RELOC_SIZE <= size) {
            relocs = get(d + get(h + 24, 4), 4);
        }
        if (get(h + 34, 2) != 0 ||
            (!(s->flags & SCN_UNINITIALIZED) &&
             (get(h + 20, 4) > size || raw > size - get(h + 20, 4))) ||
            get(h + 24, 4) > size ||
            relocs > (size - get(h + 24, 4)) / RELOC_SIZE) {
            text_appendf(j->error, "%s has a section %.*s that a join does "
                                   "not read", o->in->name,
                         (int)s->name_length, s->name);
            return false;
        }
    }
    return true;
}

/* The COMDAT facts of each section, from the section symbols and the
   symbol that follows each, and the check of every auxiliary record. */
static bool read_comdats(struct join *j, struct object *o)
{
    uint32_t i;

    for (i = 0; i < o->symbol_count; i++) {
        const unsigned char *s = symbol_at(o, i);
        int16_t number = (int16_t)get(s + 12, 2);
        uint8_t storage = s[16];
        uint8_t aux = s[17];
        const char *name;
        size_t length;
        if (aux > o->symbol_count - i - 1 || !symbol_name(o, i, &name, &length)) {
            text_appendf(j->error, "%s has a damaged symbol table", o->in->name);
            return false;
        }
        if (number > 0 && (size_t)number > o->section_count) {
            return fail(j, "a section number outside the object for", o, name,
                        length);
        }
        if (aux > 0 && storage == CLASS_STATIC && number > 0) {
            struct section *sec = &o->sections[number - 1];
            const unsigned char *a = s + SYMBOL_SIZE;
            if (sec->flags & SCN_LNK_COMDAT) {
                sec->selection = a[14];
                sec->parent = get(a + 12, 2);
            }
        } else if (aux > 0 && storage != CLASS_FILE &&
                   storage != CLASS_WEAK_EXTERNAL) {
            return fail(j, "an auxiliary record that a join does not read for",
                        o, name, length);
        } else if (number > 0 && (o->sections[number - 1].flags & SCN_LNK_COMDAT) &&
                   o->sections[number - 1].leader == DROPPED) {
            o->sections[number - 1].leader = i;
        }
        i += aux;
    }
    return true;
}

/* Take the directives, drop the tables that name symbols by index, and
   refuse a second line table or type stream. */
static bool read_sections(struct join *j, struct object *o, bool *lines,
                          bool *types)
{
    size_t i;
    bool has_lines = false;
    bool has_types = false;

    for (i = 0; i < o->section_count; i++) {
        struct section *s = &o->sections[i];
        const unsigned char *data = o->in->data + get(s->header + 20, 4);
        uint32_t size = get(s->header + 16, 4);
        if (is_named(s->name, s->name_length, ".drectve") &&
            (s->flags & SCN_LNK_INFO)) {
            if (j->directive_flags == 0) {
                j->directive_flags = s->flags;
            }
            add_directives(&j->directives, (const char *)data, size);
            s->kept = false;
        } else if (is_named(s->name, s->name_length, ".llvm_addrsig")) {
            s->kept = false;
        } else if (is_named(s->name, s->name_length, ".debug$T") ||
                   is_named(s->name, s->name_length, ".debug$P")) {
            has_types = has_types || size > 0;
        } else if (is_named(s->name, s->name_length, ".debug$S")) {
            has_lines = has_lines || holds_lines(data, size);
        }
    }
    if ((has_lines && *lines) || (has_types && *types)) {
        text_appendf(j->error, "%s carries CodeView lines or types, and so "
                               "does an object before it", o->in->name);
        return false;
    }
    *lines = *lines || has_lines;
    *types = *types || has_types;
    return true;
}

/* Keep the first COMDAT of each name, and drop every section associated
   with a dropped one. */
static bool choose_comdats(struct join *j)
{
    size_t k;
    bool changed = true;

    for (k = 0; k < j->count; k++) {
        struct object *o = &j->objects[k];
        size_t i;
        for (i = 0; i < o->section_count; i++) {
            struct section *s = &o->sections[i];
            const char *name;
            size_t length;
            struct entry *e;
            if (!(s->flags & SCN_LNK_COMDAT) || s->selection == SELECT_ASSOCIATIVE) {
                continue;
            }
            if (s->leader == DROPPED) {
                text_appendf(j->error, "a COMDAT of %s has no symbol",
                             o->in->name);
                return false;
            }
            /* A COMDAT of a static symbol is the object's own, and no
               linker merges it with another. */
            if (symbol_at(o, s->leader)[16] != CLASS_EXTERNAL) {
                continue;
            }
            symbol_name(o, s->leader, &name, &length);
            e = find(&j->comdats, name, length, true);
            if (e->name == NULL) {
                e->name = name;
                e->length = length;
                e->object = k;
                e->index = (uint32_t)i;
                j->comdats.count++;
            } else if (s->selection == SELECT_ANY &&
                       j->objects[e->object].sections[e->index].selection ==
                           SELECT_ANY) {
                s->kept = false;
            } else {
                return fail(j, "a second COMDAT", o, name, length);
            }
        }
    }
    while (changed) {
        changed = false;
        for (k = 0; k < j->count; k++) {
            struct object *o = &j->objects[k];
            size_t i;
            for (i = 0; i < o->section_count; i++) {
                struct section *s = &o->sections[i];
                if (!(s->flags & SCN_LNK_COMDAT) ||
                    s->selection != SELECT_ASSOCIATIVE) {
                    continue;
                }
                if (s->parent == 0 || s->parent > o->section_count) {
                    text_appendf(j->error, "a COMDAT of %s names no section",
                                 o->in->name);
                    return false;
                }
                if (s->kept && !o->sections[s->parent - 1].kept) {
                    s->kept = false;
                    changed = true;
                }
            }
        }
    }
    return true;
}

/* Enter every external definition, and refuse a name defined twice. */
static bool enter_definitions(struct join *j)
{
    size_t k;

    for (k = 0; k < j->count; k++) {
        struct object *o = &j->objects[k];
        uint32_t i;
        for (i = 0; i < o->symbol_count; i += 1 + symbol_at(o, i)[17]) {
            const unsigned char *s = symbol_at(o, i);
            int16_t number = (int16_t)get(s + 12, 2);
            const char *name;
            size_t length;
            struct entry *e;
            if (s[16] != CLASS_EXTERNAL || number == 0 ||
                (number > 0 && !o->sections[number - 1].kept)) {
                continue;
            }
            symbol_name(o, i, &name, &length);
            e = find(&j->defined, name, length, true);
            if (e->name != NULL) {
                text_appendf(j->error, "duplicate symbol %.*s in %s and %s",
                             (int)length, name,
                             j->objects[e->object].in->name, o->in->name);
                return false;
            }
            e->name = name;
            e->length = length;
            e->object = k;
            e->index = i;
            j->defined.count++;
        }
    }
    return true;
}

/* Give each symbol that stays its index in the joined object. Returns the
   count of records, the auxiliary ones among them. */
static uint32_t number_symbols(struct join *j, uint32_t *features_index,
                               uint32_t *features)
{
    uint32_t count = 0;
    size_t k;

    *features_index = DROPPED;
    *features = UINT32_MAX;
    for (k = 0; k < j->count; k++) {
        struct object *o = &j->objects[k];
        uint32_t feature = 0;
        uint32_t i;
        for (i = 0; i < o->symbol_count; i++) {
            const unsigned char *s = symbol_at(o, i);
            int16_t number = (int16_t)get(s + 12, 2);
            uint8_t aux = s[17];
            const char *name;
            size_t length;
            struct entry *e;
            uint32_t a;
            symbol_name(o, i, &name, &length);
            for (a = 1; a <= aux; a++) {
                o->map[i + a] = DROPPED;
            }
            if (number > 0 && !o->sections[number - 1].kept) {
                o->map[i] = s[16] == CLASS_EXTERNAL ? BY_NAME : DROPPED;
            } else if (number == -1 && s[16] == CLASS_STATIC &&
                       is_named(name, length, FEATURES)) {
                feature = get(s + 8, 4);
                if (*features_index == DROPPED) {
                    *features_index = count;
                    o->map[i] = count;
                    o->emits[i] = true;
                    count += 1u + aux;
                } else {
                    o->map[i] = *features_index;
                }
            } else if (number == 0 && s[16] == CLASS_EXTERNAL &&
                       get(s + 8, 4) == 0 &&
                       find(&j->defined, name, length, false) != NULL) {
                o->map[i] = BY_NAME;
            } else if (number == 0 && s[16] == CLASS_EXTERNAL &&
                       get(s + 8, 4) == 0) {
                e = find(&j->undefined, name, length, true);
                if (e->name == NULL) {
                    e->name = name;
                    e->length = length;
                    e->index = count;
                    j->undefined.count++;
                    o->map[i] = count;
                    o->emits[i] = true;
                    count += 1u + aux;
                } else {
                    o->map[i] = e->index;
                }
            } else {
                o->map[i] = count;
                o->emits[i] = true;
                count += 1u + aux;
            }
            i += aux;
        }
        *features &= feature;
    }
    for (k = 0; k < j->count; k++) {
        struct object *o = &j->objects[k];
        uint32_t i;
        for (i = 0; i < o->symbol_count; i += 1 + symbol_at(o, i)[17]) {
            const char *name;
            size_t length;
            struct entry *e;
            if (o->map[i] != BY_NAME) {
                continue;
            }
            symbol_name(o, i, &name, &length);
            e = find(&j->defined, name, length, false);
            o->map[i] = e != NULL ? j->objects[e->object].map[e->index] : DROPPED;
        }
    }
    return count;
}

/* The 8 bytes of a name field: the name itself, or its offset in the
   string table of the joined object, as `/offset` for a section. */
static bool name_field(struct join *j, unsigned char *field, const char *name,
                       size_t length, bool section)
{
    size_t offset = 4 + j->strings.length;

    memset(field, 0, 8);
    if (length <= 8) {
        memcpy(field, name, length);
        return true;
    }
    if (section) {
        char digits[9];
        if (offset > LONG_NAME_LIMIT) {
            text_append(j->error, "the joined object has too many long "
                                  "section names");
            return false;
        }
        snprintf(digits, sizeof digits, "/%zu", offset);
        memcpy(field, digits, strlen(digits));
    } else {
        put(field + 4, (uint32_t)offset, 4);
    }
    text_append_bytes(&j->strings, name, length);
    text_append_bytes(&j->strings, "", 1);
    return true;
}

static void pad(struct text *out)
{
    static const char zeros[4] = {0};

    text_append_bytes(out, zeros, (4 - out->length % 4) % 4);
}

/* Write the sections of every object that stay, with their relocations
   renumbered, and the one .drectve. headers is the table of section
   headers, which this fills in. */
static bool write_sections(struct join *j, struct text *out,
                           unsigned char *headers)
{
    uint32_t number = 0;
    size_t k;

    for (k = 0; k < j->count; k++) {
        struct object *o = &j->objects[k];
        size_t i;
        for (i = 0; i < o->section_count; i++) {
            struct section *s = &o->sections[i];
            unsigned char *h = headers + SECTION_SIZE * number;
            uint32_t raw = get(s->header + 16, 4);
            uint32_t relocs = get(s->header + 32, 2);
            const unsigned char *r = o->in->data + get(s->header + 24, 4);
            uint32_t n;
            if (!s->kept) {
                continue;
            }
            memcpy(h, s->header, SECTION_SIZE);
            if (!name_field(j, h, s->name, s->name_length, true)) {
                return false;
            }
            put(h + 20, 0, 4);
            put(h + 28, 0, 4);
            if (!(s->flags & SCN_UNINITIALIZED) && raw > 0) {
                pad(out);
                put(h + 20, (uint32_t)out->length, 4);
                text_append_bytes(out, o->in->data + get(s->header + 20, 4), raw);
            }
            if (s->flags & SCN_LNK_NRELOC_OVFL && relocs == 0xFFFF) {
                relocs = get(r, 4);
            }
            put(h + 24, 0, 4);
            if (relocs > 0) {
                pad(out);
                put(h + 24, (uint32_t)out->length, 4);
            }
            for (n = 0; n < relocs; n++) {
                unsigned char record[RELOC_SIZE];
                uint32_t symbol;
                memcpy(record, r + RELOC_SIZE * n, RELOC_SIZE);
                symbol = get(record + 4, 4);
                if (!(n == 0 && s->flags & SCN_LNK_NRELOC_OVFL &&
                      get(s->header + 32, 2) == 0xFFFF)) {
                    if (symbol >= o->symbol_count || o->map[symbol] >= BY_NAME) {
                        text_appendf(j->error, "a relocation of %.*s in %s "
                                               "names a symbol the join drops",
                                     (int)s->name_length, s->name, o->in->name);
                        return false;
                    }
                    put(record + 4, o->map[symbol], 4);
                }
                text_append_bytes(out, record, RELOC_SIZE);
            }
            number++;
        }
    }
    if (j->directives.length > 0) {
        unsigned char *h = headers + SECTION_SIZE * number;
        memset(h, 0, SECTION_SIZE);
        memcpy(h, ".drectve", 8);
        pad(out);
        put(h + 16, (uint32_t)j->directives.length, 4);
        put(h + 20, (uint32_t)out->length, 4);
        put(h + 36, j->directive_flags, 4);
        text_append_bytes(out, j->directives.data, j->directives.length);
    }
    return true;
}

/* Write the symbols that stay, with their sections and the auxiliary
   records that point at a section or a symbol renumbered. */
static bool write_symbols(struct join *j, struct text *out,
                          uint32_t features_index, uint32_t features)
{
    size_t k;

    for (k = 0; k < j->count; k++) {
        struct object *o = &j->objects[k];
        uint32_t i;
        for (i = 0; i < o->symbol_count; i += 1 + symbol_at(o, i)[17]) {
            const unsigned char *s = symbol_at(o, i);
            int16_t number = (int16_t)get(s + 12, 2);
            uint8_t aux = s[17];
            unsigned char record[SYMBOL_SIZE];
            const char *name;
            size_t length;
            uint32_t a;
            if (!o->emits[i]) {
                continue;
            }
            memcpy(record, s, SYMBOL_SIZE);
            symbol_name(o, i, &name, &length);
            if (!name_field(j, record, name, length, false)) {
                return false;
            }
            if (number > 0) {
                put(record + 12, o->sections[number - 1].out, 2);
            }
            if (o->map[i] == features_index) {
                put(record + 8, features, 4);
            }
            text_append_bytes(out, record, SYMBOL_SIZE);
            for (a = 1; a <= aux; a++) {
                memcpy(record, s + SYMBOL_SIZE * a, SYMBOL_SIZE);
                if (a == 1 && s[16] == CLASS_STATIC && number > 0 &&
                    o->sections[number - 1].selection == SELECT_ASSOCIATIVE &&
                    (o->sections[number - 1].flags & SCN_LNK_COMDAT)) {
                    put(record + 12,
                        o->sections[o->sections[number - 1].parent - 1].out, 2);
                } else if (a == 1 && s[16] == CLASS_WEAK_EXTERNAL) {
                    uint32_t tag = get(record, 4);
                    if (tag >= o->symbol_count || o->map[tag] >= BY_NAME) {
                        return fail(j, "a weak external without its default",
                                    o, name, length);
                    }
                    put(record, o->map[tag], 4);
                }
                text_append_bytes(out, record, SYMBOL_SIZE);
            }
        }
    }
    return true;
}

static void release(struct join *j)
{
    size_t k;

    for (k = 0; k < j->count; k++) {
        free(j->objects[k].sections);
        free(j->objects[k].map);
        free(j->objects[k].emits);
    }
    free(j->objects);
    free(j->defined.items);
    free(j->undefined.items);
    free(j->comdats.items);
    text_free(&j->directives);
    text_free(&j->strings);
}

bool coff_join(const struct coff_input *inputs, size_t count,
               struct text *out, struct text *error)
{
    struct join j;
    struct text body = {0};
    unsigned char *headers = NULL;
    uint16_t machine = 0;
    uint32_t sections = 0;
    uint32_t symbols;
    uint32_t features_index;
    uint32_t features;
    bool lines = false;
    bool types = false;
    bool ok = true;
    size_t k;

    memset(&j, 0, sizeof j);
    j.objects = allocate(count, sizeof *j.objects);
    j.count = count;
    j.error = error;
    for (k = 0; ok && k < count; k++) {
        uint16_t m = 0;
        j.objects[k].in = &inputs[k];
        ok = parse(&j, &j.objects[k], &m) && read_comdats(&j, &j.objects[k]) &&
             read_sections(&j, &j.objects[k], &lines, &types);
        if (ok && k > 0 && m != machine) {
            text_appendf(error, "%s is for another machine than %s",
                         inputs[k].name, inputs[0].name);
            ok = false;
        }
        machine = m;
    }
    ok = ok && choose_comdats(&j) && enter_definitions(&j);
    if (ok) {
        for (k = 0; k < count; k++) {
            size_t i;
            for (i = 0; i < j.objects[k].section_count; i++) {
                if (j.objects[k].sections[i].kept) {
                    j.objects[k].sections[i].out = ++sections;
                }
            }
        }
        symbols = number_symbols(&j, &features_index, &features);
        sections += j.directives.length > 0 ? 1u : 0u;
        headers = allocate(sections, SECTION_SIZE);
        /* The body starts after the headers, so its offsets are those of
           the file. */
        text_append_bytes(&body, headers, HEADER_SIZE);
        text_append_bytes(&body, headers, (size_t)SECTION_SIZE * sections);
        ok = write_sections(&j, &body, headers);
    }
    if (ok) {
        struct text table = {0};
        unsigned char header[HEADER_SIZE];
        size_t symbols_at;
        pad(&body);
        symbols_at = body.length;
        ok = write_symbols(&j, &table, features_index, features);
        memset(header, 0, sizeof header);
        put(header, machine, 2);
        put(header + 2, sections, 2);
        put(header + 8, (uint32_t)symbols_at, 4);
        put(header + 12, symbols, 4);
        put(header + 18, get(inputs[0].data + 18, 2), 2);
        memcpy(body.data, header, HEADER_SIZE);
        memcpy(body.data + HEADER_SIZE, headers, (size_t)SECTION_SIZE * sections);
        text_append_bytes(&body, table.data, table.length);
        {
            unsigned char size[4];
            put(size, (uint32_t)(4 + j.strings.length), 4);
            text_append_bytes(&body, size, 4);
            text_append_bytes(&body, j.strings.data, j.strings.length);
        }
        if (ok && table.length / SYMBOL_SIZE != symbols) {
            text_append(error, "internal error: the symbols of the join do "
                               "not add up");
            ok = false;
        }
        text_free(&table);
    }
    if (ok) {
        text_append_bytes(out, body.data, body.length);
    }
    free(headers);
    text_free(&body);
    release(&j);
    return ok;
}

/* The join of COFF objects that a bundled runtime of a Windows library
   takes. Each test builds small objects by hand, joins them and reads the
   result back. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../binary_stdio.h"
#include "check.h"
#include "alloc.h"
#include "coff.h"
#include "text.h"

enum {
    AMD64 = 0x8664,
    ARM64 = 0xAA64,
    CODE = 0x60500020,
    RDATA = 0x40301040,
    DATA = 0x40300040,
    COMDAT = 0x1000,
    INFO = 0x00100A00,
    DEBUG = 0x42100040,
    EXTERNAL = 2,
    STATIC = 3,
    ANY = 2,
    NODUPLICATES = 1,
    ASSOCIATIVE = 5,
    REL32 = 4
};

struct t_reloc {
    uint32_t at;
    uint32_t symbol;
};

struct t_section {
    const char *name;
    uint32_t flags;
    const char *data;
    size_t size;
    struct t_reloc relocs[3];
    size_t reloc_count;
};

/* A symbol, and the one auxiliary record of a section symbol. A section
   symbol takes two indices. */
struct t_symbol {
    const char *name;
    uint32_t value;
    int16_t section;
    uint8_t storage;
    bool section_aux;
    uint8_t selection;
    uint16_t number;
};

struct t_object {
    uint16_t machine;
    struct t_section sections[5];
    size_t section_count;
    struct t_symbol symbols[8];
    size_t symbol_count;
};

static void put16(struct text *t, uint16_t v)
{
    unsigned char b[2] = {(unsigned char)v, (unsigned char)(v >> 8)};
    text_append_bytes(t, b, 2);
}

static void put32(struct text *t, uint32_t v)
{
    put16(t, (uint16_t)v);
    put16(t, (uint16_t)(v >> 16));
}

static void put_name(struct text *t, struct text *strings, const char *name)
{
    char field[8] = {0};
    size_t n = strlen(name);

    if (n <= 8) {
        memcpy(field, name, n);
        text_append_bytes(t, field, 8);
        return;
    }
    put32(t, 0);
    put32(t, (uint32_t)(4 + strings->length));
    text_append_bytes(strings, name, n + 1);
}

static void build(const struct t_object *o, struct text *out)
{
    struct text strings = {0};
    uint32_t at = (uint32_t)(20 + 40 * o->section_count);
    uint32_t symbols;
    uint32_t count = 0;
    size_t i;

    for (i = 0; i < o->section_count; i++) {
        at += (uint32_t)(o->sections[i].size + 10 * o->sections[i].reloc_count);
    }
    symbols = at;
    for (i = 0; i < o->symbol_count; i++) {
        count += o->symbols[i].section_aux ? 2u : 1u;
    }
    put16(out, o->machine);
    put16(out, (uint16_t)o->section_count);
    put32(out, 0x12345678);
    put32(out, symbols);
    put32(out, count);
    put16(out, 0);
    put16(out, 0);
    at = (uint32_t)(20 + 40 * o->section_count);
    for (i = 0; i < o->section_count; i++) {
        const struct t_section *s = &o->sections[i];
        char field[8] = {0};
        if (strlen(s->name) <= 8) {
            memcpy(field, s->name, strlen(s->name));
        } else {
            snprintf(field, sizeof field, "/%u", (unsigned)(4 + strings.length));
            text_append_bytes(&strings, s->name, strlen(s->name) + 1);
        }
        text_append_bytes(out, field, 8);
        put32(out, 0);
        put32(out, 0);
        put32(out, (uint32_t)s->size);
        put32(out, s->size > 0 ? at : 0);
        put32(out, s->reloc_count > 0 ? at + (uint32_t)s->size : 0);
        put32(out, 0);
        put16(out, (uint16_t)s->reloc_count);
        put16(out, 0);
        put32(out, s->flags);
        at += (uint32_t)(s->size + 10 * s->reloc_count);
    }
    for (i = 0; i < o->section_count; i++) {
        const struct t_section *s = &o->sections[i];
        size_t r;
        text_append_bytes(out, s->data, s->size);
        for (r = 0; r < s->reloc_count; r++) {
            put32(out, s->relocs[r].at);
            put32(out, s->relocs[r].symbol);
            put16(out, REL32);
        }
    }
    for (i = 0; i < o->symbol_count; i++) {
        const struct t_symbol *s = &o->symbols[i];
        put_name(out, &strings, s->name);
        put32(out, s->value);
        put16(out, (uint16_t)s->section);
        put16(out, 0);
        text_append_bytes(out, &s->storage, 1);
        text_append_bytes(out, s->section_aux ? "\1" : "\0", 1);
        if (s->section_aux) {
            const struct t_section *sec = &o->sections[s->section - 1];
            char unused[3] = {0};
            put32(out, (uint32_t)sec->size);
            put16(out, (uint16_t)sec->reloc_count);
            put16(out, 0);
            put32(out, 0);
            put16(out, s->number);
            text_append_bytes(out, &s->selection, 1);
            text_append_bytes(out, unused, 3);
        }
    }
    put32(out, (uint32_t)(4 + strings.length));
    text_append_bytes(out, strings.data, strings.length);
    text_free(&strings);
}

/* The readers of the joined object. */
static uint32_t get(const struct text *t, size_t at, int n)
{
    uint32_t v = 0;
    int i;

    for (i = n - 1; i >= 0; i--) {
        v = v << 8 | (unsigned char)t->data[at + (size_t)i];
    }
    return v;
}

static size_t sections_of(const struct text *t) { return get(t, 2, 2); }
static size_t symbols_of(const struct text *t) { return get(t, 12, 4); }

static size_t strings_at(const struct text *t)
{
    return get(t, 8, 4) + 18 * symbols_of(t);
}

static size_t section_at(size_t number) { return 20 + 40 * (number - 1); }

/* The size of the buffer that takes a name of a section or a symbol. A
   longer name from the string table is cut to fit. */
#define NAME_SIZE 64

static void section_name(const struct text *t, size_t number, char *out)
{
    const char *field = t->data + section_at(number);

    if (field[0] == '/') {
        unsigned long offset = strtoul(field + 1, NULL, 10);
        snprintf(out, NAME_SIZE, "%s", t->data + strings_at(t) + offset);
    } else {
        memcpy(out, field, 8);
        out[8] = '\0';
    }
}

static void symbol_name(const struct text *t, size_t index, char *out)
{
    size_t at = get(t, 8, 4) + 18 * index;

    if (get(t, at, 4) == 0) {
        snprintf(out, NAME_SIZE, "%s",
                 t->data + strings_at(t) + get(t, at + 4, 4));
    } else {
        memcpy(out, t->data + at, 8);
        out[8] = '\0';
    }
}

/* The index of the nth symbol called name, or -1. */
static long find_symbol(const struct text *t, const char *name, int nth)
{
    size_t i;

    for (i = 0; i < symbols_of(t);
         i += 1 + (unsigned char)t->data[get(t, 8, 4) + 18 * i + 17]) {
        char got[NAME_SIZE];
        symbol_name(t, i, got);
        if (strcmp(got, name) == 0 && nth-- == 0) {
            return (long)i;
        }
    }
    return -1;
}

static int16_t symbol_section(const struct text *t, long index)
{
    return (int16_t)get(t, get(t, 8, 4) + 18 * (size_t)index + 12, 2);
}

static uint32_t symbol_value(const struct text *t, long index)
{
    return get(t, get(t, 8, 4) + 18 * (size_t)index + 8, 4);
}

/* The symbol index of relocation r of section number. */
static long reloc_symbol(const struct text *t, size_t number, size_t r)
{
    size_t at = get(t, section_at(number) + 24, 4) + 10 * r;
    return (long)get(t, at + 4, 4);
}

static bool join(const struct t_object *a, const struct t_object *b,
                 struct text *out, struct text *error)
{
    struct text first = {0};
    struct text second = {0};
    struct coff_input inputs[2];
    bool ok;

    build(a, &first);
    build(b, &second);
    inputs[0].name = "a.o";
    inputs[0].data = (const unsigned char *)first.data;
    inputs[0].size = first.length;
    inputs[1].name = "b.o";
    inputs[1].data = (const unsigned char *)second.data;
    inputs[1].size = second.length;
    ok = coff_join(inputs, 2, out, error);
    text_free(&first);
    text_free(&second);
    return ok;
}

static const char call[] = "\xe8\0\0\0\0\xc3\xcc\xcc";

/* A reference of one object to a symbol of the other takes the defined
   symbol, and an undefined symbol that nothing defines stays once. */
static void test_across(void)
{
    struct t_object a = {AMD64, {{".text", CODE, call, 8, {{1, 2}, {1, 3}}, 2}}, 1,
                         {{".text", 0, 1, STATIC, true, 0, 0},
                          {"a_long_function_name", 0, 0, EXTERNAL, false, 0, 0},
                          {"printf", 0, 0, EXTERNAL, false, 0, 0},
                          {"foo", 0, 1, EXTERNAL, false, 0, 0}}, 4};
    struct t_object b = {AMD64, {{".text", CODE, call, 8, {{1, 2}, {1, 3}}, 2}}, 1,
                         {{".text", 0, 1, STATIC, true, 0, 0},
                          {"foo", 0, 0, EXTERNAL, false, 0, 0},
                          {"printf", 0, 0, EXTERNAL, false, 0, 0},
                          {"a_long_function_name", 4, 1, EXTERNAL, false, 0, 0}}, 4};
    struct text out = {0};
    struct text error = {0};

    CHECK(join(&a, &b, &out, &error));
    CHECK_STR(text_cstr(&error), "");
    if (out.length == 0) {
        return;
    }
    CHECK(get(&out, 0, 2) == AMD64);
    CHECK(get(&out, 4, 4) == 0);
    CHECK(sections_of(&out) == 2);
    CHECK(find_symbol(&out, "foo", 1) == -1);
    CHECK(find_symbol(&out, "printf", 1) == -1);
    CHECK(find_symbol(&out, "a_long_function_name", 1) == -1);
    CHECK(symbol_section(&out, find_symbol(&out, "foo", 0)) == 1);
    CHECK(symbol_section(&out, find_symbol(&out, "a_long_function_name", 0)) == 2);
    CHECK(symbol_value(&out, find_symbol(&out, "a_long_function_name", 0)) == 4);
    CHECK(symbol_section(&out, find_symbol(&out, ".text", 1)) == 2);
    CHECK(reloc_symbol(&out, 1, 0) == find_symbol(&out, "a_long_function_name", 0));
    CHECK(reloc_symbol(&out, 1, 1) == find_symbol(&out, "printf", 0));
    CHECK(reloc_symbol(&out, 2, 0) == find_symbol(&out, "foo", 0));
    CHECK(reloc_symbol(&out, 2, 1) == find_symbol(&out, "printf", 0));
    text_free(&out);
    text_free(&error);
}

/* The directives of every object go into one .drectve, each once, and
   the table of address-significant symbols goes. */
static void test_directives(void)
{
    static const char da[] = " /DEFAULTLIB:msvcrt.lib /DEFAULTLIB:oldnames.lib";
    static const char db[] = " /DEFAULTLIB:msvcrt.lib /alternatename:x=y";
    struct t_object a = {AMD64, {{".text", CODE, call, 8, {{0}}, 0},
                                 {".drectve", INFO, da, sizeof da - 1, {{0}}, 0}}, 2,
                         {{".drectve", 0, 2, STATIC, true, 0, 0}}, 1};
    struct t_object b = {AMD64, {{".drectve", INFO, db, sizeof db - 1, {{0}}, 0},
                                 {".llvm_addrsig", 0x00100800, "\1", 1, {{0}}, 0},
                                 {".text", CODE, call, 8, {{0}}, 0}}, 3,
                         {{".text", 0, 3, STATIC, true, 0, 0},
                          {"bar", 0, 3, EXTERNAL, false, 0, 0}}, 2};
    struct text out = {0};
    struct text error = {0};
    char name[NAME_SIZE];
    size_t at;

    CHECK(join(&a, &b, &out, &error));
    CHECK_STR(text_cstr(&error), "");
    if (out.length == 0) {
        return;
    }
    CHECK(sections_of(&out) == 3);
    section_name(&out, 1, name);
    CHECK_STR(name, ".text");
    section_name(&out, 2, name);
    CHECK_STR(name, ".text");
    section_name(&out, 3, name);
    CHECK_STR(name, ".drectve");
    CHECK(get(&out, section_at(3) + 36, 4) == INFO);
    at = get(&out, section_at(3) + 20, 4);
    CHECK(get(&out, section_at(3) + 16, 4) ==
          strlen(" /DEFAULTLIB:msvcrt.lib /DEFAULTLIB:oldnames.lib "
                 "/alternatename:x=y"));
    CHECK(memcmp(out.data + at,
                 " /DEFAULTLIB:msvcrt.lib /DEFAULTLIB:oldnames.lib "
                 "/alternatename:x=y", get(&out, section_at(3) + 16, 4)) == 0);
    CHECK(find_symbol(&out, ".drectve", 0) == -1);
    CHECK(symbol_section(&out, find_symbol(&out, "bar", 0)) == 2);
    text_free(&out);
    text_free(&error);
}

/* A COMDAT of the same name keeps the first, and the sections associated
   with a dropped one go with it. An associative section names its parent
   by the number the join gave it. */
static void test_comdat(void)
{
    struct t_object a = {AMD64, {{".text", CODE, call, 8, {{1, 4}}, 1},
                                 {".rdata", RDATA | COMDAT, "hi", 2, {{0}}, 0}}, 2,
                         {{".text", 0, 1, STATIC, true, 0, 0},
                          {".rdata", 0, 2, STATIC, true, ANY, 0},
                          {"??_C@str", 0, 2, EXTERNAL, false, 0, 0}}, 3};
    struct t_object b = {AMD64, {{".text", CODE, call, 8, {{1, 4}, {1, 9}}, 2},
                                 {".rdata", RDATA | COMDAT, "hi", 2, {{0}}, 0},
                                 {".xdata", RDATA | COMDAT, "x", 1, {{0}}, 0},
                                 {".rdata", RDATA | COMDAT, "u", 1, {{0}}, 0},
                                 {".xdata", RDATA | COMDAT, "y", 1, {{0}}, 0}}, 5,
                         {{".text", 0, 1, STATIC, true, 0, 0},
                          {".rdata", 0, 2, STATIC, true, ANY, 0},
                          {"??_C@str", 0, 2, EXTERNAL, false, 0, 0},
                          {".xdata", 0, 3, STATIC, true, ASSOCIATIVE, 2},
                          {".rdata", 0, 4, STATIC, true, ANY, 0},
                          {"uniq", 0, 4, EXTERNAL, false, 0, 0},
                          {".xdata", 0, 5, STATIC, true, ASSOCIATIVE, 4}}, 7};
    struct text out = {0};
    struct text error = {0};
    long assoc;

    CHECK(join(&a, &b, &out, &error));
    CHECK_STR(text_cstr(&error), "");
    if (out.length == 0) {
        return;
    }
    CHECK(sections_of(&out) == 5);
    CHECK(find_symbol(&out, "??_C@str", 1) == -1);
    CHECK(symbol_section(&out, find_symbol(&out, "??_C@str", 0)) == 2);
    CHECK(symbol_section(&out, find_symbol(&out, "uniq", 0)) == 4);
    CHECK(reloc_symbol(&out, 3, 0) == find_symbol(&out, "??_C@str", 0));
    CHECK(reloc_symbol(&out, 3, 1) == find_symbol(&out, "uniq", 0));
    CHECK(find_symbol(&out, ".xdata", 1) == -1);
    assoc = find_symbol(&out, ".xdata", 0);
    CHECK(symbol_section(&out, assoc) == 5);
    CHECK(get(&out, get(&out, 8, 4) + 18 * (size_t)(assoc + 1) + 12, 2) == 4);
    CHECK(out.data[get(&out, 8, 4) + 18 * (size_t)(assoc + 1) + 14] ==
          ASSOCIATIVE);
    text_free(&out);
    text_free(&error);
}

/* One @feat.00 stays, with the features every object claims. */
static void test_features(void)
{
    struct t_object a = {AMD64, {{".text", CODE, call, 8, {{0}}, 0}}, 1,
                         {{"@feat.00", 0x10, -1, STATIC, false, 0, 0}}, 1};
    struct t_object b = {AMD64, {{".text", CODE, call, 8, {{0}}, 0}}, 1,
                         {{"@feat.00", 0x11, -1, STATIC, false, 0, 0}}, 1};
    struct text out = {0};
    struct text error = {0};

    CHECK(join(&a, &b, &out, &error));
    if (out.length == 0) {
        return;
    }
    CHECK(find_symbol(&out, "@feat.00", 1) == -1);
    CHECK(symbol_value(&out, find_symbol(&out, "@feat.00", 0)) == 0x10);
    text_free(&out);
    text_free(&error);
}

/* CodeView holds one type stream and one table of strings per object.
   An object with a line table or a type stream loses its debug sections
   when one before it has either. Its code stays. */
static void test_codeview(void)
{
    static const char lines[] = "\4\0\0\0\xf3\0\0\0\4\0\0\0\0\0\0\0";
    static const char names[] = "\4\0\0\0\xf1\0\0\0\0\0\0\0";
    struct t_object a = {AMD64, {{".text", CODE, call, 8, {{0}}, 0},
                                 {".debug$S", DEBUG, lines, 16, {{0}}, 0},
                                 {".debug$T", DEBUG, "\4\0\0\0", 4, {{0}}, 0}}, 3,
                         {{"foo", 0, 1, EXTERNAL, false, 0, 0}}, 1};
    struct t_object b = {AMD64, {{".debug$S", DEBUG, names, 12, {{0}}, 0},
                                 {".text", CODE, call, 8, {{0}}, 0},
                                 {".debug$S", DEBUG, lines, 16, {{0}}, 0},
                                 {".debug$T", DEBUG, "\4\0\0\0", 4, {{0}}, 0}}, 4,
                         {{".debug$S", 0, 3, STATIC, true, 0, 0},
                          {"bar", 0, 2, EXTERNAL, false, 0, 0}}, 2};
    struct text out = {0};
    struct text error = {0};
    char name[NAME_SIZE];
    size_t i;
    int debug = 0;

    CHECK(join(&a, &b, &out, &error));
    CHECK_STR(text_cstr(&error), "");
    if (out.length == 0) {
        return;
    }
    CHECK(sections_of(&out) == 4);
    for (i = 1; i <= sections_of(&out); i++) {
        section_name(&out, i, name);
        debug += strncmp(name, ".debug$", 7) == 0;
    }
    CHECK(debug == 2);
    section_name(&out, 4, name);
    CHECK_STR(name, ".text");
    CHECK(find_symbol(&out, ".debug$S", 0) == -1);
    CHECK(symbol_section(&out, find_symbol(&out, "bar", 0)) == 4);
    text_free(&out);
    text_free(&error);
}

/* The joins that would give another program than the objects do. */
static void test_refusals(void)
{
    struct t_object a = {AMD64, {{".text", CODE, call, 8, {{0}}, 0}}, 1,
                         {{"foo", 0, 1, EXTERNAL, false, 0, 0}}, 1};
    struct t_object arm = {ARM64, {{".text", CODE, call, 8, {{0}}, 0}}, 1,
                           {{"bar", 0, 1, EXTERNAL, false, 0, 0}}, 1};
    struct t_object single = {AMD64,
                              {{".rdata", RDATA | COMDAT, "u", 1, {{0}}, 0}}, 1,
                              {{".rdata", 0, 1, STATIC, true, NODUPLICATES, 0},
                               {"once", 0, 1, EXTERNAL, false, 0, 0}}, 2};
    struct text out = {0};
    struct text error = {0};

    CHECK(!join(&a, &a, &out, &error));
    CHECK(strstr(text_cstr(&error), "foo") != NULL);
    error.length = 0;
    CHECK(!join(&a, &arm, &out, &error));
    CHECK(error.length > 0);
    error.length = 0;
    CHECK(!join(&single, &single, &out, &error));
    CHECK(strstr(text_cstr(&error), "once") != NULL);
    text_free(&out);
    text_free(&error);
}

/* Join two objects given as bytes. */
static bool join_bytes(const struct text *first, const struct text *second,
                       struct text *out, struct text *error)
{
    struct coff_input inputs[2];

    inputs[0].name = "a.o";
    inputs[0].data = (const unsigned char *)first->data;
    inputs[0].size = first->length;
    inputs[1].name = "b.o";
    inputs[1].data = (const unsigned char *)second->data;
    inputs[1].size = second->length;
    return coff_join(inputs, 2, out, error);
}

static void patch32(struct text *t, size_t at, uint32_t v)
{
    size_t i;

    for (i = 0; i < 4; i++) {
        t->data[at + i] = (char)(v >> (8 * i));
    }
}

/* A section marked uninitialised holds no bytes, so the join reads none
   at its raw pointer, even for a .drectve or a .debug$S. */
static void test_uninitialised(void)
{
    struct t_object a = {AMD64, {{".text", CODE, call, 8, {{0}}, 0},
                                 {".drectve", INFO, "x", 1, {{0}}, 0},
                                 {".debug$S", DEBUG, "\4\0\0\0", 4, {{0}}, 0}}, 3,
                         {{"foo", 0, 1, EXTERNAL, false, 0, 0}}, 1};
    struct t_object b = {AMD64, {{".text", CODE, call, 8, {{0}}, 0}}, 1,
                         {{"bar", 0, 1, EXTERNAL, false, 0, 0}}, 1};
    struct text first = {0};
    struct text second = {0};
    struct text out = {0};
    struct text error = {0};
    size_t n;

    build(&a, &first);
    build(&b, &second);
    for (n = 2; n <= 3; n++) {
        size_t h = section_at(n);
        patch32(&first, h + 16, 0xFFFFFFF0u);
        patch32(&first, h + 20, 0xFFFFFFF0u);
        patch32(&first, h + 36, get(&first, h + 36, 4) | 0x80u);
    }
    CHECK(join_bytes(&first, &second, &out, &error));
    CHECK_STR(text_cstr(&error), "");
    for (n = 1; out.length > 0 && n <= sections_of(&out); n++) {
        CHECK(get(&out, section_at(n) + 20, 4) < out.length);
    }
    text_free(&first);
    text_free(&second);
    text_free(&out);
    text_free(&error);
}

/* The long name of a section is `/` and the decimal offset in the string
   table, up to the first NUL of the 7 bytes. Any other byte there is
   refused, where strtoul would skip a space, take a sign or stop early. */
static void test_long_names(void)
{
    static const char *const refused[] = {" 4", "+4", "4x", "4 ", "", "0x4"};
    struct t_object a = {AMD64, {{".text", CODE, call, 8, {{0}}, 0},
                                 {".rdata$long_name", DATA, "u", 1, {{0}}, 0}}, 2,
                         {{"foo", 0, 1, EXTERNAL, false, 0, 0}}, 1};
    struct t_object b = {AMD64, {{".text", CODE, call, 8, {{0}}, 0}}, 1,
                         {{"bar", 0, 1, EXTERNAL, false, 0, 0}}, 1};
    struct text first = {0};
    struct text second = {0};
    struct text out = {0};
    struct text error = {0};
    size_t h = section_at(2);
    char name[NAME_SIZE];
    size_t i;

    build(&a, &first);
    build(&b, &second);
    memcpy(first.data + h, "/0000004", 8);
    CHECK(join_bytes(&first, &second, &out, &error));
    CHECK_STR(text_cstr(&error), "");
    if (out.length > 0) {
        section_name(&out, 2, name);
        CHECK_STR(name, ".rdata$long_name");
    }
    for (i = 0; i < sizeof refused / sizeof refused[0]; i++) {
        memset(first.data + h + 1, 0, 7);
        memcpy(first.data + h + 1, refused[i], strlen(refused[i]));
        out.length = 0;
        error.length = 0;
        CHECK(!join_bytes(&first, &second, &out, &error));
        CHECK(strstr(text_cstr(&error), "names a section it lacks") != NULL);
    }
    text_free(&first);
    text_free(&second);
    text_free(&out);
    text_free(&error);
}

/* An object of count empty sections and no symbols. */
static void build_sections(size_t count, struct text *out)
{
    size_t i;

    put16(out, AMD64);
    put16(out, (uint16_t)count);
    put32(out, 0);
    put32(out, (uint32_t)(20 + 40 * count));
    put32(out, 0);
    put32(out, 0);
    for (i = 0; i < count; i++) {
        text_append_bytes(out, ".data\0\0\0", 8);
        put32(out, 0);
        put32(out, 0);
        put32(out, 0);
        put32(out, 0);
        put32(out, 0);
        put32(out, 0);
        put32(out, 0);
        put32(out, DATA);
    }
    put32(out, 4);
}

/* The section count of the header takes 16 bits, and a section number of
   a symbol stops at 0xFEFF below the reserved -2 and -1. A join of more
   sections is refused. */
static void test_section_limit(void)
{
    struct text first = {0};
    struct text second = {0};
    struct text more = {0};
    struct text out = {0};
    struct text error = {0};

    build_sections(32640, &first);
    build_sections(32639, &second);
    build_sections(32640, &more);
    CHECK(join_bytes(&first, &second, &out, &error));
    CHECK_STR(text_cstr(&error), "");
    CHECK(out.length > 0 && sections_of(&out) == 0xFEFF);
    out.length = 0;
    CHECK(!join_bytes(&first, &more, &out, &error));
    CHECK(strstr(text_cstr(&error), "sections") != NULL);
    text_free(&first);
    text_free(&second);
    text_free(&more);
    text_free(&out);
    text_free(&error);
}

/* Join the first size bytes of first, copied into memory of exactly that
   size, with second. The sanitizer builds catch a read past the copy. */
static bool join_cut(const struct text *first, size_t size,
                     const struct text *second, struct text *error)
{
    struct text out = {0};
    struct coff_input inputs[2];
    unsigned char *copy = alloc_zeroed(size, 1);
    bool ok;

    if (size > 0) {
        memcpy(copy, first->data, size);
    }
    inputs[0].name = "a.o";
    inputs[0].data = copy;
    inputs[0].size = size;
    inputs[1].name = "b.o";
    inputs[1].data = (const unsigned char *)second->data;
    inputs[1].size = second->length;
    error->length = 0;
    ok = coff_join(inputs, 2, &out, error);
    free(copy);
    text_free(&out);
    return ok;
}

/* The join refuses the object, and the reason holds want. */
static void refuses(int line, const struct text *first,
                    const struct text *second, const char *want)
{
    struct text error = {0};

    if (join_cut(first, first->length, second, &error)) {
        check_failures++;
        fprintf(stderr, "line %d: a damaged object was joined\n", line);
    } else if (strstr(text_cstr(&error), want) == NULL) {
        check_failures++;
        fprintf(stderr, "line %d: the refusal does not say `%s`: %s\n", line,
                want, text_cstr(&error));
    }
    text_free(&error);
}

#define REFUSES(first, second, want) \
    refuses(__LINE__, first, second, want)

/* An object cut off anywhere loses the end of its string table, and the
   join refuses it. Each table, relocation and record that points past
   the data, and each index past its table, is refused with a reason. */
static void test_truncated(void)
{
    struct t_object a = {AMD64, {{".text", CODE, call, 8, {{1, 2}, {1, 3}}, 2},
                                 {".data$a_long_name", DATA, "u", 1, {{0}}, 0}}, 2,
                         {{".text", 0, 1, STATIC, true, 0, 0},
                          {"a_long_function_name", 0, 0, EXTERNAL, false, 0, 0},
                          {"printf", 0, 0, EXTERNAL, false, 0, 0},
                          {"foo", 0, 1, EXTERNAL, false, 0, 0}}, 4};
    struct t_object b = {AMD64, {{".text", CODE, call, 8, {{0}}, 0}}, 1,
                         {{"a_long_function_name", 0, 1, EXTERNAL, false, 0, 0}}, 1};
    struct text good = {0};
    struct text first = {0};
    struct text second = {0};
    struct text error = {0};
    size_t symbols;
    size_t strings;
    size_t cut;

    build(&a, &good);
    build(&b, &second);
    CHECK(join_cut(&good, good.length, &second, &error));
    CHECK_STR(text_cstr(&error), "");
    for (cut = 0; cut < good.length; cut++) {
        CHECK(!join_cut(&good, cut, &second, &error));
        CHECK(error.length > 0);
    }
    symbols = get(&good, 8, 4);
    strings = symbols + 18 * symbols_of(&good);

#define DAMAGED(at, value, want)                         \
    do {                                                 \
        first.length = 0;                                \
        text_append_bytes(&first, good.data, good.length); \
        patch32(&first, at, value);                      \
        REFUSES(&first, &second, want);                  \
    } while (0)

    /* The symbol table outside the data, or more symbols than it holds. */
    DAMAGED(8, (uint32_t)good.length + 1, "is no COFF object");
    DAMAGED(8, 0xFFFFFFF0u, "is no COFF object");
    DAMAGED(12, (uint32_t)((good.length - symbols) / 18 + 1),
            "is no COFF object");
    DAMAGED(12, 0xFFFFFFFFu, "is no COFF object");
    /* The string table: a size below its own field, and one past the end. */
    DAMAGED(strings, 3, "damaged string table");
    DAMAGED(strings, (uint32_t)(good.length - strings) + 1,
            "damaged string table");
    /* More sections than the data holds headers for. */
    first.length = 0;
    text_append_bytes(&first, good.data, good.length);
    first.data[2] = (char)0xF0;
    REFUSES(&first, &second, "is no COFF object");
    /* The raw data, the relocations and the count of relocations of a
       section outside the data. */
    DAMAGED(section_at(1) + 20, (uint32_t)good.length - 4, "has a section .text");
    DAMAGED(section_at(1) + 16, (uint32_t)good.length, "has a section .text");
    DAMAGED(section_at(1) + 24, (uint32_t)good.length - 10,
            "has a section .text");
    DAMAGED(section_at(1) + 24, 0xFFFFFFF0u, "has a section .text");
    DAMAGED(section_at(1) + 32, 0xFFFF, "has a section .text");
    /* The overflow form of the count: the first relocation holds the
       count, here past the data or the relocations themselves past it. */
    first.length = 0;
    text_append_bytes(&first, good.data, good.length);
    patch32(&first, section_at(1) + 32, 0xFFFF);
    patch32(&first, section_at(1) + 36, CODE | 0x01000000u);
    patch32(&first, get(&good, section_at(1) + 24, 4), 0x7FFFFFFF);
    REFUSES(&first, &second, "has a section .text");
    patch32(&first, section_at(1) + 24, (uint32_t)good.length - 2);
    REFUSES(&first, &second, "has a section .text");
    /* A long name of a section past the string table. */
    first.length = 0;
    text_append_bytes(&first, good.data, good.length);
    memcpy(first.data + section_at(2), "/999\0\0\0\0", 8);
    REFUSES(&first, &second, "names a section it lacks");
    /* An auxiliary count past the table, a symbol name past the strings
       and a section number past the sections. The last record is foo. */
    first.length = 0;
    text_append_bytes(&first, good.data, good.length);
    first.data[symbols + 18 * 4 + 17] = 1;
    REFUSES(&first, &second, "damaged symbol table");
    DAMAGED(symbols + 18 * 2 + 4, (uint32_t)(good.length - strings),
            "damaged symbol table");
    DAMAGED(symbols + 18 * 2 + 4, 2, "damaged symbol table");
    first.length = 0;
    text_append_bytes(&first, good.data, good.length);
    first.data[symbols + 18 * 4 + 12] = 3;
    REFUSES(&first, &second, "a section number outside the object for foo");
    /* 0xFF00 to 0xFFFD name no section and no special value. */
    first.data[symbols + 18 * 4 + 12] = 0x00;
    first.data[symbols + 18 * 4 + 13] = (char)0xFF;
    REFUSES(&first, &second, "a section number outside the object for foo");
    first.data[symbols + 18 * 4 + 12] = (char)0xFD;
    REFUSES(&first, &second, "a section number outside the object for foo");
    /* A relocation of a symbol past the table. */
    DAMAGED(get(&good, section_at(1) + 24, 4) + 4, 5, "names a symbol");
    DAMAGED(get(&good, section_at(1) + 24, 4) + 4, 0xFFFFFFFFu,
            "names a symbol");
#undef DAMAGED
    text_free(&good);
    text_free(&first);
    text_free(&second);
    text_free(&error);
}

/* An associative COMDAT names its parent section by number, and a number
   past the sections is refused. */
static void test_associative_parent(void)
{
    struct t_object a = {AMD64, {{".text", CODE | COMDAT, call, 8, {{0}}, 0},
                                 {".xdata", RDATA | COMDAT, "u", 1, {{0}}, 0}}, 2,
                         {{".text", 0, 1, STATIC, true, ANY, 0},
                          {"foo", 0, 1, EXTERNAL, false, 0, 0},
                          {".xdata", 0, 2, STATIC, true, ASSOCIATIVE, 1}}, 3};
    struct t_object b = {AMD64, {{".text", CODE, call, 8, {{0}}, 0}}, 1,
                         {{"bar", 0, 1, EXTERNAL, false, 0, 0}}, 1};
    struct text first = {0};
    struct text second = {0};
    struct text error = {0};
    size_t parent;

    build(&a, &first);
    build(&b, &second);
    CHECK(join_cut(&first, first.length, &second, &error));
    CHECK_STR(text_cstr(&error), "");
    /* The auxiliary record of .xdata follows its symbol, the fifth. */
    parent = get(&first, 8, 4) + 18 * 4 + 12;
    first.data[parent] = 3;
    REFUSES(&first, &second, "names no section");
    first.data[parent] = 0;
    REFUSES(&first, &second, "names no section");
    text_free(&first);
    text_free(&second);
    text_free(&error);
}

/* The header of an archive member: its name, then a size field of ten
   bytes, and zeros and spaces in the other fields. */
static void member_header(struct text *t, const char *name, const char *size)
{
    char header[61];

    snprintf(header, sizeof header, "%-16s%-12s%-6s%-6s%-8s%-10s`\n", name,
             "0", "0", "0", "644", size);
    text_append_bytes(t, header, 60);
}

/* An archive whose index names foo in one member and bar in the next. The
   member of foo needs bar. Its members follow without padding, since
   both objects are of even size. */
static void build_archive(struct text *out, size_t *index_at, size_t *foo_at,
                          size_t *bar_at)
{
    struct t_object foo = {AMD64, {{".text", CODE, call, 8, {{1, 1}}, 1}}, 1,
                           {{"foo", 0, 1, EXTERNAL, false, 0, 0},
                            {"bar", 0, 0, EXTERNAL, false, 0, 0}}, 2};
    struct t_object bar = {AMD64, {{".text", CODE, call, 8, {{0}}, 0}}, 1,
                           {{"bar", 0, 1, EXTERNAL, false, 0, 0},
                            {"baz", 0, 1, STATIC, false, 0, 0}}, 2};
    struct text first = {0};
    struct text second = {0};
    char size[16];
    unsigned char word[4];
    size_t at;

    build(&foo, &first);
    build(&bar, &second);
    CHECK(first.length % 2 == 0 && second.length % 2 == 0);
    text_append(out, "!<arch>\n");
    *index_at = out->length;
    member_header(out, "/", "20");
    at = 8 + 60 + 20;
    *foo_at = at;
    *bar_at = at + 60 + first.length;
    word[0] = 0;
    word[1] = 0;
    word[2] = 0;
    word[3] = 2;
    text_append_bytes(out, word, 4);
    word[3] = (unsigned char)*foo_at;
    text_append_bytes(out, word, 4);
    word[3] = (unsigned char)*bar_at;
    word[2] = (unsigned char)(*bar_at >> 8);
    text_append_bytes(out, word, 4);
    text_append_bytes(out, "foo\0bar\0", 8);
    snprintf(size, sizeof size, "%zu", first.length);
    member_header(out, "foo.obj/", size);
    text_append_bytes(out, first.data, first.length);
    snprintf(size, sizeof size, "%zu", second.length);
    member_header(out, "bar.obj/", size);
    text_append_bytes(out, second.data, second.length);
    text_free(&first);
    text_free(&second);
}

/* The exports of the first size bytes of archive, copied into memory of
   exactly that size, for an object that needs foo. */
static bool exports_of(const struct text *archive, size_t size,
                       struct text *out)
{
    struct t_object user = {AMD64, {{".text", CODE, call, 8, {{1, 0}}, 1}}, 1,
                            {{"foo", 0, 0, EXTERNAL, false, 0, 0}}, 1};
    struct text object = {0};
    unsigned char *copy = alloc_zeroed(size, 1);
    bool ok;

    if (size > 0) {
        memcpy(copy, archive->data, size);
    }
    build(&user, &object);
    out->length = 0;
    ok = coff_archive_exports(copy, size, (const unsigned char *)object.data,
                              object.length, out);
    free(copy);
    text_free(&object);
    return ok;
}

/* The archive index of coff_archive_exports: the closure from the object
   through the index, then every damage of the index and of a member
   header. */
static void test_archive_index(void)
{
    static const char *const sizes[] = {"+%zu", " %zu", "%zux", "%zu 1",
                                        "-%zu", "", "0x%zx", "99999999999"};
    struct text archive = {0};
    struct text damaged = {0};
    struct text out = {0};
    size_t index_at;
    size_t foo_at;
    size_t bar_at;
    size_t cut;
    size_t i;

    build_archive(&archive, &index_at, &foo_at, &bar_at);
    CHECK(exports_of(&archive, archive.length, &out));
    CHECK_STR(text_cstr(&out), "foo\nbar\n");
    /* Every archive cut short loses a member that the closure reads. */
    for (cut = 0; cut < archive.length; cut++) {
        CHECK(!exports_of(&archive, cut, &out));
    }

#define DAMAGE(at, bytes, n)                                    \
    do {                                                        \
        damaged.length = 0;                                     \
        text_append_bytes(&damaged, archive.data, archive.length); \
        memcpy(damaged.data + (at), bytes, n);                  \
    } while (0)

    /* A section number from 0xFF00 to 0xFFFD, which names no section and
       no special value, here on bar of the member of foo. */
    DAMAGE(foo_at + 60 + get(&archive, foo_at + 60 + 8, 4) + 18 + 12,
           "\x00\xff", 2);
    CHECK(!exports_of(&damaged, damaged.length, &out));
    DAMAGE(foo_at + 60 + get(&archive, foo_at + 60 + 8, 4) + 18 + 12,
           "\xfd\xff", 2);
    CHECK(!exports_of(&damaged, damaged.length, &out));
    /* No archive, and a first member that is no index. */
    DAMAGE(0, "!<arch>?", 8);
    CHECK(!exports_of(&damaged, damaged.length, &out));
    DAMAGE(index_at, "x ", 2);
    CHECK(!exports_of(&damaged, damaged.length, &out));
    /* A count of names past the index, and names without their NUL. */
    DAMAGE(index_at + 60, "\0\0\0\4", 4);
    CHECK(!exports_of(&damaged, damaged.length, &out));
    DAMAGE(index_at + 60, "\xff\xff\xff\xff", 4);
    CHECK(!exports_of(&damaged, damaged.length, &out));
    DAMAGE(index_at + 60 + 19, "x", 1);
    CHECK(!exports_of(&damaged, damaged.length, &out));
    /* An index shorter than its count field. */
    DAMAGE(index_at + 48, "3         ", 10);
    CHECK(!exports_of(&damaged, damaged.length, &out));
    /* A member offset past the archive, and one whose header would end
       past it. */
    DAMAGE(index_at + 60 + 8, "\xff\xff\xff\xf0", 4);
    CHECK(!exports_of(&damaged, damaged.length, &out));
    DAMAGE(index_at + 60 + 8, "\0\0\0\0", 4);
    damaged.data[index_at + 60 + 8 + 2] = (char)((archive.length - 30) >> 8);
    damaged.data[index_at + 60 + 8 + 3] = (char)(archive.length - 30);
    CHECK(!exports_of(&damaged, damaged.length, &out));
    /* A size field of a member that is no plain decimal, each around the
       size of the member, or past the archive. */
    for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        char size[32];
        char field[11];
        snprintf(size, sizeof size, sizes[i], archive.length - bar_at - 60);
        snprintf(field, sizeof field, "%-10s", size);
        DAMAGE(bar_at + 48, field, 10);
        if (exports_of(&damaged, damaged.length, &out)) {
            check_failures++;
            fprintf(stderr, "the size field `%s` was read\n", field);
        }
    }
    /* A member whose size cuts its object short. */
    DAMAGE(bar_at + 48, "40        ", 10);
    CHECK(!exports_of(&damaged, damaged.length, &out));
#undef DAMAGE
    text_free(&archive);
    text_free(&damaged);
    text_free(&out);
}

void test_coff(void)
{
    test_across();
    test_directives();
    test_comdat();
    test_features();
    test_codeview();
    test_refusals();
    test_uninitialised();
    test_long_names();
    test_section_limit();
    test_truncated();
    test_associative_parent();
    test_archive_index();
}

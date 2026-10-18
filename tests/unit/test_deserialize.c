/* Object.deserialize of src/rt/registry.c, with src/rt/object.c and the
   scanner of src/rt/json.c, against malformed and cut-off text. An Anti
   program links the runtime of the archive, which carries no sanitizer,
   so tests/std/deserialize_input.anti cannot see a read past its text.
   This program does in the sanitizer builds. The input of each case is a
   heap copy without a NUL, so AddressSanitizer reports a read past its
   end. The allocator counts its blocks, so a refused text that keeps
   one fails, and each class counts the objects prepared and torn down.
   The classes, the registry and the loader are stand-ins for what antic
   writes into a program. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../binary_stdio.h"
#include "../../src/rt/json.h"
#include "../../src/rt/object.h"
#include "../../src/rt/plugin.h"
#include "../../src/rt/registry.h"
#include "../../src/rt/std.h"
#include "check.h"

int check_failures;

/* The stand-ins of the loader of plugins. No library is open. */

void anti_rt_plugin_hold(void)
{
}

void anti_rt_plugin_release(void)
{
}

int64_t anti_rt_plugin_open(void)
{
    return 0;
}

struct anti_plugin *anti_rt_plugin_at(int64_t index)
{
    (void)index;
    return NULL;
}

const struct anti_registry *anti_rt_plugin_registry(int64_t index)
{
    (void)index;
    return NULL;
}

/* The objects whose teardown has returned, which the loader counts off
   their library. */
static int64_t destroyed;

void anti_rt_plugin_created(const void *table)
{
    (void)table;
}

void anti_rt_plugin_destroyed(const void *table)
{
    (void)table;
    destroyed++;
}

/* The allocator, an anti.mem.Allocator whose table holds alloc and
   free. Every block it gives out stays on its list until free or the
   end of the case. */

enum { BLOCKS = 4096 };

struct counting {
    struct anti_object base;
    void *blocks[BLOCKS];
    int64_t live;
};

static void *counting_alloc(struct anti_object *self, int64_t size,
                            int64_t align)
{
    struct counting *c = (struct counting *)self;
    void *p;

    CHECK(size >= 0 && align >= 1 && align <= 16);
    CHECK(c->live < BLOCKS);
    if (size < 0 || c->live >= BLOCKS) {
        return NULL;
    }
    p = malloc(size > 0 ? (size_t)size : 1);
    if (p != NULL) {
        c->blocks[c->live++] = p;
    }
    return p;
}

static void counting_free(struct anti_object *self, void *p)
{
    struct counting *c = (struct counting *)self;
    int64_t i;

    if (p == NULL) {
        return;
    }
    for (i = 0; i < c->live; i++) {
        if (c->blocks[i] == p) {
            free(p);
            c->blocks[i] = c->blocks[--c->live];
            return;
        }
    }
    check_failures++;
    fprintf(stderr, "free of a block the allocator never gave out\n");
}

/* Free what the case left with the allocator, as an arena does. */
static void free_all(struct counting *c)
{
    while (c->live > 0) {
        free(c->blocks[--c->live]);
    }
}

static const void *allocator_table[ANTI_ENTRY_FREE + 1];

/* The classes and the values of their fields, laid out as antic lays
   them out with the C rules. */

struct point {
    int64_t x;
    int64_t y;
};

struct circle {
    int64_t r;
};

struct rect {
    int64_t w;
    int64_t h;
};

struct shape {
    int64_t tag;
    union {
        struct circle circle;
        struct rect rect;
    } u;
};

struct opt_point {
    struct point value;
    uint8_t has;
};

struct opt_int {
    int64_t value;
    uint8_t has;
};

struct pair {
    int64_t n;
    struct anti_text s;
};

struct numbers {
    struct anti_object base;
    int8_t small;
    uint8_t octet;
    int64_t n;
    uint64_t wide;
    double f;
    uint16_t half;
    float single;
    uint8_t flag;
    uint32_t letter;
};

struct named {
    struct anti_object base;
    struct anti_text name;
};

struct drawing {
    struct anti_object base;
    struct shape shape;
    struct opt_point at;
    struct opt_int size;
    struct anti_text label;
    struct pair pair;
    int32_t grid[2][3];
    int8_t row[3];
};

struct node {
    struct anti_object base;
    int64_t value;
    struct node *next;
    struct anti_text items;
    int64_t *boxed;
    int64_t *plain;
    void (*callback)(void);
};

#define TEXT(s) (const unsigned char *)(s), (int64_t)(sizeof(s) - 1)
#define FIELD(name, type, member, t, owned, d) \
    {TEXT(name), (int64_t)offsetof(type, member), (int64_t)(t), owned, d}
#define TYPE2(t, e) ((int64_t)(t) | (int64_t)(e) << 8)
#define ARRAY(e, inner, count)                                   \
    ((int64_t)ANTI_TYPE_ARRAY | (int64_t)(e) << 8 |              \
     (int64_t)(inner) << 16 | (int64_t)(count) << 24)
/* A struct, a variant, a `?T`, a tuple or the levels of an array. */
#define VALUE(name, size, fields)                                         \
    {TEXT(name), NULL, (int64_t)(size), 0, NULL,                          \
     (int64_t)(sizeof(fields) / sizeof(fields)[0]), fields, NULL, 0, 0,   \
     NULL, NULL, 0, NULL, 0, NULL}
/* A class below the root, at depth 1. */
#define CLASS(name, size, ancestors, fields, count)                      \
    {TEXT(name), &anti_lang_Object_descriptor, (int64_t)(size), 1,       \
     ancestors, count, fields, NULL, 0, 0, NULL, NULL, 0, NULL, 0, NULL}

static const struct anti_field point_fields[] = {
    FIELD("x", struct point, x, ANTI_TYPE_I64, 0, NULL),
    FIELD("y", struct point, y, ANTI_TYPE_I64, 0, NULL),
};
static const struct anti_descriptor point_d =
    VALUE("Point", sizeof(struct point), point_fields);

static const struct anti_field circle_fields[] = {
    FIELD("r", struct circle, r, ANTI_TYPE_I64, 0, NULL),
};
static const struct anti_descriptor circle_d =
    VALUE("Circle", sizeof(struct circle), circle_fields);

static const struct anti_field rect_fields[] = {
    FIELD("w", struct rect, w, ANTI_TYPE_I64, 0, NULL),
    FIELD("h", struct rect, h, ANTI_TYPE_I64, 0, NULL),
};
static const struct anti_descriptor rect_d =
    VALUE("Rect", sizeof(struct rect), rect_fields);

/* The tag, then one record per case, whose `owned` is its tag. */
static const struct anti_field shape_fields[] = {
    FIELD("tag", struct shape, tag, ANTI_TYPE_I64, 0, NULL),
    FIELD("Circle", struct shape, u, ANTI_TYPE_STRUCT, 0, &circle_d),
    FIELD("Rect", struct shape, u, ANTI_TYPE_STRUCT, 1, &rect_d),
    FIELD("Empty", struct shape, u, ANTI_TYPE_STRUCT, 2, NULL),
};
static const struct anti_descriptor shape_d =
    VALUE("Shape", sizeof(struct shape), shape_fields);

static const struct anti_field opt_point_fields[] = {
    FIELD("value", struct opt_point, value, ANTI_TYPE_STRUCT, 0, &point_d),
    FIELD("has", struct opt_point, has, ANTI_TYPE_BOOL, 0, NULL),
};
static const struct anti_descriptor opt_point_d =
    VALUE("?Point", sizeof(struct opt_point), opt_point_fields);

static const struct anti_field opt_int_fields[] = {
    FIELD("value", struct opt_int, value, ANTI_TYPE_I64, 0, NULL),
    FIELD("has", struct opt_int, has, ANTI_TYPE_BOOL, 0, NULL),
};
static const struct anti_descriptor opt_int_d =
    VALUE("?int", sizeof(struct opt_int), opt_int_fields);

static const struct anti_field pair_fields[] = {
    FIELD("0", struct pair, n, ANTI_TYPE_I64, 0, NULL),
    FIELD("1", struct pair, s, ANTI_TYPE_STR, 0, NULL),
};
static const struct anti_descriptor pair_d =
    VALUE("(int, str)", sizeof(struct pair), pair_fields);

/* The levels of [2][3]i32, the length of each in `owned`. */
static const struct anti_field grid_levels[] = {
    {TEXT("0"), 0, ANTI_TYPE_I32, 2, NULL},
    {TEXT("1"), 0, ANTI_TYPE_I32, 3, NULL},
};
static const struct anti_descriptor grid_d =
    VALUE("[2][3]i32", sizeof(int32_t[2][3]), grid_levels);

static const struct anti_field numbers_fields[] = {
    FIELD("small", struct numbers, small, ANTI_TYPE_I8, 0, NULL),
    FIELD("octet", struct numbers, octet, ANTI_TYPE_U8, 0, NULL),
    FIELD("n", struct numbers, n, ANTI_TYPE_I64, 0, NULL),
    FIELD("wide", struct numbers, wide, ANTI_TYPE_U64, 0, NULL),
    FIELD("f", struct numbers, f, ANTI_TYPE_F64, 0, NULL),
    FIELD("half", struct numbers, half, ANTI_TYPE_F16, 0, NULL),
    FIELD("single", struct numbers, single, ANTI_TYPE_F32, 0, NULL),
    FIELD("flag", struct numbers, flag, ANTI_TYPE_BOOL, 0, NULL),
    FIELD("letter", struct numbers, letter, ANTI_TYPE_CHAR, 0, NULL),
};

static const struct anti_field named_fields[] = {
    FIELD("name", struct named, name, ANTI_TYPE_STR, 0, NULL),
};

static const struct anti_field drawing_fields[] = {
    FIELD("shape", struct drawing, shape, ANTI_TYPE_VARIANT, 0, &shape_d),
    FIELD("at", struct drawing, at, ANTI_TYPE_OPTIONAL, 0, &opt_point_d),
    FIELD("size", struct drawing, size, ANTI_TYPE_OPTIONAL, 0, &opt_int_d),
    FIELD("label", struct drawing, label, ANTI_TYPE_STR, 0, NULL),
    FIELD("pair", struct drawing, pair, ANTI_TYPE_TUPLE, 0, &pair_d),
    FIELD("grid", struct drawing, grid,
          ARRAY(ANTI_TYPE_ARRAY, ANTI_TYPE_I32, 6), 0, &grid_d),
    FIELD("row", struct drawing, row, ARRAY(ANTI_TYPE_I8, ANTI_TYPE_I8, 3),
          0, NULL),
};

static const struct anti_descriptor node_d;

static const struct anti_field node_fields[] = {
    FIELD("value", struct node, value, ANTI_TYPE_I64, 0, NULL),
    FIELD("next", struct node, next, TYPE2(ANTI_TYPE_PTR, ANTI_TYPE_CLASS), 1,
          &node_d),
    FIELD("items", struct node, items, TYPE2(ANTI_TYPE_SLICE, ANTI_TYPE_I32),
          1, NULL),
    FIELD("boxed", struct node, boxed, TYPE2(ANTI_TYPE_PTR, ANTI_TYPE_I64), 1,
          NULL),
    FIELD("plain", struct node, plain, TYPE2(ANTI_TYPE_PTR, ANTI_TYPE_I64), 0,
          NULL),
    FIELD("callback", struct node, callback, ANTI_TYPE_FN, 0, NULL),
};

#define CLASSES 7

/* The classes of the registry, in its order. */
enum {
    NUMBERS, NAMED, DRAWING, NODE, TWIN_A, TWIN_B, REQUIRED
};

static const struct anti_descriptor numbers_d, named_d, drawing_d, twin_a_d,
    twin_b_d, required_d;
static const struct anti_descriptor *const numbers_up[2] = {
    &anti_lang_Object_descriptor, &numbers_d};
static const struct anti_descriptor *const named_up[2] = {
    &anti_lang_Object_descriptor, &named_d};
static const struct anti_descriptor *const drawing_up[2] = {
    &anti_lang_Object_descriptor, &drawing_d};
static const struct anti_descriptor *const node_up[2] = {
    &anti_lang_Object_descriptor, &node_d};
static const struct anti_descriptor *const twin_a_up[2] = {
    &anti_lang_Object_descriptor, &twin_a_d};
static const struct anti_descriptor *const twin_b_up[2] = {
    &anti_lang_Object_descriptor, &twin_b_d};
static const struct anti_descriptor *const required_up[2] = {
    &anti_lang_Object_descriptor, &required_d};

static const struct anti_descriptor numbers_d =
    CLASS("Numbers", sizeof(struct numbers), numbers_up, numbers_fields, 9);
static const struct anti_descriptor named_d =
    CLASS("Named", sizeof(struct named), named_up, named_fields, 1);
static const struct anti_descriptor drawing_d =
    CLASS("Drawing", sizeof(struct drawing), drawing_up, drawing_fields, 7);
static const struct anti_descriptor node_d =
    CLASS("Node", sizeof(struct node), node_up, node_fields, 6);
/* Two modules declare a class of one name. */
static const struct anti_descriptor twin_a_d =
    CLASS("Twin", sizeof(struct named), twin_a_up, named_fields, 1);
static const struct anti_descriptor twin_b_d =
    CLASS("Twin", sizeof(struct named), twin_b_up, named_fields, 1);
/* A class with an inline class field that no default fills. */
static const struct anti_descriptor required_d =
    CLASS("Required", sizeof(struct named), required_up, named_fields, 1);

static const void *tables[CLASSES][ANTI_ENTRY_HOOK];

/* The objects each class prepared, and the teardowns that ran. */
static int64_t prepared;
static int64_t torn_down;

static void prepare(void *object, int which)
{
    ((struct anti_object *)object)->table =
        (const struct anti_descriptor *const *)tables[which];
    prepared++;
}

static void init_numbers(void *object)
{
    prepare(object, NUMBERS);
}

static void init_named(void *object)
{
    prepare(object, NAMED);
}

static void init_drawing(void *object)
{
    prepare(object, DRAWING);
}

static void init_node(void *object)
{
    prepare(object, NODE);
}

static void init_twin_a(void *object)
{
    prepare(object, TWIN_A);
}

static void init_twin_b(void *object)
{
    prepare(object, TWIN_B);
}

static void init_required(void *object)
{
    prepare(object, REQUIRED);
}

/* The teardown antic writes for a class: what a Node owns goes back to
   from. A str is no field's to free. */
static void teardown(struct anti_object *self, struct anti_object *from)
{
    torn_down++;
    if (anti_rt_descriptor(self) == &node_d) {
        struct node *n = (struct node *)self;
        anti_rt_delete_from(n->next, &node_d, from);
        anti_rt_give(from, (void *)(uintptr_t)n->items.ptr);
        anti_rt_give(from, n->boxed);
    }
}

static const struct anti_class classes[CLASSES] = {
    {&numbers_d, init_numbers, TEXT("app"), 0},
    {&named_d, init_named, TEXT("app"), 0},
    {&drawing_d, init_drawing, TEXT("app"), 0},
    {&node_d, init_node, TEXT("app"), ANTI_CLASS_ARGS},
    {&twin_a_d, init_twin_a, TEXT("a"), 0},
    {&twin_b_d, init_twin_b, TEXT("b"), 0},
    {&required_d, init_required, TEXT("app"), ANTI_CLASS_REQUIRED},
};

const struct anti_registry anti_rt_registry = {CLASSES, classes};

static void make_tables(void)
{
    int i;

    allocator_table[ANTI_ENTRY_ALLOC] =
        anti_rt_body_entry((anti_rt_body)counting_alloc);
    allocator_table[ANTI_ENTRY_FREE] =
        anti_rt_body_entry((anti_rt_body)counting_free);
    for (i = 0; i < CLASSES; i++) {
        tables[i][ANTI_ENTRY_DESCRIPTOR] = classes[i].descriptor;
        tables[i][ANTI_ENTRY_SERIALIZE] =
            anti_rt_body_entry((anti_rt_body)anti_lang_Object_serialize);
        tables[i][ANTI_ENTRY_DROP] =
            anti_rt_body_entry((anti_rt_body)teardown);
    }
}

/* Reading */

static struct counting *allocator;

/* Read the len bytes of text from a heap copy of exactly that size. A
   refused text leaves no block with the allocator and every object it
   prepared torn down. An object that came back serializes into out, when
   out is not NULL, and goes back with what it owns. */
static bool read_bytes(const char *text, size_t len, struct anti_builder *out)
{
    unsigned char *copy = malloc(len > 0 ? len : 1);
    struct anti_text input;
    int64_t made = prepared;
    int64_t gone = torn_down;
    int64_t counted = destroyed;
    void *object;

    if (copy == NULL) {
        check_failures++;
        fprintf(stderr, "no memory for the input\n");
        return false;
    }
    memcpy(copy, text, len);
    input.ptr = copy;
    input.len = (int64_t)len;
    allocator->base.table =
        (const struct anti_descriptor *const *)allocator_table;
    object = anti_lang_Object_deserialize(input, &allocator->base);
    free(copy);
    if (object == NULL) {
        CHECK(allocator->live == 0);
        CHECK(torn_down - gone == prepared - made);
        CHECK(destroyed - counted == prepared - made);
        free_all(allocator);
        return false;
    }
    if (out != NULL) {
        anti_rt_builder_clear(out);
        anti_lang_Object_serialize(object, out);
    }
    anti_rt_delete_from(object, NULL, &allocator->base);
    CHECK(torn_down - gone == prepared - made);
    free_all(allocator);
    return true;
}

static bool reads(const char *text)
{
    return read_bytes(text, strlen(text), NULL);
}

/* Whether text reads back, and serializes as expected. */
static void reads_as(const char *text, const char *expected)
{
    struct anti_builder b = {0};
    struct anti_text got;
    bool read = read_bytes(text, strlen(text), &b);

    if (!read) {
        check_failures++;
        fprintf(stderr, "refused %s\n", text);
    }
    got = anti_rt_builder_text(&b);
    if (read && ((size_t)got.len != strlen(expected) ||
                 memcmp(got.ptr, expected, strlen(expected)) != 0)) {
        check_failures++;
        fprintf(stderr, "expected\n%s\ngot\n%.*s\n", expected, (int)got.len,
                (const char *)got.ptr);
    }
    free(b.room);
}

/* Every prefix of text is refused, and the whole of it reads back as
   itself. */
static void cut_everywhere(const char *text)
{
    size_t len = strlen(text);
    size_t cut;

    for (cut = 0; cut < len; cut++) {
        if (read_bytes(text, cut, NULL)) {
            check_failures++;
            fprintf(stderr, "a cut at %zu of %s read\n", cut, text);
        }
    }
    reads_as(text, text);
}

/* The cases of tests/std/deserialize_input.anti. */
static void numbers(void)
{
    CHECK(!reads("{\"type\":\"Numbers\",\"n\":+5}"));
    CHECK(!reads("{\"type\":\"Numbers\",\"n\":007}"));
    CHECK(reads("{\"type\":\"Numbers\",\"n\":-0}"));
    CHECK(!reads("{\"type\":\"Numbers\",\"n\":1.0}"));
    CHECK(!reads("{\"type\":\"Numbers\",\"n\":1e2}"));
    CHECK(!reads("{\"type\":\"Numbers\",\"n\":-}"));
    CHECK(!reads("{\"type\":\"Numbers\",\"n\":--1}"));
    CHECK(reads("{\"type\":\"Numbers\",\"n\":-9223372036854775808}"));
    CHECK(!reads("{\"type\":\"Numbers\",\"n\":9223372036854775808}"));
    CHECK(reads("{\"type\":\"Numbers\",\"small\":-128}"));
    CHECK(!reads("{\"type\":\"Numbers\",\"small\":-129}"));
    CHECK(!reads("{\"type\":\"Numbers\",\"small\":128}"));
    CHECK(reads("{\"type\":\"Numbers\",\"octet\":255}"));
    CHECK(!reads("{\"type\":\"Numbers\",\"octet\":256}"));
    CHECK(!reads("{\"type\":\"Numbers\",\"octet\":-1}"));
    CHECK(!reads("{\"type\":\"Numbers\",\"octet\":+0}"));
    CHECK(reads("{\"type\":\"Numbers\",\"wide\":18446744073709551615}"));
    CHECK(!reads("{\"type\":\"Numbers\",\"wide\":18446744073709551616}"));
    CHECK(reads("{\"type\":\"Numbers\",\"f\":-1.5e+2}"));
    CHECK(!reads("{\"type\":\"Numbers\",\"f\":+1.5}"));
    CHECK(!reads("{\"type\":\"Numbers\",\"f\":.5}"));
    CHECK(!reads("{\"type\":\"Numbers\",\"f\":01.5}"));
    CHECK(!reads("{\"type\":\"Numbers\",\"f\":1.}"));
    CHECK(!reads("{\"type\":\"Numbers\",\"f\":1e+}"));
    CHECK(!reads("{\"type\":\"Numbers\",\"flag\":tru}"));
    CHECK(!reads("{\"type\":\"Numbers\",\"letter\":\"ab\"}"));
    CHECK(!reads("{\"type\":\"Numbers\",\"letter\":\"\"}"));
    CHECK(reads("{\"type\":\"Numbers\",\"letter\":\"\\u00e9\"}"));
    cut_everywhere("{\"type\":\"Numbers\",\"small\":-12,\"octet\":200,"
                   "\"n\":-9223372036854775808,\"wide\":18446744073709551615,"
                   "\"f\":-1.5e-300,\"half\":0.5,\"single\":3.25,"
                   "\"flag\":true,\"letter\":\"\xc3\xa9\"}");
}

static void texts(void)
{
    reads_as("{\"type\":\"Named\",\"name\":\"\\u00e9\"}",
             "{\"type\":\"Named\",\"name\":\"\xc3\xa9\"}");
    CHECK(!reads("{\"type\":\"Named\",\"name\":\"a\\u0000b\"}"));
    CHECK(!reads("{\"type\":\"Named\",\"name\":\"a\xff" "b\"}"));
    CHECK(!reads("{\"type\":\"Named\",\"name\":\"a\xc3\"}"));
    CHECK(!reads("{\"type\":\"Named\",\"name\":\"a\x80\"}"));
    /* A \u escape of a surrogate half is refused, in a pair as well,
       by the DESIGN of src/rt/json.c. The serializer writes UTF-8. */
    CHECK(!reads("{\"type\":\"Named\",\"name\":\"\\ud83d\\ude00\"}"));
    reads_as("{\"type\":\"Named\",\"name\":\"\xf0\x9f\x98\x80\"}",
             "{\"type\":\"Named\",\"name\":\"\xf0\x9f\x98\x80\"}");
    CHECK(!reads("{\"type\":\"Named\",\"name\":\"\\ud83d\"}"));
    CHECK(!reads("{\"type\":\"Named\",\"name\":\"\\ude00\"}"));
    CHECK(!reads("{\"type\":\"Named\",\"name\":\"\\ud83dx\"}"));
    cut_everywhere("{\"type\":\"Named\",\"name\":\"a\\\"b\\\\c\\n\\u0001"
                   "\xc3\xa9\"}");
}

static void variants(void)
{
    reads_as("{\"type\":\"Drawing\",\"shape\":{\"Rect\":{\"w\":2}}}",
             "{\"type\":\"Drawing\",\"shape\":{\"Rect\":{\"w\":2,\"h\":0}},"
             "\"at\":null,\"size\":null,\"label\":\"\",\"pair\":[0,\"\"],"
             "\"grid\":[[0,0,0],[0,0,0]],\"row\":[0,0,0]}");
    CHECK(reads("{\"type\":\"Drawing\",\"shape\":null}"));
    CHECK(!reads("{\"type\":\"Drawing\",\"shape\":{\"Square\":{}}}"));
    CHECK(!reads("{\"type\":\"Drawing\",\"shape\":{}}"));
    CHECK(!reads("{\"type\":\"Drawing\",\"shape\":\"Circle\"}"));
    CHECK(!reads("{\"type\":\"Drawing\",\"shape\":{\"Circle\":1}}"));
    CHECK(!reads("{\"type\":\"Drawing\",\"shape\":{\"Circle\":{\"r\":\"x\"}}}"));
    CHECK(!reads(
        "{\"type\":\"Drawing\",\"shape\":{\"Circle\":{\"r\":1,\"r\":2}}}"));
    CHECK(!reads("{\"type\":\"Drawing\",\"shape\":{\"Circle\":{\"r\":1},"
                 "\"Empty\":{}}}"));
    CHECK(!reads("{\"type\":\"Drawing\",\"shape\":{\"Empty\":{\"r\":1}}}"));
    CHECK(!reads("{\"type\":\"Drawing\",\"shape\":{\"Empty\":[]}}"));
    CHECK(reads("{\"type\":\"Drawing\",\"shape\":{\"Empty\":{}}}"));
}

static void optionals_tuples_arrays(void)
{
    CHECK(reads("{\"type\":\"Drawing\",\"at\":{\"x\":1},\"size\":4}"));
    CHECK(reads("{\"type\":\"Drawing\",\"at\":null,\"size\":null}"));
    CHECK(!reads("{\"type\":\"Drawing\",\"size\":\"4\"}"));
    CHECK(!reads("{\"type\":\"Drawing\",\"size\":4.5}"));
    CHECK(!reads("{\"type\":\"Drawing\",\"size\":[4]}"));
    CHECK(!reads("{\"type\":\"Drawing\",\"size\":nul}"));
    CHECK(!reads("{\"type\":\"Drawing\",\"at\":5}"));
    CHECK(!reads("{\"type\":\"Drawing\",\"at\":[1,2]}"));
    CHECK(!reads("{\"type\":\"Drawing\",\"at\":{\"x\":\"1\"}}"));
    /* A tuple and an array hold exactly as many values as their parts. */
    CHECK(!reads("{\"type\":\"Drawing\",\"pair\":[1]}"));
    CHECK(!reads("{\"type\":\"Drawing\",\"pair\":[1,\"a\",2]}"));
    CHECK(!reads("{\"type\":\"Drawing\",\"pair\":[\"a\",1]}"));
    CHECK(!reads("{\"type\":\"Drawing\",\"row\":[1,2]}"));
    CHECK(!reads("{\"type\":\"Drawing\",\"row\":[1,2,3,4]}"));
    CHECK(!reads("{\"type\":\"Drawing\",\"row\":[1,2,128]}"));
    CHECK(!reads("{\"type\":\"Drawing\",\"grid\":[[1,2,3]]}"));
    CHECK(!reads("{\"type\":\"Drawing\",\"grid\":[[1,2,3],[4,5]]}"));
    CHECK(!reads("{\"type\":\"Drawing\",\"grid\":[1,2,3,4,5,6]}"));
    cut_everywhere("{\"type\":\"Drawing\",\"shape\":{\"Rect\":{\"w\":2,"
                   "\"h\":3}},\"at\":{\"x\":1,\"y\":2},\"size\":42,"
                   "\"label\":\"a\xc3\xa9\\\"b\",\"pair\":[7,\"seven\"],"
                   "\"grid\":[[1,2,3],[4,5,6]],\"row\":[-1,0,1]}");
}

/* What a Node owns comes from the allocator, and goes back to it when the
   text is refused after it. */
static void owned(void)
{
    cut_everywhere("{\"type\":\"Node\",\"value\":1,\"next\":{\"type\":\"Node\","
                   "\"value\":2,\"next\":null,\"items\":[4,5],\"boxed\":9,"
                   "\"plain\":null,\"callback\":null},\"items\":[1,2,3],"
                   "\"boxed\":8,\"plain\":null,\"callback\":null}");
    CHECK(reads("{\"type\":\"Node\",\"items\":[],\"boxed\":null}"));
    CHECK(reads("{\"type\":\"Node\",\"items\":null}"));
    /* An address never comes from text. */
    CHECK(!reads("{\"type\":\"Node\",\"plain\":8}"));
    CHECK(!reads("{\"type\":\"Node\",\"callback\":8}"));
    /* The class an owning field names lies at or below the field's. */
    CHECK(!reads("{\"type\":\"Node\",\"next\":{\"type\":\"Named\"}}"));
    CHECK(!reads("{\"type\":\"Node\",\"next\":{\"type\":\"Nothing\"}}"));
    CHECK(!reads("{\"type\":\"Node\",\"next\":{\"value\":1}}"));
    CHECK(!reads("{\"type\":\"Node\",\"next\":5}"));
    /* A field read twice fails, whatever the first one built. */
    CHECK(!reads("{\"type\":\"Node\",\"next\":{\"type\":\"Node\"},"
                 "\"next\":{\"type\":\"Node\"}}"));
    CHECK(!reads("{\"type\":\"Node\",\"items\":[1],\"items\":[2]}"));
    /* A failure deep in a chain gives every level back. */
    CHECK(!reads("{\"type\":\"Node\",\"next\":{\"type\":\"Node\",\"next\":"
                 "{\"type\":\"Node\",\"items\":[1,2,\"x\"]}}}"));
    CHECK(!reads("{\"type\":\"Node\",\"items\":[1,2,2147483648]}"));
}

/* The text of a chain of count Nodes, each the next of the one before. */
static char *chain(int count)
{
    static const char open[] = "{\"type\":\"Node\",\"next\":";
    size_t size = (size_t)count * (sizeof open - 1 + 1) + 5;
    char *text = malloc(size);
    char *at = text;
    int i;

    if (text == NULL) {
        return NULL;
    }
    for (i = 0; i < count; i++) {
        memcpy(at, open, sizeof open - 1);
        at += sizeof open - 1;
    }
    memcpy(at, "null", 4);
    at += 4;
    for (i = 0; i < count; i++) {
        *at++ = '}';
    }
    *at = 0;
    return text;
}

/* The members around the object, the names of the classes, and how deep
   a text may nest. */
static void classes_and_depth(void)
{
    char *deep;

    CHECK(reads(" {\"type\":\"Named\"} "));
    CHECK(!reads("{\"type\":\"Named\"} x"));
    CHECK(!reads("{\"type\":\"Named\"}{}"));
    CHECK(!reads(""));
    CHECK(!reads("null"));
    CHECK(!reads("{}"));
    CHECK(!reads("{\"name\":\"a\"}"));
    CHECK(!reads("{\"type\":1}"));
    CHECK(reads("{\"name\":\"a\",\"type\":\"Named\"}"));
    CHECK(reads("{\"type\":\"Named\",\"other\":[1,{\"a\":null}]}"));
    CHECK(!reads("{\"type\":\"Named\",\"other\":[1,{\"a\":nul}]}"));
    CHECK(!reads("{\"type\":\"Named\",\"name\":\"a\",\"name\":\"b\"}"));
    /* A bare name that two modules declare, and each by its module. */
    CHECK(!reads("{\"type\":\"Twin\"}"));
    CHECK(reads("{\"type\":\"a.Twin\"}"));
    CHECK(reads("{\"type\":\"b.Twin\"}"));
    CHECK(!reads("{\"type\":\"c.Twin\"}"));
    CHECK(!reads("{\"type\":\".Twin\"}"));
    CHECK(!reads("{\"type\":\"Twin.\"}"));
    CHECK(!reads("{\"type\":\"Required\"}"));
    CHECK(reads("{\"type\":\"app.Named\"}"));
    /* A chain inside the depth of the scanner, and one past it. */
    deep = chain(ANTI_JSON_DEPTH / 2);
    CHECK(deep != NULL && reads(deep));
    free(deep);
    deep = chain(ANTI_JSON_DEPTH * 4);
    CHECK(deep != NULL && !reads(deep));
    free(deep);
}

int main(void)
{
    struct counting *c = calloc(1, sizeof *c);

    if (c == NULL) {
        fprintf(stderr, "no memory for the allocator\n");
        return 1;
    }
    allocator = c;
    make_tables();
    numbers();
    texts();
    variants();
    optionals_tuples_arrays();
    owned();
    classes_and_depth();
    free(c);
    if (check_failures > 0) {
        fprintf(stderr, "%d checks failed\n", check_failures);
        return 1;
    }
    printf("deserialize: all checks passed\n");
    return 0;
}

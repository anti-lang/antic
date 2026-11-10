/* The walks of src/rt/object.c over records that antic does not write: a
   descriptor without its list, a variant whose tag names no case, an
   array of arrays without the descriptor of its levels, a table without
   the entry a walk calls, and the arguments of the entries of a generic
   collection outside what anti.collection passes. Each walk gives the
   text of a value no walk reads, `null`, `?` or `none`, and no copy, and
   reads nothing outside the record. */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../binary_stdio.h"
#include "../../src/rt/json.h"
#include "../../src/rt/object.h"
#include "../../src/rt/regex.h"
#include "../../src/rt/std.h"
#include "check.h"

int check_failures;

/* The stand-ins of the loader of plugins, which counts the objects a
   copy makes. No library is open. */

void anti_rt_plugin_created(const void *table)
{
    (void)table;
}

void anti_rt_plugin_destroyed(const void *table)
{
    (void)table;
}

/* A type id of a value of kind of with the kinds of its element and its
   inner element and its length, as antic writes it. */
static int64_t type_id(int64_t of, int64_t element, int64_t inner,
                       int64_t count)
{
    return of | element << 8 | inner << 16 | count << 24;
}

static const unsigned char *name(const char *s)
{
    return (const unsigned char *)s;
}

/* The text the walk wrote into b, as a C string, and b emptied. */
static char text_of[256];

static const char *taken(struct anti_builder *b)
{
    struct anti_text t = anti_rt_builder_text(b);
    size_t n = t.len < (int64_t)sizeof text_of - 1 ? (size_t)t.len
                                                   : sizeof text_of - 1;

    if (n > 0) {
        memcpy(text_of, t.ptr, n);
    }
    text_of[n] = '\0';
    anti_rt_builder_clear(b);
    return text_of;
}

/* An object of one field, whose record is the one the case names. The
   descriptor of the class is its own ancestor at depth 0. */
struct one_field {
    struct anti_object base;
    unsigned char bytes[32];
};

static struct anti_field field;
static struct anti_descriptor holder;
static const struct anti_descriptor *holder_chain[1] = {&holder};
static const struct anti_descriptor *holder_table[ANTI_ENTRY_HOOK] = {
    &holder};

static const char *serialized(struct one_field *o, int64_t type, int64_t owned,
                              const struct anti_descriptor *d,
                              struct anti_builder *b)
{
    field.name = name("f");
    field.name_length = 1;
    field.offset = (int64_t)offsetof(struct one_field, bytes);
    field.type = type;
    field.owned = owned;
    field.descriptor = d;
    holder.name = name("Holder");
    holder.name_length = 6;
    holder.size = (int64_t)sizeof *o;
    holder.ancestors = holder_chain;
    holder.field_count = 1;
    holder.fields = &field;
    o->base.table = holder_table;
    anti_lang_Object_serialize(&o->base, b);
    return taken(b);
}

static void patterns(void)
{
    unsigned char text[] = "ab";
    struct anti_pattern a = {NULL, text, 2, 0};
    struct anti_pattern bytes = {NULL, text, 2, 1};

    CHECK(anti_rt_pattern_same(&a, NULL) == 0);
    CHECK(anti_rt_pattern_same(&a, &bytes) == 0);
}

static void unquote(struct anti_builder *b)
{
    CHECK(anti_rt_json_unquote(b, NULL, 0) == 0);
    CHECK(b->length == 0);
}

static void type_args(void)
{
    static struct anti_field arg;
    static struct anti_descriptor d;
    static const struct anti_descriptor *chain[1];
    static const struct anti_descriptor *table[ANTI_ENTRY_HOOK];
    struct anti_object o = {table};

    arg.type = ANTI_TYPE_I64;
    d.depth = 0;
    d.type_arg_count = 1;
    d.type_args = &arg;
    table[0] = &d;
    CHECK(anti_rt_type_arg(NULL, 0, 0) == NULL);
    CHECK(anti_rt_type_arg(&o, -1, 0) == NULL);
    CHECK(anti_rt_type_arg(&o, 1, 0) == NULL);
    CHECK(anti_rt_type_arg(&o, 0, 0) == NULL);
    d.ancestors = chain;
    CHECK(anti_rt_type_arg(&o, 0, 0) == NULL);
    chain[0] = &d;
    CHECK(anti_rt_type_arg(&o, 0, -1) == NULL);
    CHECK(anti_rt_type_arg(&o, 0, 1) == NULL);
    CHECK(anti_rt_type_arg(&o, 0, 0) == &arg);
    CHECK(anti_rt_arg_is_str(NULL) == 0);
}

/* An element whose table holds the descriptor alone: no to_text, no
   serialize and no copy. */
static struct anti_descriptor bare;
static const struct anti_descriptor *bare_table[ANTI_ENTRY_HOOK] = {&bare};

static void elements(struct anti_builder *b)
{
    struct anti_object o = {bare_table};
    struct anti_field arg = {0};
    int64_t words[4] = {0};

    bare.name = name("Bare");
    bare.name_length = 4;
    bare.size = (int64_t)sizeof o;
    anti_rt_element_text(b, words, NULL);
    CHECK_STR(taken(b), "?");
    anti_rt_element_serialize(b, words, NULL);
    CHECK_STR(taken(b), "null");
    arg.type = ANTI_TYPE_STRUCT;
    anti_rt_element_text(b, words, &arg);
    CHECK_STR(taken(b), "?");
    arg.type = ANTI_TYPE_CLASS;
    anti_rt_element_text(b, &o, &arg);
    CHECK_STR(taken(b), "none");
    anti_rt_element_serialize(b, &o, &arg);
    CHECK_STR(taken(b), "null");
    /* An array of arrays whose record gives no levels, held in a struct,
       so its element size is 0. */
    arg.type = type_id(ANTI_TYPE_ARRAY, ANTI_TYPE_ARRAY, ANTI_TYPE_I64, 2);
    arg.offset = 0;
    anti_rt_element_text(b, words, &arg);
    CHECK_STR(taken(b), "?");
}

static void fields(struct anti_builder *b)
{
    static struct one_field o;
    static struct anti_field cases[2];
    static struct anti_descriptor variant;
    static struct anti_field levels[ANTI_ARRAY_LEVELS + 1];
    static struct anti_descriptor deep;
    int64_t i;

    CHECK_STR(serialized(&o, ANTI_TYPE_VARIANT, 0, NULL, b),
              "{\"type\":\"Holder\",\"f\":null}");
    CHECK_STR(serialized(&o, ANTI_TYPE_VARIANT, 0, &variant, b),
              "{\"type\":\"Holder\",\"f\":null}");
    /* The tag 7 names neither case. */
    cases[0].type = ANTI_TYPE_U8;
    cases[1].name = name("Empty");
    cases[1].name_length = 5;
    cases[1].owned = 0;
    variant.field_count = 2;
    variant.fields = cases;
    o.bytes[0] = 7;
    CHECK_STR(serialized(&o, ANTI_TYPE_VARIANT, 0, &variant, b),
              "{\"type\":\"Holder\",\"f\":null}");
    o.bytes[0] = 0;
    CHECK_STR(serialized(&o, ANTI_TYPE_OPTIONAL, 0, NULL, b),
              "{\"type\":\"Holder\",\"f\":null}");
    variant.field_count = 1;
    CHECK_STR(serialized(&o, ANTI_TYPE_OPTIONAL, 0, &variant, b),
              "{\"type\":\"Holder\",\"f\":null}");
    CHECK_STR(serialized(&o, ANTI_TYPE_TUPLE, 0, NULL, b),
              "{\"type\":\"Holder\",\"f\":null}");
    /* An array of structs without the descriptor of its element. */
    CHECK_STR(serialized(&o,
                         type_id(ANTI_TYPE_ARRAY, ANTI_TYPE_STRUCT,
                                 ANTI_TYPE_STRUCT, 2),
                         0, NULL, b),
              "{\"type\":\"Holder\",\"f\":null}");
    /* An array of arrays without a descriptor of its levels, with none,
       and with more than ANTI_ARRAY_LEVELS. */
    CHECK_STR(serialized(&o,
                         type_id(ANTI_TYPE_ARRAY, ANTI_TYPE_ARRAY,
                                 ANTI_TYPE_I64, 2),
                         0, NULL, b),
              "{\"type\":\"Holder\",\"f\":null}");
    CHECK_STR(serialized(&o,
                         type_id(ANTI_TYPE_ARRAY, ANTI_TYPE_ARRAY,
                                 ANTI_TYPE_I64, 2),
                         0, &deep, b),
              "{\"type\":\"Holder\",\"f\":null}");
    for (i = 0; i <= ANTI_ARRAY_LEVELS; i++) {
        levels[i].owned = 1;
    }
    deep.field_count = ANTI_ARRAY_LEVELS + 1;
    deep.fields = levels;
    CHECK_STR(serialized(&o,
                         type_id(ANTI_TYPE_ARRAY, ANTI_TYPE_ARRAY,
                                 ANTI_TYPE_I64, 1),
                         0, &deep, b),
              "{\"type\":\"Holder\",\"f\":null}");
    /* An `own` slice of structs without the descriptor of its element. */
    {
        struct anti_text s = {o.bytes, 1};
        memcpy(o.bytes, &s, sizeof s);
        CHECK_STR(serialized(&o, type_id(ANTI_TYPE_SLICE, ANTI_TYPE_STRUCT, 0, 0),
                             1, NULL, b),
                  "{\"type\":\"Holder\",\"f\":null}");
    }
    /* A function pointer marked as owned. */
    {
        void (*call)(void) = patterns;
        memcpy(o.bytes, &call, sizeof call);
        CHECK_STR(serialized(&o, ANTI_TYPE_FN, 1, NULL, b),
                  "{\"type\":\"Holder\",\"f\":null}");
    }
    memset(o.bytes, 0, sizeof o.bytes);
}

static void objects(struct anti_builder *b)
{
    static struct one_field o;
    struct anti_object unset = {NULL};
    struct anti_object to = {NULL};
    struct anti_object made = {bare_table};

    anti_lang_Object_serialize(NULL, b);
    CHECK_STR(taken(b), "null");
    anti_lang_Object_serialize(&unset, b);
    CHECK_STR(taken(b), "null");
    /* A descriptor without its ancestors writes no field. */
    holder.ancestors = NULL;
    holder.depth = 0;
    o.base.table = holder_table;
    anti_lang_Object_serialize(&o.base, b);
    CHECK_STR(taken(b), "{\"type\":\"Holder\"}");
    holder.ancestors = holder_chain;
    anti_lang_Object_copy(&unset, &to);
    CHECK(to.table == NULL);
    CHECK(anti_rt_dup(&made, &bare) == NULL);
}

int main(void)
{
    struct anti_builder b = {0};

    patterns();
    unquote(&b);
    type_args();
    elements(&b);
    fields(&b);
    objects(&b);
    free(b.room);
    if (check_failures > 0) {
        fprintf(stderr, "%d checks failed\n", check_failures);
        return 1;
    }
    printf("object: all checks passed\n");
    return 0;
}

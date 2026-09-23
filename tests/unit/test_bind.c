/* The parser of C type spellings and the evaluator of constant
   expressions of anti bind. */
#include <stdlib.h>
#include <string.h>

#include "../binary_stdio.h"
#include "bindexpr.h"
#include "bindmodel.h"
#include "check.h"

static const struct bind_type *no_typedef(void *context, const char *name)
{
    (void)context;
    (void)name;
    return NULL;
}

static struct bind_record *no_record(void *context, const char *tag,
                                     bool is_union)
{
    (void)context;
    (void)tag;
    (void)is_union;
    return NULL;
}

static const char *no_enum(void *context, const char *tag)
{
    (void)context;
    (void)tag;
    return NULL;
}

static const struct bind_type *parse(struct bind_module *b, const char *c)
{
    struct bind_names names;

    names.typedef_named = no_typedef;
    names.record_named = no_record;
    names.enum_named = no_enum;
    names.context = NULL;
    return bind_parse_type(b, c, &names);
}

static bool is_scalar(const struct bind_type *t, const char *name)
{
    return t != NULL && t->kind == BIND_SCALAR && strcmp(t->name, name) == 0;
}

static void spellings(void)
{
    struct bind_module b;
    const struct bind_type *t;

    memset(&b, 0, sizeof b);
    b.source = "test";
    CHECK(is_scalar(parse(&b, "unsigned long long"), "c_ulonglong"));
    CHECK(is_scalar(parse(&b, "long unsigned int"), "c_ulong"));
    CHECK(is_scalar(parse(&b, "signed char"), "c_char"));
    CHECK(is_scalar(parse(&b, "const volatile short"), "c_short"));
    CHECK(is_scalar(parse(&b, "uint8_t"), "u8"));
    CHECK(is_scalar(parse(&b, "size_t"), "c_size_t"));
    t = parse(&b, "long double");
    CHECK(t != NULL && t->kind == BIND_UNSUPPORTED);
    t = parse(&b, "struct Unknown *");
    CHECK(t != NULL && t->kind == BIND_POINTER && t->to->kind == BIND_OPAQUE);

    /* `const unsigned char *const *` is a pointer to a pointer. */
    t = parse(&b, "const unsigned char *const *");
    CHECK(t != NULL && t->kind == BIND_POINTER && t->to->kind == BIND_POINTER &&
          is_scalar(t->to->to, "c_uchar"));

    /* `float[2][3]` is an array of 2 arrays of 3. */
    t = parse(&b, "float[2][3]");
    CHECK(t != NULL && t->kind == BIND_ARRAY && t->length == 2 &&
          t->to->kind == BIND_ARRAY && t->to->length == 3 &&
          is_scalar(t->to->to, "c_float"));

    t = parse(&b, "void (*)(int, const char *, ...)");
    CHECK(t != NULL && t->kind == BIND_POINTER &&
          t->to->kind == BIND_FUNCTION && t->to->variadic &&
          t->to->param_count == 2 && t->to->to->kind == BIND_VOID &&
          t->to->params[1]->kind == BIND_POINTER);

    /* A pointer to a function of int that returns a pointer to a function
       of char. */
    t = parse(&b, "void (*(*)(int))(char)");
    CHECK(t != NULL && t->kind == BIND_POINTER &&
          t->to->kind == BIND_FUNCTION && t->to->param_count == 1 &&
          is_scalar(t->to->params[0], "c_int") &&
          t->to->to->kind == BIND_POINTER &&
          t->to->to->to->kind == BIND_FUNCTION &&
          is_scalar(t->to->to->to->params[0], "c_char"));

    t = parse(&b, "int (void)");
    CHECK(t != NULL && t->kind == BIND_FUNCTION && t->param_count == 0);

    CHECK(parse(&b, "int (") == NULL);
    CHECK(parse(&b, "struct (unnamed at a.h:1:2)") == NULL);
    CHECK(parse(&b, "") == NULL);
    free(b.records.items);
    arena_free(&b.arena);
}

static bool value_of(struct bind_module *b, const char *expr,
                     struct bind_eval *out)
{
    return bind_eval(b, expr, NULL, NULL, out);
}

static void expressions(void)
{
    struct bind_module b;
    struct bind_eval v;

    memset(&b, 0, sizeof b);
    b.source = "test";
    CHECK(value_of(&b, "(1u << 31)", &v) && v.i == 2147483648ll &&
          strcmp(v.type, "c_uint") == 0);
    CHECK(value_of(&b, "0x7fffffff", &v) && strcmp(v.type, "c_int") == 0);
    CHECK(value_of(&b, "0xFFFFFFFF", &v) && strcmp(v.type, "c_uint") == 0);
    CHECK(value_of(&b, "4294967296", &v) && strcmp(v.type, "c_longlong") == 0);
    CHECK(value_of(&b, "-(3)", &v) && v.i == -3 && strcmp(v.type, "c_int") == 0);
    CHECK(value_of(&b, "1.5f * 2", &v) && v.kind == BIND_EVAL_FLOAT &&
          v.f == 3.0 && strcmp(v.type, "c_float") == 0);
    CHECK(value_of(&b, "(float)1 / 4", &v) && v.f == 0.25 &&
          strcmp(v.type, "c_float") == 0);
    CHECK(value_of(&b, "1.0 / 4", &v) && strcmp(v.type, "c_double") == 0);
    CHECK(value_of(&b, "\"ab\" \"cd\"", &v) && v.kind == BIND_EVAL_STRING &&
          strcmp(v.text, "abcd") == 0);
    CHECK(value_of(&b, "~0u", &v) && v.i == 0xFFFFFFFFll);
    CHECK(value_of(&b, "7 % 4 | 8 ^ 1 & 3", &v) && v.i == (7 % 4 | (8 ^ (1 & 3))));
    CHECK(!value_of(&b, "10 / 0", &v));
    CHECK(!value_of(&b, "1 << 64", &v));
    CHECK(!value_of(&b, "(-9223372036854775807ll - 1) / -1", &v));
    CHECK(!value_of(&b, "1.0L", &v));
    CHECK(!value_of(&b, "(int)1e30", &v));
    CHECK(value_of(&b, "(unsigned char)300", &v) && v.i == 44);
    CHECK(!value_of(&b, "UNKNOWN", &v));
    CHECK(!value_of(&b, "(1 + 2", &v));
    CHECK(!value_of(&b, "", &v));
    CHECK(!value_of(&b, "\"open", &v));
    arena_free(&b.arena);
}

void test_bind(void)
{
    spellings();
    expressions();
}

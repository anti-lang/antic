/* The reader of an API description in the format of raylib's rlparser,
   raylib_api.json. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bindexpr.h"
#include "bindmodel.h"
#include "jsontree.h"

struct api {
    struct bind_module *b;
    const struct json_value *root;
    int depth;                  /* of the aliases being resolved */
};

static const struct json_value *section(const struct api *a, const char *name)
{
    const struct json_value *v = json_get(a->root, name);

    return v != NULL && v->kind == JSON_ARRAY ? v : NULL;
}

/* The entry of a section with the name, or NULL. */
static const struct json_value *entry_named(const struct api *a,
                                            const char *section_name,
                                            const char *name)
{
    const struct json_value *list = section(a, section_name);
    size_t i;

    for (i = 0; list != NULL && i < list->count; i++) {
        const char *n = json_member_string(list->items[i], "name");
        if (n != NULL && strcmp(n, name) == 0) {
            return list->items[i];
        }
    }
    return NULL;
}

static struct bind_record *record_of(struct api *a, const char *name)
{
    size_t i;

    for (i = 0; i < a->b->records.count; i++) {
        struct bind_record *r = a->b->records.items[i];
        if (strcmp(r->name, name) == 0) {
            return r;
        }
    }
    return NULL;
}

static const struct bind_type *parse(struct api *a, const char *c);

static const struct bind_type *function_type(struct api *a,
                                             const struct json_value *f)
{
    const struct json_value *params = json_get(f, "params");
    const char *result = json_member_string(f, "returnType");
    struct bind_type *t = bind_type_new(a->b, BIND_FUNCTION);
    const struct bind_type **types;
    size_t count = params != NULL ? params->count : 0;
    size_t i;

    t->to = result != NULL ? parse(a, result) : NULL;
    if (t->to == NULL) {
        return NULL;
    }
    types = arena_alloc(&a->b->arena, (count + 1) * sizeof *types);
    for (i = 0; i < count; i++) {
        const char *c = json_member_string(params->items[i], "type");
        if (c == NULL) {
            return NULL;
        }
        if (strcmp(c, "...") == 0) {
            t->variadic = true;
            continue;
        }
        types[t->param_count] = parse(a, c);
        if (types[t->param_count] == NULL) {
            return NULL;
        }
        t->param_count++;
    }
    t->params = types;
    return t;
}

static const struct bind_type *typedef_named(void *context, const char *name)
{
    struct api *a = context;
    const struct json_value *alias;
    const struct json_value *callback;
    struct bind_record *r = record_of(a, name);
    size_t i;

    if (r != NULL) {
        struct bind_type *t = bind_type_new(a->b, BIND_RECORD);
        t->name = r->name;
        t->record = r;
        return t;
    }
    if (entry_named(a, "enums", name) != NULL) {
        struct bind_type *t = bind_type_new(a->b, BIND_ENUM);
        t->name = bind_strdup(a->b, name);
        return t;
    }
    callback = entry_named(a, "callbacks", name);
    if (callback != NULL) {
        const struct bind_type *f = function_type(a, callback);
        struct bind_type *t;
        if (f == NULL) {
            return NULL;
        }
        t = bind_type_new(a->b, BIND_POINTER);
        t->to = f;
        return t;
    }
    alias = entry_named(a, "aliases", name);
    if (alias != NULL) {
        const char *c = json_member_string(alias, "type");
        return c != NULL ? parse(a, c) : NULL;
    }
    /* DESIGN: rlparser writes the alias `typedef Transform *ModelAnimPose`
       with the name `*ModelAnimPose`, so a name with a leading `*` is a
       pointer alias. */
    {
        const struct json_value *list = section(a, "aliases");
        for (i = 0; list != NULL && i < list->count; i++) {
            const char *n = json_member_string(list->items[i], "name");
            const char *c = json_member_string(list->items[i], "type");
            if (n != NULL && c != NULL && n[0] == '*' &&
                strcmp(n + 1, name) == 0) {
                const struct bind_type *to = parse(a, c);
                struct bind_type *t;
                if (to == NULL) {
                    return NULL;
                }
                t = bind_type_new(a->b, BIND_POINTER);
                t->to = to;
                return t;
            }
        }
    }
    return NULL;
}

static struct bind_record *record_named(void *context, const char *tag,
                                        bool is_union)
{
    (void)is_union;
    return record_of(context, tag);
}

static const char *enum_named(void *context, const char *tag)
{
    struct api *a = context;

    return entry_named(a, "enums", tag) != NULL ? bind_strdup(a->b, tag)
                                                 : NULL;
}

static const struct bind_type *parse(struct api *a, const char *c)
{
    struct bind_names names;
    const struct bind_type *t;

    /* An alias that names itself, directly or through others, would
       recurse without end. */
    if (a->depth > 32) {
        bind_warn(a->b, "the type `%s` names itself", c);
        return NULL;
    }
    names.typedef_named = typedef_named;
    names.record_named = record_named;
    names.enum_named = enum_named;
    names.context = a;
    a->depth++;
    t = bind_parse_type(a->b, c, &names);
    a->depth--;
    if (t == NULL) {
        bind_warn(a->b, "the type `%s` does not parse", c);
    }
    return t;
}

static const char *doc_of(struct api *a, const struct json_value *v)
{
    const char *d = json_member_string(v, "description");

    return d != NULL && d[0] != '\0' ? bind_strdup(a->b, d) : NULL;
}

static void read_structs(struct api *a)
{
    const struct json_value *list = section(a, "structs");
    size_t i;
    size_t j;

    /* Every record exists before a field names one. */
    for (i = 0; list != NULL && i < list->count; i++) {
        const char *name = json_member_string(list->items[i], "name");
        struct bind_record *r;
        if (name == NULL) {
            continue;
        }
        r = arena_alloc(&a->b->arena, sizeof *r);
        r->name = bind_strdup(a->b, name);
        r->c_name = r->name;
        r->complete = true;
        r->doc = doc_of(a, list->items[i]);
        bind_list_add(&a->b->records, r);
    }
    for (i = 0; list != NULL && i < list->count; i++) {
        const char *name = json_member_string(list->items[i], "name");
        const struct json_value *fields = json_get(list->items[i], "fields");
        struct bind_record *r = name != NULL ? record_of(a, name) : NULL;
        if (r == NULL || fields == NULL || fields->kind != JSON_ARRAY) {
            continue;
        }
        r->fields = arena_alloc(&a->b->arena,
                                (fields->count + 1) * sizeof *r->fields);
        for (j = 0; j < fields->count; j++) {
            const char *fname = json_member_string(fields->items[j], "name");
            const char *ftype = json_member_string(fields->items[j], "type");
            struct bind_field *f = &r->fields[r->field_count];
            if (fname == NULL || ftype == NULL) {
                continue;
            }
            f->name = bind_strdup(a->b, fname);
            f->c_type = bind_strdup(a->b, ftype);
            f->bits = -1;
            f->type = parse(a, ftype);
            if (f->type == NULL) {
                struct bind_type *u = bind_type_new(a->b, BIND_UNSUPPORTED);
                u->name = f->c_type;
                f->type = u;
            }
            r->field_count++;
        }
    }
}

static void read_enums(struct api *a)
{
    const struct json_value *list = section(a, "enums");
    size_t i;
    size_t j;

    for (i = 0; list != NULL && i < list->count; i++) {
        const char *name = json_member_string(list->items[i], "name");
        const struct json_value *values = json_get(list->items[i], "values");
        struct bind_enum *e;
        if (name == NULL || values == NULL || values->kind != JSON_ARRAY) {
            continue;
        }
        e = arena_alloc(&a->b->arena, sizeof *e);
        e->name = bind_strdup(a->b, name);
        e->c_name = e->name;
        e->doc = doc_of(a, list->items[i]);
        e->values = arena_alloc(&a->b->arena,
                                (values->count + 1) * sizeof *e->values);
        for (j = 0; j < values->count; j++) {
            const char *vname = json_member_string(values->items[j], "name");
            int64_t value;
            if (vname == NULL ||
                !json_integer(json_get(values->items[j], "value"), &value)) {
                bind_warn(a->b, "a value of the enum `%s` is no integer", name);
                continue;
            }
            e->values[e->value_count].name = bind_strdup(a->b, vname);
            e->values[e->value_count].value = value;
            e->values[e->value_count].doc = doc_of(a, values->items[j]);
            e->value_count++;
        }
        bind_list_add(&a->b->enums, e);
    }
}

static void read_functions(struct api *a)
{
    const struct json_value *list = section(a, "functions");
    size_t i;
    size_t j;

    for (i = 0; list != NULL && i < list->count; i++) {
        const struct json_value *item = list->items[i];
        const char *name = json_member_string(item, "name");
        const char *result = json_member_string(item, "returnType");
        const struct json_value *params = json_get(item, "params");
        size_t count = params != NULL ? params->count : 0;
        struct bind_function *f;
        bool ok = true;
        if (name == NULL || result == NULL) {
            continue;
        }
        f = arena_alloc(&a->b->arena, sizeof *f);
        f->name = bind_strdup(a->b, name);
        f->doc = doc_of(a, item);
        f->c_result = bind_strdup(a->b, result);
        f->result = parse(a, result);
        f->params = arena_alloc(&a->b->arena, (count + 1) * sizeof *f->params);
        ok = f->result != NULL;
        for (j = 0; ok && j < count; j++) {
            const char *pname = json_member_string(params->items[j], "name");
            const char *ptype = json_member_string(params->items[j], "type");
            struct bind_param *p = &f->params[f->param_count];
            if (ptype == NULL) {
                ok = false;
                break;
            }
            if (strcmp(ptype, "...") == 0) {
                f->variadic = true;
                continue;
            }
            p->name = pname != NULL ? bind_strdup(a->b, pname) : NULL;
            p->c_type = bind_strdup(a->b, ptype);
            p->type = parse(a, ptype);
            ok = p->type != NULL;
            f->param_count++;
        }
        if (!ok) {
            bind_warn(a->b, "`%s` is left out: a type does not parse", name);
            continue;
        }
        bind_list_add(&a->b->functions, f);
    }
}

/* A name inside the value of a define: a define read before, or a value
   of an enum. */
static bool lookup(void *context, const char *name, struct bind_eval *out)
{
    struct api *a = context;
    const struct json_value *enums = section(a, "enums");
    size_t i;
    size_t j;

    for (i = 0; i < a->b->consts.count; i++) {
        const struct bind_const *c = a->b->consts.items[i];
        if (strcmp(c->name, name) == 0 && c->eval != NULL) {
            *out = *c->eval;
            return true;
        }
    }
    for (i = 0; enums != NULL && i < enums->count; i++) {
        const struct json_value *values = json_get(enums->items[i], "values");
        for (j = 0; values != NULL && j < values->count; j++) {
            const char *vname = json_member_string(values->items[j], "name");
            if (vname != NULL && strcmp(vname, name) == 0 &&
                json_integer(json_get(values->items[j], "value"), &out->i)) {
                out->kind = BIND_EVAL_INT;
                out->type = "c_int";
                out->enum_type = json_member_string(enums->items[i], "name");
                out->enum_value = vname;
                return true;
            }
        }
    }
    return false;
}

/* A define of the type COLOR, `CLITERAL(Color){ 200, 200, 200, 255 }`,
   becomes a constant of the struct it names. */
static bool color(struct api *a, const char *name, const char *value,
                  const char *doc)
{
    const char *open = strchr(value, '(');
    const char *close = open != NULL ? strchr(open, ')') : NULL;
    const char *brace = close != NULL ? strchr(close, '{') : NULL;
    struct bind_record *r;
    struct bind_const *c;
    struct text literal = {0};
    const char *p;
    size_t field = 0;
    char type[64];

    if (brace == NULL || (size_t)(close - open - 1) >= sizeof type) {
        return false;
    }
    memcpy(type, open + 1, (size_t)(close - open - 1));
    type[close - open - 1] = '\0';
    r = record_of(a, type);
    if (r == NULL) {
        return false;
    }
    text_appendf(&literal, "%s { ", r->name);
    p = brace + 1;
    for (;;) {
        char *end;
        long long n;
        while (*p == ' ') {
            p++;
        }
        if (*p == '}') {
            break;
        }
        n = strtoll(p, &end, 10);
        if (end == p || field >= r->field_count) {
            text_free(&literal);
            return false;
        }
        text_appendf(&literal, "%s%s: %lld", field > 0 ? ", " : "",
                     r->fields[field].name, n);
        field++;
        p = end;
        while (*p == ' ') {
            p++;
        }
        if (*p == ',') {
            p++;
        }
    }
    if (field != r->field_count) {
        text_free(&literal);
        return false;
    }
    text_append(&literal, " }");
    c = arena_alloc(&a->b->arena, sizeof *c);
    c->name = bind_strdup(a->b, name);
    c->type = r->name;
    c->value = bind_strdup(a->b, text_cstr(&literal));
    c->record = r;
    c->doc = doc;
    bind_list_add(&a->b->consts, c);
    text_free(&literal);
    return true;
}

/* DESIGN: rlparser gives each define a kind. INT, FLOAT, FLOAT_MATH,
   STRING and an UNKNOWN that is one expression go through the evaluator
   of a macro. A define and a macro of a header then become the same
   constant. A FLOAT is a float of C, which raylib writes with the suffix
   f. COLOR is a struct literal. A GUARD, a MACRO and the rest are skipped
   with a warning, as every macro the generator cannot evaluate is. */
static void read_defines(struct api *a)
{
    const struct json_value *list = section(a, "defines");
    size_t i;

    for (i = 0; list != NULL && i < list->count; i++) {
        const struct json_value *item = list->items[i];
        const char *name = json_member_string(item, "name");
        const char *kind = json_member_string(item, "type");
        const struct json_value *value = json_get(item, "value");
        const char *doc = doc_of(a, item);
        struct bind_eval v;
        struct text expr = {0};
        bool ok = false;
        if (name == NULL || kind == NULL || value == NULL) {
            continue;
        }
        if (value->kind == JSON_STRING) {
            if (strcmp(kind, "STRING") == 0) {
                text_append(&expr, "\"");
                text_append(&expr, value->text);
                text_append(&expr, "\"");
            } else {
                text_append(&expr, value->text);
            }
        } else if (value->kind == JSON_NUMBER) {
            text_append(&expr, value->text);
            if (strcmp(kind, "FLOAT") == 0 && strchr(value->text, '.') == NULL &&
                strchr(value->text, 'e') == NULL) {
                text_append(&expr, ".0");
            }
            if (strcmp(kind, "FLOAT") == 0) {
                text_append(&expr, "f");
            }
        }
        if (strcmp(kind, "COLOR") == 0 && value->kind == JSON_STRING) {
            ok = color(a, name, value->text, doc);
        } else if ((strcmp(kind, "INT") == 0 || strcmp(kind, "FLOAT") == 0 ||
                    strcmp(kind, "FLOAT_MATH") == 0 ||
                    strcmp(kind, "STRING") == 0 ||
                    strcmp(kind, "UNKNOWN") == 0) &&
                   strchr(name, '(') == NULL && expr.length > 0 &&
                   bind_eval(a->b, text_cstr(&expr), lookup, a, &v)) {
            ok = bind_eval_const(a->b, name, &v, doc);
        }
        if (!ok) {
            bind_warn(a->b, "the define `%s` is skipped", name);
        }
        text_free(&expr);
    }
}

bool bind_read_api(struct bind_module *b, const unsigned char *bytes,
                   size_t length)
{
    struct json_tree tree;
    struct api a;
    char error[160];

    if (!json_read(bytes, length, &tree, error, sizeof error)) {
        fprintf(stderr, "anti: %s is not JSON: %s\n", b->source, error);
        return false;
    }
    if (tree.root->kind != JSON_OBJECT || json_get(tree.root, "functions") == NULL) {
        fprintf(stderr, "anti: %s holds no API description of rlparser\n",
                b->source);
        json_free(&tree);
        return false;
    }
    a.b = b;
    a.root = tree.root;
    a.depth = 0;
    read_structs(&a);
    read_enums(&a);
    read_functions(&a);
    read_defines(&a);
    json_free(&tree);
    return true;
}

#include "types.h"

#include <stdio.h>
#include <string.h>

void types_init(struct types *types, struct arena *arena)
{
    int k;

    memset(types, 0, sizeof *types);
    types->arena = arena;
    for (k = 0; k < TYPE_BUILTIN_COUNT; k++) {
        types->builtins[k].kind = (enum type_kind)k;
    }
}

struct type *types_builtin(struct types *types, enum type_kind kind)
{
    return &types->builtins[kind];
}

/* DESIGN: derived types sit in one linked list and every constructor
   searches it before it creates a type. A program has few distinct
   types, so a linear search is enough and keeps identity a pointer
   comparison. */
static struct type *find_or_add(struct types *types, const struct type *key)
{
    struct type *t;
    size_t i;

    for (t = types->derived; t != NULL; t = t->next) {
        if (t->kind != key->kind || t->element != key->element ||
            t->length != key->length || t->length_of != key->length_of ||
            t->result != key->result || t->bound != key->bound ||
            t->nullable != key->nullable || t->may_fail != key->may_fail ||
            t->has_out != key->has_out ||
            t->param_count != key->param_count) {
            continue;
        }
        for (i = 0; i < t->param_count; i++) {
            if (t->params[i] != key->params[i]) {
                break;
            }
        }
        if (i == t->param_count) {
            return t;
        }
    }
    t = arena_alloc(types->arena, sizeof *t);
    *t = *key;
    if (key->param_count > 0) {
        t->params = arena_alloc(types->arena,
                                key->param_count * sizeof *t->params);
        memcpy(t->params, key->params, key->param_count * sizeof *t->params);
    }
    t->next = types->derived;
    types->derived = t;
    return t;
}

struct type *types_pointer_of(struct types *types, struct type *element,
                              bool nullable)
{
    struct type key = {0};

    key.kind = TYPE_POINTER;
    key.element = element;
    key.nullable = nullable;
    return find_or_add(types, &key);
}

struct type *types_pointer(struct types *types, struct type *element)
{
    return types_pointer_of(types, element, false);
}

struct type *types_pointer_nullable(struct types *types, struct type *element)
{
    return types_pointer_of(types, element, true);
}

bool type_is_nullable(const struct type *t)
{
    return t != NULL && (t->kind == TYPE_POINTER || t->kind == TYPE_FN) &&
           t->nullable;
}

struct type *types_without_none(struct types *types, struct type *t)
{
    struct type key;

    if (!type_is_nullable(t)) {
        return t;
    }
    key = *t;
    key.nullable = false;
    key.next = NULL;
    return find_or_add(types, &key);
}

struct type *types_with_none(struct types *types, struct type *t)
{
    struct type key;

    if (t == NULL || type_is_nullable(t) ||
        (t->kind != TYPE_POINTER && t->kind != TYPE_FN)) {
        return t;
    }
    key = *t;
    key.nullable = true;
    key.next = NULL;
    return find_or_add(types, &key);
}

struct type *types_array(struct types *types, struct type *element,
                         uint64_t length)
{
    struct type key = {0};

    key.kind = TYPE_ARRAY;
    key.element = element;
    key.length = length;
    return find_or_add(types, &key);
}

struct type *types_array_symbolic(struct types *types, struct type *element,
                                  const struct symbolic *length)
{
    struct type key = {0};

    key.kind = TYPE_ARRAY;
    key.element = element;
    key.length_of = length;
    return find_or_add(types, &key);
}

const struct symbolic *types_symbolic(struct types *types,
                                      const struct symbolic *key)
{
    struct symbolic *s;

    for (s = types->symbolics; s != NULL; s = s->next) {
        if (s->kind == key->kind && s->type == key->type &&
            s->value == key->value && s->of == key->of && s->op == key->op &&
            s->a == key->a && s->b == key->b) {
            return s;
        }
    }
    s = arena_alloc(types->arena, sizeof *s);
    *s = *key;
    s->next = types->symbolics;
    types->symbolics = s;
    return s;
}

static const char *operator_spelling(enum token_kind op)
{
    switch (op) {
    case TOKEN_PLUS: return "+";
    case TOKEN_MINUS: return "-";
    case TOKEN_STAR: return "*";
    case TOKEN_SLASH: return "/";
    case TOKEN_PERCENT: return "%";
    case TOKEN_AMP: return "&";
    case TOKEN_PIPE: return "|";
    case TOKEN_CARET: return "^";
    case TOKEN_TILDE: return "~";
    case TOKEN_BANG: return "!";
    case TOKEN_SHL: return "<<";
    case TOKEN_SHR: return ">>";
    case TOKEN_AND_AND: return "&&";
    case TOKEN_OR_OR: return "||";
    case TOKEN_EQ: return "==";
    case TOKEN_NE: return "!=";
    case TOKEN_LT: return "<";
    case TOKEN_LE: return "<=";
    case TOKEN_GT: return ">";
    default: return ">=";
    }
}

static void print_type(struct text *out, const struct type *t, bool qualified);

/* An operand in parentheses when it is itself an operation, so the text
   keeps the grouping of the value. */
static void print_operand(struct text *out, const struct symbolic *s,
                          bool qualified)
{
    bool group = s->kind == SYMBOLIC_BINARY || s->kind == SYMBOLIC_CAST;

    text_append(out, group ? "(" : "");
    symbolic_print(out, s, qualified);
    text_append(out, group ? ")" : "");
}

void symbolic_print(struct text *out, const struct symbolic *s,
                    bool qualified)
{
    switch (s->kind) {
    case SYMBOLIC_INT:
        if (s->type->kind == TYPE_BOOL) {
            text_append(out, s->value != 0 ? "true" : "false");
        } else if (type_is_signed(s->type)) {
            text_appendf(out, "%lld", (long long)s->value);
        } else {
            text_appendf(out, "%llu", (unsigned long long)s->value);
        }
        break;
    case SYMBOLIC_SIZE_OF:
        text_append(out, "size_of(");
        print_type(out, s->of, qualified);
        text_append(out, ")");
        break;
    case SYMBOLIC_UNARY:
        text_append(out, operator_spelling(s->op));
        print_operand(out, s->a, qualified);
        break;
    case SYMBOLIC_BINARY:
        print_operand(out, s->a, qualified);
        text_appendf(out, " %s ", operator_spelling(s->op));
        print_operand(out, s->b, qualified);
        break;
    case SYMBOLIC_CAST:
        print_operand(out, s->a, qualified);
        text_append(out, " as ");
        print_type(out, s->type, qualified);
        break;
    }
}

struct type *types_slice(struct types *types, struct type *element)
{
    struct type key = {0};

    key.kind = TYPE_SLICE;
    key.element = element;
    return find_or_add(types, &key);
}

struct type *types_fn(struct types *types, struct type **params,
                      size_t param_count, struct type *result)
{
    struct type key = {0};

    key.kind = TYPE_FN;
    key.params = params;
    key.param_count = param_count;
    key.result = result;
    return find_or_add(types, &key);
}

struct type *types_fn_failing(struct types *types, struct type **params,
                              size_t param_count, struct type *result,
                              bool has_out)
{
    return types_fn_flagged(types, params, param_count, result, false, true,
                            has_out);
}

struct type *types_fn_flagged(struct types *types, struct type **params,
                              size_t param_count, struct type *result,
                              bool bound, bool may_fail, bool has_out)
{
    struct type key = {0};

    key.kind = TYPE_FN;
    key.params = params;
    key.param_count = param_count;
    key.result = result;
    key.bound = bound;
    key.may_fail = may_fail;
    key.has_out = has_out;
    return find_or_add(types, &key);
}

struct type *types_bound_of(struct types *types, const struct type *fn)
{
    struct type key = {0};

    key.kind = TYPE_FN;
    key.params = fn->params + 1;
    key.param_count = fn->param_count - 1;
    key.result = fn->result;
    key.bound = true;
    key.may_fail = fn->may_fail;
    key.has_out = fn->has_out;
    return find_or_add(types, &key);
}

/* DESIGN: `anti.lang.Object` is the root of every class chain. The
   compiler declares it, and `src/std/anti/lang.anti` does not, because a
   class inherits it without an import and its bodies and its descriptor
   are the runtime's. Its one field is the table pointer, which no
   program names and which every class carries at offset 0 through its
   base. */
bool types_is_lang_error(const struct type *t)
{
    return t != NULL && t->kind == TYPE_CLASS &&
           t->name.length == sizeof LANG_ERROR - 1 &&
           memcmp(t->name.text, LANG_ERROR, sizeof LANG_ERROR - 1) == 0 &&
           t->module.length == sizeof LANG_MODULE - 1 &&
           memcmp(t->module.text, LANG_MODULE, sizeof LANG_MODULE - 1) == 0;
}

static bool same_text(const struct name *a, const struct name *b)
{
    return a->length == b->length &&
           memcmp(a->text, b->text, a->length) == 0;
}

/* The next level of a chain: the base of a class, and nothing above any
   other type. */
static const struct type *level_above(const struct type *t)
{
    return t->kind == TYPE_CLASS ? t->base : NULL;
}

enum body_table types_body_table(const struct type *t, const struct item *m)
{
    const struct type *up;

    if (m->qualifier.length == 0 || same_text(&m->qualifier, &t->name)) {
        return BODY_PLAIN;
    }
    for (up = level_above(t); up != NULL; up = level_above(up)) {
        if (same_text(&m->qualifier, &up->name)) {
            return BODY_BASE;
        }
    }
    return BODY_INTERFACE;
}

const struct item *types_primary_member(const struct type *t,
                                        const struct name *name)
{
    size_t i;

    for (; t != NULL; t = level_above(t)) {
        const struct item *plain = NULL;
        for (i = 0; i < t->member_count; i++) {
            const struct item *m = t->members[i];
            if (m->kind != ITEM_FN || !same_text(&m->name, name)) {
                continue;
            }
            switch (types_body_table(t, m)) {
            case BODY_BASE:
                return m;
            case BODY_PLAIN:
                plain = plain != NULL ? plain : m;
                break;
            case BODY_INTERFACE:
                break;
            }
        }
        if (plain != NULL) {
            return plain;
        }
    }
    return NULL;
}

const struct type *types_member_level(const struct type *t,
                                      const struct item *m)
{
    size_t i;

    for (; t != NULL; t = level_above(t)) {
        for (i = 0; i < t->member_count; i++) {
            if (t->members[i] == m) {
                return t;
            }
        }
    }
    return NULL;
}

bool types_holds_entry(const struct type *t, const struct item *m)
{
    size_t i;

    for (; t != NULL; t = level_above(t)) {
        for (i = 0; i < t->member_count; i++) {
            if (t->members[i] == m) {
                return types_primary_member(t, &m->name) == m;
            }
        }
    }
    return false;
}

const struct item *types_interface_member(const struct type *t,
                                          const struct type *iface,
                                          const struct name *name)
{
    const struct type *chain;
    size_t i;

    for (; t != NULL; t = level_above(t)) {
        const struct item *plain = NULL;
        for (i = 0; i < t->member_count; i++) {
            const struct item *m = t->members[i];
            if (m->kind != ITEM_FN || !m->pub || !same_text(&m->name, name)) {
                continue;
            }
            switch (types_body_table(t, m)) {
            case BODY_PLAIN:
                plain = plain != NULL ? plain : m;
                break;
            case BODY_INTERFACE:
                for (chain = iface; chain != NULL;
                     chain = level_above(chain)) {
                    if (same_text(&m->qualifier, &chain->name)) {
                        return m;
                    }
                }
                break;
            case BODY_BASE:
                break;
            }
        }
        if (plain != NULL) {
            return plain;
        }
    }
    return NULL;
}

char *types_member_symbol(struct arena *arena, const struct name *owner,
                          const struct item *m)
{
    bool qualified = m->qualifier.length > 0 &&
                     !same_text(&m->qualifier, owner);
    size_t length = owner->length + 1 + m->name.length +
                    (qualified ? m->qualifier.length + 1 : 0);
    char *text = arena_alloc(arena, length + 1);

    if (qualified) {
        snprintf(text, length + 1, "%.*s.%.*s.%.*s", (int)owner->length,
                 owner->text, (int)m->qualifier.length, m->qualifier.text,
                 (int)m->name.length, m->name.text);
    } else {
        snprintf(text, length + 1, "%.*s.%.*s", (int)owner->length,
                 owner->text, (int)m->name.length, m->name.text);
    }
    return text;
}

struct type *types_object(struct types *types)
{
    static const char module_text[] = LANG_MODULE;
    static const char name_text[] = LANG_OBJECT;
    static const char field_text[] = "table";
    struct struct_field field;
    struct name module;
    struct name name;

    if (types->object != NULL) {
        return types->object;
    }
    module.text = module_text;
    module.length = sizeof module_text - 1;
    name.text = name_text;
    name.length = sizeof name_text - 1;
    types->object = types_struct(types, module, name);
    types->object->kind = TYPE_CLASS;
    memset(&field, 0, sizeof field);
    field.name.text = field_text;
    field.name.length = sizeof field_text - 1;
    field.form = FIELD_TABLE;
    field.type = types_pointer(types, types_builtin(types, TYPE_U8));
    types_set_fields(types, types->object, &field, 1);
    return types->object;
}

struct type *types_job(struct types *types, struct type *result)
{
    static const char module_text[] = LANG_MODULE;
    static const char name_text[] = LANG_JOB;
    static const char field_text[] = "handle";
    struct type *t;
    struct struct_field field;
    struct type key = {0};

    key.kind = TYPE_STRUCT;
    key.module.text = module_text;
    key.module.length = sizeof module_text - 1;
    key.name.text = name_text;
    key.name.length = sizeof name_text - 1;
    key.result = result;
    for (t = types->derived; t != NULL; t = t->next) {
        if (t->kind == TYPE_STRUCT && t->result == result &&
            t->name.length == key.name.length &&
            t->name.text == name_text) {
            return t;
        }
    }
    t = arena_alloc(types->arena, sizeof *t);
    *t = key;
    t->next = types->derived;
    types->derived = t;
    memset(&field, 0, sizeof field);
    field.name.text = field_text;
    field.name.length = sizeof field_text - 1;
    field.type = types_pointer(types, types_builtin(types, TYPE_U8));
    types_set_fields(types, t, &field, 1);
    return t;
}

struct type *types_flags(struct types *types)
{
    static const char module_text[] = LANG_MODULE;
    static const char name_text[] = LANG_FLAGS;
    static const char *const names[] = {FLAGS_OVERFLOW, FLAGS_CARRY,
                                        FLAGS_ZERO, FLAGS_NEGATIVE};
    struct struct_field fields[4];
    struct name module;
    struct name name;
    size_t i;

    if (types->flags != NULL) {
        return types->flags;
    }
    module.text = module_text;
    module.length = sizeof module_text - 1;
    name.text = name_text;
    name.length = sizeof name_text - 1;
    types->flags = types_struct(types, module, name);
    memset(fields, 0, sizeof fields);
    for (i = 0; i < 4; i++) {
        fields[i].name.text = names[i];
        fields[i].name.length = strlen(names[i]);
        fields[i].type = types_builtin(types, TYPE_BOOL);
        fields[i].vis = VIS_PUB;
    }
    types_set_fields(types, types->flags, fields, 4);
    types->flags->layout = LAYOUT_DONE;
    return types->flags;
}

/* DESIGN: the five fields stand in the order of `struct anti_field` of
   `src/rt/object.h`, and `name` covers its pointer and its length, as a
   `str` does. The `changed` hook takes a pointer into the field list the
   descriptor already carries, so the record is read and never built. */
struct type *types_field_descriptor(struct types *types)
{
    static const char module_text[] = LANG_MODULE;
    static const char name_text[] = LANG_FIELD_DESCRIPTOR;
    static const char *const names[] = {
        FIELD_RECORD_NAME, FIELD_RECORD_OFFSET, FIELD_RECORD_TYPE,
        FIELD_RECORD_OWNED, FIELD_RECORD_DESCRIPTOR
    };
    struct struct_field fields[5];
    struct name module;
    struct name name;
    size_t i;

    if (types->field_record != NULL) {
        return types->field_record;
    }
    module.text = module_text;
    module.length = sizeof module_text - 1;
    name.text = name_text;
    name.length = sizeof name_text - 1;
    types->field_record = types_struct(types, module, name);
    memset(fields, 0, sizeof fields);
    for (i = 0; i < 5; i++) {
        fields[i].name.text = names[i];
        fields[i].name.length = strlen(names[i]);
        fields[i].type = types_builtin(types, TYPE_I64);
        fields[i].vis = VIS_PUB;
    }
    fields[0].type = types_builtin(types, TYPE_STR);
    fields[4].type = types_pointer_nullable(types,
                                            types_builtin(types, TYPE_U8));
    types_set_fields(types, types->field_record, fields, 5);
    types->field_record->layout = LAYOUT_DONE;
    return types->field_record;
}

/* The struct of one handle that a Mutex and a channel are. */
static struct type *handle_struct(struct types *types, const char *name,
                                  struct type *element)
{
    static const char module_text[] = LANG_MODULE;
    static const char field_text[] = SYNC_HANDLE;
    struct struct_field field;
    struct type *t;

    t = arena_alloc(types->arena, sizeof *t);
    memset(t, 0, sizeof *t);
    t->kind = TYPE_STRUCT;
    t->module.text = module_text;
    t->module.length = sizeof module_text - 1;
    t->name.text = name;
    t->name.length = strlen(name);
    t->element = element;
    t->next = types->derived;
    types->derived = t;
    memset(&field, 0, sizeof field);
    field.name.text = field_text;
    field.name.length = sizeof field_text - 1;
    field.type = types_pointer(types, types_builtin(types, TYPE_U8));
    types_set_fields(types, t, &field, 1);
    t->layout = LAYOUT_DONE;
    return t;
}

struct type *types_mutex(struct types *types)
{
    static const char name_text[] = LANG_MUTEX;

    if (types->mutex == NULL) {
        types->mutex = handle_struct(types, name_text, NULL);
    }
    return types->mutex;
}

struct type *types_chan(struct types *types, struct type *element)
{
    static const char name_text[] = LANG_CHAN;
    struct type *t;

    for (t = types->derived; t != NULL; t = t->next) {
        if (types_is_chan(t) && t->element == element) {
            return t;
        }
    }
    return handle_struct(types, name_text, element);
}

/* Whether t is the struct of anti.lang named name. */
static bool lang_struct(const struct type *t, const char *name)
{
    size_t length = strlen(name);

    return t != NULL && t->kind == TYPE_STRUCT &&
           t->name.length == length &&
           memcmp(t->name.text, name, length) == 0 &&
           t->module.length == sizeof LANG_MODULE - 1 &&
           memcmp(t->module.text, LANG_MODULE, sizeof LANG_MODULE - 1) == 0;
}

bool types_is_mutex(const struct type *t)
{
    return lang_struct(t, LANG_MUTEX);
}

bool types_is_chan(const struct type *t)
{
    return lang_struct(t, LANG_CHAN) && t->element != NULL;
}

bool types_is_field_descriptor(const struct type *t)
{
    return lang_struct(t, LANG_FIELD_DESCRIPTOR);
}

bool types_is_flags(const struct type *t)
{
    return t != NULL && t->kind == TYPE_STRUCT && t->result == NULL &&
           t->name.length == sizeof LANG_FLAGS - 1 &&
           memcmp(t->name.text, LANG_FLAGS, sizeof LANG_FLAGS - 1) == 0 &&
           t->module.length == sizeof LANG_MODULE - 1 &&
           memcmp(t->module.text, LANG_MODULE, sizeof LANG_MODULE - 1) == 0;
}

bool types_is_job(const struct type *t)
{
    return t != NULL && t->kind == TYPE_STRUCT && t->result != NULL &&
           t->name.length == sizeof LANG_JOB - 1 &&
           memcmp(t->name.text, LANG_JOB, sizeof LANG_JOB - 1) == 0 &&
           t->module.length == sizeof LANG_MODULE - 1 &&
           memcmp(t->module.text, LANG_MODULE, sizeof LANG_MODULE - 1) == 0;
}

/* The name of element i of a tuple, `_0` upwards, in the memory pool of
   the compilation. */
static struct name element_name(struct types *types, size_t i)
{
    char digits[24];
    struct name name;
    int n = snprintf(digits, sizeof digits, "_%zu", i);
    char *text = arena_alloc(types->arena, (size_t)n + 1);

    memcpy(text, digits, (size_t)n + 1);
    name.text = text;
    name.length = (size_t)n;
    return name;
}

struct type *types_tuple(struct types *types, struct type **elements,
                         size_t count)
{
    struct struct_field *fields;
    struct type key = {0};
    struct type *t;
    size_t i;

    key.kind = TYPE_TUPLE;
    key.params = elements;
    key.param_count = count;
    for (t = types->derived; t != NULL; t = t->next) {
        if (t->kind != TYPE_TUPLE || t->param_count != count) {
            continue;
        }
        for (i = 0; i < count && t->params[i] == elements[i]; i++) {
        }
        if (i == count) {
            return t;
        }
    }
    t = arena_alloc(types->arena, sizeof *t);
    *t = key;
    t->params = arena_alloc(types->arena, count * sizeof *t->params);
    memcpy(t->params, elements, count * sizeof *t->params);
    fields = arena_alloc(types->arena, count * sizeof *fields);
    memset(fields, 0, count * sizeof *fields);
    for (i = 0; i < count; i++) {
        fields[i].name = element_name(types, i);
        fields[i].type = elements[i];
        fields[i].vis = VIS_PUB;
    }
    t->next = types->derived;
    types->derived = t;
    types_set_fields(types, t, fields, count);
    return t;
}

struct type *types_struct(struct types *types, struct name module,
                          struct name name)
{
    struct type *t = arena_alloc(types->arena, sizeof *t);

    t->kind = TYPE_STRUCT;
    t->module = module;
    t->name = name;
    return t;
}

struct type *types_mask(struct types *types, struct type *s)
{
    struct struct_field *fields;
    struct text text = {0};
    struct type *m;
    char *name;
    size_t i;

    if (s->mask != NULL || type_is_mask(s)) {
        return s->mask != NULL ? s->mask : s;
    }
    /* DESIGN: the mask carries the module of its simd struct and a name
       that no declaration can spell. Every module that compares values
       of s therefore names one type. */
    text_appendf(&text, "mask(%.*s)", (int)s->name.length, s->name.text);
    name = arena_alloc(types->arena, text.length + 1);
    memcpy(name, text_cstr(&text), text.length + 1);
    text_free(&text);
    m = arena_alloc(types->arena, sizeof *m);
    memset(m, 0, sizeof *m);
    m->kind = TYPE_STRUCT;
    m->module = s->module;
    m->name.text = name;
    m->name.length = strlen(name);
    m->simd = true;
    fields = arena_alloc(types->arena, (s->field_count + 1) * sizeof *fields);
    memset(fields, 0, (s->field_count + 1) * sizeof *fields);
    for (i = 0; i < s->field_count; i++) {
        fields[i].name = s->fields[i].name;
        fields[i].pos = s->fields[i].pos;
        fields[i].type = types_builtin(types, TYPE_BOOL);
        fields[i].vis = VIS_PUB;
    }
    types_set_fields(types, m, fields, s->field_count);
    m->layout = LAYOUT_DONE;
    s->mask = m;
    return m;
}

bool type_is_simd(const struct type *t)
{
    return t != NULL && t->kind == TYPE_STRUCT && t->simd &&
           t->field_count > 0;
}

bool type_is_mask(const struct type *t)
{
    return type_is_simd(t) && t->fields[0].type->kind == TYPE_BOOL;
}

struct type *type_simd_lane(const struct type *t)
{
    return t->fields[0].type;
}

uint64_t type_lane_bytes(const struct type *t)
{
    switch (t->kind) {
    case TYPE_BOOL:
    case TYPE_I8:
    case TYPE_U8: return 1;
    case TYPE_I16:
    case TYPE_U16:
    case TYPE_F16: return 2;
    case TYPE_CHAR:
    case TYPE_I32:
    case TYPE_U32:
    case TYPE_F32: return 4;
    case TYPE_I64:
    case TYPE_U64:
    case TYPE_F64: return 8;
    default: return 0;
    }
}

uint64_t type_simd_bytes(const struct type *t)
{
    return type_lane_bytes(type_simd_lane(t)) * t->field_count;
}

static struct name name_of(const char *text)
{
    struct name n;

    n.text = text;
    n.length = strlen(text);
    return n;
}

static bool same_name_text(const struct name *a, const struct name *b)
{
    return a->length == b->length && memcmp(a->text, b->text, a->length) == 0;
}

void types_set_cases(struct types *types, struct type *v, struct type *tag,
                     struct type **payloads, size_t count)
{
    struct struct_field fields[2];
    struct struct_field *members;
    struct type *u;
    size_t used = 0;
    size_t i;

    memset(fields, 0, sizeof fields);
    v->base = tag;
    v->params = arena_alloc(types->arena, (count + 1) * sizeof *v->params);
    memcpy(v->params, payloads, count * sizeof *v->params);
    v->param_count = count;
    fields[0].name = name_of(VARIANT_TAG);
    fields[0].type = tag;
    fields[0].vis = VIS_PUB;
    members = arena_alloc(types->arena, (count + 1) * sizeof *members);
    for (i = 0; i < count; i++) {
        if (payloads[i] == NULL) {
            continue;
        }
        memset(&members[used], 0, sizeof members[used]);
        members[used].name = tag->fields[i].name;
        members[used].type = payloads[i];
        members[used].vis = VIS_PUB;
        used++;
    }
    if (used == 0) {
        types_set_fields(types, v, fields, 1);
        return;
    }
    /* DESIGN: the union is named `T.union`, which no case can be, since
       `union` is a keyword. Its IR aggregate is then distinct from the
       struct of every case. */
    {
        char *text = arena_alloc(types->arena, v->name.length + 7);
        memcpy(text, v->name.text, v->name.length);
        memcpy(text + v->name.length, ".union", 7);
        u = types_struct(types, v->module, name_of(text));
    }
    u->is_union = true;
    u->packed = v->packed;
    types_set_fields(types, u, members, used);
    fields[1].name = name_of(VARIANT_UNION);
    fields[1].type = u;
    fields[1].vis = VIS_PUB;
    types_set_fields(types, v, fields, 2);
}

bool types_cases_from_fields(struct types *types, struct type *v)
{
    const struct type *tag;
    const struct type *u;
    size_t i;
    size_t j;

    if (v->field_count < 1 || v->field_count > 2 ||
        v->fields[0].type->kind != TYPE_ENUM) {
        return false;
    }
    tag = v->fields[0].type;
    u = v->field_count == 2 ? v->fields[1].type : NULL;
    if (u != NULL && (u->kind != TYPE_STRUCT || !u->is_union)) {
        return false;
    }
    v->base = v->fields[0].type;
    v->param_count = tag->field_count;
    v->params = arena_alloc(types->arena,
                            (tag->field_count + 1) * sizeof *v->params);
    for (i = 0; i < tag->field_count; i++) {
        if (tag->fields[i].number != i) {
            return false;
        }
        for (j = 0; u != NULL && j < u->field_count; j++) {
            if (same_name_text(&u->fields[j].name, &tag->fields[i].name)) {
                v->params[i] = u->fields[j].type;
            }
        }
    }
    return true;
}

int types_case_index(const struct type *v, const struct name *name)
{
    size_t i;

    for (i = 0; v->base != NULL && i < v->base->field_count; i++) {
        if (same_name_text(&v->base->fields[i].name, name)) {
            return (int)i;
        }
    }
    return -1;
}

/* A named integer type. Its values live in its fields, each with its
   name and the expression that gives it a number. */
struct type *types_enum(struct types *types, struct name module,
                        struct name name, struct type *base)
{
    struct type *t = arena_alloc(types->arena, sizeof *t);

    t->kind = TYPE_ENUM;
    t->module = module;
    t->name = name;
    t->base = base;
    return t;
}

void types_set_fields(struct types *types, struct type *s,
                      const struct struct_field *fields, size_t count)
{
    size_t i;

    s->fields = arena_alloc(types->arena, count * sizeof *s->fields);
    memcpy(s->fields, fields, count * sizeof *s->fields);
    s->field_count = count;
    /* Every field remembers the type that declares it, so a flattened
       chain still says which class a name came from. */
    for (i = 0; i < count; i++) {
        s->fields[i].home = s;
    }
}

static struct type *cycle_in(struct type *t);

/* The struct that a symbolic value measures with size_of and that holds
   the struct being checked. */
static struct type *cycle_in_symbolic(const struct symbolic *s)
{
    struct type *cycle = NULL;

    if (s == NULL) {
        return NULL;
    }
    if (s->kind == SYMBOLIC_SIZE_OF) {
        return cycle_in(s->of);
    }
    cycle = cycle_in_symbolic(s->a);
    return cycle != NULL ? cycle : cycle_in_symbolic(s->b);
}

/* A struct inside a value of type t that contains itself. Pointers,
   slices and function pointers hold no value of their element. t is NULL
   for the base field of a class whose base was refused. */
static struct type *cycle_in(struct type *t)
{
    struct type *cycle;

    while (t != NULL && t->kind == TYPE_ARRAY) {
        if ((cycle = cycle_in_symbolic(t->length_of)) != NULL) {
            return cycle;
        }
        t = t->element;
    }
    return type_has_fields(t) ? types_find_cycle(t) : NULL;
}

struct type *types_find_cycle(struct type *s)
{
    struct type *cycle;
    size_t i;

    if (s->layout == LAYOUT_DONE) {
        return NULL;
    }
    if (s->layout == LAYOUT_BUSY) {
        return s;
    }
    s->layout = LAYOUT_BUSY;
    for (i = 0; i < s->field_count; i++) {
        if ((cycle = cycle_in(s->fields[i].type)) != NULL) {
            s->layout = LAYOUT_NONE;
            return cycle;
        }
    }
    s->layout = LAYOUT_DONE;
    return NULL;
}

/* A walk in depth from s. A field closes a cycle when its value holds a
   struct still on the path, or its array length measures a struct that
   contains itself. */
void types_break_cycles(struct type *s, struct type *error)
{
    size_t i;

    if (s->layout != LAYOUT_NONE) {
        return;
    }
    s->layout = LAYOUT_BUSY;
    for (i = 0; i < s->field_count; i++) {
        struct type *t = s->fields[i].type;
        bool closes = false;
        while (t != NULL && t->kind == TYPE_ARRAY && !closes) {
            closes = cycle_in_symbolic(t->length_of) != NULL;
            t = t->element;
        }
        if (!closes && type_has_fields(t)) {
            closes = t->layout == LAYOUT_BUSY;
            types_break_cycles(t, error);
        }
        if (closes) {
            s->fields[i].type = error;
        }
    }
    s->layout = LAYOUT_DONE;
}

void type_name(struct text *out, const struct type *t)
{
    print_type(out, t, false);
}

void type_name_qualified(struct text *out, const struct type *t)
{
    print_type(out, t, true);
}

static void print_type(struct text *out, const struct type *t, bool qualified)
{
    static const char *const builtin_names[TYPE_BUILTIN_COUNT] = {
        [TYPE_VOID] = "void", [TYPE_BOOL] = "bool", [TYPE_CHAR] = "char",
        [TYPE_I8] = "i8", [TYPE_I16] = "i16", [TYPE_I32] = "i32",
        [TYPE_I64] = "int", [TYPE_U8] = "byte", [TYPE_U16] = "u16",
        [TYPE_U32] = "u32", [TYPE_U64] = "u64", [TYPE_F16] = "f16",
        [TYPE_F32] = "f32",
        [TYPE_CLONG] = "c_long", [TYPE_CULONG] = "c_ulong",
        [TYPE_CWCHAR] = "c_wchar",
        [TYPE_F64] = "float", [TYPE_STR] = "str", [TYPE_NONE] = "none",
        [TYPE_ERROR] = "<error>",
    };
    size_t i;

    /* A channel is written as the program writes its type. */
    if (types_is_chan(t)) {
        text_append(out, LANG_CHAN " ");
        print_type(out, t->element, qualified);
        return;
    }
    switch (t->kind) {
    case TYPE_POINTER:
        text_append(out, t->nullable ? "?*" : "*");
        print_type(out, t->element, qualified);
        return;
    case TYPE_SLICE:
        text_append(out, "[]");
        print_type(out, t->element, qualified);
        return;
    case TYPE_ARRAY:
        if (t->length_of != NULL) {
            text_append(out, "[");
            symbolic_print(out, t->length_of, qualified);
            text_append(out, "]");
        } else {
            text_appendf(out, "[%llu]", (unsigned long long)t->length);
        }
        print_type(out, t->element, qualified);
        return;
    case TYPE_FN: {
        /* A failing type is written as the program writes it, without
           the out pointer and the error of its ABI form. */
        size_t shown = t->param_count - (t->has_out ? 1 : 0);
        const struct type *result =
            t->has_out ? t->params[shown]->element : t->result;
        if (t->nullable) {
            text_append(out, "?");
        }
        text_append(out, t->bound ? "bound fn(" : "fn(");
        for (i = 0; i < shown; i++) {
            if (i > 0) {
                text_append(out, ", ");
            }
            print_type(out, t->params[i], qualified);
        }
        text_append(out, ")");
        if (t->has_out || (!t->may_fail && result->kind != TYPE_VOID)) {
            text_append(out, " -> ");
            print_type(out, result, qualified);
        }
        if (t->may_fail) {
            text_append(out, " may fail");
        }
        return;
    }
    case TYPE_TUPLE:
        text_append(out, "(");
        for (i = 0; i < t->param_count; i++) {
            text_append(out, i > 0 ? ", " : "");
            print_type(out, t->params[i], qualified);
        }
        text_append(out, ")");
        return;
    case TYPE_STRUCT:
    case TYPE_CLASS:
    case TYPE_ENUM:
    case TYPE_VARIANT:
        if (qualified) {
            text_appendf(out, "%.*s.", (int)t->module.length, t->module.text);
        }
        text_appendf(out, "%.*s", (int)t->name.length, t->name.text);
        return;
    default:
        text_append(out, builtin_names[t->kind]);
        return;
    }
}

bool type_is_integer(const struct type *t)
{
    return t->kind >= TYPE_I8 && t->kind <= TYPE_CWCHAR;
}

bool type_has_fields(const struct type *t)
{
    return t != NULL && (t->kind == TYPE_STRUCT || t->kind == TYPE_CLASS ||
                         t->kind == TYPE_TUPLE || t->kind == TYPE_VARIANT);
}

bool type_field_is_unit_break(const struct struct_field *f)
{
    return f->name.length == 1 && f->name.text[0] == '_';
}

bool type_is_target_sized(const struct type *t)
{
    return t->kind == TYPE_CLONG || t->kind == TYPE_CULONG ||
           t->kind == TYPE_CWCHAR;
}

bool type_is_signed(const struct type *t)
{
    return t->kind >= TYPE_I8 && t->kind <= TYPE_CLONG;
}

bool type_is_float(const struct type *t)
{
    return t->kind == TYPE_F32 || t->kind == TYPE_F64;
}

bool type_is_numeric(const struct type *t)
{
    return type_is_integer(t) || type_is_float(t);
}

int type_bits(const struct type *t)
{
    switch (t->kind) {
    case TYPE_I8:
    case TYPE_U8: return 8;
    case TYPE_I16:
    case TYPE_U16:
    case TYPE_CWCHAR: return 16;
    case TYPE_I32:
    case TYPE_U32:
    case TYPE_CLONG:
    case TYPE_CULONG:
    case TYPE_F32: return 32;
    case TYPE_I64:
    case TYPE_U64:
    case TYPE_F64: return 64;
    default: return 0;
    }
}

bool type_pointer_free(const struct type *t)
{
    size_t i;

    switch (t->kind) {
    case TYPE_POINTER:
    case TYPE_SLICE:
    case TYPE_FN:
        return false;
    case TYPE_ARRAY:
        return type_pointer_free(t->element);
    case TYPE_STRUCT:
    case TYPE_CLASS:
    case TYPE_TUPLE:
    case TYPE_VARIANT:
        /* DESIGN: a Mutex and a channel are handles of objects made to
           be shared between threads, so a worker takes them beside its
           values. A worker that holds a copy names the same one. */
        if (types_is_mutex(t) || types_is_chan(t)) {
            return true;
        }
        /* DESIGN: the pointer-free test of `parallel` exempts the table
           pointer and the `own` fields of a class. The table is read-only
           data that every object of the class shares, and an `own` field
           belongs to one object, so no two chunks reach the same memory
           through either. A class whose other fields are pointer-free is
           therefore pointer-free, and `[]Circle` chunks like any array. */
        for (i = 0; i < t->field_count; i++) {
            if (t->fields[i].form == FIELD_TABLE || t->fields[i].owned) {
                continue;
            }
            if (!type_pointer_free(t->fields[i].type)) {
                return false;
            }
        }
        return true;
    default:
        return true;
    }
}

/* The writer of anti bind: the module, the shim and the two probes. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bindmodel.h"
#include "modpath.h"

/* Where a type stands, which decides how an array and `void` read. */
enum place { AS_FIELD, AS_PARAM, AS_RESULT };

static bool spell(const struct bind_type *t, enum place place,
                  struct text *out);

/* DESIGN: a C function pointer is an Anti function type, `?fn(...)`,
   since C lets any of them be NULL. A variadic one has no Anti type, and
   neither has one whose parameters have none. Both become `?*byte`, which
   has the size and the passing of every pointer. */
static void spell_function(const struct bind_type *f, struct text *out)
{
    struct text params = {0};
    struct text result = {0};
    size_t i;
    bool ok = !f->variadic;

    for (i = 0; ok && i < f->param_count; i++) {
        if (i > 0) {
            text_append(&params, ", ");
        }
        ok = spell(f->params[i], AS_PARAM, &params);
    }
    if (ok && f->to->kind != BIND_VOID) {
        ok = spell(f->to, AS_RESULT, &result);
    }
    if (!ok) {
        text_append(out, "?*byte");
    } else if (f->to->kind == BIND_VOID) {
        text_appendf(out, "?fn(%s)", text_cstr(&params));
    } else {
        text_appendf(out, "?fn(%s) -> %s", text_cstr(&params),
                     text_cstr(&result));
    }
    text_free(&params);
    text_free(&result);
}

/* DESIGN: every pointer of a binding is `?*T`, the rule for every
   pointer of an `extern fn`, because C lets each one be NULL. `char *`
   and `void *` are `?*byte`. A pointer to a type Anti cannot name is
   `?*byte` too, since a pointer passes the same whatever it points at. */
static void spell_pointer(const struct bind_type *to, struct text *out)
{
    struct text inner = {0};

    switch (to->kind) {
    case BIND_VOID:
    case BIND_OPAQUE:
    case BIND_UNSUPPORTED:
    case BIND_VALIST:
    case BIND_ARRAY:
        text_append(out, "?*byte");
        return;
    case BIND_SCALAR:
        if (strcmp(to->name, "c_char") == 0) {
            text_append(out, "?*byte");
            return;
        }
        break;
    case BIND_FUNCTION:
        spell_function(to, out);
        return;
    default:
        break;
    }
    if (spell(to, AS_FIELD, &inner)) {
        text_appendf(out, "?*%s", text_cstr(&inner));
    } else {
        text_append(out, "?*byte");
    }
    text_free(&inner);
}

static bool spell(const struct bind_type *t, enum place place,
                  struct text *out)
{
    switch (t->kind) {
    case BIND_SCALAR:
    case BIND_ENUM:
        text_append(out, t->name);
        return true;
    case BIND_BOOL:
        text_append(out, "bool");
        return true;
    case BIND_RECORD:
        if (!t->record->bound) {
            return false;
        }
        text_append(out, t->record->name);
        return true;
    case BIND_POINTER:
        spell_pointer(t->to, out);
        return true;
    case BIND_ARRAY:
        /* An array parameter of C is a pointer to its first element. */
        if (place == AS_PARAM) {
            spell_pointer(t->to, out);
            return true;
        }
        if (place != AS_FIELD || t->length < 1) {
            return false;
        }
        text_appendf(out, "[%lld]", (long long)t->length);
        return spell(t->to, AS_FIELD, out);
    case BIND_FUNCTION:
        if (place != AS_PARAM) {
            return false;
        }
        spell_function(t, out);
        return true;
    case BIND_VALIST:
        /* DESIGN: a `va_list` parameter is a pointer on every target. It
           is a `char *` on macOS and Windows and an array that decays on
           x86-64 Linux. On ARM64 Linux it is a composite above 16 bytes,
           which AAPCS64 passes by reference. */
        if (place != AS_PARAM) {
            return false;
        }
        text_append(out, "?*byte");
        return true;
    default:
        return false;
    }
}

static bool record_fields_spell(const struct bind_record *r,
                                const char **why)
{
    struct text scratch = {0};
    size_t i;
    bool ok = true;

    for (i = 0; ok && i < r->field_count; i++) {
        scratch.length = 0;
        if (!spell(r->fields[i].type, AS_FIELD, &scratch)) {
            *why = r->fields[i].name != NULL ? r->fields[i].name : "_";
            ok = false;
        }
    }
    text_free(&scratch);
    return ok;
}

void bind_settle(struct bind_module *b)
{
    struct text scratch = {0};
    bool changed = true;
    size_t i;
    size_t j;

    for (i = 0; i < b->records.count; i++) {
        struct bind_record *r = b->records.items[i];
        r->bound = r->complete && r->field_count > 0;
    }
    /* A record whose field names a record that is left out is left out
       too, until nothing changes. */
    while (changed) {
        changed = false;
        for (i = 0; i < b->records.count; i++) {
            struct bind_record *r = b->records.items[i];
            const char *why;
            if (r->bound && !record_fields_spell(r, &why)) {
                r->bound = false;
                changed = true;
            }
        }
    }
    for (i = 0; i < b->records.count; i++) {
        struct bind_record *r = b->records.items[i];
        const char *why = NULL;
        if (!r->complete) {
            continue;
        }
        if (r->field_count == 0) {
            bind_warn(b, "`%s` has no field and is left out", r->name);
        } else if (!r->bound && !record_fields_spell(r, &why)) {
            bind_warn(b, "`%s` is left out: Anti has no form for the type "
                      "of its field `%s`", r->name, why);
        }
    }
    for (i = 0; i < b->functions.count; i++) {
        struct bind_function *f = b->functions.items[i];
        const char *why = NULL;
        scratch.length = 0;
        if (bind_is_keyword(f->name)) {
            why = "its name is a word of Anti";
        } else if (f->result->kind != BIND_VOID &&
                   !spell(f->result, AS_RESULT, &scratch)) {
            why = "Anti has no form for its result";
        } else if (f->shim != SHIM_NONE && f->variadic) {
            why = "a shim cannot pass on the arguments of `...`";
        } else if (f->shim == SHIM_STATIC && f->c_result != NULL &&
                   strchr(f->c_result, '(') != NULL) {
            why = "a shim cannot write a function that returns a function";
        }
        for (j = 0; why == NULL && j < f->param_count; j++) {
            scratch.length = 0;
            if (!spell(f->params[j].type, AS_PARAM, &scratch)) {
                why = "Anti has no form for the type of a parameter";
            }
        }
        f->bound = why == NULL;
        if (why != NULL) {
            bind_warn(b, "`%s` is left out: %s", f->name, why);
        }
    }
    for (i = 0; i < b->consts.count; i++) {
        struct bind_const *c = b->consts.items[i];
        c->bound = c->record == NULL || c->record->bound;
        if (!c->bound) {
            bind_warn(b, "`%s` is left out: its struct is left out", c->name);
        }
    }
    text_free(&scratch);
}

/* A name as the module writes it. A C name that is a word of Anti takes
   a trailing `_`, which changes no layout and no call. */
static void name_of(const char *name, struct text *out)
{
    text_append(out, name);
    if (bind_is_keyword(name)) {
        text_append(out, "_");
    }
}

static void doc_of(const char *doc, const char *indent, struct text *out)
{
    const char *line = doc;

    if (doc == NULL || doc[0] == '\0') {
        return;
    }
    while (*line != '\0') {
        const char *end = strchr(line, '\n');
        size_t n = end != NULL ? (size_t)(end - line) : strlen(line);
        text_appendf(out, "%s/// %.*s\n", indent, (int)n, line);
        line += n;
        if (*line == '\n') {
            line++;
        }
    }
}

static void write_record(const struct bind_record *r, struct text *out)
{
    size_t i;

    doc_of(r->doc, "", out);
    text_append(out, "pub ");
    if (r->packed) {
        text_append(out, "packed ");
    }
    text_appendf(out, "%s %s", r->is_union ? "union" : "struct", r->name);
    if (r->align > 0) {
        text_appendf(out, " align(%lld)", (long long)r->align);
    }
    text_append(out, "\n{\n");
    for (i = 0; i < r->field_count; i++) {
        const struct bind_field *f = &r->fields[i];
        if (f->c_type != NULL && strstr(f->c_type, "volatile") != NULL) {
            text_append(out, "\t// volatile in C\n");
        }
        text_append(out, "\t");
        if (f->name == NULL) {
            text_append(out, "_");
        } else {
            name_of(f->name, out);
        }
        text_append(out, ": ");
        spell(f->type, AS_FIELD, out);
        if (f->bits >= 0) {
            text_appendf(out, " : %lld", (long long)f->bits);
        }
        text_append(out, ",\n");
    }
    text_append(out, "}\n");
}

static void write_enum(const struct bind_enum *e, struct text *out)
{
    size_t i;

    doc_of(e->doc, "", out);
    text_appendf(out, "pub enum %s", e->name);
    if (e->base != NULL) {
        text_appendf(out, ": %s", e->base);
    }
    text_append(out, "\n{\n");
    for (i = 0; i < e->value_count; i++) {
        doc_of(e->values[i].doc, "\t", out);
        text_append(out, "\t");
        name_of(e->values[i].name, out);
        text_appendf(out, " = %lld,\n", (long long)e->values[i].value);
    }
    text_append(out, "}\n");
}

static void write_function(const struct bind_function *f, struct text *out)
{
    size_t i;
    bool volatile_param = false;

    doc_of(f->doc, "", out);
    for (i = 0; i < f->param_count; i++) {
        if (f->params[i].c_type != NULL &&
            strstr(f->params[i].c_type, "volatile") != NULL) {
            volatile_param = true;
        }
    }
    if (volatile_param) {
        text_append(out, "// A parameter is volatile in C.\n");
    }
    text_appendf(out, "pub extern fn %s(", f->name);
    for (i = 0; i < f->param_count; i++) {
        if (i > 0) {
            text_append(out, ", ");
        }
        if (f->params[i].name != NULL && f->params[i].name[0] != '\0') {
            name_of(f->params[i].name, out);
        } else {
            text_appendf(out, "p%zu", i);
        }
        text_append(out, ": ");
        spell(f->params[i].type, AS_PARAM, out);
    }
    if (f->variadic) {
        text_append(out, f->param_count > 0 ? ", ..." : "...");
    }
    text_append(out, ")");
    if (f->result->kind != BIND_VOID) {
        text_append(out, " -> ");
        spell(f->result, AS_RESULT, out);
    }
    text_append(out, ";\n");
}

void bind_write_module(const struct bind_module *b, struct text *out)
{
    const char *const *frameworks;
    size_t count = bind_frameworks(b->library, &frameworks);
    size_t i;

    text_appendf(out, "//! The binding of %s, which `anti bind` wrote from "
                 "`%s`.\n//! Nothing in it is written by hand. Run `anti "
                 "bind` again to change it.\n", b->library, b->source);
    if (count > 0) {
        text_append(out, "\n");
        for (i = 0; i < count; i++) {
            text_appendf(out, "link framework \"%s\";\n", frameworks[i]);
        }
    }
    for (i = 0; i < b->consts.count; i++) {
        const struct bind_const *c = b->consts.items[i];
        if (!c->bound) {
            continue;
        }
        text_append(out, "\n");
        doc_of(c->doc, "", out);
        text_appendf(out, "pub const %s: %s = %s;\n", c->name, c->type,
                     c->value);
    }
    for (i = 0; i < b->enums.count; i++) {
        text_append(out, "\n");
        write_enum(b->enums.items[i], out);
    }
    for (i = 0; i < b->records.count; i++) {
        const struct bind_record *r = b->records.items[i];
        if (r->bound) {
            text_append(out, "\n");
            write_record(r, out);
        }
    }
    for (i = 0; i < b->functions.count; i++) {
        const struct bind_function *f = b->functions.items[i];
        if (f->bound) {
            text_append(out, "\n");
            write_function(f, out);
        }
    }
}

bool bind_needs_shim(const struct bind_module *b)
{
    size_t i;

    for (i = 0; i < b->functions.count; i++) {
        const struct bind_function *f = b->functions.items[i];
        if (f->bound && f->shim != SHIM_NONE) {
            return true;
        }
    }
    return false;
}

/* The prefix that a `static` function of the header takes in the shim,
   so that the wrapper can have its name. */
#define SHIM_PREFIX "anti_inline_"

/* A declaration of C: the spelling of a type with a name placed where C
   wants it. `void (*)(int)` and `a` give `void (*a)(int)`, and `float[4]`
   gives `float a[4]`. */
static void declare(const char *c_type, const char *name, struct text *out)
{
    const char *group = strstr(c_type, "(*");
    const char *bracket = strchr(c_type, '[');

    if (group != NULL) {
        text_appendf(out, "%.*s%s%s", (int)(group + 2 - c_type), c_type, name,
                     group + 2);
    } else if (bracket != NULL) {
        size_t n = (size_t)(bracket - c_type);
        while (n > 0 && c_type[n - 1] == ' ') {
            n--;
        }
        text_appendf(out, "%.*s %s%s", (int)n, c_type, name, bracket);
    } else {
        size_t n = strlen(c_type);
        text_appendf(out, "%s%s%s", c_type,
                     n > 0 && c_type[n - 1] == '*' ? "" : " ", name);
    }
}

static void write_defines(const struct bind_module *b, struct text *out)
{
    size_t i;

    for (i = 0; i < b->define_count; i++) {
        const char *d = b->defines[i];
        const char *eq = strchr(d, '=');
        if (eq != NULL) {
            text_appendf(out, "#define %.*s %s\n", (int)(eq - d), d, eq + 1);
        } else {
            text_appendf(out, "#define %s\n", d);
        }
    }
}

/* DESIGN: a `static` function is renamed by a macro around the include
   and gets a wrapper of its own name that calls it. A C99 `inline`
   definition has an external definition in the translation unit that
   declares it `extern` (C11 6.7.4p7). The shim declares it, and the
   definition of the header becomes the symbol. The shim then holds no
   copy of a body and follows the header on every change. */
void bind_write_shim(const struct bind_module *b, struct text *out)
{
    size_t i;
    size_t j;

    text_appendf(out, "/* The shim of %s, which anti bind wrote from %s. It "
                 "gives every\n   inline function of the header a symbol. "
                 "Nothing in it is written by\n   hand. */\n", b->library,
                 b->source);
    write_defines(b, out);
    for (i = 0; i < b->functions.count; i++) {
        const struct bind_function *f = b->functions.items[i];
        if (f->bound && f->shim == SHIM_STATIC) {
            text_appendf(out, "#define %s " SHIM_PREFIX "%s\n", f->name,
                         f->name);
        }
    }
    text_appendf(out, "#include \"%s\"\n", b->header);
    for (i = 0; i < b->functions.count; i++) {
        const struct bind_function *f = b->functions.items[i];
        if (f->bound && f->shim == SHIM_STATIC) {
            text_appendf(out, "#undef %s\n", f->name);
        }
    }
    for (i = 0; i < b->functions.count; i++) {
        const struct bind_function *f = b->functions.items[i];
        if (!f->bound || f->shim == SHIM_NONE) {
            continue;
        }
        text_append(out, "\n");
        if (f->shim == SHIM_INLINE) {
            text_appendf(out, "extern __typeof__(%s) %s;\n", f->name,
                         f->name);
            continue;
        }
        text_appendf(out, "%s %s(", f->c_result, f->name);
        if (f->param_count == 0) {
            text_append(out, "void");
        }
        for (j = 0; j < f->param_count; j++) {
            struct text name = {0};
            text_appendf(&name, "a%zu", j);
            if (j > 0) {
                text_append(out, ", ");
            }
            declare(f->params[j].c_type, text_cstr(&name), out);
            text_free(&name);
        }
        text_appendf(out, ")\n{\n    %s" SHIM_PREFIX "%s(",
                     f->result->kind == BIND_VOID ? "" : "return ", f->name);
        for (j = 0; j < f->param_count; j++) {
            text_appendf(out, "%sa%zu", j > 0 ? ", " : "", j);
        }
        text_append(out, ");\n}\n");
    }
}

/* The probes. */

/* DESIGN: the walk of settable follows a record into its fields. A
   struct of raylib_api.json may hold itself or hold records nested
   without end, which C refuses and the reader does not check. The walk
   enters no record it stands in and goes no deeper than this bound. The
   field then has no value in the probe. A record that held no value is
   never entered again. Records that each hold two of the next then take
   one walk each, not two to the power of the depth. */
#define RECORD_DEPTH 64

/* One field of a probe: the path to a value inside it and the value in
   C and in Anti. A field that holds no value the probe can set, such as a
   pointer, has none. */
struct setting {
    struct text c_path;
    struct text anti_path;          /* with the names the module writes */
    struct text c_value;
    struct text anti_value;
    /* The records the walk stands in, and the records that hold no
       value, which it does not enter again. */
    const struct bind_record *path[RECORD_DEPTH];
    size_t path_count;
    struct bind_list empty;
};

static const struct bind_enum *enum_named(const struct bind_module *b,
                                          const char *name)
{
    size_t i;

    for (i = 0; i < b->enums.count; i++) {
        const struct bind_enum *e = b->enums.items[i];
        if (strcmp(e->name, name) == 0) {
            return e;
        }
    }
    return NULL;
}

static void cut(struct text *t, size_t length)
{
    t->length = length;
    if (t->data != NULL) {
        t->data[length] = '\0';
    }
}

static bool on_path(const struct setting *s, const struct bind_record *r)
{
    size_t i;

    for (i = 0; i < s->path_count; i++) {
        if (s->path[i] == r) {
            return true;
        }
    }
    return false;
}

static bool held_none(const struct setting *s, const struct bind_record *r)
{
    size_t i;

    for (i = 0; i < s->empty.count; i++) {
        const void *item = s->empty.items[i];
        if (item == r) {
            return true;
        }
    }
    return false;
}

static bool settable(const struct bind_module *b, const struct bind_type *t,
                     int64_t bits, struct setting *s);

/* The first value of a field of r that is not zero, with the path to it. */
static bool settable_record(const struct bind_module *b,
                            const struct bind_record *r, struct setting *s)
{
    size_t i;

    if (s->path_count == RECORD_DEPTH || on_path(s, r) || held_none(s, r)) {
        return false;
    }
    s->path[s->path_count++] = r;
    for (i = 0; i < r->field_count; i++) {
        const struct bind_field *f = &r->fields[i];
        size_t c_mark = s->c_path.length;
        size_t anti_mark = s->anti_path.length;
        if (f->name == NULL) {
            continue;
        }
        text_appendf(&s->c_path, ".%s", f->name);
        text_append(&s->anti_path, ".");
        name_of(f->name, &s->anti_path);
        if (settable(b, f->type, f->bits, s)) {
            s->path_count--;
            return true;
        }
        cut(&s->c_path, c_mark);
        cut(&s->anti_path, anti_mark);
    }
    s->path_count--;
    bind_list_add(&s->empty, (void *)r);
    return false;
}

/* The first value of t that is not zero, with the path from t to it. */
static bool settable(const struct bind_module *b, const struct bind_type *t,
                     int64_t bits, struct setting *s)
{
    size_t i;

    switch (t->kind) {
    case BIND_SCALAR:
        /* A signed bitfield of one bit holds 0 and -1 alone. */
        if (bits == 1 && t->name[0] != 'u' && strncmp(t->name, "c_u", 3) != 0) {
            return false;
        }
        text_append(&s->c_value, "1");
        text_append(&s->anti_value,
                    strcmp(t->name, "c_float") == 0 ||
                            strcmp(t->name, "c_double") == 0
                        ? "1.0"
                        : "1");
        return true;
    case BIND_BOOL:
        text_append(&s->c_value, "1");
        text_append(&s->anti_value, "true");
        return true;
    case BIND_ENUM: {
        const struct bind_enum *e = enum_named(b, t->name);
        for (i = 0; e != NULL && i < e->value_count; i++) {
            if (e->values[i].value != 0) {
                text_append(&s->c_value, e->values[i].name);
                text_appendf(&s->anti_value, "%s.%s.", module_path_last(b->module),
                             e->name);
                text_append(&s->anti_value, e->values[i].name);
                return true;
            }
        }
        return false;
    }
    case BIND_ARRAY:
        text_append(&s->c_path, "[0]");
        text_append(&s->anti_path, "[0]");
        return settable(b, t->to, -1, s);
    case BIND_RECORD:
        return settable_record(b, t->record, s);
    default:
        return false;
    }
}

static void setting_free(struct setting *s)
{
    text_free(&s->c_path);
    text_free(&s->anti_path);
    text_free(&s->c_value);
    text_free(&s->anti_value);
    free(s->empty.items);
}

/* A record the probe reaches: one the module holds and C can name. An
   anonymous member of C11 has no name in C. */
static bool probed(const struct bind_record *r)
{
    return r->bound && r->c_name != NULL;
}

void bind_write_probe_c(const struct bind_module *b, struct text *out)
{
    size_t i;
    size_t j;

    text_appendf(out, "/* The ABI probe of %s, which anti bind wrote from %s. "
                 "For every\n   struct and union it prints the size, the "
                 "alignment and the bytes\n   of an object cleared and given "
                 "one field. probe_%s.anti prints\n   the same from the "
                 "binding. */\n", b->library, b->source, b->library);
    text_append(out, "#include <stdio.h>\n#include <string.h>\n\n");
    write_defines(b, out);
    text_appendf(out, "#include \"%s\"\n\n", b->header);
    text_append(out,
                "static void dump(const char *name, const void *value, "
                "size_t size)\n{\n    const unsigned char *p = value;\n"
                "    size_t i;\n\n    printf(\"%s\", name);\n"
                "    for (i = 0; i < size; i++) {\n"
                "        printf(\" %02x\", p[i]);\n    }\n"
                "    printf(\"\\n\");\n}\n\nint main(void)\n{\n");
    for (i = 0; i < b->records.count; i++) {
        const struct bind_record *r = b->records.items[i];
        if (!probed(r)) {
            continue;
        }
        text_appendf(out, "    printf(\"%s size %%d align %%d\\n\", "
                     "(int)sizeof(%s), (int)_Alignof(%s));\n", r->name,
                     r->c_name, r->c_name);
        for (j = 0; j < r->field_count; j++) {
            const struct bind_field *f = &r->fields[j];
            struct setting s;
            memset(&s, 0, sizeof s);
            if (f->name != NULL && settable(b, f->type, f->bits, &s)) {
                text_appendf(out, "    {\n        %s v;\n        memset(&v, 0, "
                             "sizeof v);\n        v.%s%s = %s;\n        "
                             "dump(\"%s.%s\", &v, sizeof v);\n    }\n",
                             r->c_name, f->name, text_cstr(&s.c_path),
                             text_cstr(&s.c_value), r->name, f->name);
            }
            setting_free(&s);
        }
    }
    text_append(out, "    return 0;\n}\n");
}

void bind_write_probe_anti(const struct bind_module *b, struct text *out)
{
    const char *m = module_path_last(b->module);
    size_t i;
    size_t j;

    text_appendf(out, "//! The ABI probe of %s, which `anti bind` wrote from "
                 "`%s`. It prints\n//! what `probe_%s.c` prints, from the "
                 "binding.\n\nimport %s;\n\n", b->library, b->source,
                 b->library, b->module);
    text_append(out, "extern fn printf(format: ?*byte, ...) -> c_int;\n");
    for (i = 0; i < b->records.count; i++) {
        const struct bind_record *r = b->records.items[i];
        if (probed(r)) {
            text_appendf(out, "\nstruct Box%s\n{\n\tc: u8,\n\tt: %s.%s,\n}\n",
                         r->name, m, r->name);
        }
    }
    text_append(out,
                "\nfn clear(p: *byte, n: int)\n{\n\tlet i = 0;\n"
                "\twhile i < n do {\n\t\tp[i] = 0;\n\t\ti += 1;\n\t}\n}\n"
                "\nfn dump(name: str, p: *byte, n: int)\n{\n"
                "\tprintf(\"%s\".ptr, name.ptr);\n\tlet i = 0;\n"
                "\twhile i < n do {\n"
                "\t\tprintf(\" %02x\".ptr, p[i] as c_int);\n\t\ti += 1;\n"
                "\t}\n\tprintf(\"\\n\".ptr);\n}\n"
                "\nfn main() -> int\n{\n");
    for (i = 0; i < b->records.count; i++) {
        const struct bind_record *r = b->records.items[i];
        if (!probed(r)) {
            continue;
        }
        text_appendf(out, "\tprintf(\"%s size %%d align %%d\\n\".ptr, "
                     "size_of(%s.%s) as c_int,\n\t\t(size_of(Box%s) - "
                     "size_of(%s.%s)) as c_int);\n", r->name, m, r->name,
                     r->name, m, r->name);
        text_appendf(out, "\tlet v%zu = alloc(%s.%s, 1) else { return 1; };\n",
                     i, m, r->name);
        for (j = 0; j < r->field_count; j++) {
            const struct bind_field *f = &r->fields[j];
            struct setting s;
            memset(&s, 0, sizeof s);
            if (f->name != NULL && settable(b, f->type, f->bits, &s)) {
                text_appendf(out, "\tclear(v%zu as *byte, size_of(%s.%s));\n"
                             "\tv%zu.", i, m, r->name, i);
                name_of(f->name, out);
                text_appendf(out, "%s = %s;\n\tdump(\"%s.%s\", v%zu as *byte, "
                             "size_of(%s.%s));\n", text_cstr(&s.anti_path),
                             text_cstr(&s.anti_value), r->name, f->name, i, m,
                             r->name);
            }
            setting_free(&s);
        }
        text_appendf(out, "\tfree(v%zu);\n", i);
    }
    text_append(out, "\treturn 0;\n}\n");
}

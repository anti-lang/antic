/* The reader of a C header through clang: the AST that clang dumps as
   JSON, and the macros and pragmas of the preprocessed text. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bindexpr.h"
#include "bindmodel.h"
#include "files.h"
#include "jsontree.h"
#include "process.h"
#include "selfpath.h"
#include "target.h"

/* DESIGN: anti bind accepts the major versions of clang it was tested
   against, which is the major version of the pinned clang. The JSON of
   the AST dump is not a stable interface. A version that renamed a field
   would bind a header wrong without a word. */
static const int accepted_majors[] = {23};

/* The clang to run. In a development tree it is the pinned clang, which
   the configure step installs into build/clang of the checkout. anti
   lies in a build directory of that checkout, so the pinned clang is
   build/clang/bin/clang under the directory above the one that holds
   anti. Elsewhere it is the first clang on PATH, which a bare name finds. */
static void find_clang(struct text *out)
{
    struct text dir = {0};
    enum target host;
    const char *suffix = "";
    char *slash;

    if (target_host(&host)) {
        suffix = target_info(host)->executable_suffix;
    }
    if (self_directory(&dir)) {
        slash = strrchr(dir.data, '/');
        if (slash != NULL) {
            struct text pinned = {0};
            text_appendf(&pinned, "%.*s/build/clang/bin/clang%s",
                         (int)(slash - dir.data), dir.data, suffix);
            if (path_exists(text_cstr(&pinned))) {
                text_append(out, text_cstr(&pinned));
                text_free(&pinned);
                text_free(&dir);
                return;
            }
            text_free(&pinned);
        }
    }
    text_free(&dir);
    text_append(out, "clang");
}

/* The major version that `clang --version` names in its first line, or
   -1. Apple's clang writes `Apple clang version`, and a distribution
   writes its own name before `clang version`. */
static int major_version(const char *text)
{
    const char *at = strstr(text, "clang version ");
    const char *newline = strchr(text, '\n');

    if (at == NULL || (newline != NULL && at > newline)) {
        return -1;
    }
    at += strlen("clang version ");
    if (*at < '0' || *at > '9') {
        return -1;
    }
    return atoi(at);
}

static bool check_version(const char *clang)
{
    const char *argv[] = {clang, "--version", NULL};
    struct text out = {0};
    int status = process_capture(argv, &out);
    int major;
    size_t i;

    if (status != 0) {
        fprintf(stderr, "anti: bind --clang runs clang, and `%s --version` "
                "failed. Install clang %d or put it on PATH.\n", clang,
                accepted_majors[0]);
        text_free(&out);
        return false;
    }
    major = major_version(text_cstr(&out));
    text_free(&out);
    for (i = 0; i < sizeof accepted_majors / sizeof accepted_majors[0]; i++) {
        if (major == accepted_majors[i]) {
            return true;
        }
    }
    fprintf(stderr, "anti: %s is clang %d, and anti bind was tested against "
            "clang", clang, major);
    for (i = 0; i < sizeof accepted_majors / sizeof accepted_majors[0]; i++) {
        fprintf(stderr, "%s %d", i > 0 ? "," : "", accepted_majors[i]);
    }
    fputs(" only. Put one of those first on PATH.\n", stderr);
    return false;
}

/* The options that aim clang at a target and at the headers of its
   sysroot in the runtime archive. The build compiles the runtime of that
   target with the same ones. */
static bool target_options(const char *clang, enum target t,
                           const char *runtime, struct bind_list *args,
                           struct bind_module *b)
{
    struct text sysroot = {0};
    const char *name = target_name(t);
    bool ok = true;

    text_appendf(&sysroot, "%s/sysroot/%s", runtime != NULL ? runtime : ".",
                 name);
    if (runtime == NULL || !directory_exists(text_cstr(&sysroot))) {
        fprintf(stderr, "anti: the runtime archive holds no sysroot for %s, "
                "whose headers clang reads. Pass --runtime.\n", name);
        text_free(&sysroot);
        return false;
    }
#define ARG(s) bind_list_add(args, (void *)bind_strdup(b, (s)))
#define SYS(s)                                                             \
    do {                                                                   \
        struct text p_ = {0};                                              \
        text_appendf(&p_, "%s%s", text_cstr(&sysroot), (s));               \
        ARG("-isystem");                                                   \
        ARG(text_cstr(&p_));                                               \
        text_free(&p_);                                                    \
    } while (0)
    if (strncmp(name, "linux-", 6) == 0) {
        const char *argv[] = {clang, "-print-resource-dir", NULL};
        struct text resource = {0};
        ARG(strcmp(name, "linux-arm64") == 0 ? "--target=aarch64-linux-musl"
                                               : "--target=x86_64-linux-musl");
        ARG("-nostdinc");
        SYS("/usr/include");
        if (process_capture(argv, &resource) != 0) {
            fprintf(stderr, "anti: %s -print-resource-dir failed\n", clang);
            ok = false;
        } else {
            while (resource.length > 0 &&
                   (resource.data[resource.length - 1] == '\n' ||
                    resource.data[resource.length - 1] == '\r')) {
                resource.data[--resource.length] = '\0';
            }
            text_append(&resource, "/include");
            ARG("-isystem");
            ARG(text_cstr(&resource));
        }
        text_free(&resource);
    } else if (strncmp(name, "macos-", 6) == 0) {
        ARG(strcmp(name, "macos-arm64") == 0 ? "--target=arm64-apple-macos11"
                                               : "--target=x86_64-apple-macos11");
        ARG("-isysroot");
        ARG(text_cstr(&sysroot));
    } else {
        ARG(strcmp(name, "windows-arm64") == 0
                ? "--target=aarch64-pc-windows-msvc"
                : "--target=x86_64-pc-windows-msvc");
        SYS("/crt/include");
        SYS("/sdk/include/ucrt");
        SYS("/sdk/include/um");
        SYS("/sdk/include/shared");
    }
#undef SYS
#undef ARG
    text_free(&sysroot);
    return ok;
}

/* The declarations of the AST. */

struct tag {
    const char *name;
    bool is_union;
    const char *id;             /* of the RecordDecl that defines it */
    struct bind_record *record;
};

struct typedef_entry {
    const char *name;
    const char *c_type;         /* the qualType of the typedef */
    const char *owned_id;       /* the id of a record or enum it names */
    int depth;                  /* the resolution in progress */
};

struct enum_entry {
    const char *id;
    const char *tag;            /* NULL for an anonymous enum */
    struct bind_enum *e;
};

struct macro {
    const char *name;
    const char *text;           /* NULL once the header undefines it */
    int state;                  /* 0 not read, 1 reading, 2 done, 3 failed */
    struct bind_eval value;
};

struct pack {
    long line;
    int value;                  /* 0: no pack */
};

struct fn_entry {
    struct bind_function *f;
    bool body;
    bool any_plain;             /* a declaration without inline */
    bool any_extern;
    bool is_static;
    bool is_inline;
};

struct reader {
    struct bind_module *b;
    const char *header;
    struct bind_list tags;      /* struct tag * */
    struct bind_list typedefs;  /* struct typedef_entry * */
    struct bind_list enums;     /* struct enum_entry * */
    struct bind_list macros;    /* struct macro * */
    struct bind_list packs;     /* struct pack * */
    struct bind_list fns;       /* struct fn_entry * */
    const char *file;           /* of the last location clang wrote */
    long line;
    bool failed;
};

static void refuse(struct reader *r, const char *format, const char *name)
{
    fprintf(stderr, "anti: %s: ", r->b->source);
    fprintf(stderr, format, name);
    fputc('\n', stderr);
    r->failed = true;
}

/* DESIGN: clang writes the file and the line of a location only where
   they differ from the location before it in the document. The reader walks every location in that order and keeps the
   last file and line. `includedFrom` names another file and is no
   location of its own. */
static void track(struct reader *r, const struct json_value *v)
{
    size_t i;

    if (v == NULL) {
        return;
    }
    if (v->kind == JSON_ARRAY) {
        for (i = 0; i < v->count; i++) {
            track(r, v->items[i]);
        }
        return;
    }
    if (v->kind != JSON_OBJECT) {
        return;
    }
    for (i = 0; i < v->count; i++) {
        const char *key = v->keys[i];
        const struct json_value *item = v->items[i];
        int64_t line;
        if (strcmp(key, "includedFrom") == 0) {
            continue;
        }
        if (strcmp(key, "file") == 0 && item->kind == JSON_STRING) {
            r->file = item->text;
        } else if (strcmp(key, "line") == 0 && json_integer(item, &line)) {
            r->line = (long)line;
        } else if (item->kind == JSON_OBJECT || item->kind == JSON_ARRAY) {
            track(r, item);
        }
    }
}

static bool in_header(const struct reader *r)
{
    return r->file != NULL && strcmp(r->file, r->header) == 0;
}

static struct tag *tag_named(struct reader *r, const char *name, bool is_union)
{
    size_t i;

    for (i = 0; i < r->tags.count; i++) {
        struct tag *t = r->tags.items[i];
        if (strcmp(t->name, name) == 0 && t->is_union == is_union) {
            return t;
        }
    }
    return NULL;
}

static struct typedef_entry *typedef_entry_named(struct reader *r,
                                                 const char *name)
{
    size_t i;

    for (i = 0; i < r->typedefs.count; i++) {
        struct typedef_entry *t = r->typedefs.items[i];
        if (strcmp(t->name, name) == 0) {
            return t;
        }
    }
    return NULL;
}

static struct bind_record *record_by_id(struct reader *r, const char *id)
{
    size_t i;

    for (i = 0; i < r->tags.count; i++) {
        struct tag *t = r->tags.items[i];
        if (t->id != NULL && strcmp(t->id, id) == 0) {
            return t->record;
        }
    }
    return NULL;
}

static struct enum_entry *enum_by_id(struct reader *r, const char *id)
{
    size_t i;

    for (i = 0; i < r->enums.count; i++) {
        struct enum_entry *e = r->enums.items[i];
        if (strcmp(e->id, id) == 0) {
            return e;
        }
    }
    return NULL;
}

static const struct bind_type *parse(struct reader *r, const char *c);

static const struct bind_type *typedef_named(void *context, const char *name)
{
    struct reader *r = context;
    struct typedef_entry *t = typedef_entry_named(r, name);
    const struct bind_type *type;

    if (t == NULL) {
        return NULL;
    }
    if (t->owned_id != NULL) {
        struct bind_record *rec = record_by_id(r, t->owned_id);
        struct enum_entry *e = enum_by_id(r, t->owned_id);
        if (rec != NULL) {
            struct bind_type *bt = bind_type_new(r->b, rec->complete
                                                           ? BIND_RECORD
                                                           : BIND_OPAQUE);
            bt->name = rec->name;
            bt->record = rec;
            return bt;
        }
        if (e != NULL && e->e != NULL) {
            struct bind_type *bt = bind_type_new(r->b, BIND_ENUM);
            bt->name = e->e->name;
            return bt;
        }
    }
    /* A typedef that names itself through others would recurse without
       end. */
    if (t->depth > 0) {
        return NULL;
    }
    t->depth++;
    type = parse(r, t->c_type);
    t->depth--;
    return type;
}

static struct bind_record *record_named(void *context, const char *name,
                                        bool is_union)
{
    struct tag *t = tag_named(context, name, is_union);

    return t != NULL ? t->record : NULL;
}

static const char *enum_named(void *context, const char *name)
{
    struct reader *r = context;
    size_t i;

    for (i = 0; i < r->enums.count; i++) {
        struct enum_entry *e = r->enums.items[i];
        if (e->e != NULL && e->tag != NULL && strcmp(e->tag, name) == 0) {
            return e->e->name;
        }
        if (e->e != NULL && strcmp(e->e->name, name) == 0) {
            return e->e->name;
        }
    }
    return NULL;
}

static const struct bind_type *parse(struct reader *r, const char *c)
{
    struct bind_names names;

    names.typedef_named = typedef_named;
    names.record_named = record_named;
    names.enum_named = enum_named;
    names.context = r;
    return bind_parse_type(r->b, c, &names);
}

static const char *qual_type(const struct json_value *node)
{
    return json_member_string(json_get(node, "type"), "qualType");
}

/* The spelling of a type with the name of an anonymous record put in
   place of clang's `struct (unnamed at file:line:col)`. */
static const char *named_anonymous(struct reader *r, const char *c,
                                   const char *name)
{
    const char *open = strstr(c, "(unnamed");
    const char *kw;
    const char *close;
    int depth = 0;
    struct text out = {0};
    const char *result;

    if (open == NULL) {
        open = strstr(c, "(anonymous");
    }
    if (open == NULL || name == NULL) {
        return c;
    }
    kw = open;
    while (kw > c && strncmp(kw, "struct ", 7) != 0 &&
           strncmp(kw, "union ", 6) != 0 && strncmp(kw, "enum ", 5) != 0) {
        kw--;
    }
    for (close = open; *close != '\0'; close++) {
        if (*close == '(') {
            depth++;
        } else if (*close == ')' && --depth == 0) {
            break;
        }
    }
    if (*close == '\0') {
        return c;
    }
    text_appendf(&out, "%.*s%s %s%s", (int)(kw - c), c,
                 kw[0] == 's' ? "struct" : kw[0] == 'u' ? "union" : "enum",
                 name, close + 1);
    result = bind_strdup(r->b, text_cstr(&out));
    text_free(&out);
    return result;
}

/* The value clang computed for the first constant expression below a
   node. clang writes it as text in the member `value` of a ConstantExpr,
   which may sit below an implicit conversion. */
static bool find_constant(const struct json_value *node, int64_t *out)
{
    const struct json_value *inner = json_get(node, "inner");
    const char *kind = json_member_string(node, "kind");
    const char *value = json_member_string(node, "value");
    size_t i;

    if (kind != NULL && strcmp(kind, "ConstantExpr") == 0 && value != NULL) {
        char *end;
        *out = strtoll(value, &end, 10);
        if (*end != '\0') {
            return false;
        }
        /* A value above INT64_MAX is an unsigned 64-bit one. */
        if (value[0] != '-' && *out == INT64_MAX) {
            *out = (int64_t)strtoull(value, NULL, 10);
        }
        return true;
    }
    for (i = 0; inner != NULL && i < inner->count; i++) {
        if (find_constant(inner->items[i], out)) {
            return true;
        }
    }
    return false;
}

static int64_t constant_of(const struct json_value *node)
{
    int64_t value;

    return find_constant(node, &value) ? value : -1;
}

static bool has_attribute(const struct json_value *node, const char *kind,
                          int64_t *value)
{
    const struct json_value *inner = json_get(node, "inner");
    size_t i;

    for (i = 0; inner != NULL && i < inner->count; i++) {
        const char *k = json_member_string(inner->items[i], "kind");
        if (k != NULL && strcmp(k, kind) == 0) {
            if (value != NULL) {
                *value = constant_of(inner->items[i]);
            }
            return true;
        }
    }
    return false;
}

/* The value of `#pragma pack` at a line of the header. */
static int pack_at(const struct reader *r, long line)
{
    int value = 0;
    size_t i;

    for (i = 0; i < r->packs.count; i++) {
        const struct pack *p = r->packs.items[i];
        if (p->line <= line) {
            value = p->value;
        }
    }
    return value;
}

static struct bind_record *declare_record(struct reader *r,
                                          const struct json_value *node,
                                          const char *name);

/* The fields of a record, and every record defined inside it. An
   anonymous record takes the name of the record and the field, and an
   anonymous member a field of that name. */
static void read_fields(struct reader *r, struct bind_record *rec,
                        const struct json_value *node)
{
    const struct json_value *inner = json_get(node, "inner");
    struct bind_record *anonymous = NULL;
    size_t count = 0;
    size_t anon = 0;
    size_t i;

    rec->fields = arena_alloc(&r->b->arena,
                              ((inner != NULL ? inner->count : 0) + 1) *
                                  sizeof *rec->fields);
    for (i = 0; inner != NULL && i < inner->count; i++) {
        const struct json_value *item = inner->items[i];
        const char *kind = json_member_string(item, "kind");
        const char *fname = json_member_string(item, "name");
        struct bind_field *f;
        const char *c;
        int64_t align;
        if (kind == NULL) {
            continue;
        }
        if (strcmp(kind, "RecordDecl") == 0) {
            if (fname != NULL) {
                declare_record(r, item, fname);
            } else {
                char synth[256];
                const struct json_value *next =
                    i + 1 < inner->count ? inner->items[i + 1] : NULL;
                const char *field = json_member_string(next, "name");
                if (field != NULL) {
                    snprintf(synth, sizeof synth, "%s_%s", rec->name, field);
                } else {
                    snprintf(synth, sizeof synth, "%s_anon%zu", rec->name, anon);
                }
                anonymous = declare_record(r, item, synth);
                /* C names an anonymous record through the field that holds
                   it, and an anonymous member not at all. */
                if (field != NULL && rec->c_name != NULL) {
                    struct text c_name = {0};
                    text_appendf(&c_name, "__typeof__(((%s *)0)->%s)",
                                 rec->c_name, field);
                    anonymous->c_name = bind_strdup(r->b, text_cstr(&c_name));
                    text_free(&c_name);
                } else {
                    anonymous->c_name = NULL;
                }
            }
            continue;
        }
        if (strcmp(kind, "FieldDecl") != 0) {
            continue;
        }
        c = qual_type(item);
        if (c == NULL) {
            continue;
        }
        f = &rec->fields[rec->field_count];
        f->bits = -1;
        if (strstr(c, "(unnamed") != NULL || strstr(c, "(anonymous") != NULL) {
            c = named_anonymous(r, c, anonymous != NULL ? anonymous->name : NULL);
        }
        f->c_type = bind_strdup(r->b, c);
        if (fname != NULL) {
            f->name = bind_strdup(r->b, fname);
        } else if (!json_member_true(item, "isBitfield")) {
            /* DESIGN: an anonymous member of C11 has no Anti form, so it
               becomes a field named anon<n>. Its layout is the layout of
               C, and its members are read through that name. */
            char synth[32];
            snprintf(synth, sizeof synth, "anon%zu", anon++);
            f->name = bind_strdup(r->b, synth);
            bind_warn(r->b, "an anonymous member of `%s` is the field `%s`",
                      rec->name, synth);
        }
        if (json_member_true(item, "isBitfield")) {
            f->bits = constant_of(item);
        }
        if (has_attribute(item, "AlignedAttr", &align)) {
            if (count != 0 || align <= 0) {
                refuse(r, "`%s` aligns a field other than its first, which "
                       "Anti has no form for", rec->name);
            } else if (align > rec->align) {
                /* An aligned first field raises the alignment of the
                   record and moves no field. */
                rec->align = align;
            }
        }
        f->type = parse(r, c);
        if (f->type == NULL) {
            struct bind_type *u = bind_type_new(r->b, BIND_UNSUPPORTED);
            u->name = f->c_type;
            f->type = u;
        }
        rec->field_count++;
        count++;
    }
}

static struct bind_record *declare_record(struct reader *r,
                                          const struct json_value *node,
                                          const char *name)
{
    const char *tag_used = json_member_string(node, "tagUsed");
    const char *id = json_member_string(node, "id");
    bool is_union = tag_used != NULL && strcmp(tag_used, "union") == 0;
    bool complete = json_member_true(node, "completeDefinition");
    struct tag *t = tag_named(r, name, is_union);
    struct bind_record *rec;
    int64_t align;

    if (t == NULL) {
        t = arena_alloc(&r->b->arena, sizeof *t);
        t->name = bind_strdup(r->b, name);
        t->is_union = is_union;
        rec = arena_alloc(&r->b->arena, sizeof *rec);
        rec->name = t->name;
        rec->is_union = is_union;
        t->record = rec;
        bind_list_add(&r->tags, t);
        bind_list_add(&r->b->records, rec);
    }
    rec = t->record;
    if (id != NULL && (complete || t->id == NULL)) {
        t->id = bind_strdup(r->b, id);
    }
    if (!complete || rec->complete) {
        return rec;
    }
    rec->complete = true;
    {
        struct text c_name = {0};
        if (json_get(node, "name") != NULL) {
            text_appendf(&c_name, "%s %s", is_union ? "union" : "struct", name);
        } else {
            text_append(&c_name, name);
        }
        rec->c_name = bind_strdup(r->b, text_cstr(&c_name));
        text_free(&c_name);
    }
    if (has_attribute(node, "PackedAttr", NULL)) {
        rec->packed = true;
    }
    if (has_attribute(node, "MaxFieldAlignmentAttr", NULL)) {
        int pack = pack_at(r, r->line);
        if (pack == 1) {
            rec->packed = true;
        } else {
            refuse(r, "`%s` is packed to more than one byte, which Anti has "
                   "no form for", name);
        }
    }
    if (has_attribute(node, "AlignedAttr", &align)) {
        if (align <= 0) {
            refuse(r, "`%s` is aligned without a value", name);
        } else {
            rec->align = align;
        }
    }
    read_fields(r, rec, node);
    return rec;
}

static void declare_enum(struct reader *r, const struct json_value *node)
{
    const struct json_value *inner = json_get(node, "inner");
    const char *id = json_member_string(node, "id");
    const char *tag = json_member_string(node, "name");
    struct enum_entry *entry = arena_alloc(&r->b->arena, sizeof *entry);
    struct bind_enum *e = arena_alloc(&r->b->arena, sizeof *e);
    int64_t low = 0;
    int64_t high = 0;
    int64_t next = 0;
    size_t i;

    entry->id = id != NULL ? bind_strdup(r->b, id) : "";
    entry->tag = tag != NULL ? bind_strdup(r->b, tag) : NULL;
    entry->e = e;
    e->name = entry->tag;
    e->values = arena_alloc(&r->b->arena,
                            ((inner != NULL ? inner->count : 0) + 1) *
                                sizeof *e->values);
    for (i = 0; inner != NULL && i < inner->count; i++) {
        const struct json_value *item = inner->items[i];
        const char *kind = json_member_string(item, "kind");
        const char *name = json_member_string(item, "name");
        if (kind == NULL || strcmp(kind, "EnumConstantDecl") != 0 ||
            name == NULL) {
            continue;
        }
        /* An enumerator without a value is the one before it plus one,
           and the first is 0. */
        if (!find_constant(item, &next)) {
            next = e->value_count > 0 ? e->values[e->value_count - 1].value + 1
                                      : 0;
        }
        e->values[e->value_count].name = bind_strdup(r->b, name);
        e->values[e->value_count].value = next;
        if (e->values[e->value_count].value < low) {
            low = e->values[e->value_count].value;
        }
        if (e->values[e->value_count].value > high) {
            high = e->values[e->value_count].value;
        }
        e->value_count++;
    }
    /* DESIGN: the base of an enum is c_int, the type of an enumerator in
       C, unless a value leaves its range. clang then gives the enum
       unsigned int when no value is negative. */
    if (high > 0x7FFFFFFF || low < -0x7FFFFFFF - 1) {
        e->base = low >= 0 && high <= 0xFFFFFFFFll ? "c_uint" : "c_longlong";
    }
    bind_list_add(&r->enums, entry);
}

static void declare_typedef(struct reader *r, const struct json_value *node)
{
    const char *name = json_member_string(node, "name");
    const char *c = qual_type(node);
    const struct json_value *inner = json_get(node, "inner");
    struct typedef_entry *t;

    if (name == NULL || c == NULL || typedef_entry_named(r, name) != NULL) {
        return;
    }
    t = arena_alloc(&r->b->arena, sizeof *t);
    t->name = bind_strdup(r->b, name);
    t->c_type = bind_strdup(r->b, c);
    if (inner != NULL && inner->count > 0) {
        const struct json_value *decl = json_get(inner->items[0], "decl");
        const char *id = json_member_string(decl, "id");
        const char *kind = json_member_string(inner->items[0], "kind");
        if (id != NULL && kind != NULL &&
            (strcmp(kind, "RecordType") == 0 || strcmp(kind, "EnumType") == 0)) {
            t->owned_id = bind_strdup(r->b, id);
        }
    }
    bind_list_add(&r->typedefs, t);
}

/* A typedef of the header names the anonymous record or enum it owns. */
static void name_owned(struct reader *r, const struct json_value *node)
{
    const char *name = json_member_string(node, "name");
    const struct json_value *inner = json_get(node, "inner");
    const struct json_value *decl;
    const char *id;
    struct enum_entry *e;

    if (name == NULL || inner == NULL || inner->count == 0) {
        return;
    }
    decl = json_get(inner->items[0], "decl");
    id = json_member_string(decl, "id");
    if (id == NULL || json_member_string(decl, "name") == NULL) {
        return;
    }
    if (json_member_string(decl, "name")[0] != '\0') {
        /* A typedef of a tagged enum takes the typedef name when the two
           differ, since C code names the typedef. */
        return;
    }
    e = enum_by_id(r, id);
    if (e != NULL && e->e->name == NULL) {
        e->e->name = bind_strdup(r->b, name);
        e->e->c_name = e->e->name;
    }
}

static struct fn_entry *fn_named(struct reader *r, const char *name)
{
    size_t i;

    for (i = 0; i < r->fns.count; i++) {
        struct fn_entry *e = r->fns.items[i];
        if (strcmp(e->f->name, name) == 0) {
            return e;
        }
    }
    return NULL;
}

static void declare_function(struct reader *r, const struct json_value *node)
{
    const char *name = json_member_string(node, "name");
    const char *c = qual_type(node);
    const char *storage = json_member_string(node, "storageClass");
    const struct json_value *inner = json_get(node, "inner");
    bool is_inline = json_member_true(node, "inline");
    struct fn_entry *e;
    const struct bind_type *type;
    size_t i;
    bool body = false;

    if (name == NULL || c == NULL) {
        return;
    }
    for (i = 0; inner != NULL && i < inner->count; i++) {
        const char *kind = json_member_string(inner->items[i], "kind");
        if (kind != NULL && strcmp(kind, "CompoundStmt") == 0) {
            body = true;
        }
    }
    e = fn_named(r, name);
    if (e == NULL) {
        struct bind_function *f = arena_alloc(&r->b->arena, sizeof *f);
        type = parse(r, c);
        f->name = bind_strdup(r->b, name);
        if (type == NULL || type->kind != BIND_FUNCTION) {
            bind_warn(r->b, "`%s` is left out: its type `%s` does not parse",
                      name, c);
            return;
        }
        f->result = type->to;
        f->variadic = type->variadic;
        f->params = arena_alloc(&r->b->arena,
                                (type->param_count + 1) * sizeof *f->params);
        f->param_count = type->param_count;
        {
            size_t k = 0;
            const char *open = strchr(c, '(');
            f->c_result = bind_strndup(r->b, c,
                                       open != NULL ? (size_t)(open - c) : 0);
            while (f->c_result[0] != '\0' &&
                   f->c_result[strlen(f->c_result) - 1] == ' ') {
                f->c_result = bind_strndup(r->b, f->c_result,
                                           strlen(f->c_result) - 1);
            }
            for (i = 0; inner != NULL && i < inner->count; i++) {
                const char *kind = json_member_string(inner->items[i], "kind");
                if (kind == NULL || strcmp(kind, "ParmVarDecl") != 0 ||
                    k >= f->param_count) {
                    continue;
                }
                f->params[k].name =
                    json_member_string(inner->items[i], "name") != NULL
                        ? bind_strdup(r->b, json_member_string(inner->items[i],
                                                               "name"))
                        : NULL;
                f->params[k].c_type =
                    bind_strdup(r->b, qual_type(inner->items[i]) != NULL
                                          ? qual_type(inner->items[i])
                                          : "int");
                k++;
            }
            for (k = 0; k < f->param_count; k++) {
                f->params[k].type = type->params[k];
            }
        }
        e = arena_alloc(&r->b->arena, sizeof *e);
        e->f = f;
        bind_list_add(&r->fns, e);
        bind_list_add(&r->b->functions, f);
    }
    e->body = e->body || body;
    e->is_static = e->is_static || (storage != NULL && strcmp(storage, "static") == 0);
    e->any_extern = e->any_extern || (storage != NULL && strcmp(storage, "extern") == 0);
    e->any_plain = e->any_plain || !is_inline;
    e->is_inline = e->is_inline || is_inline;
}

/* DESIGN: a function of the header reaches a symbol in one of three
   ways. A `static` definition has none, and the shim wraps it. A C99
   `inline` definition has none unless some declaration of it is `extern`
   or not inline (C11 6.7.4p7), and the shim declares it `extern`. Every
   other function is defined by the library. A `static` function without
   a body has no symbol anywhere and is left out. */
static void settle_functions(struct reader *r)
{
    size_t i;

    for (i = 0; i < r->fns.count; i++) {
        struct fn_entry *e = r->fns.items[i];
        if (e->is_static && e->body) {
            e->f->shim = SHIM_STATIC;
        } else if (e->is_static) {
            bind_warn(r->b, "`%s` is static without a body and is left out",
                      e->f->name);
            e->f->result = bind_type_new(r->b, BIND_UNSUPPORTED);
        } else if (e->body && e->is_inline && !e->any_plain && !e->any_extern) {
            e->f->shim = SHIM_INLINE;
        }
    }
}

/* The macros. */

static struct macro *macro_named(struct reader *r, const char *name)
{
    size_t i;

    for (i = 0; i < r->macros.count; i++) {
        struct macro *m = r->macros.items[i];
        if (strcmp(m->name, name) == 0) {
            return m;
        }
    }
    return NULL;
}

static bool lookup(void *context, const char *name, struct bind_eval *out);

static bool evaluate(struct reader *r, struct macro *m)
{
    if (m->state == 1) {
        m->state = 3;
    }
    if (m->state == 0) {
        m->state = 1;
        m->state = bind_eval(r->b, m->text, lookup, r, &m->value) ? 2 : 3;
    }
    return m->state == 2;
}

static bool lookup(void *context, const char *name, struct bind_eval *out)
{
    struct reader *r = context;
    struct macro *m = macro_named(r, name);
    size_t i;
    size_t j;

    if (m != NULL && m->text != NULL) {
        if (!evaluate(r, m)) {
            return false;
        }
        *out = m->value;
        return true;
    }
    for (i = 0; i < r->enums.count; i++) {
        const struct enum_entry *e = r->enums.items[i];
        for (j = 0; j < e->e->value_count; j++) {
            if (strcmp(e->e->values[j].name, name) == 0) {
                memset(out, 0, sizeof *out);
                out->kind = BIND_EVAL_INT;
                out->i = e->e->values[j].value;
                out->type = e->e->base != NULL ? e->e->base : "c_int";
                if (e->e->name != NULL) {
                    out->enum_type = e->e->name;
                    out->enum_value = e->e->values[j].name;
                }
                return true;
            }
        }
    }
    return false;
}

/* The line markers, the macros and the pragmas of the output of
   `clang -E -dD`. A line marker `# 12 "file" 2` says that the next line
   is line 12 of file, and every line after it counts one. */
static void read_preprocessed(struct reader *r, char *text)
{
    const char *file = NULL;
    long line = 0;
    int stack[64];
    int depth = 0;
    int pack = 0;
    char *at = text;

    while (*at != '\0') {
        char *end = strchr(at, '\n');
        if (end != NULL) {
            *end = '\0';
        }
        if (at[0] == '#' && at[1] == ' ' && at[2] >= '0' && at[2] <= '9') {
            char *quote = strchr(at, '"');
            char *close = quote != NULL ? strrchr(quote + 1, '"') : NULL;
            line = strtol(at + 2, NULL, 10);
            if (close != NULL) {
                *close = '\0';
                file = bind_strdup(r->b, quote + 1);
            }
        } else {
            bool here = file != NULL && strcmp(file, r->header) == 0;
            if (here && strncmp(at, "#define ", 8) == 0) {
                const char *name = at + 8;
                size_t n = 0;
                while ((name[n] >= 'a' && name[n] <= 'z') ||
                       (name[n] >= 'A' && name[n] <= 'Z') ||
                       (name[n] >= '0' && name[n] <= '9') || name[n] == '_') {
                    n++;
                }
                if (name[n] == '(') {
                    bind_warn(r->b, "the macro `%.*s` takes arguments and is "
                              "skipped", (int)n, name);
                } else if (n > 0) {
                    const char *macro_name = bind_strndup(r->b, name, n);
                    struct macro *m = macro_named(r, macro_name);
                    if (m == NULL) {
                        m = arena_alloc(&r->b->arena, sizeof *m);
                        m->name = macro_name;
                        bind_list_add(&r->macros, m);
                    }
                    /* A macro defined again takes its last value. */
                    m->text = bind_strdup(r->b, name[n] == ' ' ? name + n + 1
                                                               : name + n);
                }
            } else if (here && strncmp(at, "#undef ", 7) == 0) {
                struct macro *m = macro_named(r, at + 7);
                if (m != NULL) {
                    m->text = NULL;
                }
            } else if (strncmp(at, "#pragma pack", 12) == 0) {
                /* push, pop, a value and a push with a value, as MSVC
                   and clang read them. */
                const char *args = strchr(at, '(');
                int value = pack;
                if (args != NULL && strstr(args, "pop") != NULL) {
                    value = depth > 0 ? stack[--depth] : 0;
                } else if (args != NULL) {
                    const char *digit = args;
                    bool push = strstr(args, "push") != NULL;
                    while (*digit != '\0' && (*digit < '0' || *digit > '9')) {
                        digit++;
                    }
                    if (push && depth < (int)(sizeof stack / sizeof stack[0])) {
                        stack[depth++] = pack;
                    }
                    value = *digit != '\0' ? atoi(digit) : (push ? pack : 0);
                }
                pack = value;
                if (here) {
                    struct pack *p = arena_alloc(&r->b->arena, sizeof *p);
                    p->line = line;
                    p->value = pack;
                    bind_list_add(&r->packs, p);
                }
            }
            line++;
        }
        if (end == NULL) {
            break;
        }
        at = end + 1;
    }
}

static void read_macros(struct reader *r)
{
    size_t i;

    for (i = 0; i < r->macros.count; i++) {
        struct macro *m = r->macros.items[i];
        if (m->text == NULL) {
            continue;
        }
        if (m->text[0] == '\0') {
            bind_warn(r->b, "the macro `%s` has no value and is skipped",
                      m->name);
            continue;
        }
        if (bind_is_keyword(m->name)) {
            bind_warn(r->b, "the macro `%s` is a word of Anti and is skipped",
                      m->name);
            continue;
        }
        if (!evaluate(r, m) || !bind_eval_const(r->b, m->name, &m->value, NULL)) {
            bind_warn(r->b, "the macro `%s` is no constant and is skipped",
                      m->name);
        }
    }
}

static bool run(const struct bind_list *args, struct text *out,
                const char *what)
{
    int status = process_capture((const char *const *)args->items, out);

    if (status != 0) {
        fprintf(stderr, "anti: clang could not %s, status %d\n", what, status);
        return false;
    }
    return true;
}

/* The anonymous enums of the header hold constants, which become
   `const` values of c_int. */
static void settle_enums(struct reader *r)
{
    size_t i;
    size_t j;

    for (i = 0; i < r->enums.count; i++) {
        struct enum_entry *e = r->enums.items[i];
        if (e->e->name != NULL) {
            bind_list_add(&r->b->enums, e->e);
            continue;
        }
        for (j = 0; j < e->e->value_count; j++) {
            struct bind_eval v;
            memset(&v, 0, sizeof v);
            v.kind = BIND_EVAL_INT;
            v.i = e->e->values[j].value;
            v.type = e->e->base != NULL ? e->e->base : "c_int";
            bind_eval_const(r->b, e->e->values[j].name, &v, NULL);
        }
    }
}

bool bind_read_clang(struct bind_module *b, const struct bind_clang_request *q)
{
    struct reader r;
    struct text clang = {0};
    struct bind_list common = {0};
    struct bind_list ast = {0};
    struct bind_list pre = {0};
    struct text json = {0};
    struct text text = {0};
    struct json_tree tree;
    const struct json_value *inner;
    enum target t;
    char error[160];
    size_t i;
    bool ok = false;

    memset(&r, 0, sizeof r);
    memset(&tree, 0, sizeof tree);
    r.b = b;
    r.header = q->header;
    if (q->target != NULL ? !target_from_name(q->target, &t) : !target_host(&t)) {
        fprintf(stderr, "anti: unknown target %s\n",
                q->target != NULL ? q->target : "of this host");
        return false;
    }
    find_clang(&clang);
    if (!check_version(text_cstr(&clang))) {
        goto done;
    }
    bind_list_add(&common, (void *)text_cstr(&clang));
    if (!target_options(text_cstr(&clang), t, q->runtime, &common, b)) {
        goto done;
    }
    bind_list_add(&common, "-x");
    bind_list_add(&common, "c");
    for (i = 0; i < q->include_count; i++) {
        bind_list_add(&common, "-I");
        bind_list_add(&common, (void *)q->includes[i]);
    }
    for (i = 0; i < q->define_count; i++) {
        struct text d = {0};
        text_appendf(&d, "-D%s", q->defines[i]);
        bind_list_add(&common, (void *)bind_strdup(b, text_cstr(&d)));
        text_free(&d);
    }
    for (i = 0; i < common.count; i++) {
        bind_list_add(&ast, common.items[i]);
        bind_list_add(&pre, common.items[i]);
    }
    bind_list_add(&ast, "-fsyntax-only");
    bind_list_add(&ast, "-Xclang");
    bind_list_add(&ast, "-ast-dump=json");
    bind_list_add(&ast, (void *)q->header);
    bind_list_add(&ast, NULL);
    bind_list_add(&pre, "-E");
    bind_list_add(&pre, "-dD");
    bind_list_add(&pre, (void *)q->header);
    bind_list_add(&pre, NULL);
    if (!run(&pre, &text, "preprocess the header") ||
        !run(&ast, &json, "read the header as C")) {
        goto done;
    }
    read_preprocessed(&r, text.data != NULL ? text.data : (char *)"");
    if (!json_read((const unsigned char *)json.data, json.length, &tree, error,
                   sizeof error)) {
        fprintf(stderr, "anti: the AST of clang is not JSON: %s\n", error);
        goto done;
    }
    inner = json_get(tree.root, "inner");
    /* Every typedef of the translation unit resolves a name, and every
       declaration of the header is bound. */
    for (i = 0; inner != NULL && i < inner->count; i++) {
        const struct json_value *node = inner->items[i];
        const char *kind = json_member_string(node, "kind");
        const char *name = json_member_string(node, "name");
        track(&r, json_get(node, "loc"));
        if (kind != NULL && in_header(&r)) {
            if (strcmp(kind, "RecordDecl") == 0 && name != NULL) {
                declare_record(&r, node, name);
            } else if (strcmp(kind, "RecordDecl") == 0) {
                /* An anonymous record takes its name from the typedef
                   after it. */
                const struct json_value *next =
                    i + 1 < inner->count ? inner->items[i + 1] : NULL;
                const char *next_kind = json_member_string(next, "kind");
                const char *tname = json_member_string(next, "name");
                if (next_kind != NULL && strcmp(next_kind, "TypedefDecl") == 0 &&
                    tname != NULL) {
                    declare_record(&r, node, tname);
                }
            } else if (strcmp(kind, "EnumDecl") == 0) {
                declare_enum(&r, node);
            } else if (strcmp(kind, "TypedefDecl") == 0) {
                name_owned(&r, node);
            } else if (strcmp(kind, "VarDecl") == 0) {
                bind_warn(b, "the variable `%s` is skipped", name != NULL ? name : "");
            }
        }
        if (kind != NULL && strcmp(kind, "TypedefDecl") == 0) {
            declare_typedef(&r, node);
        }
        track(&r, json_get(node, "range"));
        track(&r, json_get(node, "inner"));
    }
    /* The functions come last, when every type they name is known. */
    r.file = NULL;
    r.line = 0;
    for (i = 0; inner != NULL && i < inner->count; i++) {
        const struct json_value *node = inner->items[i];
        const char *kind = json_member_string(node, "kind");
        track(&r, json_get(node, "loc"));
        if (kind != NULL && in_header(&r) && strcmp(kind, "FunctionDecl") == 0) {
            declare_function(&r, node);
        }
        track(&r, json_get(node, "range"));
        track(&r, json_get(node, "inner"));
    }
    settle_functions(&r);
    settle_enums(&r);
    read_macros(&r);
    ok = !r.failed;

done:
    json_free(&tree);
    text_free(&clang);
    text_free(&json);
    text_free(&text);
    free(common.items);
    free(ast.items);
    free(pre.items);
    free(r.tags.items);
    free(r.typedefs.items);
    free(r.enums.items);
    free(r.macros.items);
    free(r.packs.items);
    free(r.fns.items);
    return ok;
}

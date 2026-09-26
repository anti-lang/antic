/* `anti doc`: the documentation generator of docs/tooling.md.

   DESIGN: user docs are built from the public interface of a module and
   nothing else. A library file alone is then enough, and a reader needs
   no source. The same structure comes out of the source, which is what
   the doc-equivalence test of docs/tooling.md compares. Dev docs need
   the syntax tree, because the private items and the `//#` notes live
   nowhere else. `--private` reads the tree for the same reason.

   The output is plain semantic HTML with the eight class names below and
   no styling. `--markdown` writes Markdown instead and passes the doc
   text through unchanged, for a Hugo site to render. */
#include "doc.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "antl.h"
#include "arena.h"
#include "ast.h"
#include "cpu.h"
#include "driver.h"
#include "files.h"
#include "ir.h"
#include "modpath.h"
#include "repo.h"
#include "sema.h"
#include "target.h"
#include "text.h"
#include "types.h"
#include "units.h"

/* DESIGN: the fixed set of class names of the output, which the entry
   under "Names and publication" in docs/decisions.md asks for. The
   stylesheet of the site addresses these eight and nothing else. A page
   carries no styling of its own. */
#define DOC_CLASS_PAGE "doc"
#define DOC_CLASS_INDEX "index"
#define DOC_CLASS_ITEM "item"
#define DOC_CLASS_SIGNATURE "signature"
#define DOC_CLASS_FIELDS "fields"
#define DOC_CLASS_MEMBERS "members"
#define DOC_CLASS_INTERNALS "internals"
#define DOC_CLASS_CODE "code"

/* The sentence at each function of a synchronized class that runs under
   the lock of its object. */
#define DOC_LOCKED "Runs under the lock of its object."

/* The suffix of a page and of the index, one per form. */
#define DOC_HTML_SUFFIX ".html"
#define DOC_MARKDOWN_SUFFIX ".md"

/* One documented item, or one field or function of a body. */
struct entry {
    struct text signature;      /* the declaration, without its body */
    struct text name;           /* the anchor and the heading */
    struct doc_text doc;        /* the `///` text */
    struct doc_text note;       /* the `//#` text, dev docs alone */
    struct entry *fields;       /* the fields or the values of a body */
    size_t field_count;
    size_t field_capacity;
    struct entry *members;      /* the functions of a class body */
    size_t member_count;
    size_t member_capacity;
    /* A function of a synchronized class that runs under the lock of its
       object, which the page says at the function. */
    bool locked;
    /* A name that `type` gives a copy of a generic: the module and the
       name of that generic, which the page links to. Empty otherwise. */
    struct text copy_of_module;
    struct text copy_of;
};

/* One page: a module with its items. */
struct page {
    struct text module;
    struct doc_text doc;        /* the `//!` text */
    struct doc_text note;       /* the `//#!` text */
    const struct interface *iface;  /* the tables a backtick name reads */
    struct entry *items;
    size_t item_count;
    size_t item_capacity;
};

static struct entry *entry_add(struct entry **list, size_t *count,
                               size_t *capacity)
{
    struct entry *one;

    if (*count == *capacity) {
        *list = files_grow(*list, capacity, sizeof **list);
    }
    one = &(*list)[(*count)++];
    memset(one, 0, sizeof *one);
    return one;
}

static void entry_free(struct entry *e)
{
    size_t i;

    for (i = 0; i < e->field_count; i++) {
        entry_free(&e->fields[i]);
    }
    for (i = 0; i < e->member_count; i++) {
        entry_free(&e->members[i]);
    }
    free(e->fields);
    free(e->members);
    text_free(&e->signature);
    text_free(&e->name);
    text_free(&e->copy_of_module);
    text_free(&e->copy_of);
}

static void page_free(struct page *p)
{
    size_t i;

    for (i = 0; i < p->item_count; i++) {
        entry_free(&p->items[i]);
    }
    free(p->items);
    text_free(&p->module);
}

/* Signatures */

static bool name_is(const struct name *n, const char *s)
{
    return n->length == strlen(s) && memcmp(n->text, s, n->length) == 0;
}

/* The word a declaration wrote for the visibility of an item. A private
   item carries none, and only dev docs and `--private` show one. */
static const char *visibility_word(enum visibility vis)
{
    switch (vis) {
    case VIS_PUB: return "pub ";
    case VIS_INTERNAL: return "internal ";
    case VIS_PROTECTED: return "protected ";
    default: return "";
    }
}

/* The result a declaration wrote. A `may fail` function returns `?*Error`
   and writes its result through the out pointer, so the result of the
   declaration is what that pointer points at. */
static const struct type *declared_result(const struct type *t)
{
    if (t->may_fail) {
        return t->has_out && t->param_count > 0
                   ? t->params[t->param_count - 1]->element
                   : NULL;
    }
    return t->result != NULL && t->result->kind != TYPE_VOID ? t->result
                                                             : NULL;
}

/* The number of parameters a declaration wrote: the parameters of the
   type without `self` and without the out pointer of `may fail`. */
static size_t declared_params(const struct type *t, bool self)
{
    size_t n = t->param_count;

    if (t->may_fail && t->has_out && n > 0) {
        n--;
    }
    return self && n > 0 ? n - 1 : n;
}

/* DESIGN: a signature carries the visibility, the form word, the name,
   the name, the mark and the type of every parameter, the result and
   `may fail`. It
   carries no default value, no `own` and no body. The library file keeps
   the first set for every function and the rest for some. A page built
   from the file reads as the page built from the source. */
/* Whether keyword declares a function of C, whose parameters of function
   type are C function pointers without a mark. */
static bool c_function(const char *keyword)
{
    return strstr(keyword, "extern") != NULL;
}

/* DESIGN: a generic shows its type parameters after its name, each with
   its constraints as the declaration wrote them, `<T: lt + eq, N: int>`.
   The library file carries them in the public interface. A page built
   from it then writes what the page built from the source writes. */
static void constraint_list(struct text *out, const struct constraint_ref *refs,
                            size_t count)
{
    size_t i;

    for (i = 0; i < count; i++) {
        text_append(out, i > 0 ? " + " : "");
        if (refs[i].module.length > 0) {
            text_appendf(out, "%.*s.", (int)refs[i].module.length,
                         refs[i].module.text);
        }
        text_appendf(out, "%.*s", (int)refs[i].name.length, refs[i].name.text);
    }
}

static void type_param(struct text *out, const struct type_param *p)
{
    text_appendf(out, "%.*s", (int)p->name.length, p->name.text);
    if (p->constant) {
        text_append(out, ": int");
    } else if (p->constraint_count > 0) {
        text_append(out, ": ");
        constraint_list(out, p->constraints, p->constraint_count);
    }
}

/* The parameters of a generic function. */
static void fn_params(struct text *out, const struct item *it)
{
    size_t i;

    if (it == NULL || it->type_param_count == 0) {
        return;
    }
    text_append(out, "<");
    for (i = 0; i < it->type_param_count; i++) {
        text_append(out, i > 0 ? ", " : "");
        type_param(out, &it->type_params[i]);
    }
    text_append(out, ">");
}

/* The parameters of a generic struct, class or variant. */
static void type_params(struct text *out, const struct type *t)
{
    size_t i;

    if (t->type_param_count == 0) {
        return;
    }
    text_append(out, "<");
    for (i = 0; i < t->type_param_count; i++) {
        text_append(out, i > 0 ? ", " : "");
        if (t->type_params[i]->param != NULL) {
            type_param(out, t->type_params[i]->param);
        }
    }
    text_append(out, ">");
}

static void fn_signature(struct text *out, const char *lead,
                         const char *keyword, const struct name *name,
                         const struct item *generic, const struct type *t,
                         const struct name *params, size_t param_count,
                         bool self, bool variadic)
{
    const struct type *result = declared_result(t);
    size_t first = self ? 1 : 0;
    size_t i;

    text_appendf(out, "%s%s %.*s", lead, keyword, (int)name->length,
                 name->text);
    fn_params(out, generic);
    text_append(out, "(");
    if (self) {
        text_append(out, "self");
    }
    for (i = 0; i < param_count && first + i < t->param_count; i++) {
        const struct type *p = t->params[first + i];
        text_append(out, i > 0 || self ? ", " : "");
        /* A parameter of function type carries its mark in its type. An
           `extern fn` takes C function pointers alone and writes none. */
        if (p->kind == TYPE_FN && !p->bound && !c_function(keyword)) {
            text_append(out, p->owned        ? "keep own "
                             : !p->context   ? "keep "
                             : p->concurrent ? "concurrent "
                                             : "");
        }
        /* `lent` stands before the name, as the declaration writes it. */
        if (type_is_lent(p) && params != NULL &&
            params[i].length > 0) {
            struct type bare = *p;
            bare.lent = false;
            text_appendf(out, "lent %.*s: ", (int)params[i].length,
                         params[i].text);
            type_name(out, &bare);
            continue;
        }
        if (params != NULL && params[i].length > 0) {
            text_appendf(out, "%.*s: ", (int)params[i].length,
                         params[i].text);
        }
        type_name(out, p);
    }
    if (variadic) {
        text_append(out, param_count > 0 || self ? ", ..." : "...");
    }
    text_append(out, ")");
    if (result != NULL) {
        text_append(out, " -> ");
        type_name(out, result);
    }
    if (t->may_fail) {
        text_append(out, " may fail");
    }
}

/* The value of a constant, for the kinds a page can print in one line.
   An array, a struct, a character and a value computed from size_of
   carry none. */
static void const_value(struct text *out, const struct const_value *v)
{
    if (v == NULL) {
        return;
    }
    switch (v->kind) {
    case CONST_INT:
        if (v->type != NULL && !type_is_signed(v->type)) {
            text_appendf(out, " = %" PRIu64, v->as.integer);
        } else {
            text_appendf(out, " = %" PRId64, (int64_t)v->as.integer);
        }
        break;
    case CONST_BOOL:
        text_appendf(out, " = %s", v->as.boolean ? "true" : "false");
        break;
    case CONST_FLOAT:
        text_appendf(out, " = %g", v->as.floating);
        break;
    case CONST_NULL:
        text_append(out, " = none");
        break;
    default:
        break;
    }
}

/* The declaration of a struct, a union, a class, an enum or a variant. */
static void type_signature(struct text *out, const char *lead,
                           const struct name *name, const struct type *t)
{
    switch (t->kind) {
    case TYPE_ENUM:
        text_appendf(out, "%senum %.*s", lead, (int)name->length, name->text);
        if (t->base != NULL) {
            text_append(out, ": ");
            type_name(out, t->base);
        }
        break;
    case TYPE_VARIANT:
        text_appendf(out, "%svariant %.*s", lead, (int)name->length,
                     name->text);
        type_params(out, t);
        break;
    case TYPE_CLASS:
        text_appendf(out, "%s%s%s%s%sclass %.*s", lead,
                     t->has_abstract ? "abstract " : "",
                     t->is_final ? "final " : "", t->traced ? "trace " : "",
                     t->safety == SAFETY_SYNCHRONIZED ? "synchronized "
                     : t->safety == SAFETY_CONCURRENT ? "concurrent "
                                                      : "",
                     (int)name->length, name->text);
        type_params(out, t);
        /* DESIGN: `anti.lang.Object` is the root of every class chain,
           so naming it after `inherits` says nothing a reader does not
           know. The base of a class that names one is written. */
        if (t->base != NULL && !(name_is(&t->base->module, LANG_MODULE) &&
                                 name_is(&t->base->name, LANG_OBJECT))) {
            text_append(out, " inherits ");
            type_name_qualified(out, t->base);
        }
        break;
    default:
        if (t->packed) {
            text_appendf(out, "%spacked ", lead);
        } else if (t->align > 0) {
            text_appendf(out, "%salign(%" PRIu64 ") ", lead, t->align);
        } else {
            text_append(out, lead);
        }
        text_appendf(out, "%s%s %.*s", t->simd ? "simd " : "",
                     t->is_union ? "union" : "struct", (int)name->length,
                     name->text);
        type_params(out, t);
        break;
    }
}

/* The declaration of a field, with the word of its form. The base of a
   class and its table pointer are no fields a program wrote. */
static bool field_signature(struct text *out, const struct struct_field *f,
                            bool with_visibility)
{
    if (f->form == FIELD_BASE || f->form == FIELD_TABLE) {
        return false;
    }
    if (with_visibility) {
        text_append(out, visibility_word(f->vis));
    }
    if (f->form == FIELD_USE) {
        text_append(out, "use ");
    } else if (f->form == FIELD_IMPL) {
        text_append(out, "implements ");
    }
    if (f->owned) {
        text_append(out, "own ");
    }
    text_appendf(out, "%.*s: ", (int)f->name.length, f->name.text);
    type_name(out, f->type);
    if (f->bits > 0) {
        text_appendf(out, " : %u", (unsigned)f->bits);
    }
    return true;
}

/* The cases of a variant, each with the fields it holds. The tag enum
   names them in the order of the declaration. */
static void variant_cases(struct entry *item, const struct type *t)
{
    size_t i;
    size_t j;

    if (t->base == NULL) {
        return;
    }
    for (i = 0; i < t->base->field_count && i < t->param_count; i++) {
        struct entry *one = entry_add(&item->fields, &item->field_count,
                                      &item->field_capacity);
        const struct type *body = t->params[i];
        text_append_bytes(&one->name, t->base->fields[i].name.text,
                          t->base->fields[i].name.length);
        text_appendf(&one->signature, "%.*s",
                     (int)t->base->fields[i].name.length,
                     t->base->fields[i].name.text);
        if (body == NULL || body->field_count == 0) {
            continue;
        }
        text_append(&one->signature, " { ");
        for (j = 0; j < body->field_count; j++) {
            text_appendf(&one->signature, "%s%.*s: ", j > 0 ? ", " : "",
                         (int)body->fields[j].name.length,
                         body->fields[j].name.text);
            type_name(&one->signature, body->fields[j].type);
        }
        text_append(&one->signature, " }");
    }
}

/* The values of an enum, each with the number it stands for. */
static void enum_values(struct entry *item, const struct type *t)
{
    size_t i;

    for (i = 0; i < t->field_count; i++) {
        struct entry *one = entry_add(&item->fields, &item->field_count,
                                      &item->field_capacity);
        text_append_bytes(&one->name, t->fields[i].name.text,
                          t->fields[i].name.length);
        text_appendf(&one->signature, "%.*s = %" PRIu64,
                     (int)t->fields[i].name.length, t->fields[i].name.text,
                     t->fields[i].number);
        one->doc = t->fields[i].doc;
    }
}

/* The fields and the functions of a body. all takes every one of them,
   which dev docs and `--private` ask for, and user docs take the `pub`
   ones alone. */
static void body_entries(struct entry *item, const struct type *t, bool all)
{
    struct name *names;
    size_t i;
    size_t j;

    if (t->kind == TYPE_ENUM) {
        enum_values(item, t);
        return;
    }
    if (t->kind == TYPE_VARIANT) {
        variant_cases(item, t);
        return;
    }
    /* DESIGN: a struct body carries no visibility marker, and every
       field of one is readable and writable everywhere, which the object
       model says. A class body marks its fields, so user docs take the
       `pub` ones of a class and every field of a struct. */
    for (i = 0; i < t->field_count; i++) {
        const struct struct_field *f = &t->fields[i];
        bool marked = t->kind == TYPE_CLASS;
        struct entry *one;
        if (!all && marked && f->vis != VIS_PUB) {
            continue;
        }
        one = entry_add(&item->fields, &item->field_count,
                        &item->field_capacity);
        if (!field_signature(&one->signature, f, all && marked)) {
            item->field_count--;
            text_free(&one->signature);
            continue;
        }
        text_append_bytes(&one->name, f->name.text, f->name.length);
        one->doc = f->doc;
    }
    for (i = 0; i < t->member_count; i++) {
        const struct item *m = t->members[i];
        struct entry *one;
        bool self;
        size_t params;
        /* A function with type parameters of its own compiles no copy
           yet, so the library file leaves it out, and so does the
           page. */
        if (m->kind != ITEM_FN || m->symbol == NULL ||
            m->symbol->type == NULL || m->type_param_count > 0) {
            continue;
        }
        if (!all && m->vis != VIS_PUB) {
            continue;
        }
        /* `self` stands where the type carries a parameter the
           declaration did not write. The count of the declaration comes
           from the library file and from the source alike, so the
           question is answered and never guessed. */
        params = m->param_count;
        self = declared_params(m->symbol->type, false) > params;
        one = entry_add(&item->members, &item->member_count,
                        &item->member_capacity);
        text_append_bytes(&one->name, m->name.text, m->name.length);
        one->locked = t->safety == SAFETY_SYNCHRONIZED && self &&
                      m->vis != VIS_PRIVATE && !name_is(&m->name, "construct") &&
                      !name_is(&m->name, "destruct");
        text_append(&one->signature, visibility_word(m->vis));
        if (m->contract == FN_ABSTRACT) {
            text_append(&one->signature, "abstract ");
        } else if (m->contract == FN_CONCRETE) {
            text_append(&one->signature, "concrete ");
        }
        /* The library file keeps the names on the symbol and the source
           on the item, and the two lists carry the same names. */
        names = NULL;
        if (params > 0) {
            names = files_array(params, sizeof *names);
            for (j = 0; j < params; j++) {
                names[j] = m->symbol->params != NULL ? m->symbol->params[j]
                                                     : m->params[j].name;
            }
        }
        fn_signature(&one->signature, "", "fn", &m->name, NULL,
                     m->symbol->type, names, params, self, false);
        free(names);
        one->doc = m->doc;
    }
}

/* `constraint Name = a + b`, whose set its type holds with the
   constraints the declaration wrote. */
static void constraint_signature(struct text *out, const char *lead,
                                 const struct name *name,
                                 const struct type *set)
{
    const struct item *it = set != NULL ? set->declared_by : NULL;

    text_appendf(out, "%sconstraint %.*s = ", lead, (int)name->length,
                 name->text);
    if (it != NULL) {
        constraint_list(out, it->constraints, it->constraint_count);
    }
}

/* `type Name = T`. A name of a copy of a generic links to the generic. */
static void alias_signature(struct entry *one, const char *lead,
                            const struct name *name, const struct type *t)
{
    text_appendf(&one->signature, "%stype %.*s = ", lead, (int)name->length,
                 name->text);
    type_name(&one->signature, t);
    if (t->generic != NULL) {
        text_append_bytes(&one->copy_of_module, t->generic->module.text,
                          t->generic->module.length);
        text_append_bytes(&one->copy_of, t->generic->name.text,
                          t->generic->name.length);
    }
}

/* One item of the public interface. DESIGN: an `internal` item stands in
   the interface marked as internal, and the object model puts it in the
   developer docs. The user docs of a library leave it out, whichever of
   the two inputs they were built from. */
static void interface_item(struct page *p, const struct symbol *sym)
{
    struct entry *one;
    const char *lead = "pub ";
    struct name shown;
    const char *colon;

    if (sym->type == NULL || sym->internal) {
        return;
    }
    one = entry_add(&p->items, &p->item_count, &p->item_capacity);
    text_append_bytes(&one->name, sym->name.text, sym->name.length);
    one->doc = sym->doc;
    /* An `operator fn` that shares its name stands as `eq:Point`, which
       keeps its heading apart from the others, and its signature names
       the operator alone. */
    shown = sym->name;
    colon = memchr(shown.text, ':', shown.length);
    if (colon != NULL) {
        shown.length = (size_t)(colon - shown.text);
    }
    if (sym->alias) {
        alias_signature(one, lead, &sym->name, sym->type);
        return;
    }
    switch (sym->kind) {
    case SYMBOL_FN:
        fn_signature(&one->signature, lead, sym->worker ? "worker fn" : "fn",
                     &shown, sym->item, sym->type, sym->params,
                     declared_params(sym->type, false), false, false);
        break;
    case SYMBOL_EXTERN_FN:
        fn_signature(&one->signature, lead, "extern fn", &sym->name, NULL,
                     sym->type, sym->params,
                     declared_params(sym->type, false), false, sym->variadic);
        break;
    case SYMBOL_CONSTRAINT:
        constraint_signature(&one->signature, lead, &sym->name, sym->type);
        break;
    case SYMBOL_CONST:
        text_appendf(&one->signature, "%sconst %.*s: ", lead,
                     (int)sym->name.length, sym->name.text);
        type_name(&one->signature, sym->type);
        const_value(&one->signature, sym->value);
        break;
    default:
        type_signature(&one->signature, lead, &sym->name, sym->type);
        body_entries(one, sym->type, false);
        break;
    }
}

/* One item of the syntax tree, which carries the private items and the
   `//#` notes. */
static void tree_item(struct page *p, const struct item *it, bool notes)
{
    const struct symbol *sym = it->symbol;
    struct entry *one;
    const char *lead;
    size_t i;

    if (sym == NULL || sym->type == NULL || it->block != BLOCK_NONE) {
        return;
    }
    one = entry_add(&p->items, &p->item_count, &p->item_capacity);
    text_append_bytes(&one->name, it->name.text, it->name.length);
    one->doc = it->doc;
    if (notes) {
        one->note = it->note;
    }
    lead = visibility_word(it->vis);
    switch (it->kind) {
    case ITEM_TYPE:
        alias_signature(one, lead, &it->name, sym->type);
        break;
    case ITEM_CONSTRAINT:
        constraint_signature(&one->signature, lead, &it->name, sym->type);
        break;
    case ITEM_FN:
    case ITEM_EXTERN_FN: {
        struct name *names = NULL;
        if (it->param_count > 0) {
            names = files_array(it->param_count, sizeof *names);
            for (i = 0; i < it->param_count; i++) {
                names[i] = it->params[i].name;
            }
        }
        fn_signature(&one->signature, lead,
                     it->kind == ITEM_EXTERN_FN
                         ? "extern fn"
                         : (it->worker ? "worker fn" : "fn"),
                     &it->name, it, sym->type, names, it->param_count, false,
                     it->variadic);
        free(names);
        break;
    }
    case ITEM_CONST:
        text_appendf(&one->signature, "%sconst %.*s: ", lead,
                     (int)it->name.length, it->name.text);
        type_name(&one->signature, sym->type);
        const_value(&one->signature, sym->value);
        break;
    default:
        type_signature(&one->signature, lead, &it->name, sym->type);
        body_entries(one, sym->type, true);
        break;
    }
}

/* The items of the tree in the order of the source, those the checker
   took out among them: the generics, every `type` and every
   `constraint`. */
static void tree_items(struct page *p, const struct module *tree, bool notes)
{
    size_t i = 0;
    size_t k = 0;

    while (i < tree->item_count || k < tree->stripped_count) {
        const struct item *a = i < tree->item_count ? tree->items[i] : NULL;
        const struct item *b =
            k < tree->stripped_count ? tree->stripped[k] : NULL;
        if (b != NULL &&
            (a == NULL || b->pos.line < a->pos.line ||
             (b->pos.line == a->pos.line && b->pos.column < a->pos.column))) {
            tree_item(p, b, notes);
            k++;
        } else {
            tree_item(p, a, notes);
            i++;
        }
    }
}

/* Rendering */

static void escape_html(struct text *out, const char *s, size_t n)
{
    size_t i;

    for (i = 0; i < n; i++) {
        switch (s[i]) {
        case '&': text_append(out, "&amp;"); break;
        case '<': text_append(out, "&lt;"); break;
        case '>': text_append(out, "&gt;"); break;
        case '"': text_append(out, "&quot;"); break;
        default: text_append_bytes(out, &s[i], 1); break;
        }
    }
}

/* Whether the bytes are one name: an identifier, or a path of
   identifiers, with an optional `()` at the end. Everything else between
   backticks is code and no name, the rule the check command follows. */
static bool is_name(const char *s, size_t n)
{
    size_t i;
    bool start = true;

    if (n > 2 && s[n - 2] == '(' && s[n - 1] == ')') {
        n -= 2;
    }
    if (n == 0) {
        return false;
    }
    for (i = 0; i < n; i++) {
        char c = s[i];
        if (c == '.') {
            if (start) {
                return false;
            }
            start = true;
            continue;
        }
        if (start && !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                       c == '_')) {
            return false;
        }
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_')) {
            return false;
        }
        start = false;
    }
    return !start;
}

static bool same(const char *s, size_t n, const char *other)
{
    return strlen(other) == n && memcmp(s, other, n) == 0;
}

/* The last segment of a module path, which an import declares as its
   name where the import writes no alias. */
/* The last `.` of the bytes, or NULL where they hold none. */
static const char *last_dot(const char *s, size_t n)
{
    while (n > 0) {
        n--;
        if (s[n] == '.') {
            return s + n;
        }
    }
    return NULL;
}

static bool is_item(const struct interface *iface, const char *s, size_t n)
{
    size_t i;

    for (i = 0; iface != NULL && i < iface->item_count; i++) {
        if (iface->items[i]->name.length == n &&
            memcmp(iface->items[i]->name.text, s, n) == 0) {
            return true;
        }
    }
    return false;
}

/* DESIGN: a backtick name becomes a link where it resolves against the
   items of the module and the modules it imports. An item of the
   documented module links to its place on the page. A `module.Item`
   links to the place on that module's page, and a module path to the
   page itself. The tables are the ones a library file carries, so a page
   built from a library file writes the links of the source. */
static bool resolve_name(const struct interface *iface, const char *s,
                         size_t n, struct text *href)
{
    const char *dot;
    size_t head;
    size_t i;

    if (iface == NULL || !is_name(s, n)) {
        return false;
    }
    if (n > 2 && s[n - 2] == '(' && s[n - 1] == ')') {
        n -= 2;
    }
    if (is_item(iface, s, n)) {
        text_appendf(href, "#%.*s", (int)n, s);
        return true;
    }
    if (same(s, n, iface->module)) {
        text_appendf(href, "%s%s", iface->module, DOC_HTML_SUFFIX);
        return true;
    }
    for (i = 0; i < iface->import_count; i++) {
        if (same(s, n, iface->imports[i])) {
            text_appendf(href, "%s%s", iface->imports[i], DOC_HTML_SUFFIX);
            return true;
        }
    }
    dot = last_dot(s, n);
    if (dot == NULL) {
        return false;
    }
    head = (size_t)(dot - s);
    if (same(s, head, iface->module)) {
        text_appendf(href, "#%.*s", (int)(n - head - 1), dot + 1);
        return true;
    }
    /* A member of an item of the documented module has no place of its
       own on the page. The name links to the item that holds it. */
    if (is_item(iface, s, head)) {
        text_appendf(href, "#%.*s", (int)head, s);
        return true;
    }
    for (i = 0; i < iface->import_count; i++) {
        if (same(s, head, iface->imports[i]) ||
            same(s, head, module_path_last(iface->imports[i]))) {
            text_appendf(href, "%s%s#%.*s", iface->imports[i],
                         DOC_HTML_SUFFIX, (int)(n - head - 1), dot + 1);
            return true;
        }
    }
    return false;
}

/* Inline code in backticks and a link written `[text](url)`. Everything
   else is text, and the forms the subset leaves out stand as they are
   written. */
static void inline_html(struct text *out, const char *s, size_t n,
                        const struct interface *iface)
{
    size_t i = 0;

    while (i < n) {
        if (s[i] == '`') {
            const char *end = memchr(s + i + 1, '`', n - i - 1);
            size_t length;
            struct text href = {0};
            if (end == NULL) {
                escape_html(out, &s[i], 1);
                i++;
                continue;
            }
            length = (size_t)(end - (s + i + 1));
            if (resolve_name(iface, s + i + 1, length, &href)) {
                text_append(out, "<a href=\"");
                escape_html(out, text_cstr(&href), href.length);
                text_append(out, "\">");
            }
            text_append(out, "<code>");
            escape_html(out, s + i + 1, length);
            text_append(out, "</code>");
            if (href.length > 0) {
                text_append(out, "</a>");
            }
            text_free(&href);
            i += length + 2;
            continue;
        }
        if (s[i] == '[') {
            const char *close = memchr(s + i, ']', n - i);
            size_t at;
            const char *end;
            if (close == NULL || (size_t)(close - s) + 1 >= n ||
                close[1] != '(') {
                escape_html(out, &s[i], 1);
                i++;
                continue;
            }
            at = (size_t)(close - s) + 2;
            end = memchr(s + at, ')', n - at);
            if (end == NULL) {
                escape_html(out, &s[i], 1);
                i++;
                continue;
            }
            text_append(out, "<a href=\"");
            escape_html(out, s + at, (size_t)(end - (s + at)));
            text_append(out, "\">");
            escape_html(out, s + i + 1, (size_t)(close - (s + i + 1)));
            text_append(out, "</a>");
            i = (size_t)(end - s) + 1;
            continue;
        }
        escape_html(out, &s[i], 1);
        i++;
    }
}

/* The bytes of one line, without its line end. */
static size_t line_of(const char *s, size_t n, size_t from)
{
    size_t end = from;

    while (end < n && s[end] != '\n') {
        end++;
    }
    return end;
}

static size_t lead_of(const char *s, size_t from, size_t end)
{
    size_t i = from;

    while (i < end && (s[i] == ' ' || s[i] == '\t')) {
        i++;
    }
    return i;
}

/* The doc subset of docs/tooling-addendum.md as HTML: paragraphs, fenced
   code blocks with a language tag, inline code, `-` lists and links.
   Everything else is text. */
static void doc_html(struct text *out, const char *s, size_t n,
                     const struct interface *iface)
{
    size_t at = 0;
    bool fenced = false;
    bool listed = false;
    bool paragraph = false;
    bool item = false;

    while (at <= n) {
        size_t end = line_of(s, n, at);
        size_t lead = lead_of(s, at, end);
        bool blank = lead == end;
        bool fence = end - lead >= 3 && memcmp(s + lead, "```", 3) == 0;
        if (fenced) {
            if (fence) {
                text_append(out, "</pre>\n");
                fenced = false;
            } else {
                escape_html(out, s + at, end - at);
                text_append(out, "\n");
            }
            at = end + 1;
            continue;
        }
        if (fence) {
            if (paragraph) {
                text_append(out, "</p>\n");
                paragraph = false;
            }
            if (listed) {
                text_append(out, item ? "</li>\n</ul>\n" : "</ul>\n");
                listed = false;
                item = false;
            }
            text_append(out, "<pre class=\"" DOC_CLASS_CODE "\">");
            fenced = true;
            at = end + 1;
            continue;
        }
        if (blank) {
            if (paragraph) {
                text_append(out, "</p>\n");
                paragraph = false;
            }
            if (listed) {
                text_append(out, item ? "</li>\n</ul>\n" : "</ul>\n");
                listed = false;
                item = false;
            }
            at = end + 1;
            continue;
        }
        if (end - lead >= 2 && s[lead] == '-' && s[lead + 1] == ' ') {
            if (paragraph) {
                text_append(out, "</p>\n");
                paragraph = false;
            }
            if (!listed) {
                text_append(out, "<ul>\n");
                listed = true;
            }
            if (item) {
                text_append(out, "</li>\n");
            }
            text_append(out, "<li>");
            inline_html(out, s + lead + 2, end - lead - 2, iface);
            item = true;
            at = end + 1;
            continue;
        }
        if (listed && item) {
            text_append(out, " ");
            inline_html(out, s + lead, end - lead, iface);
            at = end + 1;
            continue;
        }
        if (!paragraph) {
            text_append(out, "<p>");
            paragraph = true;
        } else {
            text_append(out, " ");
        }
        inline_html(out, s + lead, end - lead, iface);
        at = end + 1;
    }
    if (paragraph) {
        text_append(out, "</p>\n");
    }
    if (listed) {
        text_append(out, item ? "</li>\n</ul>\n" : "</ul>\n");
    }
    if (fenced) {
        text_append(out, "</pre>\n");
    }
}

/* DESIGN: `anti doc --markdown` passes the doc text through unchanged,
   which the addendum asks for, so a Hugo site renders the subset with
   its own renderer. The generator writes the headings and the
   signatures around it. */
static void doc_markdown(struct text *out, const char *s, size_t n)
{
    if (n == 0) {
        return;
    }
    text_append_bytes(out, s, n);
    if (s[n - 1] != '\n') {
        text_append(out, "\n");
    }
    text_append(out, "\n");
}

static void doc_body(struct text *out, const struct doc_text *doc,
                     const struct interface *iface, enum doc_form form)
{
    if (doc->text == NULL || doc->length == 0) {
        return;
    }
    if (form == DOC_HTML) {
        doc_html(out, doc->text, doc->length, iface);
    } else {
        doc_markdown(out, doc->text, doc->length);
    }
}

/* One list of fields, values, cases or functions. */
static void entry_list(struct text *out, const struct entry *list,
                       size_t count, const char *class_name,
                       const struct interface *iface, enum doc_form form)
{
    size_t i;

    if (count == 0) {
        return;
    }
    if (form == DOC_HTML) {
        text_appendf(out, "<ul class=\"%s\">\n", class_name);
    }
    for (i = 0; i < count; i++) {
        if (form == DOC_HTML) {
            text_append(out, "<li><code>");
            escape_html(out, text_cstr(&list[i].signature),
                        list[i].signature.length);
            text_append(out, "</code>\n");
            doc_body(out, &list[i].doc, iface, form);
            if (list[i].locked) {
                text_append(out, "<p>" DOC_LOCKED "</p>\n");
            }
            text_append(out, "</li>\n");
        } else {
            text_appendf(out, "- `%s`\n", text_cstr(&list[i].signature));
            if (list[i].locked) {
                text_append(out, "  " DOC_LOCKED "\n");
            }
            if (list[i].doc.length > 0) {
                size_t at = 0;
                while (at < list[i].doc.length) {
                    size_t end = line_of(list[i].doc.text, list[i].doc.length,
                                         at);
                    text_appendf(out, "  %.*s\n", (int)(end - at),
                                 list[i].doc.text + at);
                    at = end + 1;
                }
            }
        }
    }
    if (form == DOC_HTML) {
        text_append(out, "</ul>\n");
    } else {
        text_append(out, "\n");
    }
}

static const char *suffix_of(enum doc_form form);

/* DESIGN: the name a `type` gives a copy of a generic links to that
   generic, on the page of its module. The link stands after the
   signature, which the page writes as code. */
static void copy_link(struct text *out, const struct entry *e,
                      const struct interface *iface, enum doc_form form)
{
    struct text href = {0};

    if (e->copy_of.length == 0) {
        return;
    }
    if (iface == NULL || strcmp(text_cstr(&e->copy_of_module),
                                iface->module) != 0) {
        text_appendf(&href, "%s%s", text_cstr(&e->copy_of_module),
                     suffix_of(form));
    }
    text_appendf(&href, "#%s", text_cstr(&e->copy_of));
    if (form == DOC_HTML) {
        text_append(out, "<p>A copy of <a href=\"");
        escape_html(out, text_cstr(&href), href.length);
        text_append(out, "\"><code>");
        escape_html(out, text_cstr(&e->copy_of), e->copy_of.length);
        text_append(out, "</code></a>.</p>\n");
    } else {
        text_appendf(out, "A copy of [`%s`](%s).\n\n", text_cstr(&e->copy_of),
                     text_cstr(&href));
    }
    text_free(&href);
}

static void entry_page(struct text *out, const struct entry *e,
                       const struct interface *iface, enum doc_form form)
{
    if (form == DOC_HTML) {
        /* M14: a name of a library file reaches the page as text. */
        text_append(out, "<section class=\"" DOC_CLASS_ITEM "\" id=\"");
        escape_html(out, text_cstr(&e->name), e->name.length);
        text_append(out, "\">\n<h2>");
        escape_html(out, text_cstr(&e->name), e->name.length);
        text_append(out, "</h2>\n");
        text_append(out, "<pre class=\"" DOC_CLASS_SIGNATURE "\">");
        escape_html(out, text_cstr(&e->signature), e->signature.length);
        text_append(out, "</pre>\n");
    } else {
        text_appendf(out, "## %s\n\n```anti\n%s\n```\n\n",
                     text_cstr(&e->name), text_cstr(&e->signature));
    }
    copy_link(out, e, iface, form);
    doc_body(out, &e->doc, iface, form);
    entry_list(out, e->fields, e->field_count, DOC_CLASS_FIELDS, iface, form);
    entry_list(out, e->members, e->member_count, DOC_CLASS_MEMBERS, iface,
               form);
    if (e->note.length > 0) {
        if (form == DOC_HTML) {
            text_append(out,
                        "<section class=\"" DOC_CLASS_INTERNALS "\">\n"
                        "<h3>Internals</h3>\n");
            doc_body(out, &e->note, iface, form);
            text_append(out, "</section>\n");
        } else {
            text_append(out, "### Internals\n\n");
            doc_body(out, &e->note, iface, form);
        }
    }
    if (form == DOC_HTML) {
        text_append(out, "</section>\n");
    }
}

static void page_render(struct text *out, const struct page *p,
                        enum doc_form form)
{
    size_t i;

    if (form == DOC_HTML) {
        text_append(out, "<!DOCTYPE html>\n<meta charset=\"utf-8\">\n");
        text_append(out, "<title>");
        escape_html(out, text_cstr(&p->module), p->module.length);
        text_append(out, "</title>\n");
        text_append(out, "<main class=\"" DOC_CLASS_PAGE "\">\n");
        text_append(out, "<h1>");
        escape_html(out, text_cstr(&p->module), p->module.length);
        text_append(out, "</h1>\n");
    } else {
        text_appendf(out, "# %s\n\n", text_cstr(&p->module));
    }
    doc_body(out, &p->doc, p->iface, form);
    if (p->note.length > 0) {
        if (form == DOC_HTML) {
            text_append(out,
                        "<section class=\"" DOC_CLASS_INTERNALS "\">\n"
                        "<h2>Internals</h2>\n");
            doc_body(out, &p->note, p->iface, form);
            text_append(out, "</section>\n");
        } else {
            text_append(out, "## Internals\n\n");
            doc_body(out, &p->note, p->iface, form);
        }
    }
    for (i = 0; i < p->item_count; i++) {
        entry_page(out, &p->items[i], p->iface, form);
    }
    if (form == DOC_HTML) {
        text_append(out, "</main>\n");
    }
}

/* The first paragraph of a doc text, which the index prints beside the
   name of the module. */
static void first_paragraph(struct text *out, const struct doc_text *doc,
                            const struct interface *iface, enum doc_form form)
{
    size_t at = 0;

    while (at < doc->length) {
        size_t end = line_of(doc->text, doc->length, at);
        if (lead_of(doc->text, at, end) == end) {
            break;
        }
        if (form == DOC_HTML) {
            if (at > 0) {
                text_append(out, " ");
            }
            inline_html(out, doc->text + at, end - at, iface);
        } else {
            text_appendf(out, "%s%.*s", at > 0 ? " " : "", (int)(end - at),
                         doc->text + at);
        }
        at = end + 1;
    }
}

static void index_render(struct text *out, const struct page *pages,
                         size_t count, enum doc_form form)
{
    size_t i;

    if (form == DOC_HTML) {
        text_append(out, "<!DOCTYPE html>\n<meta charset=\"utf-8\">\n"
                         "<title>Modules</title>\n");
        text_append(out, "<main class=\"" DOC_CLASS_PAGE "\">\n"
                         "<h1>Modules</h1>\n");
        text_append(out, "<ul class=\"" DOC_CLASS_INDEX "\">\n");
    } else {
        text_append(out, "# Modules\n\n");
    }
    for (i = 0; i < count; i++) {
        const char *name = text_cstr(&pages[i].module);
        if (form == DOC_HTML) {
            text_append(out, "<li><a href=\"");
            escape_html(out, name, pages[i].module.length);
            text_append(out, DOC_HTML_SUFFIX "\">");
            escape_html(out, name, pages[i].module.length);
            text_append(out, "</a>");
            if (pages[i].doc.length > 0) {
                text_append(out, "\n<p>");
                first_paragraph(out, &pages[i].doc, pages[i].iface, form);
                text_append(out, "</p>");
            }
            text_append(out, "</li>\n");
        } else {
            text_appendf(out, "- [%s](%s%s)", name, name,
                         DOC_MARKDOWN_SUFFIX);
            if (pages[i].doc.length > 0) {
                text_append(out, "\n  ");
                first_paragraph(out, &pages[i].doc, pages[i].iface, form);
            }
            text_append(out, "\n");
        }
    }
    if (form == DOC_HTML) {
        text_append(out, "</ul>\n</main>\n");
    } else {
        text_append(out, "\n");
    }
}

/* The run */

static const char *suffix_of(enum doc_form form)
{
    return form == DOC_HTML ? DOC_HTML_SUFFIX : DOC_MARKDOWN_SUFFIX;
}

/* Whether the path names a library file. */
static bool is_library(const char *path)
{
    size_t n = strlen(path);
    size_t s = strlen(ANTL_SUFFIX);

    return n > s && strcmp(path + n - s, ANTL_SUFFIX) == 0;
}

/* DESIGN: a module of the project that imports another needs that
   module's interface file, which `antic -c` is what writes. The command
   writes one per source into the work directory, in the order the
   imports ask for. A search root of its own reads them back. `anti
   check` writes the same files for the same reason, and they are the
   only thing the run writes beside the pages. */
static bool write_interfaces(const char *const *sources, size_t count,
                             const char **roots, size_t root_count,
                             const char *work, const char *runtime,
                             struct unit *units, size_t *order)
{
    size_t written = 0;
    size_t i;
    bool ok = true;

    for (i = 0; i < count; i++) {
        memset(&units[i], 0, sizeof units[i]);
        if (is_library(sources[i])) {
            continue;
        }
        if (!unit_read(sources[i], roots, root_count,
                       work, &units[i])) {
            return false;
        }
        written++;
    }
    if (written == 0) {
        return true;
    }
    unit_order(units, count, order);
    for (i = 0; i < count && ok; i++) {
        const struct unit *u = &units[order[i]];
        struct options o;
        if (u->source == NULL || !u->parsed) {
            continue;
        }
        memset(&o, 0, sizeof o);
        o.input = u->source;
        o.roots = roots;
        o.root_count = root_count;
        o.runtime = runtime;
        o.front_end = true;
        o.library = true;
        o.output = text_cstr(&u->library);
        if (!target_host(&o.target)) {
            fputs("anti: unknown host target\n", stderr);
            return false;
        }
        o.cpu = cpu_default(o.target);
        ok = driver_run(&o) == 0;
    }
    return ok;
}

int doc_run(const char *const *sources, size_t count,
            const char *const *roots, size_t root_count, const char *out,
            const char *work, const char *runtime, enum doc_form form,
            bool dev, bool private_items)
{
    struct arena *arenas = NULL;
    struct types *tables = NULL;
    struct ir_module *programs = NULL;
    struct page *pages = NULL;
    struct unit *units = NULL;
    const char **search = NULL;
    size_t *order = NULL;
    struct text body = {0};
    struct text path = {0};
    size_t search_count = 0;
    size_t made = 0;
    size_t i;
    int status = 0;

    if (count == 0) {
        fputs("anti: doc takes a source or a library file\n", stderr);
        return 2;
    }
    arenas = files_array(count, sizeof *arenas);
    tables = files_array(count, sizeof *tables);
    programs = files_array(count, sizeof *programs);
    pages = files_array(count, sizeof *pages);
    units = files_array(count, sizeof *units);
    order = files_array(count, sizeof *order);
    search = files_array(root_count + 2, sizeof *search);
    for (i = 0; i < root_count; i++) {
        search[search_count++] = roots[i];
    }
    search[search_count++] = work;
    if (!files_make_dirs(out) || !files_make_dirs(work) ||
        !write_interfaces(sources, count, search, search_count, work, runtime,
                          units, order)) {
        status = 1;
        goto done;
    }
    for (i = 0; i < count; i++) {
        struct options o;
        const struct interface *iface;
        struct module *tree = NULL;
        size_t j;
        memset(&o, 0, sizeof o);
        o.input = sources[i];
        o.roots = search;
        o.root_count = search_count;
        o.runtime = runtime;
        o.front_end = true;
        if (!target_host(&o.target)) {
            fputs("anti: unknown host target\n", stderr);
            status = 2;
            goto done;
        }
        o.cpu = cpu_default(o.target);
        if ((dev || private_items) && is_library(sources[i])) {
            fprintf(stderr, "anti: %s: %s needs the source\n", sources[i],
                    dev ? "--dev" : "--private");
            status = 1;
            goto done;
        }
        ir_module_init(&programs[i], &arenas[i], "");
        made = i + 1;
        iface = driver_interface(&o, &arenas[i], &tables[i], &programs[i],
                                 &tree);
        if (iface == NULL) {
            status = 1;
            goto done;
        }
        /* M14: the module path of a library file names the file of its
           page, and a library file may come from a repository. */
        if (!repo_name_valid(iface->module)) {
            fprintf(stderr, "anti: %s names the module %s, which is no module "
                            "path\n", sources[i], iface->module);
            status = 1;
            goto done;
        }
        pages[i].iface = iface;
        text_append(&pages[i].module, iface->module);
        if (tree != NULL && (dev || private_items)) {
            pages[i].doc = tree->doc;
            if (dev) {
                pages[i].note = tree->note;
            }
            tree_items(&pages[i], tree, dev);
        } else {
            pages[i].doc.text = iface->doc;
            pages[i].doc.length = iface->doc != NULL ? strlen(iface->doc) : 0;
            for (j = 0; j < iface->item_count; j++) {
                interface_item(&pages[i], iface->items[j]);
            }
        }
    }
    for (i = 0; i < count; i++) {
        body.length = 0;
        path.length = 0;
        page_render(&body, &pages[i], form);
        text_appendf(&path, "%s/%s%s", out, text_cstr(&pages[i].module),
                     suffix_of(form));
        if (!files_write(text_cstr(&path), &body)) {
            status = 1;
            goto done;
        }
    }
    body.length = 0;
    path.length = 0;
    index_render(&body, pages, count, form);
    text_appendf(&path, "%s/index%s", out, suffix_of(form));
    if (!files_write(text_cstr(&path), &body)) {
        status = 1;
        goto done;
    }
    printf("doc: %zu module%s into %s\n", count, count == 1 ? "" : "s", out);

done:
    for (i = 0; i < count; i++) {
        page_free(&pages[i]);
        if (units[i].source != NULL) {
            unit_free(&units[i]);
        }
    }
    for (i = 0; i < made; i++) {
        ir_module_free(&programs[i]);
        arena_free(&arenas[i]);
    }
    free(pages);
    free(programs);
    free(tables);
    free(arenas);
    free(units);
    free(order);
    free((void *)search);
    text_free(&body);
    text_free(&path);
    return status;
}

/* The page of `anti doc`, built from the public interface of a module or
   from its syntax tree. antic.h holds the records and the DESIGN of the
   split: anti renders them and reads nothing of the checker. */
#include "antic.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "ast.h"
#include "driver.h"
#include "ir.h"
#include "sema.h"
#include "types.h"

static struct doc_entry *entry_add(struct doc_entry **list, size_t *count,
                                   size_t *capacity)
{
    struct doc_entry *one;

    *list = alloc_grow(*list, capacity, *count + 1, sizeof **list);
    one = &(*list)[(*count)++];
    memset(one, 0, sizeof *one);
    return one;
}

static void entry_free(struct doc_entry *e)
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
    text_free(&e->doc);
    text_free(&e->note);
    text_free(&e->copy_of_module);
    text_free(&e->copy_of);
}

void doc_page_free(struct doc_page *page)
{
    size_t i;

    for (i = 0; i < page->item_count; i++) {
        entry_free(&page->items[i]);
    }
    for (i = 0; i < page->name_count; i++) {
        text_free(&page->names[i]);
    }
    for (i = 0; i < page->import_count; i++) {
        text_free(&page->imports[i]);
    }
    free(page->items);
    free(page->names);
    free(page->imports);
    text_free(&page->module);
    text_free(&page->doc);
    text_free(&page->note);
    memset(page, 0, sizeof *page);
}

/* A copy of a doc text of the module, which the page outlives. */
static void doc_copy(struct text *out, const struct doc_text *doc)
{
    if (doc->text != NULL && doc->length > 0) {
        text_append_bytes(out, doc->text, doc->length);
    }
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

/* One function a page shows: the words before its form word, the form
   word, the name, the generic that declares it or NULL, its type, the
   names of its parameters or NULL, how many of them follow self, whether
   it takes self and whether it is variadic. */
struct fn_shown {
    const char *lead;
    const char *keyword;
    const struct name *name;
    const struct item *generic;
    const struct type *type;
    const struct name *params;
    size_t param_count;
    bool self;
    bool variadic;
};

/* DESIGN: a signature carries the visibility, the form word and the name
   of the function, the name, the mark and the type of every parameter,
   the result and `may fail`. It carries no default value, no `own` and no
   body. The library file keeps the first set for every function and the
   rest for some. A page built from the file reads as the page built from
   the source. */
static void fn_signature(struct text *out, const struct fn_shown *f)
{
    const struct type *result = declared_result(f->type);
    size_t first = f->self ? 1 : 0;
    size_t i;

    text_appendf(out, "%s%s %.*s", f->lead, f->keyword, (int)f->name->length,
                 f->name->text);
    fn_params(out, f->generic);
    text_append(out, "(");
    if (f->self) {
        text_append(out, "self");
    }
    for (i = 0; i < f->param_count && first + i < f->type->param_count; i++) {
        const struct type *p = f->type->params[first + i];
        text_append(out, i > 0 || f->self ? ", " : "");
        /* A parameter of function type carries its mark in its type. An
           `extern fn` takes C function pointers alone and writes none. */
        if (p->kind == TYPE_FN && !p->bound && !c_function(f->keyword)) {
            text_append(out, p->owned        ? "keep own "
                             : !p->context   ? "keep "
                             : p->concurrent ? "concurrent "
                                             : "");
        }
        /* `lent` stands before the name, as the declaration writes it. */
        if (types_is_lent(p) && f->params != NULL &&
            f->params[i].length > 0) {
            struct type bare = *p;
            bare.lent = false;
            text_appendf(out, "lent %.*s: ", (int)f->params[i].length,
                         f->params[i].text);
            types_name(out, &bare);
            continue;
        }
        if (f->params != NULL && f->params[i].length > 0) {
            text_appendf(out, "%.*s: ", (int)f->params[i].length,
                         f->params[i].text);
        }
        types_name(out, p);
    }
    if (f->variadic) {
        text_append(out, f->param_count > 0 || f->self ? ", ..." : "...");
    }
    text_append(out, ")");
    if (result != NULL) {
        text_append(out, " -> ");
        types_name(out, result);
    }
    if (f->type->may_fail) {
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
        if (v->type != NULL && !types_is_signed(v->type)) {
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
            types_name(out, t->base);
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
            types_name_qualified(out, t->base);
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
    types_name(out, f->type);
    if (f->bits > 0) {
        text_appendf(out, " : %u", (unsigned)f->bits);
    }
    return true;
}

/* The cases of a variant, each with the fields it holds. The tag enum
   names them in the order of the declaration. */
static void variant_cases(struct doc_entry *item, const struct type *t)
{
    size_t i;
    size_t j;

    if (t->base == NULL) {
        return;
    }
    for (i = 0; i < t->base->field_count && i < t->param_count; i++) {
        struct doc_entry *one = entry_add(&item->fields, &item->field_count,
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
            types_name(&one->signature, body->fields[j].type);
        }
        text_append(&one->signature, " }");
    }
}

/* The values of an enum, each with the number it stands for. */
static void enum_values(struct doc_entry *item, const struct type *t)
{
    size_t i;

    for (i = 0; i < t->field_count; i++) {
        struct doc_entry *one = entry_add(&item->fields, &item->field_count,
                                      &item->field_capacity);
        text_append_bytes(&one->name, t->fields[i].name.text,
                          t->fields[i].name.length);
        text_appendf(&one->signature, "%.*s = %" PRIu64,
                     (int)t->fields[i].name.length, t->fields[i].name.text,
                     t->fields[i].number);
        doc_copy(&one->doc, &t->fields[i].doc);
    }
}

/* The fields and the functions of a body. all takes every one of them,
   which dev docs and `--private` ask for, and user docs take the `pub`
   ones alone. */
static void body_entries(struct doc_entry *item, const struct type *t, bool all)
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
        struct doc_entry *one;
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
        doc_copy(&one->doc, &f->doc);
    }
    for (i = 0; i < t->member_count; i++) {
        const struct item *m = t->members[i];
        struct fn_shown shown = {0};
        struct doc_entry *one;
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
            names = alloc_zeroed(params, sizeof *names);
            for (j = 0; j < params; j++) {
                names[j] = m->symbol->params != NULL ? m->symbol->params[j]
                                                     : m->params[j].name;
            }
        }
        shown.lead = "";
        shown.keyword = "fn";
        shown.name = &m->name;
        shown.type = m->symbol->type;
        shown.params = names;
        shown.param_count = params;
        shown.self = self;
        fn_signature(&one->signature, &shown);
        free(names);
        doc_copy(&one->doc, &m->doc);
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
static void alias_signature(struct doc_entry *one, const char *lead,
                            const struct name *name, const struct type *t)
{
    text_appendf(&one->signature, "%stype %.*s = ", lead, (int)name->length,
                 name->text);
    types_name(&one->signature, t);
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
static void interface_item(struct doc_page *p, const struct symbol *sym)
{
    struct fn_shown fn = {0};
    struct doc_entry *one;
    const char *lead = "pub ";
    struct name shown;
    const char *colon;

    if (sym->type == NULL || sym->internal) {
        return;
    }
    one = entry_add(&p->items, &p->item_count, &p->item_capacity);
    text_append_bytes(&one->name, sym->name.text, sym->name.length);
    doc_copy(&one->doc, &sym->doc);
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
        fn.lead = lead;
        fn.keyword = sym->worker ? "worker fn" : "fn";
        fn.name = &shown;
        fn.generic = sym->item;
        fn.type = sym->type;
        fn.params = sym->params;
        fn.param_count = declared_params(sym->type, false);
        fn_signature(&one->signature, &fn);
        break;
    case SYMBOL_EXTERN_FN:
        fn.lead = lead;
        fn.keyword = "extern fn";
        fn.name = &sym->name;
        fn.type = sym->type;
        fn.params = sym->params;
        fn.param_count = declared_params(sym->type, false);
        fn.variadic = sym->variadic;
        fn_signature(&one->signature, &fn);
        break;
    case SYMBOL_CONSTRAINT:
        constraint_signature(&one->signature, lead, &sym->name, sym->type);
        break;
    case SYMBOL_CONST:
        text_appendf(&one->signature, "%sconst %.*s: ", lead,
                     (int)sym->name.length, sym->name.text);
        types_name(&one->signature, sym->type);
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
static void tree_item(struct doc_page *p, const struct item *it, bool notes)
{
    const struct symbol *sym = it->symbol;
    struct fn_shown fn = {0};
    struct doc_entry *one;
    const char *lead;
    size_t i;

    if (sym == NULL || sym->type == NULL || it->block != BLOCK_NONE) {
        return;
    }
    one = entry_add(&p->items, &p->item_count, &p->item_capacity);
    text_append_bytes(&one->name, it->name.text, it->name.length);
    doc_copy(&one->doc, &it->doc);
    if (notes) {
        doc_copy(&one->note, &it->note);
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
            names = alloc_zeroed(it->param_count, sizeof *names);
            for (i = 0; i < it->param_count; i++) {
                names[i] = it->params[i].name;
            }
        }
        fn.lead = lead;
        fn.keyword = it->kind == ITEM_EXTERN_FN ? "extern fn"
                     : it->worker               ? "worker fn"
                                                : "fn";
        fn.name = &it->name;
        fn.generic = it;
        fn.type = sym->type;
        fn.params = names;
        fn.param_count = it->param_count;
        fn.variadic = it->variadic;
        fn_signature(&one->signature, &fn);
        free(names);
        break;
    }
    case ITEM_CONST:
        text_appendf(&one->signature, "%sconst %.*s: ", lead,
                     (int)it->name.length, it->name.text);
        types_name(&one->signature, sym->type);
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
static void tree_items(struct doc_page *p, const struct module *tree, bool notes)
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

/* The tables a backtick name resolves against, copied from the
   interface: the name of every item and the path of every import. */
static void link_tables(struct doc_page *p, const struct interface *iface)
{
    size_t i;

    p->names = alloc_zeroed(iface->item_count, sizeof *p->names);
    for (i = 0; i < iface->item_count; i++) {
        text_append_bytes(&p->names[i], iface->items[i]->name.text,
                          iface->items[i]->name.length);
    }
    p->name_count = iface->item_count;
    p->imports = alloc_zeroed(iface->import_count, sizeof *p->imports);
    for (i = 0; i < iface->import_count; i++) {
        text_append(&p->imports[i], iface->imports[i]);
    }
    p->import_count = iface->import_count;
}

bool antic_doc_page(const struct options *options, enum doc_items items,
                    struct doc_page *out)
{
    struct arena arena = {0};
    struct types types;
    struct ir_module program;
    struct module *tree = NULL;
    const struct interface *iface;
    size_t i;

    memset(out, 0, sizeof *out);
    ir_module_init(&program, &arena, "");
    iface = driver_interface(options, &arena, &types, &program, &tree);
    if (iface != NULL) {
        text_append(&out->module, iface->module);
        link_tables(out, iface);
        if (tree != NULL && items != DOC_ITEMS_PUBLIC) {
            doc_copy(&out->doc, &tree->doc);
            if (items == DOC_ITEMS_DEV) {
                doc_copy(&out->note, &tree->note);
            }
            tree_items(out, tree, items == DOC_ITEMS_DEV);
        } else {
            if (iface->doc != NULL) {
                text_append(&out->doc, iface->doc);
            }
            for (i = 0; i < iface->item_count; i++) {
                interface_item(out, iface->items[i]);
            }
        }
    }
    ir_module_free(&program);
    arena_free(&arena);
    return iface != NULL;
}

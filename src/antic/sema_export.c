/* The checks of what crosses to C, the doc warnings of `anti check` and
   the interface of a checked module. */

#include <stdlib.h>
#include <string.h>

#include "sema_checker.h"

/* A copy of the length bytes at text with a NUL after them, in the
   memory pool of the first parameter, which frees it. */
static const char *keep_name(struct arena *arena, const char *text,
                             size_t length)
{
    char *copy = arena_alloc(arena, length + 1);

    memcpy(copy, text, length);
    return copy;
}

/* DESIGN: `anti.lang.Error` crosses to C as `struct anti_Error *`, the
   type the object model gives the generated helpers. The header declares
   the tag itself and C never reads the layout, so an export signature
   names the class without it being an `export class`. A class below it
   has no C name and stays out. */
static bool is_error_class(const struct type *t)
{
    return types_is_lang_error(t);
}

/* Whether t is a type declared in the body of cls, at any depth. */
static bool nested_in(const struct type *t, const struct item *cls)
{
    size_t i;

    for (i = 0; cls != NULL && i < cls->nested_count; i++) {
        const struct item *inner = cls->nested[i];
        if ((inner->symbol != NULL && inner->symbol->type == t) ||
            nested_in(t, inner)) {
            return true;
        }
    }
    return false;
}

/* Whether a value of type t has a C representation. That is a scalar
   other than char, a pointer to such a type, an exported struct or union,
   or a function pointer of such types. A struct field may also be a
   fixed-size array of such a type. A type nested in the export class cls
   crosses with it. Sets *hidden to a struct that is not exported. */
static bool c_representable(const struct type *t, bool field,
                            const struct item *cls,
                            const struct type **hidden)
{
    size_t i;

    switch (t->kind) {
    case TYPE_CHAR:
    case TYPE_STR:
    case TYPE_SLICE:
    case TYPE_NONE:
        return false;
    case TYPE_ARRAY:
        return field && t->length_of == NULL &&
               c_representable(t->element, true, cls, hidden);
    case TYPE_POINTER:
        return c_representable(t->element, false, cls, hidden);
    case TYPE_FN:
        for (i = 0; i < t->param_count; i++) {
            if (!c_representable(t->params[i], false, cls, hidden)) {
                return false;
            }
        }
        return t->result->kind == TYPE_VOID ||
               c_representable(t->result, false, cls, hidden);
    /* Flags crosses as the struct of four bools that the header writes
       for it. A Mutex and a channel hold a handle of the runtime, which
       C has no declaration of. */
    case TYPE_STRUCT:
    case TYPE_CLASS:
    case TYPE_VARIANT:
        if (t->item_exported || is_error_class(t) || types_is_flags(t) ||
            nested_in(t, cls)) {
            return true;
        }
        if (types_is_mutex(t) || types_is_chan(t) || types_is_regex(t)) {
            return false;
        }
        /* The hidden lock of a synchronized class, which the header
           writes as its bytes. */
        if (types_is_object_lock(t)) {
            return true;
        }
        *hidden = t;
        return false;
    /* A tuple crosses as the named struct the header writes for it, so
       it crosses when every element does. Its elements are fields of
       that struct, which is why an array among them is allowed. */
    case TYPE_TUPLE:
        for (i = 0; i < t->param_count; i++) {
            if (!c_representable(t->params[i], true, cls, hidden)) {
                return false;
            }
        }
        return true;
    /* An enum is its underlying integer, which the header writes as an
       enum of the same name. */
    case TYPE_ENUM:
        return true;
    default:
        return true;
    }
}

/* Report a type in an export signature or struct that C cannot represent.
   what names the place, as `the parameter `s` of export fn `f``. */
static void check_c_type(struct checker *c, struct pos pos, const char *what,
                         const struct type *t, bool field,
                         const struct item *cls)
{
    const struct type *hidden = NULL;

    if (sema_is_error(t) || c_representable(t, field, cls, &hidden)) {
        return;
    }
    if (hidden != NULL) {
        sema_error_at(c, pos, "%s has type `%s`, and `%s` is not exported",
                      what,
                      sema_tn(t), sema_tn(hidden));
    } else {
        sema_error_at(c, pos, "%s has type `%s`, which C cannot represent",
                      what,
                      sema_tn(t));
    }
}

/* DESIGN: C has no type that says a pointer is never `none`, so a
   pointer that comes from C may be `none` whatever the declaration says.
   Every pointer of an `extern fn` and of the C callbacks in its
   signature is therefore `?*T`, and the program checks it where it uses
   it. An export item goes the other way: its pointers are the ones this
   program holds, so a `*T` there is honest, and the header marks it as
   non-null in a comment. */
static bool has_plain_pointer(const struct type *t)
{
    size_t i;

    if (t == NULL) {
        return false;
    }
    switch (t->kind) {
    case TYPE_POINTER:
        return !t->nullable || has_plain_pointer(t->element);
    case TYPE_ARRAY:
    case TYPE_SLICE:
        return has_plain_pointer(t->element);
    case TYPE_FN:
        for (i = 0; i < t->param_count; i++) {
            if (has_plain_pointer(t->params[i])) {
                return true;
            }
        }
        return has_plain_pointer(t->result);
    default:
        return false;
    }
}

/* Report a `*T` where the C boundary takes `?*T`. what names the place. */
static void check_c_nullable(struct checker *c, struct pos pos,
                             const char *what, const struct type *t)
{
    if (!sema_is_error(t) && has_plain_pointer(t)) {
        sema_error_at(c, pos, "every pointer %s is `?*T`", what);
    }
}

/* Every pointer of an `extern fn` signature, the C callbacks in it
   included. */
void sema_check_extern_fn(struct checker *c, struct item *it)
{
    const struct type *t = it->symbol->type;
    char what[160];
    size_t i;

    if (t == NULL || t->kind != TYPE_FN) {
        return;
    }
    for (i = 0; i < it->param_count && i < t->param_count; i++) {
        sema_format_to(what, sizeof what,
                       "of the parameter `%.*s` of `extern fn "
                       "%.*s`", (int)it->params[i].name.length,
                       it->params[i].name.text, (int)it->name.length,
                       it->name.text);
        check_c_nullable(c, it->params[i].pos, what, t->params[i]);
    }
    if (it->result != NULL && t->result->kind != TYPE_VOID) {
        sema_format_to(what, sizeof what, "of the result of `extern fn %.*s`",
                       (int)it->name.length, it->name.text);
        check_c_nullable(c, it->result->pos, what, t->result);
    }
}

/* The type nested in cls that a field of type t holds by value or
   points to, through arrays and pointers, or NULL. */
static const struct type *held_nested(const struct type *t,
                                      const struct item *cls)
{
    while (t != NULL && (t->kind == TYPE_POINTER || t->kind == TYPE_ARRAY)) {
        t = t->element;
    }
    return t != NULL && nested_in(t, cls) ? t : NULL;
}

/* DESIGN: a type nested in an export class crosses to C with the class
   when the layout reaches it. A field of the class reaches it, or a field
   of another nested type the layout reaches. The header writes the layout of each one, so
   its fields follow the export rule. A nested type the layout does not
   reach stays out of C, as a library file carries it only when a field
   reaches it. */
static void check_nested_fields(struct checker *c, const struct item *cls,
                                const struct type *t, struct ptr_set *seen)
{
    char what[160];
    size_t i;

    for (i = 0; i < t->field_count; i++) {
        const struct struct_field *f = &t->fields[i];
        const struct type *inner = held_nested(f->type, cls);
        size_t j;
        if (inner == NULL || !sema_ptr_set_add(seen, inner) ||
            inner->kind == TYPE_ENUM) {
            continue;
        }
        for (j = 0; j < inner->field_count; j++) {
            const struct struct_field *g = &inner->fields[j];
            if (g->form == FIELD_BASE || g->form == FIELD_TABLE) {
                continue;
            }
            sema_format_to(what, sizeof what,
                           "the field `%.*s` of `%.*s`, which export class "
                           "`%.*s` holds,",
                           (int)g->name.length, g->name.text,
                           (int)inner->name.length, inner->name.text,
                           (int)cls->name.length, cls->name.text);
            check_c_type(c, g->pos, what, g->type, true, cls);
        }
        check_nested_fields(c, cls, inner, seen);
    }
}

/* DESIGN: an export item has a C symbol and appears in a C header. Its
   types have a C representation, its name is not main, and no other module
   exports the same name. */
void sema_check_export(struct checker *c, struct item *it)
{
    const struct type *t = it->symbol->type;
    char what[160];
    size_t i;
    size_t j;

    switch (it->kind) {
    case ITEM_FN:
        if (sema_name_is(&it->name, "main")) {
            sema_error_at(c, it->name_pos,
                          "`main` is the entry of the program and "
                          "cannot be exported");
            return;
        }
        for (i = 0; i < it->param_count && t->kind == TYPE_FN; i++) {
            sema_format_to(what, sizeof what,
                           "the parameter `%.*s` of export fn "
                           "`%.*s`", (int)it->params[i].name.length,
                           it->params[i].name.text, (int)it->name.length,
                           it->name.text);
            check_c_type(c, it->params[i].pos, what, t->params[i], false,
                         NULL);
        }
        if (it->result != NULL && t->kind == TYPE_FN &&
            t->result->kind != TYPE_VOID) {
            sema_format_to(what, sizeof what, "the result of export fn `%.*s`",
                           (int)it->name.length, it->name.text);
            check_c_type(c, it->result->pos, what, t->result, false, NULL);
        }
        for (i = 0; i < c->library_count; i++) {
            const struct interface *lib = c->libraries[i];
            for (j = 0; j < lib->item_count; j++) {
                const struct symbol *other = lib->items[j];
                if (other->exported && other->kind == SYMBOL_FN &&
                    sema_same_name(&other->name, &it->name)) {
                    sema_error_at(c, it->name_pos,
                                  "export fn `%.*s` has the symbol "
                                  "of export fn `%.*s` in module `%s`",
                                  (int)it->name.length, it->name.text,
                                  (int)other->name.length, other->name.text,
                                  lib->module);
                }
            }
        }
        return;
    /* DESIGN: an export class crosses as its layout, its table type and
       one prototype per public function. Every field and every public
       signature therefore follows the export rule, and the base and the
       table pointer are the compiler's own and always cross. A
       `construct` with arguments crosses as `anti_<Class>_construct`
       whatever its level, so its parameters follow the rule as well. */
    case ITEM_CLASS:
        for (i = 0; i < t->field_count; i++) {
            if (t->fields[i].form == FIELD_BASE ||
                t->fields[i].form == FIELD_TABLE) {
                continue;
            }
            sema_format_to(what, sizeof what,
                           "the field `%.*s` of export class `%.*s`",
                           (int)t->fields[i].name.length,
                           t->fields[i].name.text,
                           (int)it->name.length, it->name.text);
            check_c_type(c, t->fields[i].pos, what, t->fields[i].type, true,
                         it);
        }
        if (it->nested_count > 0) {
            struct ptr_set seen = {0};
            check_nested_fields(c, it, t, &seen);
            free((void *)seen.slots);
        }
        for (i = 0; i < it->member_count; i++) {
            const struct item *m = it->members[i];
            const struct type *ft = m->symbol != NULL ? m->symbol->type : NULL;
            bool made = m->has_self && m->param_count > 0 &&
                        sema_name_is(&m->name, "construct");
            if (m->kind != ITEM_FN || (!m->pub && !made) || ft == NULL ||
                ft->kind != TYPE_FN) {
                continue;
            }
            for (j = m->has_self ? 1 : 0; j < ft->param_count; j++) {
                sema_format_to(what, sizeof what,
                               "the parameter %zu of `%.*s.%.*s`",
                               j, (int)it->name.length, it->name.text,
                               (int)m->name.length, m->name.text);
                check_c_type(c, m->name_pos, what, ft->params[j], false,
                             NULL);
            }
            if (ft->result->kind != TYPE_VOID) {
                sema_format_to(what, sizeof what, "the result of `%.*s.%.*s`",
                               (int)it->name.length, it->name.text,
                               (int)m->name.length, m->name.text);
                check_c_type(c, m->name_pos, what, ft->result, false, NULL);
            }
        }
        return;
    case ITEM_STRUCT:
    case ITEM_UNION:
        for (i = 0; i < t->field_count; i++) {
            sema_format_to(what, sizeof what,
                           "the field `%.*s` of export %s `%.*s`",
                           (int)t->fields[i].name.length,
                           t->fields[i].name.text,
                           it->kind == ITEM_UNION ? "union" : "struct",
                           (int)it->name.length, it->name.text);
            check_c_type(c, t->fields[i].pos, what, t->fields[i].type, true,
                         NULL);
        }
        /* DESIGN: the header writes align(N) as _Alignas on the first
           field, which keeps offset 0 on every target. C refuses
           _Alignas on a bitfield. */
        if (it->align != NULL && t->field_count > 0 &&
            (t->fields[0].bits > 0 ||
             type_field_is_unit_break(&t->fields[0]))) {
            sema_error_at(c, t->fields[0].pos,
                          "the first field `%.*s` of export "
                          "%s `%.*s` is a bitfield, and the C header aligns "
                          "the struct on its first field",
                          (int)t->fields[0].name.length, t->fields[0].name.text,
                          it->kind == ITEM_UNION ? "union" : "struct",
                          (int)it->name.length, it->name.text);
        }
        return;
    /* A variant crosses as the struct of its tag and the union of its
       cases, so the fields of every case follow the export rule. */
    case ITEM_VARIANT:
        for (i = 0; i < t->param_count; i++) {
            const struct type *payload = t->params[i];
            for (j = 0; payload != NULL && j < payload->field_count; j++) {
                sema_format_to(what, sizeof what,
                               "the field `%.*s` of case `%.*s` "
                               "of export variant `%.*s`",
                               (int)payload->fields[j].name.length,
                               payload->fields[j].name.text,
                               (int)t->base->fields[i].name.length,
                               t->base->fields[i].name.text,
                               (int)it->name.length, it->name.text);
                check_c_type(c, payload->fields[j].pos, what,
                             payload->fields[j].type, true, NULL);
            }
        }
        return;
    case ITEM_CONST:
        if (!sema_is_error(t) && !type_is_numeric(t) &&
            t->kind != TYPE_BOOL && t->kind != TYPE_STR) {
            sema_error_at(c, it->type->pos,
                          "export const `%.*s` has type `%s`, and "
                          "an export const is a number, a bool or a str",
                          (int)it->name.length, it->name.text, sema_tn(t));
        }
        return;
    default:
        return;
    }
}

/* DESIGN: the doc warnings read a doc comment against the markup subset
   of "Doc markup" in docs/tooling-addendum.md. The subset holds
   paragraphs, fenced code blocks, inline code, `-` lists and links.
   Markup outside it is a warning, and so is a backtick name that
   resolves nowhere. Each warning stands at the position of the comment,
   where a reader looks for its text. */
struct doc_scope {
    const struct module *module;
    struct name module_name;    /* empty when the module has no name */
    const struct interface *const *libraries;
    size_t library_count;
    struct types *types;
    struct diagnostics *diags;
    const struct item *item;    /* the item the comment belongs to */
};

static void doc_markup_warning(const struct doc_scope *s,
                               const struct doc_text *doc,
                               const struct name *owner, const char *what)
{
    if (owner == NULL) {
        diagnostics_doc(s->diags, NAME_DOC_MARKUP, doc->line, doc->column,
                        "the doc comment of the module holds %s, which the "
                        "doc markup has not", what);
        return;
    }
    diagnostics_doc(s->diags, NAME_DOC_MARKUP, doc->line, doc->column,
                    "the doc comment of `%.*s` holds %s, which the doc "
                    "markup has not", (int)owner->length, owner->text, what);
}

static void doc_name_warning(const struct doc_scope *s,
                             const struct doc_text *doc,
                             const struct name *owner, const char *name,
                             size_t length)
{
    if (owner == NULL) {
        diagnostics_doc(s->diags, NAME_DOC_UNRESOLVED, doc->line, doc->column,
                        "`%.*s` in the doc comment of the module resolves to "
                        "nothing", (int)length, name);
        return;
    }
    diagnostics_doc(s->diags, NAME_DOC_UNRESOLVED, doc->line, doc->column,
                    "`%.*s` in the doc comment of `%.*s` resolves to nothing",
                    (int)length, name, (int)owner->length, owner->text);
}

static bool doc_same(const struct name *a, const char *text, size_t length)
{
    return a->length == length && memcmp(a->text, text, length) == 0;
}

/* The last segment of a module path, which is the name an import
   declares. */
static struct name doc_last_segment(struct name path)
{
    size_t start = path.length;

    while (start > 0 && path.text[start - 1] != '.') {
        start--;
    }
    path.text += start;
    path.length -= start;
    return path;
}

/* Whether one name of the item, its own or one it declares, is the n
   bytes at s. */
static bool doc_name_in_item(const struct item *it, const char *s, size_t n)
{
    size_t i;

    if (doc_same(&it->name, s, n)) {
        return true;
    }
    for (i = 0; i < it->param_count; i++) {
        if (doc_same(&it->params[i].name, s, n)) {
            return true;
        }
    }
    for (i = 0; i < it->case_count; i++) {
        size_t j;
        if (doc_same(&it->cases[i].name, s, n)) {
            return true;
        }
        for (j = 0; j < it->cases[i].field_count; j++) {
            if (doc_same(&it->cases[i].fields[j].name, s, n)) {
                return true;
            }
        }
    }
    for (i = 0; i < it->member_count; i++) {
        if (doc_name_in_item(it->members[i], s, n)) {
            return true;
        }
    }
    return false;
}

/* DESIGN: a backtick name resolves through the tables this pass has.
   Those are the items of the module and the names they declare, the
   imports, the module itself and the items of every loaded library. A
   path of two segments or more resolves through its first segment. The
   deeper lookup needs the type tables of the checker, which this pass
   does not carry. Whatever resolves nowhere is the warning. */
static bool doc_name_known(const struct doc_scope *s, const char *name,
                           size_t length)
{
    const char *dot = memchr(name, '.', length);
    size_t head = dot == NULL ? length : (size_t)(dot - name);
    size_t i;

    /* The names the compiler declares itself, which no module holds.
       They are the root and the other types of `anti.lang`. Two
       functions of the object model and the entry function of a program
       follow them. A doc comment names each as a language word. */
    static const char *const declared[] = {
        LANG_OBJECT, LANG_JOB, LANG_FLAGS, LANG_MUTEX, LANG_REGEX,
        LANG_FIELD_DESCRIPTOR,
        "construct", "deserialize", "main"
    };
    const struct type *object;

    if (lexer_is_keyword(name, head)) {
        return true;
    }
    for (i = 0; i < sizeof declared / sizeof declared[0]; i++) {
        if (strlen(declared[i]) == head &&
            memcmp(declared[i], name, head) == 0) {
            return true;
        }
    }
    object = s->types != NULL ? types_object(s->types) : NULL;
    for (i = 0; object != NULL && i < object->member_count; i++) {
        if (doc_name_in_item(object->members[i], name, head)) {
            return true;
        }
    }
    if (s->module_name.length > 0) {
        struct name last = doc_last_segment(s->module_name);
        size_t first = 0;
        if (doc_same(&last, name, head)) {
            return true;
        }
        while (first < s->module_name.length &&
               s->module_name.text[first] != '.') {
            first++;
        }
        if (first == head && memcmp(s->module_name.text, name, head) == 0) {
            return true;
        }
    }
    for (i = 0; i < s->module->import_count; i++) {
        const struct import *im = &s->module->imports[i];
        const char *segment;
        size_t last;
        if (im->alias.length > 0 && doc_same(&im->alias, name, head)) {
            return true;
        }
        if (im->module.length == length &&
            memcmp(im->module.text, name, length) == 0) {
            return true;
        }
        segment = im->module.text;
        last = im->module.length;
        while (last > 0 && segment[last - 1] != '.') {
            last--;
        }
        if (im->module.length - last == head &&
            memcmp(segment + last, name, head) == 0) {
            return true;
        }
        /* The first segment of a module path, so that a doc comment may
           write the whole path of an item of another package. */
        last = 0;
        while (last < im->module.length && segment[last] != '.') {
            last++;
        }
        if (last == head && memcmp(segment, name, head) == 0) {
            return true;
        }
    }
    for (i = 0; i < s->module->item_count; i++) {
        if (doc_name_in_item(s->module->items[i], name, head)) {
            return true;
        }
    }
    for (i = 0; i < s->library_count; i++) {
        size_t j;
        for (j = 0; j < s->libraries[i]->item_count; j++) {
            const struct symbol *sym = s->libraries[i]->items[j];
            const struct type *t = sym->type;
            size_t k;
            if (doc_same(&sym->name, name, head)) {
                return true;
            }
            /* A class or a struct of a library, whose fields and whose
               functions a doc comment names as often as the type. */
            if (t == NULL || t->kind != TYPE_STRUCT) {
                continue;
            }
            for (k = 0; k < t->field_count; k++) {
                if (doc_same(&t->fields[k].name, name, head)) {
                    return true;
                }
            }
            for (k = 0; k < t->member_count; k++) {
                if (doc_name_in_item(t->members[k], name, head)) {
                    return true;
                }
            }
        }
    }
    if (s->item != NULL && doc_name_in_item(s->item, name, head)) {
        return true;
    }
    return false;
}

static bool doc_ident_char(char c, bool first)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' ||
           (!first && c >= '0' && c <= '9');
}

/* Whether the n bytes at s spell one name. That is an identifier, or a
   path of identifiers with a dot between them, with an optional `()` at
   the end. Everything else between backticks is code and no name. */
static bool doc_is_name(const char *s, size_t n)
{
    bool first = true;
    size_t i;

    if (n > 2 && s[n - 2] == '(' && s[n - 1] == ')') {
        n -= 2;
    }
    if (n == 0) {
        return false;
    }
    for (i = 0; i < n; i++) {
        if (s[i] == '.') {
            if (first || i + 1 == n) {
                return false;
            }
            first = true;
            continue;
        }
        if (!doc_ident_char(s[i], first)) {
            return false;
        }
        first = false;
    }
    return true;
}

/* The content of one inline code span, which is a name or nothing. */
static void doc_check_span(const struct doc_scope *s,
                           const struct doc_text *doc,
                           const struct name *owner, const char *text,
                           size_t length)
{
    size_t n = length;

    if (!doc_is_name(text, length)) {
        return;
    }
    if (n > 2 && text[n - 2] == '(' && text[n - 1] == ')') {
        n -= 2;
    }
    if (!doc_name_known(s, text, n)) {
        doc_name_warning(s, doc, owner, text, n);
    }
}

/* Whether the delimiter at s opens emphasis and the line holds one that
   closes it. Markdown asks the opener to have text after it and the
   closer to have text before it. That rule keeps `a * b` and a pointer
   type out. */
static bool doc_emphasis(const char *line, size_t length, size_t at)
{
    char d = line[at];
    size_t i;

    if (at + 1 >= length || line[at + 1] == ' ' || line[at + 1] == d) {
        return false;
    }
    if (at > 0 && line[at - 1] != ' ' && line[at - 1] != '(' &&
        line[at - 1] != '"') {
        return false;
    }
    for (i = at + 2; i < length; i++) {
        if (line[i] == '`') {
            return false;
        }
        if (line[i] != d || line[i - 1] == ' ') {
            continue;
        }
        if (i + 1 == length || line[i + 1] == ' ' || line[i + 1] == '.' ||
            line[i + 1] == ',' || line[i + 1] == ')' || line[i + 1] == ':' ||
            line[i + 1] == ';' || line[i + 1] == '"') {
            return true;
        }
    }
    return false;
}

/* Whether the line at s holds an HTML tag: `<b>`, `</b>` or `<br/>`. */
static bool doc_html(const char *line, size_t length, size_t at)
{
    size_t i = at + 1;

    if (i < length && line[i] == '/') {
        i++;
    }
    if (i >= length || !doc_ident_char(line[i], true)) {
        return false;
    }
    while (i < length && doc_ident_char(line[i], false)) {
        i++;
    }
    if (i < length && line[i] == '/') {
        i++;
    }
    return i < length && line[i] == '>';
}

/* One doc comment against the markup subset. Each kind is reported once,
   and the lines of a fenced block are skipped. */
static void doc_check_text(const struct doc_scope *s,
                           const struct doc_text *doc,
                           const struct name *owner)
{
    bool fenced = false;
    bool heading = false;
    bool table = false;
    bool numbered = false;
    bool image = false;
    bool html = false;
    bool emphasis = false;
    size_t start = 0;

    /* An item without a doc comment has no text, and NULL + 0 is
       undefined. */
    if (doc->length == 0) {
        return;
    }
    while (start <= doc->length) {
        const char *line = doc->text + start;
        size_t end = start;
        size_t lead = 0;
        size_t i;
        bool code = false;
        size_t span = 0;
        while (end < doc->length && doc->text[end] != '\n') {
            end++;
        }
        while (lead < end - start &&
               (line[lead] == ' ' || line[lead] == '\t')) {
            lead++;
        }
        if (end - start - lead >= 3 && memcmp(line + lead, "```", 3) == 0) {
            fenced = !fenced;
            start = end + 1;
            continue;
        }
        if (fenced) {
            start = end + 1;
            continue;
        }
        if (lead < end - start) {
            size_t digits = lead;
            while (digits < end - start && line[digits] >= '0' &&
                   line[digits] <= '9') {
                digits++;
            }
            if (!heading && line[lead] == '#') {
                doc_markup_warning(s, doc, owner, "a heading");
                heading = true;
            }
            if (!table && line[lead] == '|') {
                doc_markup_warning(s, doc, owner, "a table");
                table = true;
            }
            if (!numbered && digits > lead && digits < end - start &&
                line[digits] == '.') {
                doc_markup_warning(s, doc, owner, "a numbered list");
                numbered = true;
            }
        }
        for (i = lead; i < end - start; i++) {
            if (line[i] == '`') {
                if (code) {
                    doc_check_span(s, doc, owner, line + span, i - span);
                }
                code = !code;
                span = i + 1;
                continue;
            }
            if (code) {
                continue;
            }
            if (!image && line[i] == '!' && i + 1 < end - start &&
                line[i + 1] == '[') {
                doc_markup_warning(s, doc, owner, "an image");
                image = true;
            }
            if (!html && line[i] == '<' && doc_html(line, end - start, i)) {
                doc_markup_warning(s, doc, owner, "HTML");
                html = true;
            }
            if (!emphasis && (line[i] == '*' || line[i] == '_') &&
                doc_emphasis(line, end - start, i)) {
                doc_markup_warning(s, doc, owner, "emphasis");
                emphasis = true;
            }
        }
        start = end + 1;
    }
}

/* Every doc comment of an item. They are its own two, those of its
   fields, those of the cases of a variant and those of every member of
   its body. */
static void doc_check_item(struct doc_scope *s, const struct item *it)
{
    const struct item *outer = s->item;
    size_t i;

    s->item = it;
    doc_check_text(s, &it->doc, &it->name);
    doc_check_text(s, &it->note, &it->name);
    for (i = 0; i < it->param_count; i++) {
        doc_check_text(s, &it->params[i].doc, &it->params[i].name);
        doc_check_text(s, &it->params[i].note, &it->params[i].name);
    }
    for (i = 0; i < it->case_count; i++) {
        doc_check_text(s, &it->cases[i].doc, &it->cases[i].name);
    }
    for (i = 0; i < it->member_count; i++) {
        doc_check_item(s, it->members[i]);
    }
    s->item = outer;
}

/* DESIGN: --warn-undocumented reports every `pub` item without a `///`
   comment, the item of a class body among them. An item the compiler
   wrote, such as the `get` of a singleton or a function of a `tests`
   block, is no item a reader documents. */
static void doc_check_undocumented(const struct doc_scope *s,
                                   const struct item *it)
{
    size_t i;

    if (it->pub && it->doc.length == 0 && !it->singleton_get &&
        it->block == BLOCK_NONE) {
        diagnostics_doc(s->diags, NAME_UNDOCUMENTED, it->name_pos.line,
                        it->name_pos.column, "the pub item `%.*s` has no `///` comment",
                        (int)it->name.length, it->name.text);
    }
    for (i = 0; i < it->member_count; i++) {
        doc_check_undocumented(s, it->members[i]);
    }
}

/* DESIGN: doc warnings belong to anti check, which passes --doc-warnings.
   Without that option antic says nothing about documentation. */
void sema_doc_warnings(const struct module *module, const char *module_name,
                       const struct interface *const *libraries,
                       size_t library_count, struct types *types,
                       bool undocumented, struct diagnostics *diags)
{
    struct doc_scope scope;
    size_t i;

    memset(&scope, 0, sizeof scope);
    scope.module = module;
    if (module_name != NULL) {
        scope.module_name.text = module_name;
        scope.module_name.length = strlen(module_name);
    }
    scope.libraries = libraries;
    scope.library_count = library_count;
    scope.types = types;
    scope.diags = diags;
    doc_check_text(&scope, &module->doc, NULL);
    doc_check_text(&scope, &module->note, NULL);
    for (i = 0; i < module->item_count; i++) {
        doc_check_item(&scope, module->items[i]);
    }
    for (i = 0; i < module->item_count; i++) {
        const struct item *it = module->items[i];
        if (it->pub && it->note.length > 0 && it->doc.length == 0) {
            diagnostics_doc(diags, NAME_DOC_NOTE_ONLY, it->name_pos.line,
                            it->name_pos.column, "the pub item `%.*s` has a `//#` note and no "
                            "`///` comment", (int)it->name.length,
                            it->name.text);
        }
    }
    if (undocumented) {
        for (i = 0; i < module->item_count; i++) {
            doc_check_undocumented(&scope, module->items[i]);
        }
    }
    for (i = 0; i < module->dropped_count; i++) {
        const struct dropped_doc *d = &module->dropped[i];
        diagnostics_doc(diags, NAME_DOC_DROPPED, d->pos.line, d->pos.column,
                        d->module_form
                            ? "the `%.*s` comment is dropped, because it "
                              "stands after the first import or item"
                            : "the `%.*s` comment is dropped, because no "
                              "item or field follows it",
                        (int)d->marker.length, d->marker.text);
    }
}

void sema_interface(const struct module *module, const char *module_name,
                    struct arena *arena, struct interface *out)
{
    size_t i;

    memset(out, 0, sizeof *out);
    out->module = keep_name(arena, module_name, strlen(module_name));
    out->doc = keep_name(arena, module->doc.length > 0 ? module->doc.text : "",
                         module->doc.length);
    out->package.name = out->module;
    out->package.version = PACKAGE_VERSION_DEFAULT;
    out->package.license = "";
    out->package.license_text = "";
    out->imports = types_alloc_array(arena, module->import_count + 1,
                                     sizeof *out->imports);
    for (i = 0; i < module->import_count; i++) {
        out->imports[i] = keep_name(arena, module->imports[i].module.text,
                                    module->imports[i].module.length);
    }
    out->import_count = module->import_count;
    out->frameworks = types_alloc_array(arena, module->framework_count + 1,
                                        sizeof *out->frameworks);
    for (i = 0; i < module->framework_count; i++) {
        out->frameworks[i] = keep_name(arena, module->frameworks[i].name.text,
                                       module->frameworks[i].name.length);
    }
    out->framework_count = module->framework_count;
    out->linux_libraries = types_alloc_array(
        arena, module->linux_library_count + 1, sizeof *out->linux_libraries);
    for (i = 0; i < module->linux_library_count; i++) {
        out->linux_libraries[i] =
            keep_name(arena, module->linux_libraries[i].name.text,
                      module->linux_libraries[i].name.length);
    }
    out->linux_library_count = module->linux_library_count;
    out->items = types_alloc_array(arena, module->item_count + 1,
                                   sizeof *out->items);
    for (i = 0; i < module->item_count; i++) {
        const struct item *it = module->items[i];
        struct symbol *sym;
        /* DESIGN: an `internal` item goes into the interface marked
           as internal. A module of the same package may then import it,
           and a module of another package may not. */
        if ((!it->pub && it->vis != VIS_INTERNAL) || it->symbol == NULL) {
            continue;
        }
        sym = arena_alloc(arena, sizeof *sym);
        *sym = *it->symbol;
        sym->internal = it->vis == VIS_INTERNAL;
        /* DESIGN: an interface keeps the parameter names of a function,
           which the generated header and anti doc print. */
        if (it->kind == ITEM_FN || it->kind == ITEM_EXTERN_FN) {
            /* A `may fail` function has one parameter more than the
               declaration wrote, the out pointer the ABI names `out`. */
            size_t total = sym->type != NULL &&
                                   sym->type->kind == TYPE_FN &&
                                   sym->type->param_count > it->param_count
                               ? sym->type->param_count
                               : it->param_count;
            struct name *names =
                types_alloc_array(arena, total + 1, sizeof *names);
            size_t j;
            for (j = 0; j < it->param_count && j < total; j++) {
                names[j].text = keep_name(arena, it->params[j].name.text,
                                          it->params[j].name.length);
                names[j].length = it->params[j].name.length;
            }
            for (; j < total; j++) {
                names[j].text = keep_name(arena, "out", 3);
                names[j].length = 3;
            }
            sym->params = names;
        }
        sym->item = NULL;
        sym->home = out;
        out->items[out->item_count++] = sym;
    }
}

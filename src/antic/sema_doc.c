/* The doc warnings of `anti check`: each doc comment against the markup
   subset, the backtick names that resolve nowhere, the comments the
   parser dropped and, with --warn-undocumented, the `pub` items without
   one. They read the checked module and never the state of the checker. */

#include <string.h>

#include "sema.h"

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

    /* The two functions of the object model and the entry function of a
       program, which a doc comment names as language words beside the
       types the compiler declares. */
    static const char *const declared[] = {
        "construct", "deserialize", "main"
    };
    const struct type *object;

    if (lexer_is_keyword(name, head) || types_declares(name, head)) {
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
            /* A struct, a class or a variant of a library, whose fields
               and whose functions a doc comment names as often as the
               type. */
            if (!types_has_fields(t)) {
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

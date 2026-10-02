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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "antic.h"
#include "cpu.h"
#include "driver.h"
#include "files.h"
#include "modpath.h"
#include "repo.h"
#include "target.h"
#include "text.h"
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

static bool is_item(const struct doc_page *page, const char *s, size_t n)
{
    size_t i;

    for (i = 0; page != NULL && i < page->name_count; i++) {
        if (same(s, n, text_cstr(&page->names[i]))) {
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
static bool resolve_name(const struct doc_page *page, const char *s,
                         size_t n, struct text *href)
{
    const char *dot;
    size_t head;
    size_t i;

    if (page == NULL || !is_name(s, n)) {
        return false;
    }
    if (n > 2 && s[n - 2] == '(' && s[n - 1] == ')') {
        n -= 2;
    }
    if (is_item(page, s, n)) {
        text_appendf(href, "#%.*s", (int)n, s);
        return true;
    }
    if (same(s, n, text_cstr(&page->module))) {
        text_appendf(href, "%s%s", text_cstr(&page->module), DOC_HTML_SUFFIX);
        return true;
    }
    for (i = 0; i < page->import_count; i++) {
        if (same(s, n, text_cstr(&page->imports[i]))) {
            text_appendf(href, "%s%s", text_cstr(&page->imports[i]),
                         DOC_HTML_SUFFIX);
            return true;
        }
    }
    dot = last_dot(s, n);
    if (dot == NULL) {
        return false;
    }
    head = (size_t)(dot - s);
    if (same(s, head, text_cstr(&page->module))) {
        text_appendf(href, "#%.*s", (int)(n - head - 1), dot + 1);
        return true;
    }
    /* A member of an item of the documented module has no place of its
       own on the page. The name links to the item that holds it. */
    if (is_item(page, s, head)) {
        text_appendf(href, "#%.*s", (int)head, s);
        return true;
    }
    for (i = 0; i < page->import_count; i++) {
        const char *import = text_cstr(&page->imports[i]);
        if (same(s, head, import) ||
            same(s, head, modpath_last(import))) {
            text_appendf(href, "%s%s#%.*s", text_cstr(&page->imports[i]),
                         DOC_HTML_SUFFIX, (int)(n - head - 1), dot + 1);
            return true;
        }
    }
    return false;
}

/* Whether the scheme before the colon at n of a URL is the word, in
   either case, as RFC 3986 reads a scheme. */
static bool scheme_is(const char *s, size_t n, const char *word)
{
    size_t i;

    if (n != strlen(word)) {
        return false;
    }
    for (i = 0; i < n; i++) {
        char c = s[i] >= 'A' && s[i] <= 'Z' ? (char)(s[i] - 'A' + 'a') : s[i];
        if (c != word[i]) {
            return false;
        }
    }
    return true;
}

/* DESIGN: a page writes a link of a doc comment as a link only when it
   leads to a web page: an `http:` or `https:` URL, a path relative to
   the page, or a `#` anchor on it. The doc text may come from a library
   file of any repository, and a `javascript:` or `data:` URL would run
   in the page of the reader who follows it. A URL with a blank or a
   control byte is refused too, since a browser drops a tab or a line
   end inside a scheme. So is `//host`, which leaves the site of the
   page. Any other link stays text, as written. */
static bool safe_url(const char *s, size_t n)
{
    size_t i;

    if (n == 0 || (n >= 2 && s[0] == '/' && s[1] == '/')) {
        return false;
    }
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c <= ' ' || c == 0x7f) {
            return false;
        }
    }
    for (i = 0; i < n; i++) {
        if (s[i] == '/' || s[i] == '?' || s[i] == '#') {
            return true;
        }
        if (s[i] == ':') {
            return scheme_is(s, i, "http") || scheme_is(s, i, "https");
        }
    }
    return true;
}

/* Inline code in backticks and a link written `[text](url)`. Everything
   else is text, and the forms the subset leaves out stand as they are
   written. */
static void inline_html(struct text *out, const char *s, size_t n,
                        const struct doc_page *page)
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
            if (resolve_name(page, s + i + 1, length, &href)) {
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
            if (end == NULL || !safe_url(s + at, (size_t)(end - (s + at)))) {
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
                     const struct doc_page *page)
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
            inline_html(out, s + lead + 2, end - lead - 2, page);
            item = true;
            at = end + 1;
            continue;
        }
        if (listed && item) {
            text_append(out, " ");
            inline_html(out, s + lead, end - lead, page);
            at = end + 1;
            continue;
        }
        if (!paragraph) {
            text_append(out, "<p>");
            paragraph = true;
        } else {
            text_append(out, " ");
        }
        inline_html(out, s + lead, end - lead, page);
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

static void doc_body(struct text *out, const struct text *doc,
                     const struct doc_page *page, enum doc_form form)
{
    if (doc->data == NULL || doc->length == 0) {
        return;
    }
    if (form == DOC_HTML) {
        doc_html(out, doc->data, doc->length, page);
    } else {
        doc_markdown(out, doc->data, doc->length);
    }
}

/* One list of fields, values, cases or functions. */
static void entry_list(struct text *out, const struct doc_entry *list,
                       size_t count, const char *class_name,
                       const struct doc_page *page, enum doc_form form)
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
            doc_body(out, &list[i].doc, page, form);
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
                    size_t end = line_of(list[i].doc.data, list[i].doc.length,
                                         at);
                    text_appendf(out, "  %.*s\n", (int)(end - at),
                                 list[i].doc.data + at);
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
static void copy_link(struct text *out, const struct doc_entry *e,
                      const struct doc_page *page, enum doc_form form)
{
    struct text href = {0};

    if (e->copy_of.length == 0) {
        return;
    }
    if (page == NULL || strcmp(text_cstr(&e->copy_of_module),
                               text_cstr(&page->module)) != 0) {
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

static void entry_page(struct text *out, const struct doc_entry *e,
                       const struct doc_page *page, enum doc_form form)
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
    copy_link(out, e, page, form);
    doc_body(out, &e->doc, page, form);
    entry_list(out, e->fields, e->field_count, DOC_CLASS_FIELDS, page, form);
    entry_list(out, e->members, e->member_count, DOC_CLASS_MEMBERS, page,
               form);
    if (e->note.length > 0) {
        if (form == DOC_HTML) {
            text_append(out,
                        "<section class=\"" DOC_CLASS_INTERNALS "\">\n"
                        "<h3>Internals</h3>\n");
            doc_body(out, &e->note, page, form);
            text_append(out, "</section>\n");
        } else {
            text_append(out, "### Internals\n\n");
            doc_body(out, &e->note, page, form);
        }
    }
    if (form == DOC_HTML) {
        text_append(out, "</section>\n");
    }
}

static void page_render(struct text *out, const struct doc_page *p,
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
    doc_body(out, &p->doc, p, form);
    if (p->note.length > 0) {
        if (form == DOC_HTML) {
            text_append(out,
                        "<section class=\"" DOC_CLASS_INTERNALS "\">\n"
                        "<h2>Internals</h2>\n");
            doc_body(out, &p->note, p, form);
            text_append(out, "</section>\n");
        } else {
            text_append(out, "## Internals\n\n");
            doc_body(out, &p->note, p, form);
        }
    }
    for (i = 0; i < p->item_count; i++) {
        entry_page(out, &p->items[i], p, form);
    }
    if (form == DOC_HTML) {
        text_append(out, "</main>\n");
    }
}

/* The first paragraph of a doc text, which the index prints beside the
   name of the module. */
static void first_paragraph(struct text *out, const struct text *doc,
                            const struct doc_page *page, enum doc_form form)
{
    size_t at = 0;

    while (at < doc->length) {
        size_t end = line_of(doc->data, doc->length, at);
        if (lead_of(doc->data, at, end) == end) {
            break;
        }
        if (form == DOC_HTML) {
            if (at > 0) {
                text_append(out, " ");
            }
            inline_html(out, doc->data + at, end - at, page);
        } else {
            text_appendf(out, "%s%.*s", at > 0 ? " " : "", (int)(end - at),
                         doc->data + at);
        }
        at = end + 1;
    }
}

static void index_render(struct text *out, const struct doc_page *pages,
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
                first_paragraph(out, &pages[i].doc, &pages[i], form);
                text_append(out, "</p>");
            }
            text_append(out, "</li>\n");
        } else {
            text_appendf(out, "- [%s](%s%s)", name, name,
                         DOC_MARKDOWN_SUFFIX);
            if (pages[i].doc.length > 0) {
                text_append(out, "\n  ");
                first_paragraph(out, &pages[i].doc, &pages[i], form);
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
                             const struct options *base, const char *work,
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
        if (!unit_read(sources[i], base->roots, base->root_count, work,
                       &units[i])) {
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
        struct options o = *base;
        if (u->source == NULL || !u->parsed) {
            continue;
        }
        o.input = u->source;
        o.library = true;
        o.output = text_cstr(&u->library);
        ok = driver_run(&o) == 0;
    }
    return ok;
}

int doc_run(const char *const *sources, size_t count,
            const char *const *roots, size_t root_count, const char *out,
            const char *work, const char *package, const char *runtime,
            enum doc_form form, bool dev, bool private_items)
{
    struct options base;
    enum target target;
    enum cpu_level cpu;
    struct doc_page *pages = NULL;
    struct unit *units = NULL;
    const char **search = NULL;
    size_t *order = NULL;
    struct text body = {0};
    struct text path = {0};
    size_t search_count = 0;
    size_t i;
    int status = 0;

    if (count == 0) {
        fputs("anti: doc takes a source or a library file\n", stderr);
        return 2;
    }
    pages = files_array(count, sizeof *pages);
    units = files_array(count, sizeof *units);
    order = files_array(count, sizeof *order);
    search = files_array(root_count + 2, sizeof *search);
    for (i = 0; i < root_count; i++) {
        search[search_count++] = roots[i];
    }
    search[search_count++] = work;
    if (!unit_host(&target, &cpu)) {
        status = 2;
        goto done;
    }
    unit_options(&base, package, runtime, search, search_count, target, cpu);
    base.front_end = true;
    if (!files_make_dirs(out) || !files_make_dirs(work) ||
        !write_interfaces(sources, count, &base, work, units, order)) {
        status = 1;
        goto done;
    }
    for (i = 0; i < count; i++) {
        struct options o = base;
        enum doc_items items = dev             ? DOC_ITEMS_DEV
                               : private_items ? DOC_ITEMS_PRIVATE
                                               : DOC_ITEMS_PUBLIC;
        o.input = sources[i];
        if ((dev || private_items) && is_library(sources[i])) {
            fprintf(stderr, "anti: %s: %s needs the source\n", sources[i],
                    dev ? "--dev" : "--private");
            status = 1;
            goto done;
        }
        if (!antic_doc_page(&o, items, &pages[i])) {
            status = 1;
            goto done;
        }
        /* M14: the module path of a library file names the file of its
           page, and a library file may come from a repository. */
        if (!repo_name_valid(text_cstr(&pages[i].module))) {
            fprintf(stderr, "anti: %s names the module %s, which is no module "
                            "path\n", sources[i], text_cstr(&pages[i].module));
            status = 1;
            goto done;
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
        doc_page_free(&pages[i]);
        if (units[i].source != NULL) {
            unit_free(&units[i]);
        }
    }
    free(pages);
    free(units);
    free(order);
    free((void *)search);
    text_free(&body);
    text_free(&path);
    return status;
}

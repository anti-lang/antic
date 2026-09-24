#include "warnings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"

struct name_entry {
    const char *name;
    enum name_kind kind;
};

/* The names, in the order of enum diag_name. docs/notes/warnings.md
   lists each with its meaning and its fix. */
static const struct name_entry NAMES[NAME_COUNT] = {
    [NAME_NONE] = {"", KIND_WARNING},
    [NAME_SHADOWED_CATCH] = {"shadowed-catch", KIND_WARNING},
    [NAME_NEVER_FAILS] = {"never-fails", KIND_WARNING},
    [NAME_ASSERT_CALL] = {"assert-call", KIND_WARNING},
    [NAME_UNFILLED_ABSTRACT] = {"unfilled-abstract", KIND_WARNING},
    [NAME_ABOVE_VECTOR_CAP] = {"above-vector-cap", KIND_WARNING},
    [NAME_SINGLE_SEGMENT_PATH] = {"single-segment-path", KIND_WARNING},
    [NAME_DOC_MARKUP] = {"doc-markup", KIND_WARNING},
    [NAME_DOC_UNRESOLVED] = {"doc-unresolved", KIND_WARNING},
    [NAME_DOC_NOTE_ONLY] = {"doc-note-only", KIND_WARNING},
    [NAME_UNDOCUMENTED] = {"undocumented", KIND_WARNING},
    [NAME_DOC_DROPPED] = {"doc-dropped", KIND_WARNING},
    [NAME_UNUSED_ALLOW] = {"unused-allow", KIND_WARNING},
    [NAME_UNUSED_UNCHECKED] = {"unused-unchecked", KIND_WARNING},
    [NAME_UNGUARDED_FIELD] = {"unguarded-field", KIND_SAFETY_CHECK},
    [NAME_EXPONENTIAL_PATTERN] = {"exponential-pattern", KIND_SAFETY_CHECK},
};

const char *warnings_name(enum diag_name name)
{
    return NAMES[name].name;
}

enum name_kind warnings_kind(enum diag_name name)
{
    return NAMES[name].kind;
}

enum diag_name warnings_find(const char *text, size_t length)
{
    int i;

    for (i = NAME_NONE + 1; i < NAME_COUNT; i++) {
        if (strlen(NAMES[i].name) == length &&
            memcmp(NAMES[i].name, text, length) == 0) {
            return (enum diag_name)i;
        }
    }
    return NAME_NONE;
}

static int pos_compare(int line, int column, struct pos p)
{
    if (line != p.line) {
        return line < p.line ? -1 : 1;
    }
    return column < p.column ? -1 : column > p.column;
}

static bool covers(const struct clause *c, const struct diagnostic *d)
{
    return pos_compare(d->line, d->column, c->from) >= 0 &&
           pos_compare(d->line, d->column, c->to) <= 0;
}

/* Whether a clause the diagnostic's kind takes covers it. Every clause
   that does counts as used, so two nested ones of one name are both
   in use. */
static bool silenced(const struct module *module, const struct diagnostic *d,
                     bool *used)
{
    bool found = false;
    size_t i;

    for (i = 0; i < module->clause_count; i++) {
        const struct clause *c = &module->clauses[i];
        if (c->unchecked !=
                (warnings_kind(d->name) == KIND_SAFETY_CHECK) ||
            warnings_find(c->name.text, c->name.length) != d->name ||
            !covers(c, d)) {
            continue;
        }
        used[i] = true;
        found = true;
    }
    return found;
}

/* Drop every diagnostic a clause covers, keeping the order of the
   rest. */
static void drop_silenced(const struct module *module,
                          struct diagnostics *diags, size_t from, bool *used)
{
    size_t kept = from;
    size_t i;

    for (i = from; i < diags->count; i++) {
        const struct diagnostic *d = &diags->items[i];
        if (d->name != NAME_NONE && (d->warning || d->promoted ||
                                     warnings_kind(d->name) ==
                                         KIND_SAFETY_CHECK) &&
            silenced(module, d, used)) {
            continue;
        }
        diags->items[kept++] = *d;
    }
    diags->count = kept;
}

static bool names_unused(const struct clause *c)
{
    enum diag_name name = warnings_find(c->name.text, c->name.length);

    return name == NAME_UNUSED_ALLOW || name == NAME_UNUSED_UNCHECKED;
}

/* The warning of each clause that silenced nothing. The ones that name
   `unused-allow` or `unused-unchecked` wait for the second round, since
   the first round writes what they silence. */
static void report_unused(const struct module *module,
                          struct diagnostics *diags, const bool *used,
                          warnings_ran ran, bool second)
{
    size_t i;

    for (i = 0; i < module->clause_count; i++) {
        const struct clause *c = &module->clauses[i];
        enum diag_name name = warnings_find(c->name.text, c->name.length);
        if (used[i] || names_unused(c) != second ||
            (ran & WARNINGS_BIT(name)) == 0) {
            continue;
        }
        if (c->unchecked) {
            diagnostics_warn(diags, NAME_UNUSED_UNCHECKED, c->pos.line,
                             c->pos.column,
                             "`unchecked(%.*s)` overrules nothing",
                             (int)c->name.length, c->name.text);
        } else {
            diagnostics_warn(diags, NAME_UNUSED_ALLOW, c->pos.line,
                             c->pos.column,
                             "`allow(%.*s)` silences nothing",
                             (int)c->name.length, c->name.text);
        }
    }
}

void warnings_apply(const struct module *module, struct diagnostics *diags,
                    warnings_ran ran, bool complete)
{
    bool *used;
    size_t first;

    if (module == NULL || module->clause_count == 0) {
        return;
    }
    used = calloc(module->clause_count, sizeof *used);
    if (used == NULL) {
        fputs("antic: out of memory\n", stderr);
        exit(70);
    }
    drop_silenced(module, diags, 0, used);
    if (complete) {
        first = diags->count;
        report_unused(module, diags, used, ran, false);
        drop_silenced(module, diags, first, used);
        report_unused(module, diags, used, ran, true);
    }
    free(used);
}

bool warnings_promote(struct diagnostics *diags)
{
    bool any = false;
    size_t i;

    for (i = 0; i < diags->count; i++) {
        struct diagnostic *d = &diags->items[i];
        if (d->warning && !d->doc) {
            d->warning = false;
            d->promoted = true;
            any = true;
        }
    }
    return any;
}

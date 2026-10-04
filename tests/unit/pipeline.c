#include "../binary_stdio.h"
#include "pipeline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "antic.h"
#include "check.h"
#include "lower.h"
#include "optimize.h"
#include "parser.h"
#include "sema.h"

/* The first message of d, or an empty text when a stage failed without
   one. */
static const char *first_message(const struct diagnostics *d)
{
    return d->count > 0 ? d->items[0].message : "";
}

void parsed_run(struct parsed *p, const char *source)
{
    memset(p, 0, sizeof *p);
    CHECK(lexer_lex(source, strlen(source), &p->arena, &p->diags,
                    &p->tokens));
    p->ok = parser_parse(source, &p->tokens, &p->arena, &p->diags,
                         &p->module);
}

void parsed_release(struct parsed *p)
{
    lexer_token_list_free(&p->tokens);
    diagnostics_free(&p->diags);
    arena_free(&p->arena);
}

void parses_to(const char *source, const char *expected)
{
    struct parsed p;
    struct text out = {0};
    size_t i;

    parsed_run(&p, source);
    CHECK(p.ok);
    for (i = 0; i < p.diags.count; i++) {
        check_failures++;
        fprintf(stderr, "unexpected: %d:%d: %s\n", p.diags.items[i].line,
                p.diags.items[i].column, p.diags.items[i].message);
    }
    if (p.module != NULL) {
        ast_dump(&out, p.module);
        CHECK_STR(text_cstr(&out), expected);
    }
    text_free(&out);
    parsed_release(&p);
}

void checked_run(struct checked *c, const char *source)
{
    checked_run_as(c, "main", source);
}

void checked_run_as(struct checked *c, const char *name, const char *source)
{
    memset(c, 0, sizeof *c);
    types_init(&c->types, &c->arena);
    c->parsed = lexer_lex(source, strlen(source), &c->arena, &c->diags,
                          &c->tokens) &&
                parser_parse(source, &c->tokens, &c->arena, &c->diags,
                             &c->module);
    if (!c->parsed) {
        fprintf(stderr, "syntax error in test source: %s\n%s\n",
                first_message(&c->diags), source);
        check_failures++;
        return;
    }
    c->ok = sema_check(c->module, name, NULL, NULL, 0, &c->types, &c->arena,
                       &c->diags, true);
}

void checked_release(struct checked *c)
{
    lexer_token_list_free(&c->tokens);
    diagnostics_free(&c->diags);
    arena_free(&c->arena);
}

void print_diagnostics(const struct diagnostics *d, const char *source)
{
    size_t i;

    for (i = 0; i < d->count; i++) {
        fprintf(stderr, "  %d:%d: %s\n", d->items[i].line, d->items[i].column,
                d->items[i].message);
    }
    fprintf(stderr, "%s\n", source);
}

void expect_accepted(bool ok, const struct diagnostics *d, const char *source)
{
    if (!ok) {
        check_failures++;
        fprintf(stderr, "rejected:\n");
        print_diagnostics(d, source);
    }
}

void expect_refused(bool ok, const struct diagnostics *d, const char *source,
                    int line, int column, const char *message)
{
    if (ok || d->count == 0) {
        check_failures++;
        fprintf(stderr, "accepted, expected %d:%d: %s\n%s\n", line, column,
                message, source);
    } else if (d->items[0].line != line || d->items[0].column != column ||
               strcmp(d->items[0].message, message) != 0) {
        check_failures++;
        fprintf(stderr, "expected %d:%d: %s\ngot\n", line, column, message);
        print_diagnostics(d, source);
    }
}

void accepts(const char *source)
{
    struct checked c;

    checked_run(&c, source);
    if (c.parsed) {
        expect_accepted(c.ok, &c.diags, source);
    }
    checked_release(&c);
}

void rejects(const char *source, int line, int column, const char *message)
{
    struct checked c;

    checked_run(&c, source);
    if (c.parsed) {
        expect_refused(c.ok, &c.diags, source, line, column, message);
    }
    checked_release(&c);
}

void rejects_with(const char *source, const char *message)
{
    struct checked c;

    checked_run(&c, source);
    if (!c.parsed) {
        /* checked_run counted the failure. */
    } else if (c.ok || c.diags.count == 0) {
        check_failures++;
        fprintf(stderr, "accepted, expected: %s\n%s\n", message, source);
    } else if (strcmp(c.diags.items[0].message, message) != 0) {
        check_failures++;
        fprintf(stderr, "expected %s\ngot      %s\n%s\n", message,
                c.diags.items[0].message, source);
    }
    checked_release(&c);
}

void lowered_run(struct lowered *l, const char *source)
{
    checked_run(&l->front, source);
    ir_module_init(&l->ir, &l->front.arena, "main");
    l->ok = l->front.ok;
    if (l->front.parsed && !l->front.ok) {
        check_failures++;
        fprintf(stderr, "test source does not check: %s\n%s\n",
                first_message(&l->front.diags), source);
    }
    if (l->ok) {
        lower_module(l->front.module, "main", &l->ir, 0, NULL, 0,
                     PACKAGE_VERSION_DEFAULT);
    }
}

void lowered_release(struct lowered *l)
{
    ir_module_free(&l->ir);
    checked_release(&l->front);
}

void ir_print_own(struct text *out, const struct ir_module *m)
{
    static const char runtime_type[] = "type anti.rt.";
    struct text all = {0};
    const char *line;

    ir_print(&all, m);
    line = text_cstr(&all);
    while (*line != '\0') {
        const char *end = strchr(line, '\n');
        size_t length = end == NULL ? strlen(line) : (size_t)(end - line) + 1;

        if (strncmp(line, runtime_type, sizeof runtime_type - 1) != 0) {
            text_append_bytes(out, line, length);
        }
        line += length;
    }
    text_free(&all);
}

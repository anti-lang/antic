#include "pipeline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "antic.h"
#include "check.h"
#include "lower.h"
#include "optimize.h"
#include "parser.h"
#include "regalloc.h"
#include "select.h"
#include "sema.h"
#include "target_desc.h"

/* The first message of d, or an empty text when a stage failed without
   one. */
static const char *first_message(const struct diagnostics *d)
{
    return d->count > 0 ? d->items[0].message : "";
}

void checked_run(struct checked *c, const char *source)
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
    c->ok = sema_check(c->module, "main", NULL, NULL, 0, &c->types, &c->arena,
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

void machine_build(struct machine *m, struct ir_module *ir, enum target t,
                   enum cpu_level level, bool allocate)
{
    size_t i;

    memset(m, 0, sizeof *m);
    m->count = ir->function_count;
    m->functions = alloc_zeroed(alloc_sum(m->count, 1), sizeof *m->functions);
    m->ok = select_module(t, level, ir, m->functions, m->error,
                          sizeof m->error);
    for (i = 0; allocate && m->ok && i < m->count; i++) {
        if (m->functions[i] != NULL) {
            m->ok = regalloc_function(t, m->functions[i], m->error,
                                      sizeof m->error);
        }
    }
}

void machine_release(struct machine *m)
{
    size_t i;

    for (i = 0; i < m->count; i++) {
        if (m->functions[i] != NULL) {
            mach_function_free(m->functions[i]);
            free(m->functions[i]);
        }
    }
    free(m->functions);
}

void machine_text(struct ir_module *ir, enum target t, enum cpu_level level,
                  bool allocate, struct text *out)
{
    struct machine m;
    size_t i;

    machine_build(&m, ir, t, level, allocate);
    for (i = 0; m.ok && i < m.count; i++) {
        if (m.functions[i] != NULL) {
            mach_print(out, target_desc(t), level, ir, m.functions[i]);
        }
    }
    if (!m.ok) {
        text_append(out, m.error);
    }
    machine_release(&m);
}

void machine_of(const char *source, enum target t, enum cpu_level level,
                bool allocate, struct text *out)
{
    struct lowered l;

    lowered_run(&l, source);
    if (l.ok) {
        optimize_program(&l.ir, "main");
        machine_text(&l.ir, t, level, allocate, out);
    }
    lowered_release(&l);
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sema_checker.h"

/* DESIGN: a declaration that names another resolves that one first, and
   the checker recursed once per link of a chain. Constants, `type` lines
   and `constraint` sets are such chains. A chain deeper than
   CHAIN_DIRECT links is taken apart first. A walk with a stack of its
   own finds the declarations the first one depends on. Each is then
   resolved after the ones it names, so no resolution goes more than one
   link deep. A shallower chain is resolved as it is met, which keeps the
   order of the messages. Each kind bounds what is left with a depth of
   its own: a cycle through a long chain, or a form the walk does not
   follow. */
#define CHAIN_DIRECT 16

void sema_chain_add(struct chain_deps *d, struct symbol *sym)
{
    if (sym == NULL || sym->state != EVAL_NONE) {
        return;
    }
    if (d->count == d->capacity) {
        size_t capacity = d->capacity == 0 ? 64 : d->capacity * 2;
        struct symbol **items =
            capacity <= SIZE_MAX / sizeof *items
                ? realloc(d->items, capacity * sizeof *items)
                : NULL;
        if (items == NULL) {
            fputs("antic: out of memory\n", stderr);
            exit(70);
        }
        d->items = items;
        d->capacity = capacity;
    }
    d->items[d->count++] = sym;
}

/* One declaration on the stack of sema_chain_prepare, and its range of
   deps. */
struct chain_frame {
    struct symbol *sym;
    size_t start;
    size_t next;
    size_t end;
};

/* Grow the array items of capacity to hold one more of size bytes. */
static void *grow(void *items, size_t *capacity, size_t count, size_t size)
{
    size_t want;
    void *p;

    if (count < *capacity) {
        return items;
    }
    want = *capacity == 0 ? 64 : *capacity * 2;
    p = want <= SIZE_MAX / size ? realloc(items, want * size) : NULL;
    if (p == NULL) {
        fputs("antic: out of memory\n", stderr);
        exit(70);
    }
    *capacity = want;
    return p;
}

void sema_chain_prepare(struct checker *c, struct symbol *root,
                        sema_chain_deps_fn *deps_of,
                        sema_chain_resolve_fn *resolve)
{
    struct chain_deps deps;
    struct ptr_set seen;
    struct chain_frame *stack = NULL;
    size_t stack_count = 0;
    size_t stack_capacity = 0;
    size_t deepest = 0;
    struct symbol **order = NULL;
    size_t order_count = 0;
    size_t order_capacity = 0;
    struct symbol *next = root;
    size_t i;

    memset(&deps, 0, sizeof deps);
    memset(&seen, 0, sizeof seen);
    deps.c = c;
    sema_ptr_set_add(&seen, root);
    for (;;) {
        struct chain_frame *top;
        if (next != NULL) {
            stack = grow(stack, &stack_capacity, stack_count, sizeof *stack);
            top = &stack[stack_count++];
            top->sym = next;
            top->start = deps.count;
            deps_of(&deps, next);
            top->next = top->start;
            top->end = deps.count;
            deepest = stack_count > deepest ? stack_count : deepest;
            next = NULL;
            continue;
        }
        if (stack_count == 0) {
            break;
        }
        top = &stack[stack_count - 1];
        if (top->next < top->end) {
            struct symbol *dep = deps.items[top->next++];
            if (dep->state == EVAL_NONE && sema_ptr_set_add(&seen, dep)) {
                next = dep;
            }
            continue;
        }
        order = grow(order, &order_capacity, order_count, sizeof *order);
        order[order_count++] = top->sym;
        deps.count = top->start;
        stack_count--;
    }
    /* The last in the order is root, which the caller resolves. */
    if (deepest > CHAIN_DIRECT) {
        for (i = 0; i + 1 < order_count; i++) {
            if (order[i]->state == EVAL_NONE) {
                resolve(c, order[i]);
            }
        }
    }
    free(order);
    free(stack);
    free(deps.items);
    free(seen.slots);
}

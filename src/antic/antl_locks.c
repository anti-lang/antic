/* The section of the lock orders of a library file, the last of the
   file: what the safety check `lock-order` keeps of the module for the
   modules that import it. */

#include "antl_io.h"

/* DESIGN: the section holds the functions of the module that take a
   lock, each by the name of its symbol with the locks it takes, and the
   edges of the graph of lock orders that the code of the module adds,
   each with its line and column. A lock is three strings: the module and
   the name of its struct or class and the name of its field. The section
   names no type of the type table, so a lock of a struct that the module
   keeps private adds nothing to the interface. A module without locks
   writes two counts of 0. */

static void put_lock(struct writer *w, const struct lock_name *lock)
{
    antl_put_bytes(w, lock->module.text, lock->module.length);
    antl_put_bytes(w, lock->type.text, lock->type.length);
    antl_put_bytes(w, lock->field.text, lock->field.length);
}

void antl_put_locks(struct writer *w, const struct lock_facts *facts)
{
    size_t i;
    size_t j;

    if (facts == NULL) {
        antl_put_count(w, 0);
        antl_put_count(w, 0);
        return;
    }
    antl_put_count(w, facts->function_count);
    for (i = 0; i < facts->function_count; i++) {
        const struct lock_function *f = &facts->functions[i];
        antl_put_bytes(w, f->symbol.text, f->symbol.length);
        antl_put_count(w, f->count);
        for (j = 0; j < f->count; j++) {
            put_lock(w, &f->locks[j]);
        }
    }
    antl_put_count(w, facts->edge_count);
    for (i = 0; i < facts->edge_count; i++) {
        const struct lock_edge *e = &facts->edges[i];
        put_lock(w, &e->held);
        put_lock(w, &e->taken);
        antl_put_u32(w, (uint32_t)e->line);
        antl_put_u32(w, (uint32_t)e->column);
    }
}

/* The three strings of a lock take 12 bytes at least. */
#define LOCK_BYTES 12

static void get_lock(struct reader *r, struct lock_name *lock)
{
    lock->module = antl_get_name(r);
    lock->type = antl_get_name(r);
    lock->field = antl_get_name(r);
}

void antl_read_locks(struct reader *r)
{
    struct lock_facts *facts = antl_allocate(r, 1, sizeof *facts);
    uint32_t count;
    uint32_t i;
    uint32_t j;

    count = antl_get_count(r, 8);
    facts->functions = antl_allocate(r, count, sizeof *facts->functions);
    for (i = 0; i < count && !r->failed; i++) {
        struct lock_function *f = &facts->functions[i];
        uint32_t locks;
        f->symbol = antl_get_name(r);
        locks = antl_get_count(r, LOCK_BYTES);
        f->locks = antl_allocate(r, locks, sizeof *f->locks);
        for (j = 0; j < locks && !r->failed; j++) {
            get_lock(r, &f->locks[j]);
        }
        f->count = locks;
    }
    facts->function_count = r->failed ? 0 : count;
    count = r->failed ? 0 : antl_get_count(r, 2 * LOCK_BYTES + 8);
    facts->edges = antl_allocate(r, count, sizeof *facts->edges);
    for (i = 0; i < count && !r->failed; i++) {
        struct lock_edge *e = &facts->edges[i];
        get_lock(r, &e->held);
        get_lock(r, &e->taken);
        e->line = antl_get_int(r);
        e->column = antl_get_int(r);
    }
    facts->edge_count = r->failed ? 0 : count;
    if (!r->failed &&
        (facts->function_count > 0 || facts->edge_count > 0)) {
        r->iface->locks = facts;
    }
}

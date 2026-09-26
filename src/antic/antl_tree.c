#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "antl_io.h"

/* DESIGN: the section of the generics. A library file stores each
   generic body as its checked syntax tree, with its names resolved, its
   types checked and its type parameters open. A module that uses the
   generic with concrete arguments lowers a copy of that tree, as it does
   for a generic of its own source. The IR gives every value a concrete
   type, so no single IR stands for an open body.

   The section follows the items. It holds a declaration per generic of
   the module, private ones among them. The symbols of other items that
   the bodies name follow, then one tree per body. A tree is written as
   tables of its local symbols, its anonymous functions, its blocks, its
   statements, its expressions and its written types. Each record names
   the others by index. A node that two places share stays shared. The
   bytes follow the source alone, so every host writes the same file.

   One walker per kind of record serves three passes. The first collects
   the records of a tree and gives every type it names an index in the
   type table, before that table is written. The second writes the
   records and the third reads them, so the reader takes every field the
   writer puts, in the same order. */

/* The file stores these enums as bytes. */
_Static_assert(EXPR_PATTERN == 37, "raise ANTL_VERSION, then update this");
_Static_assert(STMT_SELECT == 21, "raise ANTL_VERSION, then update this");
_Static_assert(ITEM_TYPE == 9, "raise ANTL_VERSION, then update this");
_Static_assert(TYPEX_OPTIONAL == 9, "raise ANTL_VERSION, then update this");
_Static_assert(HANDLE_ENCLOSING == 4, "raise ANTL_VERSION, then update this");
_Static_assert(SIMD_OP_ALL == 10, "raise ANTL_VERSION, then update this");
_Static_assert(SYNC_CHAN_DELETE == 6, "raise ANTL_VERSION, then update this");
_Static_assert(ATOMIC_CAS == 7, "raise ANTL_VERSION, then update this");
_Static_assert(VIS_PUB == 3, "raise ANTL_VERSION, then update this");
_Static_assert(FN_CONCRETE == 2, "raise ANTL_VERSION, then update this");
_Static_assert(EVAL_DONE == 2, "raise ANTL_VERSION, then update this");
_Static_assert(CONST_SYMBOLIC == 8, "raise ANTL_VERSION, then update this");

/* A reference to a symbol: 0 for none, a local by its index plus 1, and
   a symbol of the extern table with this bit set. */
#define SYM_EXTERN 0x80000000u

/* A map of pointers to indices, open addressed and at most half full. */
struct index_map {
    const void **keys;
    uint32_t *values;
    size_t capacity;
    size_t count;
};

static size_t ptr_hash(const void *p, size_t capacity)
{
    uintptr_t v = (uintptr_t)p;

    v ^= v >> 17;
    v *= (uintptr_t)0x9e3779b97f4a7c15ull;
    v ^= v >> 29;
    return (size_t)v & (capacity - 1);
}

static bool map_find(const struct index_map *m, const void *key,
                     uint32_t *value)
{
    size_t i;

    if (m->capacity == 0) {
        return false;
    }
    for (i = ptr_hash(key, m->capacity); m->keys[i] != NULL;
         i = (i + 1) & (m->capacity - 1)) {
        if (m->keys[i] == key) {
            *value = m->values[i];
            return true;
        }
    }
    return false;
}

static void out_of_memory(void)
{
    fputs("antic: out of memory\n", stderr);
    exit(70);
}

static void map_add(struct index_map *m, const void *key, uint32_t value)
{
    size_t i;

    if ((m->count + 1) * 2 > m->capacity) {
        struct index_map grown;
        size_t k;
        grown.capacity = m->capacity == 0 ? 64 : m->capacity * 2;
        grown.count = 0;
        grown.keys = calloc(grown.capacity, sizeof *grown.keys);
        grown.values = calloc(grown.capacity, sizeof *grown.values);
        if (grown.keys == NULL || grown.values == NULL) {
            out_of_memory();
        }
        for (k = 0; k < m->capacity; k++) {
            if (m->keys[k] != NULL) {
                map_add(&grown, m->keys[k], m->values[k]);
            }
        }
        free(m->keys);
        free(m->values);
        *m = grown;
    }
    for (i = ptr_hash(key, m->capacity); m->keys[i] != NULL;
         i = (i + 1) & (m->capacity - 1)) {
    }
    m->keys[i] = key;
    m->values[i] = value;
    m->count++;
}

static void map_free(struct index_map *m)
{
    free(m->keys);
    free(m->values);
    memset(m, 0, sizeof *m);
}

/* A table of one kind of record of a tree. */
struct table {
    void **items;
    size_t count;
    size_t capacity;
    struct index_map index;
};

static uint32_t table_add(struct table *t, void *p)
{
    if (t->count == t->capacity) {
        size_t capacity = t->capacity == 0 ? 16 : t->capacity * 2;
        void **items = realloc(t->items, capacity * sizeof *items);
        if (items == NULL) {
            out_of_memory();
        }
        t->items = items;
        t->capacity = capacity;
    }
    t->items[t->count] = p;
    map_add(&t->index, p, (uint32_t)t->count);
    return (uint32_t)t->count++;
}

static void table_free(struct table *t)
{
    free(t->items);
    map_free(&t->index);
    memset(t, 0, sizeof *t);
}

enum { T_SYM, T_ITEM, T_BLOCK, T_STMT, T_EXPR, T_TYPEX, T_COUNT };

/* The body of one function: the function, its owner in the section and
   the tables of its tree. Item 0 is the function itself. */
struct tree {
    struct item *fn;
    uint8_t owner_kind;             /* 0 a generic function, 1 a member */
    uint32_t owner;                 /* the index of the generic */
    uint32_t member;                /* the index among the functions */
    struct table tables[T_COUNT];
};

struct trees {
    struct tree *items;
    size_t count;
    size_t capacity;
    struct index_map externs;
};

enum io_mode { IO_COLLECT, IO_WRITE, IO_READ };

struct io {
    enum io_mode mode;
    struct writer *w;
    struct reader *r;
    struct tree *t;
    struct trees *all;
    /* The reader's extern table. */
    struct symbol **externs;
    uint32_t extern_count;
};

static bool reading(const struct io *io)
{
    return io->mode == IO_READ;
}

static bool failed(const struct io *io)
{
    return io->mode == IO_READ ? io->r->failed : false;
}

static void bad(struct io *io)
{
    if (io->mode == IO_READ) {
        antl_damaged(io->r);
    } else {
        io->w->failed = true;
    }
}

/* Scalars */

static void io_u8(struct io *io, uint8_t *v)
{
    if (io->mode == IO_WRITE) {
        antl_put_u8(io->w, *v);
    } else if (io->mode == IO_READ) {
        *v = antl_get_u8(io->r);
    }
}

static void io_bool(struct io *io, bool *v)
{
    uint8_t b = *v ? 1 : 0;

    io_u8(io, &b);
    if (reading(io)) {
        if (b > 1) {
            bad(io);
        }
        *v = b == 1;
    }
}

static void io_u32(struct io *io, uint32_t *v)
{
    if (io->mode == IO_WRITE) {
        antl_put_u32(io->w, *v);
    } else if (io->mode == IO_READ) {
        *v = antl_get_u32(io->r);
    }
}

static void io_int(struct io *io, int *v)
{
    uint32_t u = (uint32_t)*v;

    io_u32(io, &u);
    if (reading(io)) {
        *v = (int)u;
    }
}

static void io_i32(struct io *io, int32_t *v)
{
    uint32_t u = (uint32_t)*v;

    io_u32(io, &u);
    if (reading(io)) {
        *v = (int32_t)u;
    }
}

static void io_u64(struct io *io, uint64_t *v)
{
    if (io->mode == IO_WRITE) {
        antl_put_u64(io->w, *v);
    } else if (io->mode == IO_READ) {
        *v = antl_get_u64(io->r);
    }
}

static void io_i64(struct io *io, int64_t *v)
{
    uint64_t u = (uint64_t)*v;

    io_u64(io, &u);
    if (reading(io)) {
        *v = (int64_t)u;
    }
}

static void io_size(struct io *io, size_t *v)
{
    uint32_t u = (uint32_t)*v;

    if (io->mode == IO_WRITE && *v > UINT32_MAX) {
        bad(io);
    }
    io_u32(io, &u);
    if (reading(io)) {
        *v = u;
    }
}

static void io_char(struct io *io, char *v)
{
    uint8_t b = (uint8_t)*v;

    io_u8(io, &b);
    if (reading(io)) {
        *v = (char)b;
    }
}

/* An enumeration, one byte, no larger than max. */
#define IO_ENUM(io, place, max)                                           \
    do {                                                                  \
        uint8_t enum_byte_ = (uint8_t)(place);                            \
        io_u8((io), &enum_byte_);                                         \
        if (reading(io)) {                                                \
            if (enum_byte_ > (max)) {                                     \
                bad(io);                                                  \
            }                                                             \
            (place) = enum_byte_;                                         \
        }                                                                 \
    } while (0)

/* A token kind, as the operator of a node. */
static void io_token(struct io *io, enum token_kind *v)
{
    IO_ENUM(io, *v, TOKEN_KIND_COUNT - 1);
}

static void io_pos(struct io *io, struct pos *p)
{
    io_int(io, &p->line);
    io_int(io, &p->column);
}

static void io_bytes(struct io *io, const char **text, size_t *length)
{
    if (io->mode == IO_WRITE) {
        antl_put_bytes(io->w, *text, *length);
    } else if (io->mode == IO_READ) {
        struct name n = antl_get_name(io->r);
        *text = n.text;
        *length = n.length;
    }
}

static void io_name(struct io *io, struct name *n)
{
    io_bytes(io, &n->text, &n->length);
}

static void io_text(struct io *io, struct token_text *t)
{
    io_bytes(io, &t->bytes, &t->length);
}

/* Types, symbolic values and constants */

static void io_type(struct io *io, struct type **t)
{
    uint32_t index;

    switch (io->mode) {
    case IO_COLLECT:
        if (*t != NULL) {
            antl_visit_type(io->w, *t);
        }
        break;
    case IO_WRITE:
        if (*t == NULL) {
            antl_put_u32(io->w, ANTL_NO_TYPE);
        } else {
            antl_put_type_ref(io->w, *t);
        }
        break;
    case IO_READ:
        index = antl_get_u32(io->r);
        if (index == ANTL_NO_TYPE) {
            *t = NULL;
        } else if (index >= io->r->table_count) {
            bad(io);
        } else {
            *t = io->r->table[index];
        }
        break;
    }
}

static void io_ctype(struct io *io, const struct type **t)
{
    struct type *v = (struct type *)*t;

    io_type(io, &v);
    *t = v;
}

static void io_symbolic(struct io *io, const struct symbolic **s)
{
    uint8_t present = *s != NULL;

    io_u8(io, &present);
    if (present == 0) {
        if (reading(io)) {
            *s = NULL;
        }
        return;
    }
    switch (io->mode) {
    case IO_COLLECT:
        antl_visit_symbolic(io->w, *s);
        break;
    case IO_WRITE:
        antl_put_symbolic(io->w, *s);
        break;
    case IO_READ:
        *s = antl_read_symbolic(io->r);
        break;
    }
}

/* A constant with its type, or none. */
static void io_value(struct io *io, struct const_value **v)
{
    uint8_t present = *v != NULL;
    struct type *type = *v != NULL ? (*v)->type : NULL;

    io_u8(io, &present);
    if (present == 0) {
        if (reading(io)) {
            *v = NULL;
        }
        return;
    }
    io_type(io, &type);
    switch (io->mode) {
    case IO_COLLECT:
        antl_visit_value(io->w, *v);
        break;
    case IO_WRITE:
        antl_put_value(io->w, *v);
        break;
    case IO_READ:
        if (type == NULL) {
            bad(io);
            return;
        }
        *v = antl_allocate(io->r, 1, sizeof **v);
        if (!antl_read_value(io->r, type, *v)) {
            bad(io);
        }
        break;
    }
}

/* The type whose fields hold f: the struct that declares it, or one of
   the chain of hint. */
static const struct type *field_owner(const struct struct_field *f,
                                      const struct type *hint)
{
    const struct type *t = f->home;

    if (t != NULL && t->field_count > 0 && f >= t->fields &&
        f < t->fields + t->field_count) {
        return t;
    }
    if (hint != NULL && hint->kind == TYPE_POINTER) {
        hint = hint->element;
    }
    for (t = hint; t != NULL; t = t->kind == TYPE_CLASS ? t->base : NULL) {
        if (t->field_count > 0 && f >= t->fields &&
            f < t->fields + t->field_count) {
            return t;
        }
    }
    return NULL;
}

/* A field of a struct: the struct and the index of the field. */
static void io_field(struct io *io, const struct struct_field **f,
                     const struct type *hint)
{
    struct type *owner = NULL;
    uint32_t index = 0;

    if (!reading(io) && *f != NULL) {
        owner = (struct type *)field_owner(*f, hint);
        if (owner == NULL) {
            bad(io);
            return;
        }
        index = (uint32_t)(*f - owner->fields);
    }
    io_type(io, &owner);
    if (owner != NULL) {
        io_u32(io, &index);
    }
    if (reading(io)) {
        if (owner == NULL) {
            *f = NULL;
        } else if (index >= owner->field_count) {
            bad(io);
        } else {
            *f = &owner->fields[index];
        }
    }
}

/* References within a tree */

static void collect(struct io *io, int kind, void *p);

/* A reference to a record of table kind: 0 for none, its index plus 1. */
static void io_ref(struct io *io, int kind, void **p)
{
    struct table *t = &io->t->tables[kind];
    uint32_t index = 0;

    switch (io->mode) {
    case IO_COLLECT:
        if (*p != NULL && !map_find(&t->index, *p, &index)) {
            collect(io, kind, *p);
        }
        break;
    case IO_WRITE:
        if (*p != NULL) {
            if (!map_find(&t->index, *p, &index)) {
                bad(io);
            }
            index++;
        }
        antl_put_u32(io->w, index);
        break;
    case IO_READ:
        index = antl_get_u32(io->r);
        if (index == 0) {
            *p = NULL;
        } else if (index > t->count) {
            bad(io);
            *p = NULL;
        } else {
            *p = t->items[index - 1];
        }
        break;
    }
}

static void io_expr(struct io *io, struct expr **e)
{
    void *p = *e;

    io_ref(io, T_EXPR, &p);
    *e = p;
}

static void io_cexpr(struct io *io, const struct expr **e)
{
    void *p = (void *)*e;

    io_ref(io, T_EXPR, &p);
    *e = p;
}

static void io_stmt(struct io *io, struct stmt **s)
{
    void *p = *s;

    io_ref(io, T_STMT, &p);
    *s = p;
}

static void io_block(struct io *io, struct block **b)
{
    void *p = *b;

    io_ref(io, T_BLOCK, &p);
    *b = p;
}

static void io_typex(struct io *io, struct type_expr **x)
{
    void *p = *x;

    io_ref(io, T_TYPEX, &p);
    *x = p;
}

static void io_item(struct io *io, struct item **it)
{
    void *p = *it;

    io_ref(io, T_ITEM, &p);
    *it = p;
}

static void io_citem(struct io *io, const struct item **it)
{
    void *p = (void *)*it;

    io_ref(io, T_ITEM, &p);
    *it = p;
}

/* Symbols */

/* Whether sym belongs to the tree. That is a local, a parameter, a
   constant of a block, a constant parameter, a type parameter or an
   anonymous function. The copy makes each of these anew. */
static bool local_symbol(const struct symbol *sym)
{
    if (sym->kind == SYMBOL_LOCAL || sym->kind == SYMBOL_PARAM) {
        return true;
    }
    if (sym->kind == SYMBOL_CONST &&
        (sym->stmt != NULL ||
         (sym->value != NULL && sym->value->kind == CONST_SYMBOLIC &&
          sym->value->as.symbolic != NULL &&
          sym->value->as.symbolic->kind == SYMBOLIC_PARAM))) {
        return true;
    }
    if (sym->kind == SYMBOL_STRUCT && sym->type != NULL &&
        sym->type->kind == TYPE_PARAM) {
        return true;
    }
    return sym->kind == SYMBOL_FN && sym->item != NULL &&
           sym->item->enclosing != NULL;
}

static void add_extern(struct io *io, const struct symbol *sym);

static void io_sym(struct io *io, struct symbol **s)
{
    struct table *t = &io->t->tables[T_SYM];
    uint32_t index = 0;

    switch (io->mode) {
    case IO_COLLECT:
        if (*s == NULL) {
            break;
        }
        if (local_symbol(*s)) {
            if (!map_find(&t->index, *s, &index)) {
                collect(io, T_SYM, *s);
            }
        } else {
            add_extern(io, *s);
        }
        break;
    case IO_WRITE:
        if (*s != NULL) {
            if (local_symbol(*s)) {
                if (!map_find(&t->index, *s, &index)) {
                    bad(io);
                }
                index++;
            } else if (map_find(&io->all->externs, *s, &index)) {
                index |= SYM_EXTERN;
            } else {
                bad(io);
            }
        }
        antl_put_u32(io->w, index);
        break;
    case IO_READ:
        index = antl_get_u32(io->r);
        if (index == 0) {
            *s = NULL;
        } else if ((index & SYM_EXTERN) != 0) {
            index &= ~SYM_EXTERN;
            if (index >= io->extern_count) {
                bad(io);
                *s = NULL;
            } else {
                *s = io->externs[index];
            }
        } else if (index > t->count) {
            bad(io);
            *s = NULL;
        } else {
            *s = t->items[index - 1];
        }
        break;
    }
}

static void io_csym(struct io *io, const struct symbol **s)
{
    struct symbol *v = (struct symbol *)*s;

    io_sym(io, &v);
    *s = v;
}

/* Lists */

/* A count, and in the reader an array of that many zeroed elements. */
static void io_count(struct io *io, void **array, size_t *count, size_t size)
{
    io_size(io, count);
    if (reading(io)) {
        *array = *count == 0 ? NULL
                             : antl_allocate(io->r, *count, size);
        if (failed(io)) {
            *count = 0;
        }
    }
}

static void io_exprs(struct io *io, struct expr ***list, size_t *count)
{
    void *array = *list;
    size_t i;

    io_count(io, &array, count, sizeof **list);
    *list = array;
    for (i = 0; i < *count && !failed(io); i++) {
        io_expr(io, &(*list)[i]);
    }
}

static void io_inits(struct io *io, struct field_init **list, size_t *count)
{
    void *array = *list;
    size_t i;

    io_count(io, &array, count, sizeof **list);
    *list = array;
    for (i = 0; i < *count && !failed(io); i++) {
        io_name(io, &(*list)[i].name);
        io_pos(io, &(*list)[i].pos);
        io_expr(io, &(*list)[i].value);
    }
}

static void io_bindings(struct io *io, struct binding **list, size_t *count)
{
    void *array = *list;
    size_t i;

    io_count(io, &array, count, sizeof **list);
    *list = array;
    for (i = 0; i < *count && !failed(io); i++) {
        io_name(io, &(*list)[i].name);
        io_pos(io, &(*list)[i].pos);
        io_sym(io, &(*list)[i].symbol);
        io_bool(io, &(*list)[i].assigns);
    }
}

static void io_arms(struct io *io, struct switch_arm **list, size_t *count)
{
    void *array = *list;
    size_t i;

    io_count(io, &array, count, sizeof **list);
    *list = array;
    for (i = 0; i < *count && !failed(io); i++) {
        struct switch_arm *a = &(*list)[i];
        io_expr(io, &a->value);
        io_pos(io, &a->pos);
        io_stmt(io, &a->body);
        io_name(io, &a->binds);
        io_pos(io, &a->binds_pos);
        io_sym(io, &a->bound);
        io_u32(io, &a->variant_case);
        io_expr(io, &a->test);
    }
}

static void io_handler(struct io *io, struct handler *h)
{
    IO_ENUM(io, h->kind, HANDLE_ENCLOSING);
    io_name(io, &h->name);
    io_pos(io, &h->pos);
    io_block(io, &h->body);
    io_sym(io, &h->symbol);
    io_bool(io, &h->none);
}

static void io_iteration(struct io *io, struct iteration *it)
{
    io_sym(io, &it->cursor);
    io_expr(io, &it->start);
    io_expr(io, &it->advance);
    io_expr(io, &it->current);
    io_expr(io, &it->place);
    io_expr(io, &it->changed);
    io_expr(io, &it->change_file);
    io_expr(io, &it->change_file_length);
    io_expr(io, &it->change_line);
}

/* Records */

static void io_typex_body(struct io *io, struct type_expr *x)
{
    void *array;
    size_t i;

    IO_ENUM(io, x->kind, TYPEX_OPTIONAL);
    io_pos(io, &x->pos);
    io_token(io, &x->builtin);
    io_name(io, &x->module);
    io_name(io, &x->name);
    io_name(io, &x->member);
    io_typex(io, &x->element);
    io_bool(io, &x->nullable);
    io_expr(io, &x->length);
    array = x->params;
    io_count(io, &array, &x->param_count, sizeof *x->params);
    x->params = array;
    for (i = 0; i < x->param_count && !failed(io); i++) {
        io_typex(io, &x->params[i]);
    }
    io_typex(io, &x->result);
    io_bool(io, &x->may_fail);
    io_bool(io, &x->keep);
    io_bool(io, &x->concurrent);
    io_bool(io, &x->owned);
    io_bool(io, &x->lent);
    io_type(io, &x->type);
}

static void io_sym_body(struct io *io, struct symbol *s)
{
    IO_ENUM(io, s->kind, SYMBOL_CONSTRAINT);
    io_name(io, &s->name);
    io_pos(io, &s->pos);
    io_type(io, &s->type);
    io_item(io, &s->item);
    io_stmt(io, &s->stmt);
    io_value(io, &s->value);
    IO_ENUM(io, s->state, EVAL_DONE);
    io_bool(io, &s->address_taken);
    io_bool(io, &s->read_only);
    io_name(io, &s->copy_of);
    io_bool(io, &s->lent_turn);
    io_bool(io, &s->walked_part);
    io_bool(io, &s->worker);
    io_bool(io, &s->may_fail);
    io_bool(io, &s->caught);
    io_bool(io, &s->atomic);
    io_bool(io, &s->into_fields);
    io_int(io, &s->caught_loops);
    io_csym(io, &s->moved_into);
    io_bool(io, &s->deferred);
    io_bool(io, &s->snapshot_moved);
    io_pos(io, &s->snapshot_move);
    io_bool(io, &s->moved);
    io_name(io, &s->moved_to);
    io_name(io, &s->moved_by);
    io_int(io, &s->loops);
    io_bool(io, &s->captured);
    io_bool(io, &s->own_param);
    io_u8(io, &s->flags_read);
    io_citem(io, &s->frame);
    io_int(io, &s->depth);
    io_citem(io, &s->closure);
    io_csym(io, &s->holds);
}

static void io_params(struct io *io, struct param **list, size_t *count)
{
    void *array = *list;
    size_t i;

    io_count(io, &array, count, sizeof **list);
    *list = array;
    for (i = 0; i < *count && !failed(io); i++) {
        struct param *p = &(*list)[i];
        io_name(io, &p->name);
        io_pos(io, &p->pos);
        io_typex(io, &p->type);
        io_sym(io, &p->symbol);
        io_bool(io, &p->owned);
        io_bool(io, &p->keep);
        io_bool(io, &p->concurrent);
        io_bool(io, &p->lent);
    }
}

/* An anonymous function. */
static void io_item_body(struct io *io, struct item *it)
{
    void *array;
    size_t i;

    IO_ENUM(io, it->kind, ITEM_TYPE);
    io_pos(io, &it->pos);
    io_name(io, &it->name);
    io_pos(io, &it->name_pos);
    io_params(io, &it->params, &it->param_count);
    io_typex(io, &it->result);
    io_bool(io, &it->result_lent);
    io_pos(io, &it->result_lent_pos);
    io_bool(io, &it->may_fail);
    io_pos(io, &it->may_fail_pos);
    io_bool(io, &it->worker);
    io_block(io, &it->body);
    io_sym(io, &it->symbol);
    io_bool(io, &it->has_self);
    io_sym(io, &it->self);
    io_item(io, &it->enclosing);
    array = it->captures;
    io_count(io, &array, &it->capture_count, sizeof *it->captures);
    it->captures = array;
    it->capture_capacity = it->capture_count;
    for (i = 0; i < it->capture_count && !failed(io); i++) {
        struct capture *c = &it->captures[i];
        io_sym(io, &c->symbol);
        io_bool(io, &c->written);
        io_pos(io, &c->write);
        io_bool(io, &c->called);
        io_pos(io, &c->call);
    }
    io_bool(io, &it->snapshot);
    io_bool(io, &it->snapshot_heap);
}

static void io_block_body(struct io *io, struct block *b)
{
    void *array = b->stmts;
    size_t i;

    io_pos(io, &b->pos);
    io_pos(io, &b->end);
    io_count(io, &array, &b->count, sizeof *b->stmts);
    b->stmts = array;
    for (i = 0; i < b->count && !failed(io); i++) {
        io_stmt(io, &b->stmts[i]);
    }
}

static void io_format(struct io *io, struct expr *e)
{
    void *array = e->as.format.parts;
    size_t i;

    io_count(io, &array, &e->as.format.count, sizeof *e->as.format.parts);
    e->as.format.parts = array;
    for (i = 0; i < e->as.format.count && !failed(io); i++) {
        struct format_part *p = &e->as.format.parts[i];
        io_text(io, &p->text);
        io_expr(io, &p->value);
        io_char(io, &p->spec.align);
        io_bool(io, &p->spec.zero);
        io_i32(io, &p->spec.width);
        io_i32(io, &p->spec.precision);
        io_char(io, &p->spec.kind);
        io_pos(io, &p->pos);
        io_text(io, &p->source);
        io_sym(io, &p->bound);
        io_expr(io, &p->text_call);
        io_expr(io, &p->value_call);
    }
    io_bool(io, &e->as.format.raw);
    io_sym(io, &e->as.format.builder);
    io_expr(io, &e->as.format.start);
    io_expr(io, &e->as.format.take);
}

static void io_call(struct io *io, struct expr *e)
{
    void *array;
    size_t i;

    io_expr(io, &e->as.call.callee);
    io_exprs(io, &e->as.call.args, &e->as.call.arg_count);
    io_ctype(io, &e->as.call.dispatch);
    io_name(io, &e->as.call.entry);
    io_handler(io, &e->as.call.handler);
    io_expr(io, &e->as.call.out);
    io_ctype(io, &e->as.call.builds);
    io_bool(io, &e->as.call.on_heap);
    io_bool(io, &e->as.call.guards_pointer);
    io_bool(io, &e->as.call.optional);
    io_bool(io, &e->as.call.tested);
    io_cexpr(io, &e->as.call.pattern);
    io_bool(io, &e->as.call.hashes);
    io_exprs(io, &e->as.call.hash_calls, &e->as.call.hash_count);
    array = e->as.call.copy_args;
    io_count(io, &array, &e->as.call.copy_count,
             sizeof *e->as.call.copy_args);
    e->as.call.copy_args = array;
    if (reading(io)) {
        e->as.call.copy_values =
            antl_allocate(io->r, e->as.call.copy_count,
                          sizeof *e->as.call.copy_values);
    }
    for (i = 0; i < e->as.call.copy_count && !failed(io); i++) {
        io_type(io, &e->as.call.copy_args[i]);
        io_symbolic(io, &e->as.call.copy_values[i]);
    }
}

static void io_expr_body(struct io *io, struct expr *e)
{
    uint8_t lanes;
    size_t i;

    IO_ENUM(io, e->kind, EXPR_PATTERN);
    io_pos(io, &e->pos);
    io_text(io, &e->spelling);
    io_type(io, &e->type);
    io_sym(io, &e->symbol);
    io_bool(io, &e->moves);
    /* A node the checker built checked, as the receiver of a nested
       hash call, which a copy hands to the checker again as it stands. */
    io_bool(io, &e->prechecked);
    io_field(io, &e->to_iface, e->type);
    io_bool(io, &e->to_context);
    io_type(io, &e->to_optional);
    io_bool(io, &e->moves_snapshot);
    io_type(io, &e->param_type);
    switch (e->kind) {
    case EXPR_INT:
        io_u64(io, &e->as.integer);
        break;
    case EXPR_CHAR:
        io_u32(io, &e->as.character);
        break;
    case EXPR_BOOL:
        io_bool(io, &e->as.boolean);
        break;
    case EXPR_FLOAT:
    case EXPR_STRING:
    case EXPR_BYTES:
    case EXPR_PATTERN:
        io_text(io, &e->as.text);
        break;
    case EXPR_NAME:
        io_name(io, &e->as.name);
        break;
    case EXPR_UNARY:
        io_token(io, &e->as.unary.op);
        io_expr(io, &e->as.unary.operand);
        break;
    case EXPR_BINARY:
        io_token(io, &e->as.binary.op);
        io_expr(io, &e->as.binary.left);
        io_expr(io, &e->as.binary.right);
        io_bool(io, &e->as.binary.carry);
        io_bool(io, &e->as.binary.equals);
        io_exprs(io, &e->as.binary.eq_calls, &e->as.binary.eq_count);
        break;
    case EXPR_CAST:
        io_expr(io, &e->as.cast.operand);
        io_typex(io, &e->as.cast.type);
        io_bool(io, &e->as.cast.checked);
        io_bool(io, &e->as.cast.test);
        io_bool(io, &e->as.cast.from_sub);
        io_bool(io, &e->as.cast.promoted);
        io_ctype(io, &e->as.cast.target);
        io_u32(io, &e->as.cast.variant_case);
        break;
    case EXPR_CALL:
        io_call(io, e);
        break;
    case EXPR_INDEX:
        io_expr(io, &e->as.index.base);
        io_expr(io, &e->as.index.index);
        io_bool(io, &e->as.index.several);
        break;
    case EXPR_SLICE:
        io_expr(io, &e->as.slice.base);
        io_expr(io, &e->as.slice.low);
        io_expr(io, &e->as.slice.high);
        break;
    case EXPR_FIELD:
        io_expr(io, &e->as.field.base);
        io_name(io, &e->as.field.name);
        io_field(io, &e->as.field.through,
                 e->as.field.base != NULL ? e->as.field.base->type : NULL);
        io_bool(io, &e->as.field.checked);
        io_u32(io, &e->as.field.enum_value);
        io_bool(io, &e->as.field.promoted);
        io_bool(io, &e->as.field.element);
        io_bool(io, &e->as.field.optional);
        break;
    case EXPR_STRUCT_LIT:
        io_name(io, &e->as.struct_lit.module);
        io_name(io, &e->as.struct_lit.name);
        io_name(io, &e->as.struct_lit.member);
        io_inits(io, &e->as.struct_lit.fields, &e->as.struct_lit.field_count);
        io_u32(io, &e->as.struct_lit.variant_case);
        break;
    case EXPR_TUPLE:
        io_exprs(io, &e->as.tuple.elements, &e->as.tuple.count);
        break;
    case EXPR_SLICE_LIT:
        io_typex(io, &e->as.slice_lit.element);
        io_inits(io, &e->as.slice_lit.fields, &e->as.slice_lit.field_count);
        break;
    case EXPR_ARRAY_LIT:
        io_exprs(io, &e->as.array_lit.elements, &e->as.array_lit.count);
        break;
    case EXPR_ARRAY_REPEAT:
        io_expr(io, &e->as.array_repeat.value);
        io_expr(io, &e->as.array_repeat.count);
        break;
    case EXPR_ALLOC:
        io_typex(io, &e->as.alloc.type);
        io_expr(io, &e->as.alloc.count);
        io_expr(io, &e->as.alloc.value);
        break;
    case EXPR_FREE:
        io_expr(io, &e->as.free_pointer);
        break;
    case EXPR_OBJECT:
        io_token(io, &e->as.object.op);
        io_expr(io, &e->as.object.operand);
        io_expr(io, &e->as.object.from);
        io_bool(io, &e->as.object.value);
        break;
    case EXPR_ATOMIC:
        IO_ENUM(io, e->as.atomic.op, ATOMIC_CAS);
        io_expr(io, &e->as.atomic.place);
        io_expr(io, &e->as.atomic.a);
        io_expr(io, &e->as.atomic.b);
        break;
    case EXPR_SIZE_OF:
        io_typex(io, &e->as.size_of);
        break;
    case EXPR_PARALLEL:
        io_expr(io, &e->as.parallel.array);
        io_expr(io, &e->as.parallel.chunks);
        io_expr(io, &e->as.parallel.call);
        break;
    case EXPR_DISPATCH:
        io_expr(io, &e->as.dispatch.object);
        io_expr(io, &e->as.dispatch.call);
        break;
    case EXPR_JOIN:
        io_expr(io, &e->as.join.job);
        io_bool(io, &e->as.join.all);
        break;
    case EXPR_FORMAT:
        io_format(io, e);
        break;
    case EXPR_IN:
        io_expr(io, &e->as.in.value);
        io_expr(io, &e->as.in.low);
        io_expr(io, &e->as.in.high);
        io_sym(io, &e->as.in.bound);
        io_expr(io, &e->as.in.test);
        break;
    case EXPR_OPTIONAL:
        io_expr(io, &e->as.optional.base);
        io_sym(io, &e->as.optional.bound);
        io_expr(io, &e->as.optional.access);
        break;
    case EXPR_SYNC_OP:
        IO_ENUM(io, e->as.sync_op.op, SYNC_CHAN_DELETE);
        io_expr(io, &e->as.sync_op.target);
        io_expr(io, &e->as.sync_op.value);
        io_typex(io, &e->as.sync_op.element);
        break;
    case EXPR_SIMD:
        IO_ENUM(io, e->as.simd.op, SIMD_OP_ALL);
        io_exprs(io, &e->as.simd.args, &e->as.simd.arg_count);
        io_ctype(io, &e->as.simd.simd);
        /* The lanes of a shuffle, one per field of its simd struct. */
        lanes = e->as.simd.lanes != NULL && e->as.simd.simd != NULL
                    ? (uint8_t)e->as.simd.simd->field_count
                    : 0;
        io_u8(io, &lanes);
        if (reading(io)) {
            e->as.simd.lanes =
                lanes == 0 ? NULL
                           : antl_allocate(io->r, lanes,
                                           sizeof *e->as.simd.lanes);
        }
        for (i = 0; i < lanes && !failed(io); i++) {
            io_u32(io, &e->as.simd.lanes[i]);
        }
        break;
    case EXPR_DESCRIPTOR:
        io_ctype(io, &e->as.descriptor_of);
        break;
    case EXPR_COLLECT:
        io_iteration(io, &e->as.collect);
        break;
    case EXPR_FN:
        io_item(io, &e->as.fn);
        break;
    case EXPR_NONE:
    case EXPR_HERE:
        break;
    }
}

static void io_stmt_body(struct io *io, struct stmt *s)
{
    void *array;
    size_t i;

    IO_ENUM(io, s->kind, STMT_SELECT);
    io_pos(io, &s->pos);
    io_bool(io, &s->error_exit);
    switch (s->kind) {
    case STMT_LET:
    case STMT_CONST:
        io_name(io, &s->as.let.name);
        io_pos(io, &s->as.let.name_pos);
        io_typex(io, &s->as.let.type);
        io_expr(io, &s->as.let.value);
        io_sym(io, &s->as.let.symbol);
        io_block(io, &s->as.let.otherwise);
        io_handler(io, &s->as.let.guard);
        io_sym(io, &s->as.let.guard_make);
        io_bindings(io, &s->as.let.names, &s->as.let.name_count);
        io_bool(io, &s->as.let.atomic);
        break;
    case STMT_EXPR:
        io_expr(io, &s->as.expr);
        break;
    case STMT_ASSIGN:
        io_token(io, &s->as.assign.op);
        io_expr(io, &s->as.assign.target);
        io_expr(io, &s->as.assign.value);
        break;
    case STMT_IF:
        array = s->as.if_chain.branches;
        io_count(io, &array, &s->as.if_chain.count,
                 sizeof *s->as.if_chain.branches);
        s->as.if_chain.branches = array;
        for (i = 0; i < s->as.if_chain.count && !failed(io); i++) {
            io_expr(io, &s->as.if_chain.branches[i].cond);
            io_block(io, &s->as.if_chain.branches[i].body);
        }
        io_block(io, &s->as.if_chain.else_body);
        break;
    case STMT_WHILE:
    case STMT_DO_WHILE:
        io_expr(io, &s->as.loop.cond);
        io_block(io, &s->as.loop.body);
        break;
    case STMT_FOR:
        io_bindings(io, &s->as.for_loop.names, &s->as.for_loop.name_count);
        io_bool(io, &s->as.for_loop.pattern);
        io_sym(io, &s->as.for_loop.element);
        io_expr(io, &s->as.for_loop.low);
        io_expr(io, &s->as.for_loop.high);
        io_expr(io, &s->as.for_loop.over);
        io_expr(io, &s->as.for_loop.step);
        io_pos(io, &s->as.for_loop.step_pos);
        io_i64(io, &s->as.for_loop.step_value);
        io_bool(io, &s->as.for_loop.by_pointer);
        io_text(io, &s->as.for_loop.over_text);
        io_block(io, &s->as.for_loop.body);
        io_iteration(io, &s->as.for_loop.hooks);
        break;
    case STMT_DEFER:
    case STMT_UNDO:
        io_stmt(io, &s->as.deferred);
        break;
    case STMT_FAIL:
        io_expr(io, &s->as.fail.value);
        io_sym(io, &s->as.fail.make);
        io_ctype(io, &s->as.fail.error);
        io_sym(io, &s->as.fail.capture);
        break;
    case STMT_SWITCH:
        io_expr(io, &s->as.switch_stmt.value);
        io_arms(io, &s->as.switch_stmt.arms, &s->as.switch_stmt.count);
        io_stmt(io, &s->as.switch_stmt.otherwise);
        io_size(io, &s->as.switch_stmt.otherwise_at);
        io_sym(io, &s->as.switch_stmt.bound);
        io_bool(io, &s->as.switch_stmt.if_let);
        break;
    case STMT_ASSERT:
        io_expr(io, &s->as.assertion.cond);
        io_text(io, &s->as.assertion.message);
        io_text(io, &s->as.assertion.text);
        break;
    case STMT_RETURN:
        io_expr(io, &s->as.return_value);
        break;
    case STMT_YIELD:
        io_expr(io, &s->as.yielded);
        break;
    case STMT_TRY:
        io_block(io, &s->as.try_block.body);
        io_handler(io, &s->as.try_block.handler);
        break;
    case STMT_BLOCK:
        io_block(io, &s->as.block);
        break;
    case STMT_SYNC:
        io_expr(io, &s->as.sync.mutex);
        io_block(io, &s->as.sync.body);
        io_bool(io, &s->as.sync.object);
        break;
    case STMT_SELECT:
        io_arms(io, &s->as.select.arms, &s->as.select.count);
        break;
    case STMT_BREAK:
    case STMT_CONTINUE:
    case STMT_FALLTHROUGH:
        break;
    }
}

/* The collecting pass: add p to its table and walk what it names. */
static void collect(struct io *io, int kind, void *p)
{
    table_add(&io->t->tables[kind], p);
    switch (kind) {
    case T_SYM:
        io_sym_body(io, p);
        break;
    case T_ITEM:
        io_item_body(io, p);
        break;
    case T_BLOCK:
        io_block_body(io, p);
        break;
    case T_STMT:
        io_stmt_body(io, p);
        break;
    case T_EXPR:
        io_expr_body(io, p);
        break;
    case T_TYPEX:
        io_typex_body(io, p);
        break;
    default:
        break;
    }
}

/* The symbols of other items */

/* DESIGN: a body names items that are not part of its tree. They are
   functions, constants and types of its own module or of another, and
   the functions of a struct or a class. Each is written once, with its
   kind, its name, its type and the module it lives in. Lowering calls it
   by the last two. A reader takes the symbol of the library it names
   when that library carries it. A generic, a function of a generic and
   a function of a class of another module are then the ones the program
   knows.
   It makes the symbol from the record for a private item, which only
   the module of the generic sees. The object of a module keeps each of
   its functions global for the other modules, so the call links. */
enum extern_lookup { LOOK_NONE, LOOK_MEMBER, LOOK_GENERIC };

static const char *home_of(const struct io *io, const struct symbol *sym)
{
    return sym->home != NULL ? sym->home->module : io->w->iface->module;
}

static void add_extern(struct io *io, const struct symbol *sym)
{
    struct writer *w = io->w;
    const struct item *it = sym->item;
    uint32_t index;

    if (map_find(&io->all->externs, sym, &index)) {
        return;
    }
    if (w->extern_count == w->extern_capacity) {
        size_t capacity = w->extern_capacity == 0 ? 16 : w->extern_capacity * 2;
        const struct symbol **list =
            realloc((void *)w->externs, capacity * sizeof *list);
        if (list == NULL) {
            out_of_memory();
        }
        w->externs = list;
        w->extern_capacity = capacity;
    }
    map_add(&io->all->externs, sym, (uint32_t)w->extern_count);
    w->externs[w->extern_count++] = sym;
    if (sym->type != NULL) {
        antl_visit_type(w, sym->type);
    }
    if (sym->value != NULL) {
        antl_visit_value(w, sym->value);
    }
    if (it != NULL && it->owner != NULL && it->owner->symbol != NULL &&
        it->owner->symbol->type != NULL) {
        antl_visit_type(w, it->owner->symbol->type);
    }
}

static void put_extern(struct writer *w, struct io *io,
                       const struct symbol *sym)
{
    const struct item *it = sym->item;
    const char *home = home_of(io, sym);
    uint8_t lookup = LOOK_NONE;

    if (it != NULL && it->owner != NULL && it->owner->symbol != NULL &&
        it->owner->symbol->type != NULL) {
        lookup = LOOK_MEMBER;
    } else if (it != NULL && sym->kind == SYMBOL_FN &&
               it->type_param_count > 0) {
        lookup = LOOK_GENERIC;
    }
    antl_put_u8(w, (uint8_t)sym->kind);
    antl_put_bytes(w, sym->name.text, sym->name.length);
    antl_put_type_ref(w, sym->type);
    antl_put_bytes(w, home, strlen(home));
    antl_put_u8(w, (uint8_t)((unsigned)sym->exported |
                             (unsigned)sym->variadic << 1 |
                             (unsigned)sym->may_fail << 2 |
                             (unsigned)sym->worker << 3 |
                             (unsigned)sym->internal << 4));
    antl_put_u8(w, lookup);
    if (lookup == LOOK_MEMBER) {
        antl_put_type_ref(w, it->owner->symbol->type);
    }
    /* A function of the root is a symbol of the runtime. */
    antl_put_bytes(w, it != NULL && it->runtime != NULL ? it->runtime : "",
                   it != NULL && it->runtime != NULL ? strlen(it->runtime)
                                                     : 0);
    antl_put_u8(w, it != NULL);
    if (it != NULL) {
        antl_put_bytes(w, it->name.text, it->name.length);
        antl_put_bytes(w, it->qualifier.text, it->qualifier.length);
        antl_put_u8(w, (uint8_t)((unsigned)it->contract |
                                 (unsigned)it->is_final << 2 |
                                 (unsigned)it->is_operator << 3 |
                                 (unsigned)it->has_self << 4 |
                                 (unsigned)it->may_fail << 5 |
                                 (unsigned)it->is_abstract << 6));
        antl_put_u8(w, (uint8_t)it->vis);
    }
    antl_put_u8(w, sym->kind == SYMBOL_CONST && sym->value != NULL);
    if (sym->kind == SYMBOL_CONST && sym->value != NULL) {
        antl_put_value(w, sym->value);
    }
}

static bool same_name(const struct name *a, const struct name *b)
{
    return a->length == b->length && memcmp(a->text, b->text, a->length) == 0;
}

static bool name_is(const struct name *n, const char *s)
{
    return n->length == strlen(s) && memcmp(n->text, s, n->length) == 0;
}

/* The generic function or type named name among the generics of iface. */
static struct item *generic_named(const struct interface *iface,
                                  const struct name *name, bool fn)
{
    size_t i;

    for (i = 0; iface != NULL && i < iface->generic_count; i++) {
        struct item *it = iface->generics[i];
        if ((it->kind == ITEM_FN) == fn && same_name(&it->name, name)) {
            return it;
        }
    }
    return NULL;
}

static struct symbol *read_extern(struct reader *r)
{
    struct symbol *sym = antl_allocate(r, 1, sizeof *sym);
    uint8_t kind = antl_get_u8(r);
    uint32_t type_index;
    struct name home;
    uint8_t marks;
    uint8_t lookup;
    struct name runtime;
    const struct interface *lib;
    struct type *owner = NULL;
    size_t i;

    sym->name = antl_get_name(r);
    type_index = antl_get_u32(r);
    home = antl_get_name(r);
    marks = antl_get_u8(r);
    lookup = antl_get_u8(r);
    if (r->failed || kind > SYMBOL_CONSTRAINT || type_index >= r->table_count ||
        marks > 31 || lookup > LOOK_GENERIC) {
        antl_damaged(r);
        return NULL;
    }
    if (lookup == LOOK_MEMBER) {
        owner = antl_type_ref(r, r->table_count);
    }
    runtime = antl_get_name(r);
    sym->kind = (enum symbol_kind)kind;
    sym->type = r->table[type_index];
    sym->exported = (marks & 1) != 0;
    sym->variadic = (marks >> 1 & 1) != 0;
    sym->may_fail = (marks >> 2 & 1) != 0;
    sym->worker = (marks >> 3 & 1) != 0;
    sym->internal = (marks >> 4 & 1) != 0;
    sym->state = EVAL_DONE;
    lib = name_is(&home, r->iface->module) ? r->iface
                                           : antl_library(r, &home);
    if (lib == NULL && !r->failed) {
        char module[256];
        snprintf(module, sizeof module, "%.*s", (int)home.length, home.text);
        antl_fail_needs(r, "module of", module, &sym->name);
        return NULL;
    }
    sym->home = lib;
    if (antl_get_u8(r) != 0) {
        struct item *it = antl_allocate(r, 1, sizeof *it);
        uint8_t flags;
        it->kind = ITEM_FN;
        it->name = antl_get_name(r);
        it->qualifier = antl_get_name(r);
        flags = antl_get_u8(r);
        it->contract = (enum fn_contract)(flags & 3);
        it->is_final = (flags >> 2 & 1) != 0;
        it->is_operator = (flags >> 3 & 1) != 0;
        it->has_self = (flags >> 4 & 1) != 0;
        it->may_fail = (flags >> 5 & 1) != 0;
        it->is_abstract = (flags >> 6 & 1) != 0;
        it->vis = (enum visibility)antl_get_u8(r);
        it->pub = it->vis == VIS_PUB;
        if ((flags & 3) > FN_CONCRETE || it->vis > VIS_PUB) {
            antl_damaged(r);
        }
        if (runtime.length > 0) {
            char *text = antl_allocate(r, runtime.length + 1, 1);
            memcpy(text, runtime.text, runtime.length);
            it->runtime = text;
        }
        it->symbol = sym;
        sym->item = it;
    }
    if (antl_get_u8(r) != 0) {
        sym->value = antl_allocate(r, 1, sizeof *sym->value);
        if (!antl_read_value(r, sym->type, sym->value)) {
            antl_damaged(r);
        }
    }
    if (r->failed) {
        return NULL;
    }
    /* The library's own symbol, where it carries one. */
    if (lookup == LOOK_MEMBER) {
        for (i = 0; i < owner->member_count; i++) {
            const struct item *m = owner->members[i];
            if (m->symbol != NULL && same_name(&m->symbol->name, &sym->name)) {
                return m->symbol;
            }
        }
        return sym;
    }
    if (lookup == LOOK_GENERIC) {
        struct item *g = generic_named(lib, &sym->name, true);
        if (g == NULL) {
            antl_fail_needs(r, "generic", lib->module, &sym->name);
            return NULL;
        }
        return g->symbol;
    }
    for (i = 0; lib != r->iface && i < lib->item_count; i++) {
        struct symbol *s = lib->items[i];
        if (s->kind == sym->kind && same_name(&s->name, &sym->name)) {
            return s;
        }
    }
    return sym;
}

/* The declarations of the generics */

/* DESIGN: the declaration of a generic follows the type table. A
   function carries its name, its position, its level, its type and its
   marks: `may fail`, `worker` and `operator`. The checker reads them
   from the declaration when it looks for the hook of a type. The names
   and defaults of its parameters and its type parameters follow. A
   struct, a class or a variant carries its type and the marks of its
   declaration. The entry of the type holds its fields, its functions and
   its parameters. A `pub` generic stands among the items as well, and
   the reader gives that symbol the declaration. */
static void put_declaration(struct writer *w, const struct item *it)
{
    /* A shared `operator fn` stands under its shared name, which the
       items section gives its symbol. */
    const struct name *name = it->overloaded ? &it->symbol->name : &it->name;
    size_t i;

    antl_put_u8(w, (uint8_t)it->kind);
    antl_put_bytes(w, name->text, name->length);
    antl_put_u32(w, (uint32_t)it->pos.line);
    antl_put_u32(w, (uint32_t)it->pos.column);
    antl_put_u8(w, (uint8_t)it->vis);
    antl_put_type_ref(w, it->symbol->type);
    if (it->kind == ITEM_FN) {
        antl_put_u8(w, (uint8_t)((unsigned)it->may_fail |
                                 (unsigned)it->worker << 1 |
                                 (unsigned)it->is_operator << 2));
        antl_put_count(w, it->param_count);
        for (i = 0; i < it->param_count; i++) {
            antl_put_bytes(w, it->params[i].name.text,
                           it->params[i].name.length);
        }
        antl_put_param_defaults(w, it->symbol);
        antl_put_param_owned(w, it->symbol);
        antl_put_count(w, it->type_param_count);
        for (i = 0; i < it->type_param_count; i++) {
            antl_put_type_ref(w, it->type_params[i].type);
        }
    } else {
        antl_put_u8(w, (uint8_t)((unsigned)it->is_abstract |
                                 (unsigned)it->is_final << 1 |
                                 (unsigned)it->is_singleton << 2 |
                                 (unsigned)it->synchronized << 3 |
                                 (unsigned)it->concurrent << 4 |
                                 (unsigned)it->trace << 5 |
                                 (unsigned)it->packed << 6));
    }
    antl_put_bytes(w, it->doc.text, w->strip_docs ? 0 : it->doc.length);
}

/* Give the parameter types of it their declaration, and the item its own
   list of them. */
static void own_params(struct reader *r, struct item *it, struct type **types,
                       size_t count)
{
    size_t i;

    it->type_params = antl_allocate(r, count, sizeof *it->type_params);
    for (i = 0; i < count; i++) {
        struct type *p = types[i];
        if (p->declared_by != NULL) {
            /* One parameter belongs to one generic. */
            antl_damaged(r);
            return;
        }
        it->type_params[i] = *p->param;
        p->param = &it->type_params[i];
        p->declared_by = it;
        if (p->walked != NULL) {
            p->walked->declared_by = it;
        }
        if (p->indexed != NULL) {
            p->indexed->declared_by = it;
        }
    }
    it->type_param_count = count;
}

/* The symbol the items section gave a `pub` generic of the module. */
static struct symbol *listed(struct reader *r, const struct name *name,
                             enum symbol_kind kind)
{
    size_t i;

    for (i = 0; i < r->iface->item_count; i++) {
        struct symbol *sym = r->iface->items[i];
        if (sym->kind == kind && !sym->alias && same_name(&sym->name, name)) {
            return sym;
        }
    }
    return NULL;
}

static struct item *read_declaration(struct reader *r)
{
    struct item *it = antl_allocate(r, 1, sizeof *it);
    struct symbol *sym;
    uint8_t kind = antl_get_u8(r);
    struct type *type;
    uint8_t marks;
    size_t i;

    it->name = antl_get_name(r);
    it->pos.line = (int)antl_get_u32(r);
    it->pos.column = (int)antl_get_u32(r);
    it->name_pos = it->pos;
    it->vis = (enum visibility)antl_get_u8(r);
    type = antl_type_ref(r, r->table_count);
    if (r->failed || it->vis > VIS_PUB ||
        (kind != ITEM_FN && kind != ITEM_STRUCT && kind != ITEM_CLASS &&
         kind != ITEM_VARIANT)) {
        antl_damaged(r);
        return NULL;
    }
    it->kind = (enum item_kind)kind;
    it->pub = it->vis == VIS_PUB;
    sym = listed(r, &it->name, kind == ITEM_FN ? SYMBOL_FN : SYMBOL_STRUCT);
    if (!it->pub || sym == NULL) {
        sym = antl_allocate(r, 1, sizeof *sym);
        sym->kind = kind == ITEM_FN ? SYMBOL_FN : SYMBOL_STRUCT;
        sym->name = it->name;
        sym->type = type;
        sym->home = r->iface;
        sym->state = EVAL_DONE;
    } else if (sym->type != type) {
        antl_damaged(r);
        return NULL;
    }
    sym->item = it;
    it->symbol = sym;
    if (kind == ITEM_FN) {
        uint32_t n;
        struct type **params;
        marks = antl_get_u8(r);
        it->may_fail = (marks & 1) != 0;
        it->worker = (marks >> 1 & 1) != 0;
        it->is_operator = (marks >> 2 & 1) != 0;
        sym->may_fail = it->may_fail;
        sym->worker = it->worker;
        sym->is_operator = it->is_operator;
        n = antl_get_count(r, 4);
        it->params = antl_allocate(r, n, sizeof *it->params);
        for (i = 0; i < n && !r->failed; i++) {
            it->params[i].name = antl_get_name(r);
        }
        it->param_count = n;
        if (type->kind != TYPE_FN || marks > 7) {
            antl_damaged(r);
            return NULL;
        }
        if (sym->params == NULL) {
            struct name *names = antl_allocate(r, type->param_count,
                                               sizeof *names);
            for (i = 0; i < n && i < type->param_count; i++) {
                names[i] = it->params[i].name;
            }
            for (; i < type->param_count; i++) {
                names[i].text = "out";
                names[i].length = 3;
            }
            sym->params = names;
        }
        antl_read_param_defaults(r, sym);
        antl_read_param_owned(r, sym);
        n = antl_get_count(r, 4);
        params = antl_allocate(r, n, sizeof *params);
        for (i = 0; i < n && !r->failed; i++) {
            params[i] = antl_type_ref(r, r->table_count);
            if (!r->failed && (params[i]->kind != TYPE_PARAM ||
                               params[i]->param == NULL)) {
                antl_damaged(r);
            }
        }
        if (n == 0) {
            antl_damaged(r);
        }
        if (!r->failed) {
            own_params(r, it, params, n);
        }
    } else {
        marks = antl_get_u8(r);
        it->is_abstract = (marks & 1) != 0;
        it->is_final = (marks >> 1 & 1) != 0;
        it->is_singleton = (marks >> 2 & 1) != 0;
        it->synchronized = (marks >> 3 & 1) != 0;
        it->concurrent = (marks >> 4 & 1) != 0;
        it->trace = (marks >> 5 & 1) != 0;
        it->packed = (marks >> 6 & 1) != 0;
        if (type->type_param_count == 0 || (int)type->kind !=
            (kind == ITEM_STRUCT ? TYPE_STRUCT
             : kind == ITEM_CLASS ? TYPE_CLASS
                                  : TYPE_VARIANT)) {
            antl_damaged(r);
            return NULL;
        }
        own_params(r, it, type->type_params, type->type_param_count);
        it->members = type->members;
        it->member_count = type->member_count;
        for (i = 0; i < it->member_count; i++) {
            it->members[i]->owner = it;
        }
    }
    {
        struct name doc = antl_get_name(r);
        it->doc.text = doc.text;
        it->doc.length = doc.length;
        sym->doc = it->doc;
    }
    return r->failed ? NULL : it;
}

/* The bodies */

/* The function a tree belongs to and the facts of its declaration that
   the tables do not hold: its parameters, `self`, its result and its
   body. */
static void io_root(struct io *io, struct item *fn)
{
    size_t count = fn->param_count;
    size_t i;

    io_size(io, &count);
    if (reading(io) && count != fn->param_count) {
        bad(io);
        return;
    }
    for (i = 0; i < count && !failed(io); i++) {
        io_typex(io, &fn->params[i].type);
        io_sym(io, &fn->params[i].symbol);
        io_bool(io, &fn->params[i].owned);
        io_bool(io, &fn->params[i].keep);
        io_bool(io, &fn->params[i].concurrent);
        io_bool(io, &fn->params[i].lent);
    }
    io_bool(io, &fn->has_self);
    io_sym(io, &fn->self);
    io_typex(io, &fn->result);
    io_bool(io, &fn->result_lent);
    io_block(io, &fn->body);
    io_bool(io, &fn->trace);
    io_pos(io, &fn->may_fail_pos);
}

static void collect_tree(struct writer *w, struct trees *all,
                         struct item *fn, uint8_t owner_kind, uint32_t owner,
                         uint32_t member)
{
    struct io io;
    struct tree *t;

    if (all->count == all->capacity) {
        size_t capacity = all->capacity == 0 ? 8 : all->capacity * 2;
        struct tree *items = realloc(all->items, capacity * sizeof *items);
        if (items == NULL) {
            out_of_memory();
        }
        all->items = items;
        all->capacity = capacity;
    }
    t = &all->items[all->count++];
    memset(t, 0, sizeof *t);
    t->fn = fn;
    t->owner_kind = owner_kind;
    t->owner = owner;
    t->member = member;
    memset(&io, 0, sizeof io);
    io.mode = IO_COLLECT;
    io.w = w;
    io.t = t;
    io.all = all;
    /* The function is item 0 of its tree. */
    table_add(&t->tables[T_ITEM], fn);
    io_root(&io, fn);
}

void antl_visit_generics(struct writer *w)
{
    const struct interface *iface = w->iface;
    struct trees *all = calloc(1, sizeof *all);
    size_t i;
    size_t j;

    if (all == NULL) {
        out_of_memory();
    }
    w->trees = all;
    for (i = 0; i < iface->generic_count; i++) {
        struct item *it = iface->generics[i];
        antl_visit_type(w, it->symbol->type);
        for (j = 0; j < it->type_param_count; j++) {
            antl_visit_type(w, it->type_params[j].type);
        }
        if (it->kind == ITEM_FN) {
            antl_visit_defaults(w, it->symbol);
        }
    }
    for (i = 0; i < iface->generic_count; i++) {
        struct item *it = iface->generics[i];
        const struct type *t = it->symbol->type;
        uint32_t index = 0;
        if (it->kind == ITEM_FN) {
            if (it->body != NULL) {
                collect_tree(w, all, it, 0, (uint32_t)i, 0);
            }
            continue;
        }
        /* The functions of the type in the order its entry carries them. */
        for (j = 0; j < t->member_count; j++) {
            struct item *m = t->members[j];
            if (m->kind != ITEM_FN || m->symbol == NULL ||
                m->type_param_count > 0) {
                continue;
            }
            if (m->body != NULL) {
                collect_tree(w, all, m, 1, (uint32_t)i, index);
            }
            index++;
        }
    }
}

static void write_tree(struct writer *w, struct trees *all, struct tree *t)
{
    struct io io;
    size_t k;
    size_t i;

    memset(&io, 0, sizeof io);
    io.mode = IO_WRITE;
    io.w = w;
    io.t = t;
    io.all = all;
    antl_put_u8(w, t->owner_kind);
    antl_put_u32(w, t->owner);
    antl_put_u32(w, t->member);
    for (k = 0; k < T_COUNT; k++) {
        /* Item 0 is the function, which the reader has. */
        antl_put_count(w, t->tables[k].count - (k == T_ITEM ? 1 : 0));
    }
    io_root(&io, t->fn);
    for (i = 0; i < t->tables[T_SYM].count; i++) {
        io_sym_body(&io, t->tables[T_SYM].items[i]);
    }
    for (i = 1; i < t->tables[T_ITEM].count; i++) {
        io_item_body(&io, t->tables[T_ITEM].items[i]);
    }
    for (i = 0; i < t->tables[T_BLOCK].count; i++) {
        io_block_body(&io, t->tables[T_BLOCK].items[i]);
    }
    for (i = 0; i < t->tables[T_STMT].count; i++) {
        io_stmt_body(&io, t->tables[T_STMT].items[i]);
    }
    for (i = 0; i < t->tables[T_EXPR].count; i++) {
        io_expr_body(&io, t->tables[T_EXPR].items[i]);
    }
    for (i = 0; i < t->tables[T_TYPEX].count; i++) {
        io_typex_body(&io, t->tables[T_TYPEX].items[i]);
    }
}

void antl_put_generics(struct writer *w)
{
    const struct interface *iface = w->iface;
    struct trees *all = w->trees;
    struct io io;
    size_t i;
    size_t k;

    antl_put_count(w, iface->generic_count);
    for (i = 0; i < iface->generic_count; i++) {
        put_declaration(w, iface->generics[i]);
    }
    memset(&io, 0, sizeof io);
    io.mode = IO_WRITE;
    io.w = w;
    io.all = all;
    antl_put_count(w, w->extern_count);
    for (i = 0; i < w->extern_count; i++) {
        put_extern(w, &io, w->externs[i]);
    }
    antl_put_count(w, all->count);
    for (i = 0; i < all->count; i++) {
        write_tree(w, all, &all->items[i]);
    }
    for (i = 0; i < all->count; i++) {
        for (k = 0; k < T_COUNT; k++) {
            table_free(&all->items[i].tables[k]);
        }
    }
    free(all->items);
    map_free(&all->externs);
    free(all);
    w->trees = NULL;
}

/* The function of a generic type that a tree belongs to. */
static struct item *tree_owner(struct reader *r, uint8_t kind, uint32_t owner,
                               uint32_t member)
{
    struct interface *iface = r->iface;
    struct item *it;

    if (kind > 1 || owner >= iface->generic_count) {
        antl_damaged(r);
        return NULL;
    }
    it = iface->generics[owner];
    if ((kind == 0) != (it->kind == ITEM_FN) ||
        (kind == 1 && member >= it->member_count)) {
        antl_damaged(r);
        return NULL;
    }
    return kind == 0 ? it : it->members[member];
}

static void read_tree(struct reader *r, struct symbol **externs,
                      uint32_t extern_count)
{
    static const size_t sizes[T_COUNT] = {
        sizeof(struct symbol), sizeof(struct item), sizeof(struct block),
        sizeof(struct stmt), sizeof(struct expr), sizeof(struct type_expr)
    };
    struct tree t;
    struct io io;
    struct item *fn;
    uint8_t kind = antl_get_u8(r);
    uint32_t owner = antl_get_u32(r);
    uint32_t member = antl_get_u32(r);
    size_t k;
    size_t i;

    if (r->failed) {
        return;
    }
    fn = tree_owner(r, kind, owner, member);
    if (fn == NULL || fn->body != NULL) {
        antl_damaged(r);
        return;
    }
    memset(&t, 0, sizeof t);
    t.fn = fn;
    for (k = 0; k < T_COUNT && !r->failed; k++) {
        uint32_t n = antl_get_count(r, 1);
        struct table *table = &t.tables[k];
        table->count = n + (k == T_ITEM ? 1 : 0);
        table->items = calloc(table->count + 1, sizeof *table->items);
        if (table->items == NULL) {
            out_of_memory();
        }
        for (i = 0; i < table->count; i++) {
            table->items[i] = k == T_ITEM && i == 0
                                  ? (void *)fn
                                  : antl_allocate(r, 1, sizes[k]);
        }
    }
    memset(&io, 0, sizeof io);
    io.mode = IO_READ;
    io.r = r;
    io.t = &t;
    io.externs = externs;
    io.extern_count = extern_count;
    if (!r->failed) {
        io_root(&io, fn);
    }
    for (i = 0; i < t.tables[T_SYM].count && !r->failed; i++) {
        io_sym_body(&io, t.tables[T_SYM].items[i]);
    }
    for (i = 1; i < t.tables[T_ITEM].count && !r->failed; i++) {
        io_item_body(&io, t.tables[T_ITEM].items[i]);
    }
    for (i = 0; i < t.tables[T_BLOCK].count && !r->failed; i++) {
        io_block_body(&io, t.tables[T_BLOCK].items[i]);
    }
    for (i = 0; i < t.tables[T_STMT].count && !r->failed; i++) {
        io_stmt_body(&io, t.tables[T_STMT].items[i]);
    }
    for (i = 0; i < t.tables[T_EXPR].count && !r->failed; i++) {
        io_expr_body(&io, t.tables[T_EXPR].items[i]);
    }
    for (i = 0; i < t.tables[T_TYPEX].count && !r->failed; i++) {
        io_typex_body(&io, t.tables[T_TYPEX].items[i]);
    }
    if (fn->body == NULL && !r->failed) {
        antl_damaged(r);
    }
    if (fn->self != NULL) {
        fn->has_self = true;
    }
    for (k = 0; k < T_COUNT; k++) {
        free(t.tables[k].items);
    }
}

void antl_read_generics(struct reader *r)
{
    struct interface *iface = r->iface;
    uint32_t count = antl_get_count(r, 12);
    struct symbol **externs;
    uint32_t extern_count;
    uint32_t trees;
    uint32_t i;

    iface->generics = antl_allocate(r, count, sizeof *iface->generics);
    for (i = 0; i < count && !r->failed; i++) {
        struct item *it = read_declaration(r);
        if (it != NULL) {
            iface->generics[iface->generic_count++] = it;
        }
    }
    /* Every generic function among the items has its declaration. */
    for (i = 0; i < iface->item_count && !r->failed; i++) {
        const struct symbol *sym = iface->items[i];
        if (r->marked_generic[i] !=
            (sym->kind == SYMBOL_FN && sym->item != NULL)) {
            antl_damaged(r);
        }
    }
    extern_count = antl_get_count(r, 12);
    externs = antl_allocate(r, extern_count, sizeof *externs);
    for (i = 0; i < extern_count && !r->failed; i++) {
        externs[i] = read_extern(r);
    }
    trees = antl_get_count(r, 33);
    for (i = 0; i < trees && !r->failed; i++) {
        read_tree(r, externs, extern_count);
    }
}

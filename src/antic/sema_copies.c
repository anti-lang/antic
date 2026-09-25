#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sema_checker.h"

/* DESIGN: the compiled copies of generics. A generic is checked once,
   against its type parameters, and its checked tree keeps them open. A
   use with concrete arguments gets a copy, a clone of the tree of the
   generic. The clone has the arguments in place of the parameters in
   every type. Each local, parameter and anonymous function of the
   generic has one of its own there. The copy is an ordinary item of the
   module, so lowering and every pass after it see code that is not
   generic. A library file stores the same checked tree. A copy of a
   generic of another module is made the same way.

   Most of the tree reads the same for every argument. A few nodes mean
   what the type of the argument makes of them. They are an operator on
   a value of a type parameter, a `for` over one, `e[i]` on one, and a
   call of a function of an interface of its constraints. The copy hands
   each of those to the checker again, with its operands made and marked
   prechecked. The operator of an `int` is then the instruction, and that
   of a struct the call of its `operator fn`, as in code written by hand.
   A call of a generic names its copy. The checker recorded the arguments
   of that copy on the call, and the copy puts its own arguments into
   them.

   A copy of a generic struct, class or variant is a type the checker
   built already. Here it gets an item of its own, whose functions are
   copies of those of the generic. It replaces the members of the type.
   The table, the descriptor and every call lowering writes from the type
   then reach the copy. The names carry the arguments, `max<int>` and
   `List<int>.push`. The symbol of a copy follows from the generic and its
   arguments alone. */

/* A map of pointers to pointers, open addressed and at most half full. */
struct ptr_map {
    const void **keys;
    void **values;
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

static void *map_get(const struct ptr_map *m, const void *key)
{
    size_t i;

    if (m->capacity == 0) {
        return NULL;
    }
    for (i = ptr_hash(key, m->capacity); m->keys[i] != NULL;
         i = (i + 1) & (m->capacity - 1)) {
        if (m->keys[i] == key) {
            return m->values[i];
        }
    }
    return NULL;
}

static void map_put(struct ptr_map *m, const void *key, void *value)
{
    size_t i;

    if ((m->count + 1) * 2 > m->capacity) {
        struct ptr_map grown;
        size_t k;
        grown.capacity = m->capacity == 0 ? 64 : m->capacity * 2;
        grown.count = 0;
        grown.keys = calloc(grown.capacity, sizeof *grown.keys);
        grown.values = calloc(grown.capacity, sizeof *grown.values);
        if (grown.keys == NULL || grown.values == NULL) {
            fputs("antic: out of memory\n", stderr);
            exit(70);
        }
        for (k = 0; k < m->capacity; k++) {
            if (m->keys[k] != NULL) {
                map_put(&grown, m->keys[k], m->values[k]);
            }
        }
        free(m->keys);
        free(m->values);
        *m = grown;
    }
    for (i = ptr_hash(key, m->capacity); m->keys[i] != NULL;
         i = (i + 1) & (m->capacity - 1)) {
        if (m->keys[i] == key) {
            m->values[i] = value;
            return;
        }
    }
    m->keys[i] = key;
    m->values[i] = value;
    m->count++;
}

static void map_free(struct ptr_map *m)
{
    free(m->keys);
    free(m->values);
    memset(m, 0, sizeof *m);
}

static void *grow(void *items, size_t *capacity, size_t count, size_t size)
{
    size_t want;
    void *p;

    if (count < *capacity) {
        return items;
    }
    want = *capacity == 0 ? 16 : *capacity * 2;
    p = want <= SIZE_MAX / size ? realloc(items, want * size) : NULL;
    if (p == NULL) {
        fputs("antic: out of memory\n", stderr);
        exit(70);
    }
    *capacity = want;
    return p;
}

/* A copy of a generic function, by the generic and its arguments. */
struct fn_copy {
    const struct item *generic;
    struct type **args;
    const struct symbolic **values;
    size_t count;
    struct item *copy;
};

/* A function of a copy whose body is still to be made: the function of
   the generic it comes from and the arguments. */
struct work {
    struct item *from;
    struct item *to;
    struct generic_map map;
};

struct copies {
    struct checker *c;
    struct fn_copy *fns;
    size_t fn_count;
    size_t fn_capacity;
    struct work *work;
    size_t work_count;
    size_t work_capacity;
    struct ptr_map generics;        /* a generic type to its item */
    struct ptr_map items;           /* a copy type to its item */
    struct item **added;
    size_t added_count;
    size_t added_capacity;
    bool failed;
};

/* The making of one copy, or the walk of code that is not generic, which
   rewrites its calls of generics in place. */
struct clone {
    struct copies *k;
    struct generic_map map;
    size_t map_capacity;
    /* A node of the generic and its copy. */
    struct ptr_map nodes;
    /* The walk changes code that is not generic in place. */
    bool fresh;
    /* The name being made is called. */
    bool callee;
    /* The generic the copy comes from. */
    const struct item *generic;
};

/* Refuse a copy of the generic named name that this step of antic does
   not compile, once, with the form that stops it. */
static void copy_failed(struct clone *cl, struct pos pos,
                        const struct name *name, const char *form)
{
    struct copies *k = cl->k;

    if (k->failed) {
        return;
    }
    k->failed = true;
    if (name == NULL && cl->generic != NULL) {
        name = &cl->generic->name;
    }
    if (name != NULL) {
        sema_error_at(k->c, pos, "antic compiles no copy of `%.*s` yet, %s",
                      (int)name->length, name->text, form);
    } else {
        sema_error_at(k->c, pos, "antic compiles no copy here yet, %s", form);
    }
}

/* Types */

static struct type *ty(struct clone *cl, struct type *t, struct pos pos)
{
    struct type *r;

    if (t == NULL || (cl->map.count == 0 && cl->map.from == NULL)) {
        return t;
    }
    r = sema_subst(cl->k->c, t, &cl->map);
    if (sema_has_params(r)) {
        copy_failed(cl, pos, NULL, "which names a type its arguments do not settle");
    }
    return r;
}

/* Add p, a parameter the checker made for the value a hook gives, with
   the type the copy found for it. */
static void map_add(struct clone *cl, struct type *p, struct type *arg)
{
    struct type **params;

    if (p == NULL || arg == NULL) {
        return;
    }
    if (cl->map.count + 1 >= cl->map_capacity) {
        size_t capacity = (cl->map.count + 1) * 2 + 8;
        struct type **args =
            types_alloc_array(cl->k->c->arena, capacity, sizeof *args);
        const struct symbolic **values =
            types_alloc_array(cl->k->c->arena, capacity, sizeof *values);
        params = types_alloc_array(cl->k->c->arena, capacity, sizeof *params);
        if (cl->map.count > 0) {
            memcpy(params, cl->map.params, cl->map.count * sizeof *params);
            memcpy(args, cl->map.args, cl->map.count * sizeof *args);
            memcpy(values, cl->map.values, cl->map.count * sizeof *values);
        }
        cl->map.params = params;
        cl->map.args = args;
        cl->map.values = values;
        cl->map_capacity = capacity;
    }
    params = (struct type **)cl->map.params;
    params[cl->map.count] = p;
    cl->map.args[cl->map.count] = arg;
    cl->map.values[cl->map.count] = NULL;
    cl->map.count++;
}

static struct type_expr *xt(struct clone *cl, struct type_expr *t)
{
    struct type_expr *n;
    size_t i;

    if (t == NULL || !cl->fresh) {
        return t;
    }
    n = arena_alloc(cl->k->c->arena, sizeof *n);
    *n = *t;
    n->type = ty(cl, t->type, t->pos);
    n->element = xt(cl, t->element);
    n->result = xt(cl, t->result);
    if (t->param_count > 0) {
        n->params = types_alloc_array(cl->k->c->arena, t->param_count,
                                      sizeof *n->params);
        for (i = 0; i < t->param_count; i++) {
            n->params[i] = xt(cl, t->params[i]);
        }
    }
    return n;
}

/* The field of the copy that stands where f stands in the type t of the
   generic, found along the chain of bases. A field of a type that is not
   generic is itself. */
static const struct struct_field *xf(struct clone *cl, struct type *t,
                                     const struct struct_field *f)
{
    struct type *up;

    if (f == NULL || t == NULL || !cl->fresh) {
        return f;
    }
    if (t->kind == TYPE_POINTER) {
        t = t->element;
    }
    for (up = t; up != NULL; up = up->kind == TYPE_CLASS ? up->base : NULL) {
        if (up->field_count > 0 && f >= up->fields &&
            f < up->fields + up->field_count) {
            struct type *copy = ty(cl, up, f->pos);
            size_t index = (size_t)(f - up->fields);
            return copy != NULL && index < copy->field_count
                       ? &copy->fields[index]
                       : f;
        }
    }
    return f;
}

/* Symbols */

static bool symbol_open(const struct symbol *s)
{
    return s->kind == SYMBOL_CONST && s->value != NULL &&
           s->value->kind == CONST_SYMBOLIC && s->value->as.symbolic != NULL &&
           s->value->as.symbolic->kind == SYMBOLIC_PARAM;
}

static struct symbol *xsym(struct clone *cl, struct symbol *s)
{
    struct symbol *n;

    if (s == NULL || !cl->fresh) {
        return s;
    }
    if (s->kind != SYMBOL_LOCAL && s->kind != SYMBOL_PARAM &&
        !(s->kind == SYMBOL_CONST && (s->stmt != NULL || symbol_open(s)))) {
        return s;
    }
    n = map_get(&cl->nodes, s);
    if (n != NULL) {
        return n;
    }
    n = arena_alloc(cl->k->c->arena, sizeof *n);
    *n = *s;
    n->ir = 0;
    map_put(&cl->nodes, s, n);
    n->type = ty(cl, s->type, s->pos);
    if (s->frame != NULL && map_get(&cl->nodes, s->frame) != NULL) {
        n->frame = map_get(&cl->nodes, s->frame);
    }
    if (s->kind == SYMBOL_CONST && s->value != NULL &&
        s->value->kind == CONST_SYMBOLIC) {
        struct const_value *v = arena_alloc(cl->k->c->arena, sizeof *v);
        *v = *s->value;
        v->type = ty(cl, s->value->type, s->pos);
        v->as.symbolic =
            sema_subst_symbolic(cl->k->c, s->value->as.symbolic, &cl->map);
        if (v->as.symbolic != NULL && v->as.symbolic->kind == SYMBOLIC_INT) {
            v->kind = CONST_INT;
            v->as.integer = v->as.symbolic->value;
        }
        n->value = v;
    }
    return n;
}

/* Copies of generic functions and types */

static struct item *type_item(struct copies *k, struct type *copy);
static struct item *fn_copy(struct copies *k, struct item *generic,
                            struct type **args,
                            const struct symbolic **values);

static void add_work(struct copies *k, struct item *from, struct item *to,
                     const struct generic_map *map)
{
    k->work = grow(k->work, &k->work_capacity, k->work_count, sizeof *k->work);
    k->work[k->work_count].from = from;
    k->work[k->work_count].to = to;
    k->work[k->work_count].map = *map;
    k->work_count++;
}

/* The name of the function name of the copy, `List<geo.Point>.push`,
   whose arguments carry their modules as its symbol does. */
static struct name symbol_name(struct copies *k, const struct type *copy,
                               const struct name *name)
{
    struct text out = {0};
    struct name made;
    char *text;

    type_symbol_name(&out, copy);
    text_appendf(&out, ".%.*s", (int)name->length, name->text);
    text = arena_alloc(k->c->arena, out.length + 1);
    memcpy(text, text_cstr(&out), out.length + 1);
    text_free(&out);
    made.text = text;
    made.length = strlen(text);
    return made;
}

/* The module of a generic of a library, or NULL for a generic of the
   module. The functions and the data of its copies carry its path. */
static const char *generic_home(const struct item *generic)
{
    const struct symbol *sym = generic->symbol;

    return sym != NULL && sym->home != NULL ? sym->home->module : NULL;
}

/* The copy of the function member of a generic, as a member of the copy
   item owner of the type copy. Its body is made later. */
static struct item *member_copy(struct copies *k, struct item *owner,
                                struct type *copy, struct item *member)
{
    struct item *n = arena_alloc(k->c->arena, sizeof *n);
    struct symbol *sym = arena_alloc(k->c->arena, sizeof *sym);
    struct generic_map map = sema_copy_map(copy);

    *n = *member;
    *sym = *member->symbol;
    sym->ir = 0;
    sym->item = n;
    sym->type = sema_subst(k->c, member->symbol->type, &map);
    sym->home = NULL;
    n->symbol = sym;
    n->owner = owner;
    n->home_module = owner->home_module;
    n->pub = member->pub;
    /* A public function of a copy that C knows by a name has the symbol
       `Name.f`, whose C symbol is `Name_f`, as one of an export class. */
    n->exported = owner->exported && member->pub;
    sym->exported = n->exported;
    sym->name = n->exported
                    ? types_member_symbol(k->c->arena, &copy->c_name, member)
                    : symbol_name(k, copy, &member->name);
    if (member->body != NULL) {
        n->body = NULL;
        add_work(k, member, n, &map);
    }
    return n;
}

/* The item of the concrete copy of a generic struct, class or variant,
   made on its first use. */
static struct item *type_item(struct copies *k, struct type *copy)
{
    struct item *made = map_get(&k->items, copy);
    struct item *generic;
    struct item **members;
    struct item **fns;
    struct symbol *sym;
    size_t count;
    size_t i;

    if (made != NULL) {
        return made;
    }
    generic = map_get(&k->generics, copy->generic);
    if (generic == NULL) {
        return NULL;
    }
    made = arena_alloc(k->c->arena, sizeof *made);
    *made = *generic;
    map_put(&k->items, copy, made);
    sym = arena_alloc(k->c->arena, sizeof *sym);
    *sym = *generic->symbol;
    sym->ir = 0;
    sym->item = made;
    sym->name = copy->name;
    sym->type = copy;
    sym->home = NULL;
    made->symbol = sym;
    made->name = copy->name;
    made->home_module = generic_home(generic);
    made->type_params = NULL;
    made->type_param_count = 0;
    /* A copy stands in no interface: each module that uses the generic
       makes its own. One that an `export type` names is an export class
       to C, which the module of that `type` defines. */
    made->pub = false;
    made->vis = VIS_PRIVATE;
    made->exported = copy->c_name.length > 0;
    if (made->exported) {
        made->home_module = NULL;
    }
    made->nested = NULL;
    made->nested_count = 0;
    /* A copy of a type nested in a generic class stands as an item of
       the module, which no pass after the checker leaves out. */
    made->outer = NULL;
    count = copy->member_count;
    members = types_alloc_array(k->c->arena, count + 1, sizeof *members);
    fns = types_alloc_array(k->c->arena, count + 1, sizeof *fns);
    made->members = fns;
    made->member_count = 0;
    for (i = 0; i < count; i++) {
        struct item *m = copy->generic->members[i];
        if (m->kind != ITEM_FN || m->symbol == NULL ||
            m->type_param_count > 0) {
            members[i] = m;
            continue;
        }
        members[i] = member_copy(k, made, copy, m);
        fns[made->member_count++] = members[i];
    }
    copy->members = members;
    /* A field of the copy is declared by the copy, whose symbols then
       name its own reach and thunks. */
    for (i = 0; i < copy->field_count; i++) {
        if (copy->fields[i].home == copy->generic) {
            copy->fields[i].home = copy;
        }
    }
    k->added = grow(k->added, &k->added_capacity, k->added_count,
                    sizeof *k->added);
    k->added[k->added_count++] = made;
    return made;
}

/* The name of a copy of the function generic, `max<int>`, with the
   modules of its arguments as the name of a copied type has them. */
static struct name copy_name(struct copies *k, const struct name *generic,
                             struct type *const *args,
                             const struct symbolic *const *values,
                             size_t count)
{
    struct text out = {0};
    struct name name;
    char *text;
    size_t i;

    text_appendf(&out, "%.*s<", (int)generic->length, generic->text);
    for (i = 0; i < count; i++) {
        text_append(&out, i > 0 ? ", " : "");
        if (values[i] != NULL) {
            symbolic_print(&out, values[i], true);
        } else {
            type_name_qualified(&out, args[i]);
        }
    }
    text_append(&out, ">");
    text = arena_alloc(k->c->arena, out.length + 1);
    memcpy(text, text_cstr(&out), out.length + 1);
    text_free(&out);
    name.text = text;
    name.length = strlen(text);
    return name;
}

/* The copy of the function generic with the arguments args and values,
   one per parameter of map, made once. name is the name of its symbol,
   owner the item of the class that holds it, or NULL. */
static struct item *copy_function(struct copies *k, struct item *generic,
                                  const struct generic_map *map,
                                  struct name name, struct item *owner)
{
    size_t count = map->count;
    struct item *n;
    struct symbol *sym;
    size_t i;
    size_t j;

    for (i = 0; i < k->fn_count; i++) {
        bool same = k->fns[i].generic == generic && k->fns[i].count == count;
        for (j = 0; same && j < count; j++) {
            same = k->fns[i].args[j] == map->args[j] &&
                   k->fns[i].values[j] == map->values[j];
        }
        if (same) {
            return k->fns[i].copy;
        }
    }
    n = arena_alloc(k->c->arena, sizeof *n);
    *n = *generic;
    sym = arena_alloc(k->c->arena, sizeof *sym);
    *sym = *generic->symbol;
    sym->ir = 0;
    sym->item = n;
    sym->name = name;
    sym->type = sema_subst(k->c, generic->symbol->type, map);
    sym->home = NULL;
    n->symbol = sym;
    n->name = owner != NULL ? generic->name : name;
    n->type_params = NULL;
    n->type_param_count = 0;
    n->pub = false;
    n->exported = false;
    n->body = NULL;
    if (owner != NULL) {
        /* A function of a class keeps its visibility, which decides the
           hidden lock of a synchronized class. It stands in no table and
           in no interface. */
        n->owner = owner;
        n->home_module = owner->home_module;
    } else {
        n->home_module = generic_home(generic);
        n->vis = VIS_PRIVATE;
    }
    k->fns = grow(k->fns, &k->fn_capacity, k->fn_count, sizeof *k->fns);
    k->fns[k->fn_count].generic = generic;
    k->fns[k->fn_count].args = map->args;
    k->fns[k->fn_count].values = map->values;
    k->fns[k->fn_count].count = count;
    k->fns[k->fn_count].copy = n;
    k->fn_count++;
    k->added = grow(k->added, &k->added_capacity, k->added_count,
                    sizeof *k->added);
    k->added[k->added_count++] = n;
    add_work(k, generic, n, map);
    return n;
}

static struct item *fn_copy(struct copies *k, struct item *generic,
                            struct type **args,
                            const struct symbolic **values)
{
    size_t count = generic->type_param_count;
    struct generic_map map;
    struct type **params;
    size_t i;

    params = types_alloc_array(k->c->arena, count + 1, sizeof *params);
    for (i = 0; i < count; i++) {
        params[i] = generic->type_params[i].type;
    }
    memset(&map, 0, sizeof map);
    map.types = k->c->types;
    map.params = params;
    map.args = args;
    map.values = values;
    map.count = count;
    return copy_function(k, generic, &map,
                         copy_name(k, &generic->name, args, values, count),
                         NULL);
}

/* The copy of the generic g in the chain of t, a class, a struct or a
   pointer to one, or NULL. */
static struct type *copy_in_chain(struct type *t, const struct type *g)
{
    if (t != NULL && t->kind == TYPE_POINTER) {
        t = t->element;
    }
    for (; t != NULL; t = t->kind == TYPE_CLASS ? t->base : NULL) {
        if (t->generic == g) {
            return t;
        }
    }
    return NULL;
}

/* The function of a copy that a call reaches without the arguments of
   its class: the `construct` a literal runs. The object it builds, or
   the receiver it passes, names the copy. */
static struct symbol *member_of_receiver(struct clone *cl, const struct expr *e,
                                         const struct item *it)
{
    struct type *g = it->owner->symbol->type;
    struct type *copy = copy_in_chain(
        ty(cl, (struct type *)e->as.call.builds, e->pos), g);
    struct item *made;
    size_t i;

    if (copy == NULL && e->as.call.arg_count > 0) {
        copy = copy_in_chain(ty(cl, e->as.call.args[0]->type, e->pos), g);
    }
    made = copy != NULL && !sema_has_params(copy)
               ? type_item(cl->k, copy)
               : NULL;
    for (i = 0; made != NULL && i < g->member_count; i++) {
        if (g->members[i] == it) {
            return copy->members[i]->symbol;
        }
    }
    copy_failed(cl, e->pos, &it->name, "since the call names no copy of it");
    return NULL;
}

/* DESIGN: a function of a class with type parameters of its own makes a
   copy per class and per argument of its own, `Box<int>.map<float>`. No
   table holds every such copy, so none stands in the table of its class
   or among its members. The copy is a function of the module that keeps
   its class as owner, and every call names it directly. args and values
   hold the arguments of the class first, then its own. */
static struct symbol *own_params_copy(struct clone *cl, const struct expr *e,
                                      struct item *it, struct type **args,
                                      const struct symbolic **values)
{
    struct copies *k = cl->k;
    struct item *owner = (struct item *)it->owner;
    struct type *g = owner->symbol->type;
    size_t outer = owner->type_param_count;
    size_t own = it->type_param_count;
    struct type **params;
    struct generic_map map;
    struct text out = {0};
    struct name name;
    struct name own_name;
    char *text;
    size_t i;

    if (e->as.call.copy_count != outer + own) {
        copy_failed(cl, e->pos, &it->name, "since the call names no copy of it");
        return NULL;
    }
    params = types_alloc_array(k->c->arena, outer + own + 1, sizeof *params);
    for (i = 0; i < outer; i++) {
        params[i] = g->type_params[i];
    }
    for (i = 0; i < own; i++) {
        params[outer + i] = it->type_params[i].type;
    }
    memset(&map, 0, sizeof map);
    map.types = k->c->types;
    map.params = params;
    map.args = args;
    map.values = values;
    map.count = outer + own;
    own_name = copy_name(k, &it->name, args + outer, values + outer, own);
    if (outer > 0) {
        struct type *copy = sema_copy_named(k->c, g, args, values);
        owner = type_item(k, copy);
        if (owner == NULL) {
            copy_failed(cl, e->pos, &it->name,
                        "since its class is nested in a generic");
            return NULL;
        }
        map.from = g;
        map.to = copy;
        type_symbol_name(&out, copy);
    } else {
        type_symbol_name(&out, g);
    }
    text_appendf(&out, ".%.*s", (int)own_name.length, own_name.text);
    text = arena_alloc(k->c->arena, out.length + 1);
    memcpy(text, text_cstr(&out), out.length + 1);
    text_free(&out);
    name.text = text;
    name.length = strlen(text);
    return copy_function(k, it, &map, name, owner)->symbol;
}

/* The symbol of the copy that the call e of the generic function sym
   reaches, from the arguments the checker recorded on e. NULL when sym
   is no generic. */
static struct symbol *callee_copy(struct clone *cl, const struct expr *e,
                                  struct symbol *sym)
{
    struct copies *k = cl->k;
    struct item *it = sym != NULL ? sym->item : NULL;
    const struct item *owner = it != NULL ? it->owner : NULL;
    struct type **args;
    const struct symbolic **values;
    size_t count = e->as.call.copy_count;
    size_t i;

    if (it == NULL || sym->kind != SYMBOL_FN ||
        (it->type_param_count == 0 &&
         (owner == NULL || owner->type_param_count == 0))) {
        return NULL;
    }
    if (count == 0 && owner != NULL && owner->type_param_count > 0 &&
        it->type_param_count == 0) {
        return member_of_receiver(cl, e, it);
    }
    if (count == 0) {
        copy_failed(cl, e->pos, &sym->name, "since the call names no copy of it");
        return NULL;
    }
    args = types_alloc_array(k->c->arena, count + 1, sizeof *args);
    values = types_alloc_array(k->c->arena, count + 1, sizeof *values);
    for (i = 0; i < count; i++) {
        args[i] = ty(cl, e->as.call.copy_args[i], e->pos);
        values[i] = e->as.call.copy_values[i] != NULL
                        ? sema_subst_symbolic(k->c, e->as.call.copy_values[i],
                                              &cl->map)
                        : NULL;
        if ((args[i] != NULL && sema_has_params(args[i])) ||
            (values[i] != NULL && values[i]->kind == SYMBOLIC_PARAM)) {
            copy_failed(cl, e->pos, &sym->name,
                        "since the call names no copy of it");
            return NULL;
        }
    }
    if (owner != NULL && it->type_param_count > 0) {
        return own_params_copy(cl, e, it, args, values);
    }
    if (owner != NULL && owner->type_param_count > 0) {
        struct type *g = owner->symbol->type;
        struct type *copy;
        struct item *made;
        copy = sema_copy_named(k->c, g, args, values);
        made = type_item(k, copy);
        if (made == NULL) {
            copy_failed(cl, e->pos, &sym->name,
                        "since its class is nested in a generic");
            return NULL;
        }
        for (i = 0; i < g->member_count; i++) {
            if (g->members[i] == it) {
                return copy->members[i]->symbol;
            }
        }
        copy_failed(cl, e->pos, &sym->name,
                    "since the copy of its class lacks it");
        return NULL;
    }
    return fn_copy(k, it, args, values)->symbol;
}

/* Whether the callee of a call names its function directly: a name, or
   `T.f` for a function of the body of a type, which takes no `self`. */
static bool direct_callee(const struct expr *callee)
{
    return callee->kind == EXPR_NAME ||
           (callee->kind == EXPR_FIELD && callee->symbol != NULL &&
            callee->symbol->kind == SYMBOL_FN);
}

/* Whether sym names a generic function or a function of a generic type,
   which no copy may reach but through a call. */
static bool names_generic(const struct symbol *sym)
{
    const struct item *it = sym != NULL ? sym->item : NULL;

    return it != NULL && (sym->kind == SYMBOL_FN) &&
           (it->type_param_count > 0 ||
            (it->owner != NULL && it->owner->type_param_count > 0));
}

/* Trees */

static struct expr *xe(struct clone *cl, struct expr *e);
static struct stmt *xs(struct clone *cl, struct stmt *s);
static struct block *xb(struct clone *cl, struct block *b);
static struct item *xi(struct clone *cl, struct item *it);

static struct expr **xlist(struct clone *cl, struct expr **list, size_t count)
{
    struct expr **n = list;
    size_t i;

    if (list == NULL) {
        return NULL;
    }
    if (cl->fresh) {
        n = types_alloc_array(cl->k->c->arena, count + 1, sizeof *n);
    }
    for (i = 0; i < count; i++) {
        n[i] = xe(cl, list[i]);
    }
    return n;
}

static struct field_init *xinits(struct clone *cl, struct field_init *list,
                                 size_t count)
{
    struct field_init *n = list;
    size_t i;

    if (list == NULL) {
        return NULL;
    }
    if (cl->fresh) {
        n = types_alloc_array(cl->k->c->arena, count + 1, sizeof *n);
        memcpy(n, list, count * sizeof *n);
    }
    for (i = 0; i < count; i++) {
        n[i].value = xe(cl, list[i].value);
    }
    return n;
}

static void xh(struct clone *cl, struct handler *n, const struct handler *h)
{
    n->body = xb(cl, h->body);
    n->symbol = xsym(cl, h->symbol);
}

static void xiter(struct clone *cl, struct iteration *n,
                  const struct iteration *it)
{
    n->cursor = xsym(cl, it->cursor);
    n->start = xe(cl, it->start);
    n->advance = xe(cl, it->advance);
    n->current = xe(cl, it->current);
    n->place = xe(cl, it->place);
    n->changed = xe(cl, it->changed);
    n->change_file = xe(cl, it->change_file);
    n->change_file_length = xe(cl, it->change_file_length);
    n->change_line = xe(cl, it->change_line);
}

static struct binding *xbind(struct clone *cl, struct binding *list,
                             size_t count)
{
    struct binding *n = list;
    size_t i;

    if (list == NULL) {
        return NULL;
    }
    if (cl->fresh) {
        n = types_alloc_array(cl->k->c->arena, count + 1, sizeof *n);
        memcpy(n, list, count * sizeof *n);
    }
    for (i = 0; i < count; i++) {
        n[i].symbol = xsym(cl, list[i].symbol);
    }
    return n;
}

/* The copy's calls of generics that the checker wrote into e when it
   read e again. Such a call names an `operator fn` or a hook of a
   copy. */
static void redirect(struct clone *cl, struct expr *e);

static void redirect_list(struct clone *cl, struct expr **list, size_t count)
{
    size_t i;

    for (i = 0; list != NULL && i < count; i++) {
        redirect(cl, list[i]);
    }
}

static void redirect(struct clone *cl, struct expr *e)
{
    struct symbol *copy;

    if (e == NULL || e->prechecked) {
        return;
    }
    switch (e->kind) {
    case EXPR_CALL:
        redirect(cl, e->as.call.callee);
        redirect_list(cl, e->as.call.args, e->as.call.arg_count);
        redirect_list(cl, e->as.call.hash_calls, e->as.call.hash_count);
        if (direct_callee(e->as.call.callee) &&
            (copy = callee_copy(cl, e, e->as.call.callee->symbol)) != NULL) {
            e->as.call.callee->symbol = copy;
        }
        break;
    case EXPR_UNARY:
        redirect(cl, e->as.unary.operand);
        break;
    case EXPR_BINARY:
        redirect(cl, e->as.binary.left);
        redirect(cl, e->as.binary.right);
        break;
    case EXPR_FIELD:
        redirect(cl, e->as.field.base);
        break;
    case EXPR_INDEX:
        redirect(cl, e->as.index.base);
        redirect(cl, e->as.index.index);
        break;
    default:
        break;
    }
}

/* Read e again, whose operands the copy has made, with the types of the
   arguments. */
static struct type *recheck(struct clone *cl, struct expr *e)
{
    struct type *t;

    e->type = NULL;
    t = sema_check_expr(cl->k->c, e, NULL);
    if (sema_is_error(t)) {
        cl->k->failed = true;
        return t;
    }
    redirect(cl, e);
    return t;
}

static void mark(struct expr *e)
{
    if (e != NULL) {
        e->prechecked = true;
    }
}

static bool is_param(const struct type *t)
{
    return t != NULL && t->kind == TYPE_PARAM;
}

/* The receiver of a call of a function of an interface that constrains
   a type parameter. It stands as its argument now, which reaches the
   interface as any class value does. */
static struct expr *receiver(struct clone *cl, struct expr *n,
                             const struct expr *e)
{
    struct checker *c = cl->k->c;
    struct type *got = ty(cl, e->param_type, e->pos);
    struct type *want = n->type;
    struct expr *value = n;

    n->type = got;
    if (got == NULL || want == NULL || sema_is_error(got)) {
        return n;
    }
    if (got->kind != TYPE_POINTER) {
        value = sema_new_node(c, EXPR_UNARY, n->pos);
        value->as.unary.op = TOKEN_AMP;
        value->as.unary.operand = n;
        value->type = types_pointer(c->types, got);
        sema_mark_address_taken(c, n);
        got = value->type;
    }
    if (!sema_require(c, value, got, want)) {
        cl->k->failed = true;
    }
    return value;
}

/* Whether e means what the argument of a type parameter makes of it. An
   operator and `e[i]` on a value of one do, and so does `x.hash()` on a
   value of a type that names one. */
static bool open_node(const struct expr *e)
{
    switch (e->kind) {
    case EXPR_CALL:
        return sema_is_hash_call(e) &&
               sema_has_params(e->as.call.callee->as.field.base->type);
    case EXPR_UNARY:
        return is_param(e->as.unary.operand->type) &&
               (e->as.unary.op == TOKEN_MINUS || e->as.unary.op == TOKEN_TILDE);
    case EXPR_BINARY:
        return is_param(e->as.binary.left->type) ||
               is_param(e->as.binary.right->type);
    case EXPR_INDEX:
        return is_param(e->as.index.base->type);
    default:
        return false;
    }
}

static struct expr *xe(struct clone *cl, struct expr *e)
{
    struct expr *n;
    size_t i;

    if (e == NULL) {
        return NULL;
    }
    if (cl->fresh) {
        n = map_get(&cl->nodes, e);
        if (n != NULL) {
            return n;
        }
        n = arena_alloc(cl->k->c->arena, sizeof *n);
        *n = *e;
        map_put(&cl->nodes, e, n);
    } else {
        n = e;
    }
    /* A node the copy reads again takes its type from that reading. */
    n->type = cl->fresh && open_node(e) ? NULL : ty(cl, e->type, e->pos);
    n->symbol = xsym(cl, e->symbol);
    n->to_iface = xf(cl, e->type, e->to_iface);
    n->to_optional = ty(cl, e->to_optional, e->pos);
    switch (e->kind) {
    case EXPR_NAME:
        if (names_generic(n->symbol) && !cl->callee) {
            copy_failed(cl, e->pos, &n->symbol->name,
                        "since it is named as a value and not called");
        }
        break;
    case EXPR_UNARY:
        n->as.unary.operand = xe(cl, e->as.unary.operand);
        if (cl->fresh && open_node(e)) {
            mark(n->as.unary.operand);
            recheck(cl, n);
        }
        break;
    case EXPR_BINARY:
        n->as.binary.left = xe(cl, e->as.binary.left);
        n->as.binary.right = xe(cl, e->as.binary.right);
        if (cl->fresh && open_node(e)) {
            mark(n->as.binary.left);
            mark(n->as.binary.right);
            recheck(cl, n);
        }
        break;
    case EXPR_CAST:
        n->as.cast.operand = xe(cl, e->as.cast.operand);
        n->as.cast.type = xt(cl, e->as.cast.type);
        n->as.cast.target =
            ty(cl, (struct type *)e->as.cast.target, e->pos);
        break;
    case EXPR_CALL: {
        struct symbol *copy;
        cl->callee = e->as.call.callee->kind == EXPR_NAME;
        n->as.call.callee = xe(cl, e->as.call.callee);
        cl->callee = false;
        n->as.call.args = xlist(cl, e->as.call.args, e->as.call.arg_count);
        if (e->as.call.arg_count > 0 && e->as.call.args[0]->param_type != NULL &&
            cl->fresh) {
            n->as.call.args[0] = receiver(cl, n->as.call.args[0],
                                          e->as.call.args[0]);
        }
        n->as.call.dispatch =
            ty(cl, (struct type *)e->as.call.dispatch, e->pos);
        xh(cl, &n->as.call.handler, &e->as.call.handler);
        n->as.call.out = e->as.call.out == e ? n : xe(cl, e->as.call.out);
        n->as.call.builds = ty(cl, (struct type *)e->as.call.builds, e->pos);
        if (direct_callee(n->as.call.callee) &&
            (copy = callee_copy(cl, e, n->as.call.callee->symbol)) != NULL) {
            n->as.call.callee->symbol = copy;
        }
        if (cl->fresh) {
            n->as.call.copy_args = NULL;
            n->as.call.copy_values = NULL;
            n->as.call.copy_count = 0;
        }
        /* The default hash of a type that names a parameter is read
           again, and finds the functions of the types of the argument. */
        if (cl->fresh && open_node(e)) {
            mark(n->as.call.callee->as.field.base);
            n->as.call.hashes = false;
            n->as.call.hash_calls = NULL;
            n->as.call.hash_count = 0;
            recheck(cl, n);
        } else {
            n->as.call.hash_calls =
                xlist(cl, e->as.call.hash_calls, e->as.call.hash_count);
        }
        break;
    }
    case EXPR_INDEX:
        n->as.index.base = xe(cl, e->as.index.base);
        n->as.index.index = xe(cl, e->as.index.index);
        if (cl->fresh && open_node(e)) {
            struct type *t;
            mark(n->as.index.base);
            mark(n->as.index.index);
            t = recheck(cl, n);
            map_add(cl, e->as.index.base->type->indexed, t);
        }
        break;
    case EXPR_SLICE:
        n->as.slice.base = xe(cl, e->as.slice.base);
        n->as.slice.low = xe(cl, e->as.slice.low);
        n->as.slice.high = xe(cl, e->as.slice.high);
        break;
    case EXPR_FIELD:
        n->as.field.base = xe(cl, e->as.field.base);
        n->as.field.through =
            xf(cl, e->as.field.base->type, e->as.field.through);
        break;
    case EXPR_STRUCT_LIT:
        n->as.struct_lit.fields = xinits(cl, e->as.struct_lit.fields,
                                         e->as.struct_lit.field_count);
        break;
    case EXPR_TUPLE:
        n->as.tuple.elements =
            xlist(cl, e->as.tuple.elements, e->as.tuple.count);
        break;
    case EXPR_SLICE_LIT:
        n->as.slice_lit.element = xt(cl, e->as.slice_lit.element);
        n->as.slice_lit.fields = xinits(cl, e->as.slice_lit.fields,
                                        e->as.slice_lit.field_count);
        break;
    case EXPR_ARRAY_LIT:
        n->as.array_lit.elements =
            xlist(cl, e->as.array_lit.elements, e->as.array_lit.count);
        break;
    case EXPR_ARRAY_REPEAT:
        n->as.array_repeat.value = xe(cl, e->as.array_repeat.value);
        n->as.array_repeat.count = xe(cl, e->as.array_repeat.count);
        break;
    case EXPR_ALLOC:
        n->as.alloc.type = xt(cl, e->as.alloc.type);
        n->as.alloc.count = xe(cl, e->as.alloc.count);
        n->as.alloc.value = xe(cl, e->as.alloc.value);
        break;
    case EXPR_FREE:
        n->as.free_pointer = xe(cl, e->as.free_pointer);
        break;
    case EXPR_OBJECT:
        n->as.object.operand = xe(cl, e->as.object.operand);
        n->as.object.from = xe(cl, e->as.object.from);
        break;
    case EXPR_ATOMIC:
        n->as.atomic.place = xe(cl, e->as.atomic.place);
        n->as.atomic.a = xe(cl, e->as.atomic.a);
        n->as.atomic.b = xe(cl, e->as.atomic.b);
        break;
    case EXPR_SIZE_OF:
        n->as.size_of = xt(cl, e->as.size_of);
        break;
    case EXPR_PARALLEL:
        n->as.parallel.array = xe(cl, e->as.parallel.array);
        n->as.parallel.chunks = xe(cl, e->as.parallel.chunks);
        n->as.parallel.call = xe(cl, e->as.parallel.call);
        break;
    case EXPR_DISPATCH:
        n->as.dispatch.object = xe(cl, e->as.dispatch.object);
        n->as.dispatch.call = xe(cl, e->as.dispatch.call);
        break;
    case EXPR_JOIN:
        n->as.join.job = xe(cl, e->as.join.job);
        break;
    case EXPR_FORMAT:
        if (cl->fresh) {
            n->as.format.parts = types_alloc_array(
                cl->k->c->arena, e->as.format.count + 1,
                sizeof *n->as.format.parts);
            memcpy(n->as.format.parts, e->as.format.parts,
                   e->as.format.count * sizeof *n->as.format.parts);
        }
        for (i = 0; i < e->as.format.count; i++) {
            const struct format_part *p = &e->as.format.parts[i];
            n->as.format.parts[i].value = xe(cl, p->value);
            n->as.format.parts[i].bound = xsym(cl, p->bound);
            n->as.format.parts[i].text_call = xe(cl, p->text_call);
            n->as.format.parts[i].value_call = xe(cl, p->value_call);
        }
        n->as.format.builder = xsym(cl, e->as.format.builder);
        n->as.format.start = xe(cl, e->as.format.start);
        n->as.format.take = xe(cl, e->as.format.take);
        break;
    case EXPR_IN:
        n->as.in.value = xe(cl, e->as.in.value);
        n->as.in.low = xe(cl, e->as.in.low);
        n->as.in.high = xe(cl, e->as.in.high);
        n->as.in.bound = xsym(cl, e->as.in.bound);
        n->as.in.test = xe(cl, e->as.in.test);
        break;
    case EXPR_OPTIONAL:
        n->as.optional.base = xe(cl, e->as.optional.base);
        n->as.optional.bound = xsym(cl, e->as.optional.bound);
        n->as.optional.access = xe(cl, e->as.optional.access);
        break;
    case EXPR_SYNC_OP:
        n->as.sync_op.target = xe(cl, e->as.sync_op.target);
        n->as.sync_op.value = xe(cl, e->as.sync_op.value);
        n->as.sync_op.element = xt(cl, e->as.sync_op.element);
        break;
    case EXPR_SIMD:
        n->as.simd.args = xlist(cl, e->as.simd.args, e->as.simd.arg_count);
        n->as.simd.simd = ty(cl, (struct type *)e->as.simd.simd, e->pos);
        break;
    case EXPR_DESCRIPTOR:
        n->as.descriptor_of =
            ty(cl, (struct type *)e->as.descriptor_of, e->pos);
        break;
    case EXPR_COLLECT:
        xiter(cl, &n->as.collect, &e->as.collect);
        break;
    case EXPR_FN:
        n->as.fn = xi(cl, e->as.fn);
        break;
    default:
        break;
    }
    return n;
}

static struct stmt *xs(struct clone *cl, struct stmt *s)
{
    struct stmt *n;
    size_t i;

    if (s == NULL) {
        return NULL;
    }
    if (cl->fresh) {
        n = arena_alloc(cl->k->c->arena, sizeof *n);
        *n = *s;
    } else {
        n = s;
    }
    switch (s->kind) {
    case STMT_LET:
    case STMT_CONST:
        n->as.let.value = xe(cl, s->as.let.value);
        n->as.let.type = xt(cl, s->as.let.type);
        n->as.let.symbol = xsym(cl, s->as.let.symbol);
        n->as.let.otherwise = xb(cl, s->as.let.otherwise);
        xh(cl, &n->as.let.guard, &s->as.let.guard);
        n->as.let.names = xbind(cl, s->as.let.names, s->as.let.name_count);
        break;
    case STMT_EXPR:
        n->as.expr = xe(cl, s->as.expr);
        break;
    case STMT_ASSIGN: {
        struct expr *target = s->as.assign.target;
        if (target->kind == EXPR_INDEX && cl->fresh && open_node(target)) {
            /* `e[i] = v` writes through `set_index`, so the target is
               not read as `e[i]` is. */
            struct expr *t = arena_alloc(cl->k->c->arena, sizeof *t);
            *t = *target;
            t->type = NULL;
            t->as.index.base = xe(cl, target->as.index.base);
            t->as.index.index = xe(cl, target->as.index.index);
            n->as.assign.target = t;
        } else {
            n->as.assign.target = xe(cl, target);
        }
        n->as.assign.value = xe(cl, s->as.assign.value);
        if (target->kind == EXPR_INDEX && cl->fresh && open_node(target)) {
            struct expr *t = n->as.assign.target;
            mark(t->as.index.base);
            mark(t->as.index.index);
            mark(n->as.assign.value);
            if (sema_set_index(cl->k->c, n)) {
                if (n->kind == STMT_EXPR) {
                    redirect(cl, n->as.expr);
                }
            } else {
                t->prechecked = false;
                recheck(cl, t);
            }
        }
        break;
    }
    case STMT_IF:
        if (cl->fresh) {
            n->as.if_chain.branches = types_alloc_array(
                cl->k->c->arena, s->as.if_chain.count + 1,
                sizeof *n->as.if_chain.branches);
        }
        for (i = 0; i < s->as.if_chain.count; i++) {
            n->as.if_chain.branches[i].cond =
                xe(cl, s->as.if_chain.branches[i].cond);
            n->as.if_chain.branches[i].body =
                xb(cl, s->as.if_chain.branches[i].body);
        }
        n->as.if_chain.else_body = xb(cl, s->as.if_chain.else_body);
        break;
    case STMT_WHILE:
    case STMT_DO_WHILE:
        n->as.loop.cond = xe(cl, s->as.loop.cond);
        n->as.loop.body = xb(cl, s->as.loop.body);
        break;
    case STMT_FOR: {
        struct expr *over = s->as.for_loop.over;
        n->as.for_loop.low = xe(cl, s->as.for_loop.low);
        n->as.for_loop.high = xe(cl, s->as.for_loop.high);
        n->as.for_loop.over = xe(cl, over);
        n->as.for_loop.step = xe(cl, s->as.for_loop.step);
        if (over != NULL && is_param(over->type) && cl->fresh) {
            struct scope scope;
            struct type *element = NULL;
            struct type *t = n->as.for_loop.over->type;
            mark(n->as.for_loop.over);
            memset(&n->as.for_loop.hooks, 0, sizeof n->as.for_loop.hooks);
            sema_enter_scope(cl->k->c, &scope);
            if (t->kind == TYPE_SLICE || t->kind == TYPE_ARRAY) {
                element = t->element;
            } else if (!sema_iterate(cl->k->c, n->as.for_loop.over, t,
                                     &n->as.for_loop.hooks, &element)) {
                cl->k->failed = true;
            }
            sema_leave_scope(cl->k->c, &scope);
            redirect(cl, n->as.for_loop.hooks.start);
            redirect(cl, n->as.for_loop.hooks.advance);
            redirect(cl, n->as.for_loop.hooks.current);
            redirect(cl, n->as.for_loop.hooks.place);
            redirect(cl, n->as.for_loop.hooks.changed);
            redirect(cl, n->as.for_loop.hooks.change_file);
            redirect(cl, n->as.for_loop.hooks.change_file_length);
            redirect(cl, n->as.for_loop.hooks.change_line);
            map_add(cl, over->type->walked, element);
        } else {
            xiter(cl, &n->as.for_loop.hooks, &s->as.for_loop.hooks);
        }
        n->as.for_loop.names =
            xbind(cl, s->as.for_loop.names, s->as.for_loop.name_count);
        n->as.for_loop.body = xb(cl, s->as.for_loop.body);
        break;
    }
    case STMT_DEFER:
    case STMT_UNDO:
        n->as.deferred = xs(cl, s->as.deferred);
        break;
    case STMT_FAIL:
        n->as.fail.value = xe(cl, s->as.fail.value);
        break;
    case STMT_SWITCH:
        n->as.switch_stmt.value = xe(cl, s->as.switch_stmt.value);
        if (cl->fresh) {
            n->as.switch_stmt.arms = types_alloc_array(
                cl->k->c->arena, s->as.switch_stmt.count + 1,
                sizeof *n->as.switch_stmt.arms);
            memcpy(n->as.switch_stmt.arms, s->as.switch_stmt.arms,
                   s->as.switch_stmt.count * sizeof *n->as.switch_stmt.arms);
        }
        for (i = 0; i < s->as.switch_stmt.count; i++) {
            const struct switch_arm *a = &s->as.switch_stmt.arms[i];
            n->as.switch_stmt.arms[i].value = xe(cl, a->value);
            n->as.switch_stmt.arms[i].bound = xsym(cl, a->bound);
            n->as.switch_stmt.arms[i].test = xe(cl, a->test);
            n->as.switch_stmt.arms[i].body = xs(cl, a->body);
        }
        n->as.switch_stmt.otherwise = xs(cl, s->as.switch_stmt.otherwise);
        n->as.switch_stmt.bound = xsym(cl, s->as.switch_stmt.bound);
        break;
    case STMT_ASSERT:
        n->as.assertion.cond = xe(cl, s->as.assertion.cond);
        break;
    case STMT_RETURN:
        n->as.return_value = xe(cl, s->as.return_value);
        break;
    case STMT_YIELD:
        n->as.yielded = xe(cl, s->as.yielded);
        break;
    case STMT_TRY:
        n->as.try_block.body = xb(cl, s->as.try_block.body);
        xh(cl, &n->as.try_block.handler, &s->as.try_block.handler);
        break;
    case STMT_BLOCK:
        n->as.block = xb(cl, s->as.block);
        break;
    case STMT_SYNC:
        n->as.sync.mutex = xe(cl, s->as.sync.mutex);
        n->as.sync.body = xb(cl, s->as.sync.body);
        break;
    case STMT_SELECT:
        if (cl->fresh) {
            n->as.select.arms = types_alloc_array(
                cl->k->c->arena, s->as.select.count + 1,
                sizeof *n->as.select.arms);
            memcpy(n->as.select.arms, s->as.select.arms,
                   s->as.select.count * sizeof *n->as.select.arms);
        }
        for (i = 0; i < s->as.select.count; i++) {
            const struct switch_arm *a = &s->as.select.arms[i];
            n->as.select.arms[i].value = xe(cl, a->value);
            n->as.select.arms[i].bound = xsym(cl, a->bound);
            n->as.select.arms[i].test = xe(cl, a->test);
            n->as.select.arms[i].body = xs(cl, a->body);
        }
        break;
    case STMT_BREAK:
    case STMT_CONTINUE:
    case STMT_FALLTHROUGH:
        break;
    }
    return n;
}

static struct block *xb(struct clone *cl, struct block *b)
{
    struct block *n;
    size_t i;

    if (b == NULL) {
        return NULL;
    }
    if (cl->fresh) {
        n = arena_alloc(cl->k->c->arena, sizeof *n);
        *n = *b;
        n->stmts = types_alloc_array(cl->k->c->arena, b->count + 1,
                                     sizeof *n->stmts);
    } else {
        n = b;
    }
    for (i = 0; i < b->count; i++) {
        n->stmts[i] = xs(cl, b->stmts[i]);
    }
    return n;
}

/* The parameters and the symbols of the function it in its copy n. */
static void xparams(struct clone *cl, struct item *n, const struct item *it)
{
    size_t i;

    if (!cl->fresh) {
        return;
    }
    if (it->param_count > 0) {
        n->params = types_alloc_array(cl->k->c->arena, it->param_count + 1,
                                      sizeof *n->params);
        memcpy(n->params, it->params, it->param_count * sizeof *n->params);
        for (i = 0; i < it->param_count; i++) {
            n->params[i].symbol = xsym(cl, it->params[i].symbol);
            n->params[i].type = xt(cl, it->params[i].type);
        }
    }
    n->self = xsym(cl, it->self);
    n->result = xt(cl, it->result);
}

/* An anonymous function of the body. Its copy has a symbol, parameters
   and captures of its own. */
static struct item *xi(struct clone *cl, struct item *it)
{
    struct item *n;
    size_t i;

    if (it == NULL) {
        return NULL;
    }
    if (!cl->fresh) {
        it->body = xb(cl, it->body);
        return it;
    }
    n = map_get(&cl->nodes, it);
    if (n != NULL) {
        return n;
    }
    n = arena_alloc(cl->k->c->arena, sizeof *n);
    *n = *it;
    map_put(&cl->nodes, it, n);
    if (it->symbol != NULL) {
        struct symbol *sym = arena_alloc(cl->k->c->arena, sizeof *sym);
        *sym = *it->symbol;
        sym->ir = 0;
        sym->item = n;
        sym->type = ty(cl, it->symbol->type, it->pos);
        n->symbol = sym;
    }
    if (it->enclosing != NULL && map_get(&cl->nodes, it->enclosing) != NULL) {
        n->enclosing = map_get(&cl->nodes, it->enclosing);
    }
    if (it->capture_count > 0) {
        n->captures = types_alloc_array(cl->k->c->arena, it->capture_count + 1,
                                        sizeof *n->captures);
        memcpy(n->captures, it->captures,
               it->capture_count * sizeof *n->captures);
        n->capture_capacity = it->capture_count;
        for (i = 0; i < it->capture_count; i++) {
            n->captures[i].symbol = xsym(cl, it->captures[i].symbol);
        }
    }
    xparams(cl, n, it);
    n->body = xb(cl, it->body);
    return n;
}

/* The passes */

static void make_body(struct copies *k, struct work *w)
{
    struct checker *c = k->c;
    struct item *outer_function = c->function;
    struct scope scope;
    struct clone cl;

    memset(&cl, 0, sizeof cl);
    cl.k = k;
    cl.map = w->map;
    cl.map_capacity = 0;
    cl.fresh = true;
    cl.generic = w->from->owner != NULL ? w->from->owner : w->from;
    map_put(&cl.nodes, w->from, w->to);
    xparams(&cl, w->to, w->from);
    c->function = w->to;
    sema_enter_scope(c, &scope);
    w->to->body = xb(&cl, w->from->body);
    sema_leave_scope(c, &scope);
    c->function = outer_function;
    map_free(&cl.nodes);
}

/* The calls of generics in code that is not generic name their copies. */
static void walk_plain(struct copies *k, struct item *it)
{
    struct clone cl;
    struct item *outer_function = k->c->function;

    if (it->body == NULL || it->type_param_count > 0) {
        return;
    }
    memset(&cl, 0, sizeof cl);
    cl.k = k;
    cl.fresh = false;
    k->c->function = it;
    xb(&cl, it->body);
    k->c->function = outer_function;
}

static bool generic_type_item(const struct item *it)
{
    return it->type_param_count > 0 && it->symbol != NULL &&
           it->symbol->type != NULL &&
           (it->kind == ITEM_STRUCT || it->kind == ITEM_CLASS ||
            it->kind == ITEM_VARIANT);
}

/* Make the items of the concrete copies of the generic type item that
   have none yet. Returns whether it made one. */
static bool items_of_copies(struct copies *k, const struct item *it)
{
    struct type *copy;
    bool made = false;

    if (!generic_type_item(it)) {
        return false;
    }
    for (copy = it->symbol->type->copies; copy != NULL;
         copy = copy->next_copy) {
        if (!sema_has_params(copy) && map_get(&k->items, copy) == NULL) {
            type_item(k, copy);
            made = true;
        }
    }
    return made;
}

/* Whether a concrete copy of a generic of the module or of a library had
   no item yet. Makes the items of those that had none. A copy of a
   generic of a library is made here as well. No module but the one that
   uses it need have compiled it. */
static bool copies_without_items(struct copies *k)
{
    struct module *module = k->c->module;
    bool made = false;
    size_t i;
    size_t j;

    for (i = 0; i < module->item_count; i++) {
        made = items_of_copies(k, module->items[i]) || made;
    }
    for (i = 0; i < k->c->library_count; i++) {
        const struct interface *lib = k->c->libraries[i];
        for (j = 0; j < lib->generic_count; j++) {
            made = items_of_copies(k, lib->generics[j]) || made;
        }
    }
    return made;
}

void sema_compile_copies(struct checker *c)
{
    struct module *module = c->module;
    struct copies k;
    struct item **items;
    size_t done = 0;
    size_t count = module->item_count;
    size_t i;
    size_t j;

    memset(&k, 0, sizeof k);
    k.c = c;
    for (i = 0; i < count; i++) {
        struct item *it = module->items[i];
        if (generic_type_item(it)) {
            map_put(&k.generics, it->symbol->type, it);
        }
    }
    for (i = 0; i < c->library_count; i++) {
        const struct interface *lib = c->libraries[i];
        for (j = 0; j < lib->generic_count; j++) {
            struct item *it = lib->generics[j];
            if (generic_type_item(it)) {
                map_put(&k.generics, it->symbol->type, it);
            }
        }
    }
    for (i = 0; i < count; i++) {
        struct item *it = module->items[i];
        bool generic = it->type_param_count > 0;
        const struct item *w;
        for (w = it->outer; w != NULL; w = w->outer) {
            generic = generic || w->type_param_count > 0;
        }
        if (generic) {
            continue;
        }
        if (it->kind == ITEM_FN) {
            walk_plain(&k, it);
        }
        for (j = 0; j < it->member_count; j++) {
            if (it->members[j]->kind == ITEM_FN) {
                walk_plain(&k, it->members[j]);
            }
        }
    }
    do {
        while (done < k.work_count && !k.failed) {
            struct work w = k.work[done++];
            make_body(&k, &w);
        }
    } while (!k.failed && copies_without_items(&k));
    if (!k.failed && k.added_count > 0) {
        items = types_alloc_array(c->arena, count + k.added_count + 1,
                                  sizeof *items);
        memcpy(items, module->items, count * sizeof *items);
        memcpy(items + count, k.added, k.added_count * sizeof *items);
        module->items = items;
        module->item_count = count + k.added_count;
    }
    if (k.failed) {
        c->ok = false;
    }
    free(k.fns);
    free(k.work);
    free(k.added);
    map_free(&k.generics);
    map_free(&k.items);
}

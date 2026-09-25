#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sema_checker.h"

/* DESIGN: the checker's part of generics. A generic declares its type
   parameters, each a TYPE_PARAM that meets the hooks and the interfaces
   of its constraints. The body is checked once, against the parameters,
   so an operation the constraints do not give is an error where the
   generic is written. A use names a copy, whose arguments are checked
   against the constraints where it stands. A copy of a struct, a class
   or a variant is a type the checker builds by putting the arguments in
   place of the parameters. A call of a generic function puts them into
   its signature, from the written arguments or from the types of the
   values it is given. Compiling the copies is a later pass. The checker
   records the first use that needs one, and the driver refuses a build
   past the front end that has one. */

/* The hook table, in the order of the bits of a TYPE_PARAM. The first
   fourteen are the operators, then the five language hooks and `hash`. */
static const char *const hook_names[] = {
    "add", "sub", "mul", "div", "rem", "neg", "eq", "lt", "and", "or",
    "xor", "shl", "shr", "not", LANG_HOOK_ITER, LANG_HOOK_NEXT,
    LANG_HOOK_VALUE, LANG_HOOK_INDEX, LANG_HOOK_SET_INDEX, LANG_HOOK_HASH
};

#define HOOK_COUNT (sizeof hook_names / sizeof hook_names[0])

/* The hooks of `anti.lang.Number`. */
static const char *const number_hooks[] = {
    "add", "sub", "mul", "div", "neg", "lt"
};

/* The hooks of `anti.lang.Ordered`, which the source of `anti.lang`
   declares as well. */
static const char *const ordered_hooks[] = {"eq", "lt"};

static int hook_index(const struct name *name)
{
    size_t i;

    for (i = 0; i < HOOK_COUNT; i++) {
        if (sema_name_is(name, hook_names[i])) {
            return (int)i;
        }
    }
    return -1;
}

static struct type *copy_in_chain(struct type *t, const struct type *g);

static int hook_of(const char *text)
{
    struct name name;

    name.text = text;
    name.length = strlen(text);
    return hook_index(&name);
}

/* Declarations */

/* The type parameter or the constant parameter named name among the
   generics around the code being checked. Those are the function, the
   functions around an anonymous one and the classes around a type. */
struct symbol *sema_type_param_find(const struct checker *c,
                                    const struct name *name)
{
    const struct item *f;
    const struct item *w;
    size_t i;

    for (f = c->signature != NULL ? c->signature : c->function; f != NULL;
         f = f->enclosing) {
        for (i = 0; i < f->type_param_count; i++) {
            if (f->type_params[i].symbol != NULL &&
                sema_same_name(&f->type_params[i].name, name)) {
                return f->type_params[i].symbol;
            }
        }
    }
    for (w = c->within; w != NULL; w = w->outer) {
        for (i = 0; i < w->type_param_count; i++) {
            if (w->type_params[i].symbol != NULL &&
                sema_same_name(&w->type_params[i].name, name)) {
                return w->type_params[i].symbol;
            }
        }
    }
    return NULL;
}

/* Whether the code being checked stands inside a generic, where a use
   names a copy only once the generic has one. */
static bool in_generic(const struct checker *c)
{
    const struct item *f;
    const struct item *w;

    for (f = c->signature != NULL ? c->signature : c->function; f != NULL;
         f = f->enclosing) {
        if (f->type_param_count > 0) {
            return true;
        }
    }
    for (w = c->within; w != NULL; w = w->outer) {
        if (w->type_param_count > 0) {
            return true;
        }
    }
    return false;
}

/* Give each type parameter of it its type and its symbol. A constant
   parameter is a constant of type int whose value is the parameter. */
static void declare_params(struct checker *c, struct item *it)
{
    size_t i;
    size_t j;

    for (i = 0; i < it->type_param_count; i++) {
        struct type_param *tp = &it->type_params[i];
        struct symbol *sym = arena_alloc(c->arena, sizeof *sym);
        for (j = 0; j < i; j++) {
            if (sema_same_name(&it->type_params[j].name, &tp->name)) {
                sema_error_at(c, tp->pos, "`%.*s` names two type parameters "
                              "`%.*s`", (int)it->name.length, it->name.text,
                              (int)tp->name.length, tp->name.text);
            }
        }
        tp->type = types_param(c->types, tp->name);
        tp->type->param = tp;
        tp->type->declared_by = it;
        sym->name = tp->name;
        sym->pos = tp->pos;
        if (tp->constant) {
            struct symbolic key;
            struct const_value *v = arena_alloc(c->arena, sizeof *v);
            memset(&key, 0, sizeof key);
            key.kind = SYMBOLIC_PARAM;
            key.type = sema_builtin(c, TYPE_I64);
            key.of = tp->type;
            v->kind = CONST_SYMBOLIC;
            v->type = key.type;
            v->as.symbolic = types_symbolic(c->types, &key);
            sym->kind = SYMBOL_CONST;
            sym->type = key.type;
            sym->value = v;
            sym->state = EVAL_DONE;
        } else {
            sym->kind = SYMBOL_STRUCT;
            sym->type = tp->type;
        }
        tp->symbol = sym;
    }
}

void sema_declare_generics(struct checker *c)
{
    const struct module *module = c->module;
    size_t i;
    size_t j;

    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        if (it->symbol == NULL) {
            continue;
        }
        declare_params(c, it);
        if (it->type_param_count > 0 && it->symbol->type != NULL) {
            struct type *g = it->symbol->type;
            g->type_param_count = it->type_param_count;
            g->type_params = types_alloc_array(c->arena, it->type_param_count,
                                               sizeof *g->type_params);
            for (j = 0; j < it->type_param_count; j++) {
                g->type_params[j] = it->type_params[j].type;
            }
        }
        for (j = 0; j < it->member_count; j++) {
            declare_params(c, it->members[j]);
        }
        if (it->kind == ITEM_CONSTRAINT) {
            it->symbol->type = types_param(c->types, it->name);
            it->symbol->type->declared_by = it;
        }
    }
    /* A type nested in a generic class takes the parameters of the
       class, then its own. Each class stands after the types nested in
       it, so the walk from the end reaches a class first. */
    for (i = module->item_count; i-- > 0;) {
        struct item *it = module->items[i];
        const struct item *outer = it->outer;
        struct type_param *params;
        struct type *t;
        size_t count;
        if (it->symbol == NULL || it->symbol->type == NULL || outer == NULL ||
            outer->type_param_count == 0 ||
            (it->kind != ITEM_STRUCT && it->kind != ITEM_CLASS &&
             it->kind != ITEM_VARIANT)) {
            continue;
        }
        t = it->symbol->type;
        count = outer->type_param_count + it->type_param_count;
        params = types_alloc_array(c->arena, count + 1, sizeof *params);
        memcpy(params, outer->type_params,
               outer->type_param_count * sizeof *params);
        if (it->type_param_count > 0) {
            memcpy(params + outer->type_param_count, it->type_params,
                   it->type_param_count * sizeof *params);
        }
        it->type_params = params;
        it->type_param_count = count;
        t->type_param_count = count;
        t->type_params = types_alloc_array(c->arena, count + 1,
                                           sizeof *t->type_params);
        for (j = 0; j < count; j++) {
            t->type_params[j] = params[j].type;
        }
        t->nested_in = outer->symbol->type;
    }
}

/* Add the constraint r to the set of p. */
static void add_constraint(struct checker *c, struct type *p,
                           const struct constraint_ref *r);

/* Which interface of walking the copy t is: WALK_ITERABLE for a copy of
   `Iterable` of `anti.collection`, WALK_ITERATOR for one of `Iterator`,
   and WALK_NONE for every other type. */
enum walk_iface { WALK_NONE, WALK_ITERABLE, WALK_ITERATOR };

static enum walk_iface walk_iface(const struct type *t)
{
    const struct type *g = t != NULL ? t->generic : NULL;

    if (g == NULL || g->kind != TYPE_CLASS ||
        !sema_name_is(&g->module, COLLECTION_MODULE) ||
        t->args == NULL || t->args[0] == NULL) {
        return WALK_NONE;
    }
    if (sema_name_is(&g->name, COLLECTION_ITERABLE)) {
        return WALK_ITERABLE;
    }
    return sema_name_is(&g->name, COLLECTION_ITERATOR) ? WALK_ITERATOR
                                                         : WALK_NONE;
}

static void add_iface(struct checker *c, struct type *p,
                      const struct type *iface)
{
    const struct type **ifaces;
    size_t i;

    for (i = 0; i < p->iface_count; i++) {
        if (p->ifaces[i] == iface) {
            return;
        }
    }
    ifaces = types_alloc_array(c->arena, p->iface_count + 1, sizeof *ifaces);
    if (p->iface_count > 0) {
        memcpy(ifaces, p->ifaces, p->iface_count * sizeof *ifaces);
    }
    ifaces[p->iface_count++] = iface;
    p->ifaces = ifaces;
}

/* The set a `constraint` names, resolved on its first use. A set that
   names itself, directly or through others, is refused. */
static bool resolve_set(struct checker *c, struct symbol *sym)
{
    const struct item *it = sym->item;
    size_t i;

    if (sym->state == EVAL_DONE) {
        return true;
    }
    if (sym->state == EVAL_BUSY) {
        sema_error_at(c, it->name_pos, "constraint `%.*s` names itself",
                      (int)it->name.length, it->name.text);
        return false;
    }
    sym->state = EVAL_BUSY;
    for (i = 0; i < it->constraint_count; i++) {
        add_constraint(c, sym->type, &it->constraints[i]);
    }
    sym->state = EVAL_DONE;
    return true;
}

static void add_constraint(struct checker *c, struct type *p,
                           const struct constraint_ref *r)
{
    const struct type *t;
    struct symbol *sym;
    int hook;
    size_t i;

    if (r->module.length == 0 && (hook = hook_index(&r->name)) >= 0) {
        p->hooks |= 1u << hook;
        return;
    }
    if (r->module.length == 0) {
        sym = sema_module_find(c, &r->name);
        if (sym != NULL && sym->kind == SYMBOL_CONSTRAINT) {
            if (resolve_set(c, sym)) {
                p->hooks |= sym->type->hooks;
                for (i = 0; i < sym->type->iface_count; i++) {
                    add_iface(c, p, sym->type->ifaces[i]);
                }
            }
            return;
        }
        if (sym == NULL && sema_name_is(&r->name, LANG_NUMBER)) {
            for (i = 0; i < sizeof number_hooks / sizeof number_hooks[0];
                 i++) {
                p->hooks |= 1u << hook_of(number_hooks[i]);
            }
            return;
        }
        if (sym == NULL && sema_name_is(&r->name, LANG_ORDERED)) {
            for (i = 0; i < sizeof ordered_hooks / sizeof ordered_hooks[0];
                 i++) {
                p->hooks |= 1u << hook_of(ordered_hooks[i]);
            }
            return;
        }
        if (sym == NULL || sym->kind != SYMBOL_STRUCT) {
            sema_error_at(c, r->pos, "`%.*s` is no hook of the operator "
                          "table, no interface and no constraint",
                          (int)r->name.length, r->name.text);
            return;
        }
        t = sym->type;
    } else {
        /* A `constraint` of another module comes with its library
           file, which holds its set. */
        const struct symbol *module = sema_scope_find_local(&c->module_scope,
                                                            &r->module);
        const struct interface *lib =
            module != NULL && module->kind == SYMBOL_MODULE
                ? module->home
                : sema_find_library(c, &r->module);
        sym = lib != NULL ? sema_library_item(c, lib, &r->name) : NULL;
        if (sym != NULL && sym->kind == SYMBOL_CONSTRAINT) {
            p->hooks |= sym->type->hooks;
            for (i = 0; i < sym->type->iface_count; i++) {
                add_iface(c, p, sym->type->ifaces[i]);
            }
            return;
        }
        t = sema_interface_named(c, &r->module, &r->name, r->pos);
        if (t == NULL) {
            return;
        }
    }
    if (t == NULL || t->kind != TYPE_CLASS || !t->has_abstract) {
        sema_error_at(c, r->pos, "`%.*s` is no hook of the operator table, no "
                      "interface and no constraint",
                      (int)r->name.length, r->name.text);
        return;
    }
    /* A generic interface is named with its type arguments, and the
       constraint is the copy they give. */
    if (t->type_param_count > 0 && r->type_arg_count == 0) {
        sema_error_at(c, r->pos, "`%.*s` is generic, and a constraint "
                      "writes its type arguments after its name",
                      (int)r->name.length, r->name.text);
        return;
    }
    if (r->type_arg_count > 0) {
        struct type *copy = sema_copy_of(c, (struct type *)t, r->type_args,
                                         r->type_arg_count, r->type_args_pos);
        if (sema_is_error(copy)) {
            return;
        }
        t = copy;
    }
    add_iface(c, p, t);
    /* An interface of walking gives the hooks that `for` needs. */
    if (walk_iface(t) == WALK_ITERABLE) {
        p->hooks |= 1u << hook_of(LANG_HOOK_ITER);
    } else if (walk_iface(t) == WALK_ITERATOR) {
        p->hooks |= 1u << hook_of(LANG_HOOK_NEXT);
        p->hooks |= 1u << hook_of(LANG_HOOK_VALUE);
    }
}

static void resolve_params(struct checker *c, struct item *it)
{
    const struct item *within = c->within;
    const struct item *signature = c->signature;
    size_t i;
    size_t j;

    /* The type arguments of a constraint may name the parameters of the
       generic, or those of the class around a function. */
    if (it->kind == ITEM_FN) {
        c->signature = it;
        c->within = it->owner;
    } else {
        c->signature = NULL;
        c->within = it;
    }
    for (i = 0; i < it->type_param_count; i++) {
        struct type_param *tp = &it->type_params[i];
        for (j = 0; j < tp->constraint_count; j++) {
            add_constraint(c, tp->type, &tp->constraints[j]);
        }
    }
    c->within = within;
    c->signature = signature;
}

/* DESIGN: `type Name = T;` names T, and every use of the name is T.
   The target is resolved on the first use, so a name may stand before
   the types it names. One that names itself is refused. */
struct type *sema_alias_type(struct checker *c, struct symbol *sym)
{
    struct item *it = sym->item;
    const struct item *within = c->within;
    const struct item *signature = c->signature;
    struct item *function = c->function;
    struct type *t;

    if (sym->state == EVAL_DONE) {
        return sym->type;
    }
    if (sym->state == EVAL_BUSY) {
        sema_error_at(c, it->name_pos, "type `%.*s` names itself",
                      (int)it->name.length, it->name.text);
        sym->type = sema_builtin(c, TYPE_ERROR);
        return sym->type;
    }
    sym->state = EVAL_BUSY;
    c->within = NULL;
    c->signature = NULL;
    c->function = NULL;
    t = sema_resolve_type(c, it->type);
    c->within = within;
    c->signature = signature;
    c->function = function;
    if (sym->state == EVAL_BUSY) {
        sym->type = t;
    }
    sym->state = EVAL_DONE;
    return sym->type;
}

void sema_resolve_generics(struct checker *c)
{
    const struct module *module = c->module;
    size_t i;
    size_t j;

    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        if (it->symbol == NULL) {
            continue;
        }
        if (it->kind == ITEM_CONSTRAINT) {
            resolve_set(c, it->symbol);
        }
    }
    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        if (it->symbol == NULL) {
            continue;
        }
        resolve_params(c, it);
        for (j = 0; j < it->member_count; j++) {
            resolve_params(c, it->members[j]);
        }
    }
    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        struct type *t;
        if (it->symbol == NULL || it->kind != ITEM_TYPE) {
            continue;
        }
        t = sema_alias_type(c, it->symbol);
        /* The copy an `export type` names is an export class or struct
           to C, under the name of the `type`. The checks of what crosses
           to C read that. */
        if (it->exported && !sema_is_error(t) && t->generic != NULL &&
            !sema_has_params(t) && t->c_name.length == 0) {
            t->c_name = it->name;
            t->item_exported = true;
        }
    }
}

/* Constraints */

/* Whether a type the language builds in has the hook of index hook, as
   its operators give it. `hash` belongs to every type: the built-in types
   have it, and a struct or a class gets a default. */
static bool builtin_meets(const struct type *t, const char *hook)
{
    bool integer = type_is_integer(t);
    bool numeric = type_is_numeric(t);

    if (strcmp(hook, LANG_HOOK_HASH) == 0) {
        return true;
    }
    if (strcmp(hook, "eq") == 0) {
        return !type_has_fields(t) && t->kind != TYPE_ARRAY &&
               t->kind != TYPE_SLICE && t->kind != TYPE_STR &&
               t->kind != TYPE_F16 && t->kind != TYPE_VOID;
    }
    if (strcmp(hook, "lt") == 0) {
        return numeric || t->kind == TYPE_CHAR;
    }
    if (strcmp(hook, "add") == 0 || strcmp(hook, "sub") == 0 ||
        strcmp(hook, "mul") == 0 || strcmp(hook, "div") == 0) {
        return numeric;
    }
    if (strcmp(hook, "neg") == 0) {
        return type_is_signed(t) || type_is_float(t);
    }
    if (strcmp(hook, "rem") == 0 || strcmp(hook, "and") == 0 ||
        strcmp(hook, "or") == 0 || strcmp(hook, "xor") == 0 ||
        strcmp(hook, "shl") == 0 || strcmp(hook, "shr") == 0 ||
        strcmp(hook, "not") == 0) {
        return integer;
    }
    return false;
}

static bool meets_hook(struct checker *c, struct type *t, const char *hook)
{
    int bit = hook_of(hook);

    if (t->kind == TYPE_PARAM) {
        return bit >= 0 && (t->hooks & (1u << bit)) != 0;
    }
    if (strcmp(hook, LANG_HOOK_ITER) == 0 ||
        strcmp(hook, LANG_HOOK_NEXT) == 0 ||
        strcmp(hook, LANG_HOOK_VALUE) == 0 ||
        strcmp(hook, LANG_HOOK_INDEX) == 0 ||
        strcmp(hook, LANG_HOOK_SET_INDEX) == 0) {
        return sema_hook(c, t, hook) != NULL;
    }
    if (type_has_fields(t) && strcmp(hook, LANG_HOOK_HASH) != 0) {
        return sema_operator_symbol(c, t, hook) != NULL;
    }
    return builtin_meets(t, hook);
}

/* The type of the element that the hook of the type t gives, or NULL
   when t has no such hook. A lent pointer gives what it points at. */
static struct type *hook_result(struct checker *c, struct type *t,
                                const char *hook)
{
    struct symbol *sym = sema_hook(c, t, hook);
    struct type *fn;

    if (sym == NULL || sym->type == NULL || sym->type->kind != TYPE_FN) {
        return NULL;
    }
    fn = sema_member_type(c, sym->type,
                          t->kind == TYPE_POINTER ? t->element : t);
    if (fn->result != NULL && fn->result->kind == TYPE_POINTER &&
        fn->result->lent) {
        return fn->result->element;
    }
    return fn->result;
}

/* Whether the type t walks as the interface iface of walking says: an
   iterator of iface's element, or through `iter` a collection of it. */
static bool walks_as(struct checker *c, struct type *t,
                     const struct type *iface)
{
    if (walk_iface(iface) == WALK_ITERABLE) {
        t = hook_result(c, t, LANG_HOOK_ITER);
        if (t == NULL) {
            return false;
        }
    }
    return sema_hook(c, t, LANG_HOOK_NEXT) != NULL &&
           hook_result(c, t, LANG_HOOK_VALUE) == iface->args[0];
}

static bool meets_iface(struct checker *c, struct type *t,
                        const struct type *iface)
{
    size_t i;

    if (t->kind != TYPE_PARAM && walk_iface(iface) != WALK_NONE &&
        walks_as(c, t, iface)) {
        return true;
    }
    if (t->kind == TYPE_PARAM) {
        for (i = 0; i < t->iface_count; i++) {
            if (sema_descends_from(t->ifaces[i], iface)) {
                return true;
            }
        }
        return false;
    }
    return t->kind == TYPE_CLASS && (sema_descends_from(t, iface) ||
                                     sema_implemented_in(t, iface));
}

/* Check the argument t of the parameter p of the generic named by
   generic at pos, and report the first hook or interface it lacks. */
static void check_meets(struct checker *c, struct type *t,
                        const struct type *p, const struct name *generic,
                        struct pos pos)
{
    size_t i;

    if (sema_is_error(t)) {
        return;
    }
    for (i = 0; i < HOOK_COUNT; i++) {
        if ((p->hooks & (1u << i)) != 0 && !meets_hook(c, t, hook_names[i])) {
            sema_error_at(c, pos, "`%s` has no `%s`, which `%.*s` needs for "
                          "`%.*s`", sema_tn(t), hook_names[i],
                          (int)generic->length, generic->text,
                          (int)p->name.length, p->name.text);
            return;
        }
    }
    for (i = 0; i < p->iface_count; i++) {
        if (!meets_iface(c, t, p->ifaces[i])) {
            sema_error_at(c, pos, "`%s` has no `%s`, which `%.*s` needs for "
                          "`%.*s`", sema_tn(t), sema_tn(p->ifaces[i]),
                          (int)generic->length, generic->text,
                          (int)p->name.length, p->name.text);
            return;
        }
    }
}

/* DESIGN: a copy may be named in a field or a signature before the
   functions of the module have their types. It is checked against its
   constraints once they do, since an `operator fn` gives a hook. Every
   check after that runs where the use stands. */
static void meets(struct checker *c, struct type *t, const struct type *p,
                  const struct name *generic, struct pos pos)
{
    struct pending_check *pc;

    if (c->pending_done) {
        check_meets(c, t, p, generic, pos);
        return;
    }
    if (c->pending_count == c->pending_capacity) {
        size_t capacity =
            c->pending_capacity == 0 ? 16 : c->pending_capacity * 2;
        pc = capacity <= SIZE_MAX / sizeof *pc
                 ? realloc(c->pending, capacity * sizeof *pc)
                 : NULL;
        if (pc == NULL) {
            fputs("antic: out of memory\n", stderr);
            exit(70);
        }
        c->pending = pc;
        c->pending_capacity = capacity;
    }
    pc = &c->pending[c->pending_count++];
    pc->type = t;
    pc->param = p;
    pc->generic = *generic;
    pc->pos = pos;
}

void sema_run_pending(struct checker *c)
{
    size_t i;

    c->pending_done = true;
    for (i = 0; i < c->pending_count; i++) {
        check_meets(c, c->pending[i].type, c->pending[i].param,
                    &c->pending[i].generic, c->pending[i].pos);
    }
    free(c->pending);
    c->pending = NULL;
    c->pending_count = 0;
    c->pending_capacity = 0;
}

/* Substitution */

bool sema_has_params(const struct type *t)
{
    size_t i;

    if (t == NULL) {
        return false;
    }
    switch (t->kind) {
    case TYPE_PARAM:
        return true;
    case TYPE_POINTER:
    case TYPE_SLICE:
    case TYPE_OPTIONAL:
        return sema_has_params(t->element);
    case TYPE_ARRAY:
        return sema_has_params(t->element) ||
               (t->length_of != NULL &&
                t->length_of->kind == SYMBOLIC_PARAM);
    case TYPE_FN:
    case TYPE_TUPLE:
        for (i = 0; i < t->param_count; i++) {
            if (sema_has_params(t->params[i])) {
                return true;
            }
        }
        return t->kind == TYPE_FN && sema_has_params(t->result);
    case TYPE_STRUCT:
    case TYPE_CLASS:
    case TYPE_VARIANT:
        if (types_is_chan(t)) {
            return sema_has_params(t->element);
        }
        for (i = 0; t->generic != NULL && i < t->generic->type_param_count;
             i++) {
            if ((t->args[i] != NULL && sema_has_params(t->args[i])) ||
                (t->values[i] != NULL &&
                 t->values[i]->kind == SYMBOLIC_PARAM)) {
                return true;
            }
        }
        return t->type_param_count > 0;
    default:
        return false;
    }
}

static struct type *subst(struct checker *c, struct type *t,
                          const struct generic_map *map);

struct type *sema_subst(struct checker *c, struct type *t,
                        const struct generic_map *map)
{
    return subst(c, t, map);
}

static const struct symbolic *subst_symbolic(struct checker *c,
                                             const struct symbolic *s,
                                             const struct generic_map *map)
{
    struct symbolic key;
    size_t i;

    if (s == NULL) {
        return NULL;
    }
    key = *s;
    key.next = NULL;
    switch (s->kind) {
    case SYMBOLIC_INT:
        return s;
    case SYMBOLIC_PARAM:
        for (i = 0; i < map->count; i++) {
            if (map->params[i] == s->of && map->values[i] != NULL) {
                return map->values[i];
            }
        }
        return s;
    case SYMBOLIC_SIZE_OF:
        key.of = subst(c, s->of, map);
        break;
    case SYMBOLIC_UNARY:
    case SYMBOLIC_CAST:
    case SYMBOLIC_BINARY:
        key.a = subst_symbolic(c, s->a, map);
        key.b = subst_symbolic(c, s->b, map);
        break;
    }
    return types_symbolic(c->types, &key);
}

static struct type *copy_named(struct checker *c, struct type *generic,
                               struct type **args,
                               const struct symbolic **values);

const struct symbolic *sema_subst_symbolic(struct checker *c,
                                           const struct symbolic *s,
                                           const struct generic_map *map)
{
    return subst_symbolic(c, s, map);
}

/* The value of the constant parameter p where its generic names it. */
static const struct symbolic *param_value(const struct type *p)
{
    const struct symbol *sym = p->param->symbol;

    return sym != NULL && sym->value != NULL ? sym->value->as.symbolic : NULL;
}

/* The type t nested in a generic class, as the body of the class names
   it. The arguments of map stand in place of the parameters. */
static struct type *subst_nested(struct checker *c, struct type *t,
                                 const struct generic_map *map)
{
    size_t count = t->type_param_count;
    struct type **args = types_alloc_array(c->arena, count + 1, sizeof *args);
    const struct symbolic **values =
        types_alloc_array(c->arena, count + 1, sizeof *values);
    bool changed = false;
    size_t i;

    for (i = 0; i < count; i++) {
        struct type *p = t->type_params[i];
        if (p->param->constant) {
            args[i] = NULL;
            values[i] = subst_symbolic(c, param_value(p), map);
            changed = changed || values[i] != param_value(p);
        } else {
            args[i] = subst(c, p, map);
            values[i] = NULL;
            changed = changed || args[i] != p;
        }
    }
    return changed ? copy_named(c, t, args, values) : t;
}

static struct type *subst(struct checker *c, struct type *t,
                          const struct generic_map *map)
{
    struct type *element;
    struct type **params;
    struct type *result;
    struct type *fn;
    const struct symbolic **values;
    bool changed = false;
    size_t i;

    if (t == NULL) {
        return NULL;
    }
    if (t == map->from && map->to != NULL) {
        return map->to;
    }
    switch (t->kind) {
    case TYPE_PARAM:
        for (i = 0; i < map->count; i++) {
            if (map->params[i] == t && map->args[i] != NULL) {
                return map->args[i];
            }
        }
        return t;
    case TYPE_POINTER:
        element = subst(c, t->element, map);
        if (element == t->element) {
            return t;
        }
        element = types_pointer_of(c->types, element, t->nullable);
        return t->lent ? types_lent(c->types, element) : element;
    case TYPE_SLICE:
        element = subst(c, t->element, map);
        return element == t->element ? t : types_slice(c->types, element);
    /* `?T` with a pointer or a function type for T is the `?*U` or the
       `?fn(...)` of it, one word with `none` as zero. */
    case TYPE_OPTIONAL:
        element = subst(c, t->element, map);
        return element == t->element ? t : types_with_none(c->types, element);
    case TYPE_ARRAY: {
        const struct symbolic *length = subst_symbolic(c, t->length_of, map);
        element = subst(c, t->element, map);
        if (element == t->element && length == t->length_of) {
            return t;
        }
        if (length == NULL) {
            return types_array(c->types, element, t->length);
        }
        if (length->kind == SYMBOLIC_INT) {
            return types_array(c->types, element, length->value);
        }
        return types_array_symbolic(c->types, element, length);
    }
    case TYPE_FN:
        params = types_alloc_array(c->arena, t->param_count + 1,
                                   sizeof *params);
        for (i = 0; i < t->param_count; i++) {
            params[i] = subst(c, t->params[i], map);
            changed = changed || params[i] != t->params[i];
        }
        result = subst(c, t->result, map);
        if (!changed && result == t->result) {
            return t;
        }
        fn = types_fn_flagged(c->types, params, t->param_count, result,
                              t->bound, t->may_fail, t->has_out);
        if (t->owned) {
            fn = types_fn_owned(c->types, fn);
        } else if (t->context) {
            fn = types_fn_form(c->types, fn, true, t->concurrent);
        }
        return t->nullable ? types_with_none(c->types, fn) : fn;
    case TYPE_TUPLE:
        params = types_alloc_array(c->arena, t->param_count + 1,
                                   sizeof *params);
        for (i = 0; i < t->param_count; i++) {
            params[i] = subst(c, t->params[i], map);
            changed = changed || params[i] != t->params[i];
        }
        return changed ? types_tuple(c->types, params, t->param_count) : t;
    case TYPE_STRUCT:
    case TYPE_CLASS:
    case TYPE_VARIANT:
        if (types_is_chan(t)) {
            element = subst(c, t->element, map);
            return element == t->element ? t : types_chan(c->types, element);
        }
        if (t->generic == NULL && t->nested_in != NULL) {
            return subst_nested(c, t, map);
        }
        if (t->generic == NULL) {
            return t;
        }
        params = types_alloc_array(c->arena, t->generic->type_param_count + 1,
                                   sizeof *params);
        values = types_alloc_array(c->arena, t->generic->type_param_count + 1,
                                   sizeof *values);
        for (i = 0; i < t->generic->type_param_count; i++) {
            params[i] = subst(c, t->args[i], map);
            values[i] = subst_symbolic(c, t->values[i], map);
            changed = changed || params[i] != t->args[i] ||
                      values[i] != t->values[i];
        }
        return changed ? copy_named(c, t->generic, params, values) : t;
    default:
        return t;
    }
}

/* Copies */

/* The name of a copy as a program writes it, `Pair<int, str>`. */
static struct name copy_name(struct checker *c, const struct type *generic,
                             struct type *const *args,
                             const struct symbolic *const *values)
{
    struct text out = {0};
    struct name name;
    char *text;

    type_copy_name(&out, generic, args, values, false);
    text = arena_alloc(c->arena, out.length + 1);
    memcpy(text, text_cstr(&out), out.length + 1);
    text_free(&out);
    name.text = text;
    name.length = strlen(text);
    return name;
}

struct generic_map sema_copy_map(const struct type *copy)
{
    struct generic_map map;

    memset(&map, 0, sizeof map);
    map.from = copy->generic;
    map.to = (struct type *)copy;
    map.params = copy->generic->type_params;
    map.args = copy->args;
    map.values = (const struct symbolic **)copy->values;
    map.count = copy->generic->type_param_count;
    return map;
}

/* The struct of a case of a variant copy, its fields with the arguments
   in place. */
static struct type *copy_payload(struct checker *c, struct type *payload,
                                 const struct type *copy,
                                 const struct generic_map *map)
{
    struct struct_field *fields;
    struct type *s;
    const char *dot;
    struct name name;
    size_t i;

    if (payload == NULL) {
        return NULL;
    }
    fields = types_alloc_array(c->arena, payload->field_count + 1,
                               sizeof *fields);
    for (i = 0; i < payload->field_count; i++) {
        fields[i] = payload->fields[i];
        fields[i].type = subst(c, payload->fields[i].type, map);
    }
    dot = memchr(payload->name.text, '.', payload->name.length);
    name = payload->name;
    if (dot != NULL) {
        struct name case_name;
        case_name.text = dot + 1;
        case_name.length =
            payload->name.length - (size_t)(dot + 1 - payload->name.text);
        name = sema_dotted(c, &copy->name, &case_name);
    }
    s = types_struct(c->types, payload->module, name);
    s->packed = payload->packed;
    types_set_fields(c->types, s, fields, payload->field_count);
    return s;
}

/* Give the copy the fields, the base and the cases of its generic with
   the arguments in place. */
static void fill_copy(struct checker *c, struct type *copy)
{
    const struct type *g = copy->generic;
    struct generic_map map = sema_copy_map(copy);
    struct struct_field *fields;
    size_t i;

    copy->base = subst(c, g->base, &map);
    copy->packed = g->packed;
    copy->align = g->align;
    copy->has_abstract = g->has_abstract;
    copy->is_final = g->is_final;
    copy->traced = g->traced;
    copy->safety = g->safety;
    copy->unchecked_fields = g->unchecked_fields;
    copy->compatible = g->compatible;
    if (g->kind == TYPE_VARIANT) {
        struct type **payloads =
            types_alloc_array(c->arena, g->param_count + 1, sizeof *payloads);
        for (i = 0; i < g->param_count; i++) {
            payloads[i] = copy_payload(c, g->params[i], copy, &map);
        }
        types_set_cases(c->types, copy, g->base, payloads, g->param_count);
        return;
    }
    fields = types_alloc_array(c->arena, g->field_count + 1, sizeof *fields);
    for (i = 0; i < g->field_count; i++) {
        fields[i] = g->fields[i];
        fields[i].type = subst(c, g->fields[i].type, &map);
    }
    types_set_fields(c->types, copy, fields, g->field_count);
}

/* DESIGN: filling a copy may name another copy, which is filled in
   turn. A generic may name a copy of itself with other arguments, as
   `W<T>` holding a `W<Box<T>>` does. It names new copies without end, so
   a chain of copies past COPY_DEPTH_MAX is refused once. The message
   stands at the first type parameter of the generic the chain began
   with. */
#define COPY_DEPTH_MAX 64

static void fill_one(struct checker *c, struct type *copy)
{
    if (c->copy_depth >= COPY_DEPTH_MAX) {
        if (!c->copy_refused) {
            c->copy_refused = true;
            sema_error_at(c, c->copy_root->type_params[0]->param->pos,
                          "the copies of `%s` name ever deeper copies of it",
                          sema_tn(c->copy_root));
        }
        return;
    }
    if (c->copy_depth == 0) {
        c->copy_root = copy->generic;
    }
    c->copy_depth++;
    fill_copy(c, copy);
    c->copy_depth--;
}

void sema_generic_ready(struct checker *c, struct type *generic)
{
    struct type *copy;

    if (generic == NULL || generic->type_param_count == 0) {
        return;
    }
    generic->generic_ready = true;
    for (copy = generic->copies; copy != NULL; copy = copy->next_copy) {
        fill_one(c, copy);
    }
}

/* The copy of generic with these arguments, one type for every use that
   names the same ones. A copy whose arguments are the parameters of the
   generic itself is the generic, as `List<T>` is in the body of
   `List`. */
static struct type *copy_named(struct checker *c, struct type *generic,
                               struct type **args,
                               const struct symbolic **values)
{
    struct type *copy;
    size_t count = generic->type_param_count;
    bool itself = true;
    size_t i;

    for (i = 0; i < count; i++) {
        struct type *p = generic->type_params[i];
        itself = itself && (p->param->constant
                                ? values[i] != NULL &&
                                      values[i]->kind == SYMBOLIC_PARAM &&
                                      values[i]->of == p
                                : args[i] == p);
    }
    if (itself) {
        return generic;
    }
    for (copy = generic->copies; copy != NULL; copy = copy->next_copy) {
        bool same = true;
        for (i = 0; i < count && same; i++) {
            same = copy->args[i] == args[i] && copy->values[i] == values[i];
        }
        if (same) {
            return copy;
        }
    }
    copy = types_struct(c->types, generic->module,
                        copy_name(c, generic, args, values));
    copy->kind = generic->kind;
    copy->generic = generic;
    copy->args = args;
    copy->values = values;
    copy->members = generic->members;
    copy->member_count = generic->member_count;
    copy->item_exported = generic->item_exported;
    copy->next_copy = generic->copies;
    generic->copies = copy;
    if (generic->generic_ready) {
        fill_one(c, copy);
    }
    return copy;
}

struct type *sema_copy_named(struct checker *c, struct type *generic,
                             struct type **args,
                             const struct symbolic **values)
{
    return copy_named(c, generic, args, values);
}

/* The name of the item that declares the generic g, for a message. */
static const struct name *generic_name(const struct type *g)
{
    return &g->name;
}

/* Check each argument of a copy of generic against its parameter, and
   make the copy. */
static struct type *make_copy(struct checker *c, struct type *generic,
                              struct type **args,
                              const struct symbolic **values, struct pos pos)
{
    size_t i;

    for (i = 0; i < generic->type_param_count; i++) {
        if (args[i] != NULL) {
            meets(c, args[i], generic->type_params[i], generic_name(generic),
                  pos);
        }
    }
    return copy_named(c, generic, args, values);
}

/* The argument of the parameter p of the generic named name, as written
   at a. A type parameter takes a type. A constant one takes an integer
   literal or the name of a constant. */
static bool resolve_arg(struct checker *c, const struct name *name,
                        const struct type *p, struct type_expr *a,
                        struct type **type, const struct symbolic **value)
{
    *type = NULL;
    *value = NULL;
    if (!p->param->constant) {
        if (a->kind == TYPEX_CONST) {
            sema_error_at(c, a->pos, "`%.*s` takes a type for `%.*s`, "
                          "found a constant", (int)name->length, name->text,
                          (int)p->name.length, p->name.text);
            return false;
        }
        *type = sema_resolve_type(c, a);
        return !sema_is_error(*type);
    }
    if (a->kind == TYPEX_CONST) {
        struct symbolic key;
        memset(&key, 0, sizeof key);
        key.kind = SYMBOLIC_INT;
        key.type = sema_builtin(c, TYPE_I64);
        key.value = a->length->as.integer;
        a->length->type = key.type;
        *value = types_symbolic(c->types, &key);
        return true;
    }
    if (a->kind == TYPEX_NAMED && a->module.length == 0 && a->arg_count == 0) {
        struct symbol *sym = sema_lookup(c, &a->name);
        if (sym != NULL && sym->kind == SYMBOL_CONST &&
            sema_const_symbol(c, sym, a->pos) && sym->value != NULL &&
            type_is_integer(sym->type)) {
            if (sym->value->kind == CONST_SYMBOLIC) {
                *value = sym->value->as.symbolic;
            } else {
                struct symbolic key;
                memset(&key, 0, sizeof key);
                key.kind = SYMBOLIC_INT;
                key.type = sema_builtin(c, TYPE_I64);
                key.value = sym->value->as.integer;
                *value = types_symbolic(c->types, &key);
            }
            return true;
        }
        sema_error_at(c, a->pos, "`%.*s` must be a constant for `%.*s`",
                      (int)a->name.length, a->name.text, (int)p->name.length,
                      p->name.text);
        return false;
    }
    sema_error_at(c, a->pos, "`%.*s` takes a constant for `%.*s`, found a type",
                  (int)name->length, name->text, (int)p->name.length,
                  p->name.text);
    return false;
}

struct type *sema_copy_of(struct checker *c, struct type *generic,
                          struct type_expr *const *written, size_t count,
                          struct pos pos)
{
    size_t want = generic->type_param_count;
    size_t outer = want - sema_nested_own(generic);
    struct type **args;
    const struct symbolic **values;
    bool ok = true;
    size_t i;

    if (want == outer) {
        sema_error_at(c, pos, "`%s` is not generic", sema_tn(generic));
        return sema_builtin(c, TYPE_ERROR);
    }
    if (count != want - outer) {
        sema_error_at(c, pos, "`%s` takes %zu type argument%s, found %zu",
                      sema_tn(generic), want - outer,
                      want - outer == 1 ? "" : "s", count);
        return sema_builtin(c, TYPE_ERROR);
    }
    args = types_alloc_array(c->arena, want + 1, sizeof *args);
    values = types_alloc_array(c->arena, want + 1, sizeof *values);
    /* A type nested in a generic class is named in its body, where the
       parameters of the class stand for themselves. */
    for (i = 0; i < outer; i++) {
        struct type *p = generic->type_params[i];
        args[i] = p->param->constant ? NULL : p;
        values[i] = p->param->constant ? param_value(p) : NULL;
    }
    for (i = outer; i < want; i++) {
        ok = resolve_arg(c, &generic->name, generic->type_params[i],
                         written[i - outer], &args[i], &values[i]) && ok;
    }
    if (!ok) {
        return sema_builtin(c, TYPE_ERROR);
    }
    return make_copy(c, generic, args, values, pos);
}

size_t sema_nested_own(const struct type *t)
{
    return t->nested_in != NULL
               ? t->type_param_count - t->nested_in->type_param_count
               : t->type_param_count;
}

/* The type a member of the copy has there: the type fn of the member of
   its generic with the arguments in place. */
struct type *sema_member_type(struct checker *c, struct type *fn,
                              const struct type *copy)
{
    struct generic_map map;

    if (copy == NULL || copy->generic == NULL) {
        return fn;
    }
    map = sema_copy_map(copy);
    return subst(c, fn, &map);
}

/* The type fn of a function that the class of the item owner declares,
   as the class s reaches it. That is through the copy of owner in the
   chain of s, or through s itself when owner is no generic. */
struct type *sema_member_type_in(struct checker *c, struct type *fn,
                                 const struct item *owner, struct type *s)
{
    struct type *level;

    if (owner != NULL && owner->type_param_count > 0 && owner->symbol != NULL &&
        (level = copy_in_chain(s, owner->symbol->type)) != NULL) {
        return sema_member_type(c, fn, level);
    }
    return sema_member_type(c, fn, s);
}

/* Uses in expressions */

void sema_refuse_type_args(struct checker *c, const struct expr *e,
                           const struct name *name)
{
    sema_error_at(c, e->type_args_pos, "`%.*s` is not generic. Put the "
                  "comparison in parentheses", (int)name->length, name->text);
}

/* The type a name of a struct, class or variant stands for at e, with
   the arguments e writes after it. A generic without them takes the
   copy the context expects. */
struct type *sema_generic_named(struct checker *c, struct expr *e,
                                struct type *t, const struct name *name,
                                struct type *expected)
{
    struct type *s = expected != NULL ? sema_struct_of(expected) : NULL;

    if (sema_is_error(t)) {
        return t;
    }
    if (t->type_param_count == 0) {
        if (e->type_arg_count > 0) {
            sema_refuse_type_args(c, e, name);
            return sema_builtin(c, TYPE_ERROR);
        }
        return t;
    }
    if (e->type_arg_count > 0) {
        return sema_copy_of(c, t, e->type_args, e->type_arg_count,
                            e->type_args_pos);
    }
    if (s != NULL && (s == t || s->generic == t)) {
        return s;
    }
    if (t->nested_in != NULL && sema_nested_own(t) == 0) {
        return t;
    }
    if (in_generic(c) && sema_checking_class(c) == t) {
        return t;
    }
    sema_error_at(c, e->pos, "`%s` is generic, and nothing gives its type "
                  "arguments here. Write them out after its name",
                  sema_tn(t));
    return sema_builtin(c, TYPE_ERROR);
}

/* Inference */

/* Bind a constant parameter that stands as param to the constant arg. */
static void unify_value(const struct symbolic *param,
                        const struct symbolic *arg,
                        const struct generic_map *map)
{
    size_t i;

    if (param == NULL || arg == NULL || param->kind != SYMBOLIC_PARAM) {
        return;
    }
    for (i = 0; i < map->count; i++) {
        if (map->params[i] == param->of && map->values[i] == NULL) {
            map->values[i] = arg;
        }
    }
}

/* Bind the parameters in param to the parts of arg that stand where they
   stand. A parameter already bound keeps its argument. */
static void unify(struct type *param, struct type *arg,
                  const struct generic_map *map)
{
    size_t i;

    if (param == NULL || arg == NULL || sema_is_error(arg) ||
        arg->kind == TYPE_NONE) {
        return;
    }
    /* DESIGN: a type argument of function type is the plain form, one C
       function pointer. A function type is that wherever a type stands
       but at a parameter. A closure, or a parameter of the form of two
       words, that gives it is held to the rules of `keep`, as at a
       field. */
    if (param->kind == TYPE_PARAM) {
        if (arg->kind == TYPE_FN && arg->context && map->types != NULL) {
            arg = types_fn_form(map->types, arg, false, false);
        }
        /* A type argument is never `lent`, so a lent pointer at a
           parameter of a type parameter meets the rule of `lent` there. */
        if (map->types != NULL) {
            arg = types_unlent(map->types, arg);
        }
        for (i = 0; i < map->count; i++) {
            if (map->params[i] == param && map->args[i] == NULL &&
                !param->param->constant) {
                map->args[i] = arg;
            }
        }
        return;
    }
    /* A `?T` takes a `?U`, a `?*U` and a plain value of U, and each
       gives T the U. */
    if (param->kind == TYPE_OPTIONAL) {
        if (arg->kind == TYPE_OPTIONAL) {
            unify(param->element, arg->element, map);
        } else if (map->types != NULL) {
            unify(param->element, types_without_none(map->types, arg), map);
        }
        return;
    }
    if (param->kind != arg->kind) {
        return;
    }
    switch (param->kind) {
    case TYPE_POINTER:
    case TYPE_SLICE:
        unify(param->element, arg->element, map);
        return;
    case TYPE_ARRAY:
        unify(param->element, arg->element, map);
        if (param->length_of != NULL && arg->length_of == NULL) {
            struct symbolic key;
            memset(&key, 0, sizeof key);
            key.kind = SYMBOLIC_INT;
            key.type = param->length_of->type;
            key.value = arg->length;
            unify_value(param->length_of, types_symbolic(map->types, &key),
                        map);
        } else {
            unify_value(param->length_of, arg->length_of, map);
        }
        return;
    case TYPE_FN:
    case TYPE_TUPLE:
        if (param->param_count != arg->param_count) {
            return;
        }
        for (i = 0; i < param->param_count; i++) {
            unify(param->params[i], arg->params[i], map);
        }
        if (param->kind == TYPE_FN) {
            unify(param->result, arg->result, map);
        }
        return;
    default:
        if (param->generic != NULL && param->generic == arg->generic) {
            for (i = 0; i < param->generic->type_param_count; i++) {
                unify(param->args[i], arg->args[i], map);
                unify_value(param->values[i], arg->values[i], map);
            }
        }
        return;
    }
}

/* The copy of the generic class g that the type t is or inherits. */
static struct type *copy_in_chain(struct type *t, const struct type *g)
{
    for (t = sema_struct_of(t); t != NULL;
         t = t->kind == TYPE_CLASS ? t->base : NULL) {
        if (t == g || t->generic == g) {
            return t;
        }
    }
    return NULL;
}

struct type *sema_generic_call(struct checker *c, struct expr *e,
                               struct type *fn, const struct symbol *sym,
                               size_t fixed, const struct generic_call *g)
{
    const struct item *it = sym != NULL ? sym->item : NULL;
    const struct item *owner = it != NULL ? it->owner : NULL;
    struct type *outer = owner != NULL && owner->type_param_count > 0 &&
                                 owner->symbol != NULL
                             ? owner->symbol->type
                             : NULL;
    size_t own = it != NULL ? it->type_param_count : 0;
    size_t outer_count = outer != NULL ? outer->type_param_count : 0;
    struct type *copy = NULL;
    struct type **params;
    struct generic_map map;
    const struct name *name;
    size_t i;

    if (own == 0 && g->count > 0) {
        sema_refuse_type_args(c, g->written_at, g->name);
        return sema_builtin(c, TYPE_ERROR);
    }
    if (own == 0 && outer == NULL) {
        return fn;
    }
    name = &it->name;
    if (g->count > 0 && g->count != own) {
        sema_error_at(c, g->written_at->type_args_pos,
                      "`%.*s` takes %zu type argument%s, found %zu",
                      (int)name->length, name->text, own,
                      own == 1 ? "" : "s", g->count);
        return sema_builtin(c, TYPE_ERROR);
    }
    params = types_alloc_array(c->arena, outer_count + own + 1,
                               sizeof *params);
    memset(&map, 0, sizeof map);
    map.types = c->types;
    map.count = outer_count + own;
    map.params = params;
    map.args = types_alloc_array(c->arena, map.count + 1, sizeof *map.args);
    map.values = types_alloc_array(c->arena, map.count + 1,
                                   sizeof *map.values);
    for (i = 0; i < outer_count; i++) {
        params[i] = outer->type_params[i];
    }
    for (i = 0; i < own; i++) {
        params[outer_count + i] = it->type_params[i].type;
    }
    /* The class of the call gives its own parameters: the copy that
       `List<int>.new()` names, or the copy the receiver is. A receiver
       of the generic itself is `self` in its body, whose parameters stand
       as they are. `List.new()` leaves them to inference. */
    if (outer != NULL) {
        copy = g->owner != NULL ? copy_in_chain(g->owner, outer) : NULL;
        if (copy == outer && fixed == 0) {
            copy = NULL;
        }
        if (copy != NULL && copy != outer) {
            for (i = 0; i < outer_count; i++) {
                map.args[i] = copy->args[i];
                map.values[i] = copy->values[i];
            }
            map.from = outer;
            map.to = copy;
        } else if (copy == outer) {
            for (i = 0; i < outer_count; i++) {
                struct type *p = outer->type_params[i];
                map.args[i] = p->param->constant ? NULL : p;
                map.values[i] = p->param->constant ? param_value(p) : NULL;
            }
        }
    }
    for (i = 0; i < g->count; i++) {
        if (!resolve_arg(c, name, params[outer_count + i], g->written[i],
                         &map.args[outer_count + i],
                         &map.values[outer_count + i])) {
            return sema_builtin(c, TYPE_ERROR);
        }
    }
    /* DESIGN: inference reads the arguments of the call, in order, and
       never the body or a later use of the result. An argument whose
       parameter names an unbound parameter is checked without a type
       expected, and its type binds what stands in the same place. The
       receiver of a generic function called as `v.f(args)` is its first
       argument. */
    for (i = 0; outer == NULL && i < fixed && i < fn->param_count; i++) {
        unify(fn->params[i], e->as.call.args[i]->type, &map);
    }
    for (i = fixed; i < e->as.call.arg_count && i < fn->param_count; i++) {
        struct type *p = subst(c, fn->params[i], &map);
        struct type *t;
        size_t k;
        bool open = false;
        for (k = 0; k < map.count; k++) {
            if (map.args[k] == NULL && map.values[k] == NULL &&
                p != NULL && sema_has_params(p)) {
                open = true;
            }
        }
        if (!open) {
            continue;
        }
        t = sema_check_expr(c, e->as.call.args[i], NULL);
        g->prechecked[i] = t;
        if (sema_is_error(t)) {
            return t;
        }
        unify(p, t, &map);
    }
    for (i = 0; i < map.count; i++) {
        if (map.args[i] == NULL && map.values[i] == NULL) {
            const struct type *p = params[i];
            if (i < outer_count && copy == outer) {
                continue;
            }
            sema_error_at(c, e->pos, "nothing in the arguments gives `%.*s` "
                          "of `%.*s`, so the call writes it out after the "
                          "name", (int)p->name.length, p->name.text,
                          (int)(i < outer_count ? outer->name.length
                                                : name->length),
                          i < outer_count ? outer->name.text : name->text);
            return sema_builtin(c, TYPE_ERROR);
        }
    }
    for (i = 0; i < map.count; i++) {
        if (map.args[i] != NULL) {
            check_meets(c, map.args[i], params[i],
                        i < outer_count ? &outer->name : name, e->pos);
        }
    }
    /* A copy of the class that inference completed replaces the class in
       the signature, `List.new()` of `*List<T>` among them. */
    if (outer != NULL && map.to == NULL && copy != outer) {
        struct type **args =
            types_alloc_array(c->arena, outer_count + 1, sizeof *args);
        const struct symbolic **values =
            types_alloc_array(c->arena, outer_count + 1, sizeof *values);
        for (i = 0; i < outer_count; i++) {
            args[i] = map.args[i];
            values[i] = map.values[i];
        }
        map.from = outer;
        map.to = make_copy(c, outer, args, values, e->pos);
    }
    /* The call keeps the arguments of the copy it calls, which the copy
       of the function that holds it puts its own arguments into. */
    e->as.call.copy_args = map.args;
    e->as.call.copy_values = map.values;
    e->as.call.copy_count = map.count;
    return subst(c, fn, &map);
}

/* Check the arguments that map gives the generic function it against its
   constraints and record them on call for its copy. Gives the signature
   fn of the copy, or NULL when a parameter has no argument. */
static struct type *finish_copy(struct checker *c, struct expr *call,
                                struct type *fn, const struct item *it,
                                struct generic_map *map)
{
    size_t i;

    for (i = 0; i < map->count; i++) {
        const struct type *p = map->params[i];
        if (map->args[i] == NULL && map->values[i] == NULL) {
            sema_error_at(c, call->pos, "nothing in the arguments gives `%.*s` "
                          "of `%.*s`, so the call writes it out after the "
                          "name", (int)p->name.length, p->name.text,
                          (int)it->name.length, it->name.text);
            return NULL;
        }
        if (map->args[i] != NULL) {
            check_meets(c, map->args[i], p, &it->name, call->pos);
        }
    }
    call->as.call.copy_args = map->args;
    call->as.call.copy_values = map->values;
    call->as.call.copy_count = map->count;
    return subst(c, fn, map);
}

/* DESIGN: a generic `worker fn` named by `parallel` or `dispatch` takes
   its arguments as a call does. They are written after its name, or
   they come from the values it is given. The chunk or the object
   comes first, then the arguments of the call. The copy's signature is
   then checked against the worker rules, with the concrete types. */
struct type *sema_worker_copy(struct checker *c, struct expr *call,
                              struct type *fn, const struct symbol *sym,
                              struct type *first)
{
    const struct item *it = sym->item;
    struct expr *callee = call->as.call.callee;
    size_t count = it->type_param_count;
    struct type **params;
    struct generic_map map;
    size_t i;

    if (callee->type_arg_count > 0 && callee->type_arg_count != count) {
        sema_error_at(c, callee->type_args_pos,
                      "`%.*s` takes %zu type argument%s, found %zu",
                      (int)it->name.length, it->name.text, count,
                      count == 1 ? "" : "s", callee->type_arg_count);
        return NULL;
    }
    params = types_alloc_array(c->arena, count + 1, sizeof *params);
    memset(&map, 0, sizeof map);
    map.types = c->types;
    map.count = count;
    map.params = params;
    map.args = types_alloc_array(c->arena, count + 1, sizeof *map.args);
    map.values = types_alloc_array(c->arena, count + 1, sizeof *map.values);
    for (i = 0; i < count; i++) {
        params[i] = it->type_params[i].type;
    }
    for (i = 0; i < callee->type_arg_count; i++) {
        if (!resolve_arg(c, &it->name, params[i], callee->type_args[i],
                         &map.args[i], &map.values[i])) {
            return NULL;
        }
    }
    unify(fn->params[0], first, &map);
    for (i = 0; i < call->as.call.arg_count && i + 1 < fn->param_count; i++) {
        struct expr *arg = call->as.call.args[i];
        struct type *p = subst(c, fn->params[i + 1], &map);
        struct type *t;
        if (!sema_has_params(p)) {
            continue;
        }
        t = sema_check_expr(c, arg, NULL);
        if (sema_is_error(t)) {
            return NULL;
        }
        arg->prechecked = true;
        unify(p, t, &map);
    }
    return finish_copy(c, call, fn, it, &map);
}

/* DESIGN: an operator on a copy of a generic struct calls the generic
   `operator fn` of its module. Its arguments come from the types of the
   operands, as those of a call come from its arguments. A function of
   a copy of a generic class takes the arguments of the copy. */
struct type *sema_operator_copy(struct checker *c, struct expr *call,
                                struct type *fn, const struct symbol *sym,
                                struct type *left, struct type *right)
{
    const struct item *it = sym->item;
    const struct item *owner = it != NULL ? it->owner : NULL;
    size_t count = it != NULL ? it->type_param_count : 0;
    struct type **params;
    struct generic_map map;
    size_t i;

    if (owner != NULL && owner->type_param_count > 0 && count == 0) {
        struct type *copy = copy_in_chain(left, owner->symbol->type);
        return copy != NULL ? sema_member_type(c, fn, copy) : fn;
    }
    if (count == 0 || owner != NULL) {
        return fn;
    }
    params = types_alloc_array(c->arena, count + 1, sizeof *params);
    memset(&map, 0, sizeof map);
    map.types = c->types;
    map.count = count;
    map.params = params;
    map.args = types_alloc_array(c->arena, count + 1, sizeof *map.args);
    map.values = types_alloc_array(c->arena, count + 1, sizeof *map.values);
    for (i = 0; i < count; i++) {
        params[i] = it->type_params[i].type;
    }
    if (fn->param_count > 0) {
        struct type *first = fn->params[0];
        unify(first, first->kind == TYPE_POINTER && left->kind != TYPE_POINTER
                         ? types_pointer(c->types, left)
                         : left,
              &map);
    }
    if (fn->param_count > 1 && right != NULL) {
        unify(fn->params[1], right, &map);
    }
    return finish_copy(c, call, fn, it, &map);
}

/* Operators on a type parameter */

/* Report that the generic that declares the parameter p uses the form
   form on it, which the hook hook would give. */
static void param_lacks(struct checker *c, struct pos pos, const char *form,
                        const struct type *p, const char *hook)
{
    const struct item *by = p->declared_by;

    /* The value a hook gives has no constraints that could name one. */
    if (p->param == NULL) {
        sema_error_at(c, pos, "`%.*s` uses `%s` on `%.*s`, which no "
                      "constraint gives", by != NULL ? (int)by->name.length : 0,
                      by != NULL ? by->name.text : "", form,
                      (int)p->name.length, p->name.text);
        return;
    }
    sema_error_at(c, pos, "`%.*s` uses `%s` on `%.*s`, which its "
                  "constraints do not give. Add `%s` to them",
                  by != NULL ? (int)by->name.length : 0,
                  by != NULL ? by->name.text : "", form, (int)p->name.length,
                  p->name.text, hook);
}

static bool has_hook(const struct type *p, const char *hook)
{
    int bit = hook_of(hook);

    return bit >= 0 && (p->hooks & (1u << bit)) != 0;
}

bool sema_param_operator(struct checker *c, struct expr *e,
                         enum token_kind op, const char *hook,
                         struct type *operand)
{
    char spelling[OP_TEXT];

    if (has_hook(operand, hook)) {
        return true;
    }
    param_lacks(c, e->pos, sema_op_text(op, spelling), operand, hook);
    return false;
}

/* DESIGN: the constraints of a parameter name a hook and not the types
   it takes and gives. The value a walk of the parameter gives and the
   value `e[i]` reads are each a type parameter without constraints of
   their own, `C.value` and `C.index`, made once per parameter. Such a
   value can be stored, copied and passed on, as an unconstrained
   parameter can. The index and the value `e[i] = v` writes are checked
   without a type expected. Each copy checks the types the hooks of its
   argument name. */
static struct type *hook_value(struct checker *c, struct type *p,
                               const char *hook)
{
    struct type *t;
    struct name name;
    size_t length = p->name.length + 1 + strlen(hook);
    char *text = arena_alloc(c->arena, length + 1);

    snprintf(text, length + 1, "%.*s.%s", (int)p->name.length, p->name.text,
             hook);
    name.text = text;
    name.length = length;
    t = types_param(c->types, name);
    t->declared_by = p->declared_by;
    t->hook_owner = p;
    return t;
}

/* A `for` over a value of the parameter p, which its constraints allow
   through `iter`, or through `next` and `value` together. */
bool sema_param_iterate(struct checker *c, struct expr *e, struct type *p,
                        struct type **element)
{
    size_t i;

    if (!has_hook(p, LANG_HOOK_ITER)) {
        const char *missing = NULL;
        if (!has_hook(p, LANG_HOOK_NEXT)) {
            missing = has_hook(p, LANG_HOOK_VALUE) ? LANG_HOOK_NEXT
                                                   : LANG_HOOK_ITER;
        } else if (!has_hook(p, LANG_HOOK_VALUE)) {
            missing = LANG_HOOK_VALUE;
        }
        if (missing != NULL) {
            param_lacks(c, e->pos, "for", p, missing);
            *element = sema_builtin(c, TYPE_ERROR);
            return true;
        }
    }
    /* An interface of walking names the element. */
    for (i = 0; i < p->iface_count; i++) {
        if (walk_iface(p->ifaces[i]) != WALK_NONE) {
            *element = p->ifaces[i]->args[0];
            return true;
        }
    }
    if (p->walked == NULL) {
        p->walked = hook_value(c, p, LANG_HOOK_VALUE);
    }
    *element = p->walked;
    return true;
}

/* `e[i]` on a value of the parameter p, a read through `index` or a
   write through `set_index`. The index is e's. */
struct type *sema_param_index(struct checker *c, struct expr *e,
                              struct type *p, bool write)
{
    const char *hook = write ? LANG_HOOK_SET_INDEX : LANG_HOOK_INDEX;
    struct type *index = sema_check_expr(c, e->as.index.index, NULL);

    if (!has_hook(p, hook)) {
        param_lacks(c, e->pos, write ? "e[i] = v" : "e[i]", p, hook);
        return sema_builtin(c, TYPE_ERROR);
    }
    if (sema_is_error(index)) {
        return index;
    }
    if (p->indexed == NULL) {
        p->indexed = hook_value(c, p, LANG_HOOK_INDEX);
    }
    return p->indexed;
}

bool sema_param_has(const struct type *p, const char *hook)
{
    return has_hook(p, hook);
}

/* `x.hash()` on a value of the parameter p, which its constraints allow
   through `hash`. */
bool sema_param_hash(struct checker *c, struct expr *e, const struct type *p)
{
    if (has_hook(p, LANG_HOOK_HASH)) {
        return true;
    }
    param_lacks(c, e->pos, "hash", p, LANG_HOOK_HASH);
    return false;
}

/* The interface among the constraints of the parameter p that declares
   a function name, or NULL. */
const struct type *sema_param_iface(const struct type *p,
                                    const struct name *name)
{
    size_t i;

    for (i = 0; i < p->iface_count; i++) {
        if (sema_find_member(p->ifaces[i], name) != NULL) {
            return p->ifaces[i];
        }
    }
    return NULL;
}

/* Declarations the checker refuses */

void sema_check_generic_item(struct checker *c, const struct item *it)
{
    size_t j;

    if (it->exported && it->type_param_count > 0) {
        sema_error_at(c, it->name_pos, "an `export %s` cannot be generic. "
                      "Export a named copy with `export type`",
                      it->kind == ITEM_FN ? "fn"
                      : it->kind == ITEM_STRUCT ? "struct"
                      : it->kind == ITEM_VARIANT ? "variant"
                                                 : "class");
    }
    for (j = 0; j < it->member_count; j++) {
        const struct item *m = it->members[j];
        if (m->type_param_count > 0 && m->contract != FN_PLAIN) {
            sema_error_at(c, m->name_pos, "`%.*s` takes type parameters, so "
                          "it cannot be `abstract` or replaced",
                          (int)m->name.length, m->name.text);
        }
    }
    if (it->kind == ITEM_TYPE && it->exported && it->symbol != NULL &&
        !sema_is_error(it->symbol->type) &&
        (it->symbol->type->generic == NULL ||
         (it->symbol->type->kind != TYPE_STRUCT &&
          it->symbol->type->kind != TYPE_CLASS))) {
        sema_error_at(c, it->type->pos, "`export type` names a copy of a "
                      "generic struct or class, and `%s` is none",
                      sema_tn(it->symbol->type));
    }
}

/* Whether the passes after the checker leave the item out. */
static bool stripped(const struct item *it)
{
    const struct item *outer;

    for (outer = it->outer; outer != NULL; outer = outer->outer) {
        if (outer->type_param_count > 0) {
            return true;
        }
    }
    return it->type_param_count > 0 || it->kind == ITEM_TYPE ||
           it->kind == ITEM_CONSTRAINT;
}

void sema_strip_generics(struct module *module, struct arena *arena)
{
    size_t kept = 0;
    size_t i;
    size_t j;

    module->stripped = types_alloc_array(arena, module->item_count + 1,
                                         sizeof *module->stripped);
    module->stripped_count = 0;
    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        size_t members = 0;
        if (stripped(it)) {
            module->stripped[module->stripped_count++] = it;
            continue;
        }
        for (j = 0; j < it->member_count; j++) {
            if (!stripped(it->members[j])) {
                it->members[members++] = it->members[j];
            }
        }
        it->member_count = members;
        if (it->symbol != NULL && it->symbol->type != NULL &&
            it->symbol->type->members == it->members) {
            it->symbol->type->member_count = members;
        }
        module->items[kept++] = it;
    }
    module->item_count = kept;
}

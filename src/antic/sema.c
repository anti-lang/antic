/* The helpers every file of the checker shares: scopes and lookup,
   narrowing, the resolution of types and signatures and the items of the
   standard modules the checker names. The passes over the function
   bodies and over what crosses to C, and sema_check, which runs every
   pass in order. sema_declare.c holds the passes that declare the items. */

#include "sema.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "sema_checker.h"

/* DESIGN: one pass over the syntax tree per module, after every
   module-level name is declared, so an item can be used before its
   declaration. Each expression is checked with the type its context
   expects, which is how a literal gets its type. The checker writes the
   type into every expression and a symbol into every name. */

/* Helpers */

/* The path of the root module, which the checker looks up its standard
   items in. */
static const struct name lang_module = {LANG_MODULE, sizeof LANG_MODULE - 1};

/* The bits of v read as a two's complement int64_t. C leaves the
   conversion of a uint64_t above INT64_MAX to the implementation, so a
   negative value is computed from its complement. */
int64_t sema_signed_bits(uint64_t v)
{
    return v <= INT64_MAX ? (int64_t)v : -(int64_t)~v - 1;
}

void sema_error_at(struct checker *c, struct pos pos, const char *format,
                   ...)
{
    va_list args;

    if (c->ctx.quiet > 0) {
        return;
    }
    va_start(args, format);
    diagnostics_vadd(c->diags, pos.line, pos.column, format, args);
    va_end(args);
    c->ok = false;
}

void sema_check_at(struct checker *c, enum diag_name name, struct pos pos,
                   const char *format, ...)
{
    va_list args;

    if (c->ctx.quiet > 0) {
        return;
    }
    va_start(args, format);
    diagnostics_vcheck(c->diags, name, pos.line, pos.column, format, args);
    va_end(args);
}

bool sema_reported_at(const struct checker *c, struct pos pos,
                      enum diag_name name)
{
    size_t i;

    for (i = 0; i < c->diags->count; i++) {
        const struct diagnostic *d = &c->diags->items[i];
        if (d->name == name && d->line == pos.line &&
            d->column == pos.column) {
            return true;
        }
    }
    return false;
}

/* A type name for a message, kept in one of four rotating buffers so
   that one message can name up to four types. DESIGN: the buffers are
   global state, which antic may keep since it runs on one thread. The
   fifth call writes over the first, so a caller that keeps the text
   while it checks more of the tree copies it, as sema_check_field_inits
   does. */
/* The spelling of the module the checker reads, which sema_tn follows.
   DESIGN: global for the reason of the buffers below. */
static unsigned module_spelled;

const char *sema_tn(const struct type *t)
{
    static char buffers[4][96];
    static size_t next;
    struct text text = {0};
    char *buffer = buffers[next++ % 4];

    types_name_spelled(&text, t, module_spelled);
    text_format(buffer, sizeof buffers[0], "%s", text_cstr(&text));
    text_free(&text);
    return buffer;
}

struct type *sema_builtin(struct checker *c, enum type_kind kind)
{
    return types_builtin(c->types, kind);
}

bool sema_is_error(const struct type *t)
{
    return t == NULL || t->kind == TYPE_ERROR;
}

bool sema_name_is(const struct name *a, const char *text)
{
    return a->length == strlen(text) && memcmp(a->text, text, a->length) == 0;
}

const struct expr *sema_bare_place(const struct expr *e)
{
    while (e->kind == EXPR_UNARY && (e->as.unary.op == TOKEN_AMP ||
                                     e->as.unary.op == TOKEN_STAR)) {
        e = e->as.unary.operand;
    }
    return e;
}

bool sema_same_place(const struct expr *a, const struct expr *b)
{
    a = sema_bare_place(a);
    b = sema_bare_place(b);
    if (a->kind == EXPR_NAME && b->kind == EXPR_NAME) {
        return a->symbol != NULL && a->symbol == b->symbol;
    }
    if (a->kind == EXPR_FIELD && b->kind == EXPR_FIELD) {
        return types_same_name(&a->as.field.name, &b->as.field.name) &&
               sema_same_place(a->as.field.base, b->as.field.base);
    }
    return false;
}

struct expr *sema_new_node(struct checker *c, enum expr_kind kind,
                           struct pos pos)
{
    struct expr *e = arena_alloc(c->arena, sizeof *e);
    e->kind = kind;
    e->pos = pos;
    return e;
}

bool sema_has_body(const struct item *fn)
{
    return fn->body != NULL || fn->runtime != NULL ||
           (fn->symbol != NULL && fn->symbol->home != NULL &&
            fn->contract != FN_ABSTRACT);
}

/* Scopes */

struct symbol *sema_scope_find_local(const struct scope *s,
                                     const struct name *name)
{
    size_t i;

    for (i = 0; i < s->count; i++) {
        if (types_same_name(&s->entries[i].name, name)) {
            return s->entries[i].symbol;
        }
    }
    return NULL;
}

/* The class whose nested types the body of it names: the class that
   declares a member, and the item itself otherwise. */
const struct item *sema_within(const struct item *it)
{
    return it != NULL && it->owner != NULL ? it->owner : it;
}

/* DESIGN: Q20 of "Answers of 2026-10-24". The bare name of a `const`
   of a class body reaches it inside that class: in its fields, their
   defaults, its functions and the types nested in it, whose text stands
   inside the class. A derived class names a constant of its base as
   `Shape.MAX`, and a static field keeps `Name.field`. The constant
   stands at the level of its class, so a local or a parameter of the
   same name wins and an item of the module of the same name is hidden. */
static struct symbol *body_constant(const struct item *it,
                                    const struct name *name)
{
    size_t i;

    for (i = 0; i < it->member_count; i++) {
        const struct item *m = it->members[i];
        if (m->kind == ITEM_CONST && !m->is_static && m->symbol != NULL &&
            types_same_name(&m->name, name)) {
            return m->symbol;
        }
    }
    return NULL;
}

/* DESIGN: a type nested in a class is named as written in the body of
   that class. It is named so in the body of every type nested in it as
   well, and nowhere else. The innermost class wins, then the classes around it, then the
   module. The full name `PeopleList.Node` holds a dot, which no name of
   the source holds, so the module scope never gives it to a name. */
static struct symbol *nested_find(const struct checker *c,
                                  const struct name *name)
{
    const struct item *it;
    size_t i;

    for (it = c->ctx.within; it != NULL; it = it->outer) {
        struct symbol *constant = body_constant(it, name);
        if (constant != NULL) {
            return constant;
        }
        for (i = 0; i < it->nested_count; i++) {
            const struct item *inner = it->nested[i];
            if (inner->symbol != NULL &&
                types_same_name(&inner->local_name, name)) {
                return inner->symbol;
            }
        }
    }
    return NULL;
}

/* An item of the module by name, the constants and the nested types of
   c->ctx.within first. */
struct symbol *sema_module_find(const struct checker *c,
                                const struct name *name)
{
    struct symbol *found = sema_type_param_find(c, name);

    if (found != NULL) {
        return found;
    }
    found = nested_find(c, name);

    return found != NULL ? found
                         : sema_scope_find_local(&c->module_scope, name);
}

struct symbol *sema_lookup(const struct checker *c, const struct name *name)
{
    const struct scope *s;

    for (s = c->ctx.scope; s != NULL; s = s->parent) {
        struct symbol *found = s == &c->module_scope
                                   ? sema_module_find(c, name)
                                   : sema_scope_find_local(s, name);
        if (found != NULL) {
            return found;
        }
    }
    return NULL;
}

/* Modules */

const struct interface *sema_find_library(const struct checker *c,
                                          const struct name *module)
{
    size_t i;

    for (i = 0; i < c->library_count; i++) {
        if (sema_name_is(module, c->libraries[i]->module)) {
            return c->libraries[i];
        }
    }
    return NULL;
}

/* DESIGN: an `internal` item reaches the modules of its own package and
   no others. The compilation names its package, and a library carries
   the name of the package that built it, so the two are compared here.
   From outside the package the item is not there at all, and the caller
   reports the name as missing. */
static bool package_shared(const struct checker *c,
                           const struct interface *lib)
{
    return c->package != NULL && lib->package.name != NULL &&
           strcmp(c->package, lib->package.name) == 0;
}

struct symbol *sema_library_item(const struct checker *c,
                                 const struct interface *lib,
                                 const struct name *name)
{
    size_t i;

    for (i = 0; i < lib->item_count; i++) {
        if (!types_same_name(&lib->items[i]->name, name)) {
            continue;
        }
        if (lib->items[i]->internal && !package_shared(c, lib)) {
            return NULL;
        }
        return lib->items[i];
    }
    return NULL;
}

/* Put a name and its symbol into the scope s. */
void sema_scope_put(struct scope *s, const struct name *name,
                    struct symbol *sym)
{
    s->entries = alloc_grow(s->entries, &s->capacity, s->count,
                            sizeof *s->entries);
    s->entries[s->count].name = *name;
    s->entries[s->count].symbol = sym;
    s->count++;
}

/* Declare a symbol in the current scope. A second name in the same scope
   is an error, a name that an outer scope holds is shadowed. */
struct symbol *sema_declare(struct checker *c, enum symbol_kind kind,
                            const struct name *name, struct pos pos,
                            const char *duplicate_message)
{
    struct scope *s = c->ctx.scope;
    struct symbol *sym;

    if (sema_scope_find_local(s, name) != NULL) {
        sema_error_at(c, pos, duplicate_message, (int)name->length, name->text);
        return NULL;
    }
    sym = arena_alloc(c->arena, sizeof *sym);
    sym->kind = kind;
    sym->name = *name;
    sym->pos = pos;
    sym->frame = c->ctx.function;
    sym->depth = s->depth;
    sym->loops = c->loop_depth;
    sema_scope_put(s, name, sym);
    return sym;
}

/* Whether sym is an item of a library that a direct import put into the
   module scope under its own name. */
bool sema_direct_item(const struct symbol *sym)
{
    return sym != NULL && sym->home != NULL && sym->kind != SYMBOL_MODULE;
}

/* DESIGN: the name after `catch` is any identifier. The object model makes
   a name that an outer scope holds a warning of the checker and not an
   error. Only a variable is reported, because that is what the message
   names. A function or a constant of the module is no variable. The call
   stands inside the handler's own scope, so the lookup starts at the scope
   above it. */
void sema_warn_catch_shadow(struct checker *c, const struct name *name,
                            struct pos pos)
{
    const struct symbol *outer;

    if (c->ctx.quiet > 0 || name->length == 0) {
        return;
    }
    outer = sema_lookup(c, name);
    if (outer == NULL ||
        (outer->kind != SYMBOL_LOCAL && outer->kind != SYMBOL_PARAM)) {
        return;
    }
    diagnostics_warn(c->diags, NAME_SHADOWED_CATCH, pos.line, pos.column,
                     "`%.*s` shadows the outer `%.*s`", (int)name->length,
                     name->text, (int)name->length, name->text);
}

void sema_enter(struct checker *c, const struct context *next,
                struct context *saved)
{
    *saved = c->ctx;
    c->ctx = *next;
}

void sema_leave(struct checker *c, const struct context *saved)
{
    c->ctx = *saved;
}

void sema_enter_quiet(struct checker *c, struct context *saved)
{
    struct context next = c->ctx;

    next.quiet++;
    sema_enter(c, &next, saved);
}

void sema_enter_sized(struct checker *c, struct context *saved)
{
    struct context next = c->ctx;

    next.target_sized = true;
    sema_enter(c, &next, saved);
}

void sema_enter_within(struct checker *c, const struct item *within,
                       struct context *saved)
{
    struct context next = c->ctx;

    next.within = within;
    sema_enter(c, &next, saved);
}

struct context sema_declaration_context(struct checker *c,
                                        const struct item *within)
{
    struct context at;

    memset(&at, 0, sizeof at);
    at.scope = &c->module_scope;
    at.within = within;
    return at;
}

void sema_enter_scope(struct checker *c, struct scope *s)
{
    struct context next = c->ctx;

    memset(s, 0, sizeof *s);
    s->parent = c->ctx.scope;
    s->depth = c->ctx.scope != NULL ? c->ctx.scope->depth + 1 : 0;
    next.scope = s;
    sema_enter(c, &next, &s->outer);
}

void sema_leave_scope(struct checker *c, struct scope *s)
{
    sema_leave(c, &s->outer);
    free(s->entries);
    free(s->narrowed);
}

/* The type a check proved for sym, or NULL when no enclosing block holds
   one. The innermost record wins, so a block that put the declared type
   back ends the narrowing of every block outside it as well. */
struct type *sema_narrowed_type(const struct checker *c,
                                const struct symbol *sym)
{
    const struct scope *s;
    size_t i;

    for (s = c->ctx.scope; s != NULL; s = s->parent) {
        for (i = 0; i < s->narrowed_count; i++) {
            if (s->narrowed[i].symbol == sym) {
                return s->narrowed[i].type;
            }
        }
    }
    return NULL;
}

/* Record that sym has type t for the rest of the current block. */
void sema_narrow(struct checker *c, struct symbol *sym, struct type *t)
{
    struct scope *s = c->ctx.scope;
    size_t i;

    for (i = 0; i < s->narrowed_count; i++) {
        if (s->narrowed[i].symbol == sym) {
            s->narrowed[i].type = t;
            return;
        }
    }
    s->narrowed = alloc_grow(s->narrowed, &s->narrowed_capacity,
                             s->narrowed_count, sizeof *s->narrowed);
    s->narrowed[s->narrowed_count].symbol = sym;
    s->narrowed[s->narrowed_count].type = t;
    s->narrowed_count++;
}

/* An assignment to sym ends every narrowing of it, in the block that
   holds the record and so in every block inside that one. */
void sema_end_narrowing(struct checker *c, const struct symbol *sym)
{
    struct scope *s;
    size_t i;

    for (s = c->ctx.scope; s != NULL; s = s->parent) {
        for (i = 0; i < s->narrowed_count; i++) {
            if (s->narrowed[i].symbol == sym) {
                s->narrowed[i].type = sym->type;
            }
        }
    }
}

/* Types */

static struct type *builtin_of_token(struct checker *c, enum token_kind k)
{
    switch (k) {
    case TOKEN_BOOL_TYPE: return sema_builtin(c, TYPE_BOOL);
    case TOKEN_CHAR_TYPE: return sema_builtin(c, TYPE_CHAR);
    case TOKEN_I8:
    case TOKEN_C_CHAR: return sema_builtin(c, TYPE_I8);
    case TOKEN_I16:
    case TOKEN_C_SHORT: return sema_builtin(c, TYPE_I16);
    case TOKEN_I32:
    case TOKEN_C_INT: return sema_builtin(c, TYPE_I32);
    case TOKEN_I64:
    case TOKEN_INT_TYPE:
    case TOKEN_C_LONGLONG: return sema_builtin(c, TYPE_I64);
    case TOKEN_U8:
    case TOKEN_BYTE_TYPE:
    case TOKEN_C_UCHAR: return sema_builtin(c, TYPE_U8);
    case TOKEN_U16:
    case TOKEN_C_USHORT: return sema_builtin(c, TYPE_U16);
    case TOKEN_U32:
    case TOKEN_C_UINT: return sema_builtin(c, TYPE_U32);
    case TOKEN_U64:
    case TOKEN_UINT_TYPE:
    case TOKEN_C_ULONGLONG:
    case TOKEN_C_SIZE_T: return sema_builtin(c, TYPE_U64);
    case TOKEN_C_LONG: return sema_builtin(c, TYPE_CLONG);
    case TOKEN_C_ULONG: return sema_builtin(c, TYPE_CULONG);
    case TOKEN_C_WCHAR: return sema_builtin(c, TYPE_CWCHAR);
    case TOKEN_F16: return sema_builtin(c, TYPE_F16);
    case TOKEN_F32:
    case TOKEN_C_FLOAT: return sema_builtin(c, TYPE_F32);
    case TOKEN_F64:
    case TOKEN_FLOAT_TYPE:
    case TOKEN_C_DOUBLE: return sema_builtin(c, TYPE_F64);
    case TOKEN_STR_TYPE: return sema_builtin(c, TYPE_STR);
    default: return sema_builtin(c, TYPE_ERROR);
    }
}

/* Whether s is built of constant parameters and integers alone, with no
   `size_of`. */
static bool params_alone(const struct symbolic *s)
{
    if (s == NULL) {
        return true;
    }
    switch (s->kind) {
    case SYMBOLIC_INT:
    case SYMBOLIC_PARAM:
        return true;
    case SYMBOLIC_SIZE_OF:
        return false;
    default:
        return params_alone(s->a) && params_alone(s->b);
    }
}

/* A constant expression of type int with a value of at least 1, or a
   symbolic value. DESIGN: a length computed from size_of stays symbolic,
   and the back end checks that it is at least 1 on its target. It
   stands in a struct field and a local variable alone. A length computed from
   constant parameters alone is an integer in every copy, so it stands
   wherever a length does. */
struct type *sema_array_of(struct checker *c, struct expr *e,
                           struct type *element)
{
    struct const_value v;

    if (sema_refuse_runtime_parts(c, e) ||
        sema_is_error(sema_check_expr(c, e, sema_builtin(c, TYPE_I64))) ||
        sema_is_error(element)) {
        return sema_builtin(c, TYPE_ERROR);
    }
    if (e->type->kind != TYPE_I64) {
        sema_error_at(c, e->pos, "an array length has type `int`, found `%s`",
                      sema_tn(e->type));
        return sema_builtin(c, TYPE_ERROR);
    }
    if (!sema_eval_const(c, e, &v)) {
        return sema_builtin(c, TYPE_ERROR);
    }
    if (v.kind == CONST_SYMBOLIC) {
        if (!c->ctx.target_sized && !params_alone(v.as.symbolic)) {
            sema_error_at(c, e->pos,
                          "a length computed from `size_of` is allowed "
                          "only in a struct field or a local variable");
            return sema_builtin(c, TYPE_ERROR);
        }
        return types_array_symbolic(c->types, element, v.as.symbolic);
    }
    if (sema_signed_bits(v.as.integer) < 1) {
        sema_error_at(c, e->pos, "an array length is at least 1");
        return sema_builtin(c, TYPE_ERROR);
    }
    return types_array(c->types, element, v.as.integer);
}

/* The library that the module name refers to, or NULL after an error. */
static const struct interface *module_of(struct checker *c,
                                         const struct name *module,
                                         struct pos pos)
{
    struct symbol *sym = sema_lookup(c, module);

    if (sym == NULL || sym->kind != SYMBOL_MODULE) {
        sema_error_at(c, pos, "cannot find module `%.*s`", (int)module->length,
                      module->text);
        return NULL;
    }
    return sym->home;
}

/* The type of module.name, a pub struct of an imported module. */
struct type *sema_imported_struct(struct checker *c,
                                  const struct name *module,
                                  const struct name *name, struct pos pos)
{
    const struct interface *lib = module_of(c, module, pos);
    struct symbol *sym;

    if (lib == NULL) {
        return sema_builtin(c, TYPE_ERROR);
    }
    sym = sema_library_item(c, lib, name);
    if (sym == NULL || sym->kind != SYMBOL_STRUCT) {
        sema_error_at(c, pos, "`%.*s` has no public struct `%.*s`",
                      (int)module->length, module->text, (int)name->length,
                      name->text);
        return sema_builtin(c, TYPE_ERROR);
    }
    return sym->type;
}

/* DESIGN: f16 is storage: sixteen bits in a field, an array, a slice or
   a variable. What a function takes and gives is a value. The value of
   an f16 is the f32 that a read gives, so neither a parameter nor a
   result is f16. C's __fp16 follows the same rule. */
static bool refuses_half_value(struct checker *c, struct pos pos,
                               const struct type *t, const char *what)
{
    if (t->kind != TYPE_F16) {
        return false;
    }
    sema_error_at(c, pos, "%s cannot be `f16`, which is storage only", what);
    return true;
}

/* An operator on an f16. A read of one is an f32 already, so the operand
   is an `as f16`, which the program converts back itself. */
bool sema_refuses_half(struct checker *c, struct pos pos,
                       const struct type *t)
{
    if (t->kind != TYPE_F16) {
        return false;
    }
    sema_error_at(c, pos, "`f16` has no arithmetic, convert with `as f32`");
    return true;
}

/* DESIGN: a channel carries values under the rule of `parallel`, so
   no two threads reach one piece of memory through what it carries. A
   channel of a type parameter is held to the rule in each copy, where
   the argument stands, by T058 of docs/audit/tests-findings.md. */
bool sema_refuse_chan_element(struct checker *c, struct pos pos,
                              const struct type *element)
{
    if (sema_is_error(element) || types_pointer_free(element)) {
        return false;
    }
    sema_error_at(c, pos, "a channel carries values alone, and `%s` holds "
                  "a pointer", sema_tn(element));
    return true;
}

struct type *sema_chan_element(struct checker *c, struct type_expr *t)
{
    struct type *element = sema_resolve_type(c, t);

    if (sema_refuse_chan_element(c, t->pos, element)) {
        return sema_builtin(c, TYPE_ERROR);
    }
    return element;
}

/* A type of an imported module, `module.Name`. */
static struct type *resolve_imported(struct checker *c, struct type_expr *t)
{
    struct type *imported =
        sema_imported_struct(c, &t->module, &t->name, t->pos);

    /* A generic of a library is named with its arguments, as one of the
       module is. */
    if (!sema_is_error(imported) && imported->type_param_count > 0) {
        if (t->arg_count == 0) {
            sema_error_at(c, t->pos, "`%s` takes %zu type argument%s",
                          sema_tn(imported), imported->type_param_count,
                          imported->type_param_count == 1 ? "" : "s");
            return sema_builtin(c, TYPE_ERROR);
        }
        return sema_copy_of(c, imported, t->args, t->arg_count, t->pos);
    }
    if (t->arg_count > 0 && !sema_is_error(imported)) {
        sema_error_at(c, t->pos, "`%.*s.%.*s` is not generic",
                      (int)t->module.length, t->module.text,
                      (int)t->name.length, t->name.text);
        return sema_builtin(c, TYPE_ERROR);
    }
    return imported;
}

/* A type the module declares or names in `anti.lang`, `Name` or
   `Name<Args>`. */
static struct type *resolve_named(struct checker *c, struct type_expr *t)
{
    struct symbol *sym;
    struct type *declared;

    if (t->module.length > 0) {
        return resolve_imported(c, t);
    }
    sym = sema_module_find(c, &t->name);
    /* DESIGN: `Object` is the root of every class chain, which the
       compiler declares. A program writes the name where the object model
       uses it, as in `equals(self, other: *Object)`, and a class of that
       name in the module wins over it. The other types the compiler
       declares in `anti.lang` resolve the same way. */
    if (sym == NULL &&
        (declared = types_declared(c->types, &t->name, false)) != NULL) {
        return declared;
    }
    /* DESIGN: `never` is a result alone, written after `->`, and the type
       has no values. Elsewhere the word names a type of the program, and
       without one the message says why. */
    if (sym == NULL && sema_name_is(&t->name, "never")) {
        sema_error_at(c, t->pos, "`never` is the result of a function "
                      "that does not return, and no value has it");
        return sema_builtin(c, TYPE_ERROR);
    }
    if (sym == NULL || sym->kind != SYMBOL_STRUCT) {
        sema_error_at(c, t->pos, "unknown type `%.*s`", (int)t->name.length,
                      t->name.text);
        return sema_builtin(c, TYPE_ERROR);
    }
    if (sym->item != NULL && sym->item->kind == ITEM_TYPE) {
        struct type *aliased = sema_alias_type(c, sym);
        if (t->arg_count > 0 && !sema_is_error(aliased)) {
            sema_error_at(c, t->pos, "`%.*s` is not generic",
                          (int)t->name.length, t->name.text);
            return sema_builtin(c, TYPE_ERROR);
        }
        return aliased;
    }
    /* A generic is named with its type arguments, and the name of
       anything else takes none. */
    if (sym->type != NULL && sym->type->type_param_count > 0) {
        if (t->arg_count == 0 && sym->type->nested_in != NULL &&
            sema_nested_own(sym->type) == 0) {
            return sym->type;
        }
        if (t->arg_count == 0) {
            sema_error_at(c, t->pos, "`%s` takes %zu type argument%s",
                          sema_tn(sym->type),
                          sym->type->type_param_count,
                          sym->type->type_param_count == 1 ? "" : "s");
            return sema_builtin(c, TYPE_ERROR);
        }
        return sema_copy_of(c, sym->type, t->args, t->arg_count, t->pos);
    }
    if (t->arg_count > 0 && sym->type != NULL &&
        !sema_is_error(sym->type)) {
        sema_error_at(c, t->pos, "`%.*s` is not generic",
                      (int)t->name.length, t->name.text);
        return sema_builtin(c, TYPE_ERROR);
    }
    /* A type whose own declaration names it, as `enum A: A`, has no
       type yet while that declaration is resolved. */
    if (sym->type == NULL) {
        sema_error_at(c, t->pos, "`%.*s` names itself in its own "
                      "declaration", (int)t->name.length, t->name.text);
        return sema_builtin(c, TYPE_ERROR);
    }
    return sym->type;
}

/* A function type, `fn(A) -> R` with its marks. */
static struct type *resolve_fn_type(struct checker *c, struct type_expr *t)
{
    struct type **params =
        types_alloc_array(c->arena, t->param_count + 1, sizeof *params);
    struct type *result = sema_builtin(c, TYPE_VOID);
    struct type *fn;
    size_t i;

    for (i = 0; i < t->param_count; i++) {
        params[i] = sema_param_form(c, sema_resolve_type(c, t->params[i]),
                                    t->params[i]->keep,
                                    t->params[i]->concurrent,
                                    t->params[i]->owned,
                                    t->params[i]->pos);
        params[i] = sema_lent_form(c, params[i], t->params[i]->lent, false,
                                   t->params[i]->pos);
        if (sema_is_error(params[i])) {
            return params[i];
        }
        if (refuses_half_value(c, t->params[i]->pos, params[i],
                               "a parameter")) {
            return sema_builtin(c, TYPE_ERROR);
        }
    }
    if (t->result != NULL &&
        sema_is_error(result = sema_resolve_type(c, t->result))) {
        return result;
    }
    if (t->result != NULL &&
        refuses_half_value(c, t->result->pos, result, "a result")) {
        return sema_builtin(c, TYPE_ERROR);
    }
    /* `fn(A) -> R may fail` takes the ABI form a `may fail` function
       has, so a value of it holds such a function as it is. */
    if (t->may_fail) {
        struct type *error = sema_error_class(c, t->pos);
        if (error == NULL) {
            return sema_builtin(c, TYPE_ERROR);
        }
        if (t->result != NULL) {
            params[t->param_count] = types_pointer(c->types, result);
        }
        fn = types_fn_failing(c->types, params,
                              t->param_count + (t->result != NULL ? 1 : 0),
                              types_pointer_nullable(c->types, error),
                              t->result != NULL);
    } else {
        fn = types_fn(c->types, params, t->param_count, result);
    }
    if (t->never) {
        fn = types_fn_never(c->types, fn);
    }
    return t->nullable ? types_with_none(c->types, fn) : fn;
}

static struct type *resolve_type_inner(struct checker *c, struct type_expr *t)
{
    struct type *element;
    size_t i;

    switch (t->kind) {
    case TYPEX_BUILTIN:
        return builtin_of_token(c, t->builtin);
    /* `?T` of any type. Before a pointer or a function type it is the
       `?*T` or `?fn(...)` that holds `none` as zero, and before a type
       that may already be `none` it is a `?T` of its own. */
    case TYPEX_OPTIONAL:
        return types_with_none(c->types, sema_resolve_type(c, t->element));
    case TYPEX_NAMED:
        return resolve_named(c, t);
    case TYPEX_POINTER:
        element = sema_resolve_type(c, t->element);
        return sema_is_error(element)
                   ? element
                   : types_pointer_of(c->types, element, t->nullable);
    case TYPEX_SLICE:
        element = sema_resolve_type(c, t->element);
        return sema_is_error(element) ? element : types_slice(c->types,
                                                              element);
    case TYPEX_ARRAY:
        return sema_array_of(c, t->length, sema_resolve_type(c, t->element));
    case TYPEX_FN:
        return resolve_fn_type(c, t);
    case TYPEX_TUPLE: {
        /* DESIGN: a part of a tuple is written `lent *T` in the result
           of `operator fn value` alone, at the top level of the tuple.
           A map whose keys and values stand apart gives `(K, lent *V)`,
           and the loop binds what it gives. */
        struct type **elements =
            types_alloc_array(c->arena, t->param_count, sizeof *elements);
        bool value_result = c->ctx.value_result;
        struct context next = c->ctx;
        struct context saved;
        next.value_result = false;
        sema_enter(c, &next, &saved);
        for (i = 0; i < t->param_count; i++) {
            elements[i] = sema_resolve_type(c, t->params[i]);
            if (!sema_is_error(elements[i]) && t->params[i]->lent &&
                !value_result) {
                sema_error_at(c, t->params[i]->pos, "`lent` stands before "
                              "the result of `operator fn value` alone");
                elements[i] = sema_builtin(c, TYPE_ERROR);
            } else if (!sema_is_error(elements[i]) && t->params[i]->lent &&
                       elements[i]->kind != TYPE_POINTER) {
                sema_error_at(c, t->params[i]->pos, "`lent` marks a pointer "
                              "part, and this one is `%s`",
                              sema_tn(elements[i]));
                elements[i] = sema_builtin(c, TYPE_ERROR);
            } else if (!sema_is_error(elements[i]) && t->params[i]->lent) {
                elements[i] = types_lent(c->types, elements[i]);
            }
            if (sema_is_error(elements[i])) {
                sema_leave(c, &saved);
                return elements[i];
            }
        }
        sema_leave(c, &saved);
        return types_tuple(c->types, elements, t->param_count);
    }
    case TYPEX_CHAN:
        element = sema_chan_element(c, t->element);
        return sema_is_error(element) ? element : types_chan(c->types, element);
    case TYPEX_CONST:
        sema_error_at(c, t->pos, "expected a type, found a constant");
        return sema_builtin(c, TYPE_ERROR);
    }
    return sema_builtin(c, TYPE_ERROR);
}

/* DESIGN: a parameter of function type does not keep its argument unless
   it is marked `keep`. It then takes the form of two words, the code and
   a context, and `concurrent` marks the form that may be called from more
   than one thread at once. A `keep` parameter holds the one C function
   pointer, as a field and a global do, and so does every parameter of an
   `extern fn`, since a closure cannot reach C. The two marks belong to a
   parameter of function type alone, and they do not stand together: a
   kept function captures nothing, so it is safe on every thread.
   `keep own` keeps and owns an `own fn`, whose snapshot the function
   frees unless it moves it on. */
struct type *sema_param_form(struct checker *c, struct type *t, bool keep,
                             bool concurrent, bool owned, struct pos pos)
{
    bool fn = t->kind == TYPE_FN && !t->bound;

    if (sema_is_error(t)) {
        return t;
    }
    if ((keep || concurrent) && !fn) {
        sema_error_at(c, pos, "`%s` marks a parameter of function type, and "
                      "this one is `%s`", keep ? "keep" : "concurrent",
                      sema_tn(t));
        return sema_builtin(c, TYPE_ERROR);
    }
    if (keep && concurrent) {
        sema_error_at(c, pos, "`keep` and `concurrent` do not stand together, "
                      "since a kept function captures nothing");
        return sema_builtin(c, TYPE_ERROR);
    }
    if (!fn) {
        return t;
    }
    if (c->ctx.plain_fns && concurrent) {
        sema_error_at(c, pos, "an `extern fn` takes plain C function "
                      "pointers, and `concurrent` marks a closure");
        return sema_builtin(c, TYPE_ERROR);
    }
    if (owned && c->ctx.plain_fns) {
        sema_error_at(c, pos, "an `extern fn` takes plain C function "
                      "pointers, and `keep own` holds a snapshot");
        return sema_builtin(c, TYPE_ERROR);
    }
    if (owned && !keep) {
        sema_error_at(c, pos, "a parameter that owns a function keeps it, "
                      "and is written `keep own`");
        return sema_builtin(c, TYPE_ERROR);
    }
    if (owned) {
        return types_fn_owned(c->types, t);
    }
    return types_fn_form(c->types, t, !keep && !c->ctx.plain_fns, concurrent);
}

/* DESIGN: `lent` marks a pointer parameter whose pointer is valid only
   during the call, and the parameter then has the type `lent *T`. A slice
   parameter takes `lent []T` under the same rules. The
   function may pass it on to another `lent` parameter and keeps it
   nowhere, which the checker enforces through the type as it enforces
   `keep`. It does not stand with `own`, which takes the object over,
   and an `extern fn` refuses it, since C keeps what it likes. */
struct type *sema_lent_form(struct checker *c, struct type *t, bool lent,
                            bool owned, struct pos pos)
{
    if (!lent || sema_is_error(t)) {
        return t;
    }
    if (t->kind != TYPE_POINTER && t->kind != TYPE_SLICE) {
        sema_error_at(c, pos, "`lent` marks a pointer or slice parameter, and "
                      "this one is `%s`", sema_tn(t));
        return sema_builtin(c, TYPE_ERROR);
    }
    if (owned) {
        sema_error_at(c, pos, "`own` and `lent` do not stand together, since "
                      "a lent %s stays with its owner",
                      t->kind == TYPE_SLICE ? "slice" : "pointer");
        return sema_builtin(c, TYPE_ERROR);
    }
    if (c->ctx.plain_fns) {
        sema_error_at(c, pos, "an `extern fn` takes no `lent` parameter, "
                      "since C may keep the pointer");
        return sema_builtin(c, TYPE_ERROR);
    }
    return types_lent(c->types, t);
}

/* Resolve t and record the result in the node for later stages. A
   `type` line and a copy of a generic put a type inside another, so a
   type may nest deeper than any the program writes. */
struct type *sema_resolve_type(struct checker *c, struct type_expr *t)
{
    struct type *resolved = resolve_type_inner(c, t);

    if (resolved->depth > TYPES_NEST_MAX) {
        sema_error_at(c, t->pos, "the type nests deeper than %d levels",
                      TYPES_NEST_MAX);
        resolved = sema_builtin(c, TYPE_ERROR);
    }
    t->type = resolved;
    return t->type;
}

/* The struct or class name of the standard module module, from its
   library file. With own set, the module being checked may be that
   module and declare name itself, as `anti.lang` declares its error
   class. NULL when neither declares a struct or class of that name. */
struct symbol *sema_std_item(struct checker *c, const struct name *module,
                             const struct name *name, bool own)
{
    const struct interface *lib = sema_find_library(c, module);
    struct symbol *sym = lib != NULL ? sema_library_item(c, lib, name) : NULL;

    if (sym == NULL && own && types_same_name(&c->module_name, module)) {
        sym = sema_lookup(c, name);
    }
    return sym != NULL && sym->kind == SYMBOL_STRUCT && sym->type != NULL
               ? sym
               : NULL;
}

/* The function text of the standard module module, which what calls with
   count arguments, or NULL after an error at pos. what names the form
   that calls it and the verb, "`s.matches` calls", as a message reads
   it. The callers index its parameters
   by the position of each argument, so a function of fewer parameters is
   refused here. A damaged or older library file gave one, and the checker
   read past its parameters (S14 of the audit). */
struct symbol *sema_std_function(struct checker *c, struct pos pos,
                                 const char *what, const struct name *module,
                                 const char *text, size_t count)
{
    const struct interface *lib = sema_find_library(c, module);
    struct name name;
    struct symbol *f;

    if (lib == NULL) {
        sema_error_at(c, pos, "%s `%.*s.%s`, so the module imports `%.*s`",
                      what, (int)module->length, module->text, text,
                      (int)module->length, module->text);
        return NULL;
    }
    name.text = text;
    name.length = strlen(text);
    f = sema_library_item(c, lib, &name);
    if (f == NULL || f->kind != SYMBOL_FN || f->type == NULL ||
        f->type->kind != TYPE_FN) {
        sema_error_at(c, pos, "%s `%.*s.%s`, which this `%.*s` lacks", what, (int)module->length, module->text, text,
                      (int)module->length, module->text);
        return NULL;
    }
    if (f->type->param_count < count) {
        sema_error_at(c, pos, "%s `%.*s.%s` with %zu arguments, and "
                      "this `%.*s` gives it %zu parameter%s", what,
                      (int)module->length, module->text, text, count,
                      (int)module->length, module->text,
                      f->type->param_count,
                      f->type->param_count == 1 ? "" : "s");
        return NULL;
    }
    return f;
}

/* DESIGN: `may fail` gives a function the convention a program used to
   write by hand: `?*lang.Error` as the result and an out pointer for
   what it computes. The class is an ordinary imported one, so a module
   that writes the form imports `anti.lang` as it does for every error it
   names. */
struct type *sema_error_class(struct checker *c, struct pos pos)
{
    static const struct name class_name = {LANG_ERROR, sizeof LANG_ERROR - 1};
    struct symbol *sym = sema_std_item(c, &lang_module, &class_name, true);

    if (sym == NULL || sym->type->kind != TYPE_CLASS) {
        sema_error_at(c, pos, "`may fail` gives `?*" LANG_MODULE "." LANG_ERROR
                      "`, so the module imports `" LANG_MODULE "`");
        return NULL;
    }
    return sym->type;
}

/* The struct `anti.lang.SourceLocation`, which `here` gives, or NULL
   after an error at pos. */
struct type *sema_location_type(struct checker *c, struct pos pos)
{
    static const struct name type_name = {
        LANG_SOURCE_LOCATION, sizeof LANG_SOURCE_LOCATION - 1};
    struct symbol *sym = sema_std_item(c, &lang_module, &type_name, true);

    if (sym == NULL || sym->type->kind != TYPE_STRUCT) {
        sema_error_at(c, pos, "`here` gives an `" LANG_MODULE "."
                      LANG_SOURCE_LOCATION "`, so the module imports `"
                      LANG_MODULE "`");
        return NULL;
    }
    return sym->type;
}

/* The class `anti.lang.Error` when the compilation carries `anti.lang`,
   and NULL where it does not. The root's `failed` hook names it, and a
   program without that module cannot name it either. */
struct type *sema_lang_error_or_null(struct checker *c)
{
    static const struct name class_name = {LANG_ERROR, sizeof LANG_ERROR - 1};
    struct symbol *sym = sema_std_item(c, &lang_module, &class_name, false);

    if (sym == NULL || sym->type->kind != TYPE_CLASS) {
        return NULL;
    }
    return sym->type;
}

/* The type `*anti.mem.Allocator`, or NULL when the module does not
   import `anti.mem`. */
static struct type *allocator_pointer(struct checker *c)
{
    static const struct name module = {MEM_MODULE, sizeof MEM_MODULE - 1};
    static const struct name class_name = {MEM_ALLOCATOR,
                                           sizeof MEM_ALLOCATOR - 1};
    struct symbol *sym = sema_std_item(c, &module, &class_name, true);

    if (sym == NULL || sym->type->kind != TYPE_CLASS) {
        return NULL;
    }
    return types_pointer(c->types, sym->type);
}

/* DESIGN: `Object.deserialize` takes the allocator of the memory it makes
   as a `*anti.mem.Allocator`. The type of a use names that class, so an
   argument converts to it as to any parameter, through the sub-object of
   an interface as well. The module imports `anti.mem`, as the one that
   writes `here` imports `anti.lang`. The type of the use, or NULL after an
   error at pos. */
struct type *sema_deserialize_type(struct checker *c, struct pos pos,
                                   const struct type *declared)
{
    struct type *from = allocator_pointer(c);
    struct type **params;

    if (from == NULL) {
        sema_error_at(c, pos,
                      "`" LANG_OBJECT "." ROOT_DESERIALIZE "` takes its "
                      "memory from an `" MEM_MODULE "." MEM_ALLOCATOR
                      "`, so the module imports `" MEM_MODULE "`");
        return NULL;
    }
    params = arena_alloc(c->arena, 2 * sizeof *params);
    params[0] = declared->params[0];
    params[1] = from;
    return types_fn(c->types, params, 2, declared->result);
}

/* DESIGN: `delete(p, from)` and `destroy(p, from)` give the memory back
   to the allocator it came from, a `*anti.mem.Allocator`, which converts
   as any argument does. The module imports `anti.mem`, as the one that
   calls `Object.deserialize` does. Whether the allocator of e checks. */
bool sema_check_object_from(struct checker *c, struct expr *e,
                            const char *what)
{
    struct type *expected = allocator_pointer(c);
    struct type *got;

    if (expected == NULL) {
        sema_error_at(c, e->as.object.from->pos, "`%s` with an allocator gives "
                      "the memory back to an `" MEM_MODULE "."
                      MEM_ALLOCATOR "`, so the module imports `" MEM_MODULE
                      "`", what);
        return false;
    }
    got = sema_check_expr(c, e->as.object.from, expected);
    return sema_require(c, e->as.object.from, got, expected);
}

/* The type of a function item, fn(params) -> result. A function of a
   struct body that takes self has a first parameter of type *T. */
static struct type *function_type_of(struct checker *c, struct item *it);

/* DESIGN: the types of a signature name the type parameters of the
   function, so the function is the signature being resolved while its
   types are. */
struct type *sema_function_type(struct checker *c, struct item *it)
{
    struct context next = c->ctx;
    struct context saved;
    struct type *t;

    next.signature = it;
    sema_enter(c, &next, &saved);
    t = function_type_of(c, it);
    sema_leave(c, &saved);
    return t;
}

static struct type *function_type_of(struct checker *c, struct item *it)
{
    size_t extra = it->has_self ? 1 : 0;
    size_t out = it->may_fail && it->result != NULL ? 1 : 0;
    struct type **params = types_alloc_array(
        c->arena, it->param_count + extra + out + 1, sizeof *params);
    struct type *result = sema_builtin(c, TYPE_VOID);
    struct context next;
    struct context saved;
    size_t i;

    if (it->has_self) {
        const struct item *owner = it->owner;
        if (owner == NULL || owner->symbol == NULL ||
            owner->symbol->type == NULL) {
            return sema_builtin(c, TYPE_ERROR);
        }
        params[0] = types_pointer(c->types, owner->symbol->type);
    }
    next = c->ctx;
    next.plain_fns = it->kind == ITEM_EXTERN_FN;
    for (i = 0; i < it->param_count; i++) {
        sema_enter(c, &next, &saved);
        params[i + extra] = sema_param_form(
            c, sema_resolve_type(c, it->params[i].type), it->params[i].keep,
            it->params[i].concurrent,
            it->params[i].owned && it->params[i].type->kind == TYPEX_FN,
            it->params[i].pos);
        params[i + extra] =
            sema_lent_form(c, params[i + extra], it->params[i].lent,
                           it->params[i].owned, it->params[i].pos);
        sema_leave(c, &saved);
        if (sema_is_error(params[i + extra])) {
            return params[i + extra];
        }
        if (refuses_half_value(c, it->params[i].type->pos, params[i + extra],
                               "a parameter")) {
            return sema_builtin(c, TYPE_ERROR);
        }
    }
    next = c->ctx;
    next.value_result = it->is_operator && it->result != NULL &&
                        sema_name_is(&it->name, LANG_HOOK_VALUE);
    if (it->result != NULL) {
        sema_enter(c, &next, &saved);
        result = sema_resolve_type(c, it->result);
        sema_leave(c, &saved);
    }
    if (it->result != NULL && sema_is_error(result)) {
        return result;
    }
    if (it->result != NULL &&
        refuses_half_value(c, it->result->pos, result, "a result")) {
        return sema_builtin(c, TYPE_ERROR);
    }
    /* DESIGN: `-> lent *T` is the result of `operator fn value` alone. It
       lends an element of a collection for one turn of a loop, so that
       `for x in &c` walks the elements in place and `for x in c` copies
       each one. */
    if (it->result_lent) {
        if (!it->is_operator || !sema_name_is(&it->name, LANG_HOOK_VALUE)) {
            sema_error_at(c, it->result_lent_pos, "`lent` stands before the "
                          "result of `operator fn value` alone");
        } else if (result->kind != TYPE_POINTER) {
            sema_error_at(c, it->result_lent_pos, "`lent` marks a pointer "
                          "result, and this one is `%s`", sema_tn(result));
        } else {
            result = types_lent(c->types, result);
        }
    }
    /* DESIGN: `construct` and `destruct` keep the forms the object model
       gives them. A `construct` with arguments that can fail is written
       `may fail` and names no result, so `-> ?*Error` written by hand is
       refused. A `construct` without arguments runs after every literal
       and cannot fail. `destruct` has no error channel at all. */
    if (it->has_self && sema_name_is(&it->name, "construct") &&
        it->param_count > 0 && it->result != NULL) {
        sema_error_at(c, it->result->pos,
                      "`construct` returns nothing, and one "
                      "that can fail is written `may fail`");
        return sema_builtin(c, TYPE_ERROR);
    }
    if (it->never && it->may_fail) {
        sema_error_at(c, it->may_fail_pos,
                      "a function that returns `never` cannot fail");
        return sema_builtin(c, TYPE_ERROR);
    }
    if (it->never) {
        return types_fn_never(c->types,
                              types_fn(c->types, params,
                                       it->param_count + extra, result));
    }
    if (it->may_fail) {
        struct type *error;
        if (it->has_self && sema_name_is(&it->name, "construct") &&
            it->param_count == 0) {
            sema_error_at(c, it->may_fail_pos,
                          "a `construct` without arguments "
                          "cannot fail and returns nothing");
            return sema_builtin(c, TYPE_ERROR);
        }
        if (it->has_self && sema_name_is(&it->name, "destruct")) {
            sema_error_at(c, it->may_fail_pos, "`destruct` cannot fail");
            return sema_builtin(c, TYPE_ERROR);
        }
        error = sema_error_class(c, it->may_fail_pos);
        if (error == NULL) {
            return sema_builtin(c, TYPE_ERROR);
        }
        if (out == 1) {
            params[it->param_count + extra] = types_pointer(c->types, result);
        }
        return types_fn_failing(c->types, params,
                                it->param_count + extra + out,
                                types_pointer_nullable(c->types, error),
                                out == 1);
    }
    return types_fn(c->types, params, it->param_count + extra, result);
}

static void check_free_functions(struct checker *c)
{
    const struct module *module = c->module;
    size_t i;

    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        if (it->symbol == NULL || it->kind != ITEM_FN) {
            continue;
        }
        sema_check_function(c, it);
        if (sema_name_is(&it->name, "main") &&
            !sema_is_error(it->symbol->type)) {
            sema_check_main(c, it);
        }
        if (it->block != BLOCK_NONE && !sema_is_error(it->symbol->type)) {
            sema_check_test_block(c, it);
        }
    }
}

/* A function of a body is checked like a free function, with `self`
   declared as a parameter of type *T. */
static void check_member_functions(struct checker *c)
{
    const struct module *module = c->module;
    size_t i;
    size_t j;

    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        for (j = 0; j < it->member_count; j++) {
            struct item *m = it->members[j];
            if (m->symbol == NULL || m->kind != ITEM_FN || m->body == NULL) {
                continue;
            }
            sema_check_function(c, m);
        }
    }
}

/* Whether a class of the program fills t, or a copy of t when t is
   generic. */
static bool filled_or_copied(const struct checker *c, const struct type *t)
{
    const struct type *copy;

    if (sema_filled_somewhere(c, t)) {
        return true;
    }
    for (copy = t->copies; copy != NULL; copy = copy->next_copy) {
        if (sema_filled_somewhere(c, copy)) {
            return true;
        }
    }
    return false;
}

/* DESIGN: the pass that only reports runs last, over every function a
   worker can reach and over the abstract classes of the program. It
   changes nothing, so a build that skips it still compiles the same
   program. */
/* Whether a class of the module names the class it as its base, and
   that base was refused. The refusal is the one message of the mistake,
   by T096 of docs/audit/tests-findings.md, and the class it meant to
   fill is no class that nothing fills. */
static bool named_by_refused_base(const struct module *module,
                                  const struct item *it)
{
    size_t i;

    for (i = 0; i < module->item_count; i++) {
        const struct item *below = module->items[i];
        if (below->kind == ITEM_CLASS && below->base_name.length > 0 &&
            below->base_module.length == 0 && below->symbol != NULL &&
            below->symbol->type != NULL && below->symbol->type->base == NULL &&
            types_same_name(&below->base_name, &it->name)) {
            return true;
        }
    }
    return false;
}

static void report_program(struct checker *c)
{
    const struct module *module = c->module;
    size_t i;

    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        if (it->kind != ITEM_FN || !it->worker || it->body == NULL) {
            continue;
        }
        sema_walk_worker(c, it);
    }
    /* An abstract class that no class of the program fills has no value
       and no use. A build that writes a program sees every class, so it
       can say so. A library build sees no program: `anti.lang` writes
       `TraceHandler` for the program that installs a handler. */
    if (!c->program) {
        return;
    }
    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        struct type *t = it->symbol != NULL && it->kind != ITEM_TYPE
                             ? it->symbol->type
                             : NULL;
        if (it->kind != ITEM_CLASS || !it->is_abstract || t == NULL) {
            continue;
        }
        if (!filled_or_copied(c, t) && !named_by_refused_base(module, it)) {
            diagnostics_warn(c->diags, NAME_UNFILLED_ABSTRACT,
                             it->name_pos.line, it->name_pos.column,
                             "`%.*s` is abstract and no class fills it",
                             (int)it->name.length, it->name.text);
        }
    }
}

/* The checks of what crosses to C: every exported item, every
   `extern fn`, `provides`, `link framework` and `link linux`. */
static void check_boundary(struct checker *c)
{
    struct module *module = c->module;
    size_t i;

    for (i = 0; i < module->item_count; i++) {
        if (module->items[i]->symbol == NULL) {
            continue;
        }
        sema_check_generic_item(c, module->items[i]);
        if (module->items[i]->kind == ITEM_TYPE ||
            module->items[i]->type_param_count > 0) {
            continue;
        }
        if (module->items[i]->exported) {
            sema_check_export(c, module->items[i]);
        }
        if (module->items[i]->kind == ITEM_EXTERN_FN) {
            sema_check_extern_fn(c, module->items[i]);
        }
    }
    sema_check_provides(c, module);
    sema_check_link_names(c, module->frameworks, module->framework_count,
                          "framework", "framework");
    sema_check_link_names(c, module->linux_libraries,
                          module->linux_library_count, "linux", "library");
}

/* DESIGN: sema_check runs its passes in order. Each walks
   `module->items` once, and a later pass reads what an earlier one
   declared. */
bool sema_check(struct module *module, const struct sema_inputs *in,
                bool program)
{
    struct checker c;
    struct context top;
    struct context outside;
    size_t i;

    memset(&c, 0, sizeof c);
    c.types = in->types;
    c.arena = in->arena;
    c.diags = in->diags;
    c.module = module;
    module_spelled = module->spelled;
    c.module_name.text = in->module_name;
    c.module_name.length = strlen(in->module_name);
    c.package = in->package != NULL ? in->package : in->module_name;
    c.libraries = in->libraries;
    c.library_count = in->library_count;
    top = sema_declaration_context(&c, NULL);
    sema_enter(&c, &top, &outside);
    c.ok = true;
    c.program = program;
    sema_declare_root(&c);
    for (i = 0; i < module->import_count; i++) {
        sema_declare_import(&c, &module->imports[i]);
    }
    sema_declare_items(&c);
    for (i = 0; i < module->import_count; i++) {
        sema_declare_direct_names(&c, &module->imports[i]);
    }
    sema_declare_generics(&c);
    sema_resolve_generics(&c);
    sema_resolve_bases(&c);
    /* DESIGN: the value of an enum may name a constant of a class body,
       by its bare name inside the class and as `Table.ROWS` outside it.
       The symbols of those constants come first, so the value folds,
       by T204 and T205 of docs/audit/tests-findings.md. */
    sema_declare_body_constants(&c);
    sema_declare_enums_and_variants(&c);
    sema_declare_all_fields(&c);
    sema_check_guards(&c);
    sema_declare_function_types(&c);
    sema_declare_all_members(&c);
    sema_check_signatures(&c);
    sema_run_pending(&c);
    check_free_functions(&c);
    sema_check_classes(&c);
    check_member_functions(&c);
    report_program(&c);
    check_boundary(&c);
    sema_report_unfixed(&c);
    sema_report_leak_calls(&c);
    /* The defaults come before the copies, whose pass makes the copies
       their calls name and gives each class copy it makes its own. */
    if (c.ok) {
        for (i = 0; i < module->item_count; i++) {
            sema_class_defaults(&c, module->items[i]);
        }
    }
    if (c.ok && module->compile_copies) {
        sema_compile_copies(&c);
    }
    if (c.ok) {
        sema_check_lock_order(&c);
    }
    sema_leave(&c, &outside);
    /* The module scope never narrows, and its list is freed beside the
       entries all the same, as sema_leave_scope frees every other one. */
    free(c.module_scope.entries);
    free(c.module_scope.narrowed);
    ptr_map_free(&c.moves);
    return c.ok;
}

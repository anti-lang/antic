#include "sema.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpu.h"
#include "sema_checker.h"

/* DESIGN: one pass over the syntax tree per module, after every
   module-level name is declared, so an item can be used before its
   declaration. Each expression is checked with the type its context
   expects, which is how a literal gets its type. The checker writes the
   type into every expression and a symbol into every name. */

/* Helpers */

/* The bits of v read as a two's complement int64_t. C leaves the
   conversion of a uint64_t above INT64_MAX to the implementation, so a
   negative value is computed from its complement. */
int64_t sema_signed_bits(uint64_t v)
{
    return v <= INT64_MAX ? (int64_t)v : -(int64_t)~v - 1;
}

/* Format into out, which holds size bytes, size being 4 or more. A text
   too long for out ends in three periods where it was cut, so that a message
   never loses its end unmarked. */
static void vformat_to(char *out, size_t size, const char *format,
                       va_list args)
{
    int n = vsnprintf(out, size, format, args);

    if (n < 0) {
        out[0] = '\0';
    } else if ((size_t)n >= size) {
        memcpy(out + size - 4, "...", 4);
    }
}

void sema_format_to(char *out, size_t size, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    vformat_to(out, size, format, args);
    va_end(args);
}

void sema_error_at(struct checker *c, struct pos pos, const char *format,
                   ...)
{
    char message[160];
    va_list args;

    if (c->quiet > 0) {
        return;
    }
    va_start(args, format);
    vformat_to(message, sizeof message, format, args);
    va_end(args);
    diagnostics_add(c->diags, pos.line, pos.column, "%s", message);
    c->ok = false;
}

void sema_check_at(struct checker *c, enum diag_name name, struct pos pos,
                   const char *format, ...)
{
    char message[160];
    va_list args;

    if (c->quiet > 0) {
        return;
    }
    va_start(args, format);
    vformat_to(message, sizeof message, format, args);
    va_end(args);
    diagnostics_check(c->diags, name, pos.line, pos.column, "%s", message);
}

/* A type name for a message, kept in one of four rotating buffers so
   that one message can name up to four types. */
const char *sema_tn(const struct type *t)
{
    static char buffers[4][96];
    static size_t next;
    struct text text = {0};
    char *buffer = buffers[next++ % 4];

    type_name(&text, t);
    sema_format_to(buffer, sizeof buffers[0], "%s", text_cstr(&text));
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

bool sema_same_name(const struct name *a, const struct name *b)
{
    return a->length == b->length && memcmp(a->text, b->text, a->length) == 0;
}

bool sema_name_is(const struct name *a, const char *text)
{
    return a->length == strlen(text) && memcmp(a->text, text, a->length) == 0;
}

/* Scopes */

struct symbol *sema_scope_find_local(const struct scope *s,
                                     const struct name *name)
{
    size_t i;

    for (i = 0; i < s->count; i++) {
        if (sema_same_name(&s->entries[i].name, name)) {
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

    for (it = c->within; it != NULL; it = it->outer) {
        for (i = 0; i < it->nested_count; i++) {
            const struct item *inner = it->nested[i];
            if (inner->symbol != NULL &&
                sema_same_name(&inner->local_name, name)) {
                return inner->symbol;
            }
        }
    }
    return NULL;
}

/* An item of the module by name, the nested types of c->within first. */
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

    for (s = c->scope; s != NULL; s = s->parent) {
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

/* Whether lib imports module, directly or through its own imports. The
   depth limit ends the search when damaged libraries import each other. */
static bool depends_on(const struct checker *c, const struct interface *lib,
                       const struct name *module, size_t depth)
{
    size_t i;

    if (depth > c->library_count) {
        return false;
    }
    for (i = 0; i < lib->import_count; i++) {
        struct name imported;
        const struct interface *next;
        imported.text = lib->imports[i];
        imported.length = strlen(lib->imports[i]);
        if (sema_same_name(&imported, module)) {
            return true;
        }
        next = sema_find_library(c, &imported);
        if (next != NULL && depends_on(c, next, module, depth + 1)) {
            return true;
        }
    }
    return false;
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
        if (!sema_same_name(&lib->items[i]->name, name)) {
            continue;
        }
        if (lib->items[i]->internal && !package_shared(c, lib)) {
            return NULL;
        }
        return lib->items[i];
    }
    return NULL;
}

/* Declare a symbol in the current scope. A second name in the same scope
   is an error, a name that an outer scope holds is shadowed. */
struct symbol *sema_declare(struct checker *c, enum symbol_kind kind,
                            const struct name *name, struct pos pos,
                            const char *duplicate_message)
{
    struct scope *s = c->scope;
    struct symbol *sym;

    if (sema_scope_find_local(s, name) != NULL) {
        sema_error_at(c, pos, duplicate_message, (int)name->length, name->text);
        return NULL;
    }
    if (s->count == s->capacity) {
        size_t capacity = s->capacity == 0 ? 16 : s->capacity * 2;
        struct scope_entry *entries =
            capacity <= SIZE_MAX / sizeof *entries
                ? realloc(s->entries, capacity * sizeof *entries)
                : NULL;
        if (entries == NULL) {
            fputs("antic: out of memory\n", stderr);
            exit(70);
        }
        s->entries = entries;
        s->capacity = capacity;
    }
    sym = arena_alloc(c->arena, sizeof *sym);
    sym->kind = kind;
    sym->name = *name;
    sym->pos = pos;
    sym->frame = c->function;
    sym->depth = s->depth;
    s->entries[s->count].name = *name;
    s->entries[s->count].symbol = sym;
    s->count++;
    return sym;
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

    if (c->quiet > 0 || name->length == 0) {
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

void sema_enter_scope(struct checker *c, struct scope *s)
{
    memset(s, 0, sizeof *s);
    s->parent = c->scope;
    s->depth = c->scope != NULL ? c->scope->depth + 1 : 0;
    c->scope = s;
}

void sema_leave_scope(struct checker *c, struct scope *s)
{
    c->scope = s->parent;
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

    for (s = c->scope; s != NULL; s = s->parent) {
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
    struct scope *s = c->scope;
    size_t i;

    for (i = 0; i < s->narrowed_count; i++) {
        if (s->narrowed[i].symbol == sym) {
            s->narrowed[i].type = t;
            return;
        }
    }
    if (s->narrowed_count == s->narrowed_capacity) {
        size_t capacity =
            s->narrowed_capacity == 0 ? 4 : s->narrowed_capacity * 2;
        struct narrowing *grown =
            capacity <= SIZE_MAX / sizeof *grown
                ? realloc(s->narrowed, capacity * sizeof *grown)
                : NULL;
        if (grown == NULL) {
            fputs("antic: out of memory\n", stderr);
            exit(70);
        }
        s->narrowed = grown;
        s->narrowed_capacity = capacity;
    }
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

    for (s = c->scope; s != NULL; s = s->parent) {
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

/* A constant expression of type int with a value of at least 1, or a
   symbolic value. DESIGN: a length computed from size_of stays symbolic,
   and the back end checks that it is at least 1 on its target. Chapter 2
   allows it in struct fields and local variables. */
struct type *sema_array_of(struct checker *c, struct expr *e,
                           struct type *element)
{
    struct const_value v;

    if (sema_is_error(sema_check_expr(c, e, sema_builtin(c, TYPE_I64))) ||
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
        if (!c->target_sized) {
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

/* The width of a bitfield: a constant from 1 to the bits of its sized
   integer type. The field _ is the zero-width bitfield of C and has 0,
   as it does after an error. */
static uint8_t bitfield_width(struct checker *c, struct param *field,
                              struct type *t)
{
    struct const_value v;
    struct expr *e = field->bits;
    bool unit_break = field->name.length == 1 && field->name.text[0] == '_';

    if (sema_is_error(t)) {
        return 0;
    }
    if (unit_break && e == NULL) {
        sema_error_at(c, field->pos, "the field `_` is a zero-width bitfield, "
                      "written `_: T : 0`");
        return 0;
    }
    if (!type_is_integer(t) || type_is_target_sized(t)) {
        sema_error_at(c, field->type->pos,
                      "a bitfield has a sized integer type, "
                      "found `%s`", sema_tn(t));
        return 0;
    }
    if (!sema_require(c, e, sema_check_expr(c, e, sema_builtin(c, TYPE_I64)),
                      sema_builtin(c, TYPE_I64)) ||
        !sema_eval_const(c, e, &v)) {
        return 0;
    }
    if (unit_break && (v.kind == CONST_SYMBOLIC || v.as.integer != 0)) {
        sema_error_at(c, field->pos, "the field `_` is a zero-width bitfield, "
                      "written `_: T : 0`");
        return 0;
    }
    if (unit_break) {
        return 0;
    }
    if (v.kind == CONST_SYMBOLIC || sema_signed_bits(v.as.integer) < 1 ||
        v.as.integer > (uint64_t)type_bits(t)) {
        sema_error_at(c, e->pos, "a bitfield of `%s` has 1 to %d bits",
                      sema_tn(t),
                      type_bits(t));
        return 0;
    }
    return (uint8_t)v.as.integer;
}

/* DESIGN: the rules of a declaration of a simd struct. Every field has
   one type, a primitive type of one width on every target, and the
   count is a power of two. The size is a multiple of eight bytes. Above
   the vector cap of the level table the struct is an array and each
   operation a loop, which a warning names. The alignment follows from
   the size, so the declaration has no `align(N)`, and a lane is a whole
   value, so no field is a bitfield. */
static void check_simd_struct(struct checker *c, struct item *it)
{
    struct type *t = it->symbol->type;
    const struct type *lane;
    uint64_t bytes;
    size_t i;

    lane = t->fields[0].type;
    if (sema_is_error(lane)) {
        return;
    }
    if (type_lane_bytes(lane) == 0) {
        sema_error_at(c, t->fields[0].pos, "a lane of a `simd struct` is an "
                      "integer of a fixed width, a float, `bool` or `char`, "
                      "not `%s`", sema_tn(lane));
        return;
    }
    for (i = 0; i < t->field_count; i++) {
        if (t->fields[i].bits != 0 ||
            type_field_is_unit_break(&t->fields[i])) {
            sema_error_at(c, t->fields[i].pos,
                          "a lane of a `simd struct` is no "
                          "bitfield");
            return;
        }
        if (t->fields[i].type != lane && !sema_is_error(t->fields[i].type)) {
            sema_error_at(c, t->fields[i].pos, "every lane of `simd struct` "
                          "`%.*s` is `%s`, and `%.*s` is `%s`",
                          (int)it->name.length, it->name.text, sema_tn(lane),
                          (int)t->fields[i].name.length, t->fields[i].name.text,
                          sema_tn(t->fields[i].type));
            return;
        }
    }
    if ((t->field_count & (t->field_count - 1)) != 0) {
        sema_error_at(c, it->name_pos, "a `simd struct` has a power of two of "
                      "lanes, and `%.*s` has %zu", (int)it->name.length,
                      it->name.text, t->field_count);
        return;
    }
    if (it->align != NULL) {
        sema_error_at(c, it->align->pos, "a `simd struct` takes its alignment "
                      "from its size and has no `align(N)`");
        return;
    }
    bytes = type_simd_bytes(t);
    if (bytes % 8 != 0) {
        sema_error_at(c, it->name_pos,
                      "a `simd struct` is a multiple of 8 bytes, "
                      "and `%.*s` is %llu", (int)it->name.length, it->name.text,
                      (unsigned long long)bytes);
        return;
    }
    if (bytes > CPU_VECTOR_BYTE_CAP) {
        diagnostics_warn(c->diags, NAME_ABOVE_VECTOR_CAP, it->name_pos.line,
                         it->name_pos.column,
                         "`%.*s` is %llu bytes, above the vector cap of %d, "
                         "so it is an array and each operation on it a loop",
                         (int)it->name.length, it->name.text,
                         (unsigned long long)bytes, CPU_VECTOR_BYTE_CAP);
    }
}

/* The N of align(N): a constant power of two, or 0 after an error. */
static uint64_t alignment(struct checker *c, struct expr *e)
{
    struct const_value v;

    if (!sema_require(c, e, sema_check_expr(c, e, sema_builtin(c, TYPE_I64)),
                      sema_builtin(c, TYPE_I64)) ||
        !sema_eval_const(c, e, &v)) {
        return 0;
    }
    if (v.kind == CONST_SYMBOLIC) {
        sema_error_at(c, e->pos,
                      "an alignment is a constant, not a value computed "
                      "from `size_of`");
        return 0;
    }
    if (sema_signed_bits(v.as.integer) < 1 ||
        (v.as.integer & (v.as.integer - 1)) != 0) {
        sema_error_at(c, e->pos, "an alignment is a power of two");
        return 0;
    }
    return v.as.integer;
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
   no two threads reach one piece of memory through what it carries. */
struct type *sema_chan_element(struct checker *c, struct type_expr *t)
{
    struct type *element = sema_resolve_type(c, t);

    if (!sema_is_error(element) && !type_pointer_free(element)) {
        sema_error_at(c, t->pos,
                      "a channel carries values alone, and `%s` holds "
                      "a pointer", sema_tn(element));
        return sema_builtin(c, TYPE_ERROR);
    }
    return element;
}

static struct type *resolve_type_inner(struct checker *c, struct type_expr *t)
{
    struct type *element;
    struct symbol *sym;
    size_t i;

    switch (t->kind) {
    case TYPEX_BUILTIN:
        return builtin_of_token(c, t->builtin);
    case TYPEX_NAMED:
        /* `?` stands before a name for a match alone, since a match is
           the one value that is not a pointer and may be `none`. */
        if (t->nullable) {
            struct type_expr bare = *t;
            struct type *named;
            bare.nullable = false;
            named = sema_resolve_type(c, &bare);
            if (sema_is_error(named) || types_is_match(named)) {
                return types_with_none(c->types, named);
            }
            sema_error_at(c, t->pos, "`?` stands before `*T`, `fn(...)`, "
                          "`Match` or `ByteMatch`, found `%s`", sema_tn(named));
            return sema_builtin(c, TYPE_ERROR);
        }
        if (t->module.length > 0) {
            struct type *imported =
                sema_imported_struct(c, &t->module, &t->name, t->pos);
            if (t->arg_count > 0 && !sema_is_error(imported)) {
                sema_error_at(c, t->pos, "`%.*s.%.*s` is not generic",
                              (int)t->module.length, t->module.text,
                              (int)t->name.length, t->name.text);
                return sema_builtin(c, TYPE_ERROR);
            }
            return imported;
        }
        sym = sema_module_find(c, &t->name);
        /* DESIGN: `Object` is the root of every class chain, which the
           compiler declares. A program writes the name where the object
           model uses it, as in `equals(self, other: *Object)`, and a
           class of that name in the module wins over it. */
        if (sym == NULL && sema_name_is(&t->name, LANG_OBJECT)) {
            return types_object(c->types);
        }
        if (sym == NULL && sema_name_is(&t->name, LANG_FLAGS)) {
            return types_flags(c->types);
        }
        if (sym == NULL && sema_name_is(&t->name, LANG_MUTEX)) {
            return types_mutex(c->types);
        }
        if (sym == NULL && sema_name_is(&t->name, LANG_REGEX)) {
            return types_regex(c->types);
        }
        if (sym == NULL && sema_name_is(&t->name, LANG_BYTE_REGEX)) {
            return types_byte_regex(c->types);
        }
        if (sym == NULL && sema_name_is(&t->name, LANG_MATCH)) {
            return types_match_of(c->types, false);
        }
        if (sym == NULL && sema_name_is(&t->name, LANG_BYTE_MATCH)) {
            return types_match_of(c->types, true);
        }
        if (sym == NULL && sema_name_is(&t->name, LANG_FIELD_DESCRIPTOR)) {
            return types_field_descriptor(c->types);
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
        return sym->type;
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
    case TYPEX_FN: {
        struct type **params =
            types_alloc_array(c->arena, t->param_count + 1, sizeof *params);
        struct type *result = sema_builtin(c, TYPE_VOID);
        struct type *fn;
        for (i = 0; i < t->param_count; i++) {
            params[i] = sema_param_form(c, sema_resolve_type(c, t->params[i]),
                                        t->params[i]->keep,
                                        t->params[i]->concurrent,
                                        t->params[i]->owned,
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
        return t->nullable ? types_with_none(c->types, fn) : fn;
    }
    case TYPEX_TUPLE: {
        struct type **elements =
            types_alloc_array(c->arena, t->param_count, sizeof *elements);
        for (i = 0; i < t->param_count; i++) {
            elements[i] = sema_resolve_type(c, t->params[i]);
            if (sema_is_error(elements[i])) {
                return elements[i];
            }
        }
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
    if (c->plain_fns > 0 && concurrent) {
        sema_error_at(c, pos, "an `extern fn` takes plain C function "
                      "pointers, and `concurrent` marks a closure");
        return sema_builtin(c, TYPE_ERROR);
    }
    if (owned && c->plain_fns > 0) {
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
    return types_fn_form(c->types, t, !keep && c->plain_fns == 0, concurrent);
}

/* Resolve t and record the result in the node for later stages. */
struct type *sema_resolve_type(struct checker *c, struct type_expr *t)
{
    t->type = resolve_type_inner(c, t);
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

    if (sym == NULL && own && sema_same_name(&c->module_name, module)) {
        sym = sema_lookup(c, name);
    }
    return sym != NULL && sym->kind == SYMBOL_STRUCT && sym->type != NULL
               ? sym
               : NULL;
}

/* DESIGN: `may fail` gives a function the convention a program used to
   write by hand: `?*lang.Error` as the result and an out pointer for
   what it computes. The class is an ordinary imported one, so a module
   that writes the form imports `anti.lang` as it does for every error it
   names. */
struct type *sema_error_class(struct checker *c, struct pos pos)
{
    static const struct name module = {LANG_MODULE, sizeof LANG_MODULE - 1};
    static const struct name class_name = {LANG_ERROR, sizeof LANG_ERROR - 1};
    struct symbol *sym = sema_std_item(c, &module, &class_name, true);

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
    static const struct name module = {LANG_MODULE, sizeof LANG_MODULE - 1};
    static const struct name type_name = {
        LANG_SOURCE_LOCATION, sizeof LANG_SOURCE_LOCATION - 1};
    struct symbol *sym = sema_std_item(c, &module, &type_name, true);

    if (sym == NULL || sym->type->kind != TYPE_STRUCT) {
        sema_error_at(c, pos, "`here` gives an `" LANG_MODULE "."
                      LANG_SOURCE_LOCATION "`, so the module imports `"
                      LANG_MODULE "`");
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
static struct type *function_type(struct checker *c, struct item *it)
{
    const struct item *saved = c->signature;
    struct type *t;

    c->signature = it;
    t = function_type_of(c, it);
    c->signature = saved;
    return t;
}

static struct type *function_type_of(struct checker *c, struct item *it)
{
    size_t extra = it->has_self ? 1 : 0;
    size_t out = it->may_fail && it->result != NULL ? 1 : 0;
    struct type **params = arena_alloc(
        c->arena, (it->param_count + extra + out + 1) * sizeof *params);
    struct type *result = sema_builtin(c, TYPE_VOID);
    size_t i;

    if (it->has_self) {
        const struct item *owner = it->owner;
        if (owner == NULL || owner->symbol == NULL ||
            owner->symbol->type == NULL) {
            return sema_builtin(c, TYPE_ERROR);
        }
        params[0] = types_pointer(c->types, owner->symbol->type);
    }
    for (i = 0; i < it->param_count; i++) {
        if (it->kind == ITEM_EXTERN_FN) {
            c->plain_fns++;
        }
        params[i + extra] = sema_param_form(
            c, sema_resolve_type(c, it->params[i].type), it->params[i].keep,
            it->params[i].concurrent,
            it->params[i].owned && it->params[i].type->kind == TYPEX_FN,
            it->params[i].pos);
        if (it->kind == ITEM_EXTERN_FN) {
            c->plain_fns--;
        }
        if (sema_is_error(params[i + extra])) {
            return params[i + extra];
        }
        if (refuses_half_value(c, it->params[i].type->pos, params[i + extra],
                               "a parameter")) {
            return sema_builtin(c, TYPE_ERROR);
        }
    }
    if (it->result != NULL &&
        sema_is_error(result = sema_resolve_type(c, it->result))) {
        return result;
    }
    if (it->result != NULL &&
        refuses_half_value(c, it->result->pos, result, "a result")) {
        return sema_builtin(c, TYPE_ERROR);
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

/* Add the generated `get` to a singleton class. It takes no arguments
   and gives the one instance. */
static void declare_get(struct checker *c, struct item *it)
{
    static const char get_text[] = "get";
    struct type *t = it->symbol->type;
    struct item **members;
    struct item *m = arena_alloc(c->arena, sizeof *m);
    struct symbol *sym = arena_alloc(c->arena, sizeof *sym);
    struct text qualified = {0};
    struct name name;
    char *text;
    size_t i;

    name.text = get_text;
    name.length = sizeof get_text - 1;
    if (sema_find_member(t, &name) != NULL) {
        return;
    }
    /* The symbol is `T.get`, as for every function of a body, so the
       module defines `module.T.get` and another module calls that. */
    text_appendf(&qualified, "%.*s.%s", (int)it->name.length, it->name.text,
                 get_text);
    sym->name.length = qualified.length;
    text = arena_alloc(c->arena, qualified.length + 1);
    memcpy(text, qualified.data, qualified.length + 1);
    text_free(&qualified);
    m->kind = ITEM_FN;
    m->pub = true;
    m->vis = VIS_PUB;
    m->name = name;
    m->name_pos = it->name_pos;
    m->owner = it;
    m->symbol = sym;
    m->singleton_get = true;
    sym->kind = SYMBOL_FN;
    sym->name.text = text;
    sym->item = m;
    sym->type = types_fn(c->types, NULL, 0, types_pointer(c->types, t));
    members =
        types_alloc_array(c->arena, it->member_count + 1, sizeof *members);
    for (i = 0; i < it->member_count; i++) {
        members[i] = it->members[i];
    }
    members[it->member_count] = m;
    it->members = members;
    it->member_count++;
    t->members = members;
    t->member_count = it->member_count;
}

/* DESIGN: `Object.deserialize(input: str, from: *Allocator) -> *Object`
   is the static counterpart of `serialize`, with its body in the
   runtime. It is visible everywhere but is not `pub` to the tables, which
   hold functions with `self` alone. Every table therefore still starts
   with the seven functions of the root. It gives `none` when the text is
   not an object of a class of the program. The checker builds the root
   before any module and cannot name `anti.mem`, so the item holds `*Object`
   in the place of the allocator. deserialize_type gives a use the real
   type. */
static void declare_deserialize(struct checker *c, struct type *object)
{
    struct item **members =
        types_alloc_array(c->arena, object->member_count + 1,
                          sizeof *members);
    struct item *it = arena_alloc(c->arena, sizeof *it);
    struct symbol *sym = arena_alloc(c->arena, sizeof *sym);
    struct type **params = arena_alloc(c->arena, 2 * sizeof *params);

    params[0] = sema_builtin(c, TYPE_STR);
    params[1] = types_pointer(c->types, object);
    it->kind = ITEM_FN;
    it->vis = VIS_PUB;
    it->runtime = ROOT_DESERIALIZE;
    it->name.text = ROOT_DESERIALIZE;
    it->name.length = sizeof ROOT_DESERIALIZE - 1;
    it->symbol = sym;
    sym->kind = SYMBOL_FN;
    sym->name = it->name;
    sym->item = it;
    sym->type = types_fn(c->types, params, 2,
                         types_pointer(c->types, object));
    memcpy(members, object->members, object->member_count * sizeof *members);
    members[object->member_count] = it;
    object->members = members;
    object->member_count++;
}

/* The class `anti.lang.Error` when the compilation carries `anti.lang`,
   and NULL where it does not. The root's `failed` hook names it, and a
   program without that module cannot name it either. */
static struct type *lang_error_or_null(struct checker *c)
{
    static const struct name module = {LANG_MODULE, sizeof LANG_MODULE - 1};
    static const struct name class_name = {LANG_ERROR, sizeof LANG_ERROR - 1};
    struct symbol *sym = sema_std_item(c, &module, &class_name, false);

    if (sym == NULL || sym->type->kind != TYPE_CLASS) {
        return NULL;
    }
    return sym->type;
}

/* DESIGN: anti.lang.Object declares seven public functions whose bodies
   live in the runtime, and nine hooks with empty bodies after them. The
   checker builds one item per function, so `v.type_name()` resolves like
   any inherited call and lowering finds the runtime symbol behind it.
   The list is built once per session. */
/* The type of one parameter of a hook, after `self`. */
enum root_param { ROOT_P_OBJECT, ROOT_P_STR, ROOT_P_ERROR, ROOT_P_FIELD };

static void declare_root(struct checker *c)
{
    static const struct {
        const char *name;
        int params;             /* besides self */
        enum type_kind result;
        enum root_param kinds[2];
    } root[] = {
        {"type_name", 0, TYPE_STR, {ROOT_P_OBJECT, ROOT_P_OBJECT}},
        {ROOT_TO_TEXT, 0, TYPE_STR, {ROOT_P_OBJECT, ROOT_P_OBJECT}},
        {"equals", 1, TYPE_BOOL, {ROOT_P_OBJECT, ROOT_P_OBJECT}},
        {"hash", 0, TYPE_U64, {ROOT_P_OBJECT, ROOT_P_OBJECT}},
        {"serialize", 1, TYPE_VOID, {ROOT_P_OBJECT, ROOT_P_OBJECT}},
        {"destruct", 0, TYPE_VOID, {ROOT_P_OBJECT, ROOT_P_OBJECT}},
        {"copy", 1, TYPE_VOID, {ROOT_P_OBJECT, ROOT_P_OBJECT}},
        {ROOT_CREATED, 0, TYPE_VOID, {ROOT_P_OBJECT, ROOT_P_OBJECT}},
        {ROOT_DESTROYED, 0, TYPE_VOID, {ROOT_P_OBJECT, ROOT_P_OBJECT}},
        {ROOT_COPIED, 1, TYPE_VOID, {ROOT_P_OBJECT, ROOT_P_OBJECT}},
        {ROOT_DISPATCHED, 0, TYPE_VOID, {ROOT_P_OBJECT, ROOT_P_OBJECT}},
        {ROOT_JOINED, 0, TYPE_VOID, {ROOT_P_OBJECT, ROOT_P_OBJECT}},
        {ROOT_ENTER, 1, TYPE_VOID, {ROOT_P_STR, ROOT_P_OBJECT}},
        {ROOT_LEAVE, 1, TYPE_VOID, {ROOT_P_STR, ROOT_P_OBJECT}},
        {ROOT_FAILED, 2, TYPE_VOID, {ROOT_P_STR, ROOT_P_ERROR}},
        {ROOT_CHANGED, 1, TYPE_VOID, {ROOT_P_FIELD, ROOT_P_OBJECT}}
    };
    struct type *object = types_object(c->types);
    struct type *error = lang_error_or_null(c);
    struct item **members;
    size_t i;
    int k;

    if (object->member_count > 0) {
        return;
    }
    members = arena_alloc(c->arena, sizeof root / sizeof root[0] *
                                        sizeof *members);
    for (i = 0; i < sizeof root / sizeof root[0]; i++) {
        struct item *it = arena_alloc(c->arena, sizeof *it);
        struct symbol *sym = arena_alloc(c->arena, sizeof *sym);
        struct type **params = arena_alloc(c->arena, 3 * sizeof *params);
        params[0] = types_pointer(c->types, object);
        for (k = 0; k < 2; k++) {
            switch (root[i].kinds[k]) {
            case ROOT_P_STR:
                params[k + 1] = sema_builtin(c, TYPE_STR);
                break;
            /* Where the compilation carries no `anti.lang`, the error of
               `failed` is a plain object, which is what a module without
               that class can name. */
            case ROOT_P_ERROR:
                params[k + 1] = types_pointer(c->types,
                                              error != NULL ? error : object);
                break;
            case ROOT_P_FIELD:
                params[k + 1] =
                    types_pointer(c->types, types_field_descriptor(c->types));
                break;
            default:
                params[k + 1] = params[0];
                break;
            }
        }
        it->kind = ITEM_FN;
        it->pub = true;
        it->vis = VIS_PUB;
        it->has_self = true;
        it->runtime = root[i].name;
        it->name.text = root[i].name;
        it->name.length = strlen(root[i].name);
        it->symbol = sym;
        sym->kind = SYMBOL_FN;
        sym->name = it->name;
        sym->item = it;
        sym->type = types_fn(c->types, params, (size_t)root[i].params + 1,
                             sema_builtin(c, root[i].result));
        members[i] = it;
    }
    object->members = members;
    object->member_count = sizeof root / sizeof root[0];
    declare_deserialize(c, object);
}

/* DESIGN: the tag of a variant is an enum named `T.tag` over the
   smallest unsigned integer that holds the number of its cases. Its
   values number the cases from 0 in the order of the declaration. Each
   case with fields is a struct named `T.Case` with C layout, which the
   union of the variant holds. No program names either, and a message
   does. packed applies to the structs of the cases as it does to the
   variant, as `#pragma pack` in C covers the definitions inside. */
static void declare_cases(struct checker *c, struct item *it)
{
    static const struct name tag_word = {VARIANT_TAG, sizeof VARIANT_TAG - 1};
    struct type *v = it->symbol->type;
    size_t count = it->case_count;
    struct struct_field *values =
        types_alloc_array(c->arena, count + 1, sizeof *values);
    struct type **payloads =
        types_alloc_array(c->arena, count + 1, sizeof *payloads);
    struct type *tag;
    size_t i;
    size_t j;
    size_t k;

    if (count == 0) {
        sema_error_at(c, it->name_pos, "variant `%.*s` has no case",
                      (int)it->name.length, it->name.text);
    }
    tag = types_enum(c->types, c->module_name,
                     sema_dotted(c, &it->name, &tag_word),
                     sema_builtin(c, count <= UINT8_MAX    ? TYPE_U8
                                     : count <= UINT16_MAX ? TYPE_U16
                                                           : TYPE_U32));
    for (i = 0; i < count; i++) {
        const struct variant_case *one = &it->cases[i];
        values[i].name = one->name;
        values[i].pos = one->pos;
        values[i].doc = one->doc;
        values[i].type = tag;
        values[i].number = i;
        for (k = 0; k < i; k++) {
            if (sema_same_name(&values[k].name, &one->name)) {
                sema_error_at(c, one->pos, "variant `%.*s` has two cases named "
                              "`%.*s`", (int)it->name.length, it->name.text,
                              (int)one->name.length, one->name.text);
                break;
            }
        }
    }
    types_set_fields(c->types, tag, values, count);
    v->packed = it->packed;
    if (it->align != NULL) {
        v->align = alignment(c, it->align);
    }
    for (i = 0; i < count; i++) {
        const struct variant_case *one = &it->cases[i];
        struct struct_field *fields;
        payloads[i] = NULL;
        if (one->field_count == 0) {
            continue;
        }
        fields = types_alloc_array(c->arena, one->field_count,
                                   sizeof *fields);
        for (j = 0; j < one->field_count; j++) {
            fields[j].name = one->fields[j].name;
            fields[j].pos = one->fields[j].pos;
            fields[j].doc = one->fields[j].doc;
            fields[j].vis = VIS_PUB;
            c->target_sized = true;
            fields[j].type = sema_resolve_type(c, one->fields[j].type);
            c->target_sized = false;
            for (k = 0; k < j; k++) {
                if (sema_same_name(&fields[k].name, &fields[j].name)) {
                    sema_error_at(c, fields[j].pos,
                                  "case `%.*s` of `%.*s` has two "
                                  "fields named `%.*s`", (int)one->name.length,
                                  one->name.text, (int)it->name.length,
                                  it->name.text, (int)fields[j].name.length,
                                  fields[j].name.text);
                    break;
                }
            }
        }
        payloads[i] = types_struct(c->types, c->module_name,
                                   sema_dotted(c, &it->name, &one->name));
        payloads[i]->packed = it->packed;
        types_set_fields(c->types, payloads[i], fields, one->field_count);
    }
    types_set_cases(c->types, v, tag, payloads, count);
    sema_generic_ready(c, v);
}

/* DESIGN: the default of a parameter is a constant expression, which the
   checker evaluates once, or `here`, which each call fills with its own
   position. The parameters with a default end the list, because a call
   gives the others in order and leaves out only the last ones. An
   `extern fn` declares what C declares, and C has no default values. */
static void check_defaults(struct checker *c, struct item *it)
{
    const struct type *fn = it->symbol != NULL ? it->symbol->type : NULL;
    size_t extra = it->has_self ? 1 : 0;
    size_t count = it->param_count + extra;
    struct param_default *list = NULL;
    const struct param *first = NULL;
    size_t i;

    if (fn == NULL || sema_is_error(fn) || fn->kind != TYPE_FN ||
        fn->param_count < count) {
        return;
    }
    for (i = 0; i < it->param_count; i++) {
        struct param *p = &it->params[i];
        struct type *t = fn->params[i + extra];
        struct const_value *v;
        if (p->value == NULL) {
            if (first != NULL) {
                sema_error_at(c, p->pos, "`%.*s` has no default and follows "
                              "`%.*s`, which has one", (int)p->name.length,
                              p->name.text, (int)first->name.length,
                              first->name.text);
            }
            continue;
        }
        if (first == NULL) {
            first = p;
        }
        if (it->kind == ITEM_EXTERN_FN) {
            sema_error_at(c, p->value->pos, "an `extern fn` declares what C "
                          "declares, and C has no default values");
            continue;
        }
        if (!sema_require(c, p->value, sema_check_expr(c, p->value, t), t)) {
            continue;
        }
        if (list == NULL) {
            list = types_alloc_array(c->arena, count, sizeof *list);
        }
        if (p->value->kind == EXPR_HERE) {
            list[i + extra].here = true;
            continue;
        }
        v = arena_alloc(c->arena, sizeof *v);
        if (sema_eval_const(c, p->value, v)) {
            list[i + extra].value = v;
        }
    }
    it->symbol->defaults = list;
    it->symbol->default_count = list != NULL ? count : 0;
}

/* DESIGN: `own` on a parameter says the function takes over the object,
   as `own` on a field says the object frees the memory. The rule of the
   field holds: a pointer or a slice. The error a handler binds moves
   into such a parameter, and nothing else changes at a call. */
static void check_owned(struct checker *c, struct item *it)
{
    const struct type *fn = it->symbol != NULL ? it->symbol->type : NULL;
    size_t extra = it->has_self ? 1 : 0;
    size_t count = it->param_count + extra;
    bool *list = NULL;
    size_t i;

    if (fn == NULL || sema_is_error(fn) || fn->kind != TYPE_FN ||
        fn->param_count < count) {
        return;
    }
    for (i = 0; i < it->param_count; i++) {
        const struct param *p = &it->params[i];
        const struct type *t = fn->params[i + extra];
        if (!p->owned) {
            continue;
        }
        if (sema_is_error(t)) {
            continue;
        }
        if (t->kind != TYPE_POINTER && t->kind != TYPE_SLICE &&
            !(t->kind == TYPE_FN && t->owned)) {
            sema_error_at(c, p->pos,
                          "`own` needs a pointer or a slice, and `%.*s` "
                          "has type `%s`", (int)p->name.length, p->name.text,
                          sema_tn(t));
            continue;
        }
        if (list == NULL) {
            list = types_alloc_array(c->arena, count, sizeof *list);
        }
        list[i + extra] = true;
    }
    it->symbol->owned = list;
    it->symbol->owned_count = list != NULL ? count : 0;
}

static enum symbol_kind item_symbol_kind(enum item_kind kind)
{
    switch (kind) {
    case ITEM_FN: return SYMBOL_FN;
    case ITEM_EXTERN_FN: return SYMBOL_EXTERN_FN;
    case ITEM_STRUCT:
    case ITEM_UNION:
    case ITEM_ENUM:
    case ITEM_CLASS:
    case ITEM_VARIANT:
    case ITEM_TYPE: return SYMBOL_STRUCT;
    case ITEM_CONSTRAINT: return SYMBOL_CONSTRAINT;
    default: return SYMBOL_CONST;
    }
}

/* An import declares the last segment of the module path, or its alias,
   at module level. */
static void declare_import(struct checker *c, const struct import *imp)
{
    const struct name *module = &imp->module;
    const struct interface *lib;
    struct symbol *sym;
    struct name local = *module;
    size_t i;

    for (i = 0; i < module->length; i++) {
        if (module->text[i] >= 'A' && module->text[i] <= 'Z') {
            sema_error_at(c, imp->module_pos, "the module path `%.*s` is not "
                          "lowercase", (int)module->length, module->text);
            return;
        }
        if (module->text[i] == '.') {
            local.text = module->text + i + 1;
            local.length = module->length - i - 1;
        }
    }
    if (sema_same_name(module, &c->module_name)) {
        sema_error_at(c, imp->module_pos, "`%.*s` cannot import itself",
                      (int)module->length, module->text);
        return;
    }
    lib = sema_find_library(c, module);
    if (lib == NULL) {
        sema_error_at(c, imp->module_pos, "cannot find module `%.*s`",
                      (int)module->length, module->text);
        return;
    }
    if (depends_on(c, lib, &c->module_name, 0)) {
        sema_error_at(c, imp->module_pos,
                      "`%.*s` depends on `%.*s`, so the import "
                      "forms a cycle", (int)module->length, module->text,
                      (int)c->module_name.length, c->module_name.text);
        return;
    }
    sym = sema_declare(c, SYMBOL_MODULE,
                       imp->alias.length > 0 ? &imp->alias : &local,
                       imp->module_pos, "`%.*s` is already declared");
    if (sym != NULL) {
        sym->home = lib;
    }
}

/* The class or interface that the qualifier q of a `concrete fn` of t
   names, or NULL. q may name t or a class of its chain. It may also name
   an interface that a class of the chain implements, or a class of the
   chain of that interface. */
static const struct type *qualified_table(const struct type *t,
                                          const struct name *q)
{
    const struct type *up;
    size_t k;

    for (up = t; up != NULL; up = sema_inherited(up)) {
        if (sema_same_name(q, &up->name)) {
            return up;
        }
        for (k = 0; k < up->field_count; k++) {
            const struct type *iface;
            if (up->fields[k].form != FIELD_IMPL) {
                continue;
            }
            for (iface = up->fields[k].type; iface != NULL;
                 iface = sema_inherited(iface)) {
                if (sema_same_name(q, &iface->name)) {
                    return iface;
                }
            }
        }
    }
    return NULL;
}

/* The function name that the primary table of the chain of t holds, or
   NULL. owner receives the class that declares it. */
static const struct item *chain_entry(const struct type *t,
                                      const struct name *name,
                                      const struct type **owner)
{
    const struct item *found = types_primary_member(t, name);
    size_t i;

    for (; found != NULL && t != NULL; t = sema_inherited(t)) {
        for (i = 0; i < t->member_count; i++) {
            if (t->members[i] == found) {
                *owner = t;
                return found;
            }
        }
    }
    return NULL;
}

/* Whether the level t itself fills the table of iface with a body
   qualified by a class of the chain of iface. That body wins in the
   table over an unqualified one of its level, as lowering fills it. A
   level above counts for nothing, because the nearest body wins. */
static bool qualified_body(const struct type *t, const struct type *iface,
                           const struct name *name)
{
    const struct type *chain;
    size_t i;

    for (i = 0; i < t->member_count; i++) {
        const struct item *m = t->members[i];
        if (m->kind != ITEM_FN || m->qualifier.length == 0 ||
            !sema_same_name(&m->name, name)) {
            continue;
        }
        for (chain = iface; chain != NULL; chain = sema_inherited(chain)) {
            if (sema_same_name(&m->qualifier, &chain->name)) {
                return true;
            }
        }
    }
    return false;
}

/* The parameters a signature writes: those of its type without `self`
   and without the out pointer of a `may fail` function. */
static size_t written_params(const struct type *fn, bool has_self)
{
    size_t count = fn->param_count - (has_self ? 1 : 0);
    return fn->may_fail && fn->has_out ? count - 1 : count;
}

/* The result a signature writes, or NULL when it writes none. */
static const struct type *written_result(const struct type *fn)
{
    if (fn->may_fail) {
        return fn->has_out ? fn->params[fn->param_count - 1]->element : NULL;
    }
    return fn->result->kind == TYPE_VOID ? NULL : fn->result;
}

/* A result for a message: the type in backquotes, or `nothing`. */
static void returned_text(char *out, size_t size, const struct type *t)
{
    if (t == NULL) {
        sema_format_to(out, size, "nothing");
    } else {
        sema_format_to(out, size, "`%s`", sema_tn(t));
    }
}

static bool owns_param(const struct symbol *sym, size_t index)
{
    return sym->owned != NULL && index < sym->owned_count && sym->owned[index];
}

/* DESIGN: the root declares `serialize(self, out: *text.Builder)`. The
   checker builds the root before any module and cannot name anti.text.
   The parameter therefore has the type `*Object`, which takes a builder
   as it takes any class. A replacement writes the parameter the
   specification gives, a `*text.Builder`. */
static bool root_builder(const struct item *entry)
{
    return entry->runtime != NULL && sema_name_is(&entry->name, "serialize");
}

static bool builder_pointer(const struct type *t)
{
    return t->kind == TYPE_POINTER && !t->nullable &&
           t->element->kind == TYPE_CLASS &&
           sema_name_is(&t->element->module, "anti.text") &&
           sema_name_is(&t->element->name, "Builder");
}

/* DESIGN: a `concrete fn` fills an entry with the signature of the
   function that declared the entry, exactly: `self`, the type and the
   `own` of each parameter, the result and `may fail`. A call through the
   table passes what that signature says, so a body that took anything
   else would read arguments that are not there. The message names the
   first difference in the order of the text. */
static bool same_signature(struct checker *c, const struct item *m,
                           const struct item *entry, const struct type *owner)
{
    const struct type *mine = m->symbol != NULL ? m->symbol->type : NULL;
    const struct type *theirs =
        entry->symbol != NULL ? entry->symbol->type : NULL;
    size_t extra = m->has_self ? 1 : 0;
    size_t count;
    size_t their_count;
    const struct type *result;
    const struct type *their_result;
    char fn[160];
    char at[160];
    size_t i;

    if (mine == NULL || theirs == NULL || mine->kind != TYPE_FN ||
        theirs->kind != TYPE_FN) {
        return true;
    }
    /* A function of a copy of a generic takes the arguments of the copy
       in place of the parameters. */
    theirs = sema_member_type(c, (struct type *)theirs, owner);
    sema_format_to(fn, sizeof fn, "concrete fn %.*s%s%.*s",
                   (int)m->qualifier.length, m->qualifier.text,
                   m->qualifier.length > 0 ? "::" : "", (int)m->name.length,
                   m->name.text);
    sema_format_to(at, sizeof at, "%s.%.*s", sema_tn(owner),
                   (int)entry->name.length,
                   entry->name.text);
    if (m->has_self != entry->has_self) {
        sema_error_at(c, m->name_pos, "`%s` %s `self`, and `%s` %s", fn,
                      m->has_self ? "takes" : "does not take", at,
                      m->has_self ? "does not" : "does");
        return false;
    }
    count = written_params(mine, m->has_self);
    their_count = written_params(theirs, entry->has_self);
    for (i = 0; i < count && i < their_count && i < m->param_count; i++) {
        const struct param *p = &m->params[i];
        const struct type *got = mine->params[i + extra];
        const struct type *want = theirs->params[i + extra];
        bool owned = owns_param(m->symbol, i + extra);
        bool builder = root_builder(entry);
        if (owned != owns_param(entry->symbol, i + extra)) {
            sema_error_at(c, p->pos,
                          owned ? "`%.*s` of `%s` is `own`, and `%s` "
                                  "does not take it as `own`"
                                : "`%.*s` of `%s` is not `own`, and "
                                  "`%s` takes it as `own`",
                          (int)p->name.length, p->name.text, fn, at);
            return false;
        }
        if (builder ? !builder_pointer(got) : got != want) {
            sema_error_at(c, p->type->pos,
                          "`%.*s` of `%s` has type `%s`, and `%s` "
                          "takes `%s`", (int)p->name.length, p->name.text, fn,
                          sema_tn(got), at,
                          builder ? "*Builder" : sema_tn(want));
            return false;
        }
    }
    if (count != their_count) {
        char takes[48];
        if (count == 0) {
            sema_format_to(takes, sizeof takes, "no parameter");
        } else {
            sema_format_to(takes, sizeof takes, "%zu parameter%s", count,
                           count == 1 ? "" : "s");
        }
        sema_error_at(c, m->name_pos, "`%s` takes %s%s, and `%s` takes %zu", fn,
                      takes, m->has_self ? " besides `self`" : "", at,
                      their_count);
        return false;
    }
    result = written_result(mine);
    their_result = written_result(theirs);
    if (result != their_result) {
        char given[100];
        char wanted[100];
        returned_text(given, sizeof given, result);
        returned_text(wanted, sizeof wanted, their_result);
        sema_error_at(c, m->result != NULL ? m->result->pos : m->name_pos,
                      "`%s` returns %s, and `%s` returns %s", fn, given, at,
                      wanted);
        return false;
    }
    if (mine->may_fail != theirs->may_fail) {
        sema_error_at(c, m->may_fail ? m->may_fail_pos : m->name_pos,
                      mine->may_fail ? "`%s` may fail, and `%s` cannot"
                                     : "`%s` cannot fail, and `%s` may fail",
                      fn, at);
        return false;
    }
    return true;
}

/* Whether two bodies of t with two qualifiers fill one table. Both may
   name a class of the base chain. Both may name a class of the chain of
   one interface that the chain of t implements. */
static bool one_table(const struct type *t, const struct item *a,
                      const struct item *b)
{
    enum body_table a_table = types_body_table(t, a);
    const struct type *up;
    const struct type *chain;
    size_t k;

    if (a_table != types_body_table(t, b) || a_table == BODY_PLAIN) {
        return false;
    }
    if (a_table == BODY_BASE) {
        return true;
    }
    for (up = t; up != NULL; up = sema_inherited(up)) {
        for (k = 0; k < up->field_count; k++) {
            bool has_a = false;
            bool has_b = false;
            if (up->fields[k].form != FIELD_IMPL) {
                continue;
            }
            for (chain = up->fields[k].type; chain != NULL;
                 chain = sema_inherited(chain)) {
                has_a = has_a || sema_same_name(&a->qualifier, &chain->name);
                has_b = has_b || sema_same_name(&b->qualifier, &chain->name);
            }
            if (has_a && has_b) {
                return true;
            }
        }
    }
    return false;
}

/* The entries a `concrete fn` m of t fills, each compared with m. A
   qualified body fills the table its qualifier names. An unqualified one
   fills the entry of the base chain, unless a body qualified by a base
   holds it. It also fills the entry of every interface of the chain that
   no qualified body fills. One that fills none of them is refused. */
static void check_replacement(struct checker *c, const struct item *it,
                              const struct type *t, const struct item *m)
{
    const struct type *owner = NULL;
    const struct item *entry;
    const struct type *up;
    bool filled = false;
    size_t k;

    if (m->qualifier.length > 0 && !sema_same_name(&m->qualifier, &t->name)) {
        const struct type *table = qualified_table(t, &m->qualifier);
        if (table == NULL) {
            return;
        }
        entry = chain_entry(table, &m->name, &owner);
        if (entry == NULL) {
            sema_error_at(c, m->name_pos, "`concrete fn %.*s::%.*s` of `%.*s` "
                          "fills no abstract function",
                          (int)m->qualifier.length,
                          m->qualifier.text, (int)m->name.length, m->name.text,
                          (int)it->name.length, it->name.text);
            return;
        }
        same_signature(c, m, entry, owner);
        return;
    }
    entry = types_holds_entry(t, m)
                ? chain_entry(sema_inherited(t), &m->name, &owner)
                : NULL;
    if (entry != NULL && !same_signature(c, m, entry, owner)) {
        return;
    }
    filled = entry != NULL;
    for (up = t; up != NULL; up = sema_inherited(up)) {
        for (k = 0; k < up->field_count; k++) {
            const struct type *iface = up->fields[k].type;
            if (up->fields[k].form != FIELD_IMPL ||
                qualified_body(t, iface, &m->name)) {
                continue;
            }
            entry = chain_entry(iface, &m->name, &owner);
            if (entry != NULL && !same_signature(c, m, entry, owner)) {
                return;
            }
            filled = filled || entry != NULL;
        }
    }
    if (!filled) {
        sema_error_at(c, m->name_pos, "`concrete fn %.*s` of `%.*s` fills no "
                      "abstract function", (int)m->name.length, m->name.text,
                      (int)it->name.length, it->name.text);
    }
}

/* DESIGN: two functions of one name in one body are one name twice,
   unless their qualifiers differ. Each qualified body fills a table of
   its own. The class's own name means no qualifier. Two qualifiers that
   reach one table are refused later, when the chain is known. */
static bool same_qualifier(const struct item *it, const struct item *a,
                           const struct item *b)
{
    bool a_plain = a->qualifier.length == 0 ||
                   sema_same_name(&a->qualifier, &it->name);
    bool b_plain = b->qualifier.length == 0 ||
                   sema_same_name(&b->qualifier, &it->name);

    if (a->kind != ITEM_FN || b->kind != ITEM_FN || (a_plain && b_plain)) {
        return true;
    }
    return !a_plain && !b_plain && sema_same_name(&a->qualifier, &b->qualifier);
}

/* A framework is named once, and its name is one name of Apple's SDK:
   no path and no empty text, since it becomes -framework <name>. A
   library of `link linux` follows the same rules, since it becomes
   -l<name>. */
static void check_link_names(struct checker *c, const struct link_name *names,
                             size_t count, const char *kind, const char *what)
{
    size_t i;
    size_t j;

    for (i = 0; i < count; i++) {
        const struct link_name *f = &names[i];
        if (f->name.length == 0) {
            sema_error_at(c, f->pos, "`link %s` names no %s", kind, what);
        } else if (memchr(f->name.text, '/', f->name.length) ||
            memchr(f->name.text, '\\', f->name.length) ||
            memchr(f->name.text, ' ', f->name.length)) {
            sema_error_at(c, f->pos, "`link %s` names a %s of %s, and `%.*s` "
                          "is none", kind, what,
                          strcmp(kind, "linux") == 0 ? "the glibc sysroot"
                                                     : "Apple's SDK",
                          (int)f->name.length, f->name.text);
        }
        for (j = 0; j < i; j++) {
            if (names[j].name.length == f->name.length &&
                memcmp(names[j].name.text, f->name.text,
                       f->name.length) == 0) {
                sema_error_at(c, f->pos, "the %s `%.*s` is linked twice",
                              what, (int)f->name.length, f->name.text);
            }
        }
    }
}

/* DESIGN: `provides Interface as Class;` says what a library offers.
   The interface is an abstract class of the module being checked or of
   one it imports. The class is a complete class of the module that
   inherits the interface or implements it. A host reaches it through
   the interface alone, so one line per interface is all it may ask
   for. */
static void check_provides(struct checker *c, struct module *module)
{
    size_t i;
    size_t j;

    for (i = 0; i < module->provides_count; i++) {
        struct provides *pr = &module->provides[i];
        const struct type *iface =
            sema_interface_named(c, &pr->qualifier, &pr->interface,
                                 pr->interface_pos);
        const struct symbol *sym =
            sema_scope_find_local(&c->module_scope, &pr->class_name);
        const struct item *it = sym != NULL ? sym->item : NULL;

        for (j = 0; j < i; j++) {
            if (module->provides[j].type != NULL &&
                module->provides[j].type == iface) {
                sema_error_at(c, pr->interface_pos,
                              "`%s` is provided twice",
                              sema_tn(iface));
            }
        }
        if (iface == NULL) {
            continue;
        }
        if (iface->kind != TYPE_CLASS || !iface->has_abstract) {
            sema_error_at(c, pr->interface_pos, "`%s` is not abstract, and a "
                          "`provides` line names an interface",
                          sema_tn(iface));
            continue;
        }
        if (it == NULL || it->kind != ITEM_CLASS || sym->type == NULL) {
            sema_error_at(c, pr->class_pos,
                          "the module declares no class `%.*s`",
                          (int)pr->class_name.length, pr->class_name.text);
            continue;
        }
        if (it->is_abstract || sym->type->has_abstract) {
            sema_error_at(c, pr->class_pos,
                          "`%.*s` is abstract, and a `provides` "
                          "line names a complete class",
                          (int)pr->class_name.length, pr->class_name.text);
            continue;
        }
        /* A singleton has one instance, which its `get` makes. A
           library that offered one would hand the host a second. */
        if (it->is_singleton) {
            sema_error_at(c, pr->class_pos, "`%.*s` is a singleton, and a "
                          "`provides` line names a class the host builds",
                          (int)pr->class_name.length, pr->class_name.text);
            continue;
        }
        if (!sema_fills(sym->type, iface)) {
            sema_error_at(c, pr->class_pos, "`%.*s` neither inherits `%s` nor "
                          "implements it", (int)pr->class_name.length,
                          pr->class_name.text, sema_tn(iface));
            continue;
        }
        pr->type = iface;
        pr->class_type = sym->type;
    }
}

/* The passes of sema_check, in the order it runs them. Each walks
   `module->items` once, and a later pass reads what an earlier one
   declared. */

/* Declare every item first, so each can be used before its
   declaration. */
static void declare_items(struct checker *c)
{
    const struct module *module = c->module;
    size_t i;

    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        it->symbol = sema_declare(c, item_symbol_kind(it->kind), &it->name,
                                  it->name_pos, "`%.*s` is already declared");
        if (it->symbol == NULL) {
            continue;
        }
        it->symbol->item = it;
        it->symbol->variadic = it->variadic;
        it->symbol->worker = it->worker;
        it->symbol->may_fail = it->may_fail;
        it->symbol->exported = it->exported;
        it->symbol->doc = it->doc;
        if (it->kind == ITEM_STRUCT || it->kind == ITEM_UNION) {
            it->symbol->type = types_struct(c->types, c->module_name, it->name);
            it->symbol->type->is_union = it->kind == ITEM_UNION;
            it->symbol->type->simd = it->simd;
            sema_safety_declare(it);
        } else if (it->kind == ITEM_CLASS) {
            it->symbol->type = types_struct(c->types, c->module_name, it->name);
            it->symbol->type->kind = TYPE_CLASS;
            it->symbol->type->has_abstract = it->is_abstract;
            it->symbol->type->traced = it->trace;
            it->symbol->type->is_final = it->is_final;
            sema_safety_declare(it);
            /* DESIGN: `compatible` names the floor of a plugin's
               version, which only an abstract class has a table for. */
            if (it->compatible.length > 0 && !it->is_abstract) {
                sema_error_at(c, it->compatible_pos,
                              "`compatible` names the versions a plugin may "
                              "carry, and belongs to an abstract class");
            }
            it->symbol->type->compatible = it->compatible;
        } else if (it->kind == ITEM_VARIANT) {
            it->symbol->type = types_struct(c->types, c->module_name, it->name);
            it->symbol->type->kind = TYPE_VARIANT;
        } else if (it->kind == ITEM_ENUM) {
            /* DESIGN: the underlying type of an enum is c_int unless the
               declaration names one, as an unfixed C enum is an int. */
            struct type *base = it->base != NULL
                                    ? sema_resolve_type(c, it->base)
                                    : types_builtin(c->types, TYPE_I32);
            it->symbol->type =
                types_enum(c->types, c->module_name, it->name, base);
        }
        if (it->symbol->type != NULL) {
            it->symbol->type->members = it->members;
            it->symbol->type->member_count = it->member_count;
        }
    }
}

/* DESIGN: a class names its base in its header. The base is nested
   whole at offset 0, so the checker resolves it before the fields,
   which put the base at index 0. A base that is not a class, or that
   is `final`, is refused. A class without `inherits` takes the root
   `anti.lang.Object`, which the compiler declares. */
static void resolve_base(struct checker *c, struct item *it)
{
    struct symbol *base;
    struct type *base_type;

    if (it->base_name.length == 0) {
        it->symbol->type->base = types_object(c->types);
        return;
    }
    /* A qualified base is a public class of an imported module. */
    if (it->base_module.length > 0) {
        base_type = sema_imported_struct(c, &it->base_module, &it->base_name,
                                         it->base_pos);
        if (sema_is_error(base_type)) {
            return;
        }
    } else {
        base = sema_module_find(c, &it->base_name);
        base_type = base != NULL && base->kind == SYMBOL_STRUCT
                        ? base->type
                        : NULL;
        if (base != NULL && base->item != NULL &&
            base->item->kind == ITEM_TYPE) {
            base_type = sema_alias_type(c, base);
        }
    }
    /* A generic base is named with its arguments,
       `inherits Iterable<T>`. */
    if (base_type != NULL && !sema_is_error(base_type) &&
        (base_type->type_param_count > 0 || it->base_arg_count > 0)) {
        base_type = sema_copy_of(c, base_type, it->base_args,
                                 it->base_arg_count, it->base_pos);
        if (sema_is_error(base_type)) {
            return;
        }
    }
    if (base_type == NULL || base_type->kind != TYPE_CLASS) {
        if (it->base_module.length > 0) {
            sema_error_at(c, it->base_pos, "`%.*s.%.*s` is not a class",
                          (int)it->base_module.length, it->base_module.text,
                          (int)it->base_name.length, it->base_name.text);
        } else {
            sema_error_at(c, it->base_pos, "`%.*s` is not a class",
                          (int)it->base_name.length, it->base_name.text);
        }
        return;
    }
    if (base_type->is_final) {
        sema_error_at(c, it->base_pos,
                      "`%.*s` cannot inherit `final` class `%.*s`",
                      (int)it->name.length, it->name.text,
                      (int)it->base_name.length, it->base_name.text);
        return;
    }
    /* The bases of the module are set in the order of the items.
       A chain therefore ends at a base not yet set. A cycle is found
       at the class that would close it, which keeps the root, so
       every walk up a chain ends. */
    if (sema_descends_from(base_type, it->symbol->type)) {
        sema_error_at(c, it->base_pos, "class `%.*s` inherits itself",
                      (int)it->name.length, it->name.text);
        it->symbol->type->base = types_object(c->types);
        return;
    }
    it->symbol->type->base = base_type;
    sema_safety_base(c, it, base_type);
}

static void resolve_bases(struct checker *c)
{
    const struct module *module = c->module;
    size_t i;

    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        if (it->kind == ITEM_CLASS && it->symbol != NULL) {
            c->within = it;
            resolve_base(c, it);
            c->within = NULL;
        }
    }
}

/* DESIGN: the values of an enum live in its fields, each with the
   enum as its type. A value without `=` follows the one before it,
   starting at 0, as C numbers an enumerator. */
static void declare_enum_values(struct checker *c, struct item *it)
{
    struct struct_field *values;
    size_t j;

    values = types_alloc_array(c->arena, it->param_count + 1, sizeof *values);
    for (j = 0; j < it->param_count; j++) {
        size_t k;
        memset(&values[j], 0, sizeof values[j]);
        values[j].name = it->params[j].name;
        values[j].pos = it->params[j].pos;
        values[j].doc = it->params[j].doc;
        values[j].type = it->symbol->type;
        values[j].value = it->params[j].value;
        /* DESIGN: a value without `=` follows the one before it and
           the first is 0, as C numbers an enumerator. */
        values[j].number = j == 0 ? 0 : values[j - 1].number + 1;
        if (it->params[j].value != NULL) {
            struct const_value v;
            struct type *base = it->symbol->type->base;
            if (sema_require(c, it->params[j].value,
                             sema_check_expr(c, it->params[j].value, base),
                             base) &&
                sema_eval_const(c, it->params[j].value, &v) &&
                v.kind == CONST_INT) {
                values[j].number = v.as.integer;
            }
        }
        for (k = 0; k < j; k++) {
            if (sema_same_name(&values[k].name, &values[j].name)) {
                sema_error_at(c, values[j].pos, "enum `%.*s` has two values "
                              "named `%.*s`", (int)it->name.length,
                              it->name.text, (int)values[j].name.length,
                              values[j].name.text);
            }
        }
    }
    types_set_fields(c->types, it->symbol->type, values, it->param_count);
}

static void declare_enums_and_variants(struct checker *c)
{
    const struct module *module = c->module;
    size_t i;

    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        if (it->symbol != NULL && it->kind == ITEM_ENUM) {
            c->within = it;
            declare_enum_values(c, it);
            c->within = NULL;
        }
    }
    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        if (it->symbol != NULL && it->kind == ITEM_VARIANT) {
            c->within = it;
            declare_cases(c, it);
            c->within = NULL;
        }
    }
}

/* DESIGN: `own` says the object frees the memory behind the field, so
   the field holds an address the object alone reaches. `str` is
   immutable and shared, and a class or struct field is inline and owned
   by the object already. A field of function type is `own fn`, the code
   and a snapshot that the teardown of the class frees. */
static void check_own_field(struct checker *c, const struct struct_field *f)
{
    const struct type *ft = f->type;

    if (!f->owned || sema_is_error(ft)) {
        return;
    }
    if (ft->kind != TYPE_POINTER && ft->kind != TYPE_SLICE &&
        !(ft->kind == TYPE_FN && ft->owned)) {
        sema_error_at(c, f->pos, "`own` needs a pointer or a "
                      "slice, and `%.*s` has type `%s`",
                      (int)f->name.length, f->name.text, sema_tn(ft));
    }
}

/* DESIGN: a `transient` field holds derived state, such as a cache. The
   copy of the object writes `none` into it, and the field list leaves it
   out, so the default `equals`, `hash` and `serialize` pass over it.
   `none` is the value the copy writes, so the field is a `?*T` or a
   `?fn(...)`. The class frees what it holds in its own `destruct`, and
   `own` would free it a second time. */
static void check_transient_field(struct checker *c,
                                  const struct struct_field *f)
{
    const struct type *ft = f->type;

    if (!f->transient || sema_is_error(ft)) {
        return;
    }
    if ((ft->kind != TYPE_POINTER && ft->kind != TYPE_FN) ||
        !type_is_nullable(ft) || ft->bound) {
        sema_error_at(c, f->pos, "`transient` needs a `?*T` or a "
                      "`?fn(...)`, and `%.*s` has type `%s`",
                      (int)f->name.length, f->name.text, sema_tn(ft));
    } else if (f->owned) {
        sema_error_at(c, f->pos, "`%.*s` is `transient`, so its "
                      "class frees it in `destruct` and it is not `own`",
                      (int)f->name.length, f->name.text);
    }
}

/* DESIGN: `inject` names a field the provider of its interface fills
   before `construct` runs. The type is therefore a pointer to an
   abstract class, which is what an interface is, and never `?*T`,
   because a provider never gives `none`. No default stands beside it,
   since the provider writes the field whatever a literal holds, and
   `own` does not, since the provider owns what it gives. */
static void check_inject_field(struct checker *c, const struct struct_field *f,
                               const struct param *p)
{
    const struct type *ft = f->type;

    if (!f->injected || sema_is_error(ft)) {
        return;
    }
    if (ft->kind != TYPE_POINTER || ft->nullable ||
        ft->element->kind != TYPE_CLASS || !ft->element->has_abstract) {
        sema_error_at(c, f->pos, "`inject` needs a pointer to an "
                      "abstract class, and `%.*s` has type `%s`",
                      (int)f->name.length, f->name.text, sema_tn(ft));
    } else if (p->value != NULL) {
        sema_error_at(c, f->pos, "`%.*s` is `inject`, so its "
                      "provider fills it and it has no default",
                      (int)f->name.length, f->name.text);
    } else if (f->owned) {
        sema_error_at(c, f->pos, "`%.*s` is `inject`, so its "
                      "provider owns what it gives and it is not `own`",
                      (int)f->name.length, f->name.text);
    }
}

/* The declared fields of it, each with its type and bitfield width,
   refusing a name used twice. */
static void resolve_fields(struct checker *c, struct item *it,
                           struct struct_field *fields)
{
    size_t j;

    for (j = 0; j < it->param_count; j++) {
        size_t k;
        fields[j].name = it->params[j].name;
        fields[j].pos = it->params[j].pos;
        fields[j].doc = it->params[j].doc;
        fields[j].form = it->params[j].form;
        fields[j].vis = it->params[j].vis;
        fields[j].owned = it->params[j].owned;
        fields[j].transient = it->params[j].transient;
        fields[j].atomic = it->params[j].atomic;
        fields[j].writable = it->params[j].writable;
        fields[j].injected = it->params[j].injected;
        fields[j].inject_final = it->params[j].inject_final;

        fields[j].value = it->params[j].value;
        c->target_sized = true;
        fields[j].type = sema_resolve_type(c, it->params[j].type);
        c->target_sized = false;
        /* An `own` field of function type holds `own fn`, which frees
           its snapshot with the object. */
        if (fields[j].owned && fields[j].type->kind == TYPE_FN &&
            !fields[j].type->bound) {
            fields[j].type = types_fn_owned(c->types, fields[j].type);
        }
        if (it->params[j].bits != NULL ||
            type_field_is_unit_break(&fields[j])) {
            fields[j].bits = bitfield_width(c, &it->params[j],
                                            fields[j].type);
        }
        if (it->kind == ITEM_UNION && type_field_is_unit_break(&fields[j])) {
            sema_error_at(c, fields[j].pos, "a union holds no zero-width "
                          "bitfield");
        }
        for (k = 0; k < j && !type_field_is_unit_break(&fields[j]); k++) {
            if (sema_same_name(&fields[k].name, &fields[j].name)) {
                sema_error_at(c, fields[j].pos, "%s `%.*s` has two fields "
                              "named `%.*s`",
                              it->kind == ITEM_UNION ? "union" : "struct",
                              (int)it->name.length, it->name.text,
                              (int)fields[j].name.length, fields[j].name.text);
            }
        }
    }
}

/* The fields of a struct, union or class, with their checks, their
   defaults and the layout attributes of the item. */
static void declare_fields(struct checker *c, struct item *it)
{
    struct struct_field *fields;
    size_t base_fields;
    size_t hidden;
    size_t j;

    /* DESIGN: a class carries its base as field 0, named `super`.
       The name is a keyword, so no declared field collides with it,
       and `self.super` is then ordinary field access. The base is
       nested whole, so the C rules of chapter 18 place it at offset
       0 and the class's own fields after it. */
    base_fields = it->kind == ITEM_CLASS ? 1 : 0;
    hidden = it->kind == ITEM_CLASS && sema_needs_hidden_lock(it) ? 1 : 0;
    fields = types_alloc_array(c->arena,
                               it->param_count + base_fields + hidden,
                               sizeof *fields);
    if (base_fields != 0) {
        static const char super_text[] = "super";
        memset(&fields[0], 0, sizeof fields[0]);
        fields[0].name.text = super_text;
        fields[0].name.length = sizeof super_text - 1;
        fields[0].pos = it->name_pos;
        fields[0].form = FIELD_BASE;
        fields[0].type = it->symbol->type->base;
    }
    fields += base_fields;
    resolve_fields(c, it, fields);
    for (j = 0; j < it->param_count; j++) {
        check_own_field(c, &fields[j]);
    }
    for (j = 0; j < it->param_count; j++) {
        check_transient_field(c, &fields[j]);
    }
    for (j = 0; j < it->param_count; j++) {
        check_inject_field(c, &fields[j], &it->params[j]);
    }
    /* A default is checked against the type of its field, so the
       value that lowering writes is complete and typed. It is a
       constant expression, and its value is kept for the library
       file. */
    for (j = 0; j < it->param_count; j++) {
        struct const_value *v;
        if (it->params[j].value == NULL ||
            !sema_require(c, it->params[j].value,
                          sema_check_expr(c, it->params[j].value,
                                          fields[j].type),
                          fields[j].type)) {
            continue;
        }
        v = arena_alloc(c->arena, sizeof *v);
        if (sema_eval_const(c, it->params[j].value, v)) {
            fields[j].constant = v;
        }
    }
    /* The hidden lock of a synchronized class follows the fields the
       class declares. */
    if (hidden != 0) {
        sema_hidden_lock(c, it, &fields[it->param_count]);
    }
    types_set_fields(c->types, it->symbol->type, fields - base_fields,
                     it->param_count + base_fields + hidden);
    it->symbol->type->packed = it->packed;
    if (it->align != NULL) {
        it->symbol->type->align = alignment(c, it->align);
    }
    if (it->simd) {
        check_simd_struct(c, it);
    }
    sema_generic_ready(c, it->symbol->type);
}

/* The fields of every struct, union and class, then the refusal of a
   type that contains itself. */
static void declare_all_fields(struct checker *c)
{
    const struct module *module = c->module;
    size_t i;

    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        if (it->symbol != NULL &&
            (it->kind == ITEM_STRUCT || it->kind == ITEM_UNION ||
             it->kind == ITEM_CLASS)) {
            c->within = it;
            declare_fields(c, it);
            c->within = NULL;
        }
    }
    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        const char *kind = it->kind == ITEM_STRUCT    ? "struct"
                           : it->kind == ITEM_UNION   ? "union"
                           : it->kind == ITEM_CLASS   ? "class"
                           : it->kind == ITEM_VARIANT ? "variant"
                                                      : NULL;
        if (it->symbol != NULL && kind != NULL &&
            types_find_cycle(it->symbol->type) != NULL) {
            sema_error_at(c, it->name_pos, "%s `%.*s` contains itself", kind,
                          (int)it->name.length, it->name.text);
            types_break_cycles(it->symbol->type, sema_builtin(c, TYPE_ERROR));
        }
    }
}

static void declare_function_types(struct checker *c)
{
    const struct module *module = c->module;
    size_t i;

    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        if (it->symbol != NULL &&
            (it->kind == ITEM_FN || it->kind == ITEM_EXTERN_FN)) {
            it->symbol->type = function_type(c, it);
        }
        /* The operators of a simd struct are built in, and an `operator
           fn` beside them would never be called. */
        if (it->symbol != NULL && it->kind == ITEM_FN && it->is_operator &&
            it->symbol->type->kind == TYPE_FN &&
            it->symbol->type->param_count > 0 &&
            type_is_simd(sema_struct_of(it->symbol->type->params[0]))) {
            sema_error_at(c, it->name_pos, "`%s` is a `simd struct`, whose "
                          "operators are built in",
                          sema_tn(sema_struct_of(it->symbol->type->params[0])));
        }
    }
}

/* DESIGN: a function of a struct body carries the name `T.f`, so its
   symbol is `module.T.f`, one segment more than a free function. It
   is not declared in the module scope, because it is reached through
   its type. */
static void declare_members(struct checker *c, struct item *it)
{
    size_t j;

    for (j = 0; j < it->member_count; j++) {
        struct item *m = it->members[j];
        struct symbol *sym = arena_alloc(c->arena, sizeof *sym);
        struct name text;
        size_t k;
        for (k = 0; k < j; k++) {
            if (sema_same_name(&it->members[k]->name, &m->name) &&
                same_qualifier(it, it->members[k], m)) {
                sema_error_at(c, m->name_pos, "`%.*s` declares `%.*s` twice",
                              (int)it->name.length, it->name.text,
                              (int)m->name.length, m->name.text);
            }
        }
        text = types_member_symbol(c->arena, &it->name, m);
        if (m->contract == FN_ABSTRACT) {
            it->symbol->type->has_abstract = true;
        }
        /* A static field is a global, which lowering writes, and a
           constant of a body is folded where it is named. */
        /* DESIGN: every public function of an export class has the
           C symbol `Class_fn`, because the generated header declares
           a prototype for each one. */
        if (it->exported && m->kind == ITEM_FN && m->pub) {
            m->exported = true;
        }
        sym->kind = m->kind != ITEM_CONST  ? SYMBOL_FN
                    : m->is_static         ? SYMBOL_GLOBAL
                                           : SYMBOL_CONST;
        sym->name = text;
        sym->pos = m->name_pos;
        sym->item = m;
        sym->exported = m->exported;
        sym->may_fail = m->may_fail;
        sym->doc = m->doc;

        m->symbol = sym;
        if (m->kind == ITEM_FN) {
            sym->type = function_type(c, m);
        }
    }
}

/* The members of every body, the export mark of every type and the
   constants of the module. */
static void declare_all_members(struct checker *c)
{
    const struct module *module = c->module;
    size_t i;

    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        if (it->symbol != NULL && it->symbol->type != NULL) {
            c->within = it;
            declare_members(c, it);
            c->within = NULL;
        }
    }
    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        if (it->symbol != NULL &&
            (it->kind == ITEM_STRUCT || it->kind == ITEM_UNION ||
             it->kind == ITEM_CLASS || it->kind == ITEM_ENUM ||
             it->kind == ITEM_VARIANT)) {
            it->symbol->type->item_exported = it->exported;
        }
    }
    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        if (it->symbol != NULL && it->kind == ITEM_CONST) {
            sema_const_symbol(c, it->symbol, it->name_pos);
        }
    }
}

/* Every body calls with the defaults and the `own` parameters, so they
   are known before the first body is checked. */
static void check_signatures(struct checker *c)
{
    const struct module *module = c->module;
    size_t i;
    size_t j;

    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        if (it->kind == ITEM_FN || it->kind == ITEM_EXTERN_FN) {
            check_defaults(c, it);
            check_owned(c, it);
        }
        c->within = it;
        for (j = 0; it->symbol != NULL && j < it->member_count; j++) {
            if (it->members[j]->kind == ITEM_FN) {
                check_defaults(c, it->members[j]);
                check_owned(c, it->members[j]);
            }
        }
        c->within = NULL;
    }
    /* DESIGN: a singleton has one instance, which `Config.get()` makes
       on the first call. The program never allocates one, so the checker
       declares `get` itself, before any body names it. */
    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        if (it->kind == ITEM_CLASS && it->is_singleton &&
            it->symbol != NULL && it->symbol->type != NULL) {
            declare_get(c, it);
        }
    }
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

/* DESIGN: `implements name: I` places a sub-object of the abstract
   class I inside the class. Only an abstract class may be implemented,
   and one class implements an interface once, so that every name it
   provides has one path. */
static void check_implements(struct checker *c, const struct item *it,
                             const struct type *t)
{
    size_t j;

    for (j = 0; j < t->field_count; j++) {
        const struct type *iface = t->fields[j].type;
        const struct type *chain;
        size_t k;
        if (t->fields[j].form != FIELD_IMPL) {
            continue;
        }
        if (iface == NULL || iface->kind != TYPE_CLASS ||
            !iface->has_abstract) {
            sema_error_at(c, t->fields[j].pos, "`%s` is not abstract and "
                          "cannot be implemented", sema_tn(iface));
            continue;
        }
        for (k = 0; k < j; k++) {
            if (t->fields[k].form == FIELD_IMPL &&
                t->fields[k].type == iface) {
                sema_error_at(c, t->fields[j].pos,
                              "`%.*s` implements `%s` twice",
                              (int)it->name.length, it->name.text,
                              sema_tn(iface));
            }
        }
        for (chain = sema_inherited(t); chain != NULL;
             chain = sema_inherited(chain)) {
            for (k = 0; k < chain->field_count; k++) {
                if (chain->fields[k].form == FIELD_IMPL &&
                    chain->fields[k].type == iface) {
                    sema_error_at(c, t->fields[j].pos, "`%.*s` implements "
                                  "`%s`, which `%.*s` implements already",
                                  (int)it->name.length, it->name.text,
                                  sema_tn(iface), (int)chain->name.length,
                                  chain->name.text);
                }
            }
        }
    }
}

/* A plain or `use` field of an abstract class is refused here, after
   every class knows whether a contract reaches it. The base and an
   interface sub-object are the two places an abstract class is a
   value, so they are skipped. */
static void refuse_abstract_fields(struct checker *c, const struct type *t)
{
    size_t j;

    for (j = 0; j < t->field_count; j++) {
        char what[96];
        if (t->fields[j].form == FIELD_BASE ||
            t->fields[j].form == FIELD_TABLE ||
            t->fields[j].form == FIELD_IMPL) {
            continue;
        }
        sema_format_to(what, sizeof what, "the field `%.*s`",
                       (int)t->fields[j].name.length, t->fields[j].name.text);
        sema_refuse_abstract_value(c, t->fields[j].pos, what,
                                   t->fields[j].type);
    }
}

/* The function of the chain or of an implemented interface that the
   member m of t matches by name, or NULL. */
static const struct item *matched_above(const struct type *t,
                                        const struct item *m)
{
    const struct item *above = NULL;
    const struct type *base;

    if (sema_inherited(t) != NULL) {
        above = types_primary_member(sema_inherited(t), &m->name);
    }
    /* An interface declares functions the class fills, so a
       `concrete fn` matches there as well as in the base chain. An
       interface a base implements counts, because a body below replaces
       the one of the base in its table. */
    for (base = t; above == NULL && base != NULL; base = sema_inherited(base)) {
        size_t k;
        for (k = 0; above == NULL && k < base->field_count; k++) {
            const struct type *iface;
            if (base->fields[k].form != FIELD_IMPL) {
                continue;
            }
            for (iface = base->fields[k].type;
                 iface != NULL &&
                 above == NULL; iface = sema_inherited(iface)) {
                const struct item *found = sema_find_member(iface, &m->name);
                if (found != NULL && found->kind == ITEM_FN) {
                    above = found;
                }
            }
        }
    }
    return above;
}

/* DESIGN: a contract is declared with `abstract fn` and filled with
   `concrete fn` of the same signature. The checker walks the chain of
   `inherits` fields of every struct and refuses one that leaves a
   contract unfilled, and `check_replacement` compares each `concrete
   fn` with the entries it fills. */
static void check_contracts(struct checker *c, const struct item *it,
                            const struct type *t)
{
    size_t j;

    for (j = 0; j < it->member_count; j++) {
        struct item *m = it->members[j];
        const struct item *above;
        if (m->kind != ITEM_FN) {
            continue;
        }
        above = matched_above(t, m);
        if (m->contract == FN_ABSTRACT) {
            continue;
        }
        if (m->contract == FN_CONCRETE && above == NULL) {
            sema_error_at(c, m->name_pos, "`concrete fn %.*s` of `%.*s` fills "
                          "no abstract function", (int)m->name.length,
                          m->name.text, (int)it->name.length, it->name.text);
        } else if (m->contract == FN_CONCRETE) {
            check_replacement(c, it, t, m);
        } else if (m->contract == FN_PLAIN && above != NULL &&
                   above->contract != FN_PLAIN) {
            sema_error_at(c, m->name_pos, "`%.*s` of `%.*s` matches an "
                          "abstract function and needs `concrete`",
                          (int)m->name.length, m->name.text,
                          (int)it->name.length, it->name.text);
        }
    }
}

/* DESIGN: a struct may not redeclare a name that its chain already
   has. A `concrete fn` is the exception, because it fills the abstract
   function of that name. */
static void refuse_redeclared(struct checker *c, const struct item *it,
                              const struct type *t)
{
    const struct type *base;
    size_t j;

    for (base = sema_inherited(t); base != NULL; base = sema_inherited(base)) {
        for (j = 0; j < t->field_count; j++) {
            if (t->fields[j].form == FIELD_BASE ||
                t->fields[j].form == FIELD_TABLE) {
                continue;
            }
            if (sema_find_field(base, &t->fields[j].name) != NULL ||
                sema_find_member(base, &t->fields[j].name) != NULL) {
                sema_error_at(c, t->fields[j].pos, "`%.*s` already has `%.*s`",
                              (int)base->name.length, base->name.text,
                              (int)t->fields[j].name.length,
                              t->fields[j].name.text);
            }
        }
        for (j = 0; j < it->member_count; j++) {
            const struct item *m = it->members[j];
            const struct item *shadowed;
            /* `construct` and `destruct` repeat down a chain by design,
               because the compiler runs one body per level. A `concrete
               fn` replaces an entry, and an `abstract fn` re-opens one,
               which an interface does when it names a function of the
               root. */
            if (m->contract != FN_PLAIN ||
                sema_name_is(&m->name, "construct") ||
                sema_name_is(&m->name, "destruct")) {
                continue;
            }
            /* DESIGN: a static function is namespaced by its class and
               reached as `Class.f`, never through a value and never
               through a table. Two statics of one name in a chain name
               two functions and no call is ambiguous, so the rule leaves
               them. A function that takes `self` is another matter, and
               so is a field. */
            shadowed = sema_find_member(base, &m->name);
            if (!m->has_self && sema_find_field(base, &m->name) == NULL &&
                shadowed != NULL && shadowed->kind == ITEM_FN &&
                !shadowed->has_self) {
                continue;
            }
            if (sema_find_field(base, &m->name) != NULL || shadowed != NULL) {
                sema_error_at(c, m->name_pos, "`%.*s` already has `%.*s`",
                              (int)base->name.length, base->name.text,
                              (int)m->name.length, m->name.text);
            }
        }
    }
}

/* DESIGN: every abstract function of the chain needs a concrete one at
   or below the class that declares it. An abstract class may leave one
   open. It is never a complete value, and every class below it is
   checked here. */
static void require_filled_chain(struct checker *c, const struct item *it,
                                 const struct type *t)
{
    const struct type *base;
    size_t j;

    for (base = sema_inherited(t); base != NULL && !it->is_abstract;
         base = sema_inherited(base)) {
        for (j = 0; j < base->member_count; j++) {
            const struct item *a = base->members[j];
            const struct item *filled;
            if (a->kind != ITEM_FN || a->contract != FN_ABSTRACT) {
                continue;
            }
            filled = types_primary_member(t, &a->name);
            if (filled == NULL || filled->contract != FN_CONCRETE) {
                sema_error_at(c, it->name_pos, "`%.*s` lacks `concrete fn "
                              "%.*s`", (int)it->name.length, it->name.text,
                              (int)a->name.length, a->name.text);
            }
        }
    }
}

/* DESIGN: a language hook has the signature its construct calls, and
   the message states that signature. No hook may fail, since the
   construct that calls it has no place for a handler. `set_index` takes
   the index and the element that `index` of the same type reads, so
   `e[i] = e[i]` holds for every type that has both. A class writes the
   receiver `self`, and a free function of a struct its first parameter
   as declared. */
static void check_hook(struct checker *c, const struct item *m,
                       struct type *owner)
{
    const struct type *sig = m->symbol != NULL ? m->symbol->type : NULL;
    char self[128];
    bool ok;

    if (sig == NULL || sig->kind != TYPE_FN) {
        return;
    }
    if (m->has_self || m->param_count == 0) {
        snprintf(self, sizeof self, "self");
    } else {
        snprintf(self, sizeof self, "%.*s: %s", (int)m->params[0].name.length,
                 m->params[0].name.text, sema_tn(sig->params[0]));
    }
    if (sema_name_is(&m->name, LANG_HOOK_NEXT)) {
        ok = !sig->may_fail && sig->param_count == 1 &&
             sig->result->kind == TYPE_BOOL;
        if (!ok) {
            sema_error_at(c, m->name_pos, "`operator fn next` is written "
                          "`operator fn next(%s) -> bool`", self);
        }
    } else if (sema_name_is(&m->name, LANG_HOOK_VALUE)) {
        ok = !sig->may_fail && sig->param_count == 1 &&
             sig->result->kind != TYPE_VOID;
        if (!ok) {
            sema_error_at(c, m->name_pos, "`operator fn value` is written "
                          "`operator fn value(%s) -> T`", self);
        }
    } else if (sema_name_is(&m->name, LANG_HOOK_ITER)) {
        ok = !sig->may_fail && sig->param_count == 1 &&
             sema_is_iterator(c, sig->result);
        if (!ok) {
            sema_error_at(c, m->name_pos, "`operator fn iter` is written "
                          "`operator fn iter(%s) -> I`, where `I` has "
                          "`operator fn next` and `operator fn value`", self);
        }
    } else if (sema_name_is(&m->name, LANG_HOOK_INDEX)) {
        ok = !sig->may_fail && sig->param_count == 2 &&
             sig->result->kind != TYPE_VOID;
        if (!ok) {
            sema_error_at(c, m->name_pos, "`operator fn index` is written "
                          "`operator fn index(%s, i: I) -> T`", self);
        }
    } else if (sema_name_is(&m->name, LANG_HOOK_SET_INDEX)) {
        struct symbol *index = sema_hook(c, owner, LANG_HOOK_INDEX);
        const struct type *read = index != NULL ? index->type : NULL;
        bool paired = read != NULL && read->kind == TYPE_FN &&
                      !read->may_fail && read->param_count == 2 &&
                      read->result->kind != TYPE_VOID;
        ok = !sig->may_fail && sig->param_count == 3 &&
             sig->result->kind == TYPE_VOID &&
             (!paired || (sig->params[1] == read->params[1] &&
                          sig->params[2] == read->result));
        if (!ok && paired) {
            sema_error_at(c, m->name_pos, "`operator fn set_index` is "
                          "written `operator fn set_index(%s, i: %s, v: "
                          "%s)`, as `index` reads", self,
                          sema_tn(read->params[1]), sema_tn(read->result));
        } else if (!ok) {
            sema_error_at(c, m->name_pos, "`operator fn set_index` is "
                          "written `operator fn set_index(%s, i: I, v: T)`",
                          self);
        }
    }
}

/* An `operator fn` carries one of the nineteen names the table holds,
   and nothing else, and a hook the signature its construct calls. owner
   is the type the functions belong to. */
static void check_operator_item(struct checker *c, const struct item *m,
                                struct type *owner)
{
    if (m->kind != ITEM_FN || !m->is_operator) {
        return;
    }
    if (!sema_operator_named(&m->name)) {
        sema_error_at(c, m->name_pos, "`operator fn` takes one of `add sub "
                      "mul div rem neg eq lt and or xor shl shr not iter "
                      "next value index set_index`");
        return;
    }
    check_hook(c, m, owner);
}

static void check_operator_names(struct checker *c, const struct item *it,
                                 struct type *t)
{
    size_t j;

    for (j = 0; j < it->member_count; j++) {
        check_operator_item(c, it->members[j], t);
    }
}

/* The operators of a struct are free functions of its module, and the
   type they belong to is the one their first parameter names. */
static void check_free_operators(struct checker *c)
{
    const struct module *module = c->module;
    size_t i;

    for (i = 0; i < module->item_count; i++) {
        const struct item *it = module->items[i];
        struct type *owner = NULL;
        if (it->kind != ITEM_FN || !it->is_operator || it->symbol == NULL ||
            it->symbol->type == NULL || it->symbol->type->kind != TYPE_FN) {
            continue;
        }
        if (it->symbol->type->param_count > 0) {
            owner = it->symbol->type->params[0];
        }
        check_operator_item(c, it, owner);
    }
}

/* DESIGN: one `construct` per class, and every alternative is a static
   function with a name of its own. A `construct` without arguments
   cannot fail and returns nothing. */
static void check_one_construct(struct checker *c, const struct item *it)
{
    const struct item *first = NULL;
    size_t j;

    for (j = 0; j < it->member_count; j++) {
        struct item *m = it->members[j];
        if (m->kind != ITEM_FN || !sema_name_is(&m->name, "construct")) {
            continue;
        }
        if (first != NULL) {
            sema_error_at(c, m->name_pos, "`%.*s` has one `construct`, "
                          "and every other maker is a static function",
                          (int)it->name.length, it->name.text);
        }
        first = m;
        if (!m->has_self) {
            sema_error_at(c, m->name_pos,
                          "`construct` takes `self` as its first parameter");
        } else if (m->param_count == 0 && m->result != NULL) {
            sema_error_at(c, m->name_pos, "a `construct` without "
                          "arguments cannot fail and returns nothing");
        }
    }
}

/* DESIGN: the qualifier of a `concrete fn` names the table it fills:
   the class itself, a class of its chain, or an interface it
   implements. Any other name reaches no table. Two qualifiers that
   reach one table fill it twice, which is refused as a name declared
   twice. */
static void check_qualifiers(struct checker *c, const struct item *it,
                             const struct type *t)
{
    size_t j;

    for (j = 0; j < it->member_count; j++) {
        const struct item *m = it->members[j];
        size_t k;
        if (m->kind != ITEM_FN || m->qualifier.length == 0) {
            continue;
        }
        if (qualified_table(t, &m->qualifier) == NULL) {
            sema_error_at(c, m->qualifier_pos, "`%.*s` is no base and no "
                          "interface of `%.*s`", (int)m->qualifier.length,
                          m->qualifier.text, (int)it->name.length,
                          it->name.text);
            continue;
        }
        for (k = 0; k < j; k++) {
            const struct item *other = it->members[k];
            if (other->kind == ITEM_FN &&
                sema_same_name(&other->name, &m->name) &&
                other->qualifier.length > 0 &&
                !sema_same_name(&other->qualifier, &m->qualifier) &&
                one_table(t, other, m)) {
                sema_error_at(c, m->name_pos, "`%.*s` declares `%.*s` twice",
                              (int)it->name.length, it->name.text,
                              (int)m->name.length, m->name.text);
            }
        }
    }
}

/* An interface leaves its functions open, and the class that
   implements it fills them. The chain of the interface counts, because
   an interface may inherit another abstract class. */
static void require_filled_interfaces(struct checker *c,
                                      const struct item *it,
                                      const struct type *t)
{
    size_t j;

    for (j = 0; j < t->field_count && !it->is_abstract; j++) {
        const struct type *iface;
        if (t->fields[j].form != FIELD_IMPL) {
            continue;
        }
        for (iface = t->fields[j].type; iface != NULL;
             iface = sema_inherited(iface)) {
            size_t k;
            for (k = 0; k < iface->member_count; k++) {
                const struct item *a = iface->members[k];
                const struct item *filled;
                if (a->kind != ITEM_FN || a->contract != FN_ABSTRACT) {
                    continue;
                }
                filled = types_interface_member(t, t->fields[j].type,
                                                &a->name);
                if (filled == NULL || filled->contract != FN_CONCRETE) {
                    sema_error_at(c, it->name_pos, "`%.*s` lacks `concrete "
                                  "fn %.*s` of `%s`", (int)it->name.length,
                                  it->name.text, (int)a->name.length,
                                  a->name.text, sema_tn(iface));
                }
            }
        }
    }
}

/* The type nested directly in cls that t names, or NULL. t may reach
   it through a pointer, an array, a slice, a channel, a Job, a tuple or
   a function type. A type nested deeper has no name in the body of
   cls. */
static const struct type *names_nested(const struct type *t,
                                       const struct item *cls)
{
    const struct type *found;
    size_t i;

    if (t == NULL) {
        return NULL;
    }
    for (i = 0; i < cls->nested_count; i++) {
        if (cls->nested[i]->symbol != NULL &&
            cls->nested[i]->symbol->type == t) {
            return t;
        }
    }
    switch (t->kind) {
    case TYPE_POINTER:
    case TYPE_ARRAY:
    case TYPE_SLICE:
        return names_nested(t->element, cls);
    case TYPE_FN:
    case TYPE_TUPLE:
        for (i = 0; i < t->param_count; i++) {
            if ((found = names_nested(t->params[i], cls)) != NULL) {
                return found;
            }
        }
        return names_nested(t->result, cls);
    case TYPE_STRUCT:
        /* A channel keeps its element and a Job its result. */
        if ((found = names_nested(t->element, cls)) != NULL) {
            return found;
        }
        return names_nested(t->result, cls);
    default:
        return NULL;
    }
}

static const char *level_word(enum visibility vis)
{
    return vis == VIS_PROTECTED ? "protected" : "public";
}

/* DESIGN: a type nested in a class is private to it, so code outside
   can neither create one nor receive one. A signature that code outside
   the class reaches therefore names none of them: a `pub` or `protected`
   function or field, a `pub` constant, and a `construct` that takes
   arguments, which `T(args)` calls from outside and which C reaches as
   `anti_<Class>_construct`. */
static void refuse_nested_in_signatures(struct checker *c,
                                        const struct item *it,
                                        const struct type *t)
{
    const struct type *found;
    size_t i;

    for (i = 0; i < t->field_count; i++) {
        const struct struct_field *f = &t->fields[i];
        if (f->form != FIELD_PLAIN || f->vis == VIS_PRIVATE ||
            (found = names_nested(f->type, it)) == NULL) {
            continue;
        }
        sema_error_at(c, f->pos, "the %s field `%.*s` of `%.*s` names "
                      "`%s`, which is private to `%.*s`",
                      level_word(f->vis), (int)f->name.length, f->name.text,
                      (int)it->name.length, it->name.text, sema_tn(found),
                      (int)it->name.length, it->name.text);
    }
    for (i = 0; i < it->member_count; i++) {
        const struct item *m = it->members[i];
        const struct type *mt = NULL;
        bool made = m->kind == ITEM_FN && m->has_self &&
                    m->param_count > 0 && sema_name_is(&m->name, "construct");
        if (m->symbol == NULL || (m->vis == VIS_PRIVATE && !made)) {
            continue;
        }
        if (m->kind == ITEM_FN) {
            mt = m->symbol->type;
        } else if (m->type != NULL) {
            c->quiet++;
            mt = sema_resolve_type(c, m->type);
            c->quiet--;
        }
        if ((found = names_nested(mt, it)) == NULL) {
            continue;
        }
        sema_error_at(c, m->name_pos, "the %s %s `%.*s` of `%.*s` names "
                      "`%s`, which is private to `%.*s`",
                      made ? "public" : level_word(m->vis),
                      m->kind == ITEM_FN ? "function" : "constant",
                      (int)m->name.length, m->name.text,
                      (int)it->name.length, it->name.text, sema_tn(found),
                      (int)it->name.length, it->name.text);
    }
}

/* The checks of every type with fields: its interfaces, its abstract
   fields, then its contracts and the functions of its body. */
static void check_types(struct checker *c)
{
    const struct module *module = c->module;
    size_t i;

    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        struct type *t = it->symbol != NULL && it->kind != ITEM_TYPE
                             ? it->symbol->type
                             : NULL;
        if (type_has_fields(t)) {
            check_implements(c, it, t);
        }
    }
    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        struct type *t = it->symbol != NULL && it->kind != ITEM_TYPE
                             ? it->symbol->type
                             : NULL;
        if (type_has_fields(t)) {
            refuse_abstract_fields(c, t);
        }
    }
    check_free_operators(c);
    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        struct type *t = it->symbol != NULL && it->kind != ITEM_TYPE
                             ? it->symbol->type
                             : NULL;
        if (!type_has_fields(t)) {
            continue;
        }
        check_contracts(c, it, t);
        refuse_redeclared(c, it, t);
        require_filled_chain(c, it, t);
        check_operator_names(c, it, t);
        check_one_construct(c, it);
        check_qualifiers(c, it, t);
        require_filled_interfaces(c, it, t);
        if (it->kind == ITEM_CLASS && it->nested_count > 0) {
            c->within = it;
            refuse_nested_in_signatures(c, it, t);
            c->within = NULL;
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
        if (!filled_or_copied(c, t)) {
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
    check_provides(c, module);
    check_link_names(c, module->frameworks, module->framework_count,
                     "framework", "framework");
    check_link_names(c, module->linux_libraries, module->linux_library_count,
                     "linux", "library");
}

bool sema_check(struct module *module, const char *module_name,
                const char *package,
                const struct interface *const *libraries,
                size_t library_count, struct types *types,
                struct arena *arena, struct diagnostics *diags,
                bool program)
{
    struct checker c;
    size_t i;

    memset(&c, 0, sizeof c);
    c.types = types;
    c.arena = arena;
    c.diags = diags;
    c.module = module;
    c.module_name.text = module_name;
    c.module_name.length = strlen(module_name);
    c.package = package != NULL ? package : module_name;
    c.libraries = libraries;
    c.library_count = library_count;
    c.scope = &c.module_scope;
    c.ok = true;
    c.program = program;
    declare_root(&c);
    for (i = 0; i < module->import_count; i++) {
        declare_import(&c, &module->imports[i]);
    }
    declare_items(&c);
    sema_declare_generics(&c);
    sema_resolve_generics(&c);
    resolve_bases(&c);
    declare_enums_and_variants(&c);
    declare_all_fields(&c);
    sema_check_guards(&c);
    declare_function_types(&c);
    declare_all_members(&c);
    check_signatures(&c);
    sema_run_pending(&c);
    check_free_functions(&c);
    check_types(&c);
    check_member_functions(&c);
    report_program(&c);
    check_boundary(&c);
    sema_report_unfixed(&c);
    free(c.module_scope.entries);
    return c.ok;
}

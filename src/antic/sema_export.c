/* The checks of what crosses to C and the interface of a checked
   module. */

#include <stdlib.h>
#include <string.h>

#include "sema_checker.h"

/* A copy of the length bytes at text with a NUL after them, in the
   memory pool of the first parameter, which frees it. */
static const char *keep_name(struct arena *arena, const char *text,
                             size_t length)
{
    char *copy = arena_alloc(arena, length + 1);

    memcpy(copy, text, length);
    return copy;
}

/* DESIGN: `anti.lang.Error` crosses to C as `struct anti_Error *`, the
   type the object model gives the generated helpers. The header declares
   the tag itself and C never reads the layout, so an export signature
   names the class without it being an `export class`. A class below it
   has no C name and stays out. */
static bool is_error_class(const struct type *t)
{
    return types_is_lang_error(t);
}

/* Whether t is a type declared in the body of cls, at any depth. */
static bool nested_in(const struct type *t, const struct item *cls)
{
    size_t i;

    for (i = 0; cls != NULL && i < cls->nested_count; i++) {
        const struct item *inner = cls->nested[i];
        if ((inner->symbol != NULL && inner->symbol->type == t) ||
            nested_in(t, inner)) {
            return true;
        }
    }
    return false;
}

/* Whether a value of type t has a C representation. That is a scalar
   other than char, a pointer to such a type, an exported struct or union,
   or a function pointer of such types. A struct field may also be a
   fixed-size array of such a type. A type nested in the export class cls
   crosses with it. Sets *hidden to a struct that is not exported. */
static bool c_representable(const struct type *t, bool field,
                            const struct item *cls,
                            const struct type **hidden)
{
    size_t i;

    switch (t->kind) {
    case TYPE_CHAR:
    case TYPE_STR:
    case TYPE_SLICE:
    case TYPE_NONE:
        return false;
    case TYPE_ARRAY:
        return field && t->length_of == NULL &&
               c_representable(t->element, true, cls, hidden);
    case TYPE_POINTER:
        return c_representable(t->element, false, cls, hidden);
    case TYPE_FN:
        for (i = 0; i < t->param_count; i++) {
            if (!c_representable(t->params[i], false, cls, hidden)) {
                return false;
            }
        }
        return t->result->kind == TYPE_VOID ||
               c_representable(t->result, false, cls, hidden);
    /* Flags crosses as the struct of four bools that the header writes
       for it. A Mutex and a channel hold a handle of the runtime, which
       C has no declaration of. */
    case TYPE_STRUCT:
    case TYPE_CLASS:
    case TYPE_VARIANT:
        if (t->item_exported || is_error_class(t) || types_is_flags(t) ||
            nested_in(t, cls)) {
            return true;
        }
        if (types_is_mutex(t) || types_is_chan(t) || types_is_regex(t) ||
            types_is_match(t)) {
            return false;
        }
        /* The hidden lock of a synchronized class, which the header
           writes as its bytes. */
        if (types_is_object_lock(t)) {
            return true;
        }
        *hidden = t;
        return false;
    /* A tuple crosses as the named struct the header writes for it, so
       it crosses when every element does. Its elements are fields of
       that struct, which is why an array among them is allowed. */
    case TYPE_TUPLE:
        for (i = 0; i < t->param_count; i++) {
            if (!c_representable(t->params[i], true, cls, hidden)) {
                return false;
            }
        }
        return true;
    /* A `?T` crosses as the struct of the value and a `bool` that the
       header writes for it, so it crosses when its value does. */
    case TYPE_OPTIONAL:
        return c_representable(t->element, true, cls, hidden);
    /* An enum is its underlying integer, which the header writes as an
       enum of the same name. */
    case TYPE_ENUM:
        return true;
    default:
        return true;
    }
}

/* Report a type in an export signature or struct that C cannot represent.
   what names the place, as `the parameter `s` of export fn `f``. */
static void check_c_type(struct checker *c, struct pos pos, const char *what,
                         const struct type *t, bool field,
                         const struct item *cls)
{
    const struct type *hidden = NULL;

    if (sema_is_error(t) || c_representable(t, field, cls, &hidden)) {
        return;
    }
    if (hidden != NULL) {
        sema_error_at(c, pos, "%s has type `%s`, and `%s` is not exported",
                      what,
                      sema_tn(t), sema_tn(hidden));
    } else {
        sema_error_at(c, pos, "%s has type `%s`, which C cannot represent",
                      what,
                      sema_tn(t));
    }
}

/* DESIGN: C has no type that says a pointer is never `none`, so a
   pointer that comes from C may be `none` whatever the declaration says.
   Every pointer of an `extern fn` and of the C callbacks in its
   signature is therefore `?*T`, and the program checks it where it uses
   it. An export item goes the other way: its pointers are the ones this
   program holds, so a `*T` there is honest, and the header marks it as
   non-null in a comment. */
static bool has_plain_pointer(const struct type *t)
{
    size_t i;

    if (t == NULL) {
        return false;
    }
    switch (t->kind) {
    case TYPE_POINTER:
        return !t->nullable || has_plain_pointer(t->element);
    case TYPE_ARRAY:
    case TYPE_SLICE:
        return has_plain_pointer(t->element);
    case TYPE_FN:
        for (i = 0; i < t->param_count; i++) {
            if (has_plain_pointer(t->params[i])) {
                return true;
            }
        }
        return has_plain_pointer(t->result);
    default:
        return false;
    }
}

/* Report a `*T` where the C boundary takes `?*T`. what names the place. */
static void check_c_nullable(struct checker *c, struct pos pos,
                             const char *what, const struct type *t)
{
    if (!sema_is_error(t) && has_plain_pointer(t)) {
        sema_error_at(c, pos, "every pointer %s is `?*T`", what);
    }
}

/* Every pointer of an `extern fn` signature, the C callbacks in it
   included. */
void sema_check_extern_fn(struct checker *c, struct item *it)
{
    const struct type *t = it->symbol->type;
    char what[160];
    size_t i;

    if (t == NULL || t->kind != TYPE_FN) {
        return;
    }
    for (i = 0; i < it->param_count && i < t->param_count; i++) {
        text_format(what, sizeof what,
                    "of the parameter `%.*s` of `extern fn "
                    "%.*s`", (int)it->params[i].name.length,
                    it->params[i].name.text, (int)it->name.length,
                    it->name.text);
        check_c_nullable(c, it->params[i].pos, what, t->params[i]);
    }
    if (it->result != NULL && t->result->kind != TYPE_VOID) {
        text_format(what, sizeof what, "of the result of `extern fn %.*s`",
                    (int)it->name.length, it->name.text);
        check_c_nullable(c, it->result->pos, what, t->result);
    }
}

/* The type nested in cls that a field of type t holds by value or
   points to, through arrays and pointers, or NULL. */
static const struct type *held_nested(const struct type *t,
                                      const struct item *cls)
{
    while (t != NULL && (t->kind == TYPE_POINTER || t->kind == TYPE_ARRAY)) {
        t = t->element;
    }
    return t != NULL && nested_in(t, cls) ? t : NULL;
}

/* DESIGN: a type nested in an export class crosses to C with the class
   when the layout reaches it. A field of the class reaches it, or a field
   of another nested type the layout reaches. The header writes the layout of each one, so
   its fields follow the export rule. A nested type the layout does not
   reach stays out of C, as a library file carries it only when a field
   reaches it. */
static void check_nested_fields(struct checker *c, const struct item *cls,
                                const struct type *t, struct ptr_set *seen)
{
    char what[160];
    size_t i;

    for (i = 0; i < t->field_count; i++) {
        const struct struct_field *f = &t->fields[i];
        const struct type *inner = held_nested(f->type, cls);
        size_t j;
        if (inner == NULL || !ptr_set_add(seen, inner) ||
            inner->kind == TYPE_ENUM) {
            continue;
        }
        for (j = 0; j < inner->field_count; j++) {
            const struct struct_field *g = &inner->fields[j];
            if (g->form == FIELD_BASE || g->form == FIELD_TABLE) {
                continue;
            }
            text_format(what, sizeof what,
                        "the field `%.*s` of `%.*s`, which export class "
                        "`%.*s` holds,",
                        (int)g->name.length, g->name.text,
                        (int)inner->name.length, inner->name.text,
                        (int)cls->name.length, cls->name.text);
            check_c_type(c, g->pos, what, g->type, true, cls);
        }
        check_nested_fields(c, cls, inner, seen);
    }
}

/* DESIGN: an export item has a C symbol and appears in a C header. Its
   types have a C representation, its name is not main, and no other module
   exports the same name. */
void sema_check_export(struct checker *c, struct item *it)
{
    const struct type *t = it->symbol->type;
    char what[160];
    size_t i;
    size_t j;

    switch (it->kind) {
    case ITEM_FN:
        if (sema_name_is(&it->name, "main")) {
            sema_error_at(c, it->name_pos,
                          "`main` is the entry of the program and "
                          "cannot be exported");
            return;
        }
        for (i = 0; i < it->param_count && t->kind == TYPE_FN; i++) {
            text_format(what, sizeof what,
                        "the parameter `%.*s` of export fn "
                        "`%.*s`", (int)it->params[i].name.length,
                        it->params[i].name.text, (int)it->name.length,
                        it->name.text);
            check_c_type(c, it->params[i].pos, what, t->params[i], false,
                         NULL);
        }
        if (it->result != NULL && t->kind == TYPE_FN &&
            t->result->kind != TYPE_VOID) {
            text_format(what, sizeof what, "the result of export fn `%.*s`",
                        (int)it->name.length, it->name.text);
            check_c_type(c, it->result->pos, what, t->result, false, NULL);
        }
        for (i = 0; i < c->library_count; i++) {
            const struct interface *lib = c->libraries[i];
            for (j = 0; j < lib->item_count; j++) {
                const struct symbol *other = lib->items[j];
                if (other->exported && other->kind == SYMBOL_FN &&
                    sema_same_name(&other->name, &it->name)) {
                    sema_error_at(c, it->name_pos,
                                  "export fn `%.*s` has the symbol "
                                  "of export fn `%.*s` in module `%s`",
                                  (int)it->name.length, it->name.text,
                                  (int)other->name.length, other->name.text,
                                  lib->module);
                }
            }
        }
        return;
    /* DESIGN: an export class crosses as its layout, its table type and
       one prototype per public function. Every field and every public
       signature therefore follows the export rule, and the base and the
       table pointer are the compiler's own and always cross. A
       `construct` with arguments crosses as `anti_<Class>_construct`
       whatever its level, so its parameters follow the rule as well. */
    case ITEM_CLASS:
        for (i = 0; i < t->field_count; i++) {
            if (t->fields[i].form == FIELD_BASE ||
                t->fields[i].form == FIELD_TABLE) {
                continue;
            }
            text_format(what, sizeof what,
                        "the field `%.*s` of export class `%.*s`",
                        (int)t->fields[i].name.length,
                        t->fields[i].name.text,
                        (int)it->name.length, it->name.text);
            check_c_type(c, t->fields[i].pos, what, t->fields[i].type, true,
                         it);
        }
        if (it->nested_count > 0) {
            struct ptr_set seen = {0};
            check_nested_fields(c, it, t, &seen);
            free((void *)seen.slots);
        }
        for (i = 0; i < it->member_count; i++) {
            const struct item *m = it->members[i];
            const struct type *ft = m->symbol != NULL ? m->symbol->type : NULL;
            bool made = m->has_self && m->param_count > 0 &&
                        sema_name_is(&m->name, "construct");
            if (m->kind != ITEM_FN || (!m->pub && !made) || ft == NULL ||
                ft->kind != TYPE_FN) {
                continue;
            }
            for (j = m->has_self ? 1 : 0; j < ft->param_count; j++) {
                text_format(what, sizeof what,
                            "the parameter %zu of `%.*s.%.*s`",
                            j, (int)it->name.length, it->name.text,
                            (int)m->name.length, m->name.text);
                check_c_type(c, m->name_pos, what, ft->params[j], false,
                             NULL);
            }
            if (ft->result->kind != TYPE_VOID) {
                text_format(what, sizeof what, "the result of `%.*s.%.*s`",
                            (int)it->name.length, it->name.text,
                            (int)m->name.length, m->name.text);
                check_c_type(c, m->name_pos, what, ft->result, false, NULL);
            }
        }
        return;
    case ITEM_STRUCT:
    case ITEM_UNION:
        for (i = 0; i < t->field_count; i++) {
            text_format(what, sizeof what,
                        "the field `%.*s` of export %s `%.*s`",
                        (int)t->fields[i].name.length,
                        t->fields[i].name.text,
                        it->kind == ITEM_UNION ? "union" : "struct",
                        (int)it->name.length, it->name.text);
            check_c_type(c, t->fields[i].pos, what, t->fields[i].type, true,
                         NULL);
        }
        /* DESIGN: the header writes align(N) as _Alignas on the first
           field, which keeps offset 0 on every target. C refuses
           _Alignas on a bitfield. */
        if (it->align != NULL && t->field_count > 0 &&
            (t->fields[0].bits > 0 ||
             type_field_is_unit_break(&t->fields[0]))) {
            sema_error_at(c, t->fields[0].pos,
                          "the first field `%.*s` of export "
                          "%s `%.*s` is a bitfield, and the C header aligns "
                          "the struct on its first field",
                          (int)t->fields[0].name.length, t->fields[0].name.text,
                          it->kind == ITEM_UNION ? "union" : "struct",
                          (int)it->name.length, it->name.text);
        }
        return;
    /* A variant crosses as the struct of its tag and the union of its
       cases, so the fields of every case follow the export rule. */
    case ITEM_VARIANT:
        for (i = 0; i < t->param_count; i++) {
            const struct type *payload = t->params[i];
            for (j = 0; payload != NULL && j < payload->field_count; j++) {
                text_format(what, sizeof what,
                            "the field `%.*s` of case `%.*s` "
                            "of export variant `%.*s`",
                            (int)payload->fields[j].name.length,
                            payload->fields[j].name.text,
                            (int)t->base->fields[i].name.length,
                            t->base->fields[i].name.text,
                            (int)it->name.length, it->name.text);
                check_c_type(c, payload->fields[j].pos, what,
                             payload->fields[j].type, true, NULL);
            }
        }
        return;
    case ITEM_CONST:
        if (!sema_is_error(t) && !type_is_numeric(t) &&
            t->kind != TYPE_BOOL && t->kind != TYPE_STR) {
            sema_error_at(c, it->type->pos,
                          "export const `%.*s` has type `%s`, and "
                          "an export const is a number, a bool or a str",
                          (int)it->name.length, it->name.text, sema_tn(t));
        }
        return;
    default:
        return;
    }
}

/* Whether a stands before b in the source. */
static bool before(const struct item *a, const struct item *b)
{
    return a->pos.line < b->pos.line ||
           (a->pos.line == b->pos.line && a->pos.column < b->pos.column);
}

void sema_interface(const struct module *module, const char *module_name,
                    struct arena *arena, struct interface *out)
{
    size_t i;
    size_t k;

    memset(out, 0, sizeof *out);
    out->module = keep_name(arena, module_name, strlen(module_name));
    out->doc = keep_name(arena, module->doc.length > 0 ? module->doc.text : "",
                         module->doc.length);
    out->package.name = out->module;
    out->package.version = PACKAGE_VERSION_DEFAULT;
    out->package.license = "";
    out->package.license_text = "";
    out->imports = types_alloc_array(arena, module->import_count + 1,
                                     sizeof *out->imports);
    for (i = 0; i < module->import_count; i++) {
        out->imports[i] = keep_name(arena, module->imports[i].module.text,
                                    module->imports[i].module.length);
    }
    out->import_count = module->import_count;
    out->frameworks = types_alloc_array(arena, module->framework_count + 1,
                                        sizeof *out->frameworks);
    for (i = 0; i < module->framework_count; i++) {
        out->frameworks[i] = keep_name(arena, module->frameworks[i].name.text,
                                       module->frameworks[i].name.length);
    }
    out->framework_count = module->framework_count;
    out->linux_libraries = types_alloc_array(
        arena, module->linux_library_count + 1, sizeof *out->linux_libraries);
    for (i = 0; i < module->linux_library_count; i++) {
        out->linux_libraries[i] =
            keep_name(arena, module->linux_libraries[i].name.text,
                      module->linux_libraries[i].name.length);
    }
    out->linux_library_count = module->linux_library_count;
    out->items = types_alloc_array(
        arena, module->item_count + module->stripped_count + 1,
        sizeof *out->items);
    out->generics = types_alloc_array(arena, module->stripped_count + 1,
                                      sizeof *out->generics);
    for (i = 0; i < module->stripped_count; i++) {
        struct item *it = module->stripped[i];
        if (it->type_param_count > 0 && it->outer == NULL &&
            it->symbol != NULL &&
            (it->kind == ITEM_FN || it->kind == ITEM_STRUCT ||
             it->kind == ITEM_CLASS || it->kind == ITEM_VARIANT)) {
            out->generics[out->generic_count++] = it;
        }
    }
    /* The items the checker kept and those it took out, merged in the
       order of the source. */
    for (i = 0, k = 0; i < module->item_count || k < module->stripped_count;) {
        const struct item *it;
        struct symbol *sym;
        bool take_stripped =
            i == module->item_count ||
            (k < module->stripped_count &&
             before(module->stripped[k], module->items[i]));
        it = take_stripped ? module->stripped[k++] : module->items[i++];
        if (take_stripped &&
            (it->outer != NULL || it->symbol == NULL ||
             (it->kind == ITEM_TYPE && sema_is_error(it->symbol->type)))) {
            continue;
        }
        /* DESIGN: an `internal` item goes into the interface marked
           as internal. A module of the same package may then import it,
           and a module of another package may not. */
        if ((!it->pub && it->vis != VIS_INTERNAL) || it->symbol == NULL) {
            continue;
        }
        sym = arena_alloc(arena, sizeof *sym);
        *sym = *it->symbol;
        sym->internal = it->vis == VIS_INTERNAL;
        /* DESIGN: an interface keeps the parameter names of a function,
           which the generated header and anti doc print. */
        if (it->kind == ITEM_FN || it->kind == ITEM_EXTERN_FN) {
            /* A `may fail` function has one parameter more than the
               declaration wrote, the out pointer the ABI names `out`. */
            size_t total = sym->type != NULL &&
                                   sym->type->kind == TYPE_FN &&
                                   sym->type->param_count > it->param_count
                               ? sym->type->param_count
                               : it->param_count;
            struct name *names =
                types_alloc_array(arena, total + 1, sizeof *names);
            size_t j;
            for (j = 0; j < it->param_count && j < total; j++) {
                names[j].text = keep_name(arena, it->params[j].name.text,
                                          it->params[j].name.length);
                names[j].length = it->params[j].name.length;
            }
            for (; j < total; j++) {
                names[j].text = keep_name(arena, "out", 3);
                names[j].length = 3;
            }
            sym->params = names;
        }
        /* A generic function keeps its declaration, which the section
           of the generics writes. */
        sym->item = it->kind == ITEM_FN && it->type_param_count > 0
                        ? (struct item *)it
                        : NULL;
        sym->is_operator = it->kind == ITEM_FN && it->is_operator;
        sym->alias = it->kind == ITEM_TYPE;
        sym->home = out;
        out->items[out->item_count++] = sym;
    }
}

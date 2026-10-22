/* The passes that declare the items of a module, which sema_check of
   sema.c runs in order: the root, the imports and the direct imports,
   the items under their names, the bases, the values of the enums and
   the cases of the variants, the constants of the bodies, the fields,
   the types of the functions, the members of every body, and the
   defaults and the `own` parameters of the signatures. It checks the
   `provides` and `link` lines and the declaration of a simd struct,
   which stand at module level beside the items. */

#include <stdint.h>
#include <string.h>

#include "cpu.h"
#include "sema_checker.h"

/* Whether lib imports module, directly or through its own imports.
   searched holds the libraries met, none of which imports module, since
   the search ends at the first that does. Each library is searched once,
   so libraries that import each other end the search as well. The
   library count bounds how deep it goes. */
static bool imports_module(const struct checker *c,
                           const struct interface *lib,
                           const struct name *module,
                           struct ptr_set *searched)
{
    size_t i;

    if (!ptr_set_add(searched, lib)) {
        return false;
    }
    for (i = 0; i < lib->import_count; i++) {
        struct name imported;
        const struct interface *next;
        imported.text = lib->imports[i];
        imported.length = strlen(lib->imports[i]);
        if (types_same_name(&imported, module)) {
            return true;
        }
        next = sema_find_library(c, &imported);
        if (next != NULL && imports_module(c, next, module, searched)) {
            return true;
        }
    }
    return false;
}

static bool depends_on(const struct checker *c, const struct interface *lib,
                       const struct name *module)
{
    struct ptr_set searched;
    bool depends;

    memset(&searched, 0, sizeof searched);
    depends = imports_module(c, lib, module, &searched);
    ptr_set_free(&searched);
    return depends;
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
    if (!types_is_integer(t) || types_is_target_sized(t)) {
        sema_error_at(c, field->type->pos,
                      "a bitfield has a sized integer type, "
                      "found `%s`", sema_tn(t));
        return 0;
    }
    if (sema_refuse_runtime_parts(c, e) ||
        !sema_require(c, e, sema_check_expr(c, e, sema_builtin(c, TYPE_I64)),
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
        v.as.integer > (uint64_t)types_bits(t)) {
        sema_error_at(c, e->pos, "a bitfield of `%s` has 1 to %d bits",
                      sema_tn(t),
                      types_bits(t));
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
static void check_simd_struct(struct checker *c, const struct item *it)
{
    struct type *t = it->symbol->type;
    const struct type *lane;
    uint64_t bytes;
    size_t i;

    lane = t->fields[0].type;
    if (sema_is_error(lane)) {
        return;
    }
    /* DESIGN: a lane is a number an operator of the processor takes, or
       the `bool` of a mask. A `char` has four bytes of a fixed width and
       no arithmetic, so R062 of "Rules of 2026-10-13" refuses it. */
    if (sema_simd_lane_refused(lane)) {
        sema_error_at(c, t->fields[0].pos, "a lane of a `simd struct` is an "
                      "integer of a fixed width, a float or `bool`, not `%s`",
                      sema_tn(lane));
        return;
    }
    for (i = 0; i < t->field_count; i++) {
        if (t->fields[i].bits != 0 ||
            types_field_is_unit_break(&t->fields[i])) {
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
    bytes = types_simd_bytes(t);
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

/* The largest N of align(N). */
#define ALIGN_LARGEST 4096

/* The N of align(N): a constant power of two up to ALIGN_LARGEST, or 0
   after an error. */
static uint64_t alignment(struct checker *c, struct expr *e)
{
    struct const_value v;

    if (sema_refuse_runtime_parts(c, e) ||
        !sema_require(c, e, sema_check_expr(c, e, sema_builtin(c, TYPE_I64)),
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
    /* R051 of "Answers of 2026-10-18": the largest alignment is the
       size of a page. */
    if (v.as.integer > ALIGN_LARGEST) {
        sema_error_at(c, e->pos, "an alignment is at most %d, found %lld",
                      ALIGN_LARGEST, (long long)v.as.integer);
        return 0;
    }
    return v.as.integer;
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
    /* DESIGN: R112 of "Rules of 2026-10-13". The compiler writes `get`,
       so the class declares none. A `get` its base declares would stand
       in place of the one written, and is refused at the class. */
    for (i = 0; i < it->member_count; i++) {
        if (types_same_name(&it->members[i]->name, &name)) {
            sema_error_at(c, it->members[i]->name_pos,
                          "`%.*s` is a singleton, and the compiler writes "
                          "its `get`", (int)it->name.length, it->name.text);
            return;
        }
    }
    if (sema_find_member(t, &name) != NULL) {
        sema_error_at(c, it->name_pos,
                      "`%.*s` is a singleton, and the compiler writes "
                      "its `get`", (int)it->name.length, it->name.text);
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

/* DESIGN: anti.lang.Object declares seven public functions whose bodies
   live in the runtime, and nine hooks with empty bodies after them. The
   checker builds one item per function, so `v.type_name()` resolves like
   any inherited call and lowering finds the runtime symbol behind it.
   The list is built once per session, from sema_root_fns, which the
   tables of sema_class.c read as well. */
void sema_declare_root(struct checker *c)
{
    const struct root_fn *root = sema_root_fns;
    struct type *object = types_object(c->types);
    struct type *error = sema_lang_error_or_null(c);
    struct item **members;
    size_t i;
    int k;

    if (object->member_count > 0) {
        return;
    }
    members = types_alloc_array(c->arena, SEMA_ROOT_COUNT, sizeof *members);
    for (i = 0; i < SEMA_ROOT_COUNT; i++) {
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
        sym->type = types_fn(c->types, params, root[i].params + 1,
                             sema_builtin(c, root[i].result));
        members[i] = it;
    }
    object->members = members;
    object->member_count = SEMA_ROOT_COUNT;
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
    struct context sized;
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
            if (types_same_name(&values[k].name, &one->name)) {
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
            sema_enter_sized(c, &sized);
            fields[j].type = sema_resolve_type(c, one->fields[j].type);
            sema_leave(c, &sized);
            for (k = 0; k < j; k++) {
                if (types_same_name(&fields[k].name, &fields[j].name)) {
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

/* DESIGN: `own` on a parameter says the function takes over the value
   it is given, of any type. A local passed there moves, and the caller
   names it no more. A value that owns no memory moves as a copy of its
   bytes. One that does keeps one owner. The caller tears it down no
   more, and the function tears it down at every exit unless it moves
   on. */
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
void sema_declare_import(struct checker *c, const struct import *imp)
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
    if (types_same_name(module, &c->module_name)) {
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
    if (depends_on(c, lib, &c->module_name)) {
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

/* DESIGN: a direct import puts each listed item of the module into the
   module scope under its own name, as `import anti.text.{Builder}` does
   for `Builder`. The entry holds the symbol of the library, the one `text.Builder`
   resolves to, so a use reads the same item either way. The import
   declares the name of the module as a plain one does. The listed names
   go in after the items of the module, so a clash with an item, with the
   name of a module or with a name of another list is found here. It is
   reported at the listed name and names both. A name the compiler
   declares, such as `Object`, stays behind the entry, as it stays
   behind an item of the module. */
void sema_declare_direct_names(struct checker *c, const struct import *imp)
{
    const struct interface *lib = sema_find_library(c, &imp->module);
    size_t i;

    if (lib == NULL) {
        return;
    }
    for (i = 0; i < imp->name_count; i++) {
        const struct import_name *n = &imp->names[i];
        struct symbol *item = sema_library_item(c, lib, &n->name);
        const struct symbol *held;
        if (item == NULL) {
            sema_error_at(c, n->pos, "`%.*s` has no public item `%.*s`",
                          (int)imp->module.length, imp->module.text,
                          (int)n->name.length, n->name.text);
            continue;
        }
        held = sema_scope_find_local(&c->module_scope, &n->name);
        if (held == NULL) {
            sema_scope_put(&c->module_scope, &n->name, item);
        } else if (held == item) {
            sema_error_at(c, n->pos, "`%.*s` of `%.*s` is listed twice",
                          (int)n->name.length, n->name.text,
                          (int)imp->module.length, imp->module.text);
        } else if (held->kind == SYMBOL_MODULE) {
            sema_error_at(c, n->pos, "`%.*s` of `%.*s` clashes with the "
                          "module `%.*s` imported on line %d",
                          (int)n->name.length, n->name.text,
                          (int)imp->module.length, imp->module.text,
                          (int)n->name.length, n->name.text, held->pos.line);
        } else if (sema_direct_item(held)) {
            sema_error_at(c, n->pos, "`%.*s` of `%.*s` clashes with `%.*s` "
                          "of `%s`", (int)n->name.length, n->name.text,
                          (int)imp->module.length, imp->module.text,
                          (int)n->name.length, n->name.text,
                          held->home->module);
        } else {
            sema_error_at(c, n->pos, "`%.*s` of `%.*s` clashes with `%.*s` "
                          "declared on line %d", (int)n->name.length,
                          n->name.text, (int)imp->module.length,
                          imp->module.text, (int)n->name.length,
                          n->name.text, held->pos.line);
        }
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
                   types_same_name(&a->qualifier, &it->name);
    bool b_plain = b->qualifier.length == 0 ||
                   types_same_name(&b->qualifier, &it->name);

    if (a->kind != ITEM_FN || b->kind != ITEM_FN || (a_plain && b_plain)) {
        return true;
    }
    return !a_plain && !b_plain &&
           types_same_name(&a->qualifier, &b->qualifier);
}

/* A framework is named once, and its name is one name of Apple's SDK:
   no path and no empty text, since it becomes -framework <name>. A
   library of `link linux` follows the same rules, since it becomes
   -l<name>. */
void sema_check_link_names(struct checker *c, const struct link_name *names,
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
void sema_check_provides(struct checker *c, struct module *module)
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

struct name sema_shared_name(struct arena *arena, const struct name *op,
                             const struct name *type)
{
    char *text = arena_alloc(arena, op->length + type->length + 2);
    struct name out;

    memcpy(text, op->text, op->length);
    text[op->length] = ':';
    memcpy(text + op->length + 1, type->text, type->length);
    text[op->length + 1 + type->length] = '\0';
    out.text = text;
    out.length = op->length + 1 + type->length;
    return out;
}

static bool module_operator(const struct item *it)
{
    return it->kind == ITEM_FN && it->is_operator && it->owner == NULL;
}

/* The name of the type the first parameter of it names, through a `*`,
   or an empty name. A generic type gives its own name without the
   arguments. */
static struct name first_type_name(const struct item *it)
{
    struct name none = {NULL, 0};
    const struct type_expr *t;

    if (it->param_count == 0) {
        return none;
    }
    t = it->params[0].type;
    while (t != NULL && t->kind == TYPEX_POINTER) {
        t = t->element;
    }
    return t != NULL && t->kind == TYPEX_NAMED ? t->name : none;
}

/* Whether another module-level `operator fn` of the module has the name
   of it. */
static bool shares_name(const struct module *module, const struct item *it)
{
    size_t i;

    for (i = 0; i < module->item_count; i++) {
        const struct item *other = module->items[i];
        if (other != it && module_operator(other) &&
            types_same_name(&other->name, &it->name)) {
            return true;
        }
    }
    return false;
}

/* The name the item it declares its symbol under: its own, or the shared
   name of an `operator fn` whose name another one of the module has. */
static struct name declared_name(struct checker *c, struct item *it)
{
    struct name type;

    if (!module_operator(it) || !shares_name(c->module, it)) {
        return it->name;
    }
    type = first_type_name(it);
    if (type.length == 0) {
        return it->name;
    }
    it->overloaded = true;
    return sema_shared_name(c->arena, &it->name, &type);
}

/* DESIGN: `export` before an `operator fn` is refused, at module level
   and in a class body, by R129 of "Rules of 2026-10-13". C calls no
   operator, and the header writes none. A public function of an export
   class still has its C symbol. */
static void refuse_exported_operator(struct checker *c, const struct item *it)
{
    sema_error_at(c, it->name_pos, "`operator fn %.*s` is not exported, "
                  "since C has no operators", (int)it->name.length,
                  it->name.text);
}

/* Declare every item first, so each can be used before its
   declaration. */
void sema_declare_items(struct checker *c)
{
    const struct module *module = c->module;
    size_t i;

    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        struct name name = declared_name(c, it);
        if (it->overloaded &&
            sema_scope_find_local(c->ctx.scope, &name) != NULL) {
            struct name type = first_type_name(it);
            sema_error_at(c, it->name_pos, "`operator fn %.*s` is already "
                          "declared for `%.*s`", (int)it->name.length,
                          it->name.text, (int)type.length, type.text);
            continue;
        }
        if (it->is_operator && it->exported) {
            refuse_exported_operator(c, it);
        }
        it->symbol = sema_declare(c, item_symbol_kind(it->kind), &name,
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
            it->symbol->type->is_singleton = it->is_singleton;
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
            /* The values of an enum are integers, as the reader of a
               library file requires. A base that is no integer, or the
               enum itself, gave lowering a NULL or a struct to read (S10
               of the audit). The error keeps c_int in its place. */
            if (!types_is_integer(base)) {
                if (!sema_is_error(base)) {
                    sema_error_at(c, it->base->pos, "the underlying type of an "
                                  "enum is an integer type, found `%s`",
                                  sema_tn(base));
                }
                base = types_builtin(c->types, TYPE_I32);
            }
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
    /* DESIGN: R111 of "Rules of 2026-10-13". No class inherits a
       singleton. A class below would make more objects of it, and a
       singleton below would be a second instance of its base. */
    if (base_type->is_singleton) {
        sema_error_at(c, it->base_pos,
                      "`%.*s` is a singleton, and no class inherits it",
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

void sema_resolve_bases(struct checker *c)
{
    const struct module *module = c->module;
    size_t i;

    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        if (it->kind == ITEM_CLASS && it->symbol != NULL) {
            struct context outer;
            sema_enter_within(c, it, &outer);
            resolve_base(c, it);
            sema_leave(c, &outer);
        }
    }
}

/* Whether the number after before, the bits of a value of base, is
   still a value of base. */
static bool follows_in(const struct type *base, uint64_t before)
{
    int bits = types_bits(base);
    uint64_t top;

    if (types_is_signed(base)) {
        top = bits < 64 ? ((uint64_t)1 << (bits - 1)) - 1 : INT64_MAX;
        return sema_signed_bits(before) < (int64_t)top;
    }
    top = bits < 64 ? ((uint64_t)1 << bits) - 1 : UINT64_MAX;
    return before < top;
}

/* Whether the name e is the value of the enum checked now whose
   expression holds it, or a value after it, which it refuses. */
bool sema_later_enum_value(struct checker *c, const struct expr *e)
{
    const struct item *it = c->enum_item;
    size_t k;

    for (k = c->enum_value; it != NULL && k < it->param_count; k++) {
        if (types_same_name(&it->params[k].name, &e->as.name)) {
            sema_error_at(c, e->pos, "a value of `%.*s` names the values "
                          "before it alone, and `%.*s` is not one",
                          (int)it->name.length, it->name.text,
                          (int)e->as.name.length, e->as.name.text);
            return true;
        }
    }
    return false;
}

/* DESIGN: R054 of "Answers of 2026-10-18". The expression of a value of
   an enum names the values before it by their bare names, as
   `All = Read | Write`. Each is a constant of the base of the enum that
   holds its number, so the operators of integers take it, and a later
   value is refused, so no cycle forms. The names live in a scope of the
   expression alone. */
static void check_enum_value(struct checker *c, struct item *it,
                             const struct struct_field *values, size_t j,
                             struct const_value *v, bool *folded)
{
    struct type *base = it->symbol->type->base;
    struct expr *e = it->params[j].value;
    struct scope earlier;
    size_t k;

    memset(&earlier, 0, sizeof earlier);
    sema_enter_scope(c, &earlier);
    for (k = 0; k < j; k++) {
        struct symbol *sym;
        struct const_value *number;
        if (sema_scope_find_local(&earlier, &values[k].name) != NULL) {
            continue;
        }
        sym = sema_declare(c, SYMBOL_CONST, &values[k].name, values[k].pos,
                           "`%.*s` is already declared in this block");
        number = arena_alloc(c->arena, sizeof *number);
        memset(number, 0, sizeof *number);
        number->kind = CONST_INT;
        number->type = base;
        number->as.integer = values[k].number;
        sym->type = base;
        sym->value = number;
        sym->state = EVAL_DONE;
    }
    c->enum_item = it;
    c->enum_value = j;
    *folded = !sema_refuse_runtime_parts(c, e) &&
              sema_require(c, e, sema_check_expr(c, e, base), base) &&
              sema_eval_const(c, e, v);
    c->enum_item = NULL;
    sema_leave_scope(c, &earlier);
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
        values[j].name = it->params[j].name;
        values[j].pos = it->params[j].pos;
        values[j].doc = it->params[j].doc;
        values[j].type = it->symbol->type;
        values[j].value = it->params[j].value;
        /* DESIGN: a value without `=` follows the one before it and
           the first is 0, as C numbers an enumerator. It must fit the
           base, as a value written with `=` must. */
        values[j].number = j == 0 ? 0 : values[j - 1].number + 1;
        if (j > 0 && it->params[j].value == NULL &&
            !follows_in(it->symbol->type->base, values[j - 1].number)) {
            sema_error_at(c, values[j].pos,
                          "`%.*s` follows `%.*s` and does not fit `%s`",
                          (int)values[j].name.length, values[j].name.text,
                          (int)values[j - 1].name.length,
                          values[j - 1].name.text,
                          sema_tn(it->symbol->type->base));
        }
        if (it->params[j].value != NULL) {
            struct const_value v;
            bool folded;
            /* DESIGN: every value of an enum is a number the checker
               knows, since `switch` covers the values and the C header
               writes them. A value computed from `size_of` differs per
               target and is refused as the N of `align(N)` is, by T021
               of `docs/audit/tests-findings.md`. */
            check_enum_value(c, it, values, j, &v, &folded);
            if (folded) {
                if (v.kind == CONST_INT) {
                    values[j].number = v.as.integer;
                } else if (v.kind == CONST_SYMBOLIC) {
                    sema_error_at(c, it->params[j].value->pos,
                                  "a value of an enum is a constant, not a "
                                  "value computed from `size_of`");
                }
            }
        }
        for (k = 0; k < j; k++) {
            if (types_same_name(&values[k].name, &values[j].name)) {
                sema_error_at(c, values[j].pos, "enum `%.*s` has two values "
                              "named `%.*s`", (int)it->name.length,
                              it->name.text, (int)values[j].name.length,
                              values[j].name.text);
            }
        }
    }
    types_set_fields(c->types, it->symbol->type, values, it->param_count);
}

void sema_declare_enums_and_variants(struct checker *c)
{
    const struct module *module = c->module;
    size_t i;

    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        if (it->symbol != NULL && it->kind == ITEM_ENUM) {
            struct context outer;
            sema_enter_within(c, it, &outer);
            declare_enum_values(c, it);
            sema_leave(c, &outer);
        }
    }
    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        if (it->symbol != NULL && it->kind == ITEM_VARIANT) {
            struct context outer;
            sema_enter_within(c, it, &outer);
            declare_cases(c, it);
            sema_leave(c, &outer);
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
        !types_is_nullable(ft) || ft->bound) {
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
    struct context sized;
    size_t j;

    for (j = 0; j < it->param_count; j++) {
        size_t k;
        fields[j].name = it->params[j].name;
        fields[j].pos = it->params[j].pos;
        fields[j].name_pos = it->params[j].name_pos.line != 0
                                 ? it->params[j].name_pos
                                 : it->params[j].pos;
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
        sema_enter_sized(c, &sized);
        fields[j].type = sema_resolve_type(c, it->params[j].type);
        sema_leave(c, &sized);
        /* An `own` field of function type holds `own fn`, which frees
           its snapshot with the object. */
        if (fields[j].owned && fields[j].type->kind == TYPE_FN &&
            !fields[j].type->bound) {
            fields[j].type = types_fn_owned(c->types, fields[j].type);
        }
        if (it->params[j].bits != NULL ||
            types_field_is_unit_break(&fields[j])) {
            fields[j].bits = bitfield_width(c, &it->params[j],
                                            fields[j].type);
        }
        if (it->kind == ITEM_UNION && types_field_is_unit_break(&fields[j])) {
            sema_error_at(c, fields[j].pos, "a union holds no zero-width "
                          "bitfield");
        }
        for (k = 0; k < j && !types_field_is_unit_break(&fields[j]); k++) {
            if (types_same_name(&fields[k].name, &fields[j].name)) {
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
       nested whole, so the C rules of struct layout place it at
       offset 0 and the class's own fields after it. */
    base_fields = it->kind == ITEM_CLASS ? 1 : 0;
    hidden = it->kind == ITEM_CLASS && sema_needs_hidden_lock(it) ? 1 : 0;
    fields = types_alloc_array(c->arena,
                               it->param_count + base_fields + hidden,
                               sizeof *fields);
    if (base_fields != 0) {
        static const char super_text[] = "super";
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
    for (j = 0; j < it->param_count; j++) {
        if (fields[j].atomic) {
            sema_refuse_atomic_field(c, fields[j].name_pos, fields[j].type);
        }
    }
    /* `mutable` is a contextual word of a singleton alone, by decision 27
       of "Decisions of 2026-10-11". */
    for (j = 0; j < it->param_count; j++) {
        if (fields[j].writable && !it->is_singleton) {
            sema_error_at(c, it->params[j].pos, "`mutable` marks a field of "
                          "a singleton, and `%.*s` is none",
                          (int)it->name.length, it->name.text);
        }
    }
    /* A default is checked against the type of its field, so the
       value that lowering writes is complete and typed. It is a
       constant expression, and its value is kept for the library
       file. It takes a length computed from `size_of`, as the type of
       its field does, by R043 of "Rules of 2026-10-13". */
    for (j = 0; j < it->param_count; j++) {
        struct const_value *v;
        struct context sized;
        if (it->params[j].value == NULL ||
            sema_refuse_runtime_parts(c, it->params[j].value)) {
            continue;
        }
        sema_enter_sized(c, &sized);
        if (sema_require(c, it->params[j].value,
                         sema_check_expr(c, it->params[j].value,
                                         fields[j].type),
                         fields[j].type)) {
            v = arena_alloc(c->arena, sizeof *v);
            if (sema_eval_const(c, it->params[j].value, v) &&
                !sema_bitfield_constant(c, it->params[j].value,
                                        &fields[j])) {
                fields[j].constant = v;
            }
        }
        sema_leave(c, &sized);
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

/* Refuse a struct, a union, a class or a variant t that contains
   itself or nests deeper than TYPES_NEST_MAX levels, at pos. The field
   that closes it takes the type error. */
void sema_check_nesting(struct checker *c, struct type *t, struct pos pos)
{
    const char *kind = t->kind == TYPE_VARIANT ? "variant"
                       : t->kind == TYPE_CLASS ? "class"
                       : t->is_union           ? "union"
                                               : "struct";
    enum nest_result nest = types_nest(t, sema_builtin(c, TYPE_ERROR));

    if (nest == NEST_CYCLE) {
        sema_error_at(c, pos, "%s `%s` contains itself", kind, sema_tn(t));
    } else if (nest == NEST_DEEP) {
        sema_error_at(c, pos, "the values of %s `%s` nest deeper than %d "
                      "levels", kind, sema_tn(t), TYPES_NEST_MAX);
    }
}

/* The fields of every struct, union and class, then the refusal of a
   type that contains itself or nests too deep. */
void sema_declare_all_fields(struct checker *c)
{
    const struct module *module = c->module;
    size_t i;

    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        if (it->symbol != NULL &&
            (it->kind == ITEM_STRUCT || it->kind == ITEM_UNION ||
             it->kind == ITEM_CLASS)) {
            struct context outer;
            sema_enter_within(c, it, &outer);
            declare_fields(c, it);
            sema_leave(c, &outer);
        }
    }
    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        if (it->symbol != NULL &&
            (it->kind == ITEM_STRUCT || it->kind == ITEM_UNION ||
             it->kind == ITEM_CLASS || it->kind == ITEM_VARIANT)) {
            sema_check_nesting(c, it->symbol->type, it->name_pos);
        }
    }
}

void sema_declare_function_types(struct checker *c)
{
    const struct module *module = c->module;
    size_t i;

    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        if (it->symbol != NULL &&
            (it->kind == ITEM_FN || it->kind == ITEM_EXTERN_FN)) {
            it->symbol->type = sema_function_type(c, it);
        }
        /* The operators of a simd struct are built in, and an `operator
           fn` beside them would never be called. */
        if (it->symbol != NULL && it->kind == ITEM_FN && it->is_operator &&
            it->symbol->type->kind == TYPE_FN &&
            it->symbol->type->param_count > 0 &&
            types_is_simd(sema_struct_of(it->symbol->type->params[0]))) {
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

    /* DESIGN: a qualifier names a type as the class body writes it, so a
       type nested in a class is `Vehicle` there. Each qualifier takes the
       full name of the type it finds, `Garage.Vehicle`, which every later
       comparison with the names of the chain reads, by T084 of
       docs/audit/tests-findings.md. */
    for (j = 0; j < it->member_count; j++) {
        struct item *m = it->members[j];
        struct symbol *named;
        if (m->qualifier.length == 0) {
            continue;
        }
        named = sema_module_find(c, &m->qualifier);
        if (named != NULL && named->kind == SYMBOL_STRUCT &&
            named->type != NULL && named->type->kind == TYPE_CLASS &&
            !types_same_name(&named->type->name, &m->qualifier)) {
            m->qualifier = named->type->name;
        }
    }
    for (j = 0; j < it->member_count; j++) {
        struct item *m = it->members[j];
        struct symbol *sym = m->symbol != NULL
                                 ? m->symbol
                                 : arena_alloc(c->arena, sizeof *sym);
        struct name text;
        size_t k;
        /* A second `construct` has the message of check_one_construct,
           which names the static function that stands for another
           maker. */
        for (k = 0; k < j && !sema_name_is(&m->name, "construct"); k++) {
            if (types_same_name(&it->members[k]->name, &m->name) &&
                same_qualifier(it, it->members[k], m)) {
                sema_error_at(c, m->name_pos, "`%.*s` declares `%.*s` twice",
                              (int)it->name.length, it->name.text,
                              (int)m->name.length, m->name.text);
            }
        }
        /* DESIGN: `c.size` names one member, the field or the function
           bound to `c`, by "Functions" in docs/anti-object-model.md. A
           field and a function of one name in one body could be neither
           for certain, so the function declares the name twice, by T185
           of docs/audit/tests-findings.md. */
        for (k = 0; (it->kind == ITEM_CLASS || it->kind == ITEM_STRUCT) &&
                    m->kind == ITEM_FN && k < it->param_count;
             k++) {
            if (types_same_name(&it->params[k].name, &m->name)) {
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
        if (m->is_operator && m->exported) {
            refuse_exported_operator(c, m);
        } else if (it->exported && m->kind == ITEM_FN && m->pub) {
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
            sym->type = sema_function_type(c, m);
        }
    }
}

/* DESIGN: the default of a field is checked with the fields, before
   the members of a body are declared, and it may name a constant of a
   class, `Name.NAME`, by T019 of docs/audit/tests-findings.md. The
   symbol of every constant of a body is therefore made first, and
   declare_members fills in the rest of it. A constant is folded where
   it is named, so the symbol needs nothing more. */
void sema_declare_body_constants(struct checker *c)
{
    const struct module *module = c->module;
    size_t i;
    size_t j;

    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        if (it->symbol == NULL || it->symbol->type == NULL) {
            continue;
        }
        for (j = 0; j < it->member_count; j++) {
            struct item *m = it->members[j];
            struct symbol *sym;
            if (m->kind != ITEM_CONST || m->is_static) {
                continue;
            }
            sym = arena_alloc(c->arena, sizeof *sym);
            sym->kind = SYMBOL_CONST;
            sym->name = types_member_symbol(c->arena, &it->name, m);
            sym->pos = m->name_pos;
            sym->item = m;
            m->symbol = sym;
        }
    }
}

/* Whether it has type parameters, or is a type nested in a body that
   has them. */
static bool in_generic_body(const struct item *it)
{
    for (; it != NULL; it = it->outer) {
        if (it->type_param_count > 0) {
            return true;
        }
    }
    return false;
}

/* The members of every body, the export mark of every type and the
   constants of the module. */
void sema_declare_all_members(struct checker *c)
{
    const struct module *module = c->module;
    size_t i;

    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        if (it->symbol != NULL && it->symbol->type != NULL) {
            struct context outer;
            sema_enter_within(c, it, &outer);
            declare_members(c, it);
            sema_leave(c, &outer);
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
    /* DESIGN: a singleton has one instance, which `Config.get()` makes
       on the first call. The program never allocates one, so the checker
       declares `get` itself, before any body or constant names it. A
       constant that calls it is then refused as a call, by T077 of
       docs/audit/tests-findings.md. */
    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        if (it->kind == ITEM_CLASS && it->is_singleton &&
            it->symbol != NULL && it->symbol->type != NULL) {
            declare_get(c, it);
        }
    }
    c->fns_typed = true;
    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        if (it->symbol != NULL && it->kind == ITEM_CONST) {
            sema_const_symbol(c, it->symbol, it->name_pos);
        }
    }
    /* DESIGN: a static field and a constant of a body hold values of
       the program text, as a constant of the module does. They are
       checked whether or not code reads them, by T057 of
       docs/audit/tests-findings.md, so a default that is no constant
       expression is refused in a field that nothing reads. A body of a
       generic type checks them in each copy. */
    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        size_t j;
        if (it->symbol == NULL || it->symbol->type == NULL ||
            in_generic_body(it)) {
            continue;
        }
        for (j = 0; j < it->member_count; j++) {
            struct item *m = it->members[j];
            if (m->kind == ITEM_CONST && m->symbol != NULL) {
                sema_const_symbol(c, m->symbol, m->name_pos);
            }
        }
    }
}

/* Every body calls with the defaults and the `own` parameters, so they
   are known before the first body is checked. */
void sema_check_signatures(struct checker *c)
{
    const struct module *module = c->module;
    size_t i;
    size_t j;

    for (i = 0; i < module->item_count; i++) {
        struct item *it = module->items[i];
        struct context outer;
        if (it->kind == ITEM_FN || it->kind == ITEM_EXTERN_FN) {
            check_defaults(c, it);
            check_owned(c, it);
        }
        sema_enter_within(c, it, &outer);
        for (j = 0; it->symbol != NULL && j < it->member_count; j++) {
            if (it->members[j]->kind == ITEM_FN) {
                check_defaults(c, it->members[j]);
                check_owned(c, it->members[j]);
            }
        }
        sema_leave(c, &outer);
    }
}

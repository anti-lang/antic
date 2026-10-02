#include "header.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"

/* The C name of a scalar type. DESIGN: a sized type maps to its <stdint.h>
   name, and int, uint and float to int64_t, uint64_t and double. c_long,
   c_ulong and c_wchar map to long, unsigned long and wchar_t. The other c_
   types are sized types, so c_int is int32_t. */
static const char *scalar_name(const struct type *t)
{
    switch (t->kind) {
    case TYPE_VOID: return "void";
    case TYPE_BOOL: return "bool";
    case TYPE_I8: return "int8_t";
    case TYPE_I16: return "int16_t";
    case TYPE_I32: return "int32_t";
    case TYPE_I64: return "int64_t";
    case TYPE_U8: return "uint8_t";
    case TYPE_U16: return "uint16_t";
    /* DESIGN: C has no half type that every C compiler reads, so an f16
       crosses as its sixteen bits. uint16_t also passes an aggregate of
       them where antic passes it, among the integers. */
    case TYPE_F16: return "uint16_t";
    case TYPE_U32: return "uint32_t";
    case TYPE_U64: return "uint64_t";
    case TYPE_F32: return "float";
    case TYPE_F64: return "double";
    case TYPE_CLONG: return "long";
    case TYPE_CULONG: return "unsigned long";
    case TYPE_CWCHAR: return "wchar_t";
    default: return "void";
    }
}

/* Names that a parameter or a field cannot take in a header for C11 and
   C++17. They are the keywords of both and the macros of the included
   headers. */
static const char *const reserved_names[] = {
    "_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex", "_Generic",
    "_Imaginary", "_Noreturn", "_Static_assert", "_Thread_local", "alignas",
    "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor", "bool",
    "break", "case", "catch", "char", "char16_t", "char32_t", "class", "compl",
    "const", "const_cast", "constexpr", "continue", "decltype", "default",
    "delete", "do", "double", "dynamic_cast", "else", "enum", "explicit",
    "export", "extern", "false", "float", "for", "friend", "goto", "if",
    "inline", "int", "long", "mutable", "namespace", "new", "noexcept", "not",
    "not_eq", "nullptr", "offsetof", "operator", "or", "or_eq", "private",
    "protected", "public", "register", "reinterpret_cast", "restrict",
    "return", "short", "signed", "sizeof", "static", "static_assert",
    "static_cast", "struct", "switch", "template", "this", "thread_local",
    "throw", "true", "try", "typedef", "typeid", "typename", "union",
    "unsigned", "using", "virtual", "void", "volatile", "wchar_t", "while",
    "xor", "xor_eq", "NULL", "ANTI_ALIGNAS"};

/* Whether name is a macro of <stdint.h>: INT8_MAX, UINT64_C, SIZE_MAX and
   the other names of that header that consist of capitals, digits and _. */
static bool stdint_macro(const struct name *name)
{
    static const char *const prefixes[] = {"INT", "UINT", "SIZE_", "PTRDIFF_",
                                           "SIG_ATOMIC_", "WCHAR_", "WINT_"};
    size_t i;

    for (i = 0; i < name->length; i++) {
        char c = name->text[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) {
            return false;
        }
    }
    for (i = 0; i < sizeof prefixes / sizeof prefixes[0]; i++) {
        size_t n = strlen(prefixes[i]);
        if (name->length > n && memcmp(name->text, prefixes[i], n) == 0) {
            return true;
        }
    }
    return false;
}

/* DESIGN: the header keeps the Anti name of a parameter or a field. A
   name that C or C++ reserves gets a trailing _, as default_. */
static void c_name(struct text *out, const struct name *name)
{
    size_t i;
    bool reserved = stdint_macro(name);

    for (i = 0; !reserved && i < sizeof reserved_names / sizeof *reserved_names;
         i++) {
        reserved = strlen(reserved_names[i]) == name->length &&
                   memcmp(reserved_names[i], name->text, name->length) == 0;
    }
    text_appendf(out, "%.*s%s", (int)name->length, name->text,
                 reserved ? "_" : "");
}

/* DESIGN: `anti.lang.Error` crosses to C as `struct anti_Error *`, the
   type the object model gives the generated helpers. C never reads the
   layout, so the header declares the tag and nothing else. */
static const struct item *construct_with_arguments(const struct type *t);

/* Whether a signature names the error class, which the header then
   declares once. A class is asked about the functions of its body, and
   about a `construct` with arguments, which C calls through a helper. */
static bool type_names_error(const struct type *t)
{
    const struct item *made;
    size_t i;

    if (t == NULL) {
        return false;
    }
    if (t->kind == TYPE_CLASS) {
        for (i = 0; i < t->member_count; i++) {
            const struct item *m = t->members[i];
            if (m->kind == ITEM_FN && m->pub && m->symbol != NULL &&
                type_names_error(m->symbol->type)) {
                return true;
            }
        }
        made = t->has_abstract ? NULL : construct_with_arguments(t);
        return made != NULL && type_names_error(made->symbol->type);
    }
    if (t->kind != TYPE_FN) {
        return false;
    }
    if (t->result->kind == TYPE_POINTER &&
        types_is_lang_error(t->result->element)) {
        return true;
    }
    for (i = 0; i < t->param_count; i++) {
        if (t->params[i]->kind == TYPE_POINTER &&
            types_is_lang_error(t->params[i]->element)) {
            return true;
        }
        /* A parameter of a `may fail` function type names the error in
           its ABI form. */
        if (t->params[i]->kind == TYPE_FN && type_names_error(t->params[i])) {
            return true;
        }
    }
    return false;
}

static void c_type_name(struct text *out, const struct type *t);
static void element_c_name(struct text *out, const struct type *t);

/* `tuple` and the name of each element of the tuple t after a `_`. */
static void tuple_elements(struct text *out, const struct type *t)
{
    size_t i;

    text_append(out, "tuple");
    for (i = 0; i < t->param_count; i++) {
        text_append(out, "_");
        element_c_name(out, t->params[i]);
    }
}

/* DESIGN: a tuple has no name of its own, so the header makes one from
   its elements: `(int, str)` becomes `anti_tuple_int_str`. A pointer
   writes `ptr_` before what it points at and a nullable pointer `optr_`,
   an array its length, and a tuple its own elements. A `?T` of a value
   has none either, and is `opt_` before its value: `?int` becomes
   `anti_opt_int`. Two tuples of the same elements are one type, and so
   are two `?T` of one T, so one name stands for one type. A tuple inside
   a name ends with `_end`, so `((int, int), int, int)` and
   `((int, int, int), int)` have two names. A function writes `fn`, its
   parameters, `to` and its result, and ends with `_end` as well:
   `fn_int_to_bool_end`. A named type writes the name C knows it by, the
   one an `export type` gives a copy of a generic. */
static void element_c_name(struct text *out, const struct type *t)
{
    size_t i;

    switch (t->kind) {
    case TYPE_POINTER:
        text_append(out, t->nullable ? "optr_" : "ptr_");
        element_c_name(out, t->element);
        return;
    case TYPE_ARRAY:
        text_appendf(out, "a%" PRIu64 "_", t->length);
        element_c_name(out, t->element);
        return;
    case TYPE_SLICE:
        text_append(out, "slice_");
        element_c_name(out, t->element);
        return;
    case TYPE_FN:
        text_append(out, "fn");
        for (i = 0; i < t->param_count; i++) {
            text_append(out, "_");
            element_c_name(out, t->params[i]);
        }
        text_append(out, "_to_");
        element_c_name(out, t->result);
        text_append(out, "_end");
        return;
    case TYPE_OPTIONAL:
        text_append(out, "opt_");
        element_c_name(out, t->element);
        return;
    case TYPE_TUPLE:
        tuple_elements(out, t);
        text_append(out, "_end");
        return;
    case TYPE_STRUCT:
    case TYPE_CLASS:
    case TYPE_VARIANT:
    case TYPE_ENUM:
        c_type_name(out, t);
        return;
    default:
        types_name(out, t);
        return;
    }
}

/* The name of the struct of the tuple or `?T` t. The outermost tuple
   writes no `_end`. */
static void tuple_c_name(struct text *out, const struct type *t)
{
    text_append(out, "anti_");
    if (t->kind == TYPE_TUPLE) {
        tuple_elements(out, t);
    } else {
        element_c_name(out, t);
    }
}

/* The comment before parameter i of sym when it is `own` or `lent`. The
   function takes over what C passes to an `own` one, as an `own` field is
   freed by its object, and keeps nothing it takes at a `lent` one. */
static const char *owned_note(const struct symbol *sym, size_t i)
{
    if (sym != NULL && sym->type != NULL && sym->type->kind == TYPE_FN &&
        i < sym->type->param_count && types_is_lent(sym->type->params[i])) {
        return "/* lent */ ";
    }
    return sym != NULL && sym->owned != NULL && i < sym->owned_count &&
                   sym->owned[i]
               ? "/* own */ "
               : "";
}

/* DESIGN: the C name of a type is its name. A type nested in a class
   has the full name `PeopleList.Node`, and a dot is no C identifier. The
   header therefore writes `PeopleList_Node`, as target_c_symbol writes
   the symbol of a function of a class. */
static void c_type_name(struct text *out, const struct type *t)
{
    struct name name = types_c_name(t);
    size_t i;

    for (i = 0; i < name.length; i++) {
        text_appendf(out, "%c", name.text[i] == '.' ? '_' : name.text[i]);
    }
}

static void declaration(struct text *out, const struct type *t,
                        const char *name, const struct type *owner);

/* The C function pointer name of the function type t. The form of two
   words takes its context last, as a `void *`. */
static void callback(struct text *out, const struct type *t,
                     const char *name, const struct type *owner)
{
    struct text inner = {0};
    size_t i;

    text_appendf(&inner, "(*%s)(", name);
    for (i = 0; i < t->param_count; i++) {
        struct text param = {0};
        declaration(&param, t->params[i], "", owner);
        text_appendf(&inner, "%s%s", i > 0 ? ", " : "", text_cstr(&param));
        text_free(&param);
    }
    if (t->context) {
        text_append(&inner, t->param_count > 0 ? ", void *" : "void *");
    }
    text_append(&inner, t->param_count == 0 && !t->context ? "void)" : ")");
    declaration(out, t->result, text_cstr(&inner), owner);
    text_free(&inner);
}

/* Append the C declaration of name with type t. owner is the aggregate
   whose definition holds the declaration, which names itself with its
   tag. */
static void declaration(struct text *out, const struct type *t,
                        const char *name, const struct type *owner)
{
    struct text inner = {0};

    switch (t->kind) {
    /* DESIGN: C has one pointer type, so `*T` and `?*T` both cross as
       `T *`. The header marks the one that never holds `none` in a
       comment beside the star, which a reader sees and which costs the
       C compiler nothing. An exported function with a `*T` parameter
       checks nothing at run time, so the comment is the whole of it. */
    case TYPE_POINTER:
        text_append(&inner, t->nullable ? "*" : "* /* non-null */");
        if (name[0] != '\0') {
            text_appendf(&inner, "%s%s", t->nullable ? "" : " ", name);
        }
        declaration(out, t->element, text_cstr(&inner), owner);
        break;
    case TYPE_ARRAY:
        text_appendf(&inner, "%s[%" PRIu64 "]", name, t->length);
        declaration(out, t->element, text_cstr(&inner), owner);
        break;
    /* DESIGN: an Anti function type is a function pointer in C. A
       parameter that does not keep its argument is written the C way, as
       a callback and a `void *` context after it. The callback takes the
       context after its own parameters, which is where antic passes it,
       and a named function passes NULL there. */
    case TYPE_FN:
        callback(out, t, name, owner);
        if (t->context) {
            text_appendf(out, ", void *%s%s", name, name[0] != '\0' ? "_context"
                                                                    : "");
        }
        break;
    case TYPE_STRUCT:
    case TYPE_CLASS:
    case TYPE_VARIANT:
        if (types_is_lang_error(t)) {
            text_append(out, "struct anti_Error");
        } else if (types_is_flags(t)) {
            text_append(out, "struct anti_" LANG_FLAGS);
        } else if (t == owner) {
            text_appendf(out, "%s ", t->is_union ? "union" : "struct");
            c_type_name(out, t);
        } else {
            c_type_name(out, t);
        }
        text_appendf(out, "%s%s", name[0] != '\0' ? " " : "", name);
        break;
    case TYPE_TUPLE:
    case TYPE_OPTIONAL:
        text_append(out, "struct ");
        tuple_c_name(out, t);
        text_appendf(out, "%s%s", name[0] != '\0' ? " " : "", name);
        break;
    /* An enum of C has the width of an int, so a value crosses as the
       underlying integer, and enum_view names the values. */
    case TYPE_ENUM:
        text_appendf(out, "%s%s%s", scalar_name(t->base),
                     name[0] != '\0' ? " " : "", name);
        break;
    default:
        text_append(out, scalar_name(t));
        text_appendf(out, "%s%s", name[0] != '\0' ? " " : "", name);
        break;
    }
    text_free(&inner);
}

/* A doc comment as a C comment at indent, one line for one line of text
   and a gutter of stars otherwise. */
static void doc_comment(struct text *out, const struct doc_text *doc,
                        const char *indent)
{
    const char *p = doc->text;
    const char *end = doc->text + doc->length;

    if (doc->length == 0) {
        return;
    }
    if (memchr(p, '\n', doc->length) == NULL) {
        text_appendf(out, "%s/** %.*s */\n", indent, (int)doc->length, p);
        return;
    }
    text_appendf(out, "%s/**\n", indent);
    while (p < end) {
        const char *nl = memchr(p, '\n', (size_t)(end - p));
        size_t n = nl != NULL ? (size_t)(nl - p) : (size_t)(end - p);
        text_appendf(out, "%s *%s%.*s\n", indent, n > 0 ? " " : "", (int)n, p);
        p += n + 1;
    }
    text_appendf(out, "%s */\n", indent);
}

struct emitted {
    const struct type **items;
    size_t count;
    size_t capacity;
};

static bool was_emitted(const struct emitted *e, const struct type *t)
{
    size_t i;

    for (i = 0; i < e->count; i++) {
        if (e->items[i] == t) {
            return true;
        }
    }
    return false;
}

/* A type is written once. The list grows, because the tuples of a
   signature are as many as its elements and no count of the items
   bounds them. */
static void mark_emitted(struct emitted *e, const struct type *t)
{
    e->items = alloc_grow(e->items, &e->capacity, e->count, sizeof *e->items);
    e->items[e->count++] = t;
}

static void aggregate(struct text *out, const struct symbol *sym,
                      const struct interface *const *ifaces, size_t count,
                      struct emitted *done);
static void class_view(struct text *out, const struct symbol *sym,
                       const struct interface *const *ifaces,
                       size_t iface_count, struct emitted *done);
static void emit_tuples(struct text *out, const struct type *t,
                        const struct interface *const *ifaces, size_t count,
                        struct emitted *done);

/* DESIGN: Flags is a struct of four bools that no module declares. The
   header writes it once, as it writes a tuple, before the first aggregate
   or signature that names it. */
static void flags_view(struct text *out, const struct type *t,
                       struct emitted *done)
{
    size_t i;

    if (was_emitted(done, t)) {
        return;
    }
    mark_emitted(done, t);
    text_append(out, "/* The flags of one arithmetic operation. */\n"
                     "struct anti_" LANG_FLAGS " {\n");
    for (i = 0; i < t->field_count; i++) {
        text_appendf(out, "    bool %.*s;\n", (int)t->fields[i].name.length,
                     t->fields[i].name.text);
    }
    text_append(out, "};\n\n");
}

/* The export aggregate that type t holds by value, and every tuple it
   names, emitted first. */
static void emit_uses(struct text *out, const struct type *t,
                      const struct interface *const *ifaces, size_t count,
                      struct emitted *done)
{
    size_t i;
    size_t j;

    emit_tuples(out, t, ifaces, count, done);
    while (t->kind == TYPE_ARRAY) {
        t = t->element;
    }
    if ((t->kind != TYPE_STRUCT && t->kind != TYPE_VARIANT &&
         t->kind != TYPE_CLASS) ||
        types_is_flags(t) || was_emitted(done, t)) {
        return;
    }
    for (i = 0; i < count; i++) {
        for (j = 0; j < ifaces[i]->item_count; j++) {
            if (ifaces[i]->items[j]->type == t) {
                aggregate(out, ifaces[i]->items[j], ifaces, count, done);
            }
        }
    }
}

/* DESIGN: the header writes one struct per distinct tuple of an
   exported signature, because C has no anonymous struct that two
   translation units agree on. A `?T` of a value is the struct of the
   value and a `bool`, `value` and `has`, one per T. What an element holds
   by value is written before it, so the definition stands complete. */
/* DESIGN: an owning struct, tuple or variant keeps its layout in the
   header, and a comment says it owns what its parts own. Anti tears it
   down once and copies it with `dup`, so C code that copies it by value
   and keeps both copies frees what they own twice. */
static void owning_note(struct text *out, const struct type *t)
{
    if ((t->kind == TYPE_STRUCT || t->kind == TYPE_TUPLE ||
         t->kind == TYPE_VARIANT) &&
        sema_needs_teardown(t)) {
        text_append(out, "/* owning: it owns what its parts own. Copy it "
                         "by value only to move it. */\n");
    }
}

static void tuple_view(struct text *out, const struct type *t,
                       const struct interface *const *ifaces, size_t count,
                       struct emitted *done)
{
    struct text tag = {0};
    struct text written = {0};
    size_t i;

    if (was_emitted(done, t)) {
        return;
    }
    mark_emitted(done, t);
    for (i = 0; i < t->field_count; i++) {
        emit_uses(out, t->fields[i].type, ifaces, count, done);
    }
    tuple_c_name(&tag, t);
    types_name(&written, t);
    owning_note(out, t);
    text_appendf(out, "/* The %s %s. */\nstruct %s {\n",
                 t->kind == TYPE_OPTIONAL ? "optional value" : "tuple",
                 text_cstr(&written), text_cstr(&tag));
    for (i = 0; i < t->field_count; i++) {
        struct text field = {0};
        struct text buffer = {0};
        c_name(&buffer, &t->fields[i].name);
        text_append(out, "    ");
        declaration(&field, t->fields[i].type, text_cstr(&buffer), t);
        text_appendf(out, "%s;\n", text_cstr(&field));
        text_free(&field);
        text_free(&buffer);
    }
    text_append(out, "};\n\n");
    text_free(&written);
    text_free(&tag);
}

/* Every tuple that type t names, the ones inside it included. */
static void emit_tuples(struct text *out, const struct type *t,
                        const struct interface *const *ifaces, size_t count,
                        struct emitted *done)
{
    size_t i;

    if (t == NULL) {
        return;
    }
    switch (t->kind) {
    case TYPE_POINTER:
    case TYPE_ARRAY:
    case TYPE_SLICE:
        emit_tuples(out, t->element, ifaces, count, done);
        return;
    case TYPE_OPTIONAL:
        emit_tuples(out, t->element, ifaces, count, done);
        tuple_view(out, t, ifaces, count, done);
        return;
    case TYPE_FN:
        for (i = 0; i < t->param_count; i++) {
            emit_tuples(out, t->params[i], ifaces, count, done);
        }
        emit_tuples(out, t->result, ifaces, count, done);
        return;
    case TYPE_TUPLE:
        for (i = 0; i < t->param_count; i++) {
            emit_tuples(out, t->params[i], ifaces, count, done);
        }
        tuple_view(out, t, ifaces, count, done);
        return;
    case TYPE_STRUCT:
        if (types_is_flags(t)) {
            flags_view(out, t, done);
        }
        return;
    default:
        return;
    }
}

/* DESIGN: an export variant writes the enum of its tags, then the
   typedef of the struct C sees. The field `tag` holds the integer of
   the tag, since an enum of C has the width of an int, and the enum names
   its values as `T_Case`. The union `u` holds one anonymous struct per
   case that has fields, named by the case. packed and align(N) follow
   the struct rules, and `#pragma pack` covers the structs inside. */
static void variant_view(struct text *out, const struct symbol *sym,
                         const struct interface *const *ifaces, size_t count,
                         struct emitted *done)
{
    const struct type *t = sym->type;
    const struct type *tag = t->base;
    const struct type *u = t->field_count > 1 ? t->fields[1].type : NULL;
    struct text name = {0};
    size_t i;
    size_t j;

    for (i = 0; u != NULL && i < u->field_count; i++) {
        const struct type *payload = u->fields[i].type;
        for (j = 0; j < payload->field_count; j++) {
            emit_uses(out, payload->fields[j].type, ifaces, count, done);
        }
    }
    c_type_name(&name, t);
    text_appendf(out, "/* The tags of %s. */\nenum %s_tag {\n",
                 text_cstr(&name), text_cstr(&name));
    for (i = 0; i < tag->field_count; i++) {
        struct text buffer = {0};
        c_name(&buffer, &tag->fields[i].name);
        doc_comment(out, &tag->fields[i].doc, "    ");
        text_appendf(out, "    %s_%s = %" PRIu64 "%s\n", text_cstr(&name),
                     text_cstr(&buffer), tag->fields[i].number,
                     i + 1 < tag->field_count ? "," : "");
        text_free(&buffer);
    }
    text_append(out, "};\n\n");
    doc_comment(out, &sym->doc, "");
    owning_note(out, t);
    if (t->packed) {
        text_append(out, "#pragma pack(push, 1)\n");
    }
    text_appendf(out, "typedef struct %s {\n    ", text_cstr(&name));
    if (t->align != 0) {
        text_appendf(out, "ANTI_ALIGNAS(%" PRIu64 ") ", t->align);
    }
    text_appendf(out, "%s " VARIANT_TAG ";\n", scalar_name(tag->base));
    if (u != NULL) {
        text_append(out, "    union {\n");
        for (i = 0; i < u->field_count; i++) {
            const struct type *payload = u->fields[i].type;
            struct text buffer = {0};
            text_append(out, "        struct {\n");
            for (j = 0; j < payload->field_count; j++) {
                struct text field = {0};
                struct text member = {0};
                c_name(&member, &payload->fields[j].name);
                doc_comment(out, &payload->fields[j].doc, "            ");
                declaration(&field, payload->fields[j].type,
                            text_cstr(&member), NULL);
                text_appendf(out, "            %s;\n", text_cstr(&field));
                text_free(&field);
                text_free(&member);
            }
            c_name(&buffer, &u->fields[i].name);
            text_appendf(out, "        } %s;\n", text_cstr(&buffer));
            text_free(&buffer);
        }
        text_append(out, "    } " VARIANT_UNION ";\n");
    }
    text_appendf(out, "} %s;\n", text_cstr(&name));
    if (t->packed) {
        text_append(out, "#pragma pack(pop)\n");
    }
    text_append(out, "\n");
    text_free(&name);
}

/* The vector type of C that a simd struct of 16 bytes is, one name per
   architecture. It is the NEON type on ARM64 and the SSE type on
   x86_64. The bits of f16 lanes cross as an integer vector, as a single
   f16 crosses as its sixteen bits. */
static void vector_typedef(struct text *out, const struct type *t)
{
    const struct type *lane = types_simd_lane(t);
    const char *neon;
    const char *sse = "__m128i";

    /* A bool lane is a byte, as a u8 lane is. The u64 lanes are the
       rest. */
    switch (lane->kind) {
    case TYPE_F32: neon = "float32x4_t"; sse = "__m128"; break;
    case TYPE_F64: neon = "float64x2_t"; sse = "__m128d"; break;
    case TYPE_I8: neon = "int8x16_t"; break;
    case TYPE_BOOL:
    case TYPE_U8: neon = "uint8x16_t"; break;
    case TYPE_I16: neon = "int16x8_t"; break;
    case TYPE_U16:
    case TYPE_F16: neon = "uint16x8_t"; break;
    case TYPE_I32: neon = "int32x4_t"; break;
    case TYPE_CHAR:
    case TYPE_U32: neon = "uint32x4_t"; break;
    case TYPE_I64: neon = "int64x2_t"; break;
    default: neon = "uint64x2_t"; break;
    }
    text_appendf(out, "#if defined(__aarch64__) || defined(_M_ARM64)\n"
                      "typedef %s %.*s;\n"
                      "#else\n"
                      "typedef %s %.*s;\n"
                      "#endif\n\n",
                 neon, (int)t->name.length, t->name.text, sse,
                 (int)t->name.length, t->name.text);
}

/* An integer as a C literal that fits its type. The least int64_t has
   no literal of its own, since C reads -9223372036854775808 as the
   negation of a literal past INT64_MAX, so it is written as a sum. An
   unsigned value past INT64_MAX takes `u`. */
static void integer_literal(struct text *out, uint64_t value, bool is_signed)
{
    if (is_signed && value == (uint64_t)INT64_MAX + 1) {
        text_append(out, "(-9223372036854775807 - 1)");
    } else if (is_signed) {
        text_appendf(out, "%" PRId64, (int64_t)value);
    } else {
        text_appendf(out, "%" PRIu64 "%s", value,
                     value > (uint64_t)INT64_MAX ? "u" : "");
    }
}

/* DESIGN: an enum becomes the C enum of its values, named as the type,
   and each value `T_Value`, as the tags of a variant are named. A field
   or a parameter of the type is its underlying integer, which
   declaration writes, because a C enum has the width of an int. */
static void enum_view(struct text *out, const struct type *t,
                      const struct doc_text *doc)
{
    struct text name = {0};
    bool is_signed = types_is_signed(t->base);
    size_t i;

    if (t->field_count == 0) {
        return;
    }
    c_type_name(&name, t);
    doc_comment(out, doc, "");
    text_appendf(out, "enum %s {\n", text_cstr(&name));
    for (i = 0; i < t->field_count; i++) {
        struct text buffer = {0};
        c_name(&buffer, &t->fields[i].name);
        doc_comment(out, &t->fields[i].doc, "    ");
        text_appendf(out, "    %s_%s = ", text_cstr(&name), text_cstr(&buffer));
        integer_literal(out, t->fields[i].number, is_signed);
        text_append(out, i + 1 < t->field_count ? ",\n" : "\n");
        text_free(&buffer);
    }
    text_append(out, "};\n\n");
    text_free(&name);
}

/* The fields of a struct or union t between its braces. */
static void struct_fields(struct text *out, const struct type *t)
{
    size_t i;

    for (i = 0; i < t->field_count; i++) {
        struct text field = {0};
        struct text buffer = {0};
        c_name(&buffer, &t->fields[i].name);
        doc_comment(out, &t->fields[i].doc, "    ");
        text_append(out, "    ");
        if (i == 0 && t->simd) {
            uint64_t bytes = types_simd_bytes(t);
            text_appendf(out, "ANTI_ALIGNAS(%" PRIu64 ") ",
                         bytes < 16 ? bytes : 16);
        } else if (i == 0 && t->align != 0) {
            text_appendf(out, "ANTI_ALIGNAS(%" PRIu64 ") ", t->align);
        }
        declaration(&field, t->fields[i].type,
                    types_field_is_unit_break(&t->fields[i])
                        ? ""
                        : text_cstr(&buffer),
                    t);
        text_append(out, text_cstr(&field));
        if (t->fields[i].bits != 0 ||
            types_field_is_unit_break(&t->fields[i])) {
            text_appendf(out, " : %u", (unsigned)t->fields[i].bits);
        }
        text_append(out, ";\n");
        text_free(&field);
        text_free(&buffer);
    }
}

/* The fields of a class t between its braces: the table pointer of the
   root, the base, then its own fields with their level. */
static void class_fields(struct text *out, const struct type *t)
{
    size_t i;

    for (i = 0; i < t->field_count; i++) {
        struct text field = {0};
        struct text buffer = {0};
        const struct struct_field *f = &t->fields[i];
        if (f->form == FIELD_TABLE) {
            text_append(out, "    const ");
            c_type_name(out, t);
            text_append(out, "_vtable *vtable;\n");
            continue;
        }
        if (f->form == FIELD_BASE) {
            text_appendf(out, "    %s", f->type->base == NULL ? "anti_" : "");
            c_type_name(out, f->type);
            text_append(out, " base;\n");
            continue;
        }
        /* DESIGN: the hidden lock of a synchronized class is a field
           of the layout that no program names. C sees its bytes, a
           Mutex word of the width of the system and two `int64_t`, and
           takes it through the functions of the class alone. */
        if (f->hidden) {
            text_append(out, "    struct {\n"
                             "#if defined(_WIN32)\n"
                             "        void *word;\n"
                             "#else\n"
                             "        uint32_t word;\n"
                             "#endif\n"
                             "        int64_t owner;\n"
                             "        int64_t depth;\n"
                             "    } anti_lock;   /* the lock of the object */\n");
            continue;
        }
        c_name(&buffer, &f->name);
        doc_comment(out, &f->doc, "    ");
        text_append(out, "    ");
        /* DESIGN: an `own fn` field is two words, the code and the
           snapshot, which anti_rt_snapshot_free gives back. */
        if (f->type->kind == TYPE_FN && f->type->owned) {
            text_append(&field, "struct {\n        ");
            callback(&field, f->type, "code", t);
            text_appendf(&field, ";\n        void *snapshot;\n    } %s",
                         text_cstr(&buffer));
        } else {
            declaration(&field, f->type, text_cstr(&buffer), t);
        }
        text_free(&buffer);
        text_append(out, text_cstr(&field));
        text_appendf(out, ";%s%s\n",
                     f->owned ? "   /* own */" : "",
                     f->vis == VIS_PUB ? ""
                     : f->vis == VIS_PROTECTED ? "   /* protected */"
                                               : "   /* private */");
        text_free(&field);
    }
}

/* Whether t is a type nested in the class outer, at any depth. Such a
   type is of the module of outer, and its full name starts with the name
   of outer and a dot. */
static bool nested_in(const struct type *t, const struct type *outer)
{
    return (t->kind == TYPE_STRUCT || t->kind == TYPE_CLASS ||
            t->kind == TYPE_ENUM) &&
           t->module.length == outer->module.length &&
           memcmp(t->module.text, outer->module.text, t->module.length) == 0 &&
           t->name.length > outer->name.length &&
           t->name.text[outer->name.length] == '.' &&
           memcmp(t->name.text, outer->name.text, outer->name.length) == 0;
}

/* The types nested in outer that the fields of t reach, directly or
   through a pointer or an array. Each goes into order after the ones
   its own fields reach, so what it holds by value stands before it. */
static void collect_nested(const struct type *t, const struct type *outer,
                           struct emitted *seen, struct emitted *order)
{
    size_t i;

    for (i = 0; i < t->field_count; i++) {
        const struct type *f = t->fields[i].type;
        while (f != NULL &&
               (f->kind == TYPE_POINTER || f->kind == TYPE_ARRAY)) {
            f = f->element;
        }
        if (f == NULL || !nested_in(f, outer) || was_emitted(seen, f)) {
            continue;
        }
        mark_emitted(seen, f);
        collect_nested(f, outer, seen, order);
        mark_emitted(order, f);
    }
}

/* DESIGN: a type nested in an export class is private to it. C sees its
   layout alone, because the layout of the class holds it. The header
   writes each one the fields of the class reach before the class. It
   writes no table, no prototype and no helper of one. A typedef of every
   struct and class comes first, so a pointer to one that stands later is
   declared.
   A library file carries the types the fields reach, so the header
   written from one equals the header of --lib. */
static void nested_views(struct text *out, const struct type *t,
                         const struct interface *const *ifaces,
                         size_t iface_count, struct emitted *done)
{
    struct emitted seen = {0};
    struct emitted order = {0};
    size_t i;
    size_t j;

    collect_nested(t, t, &seen, &order);
    /* What the fields of each hold by value stands before them. */
    for (i = 0; i < order.count; i++) {
        const struct type *n = order.items[i];
        for (j = 0; j < n->field_count; j++) {
            if (n->fields[j].form != FIELD_TABLE && !n->fields[j].hidden) {
                emit_uses(out, n->fields[j].type, ifaces, iface_count, done);
            }
        }
    }
    for (i = 0; i < order.count; i++) {
        const struct type *n = order.items[i];
        struct text name = {0};
        if (n->kind == TYPE_ENUM) {
            continue;
        }
        c_type_name(&name, n);
        text_appendf(out, "typedef %s %s %s;\n",
                     n->is_union ? "union" : "struct", text_cstr(&name),
                     text_cstr(&name));
        text_free(&name);
    }
    text_append(out, order.count > 0 ? "\n" : "");
    for (i = 0; i < order.count; i++) {
        const struct type *n = order.items[i];
        struct text name = {0};
        if (n->kind == TYPE_ENUM) {
            enum_view(out, n, &(struct doc_text){NULL, 0, 0, 0});
            continue;
        }
        c_type_name(&name, n);
        text_appendf(out, "/* %.*s, private to %.*s. */\n",
                     (int)n->name.length, n->name.text, (int)t->name.length,
                     t->name.text);
        if (n->packed) {
            text_append(out, "#pragma pack(push, 1)\n");
        }
        text_appendf(out, "struct %s {\n", text_cstr(&name));
        if (n->kind == TYPE_CLASS) {
            class_fields(out, n);
        } else {
            struct_fields(out, n);
        }
        text_append(out, "};\n");
        if (n->packed) {
            text_append(out, "#pragma pack(pop)\n");
        }
        text_append(out, "\n");
        text_free(&name);
    }
    free((void *)seen.items);
    free((void *)order.items);
}

/* DESIGN: an export struct or union becomes a typedef of the same name.
   packed becomes #pragma pack and align(N) an _Alignas on the first
   field, which C++ spells alignas. A simd struct of 16 bytes is the
   vector type of C. One of another size is the struct of its lanes,
   aligned to its size or to sixteen, whichever is less. */
static void aggregate(struct text *out, const struct symbol *sym,
                      const struct interface *const *ifaces, size_t count,
                      struct emitted *done)
{
    const struct type *t = sym->type;
    const char *kind = t->is_union ? "union" : "struct";
    size_t i;

    if (was_emitted(done, t)) {
        return;
    }
    mark_emitted(done, t);
    if (t->kind == TYPE_CLASS) {
        class_view(out, sym, ifaces, count, done);
        return;
    }
    if (t->kind == TYPE_VARIANT) {
        variant_view(out, sym, ifaces, count, done);
        return;
    }
    if (t->kind == TYPE_ENUM) {
        enum_view(out, t, &sym->doc);
        return;
    }
    for (i = 0; i < t->field_count; i++) {
        emit_uses(out, t->fields[i].type, ifaces, count, done);
    }
    doc_comment(out, &sym->doc, "");
    if (t->simd && types_simd_bytes(t) == 16) {
        vector_typedef(out, t);
        return;
    }
    owning_note(out, t);
    if (t->packed) {
        text_append(out, "#pragma pack(push, 1)\n");
    }
    text_appendf(out, "typedef %s %.*s {\n", kind,
                 (int)types_c_name(t).length, types_c_name(t).text);
    struct_fields(out, t);
    text_appendf(out, "} %.*s;\n", (int)types_c_name(t).length,
                 types_c_name(t).text);
    if (t->packed) {
        text_append(out, "#pragma pack(pop)\n");
    }
    text_append(out, "\n");
}

/* The doc comment says that a function may fail, because the C signature
   alone does not: `?*Error` and a pointer parameter are one type each. */
static void may_fail_note(struct text *out, const struct symbol *sym,
                          const char *indent)
{
    if (sym->may_fail) {
        text_appendf(out, "%s/* May fail: NULL on success, an error "
                     "otherwise. */\n", indent);
    }
}

/* One prototype of a table entry or of a function of a class, with self
   written out as the first parameter. callee is what stands before the
   parameters: the name of a function, or `(*slot)` in a table type. */
static void member_signature(struct text *out, const struct type *owner,
                             const struct item *m, const char *callee)
{
    const struct type *t = m->symbol->type;
    struct text inner = {0};
    struct text decl = {0};
    size_t i;

    text_appendf(&inner, "%s(", callee);
    for (i = 0; i < t->param_count; i++) {
        struct text param = {0};
        struct text buffer = {0};
        size_t k = i - (m->has_self ? 1 : 0);
        if (i == 0 && m->has_self) {
            c_type_name(&inner, owner);
            text_append(&inner, " *self");
            continue;
        }
        if (m->symbol->params != NULL && k < m->param_count) {
            c_name(&buffer, &m->symbol->params[k]);
        } else if (m->params != NULL && k < m->param_count) {
            c_name(&buffer, &m->params[k].name);
        } else {
            text_appendf(&buffer, "a%zu", k);
        }
        declaration(&param, t->params[i], text_cstr(&buffer), NULL);
        text_appendf(&inner, "%s%s%s", i > 0 ? ", " : "",
                     owned_note(m->symbol, i), text_cstr(&param));
        text_free(&param);
        text_free(&buffer);
    }
    text_append(&inner, t->param_count == 0 ? "void)" : ")");
    declaration(&decl, t->result, text_cstr(&inner), NULL);
    text_append(out, text_cstr(&decl));
    text_free(&inner);
    text_free(&decl);
}

/* The symbol of the function m of the class owner, which the library
   holds: `T_f`, and `T_Q_f` for a body qualified by a base, so that a
   plain body of its name keeps `T_f`. */
static void direct_name(struct text *out, const struct type *owner,
                        const struct item *m)
{
    c_type_name(out, owner);
    if (m->qualifier.length > 0 && types_body_table(owner, m) == BODY_BASE) {
        text_appendf(out, "_%.*s", (int)m->qualifier.length,
                     m->qualifier.text);
    }
    text_appendf(out, "_%.*s", (int)m->name.length, m->name.text);
}

/* Whether the entry e is one of the root's, whose function the runtime
   holds. Its slot is a `void *`, and it has no wrapper. */
static bool root_entry(const struct table_entry *e)
{
    return e->fn == NULL || e->fn->runtime != NULL;
}

/* DESIGN: a slot of the table type takes the name of its function. Two
   statics of one name, or a hook of `anti.lang.TraceHandler` beside the
   root's, are two entries of one name, and a struct of C holds one
   member per name. A slot whose name an earlier slot has therefore takes
   `_` until its name is its own, so `make` and `make_`. names holds one
   text per entry, which the caller frees. */
static void slot_names(const struct table_entry *entries, size_t count,
                       struct text *names)
{
    size_t i;
    size_t j;
    bool taken;

    for (i = 0; i < count; i++) {
        text_appendf(&names[i], "%.*s", (int)entries[i].name.length,
                     entries[i].name.text);
        do {
            taken = false;
            for (j = 0; j < i && !taken; j++) {
                taken = strcmp(text_cstr(&names[i]), text_cstr(&names[j])) == 0;
            }
            if (taken) {
                text_append(&names[i], "_");
            }
        } while (taken);
    }
}

/* Whether name, of length characters, is one of the helpers the header
   writes for the class t as `anti_<t>_<name>`: `init`, `construct`,
   `delete`, `destroy`, `dup`, `descriptor` and `vtable`, and `as_<I>` and
   `<I>_vtable` for each interface the chain implements. */
static bool is_helper(const struct type *t, const char *name, size_t length)
{
    static const char *const helpers[] = {"init", "construct", "delete",
                                          "destroy", "dup", "descriptor",
                                          "vtable"};
    const struct type *up;
    size_t i;
    bool found = false;

    for (i = 0; i < sizeof helpers / sizeof helpers[0]; i++) {
        if (strlen(helpers[i]) == length &&
            memcmp(helpers[i], name, length) == 0) {
            return true;
        }
    }
    for (up = t; up != NULL && !found;
         up = up->kind == TYPE_CLASS ? up->base : NULL) {
        for (i = 0; i < up->field_count && !found; i++) {
            struct text as = {0};
            struct text table = {0};
            if (up->fields[i].form != FIELD_IMPL) {
                continue;
            }
            text_append(&as, "as_");
            c_type_name(&as, up->fields[i].type);
            c_type_name(&table, up->fields[i].type);
            text_append(&table, "_vtable");
            found = (as.length == length &&
                     memcmp(text_cstr(&as), name, length) == 0) ||
                    (table.length == length &&
                     memcmp(text_cstr(&table), name, length) == 0);
            text_free(&as);
            text_free(&table);
        }
    }
    return found;
}

/* DESIGN: the wrapper of a slot is `anti_<C>_<slot>`, beside the helpers
   of the same prefix. A slot whose name without its trailing `_` is the
   name of a helper takes one `_` more, so `pub fn init(self)` has the
   wrapper `anti_C_init_` and `init_` has `anti_C_init__`. No wrapper then
   takes the name of a helper or of another wrapper. */
static void wrapper_name(struct text *out, const struct type *t,
                         const char *slot)
{
    size_t length = strlen(slot);

    while (length > 0 && slot[length - 1] == '_') {
        length--;
    }
    text_append(out, "anti_");
    c_type_name(out, t);
    text_appendf(out, "_%s%s", slot, is_helper(t, slot, length) ? "_" : "");
}

/* The `construct` of t that takes arguments, or NULL. */
static const struct item *construct_with_arguments(const struct type *t)
{
    size_t i;

    for (i = 0; i < t->member_count; i++) {
        const struct item *m = t->members[i];
        if (m->kind == ITEM_FN && m->has_self && m->symbol != NULL &&
            m->name.length == 9 && memcmp(m->name.text, "construct", 9) == 0 &&
            m->symbol->type->param_count > 1) {
            return m;
        }
    }
    return NULL;
}

/* DESIGN: an export class becomes the nested layout and a table type. It
   also becomes an extern table and descriptor, one prototype per public
   function, and the helpers under the `anti_` prefix. An abstract class
   has no complete value, so it gets no table symbol and no `init`. */
/* DESIGN: a function of a synchronized class that code outside the
   class calls runs under the lock of its object. The header says so
   where it declares the function. */
static void locked_note(struct text *out, const struct type *t,
                        const struct item *fn)
{
    if (t->safety == SAFETY_SYNCHRONIZED && fn->vis != VIS_PRIVATE &&
        !(fn->name.length == 9 && memcmp(fn->name.text, "construct", 9) == 0) &&
        !(fn->name.length == 8 && memcmp(fn->name.text, "destruct", 8) == 0)) {
        text_append(out, "/* Runs under the lock of its object. */\n");
    }
}

/* What the parameters and the result of the function type t hold by
   value. */
static void signature_uses(struct text *out, const struct type *t,
                           const struct interface *const *ifaces,
                           size_t iface_count, struct emitted *done)
{
    size_t i;

    for (i = 0; i < t->param_count; i++) {
        emit_uses(out, t->params[i], ifaces, iface_count, done);
    }
    emit_uses(out, t->result, ifaces, iface_count, done);
}

/* What the class t names before its own definition: the types its fields
   hold, and what the signatures of its table and its `construct` hold by
   value. The fields of a nested type are nested_views'. */
static void class_uses(struct text *out, const struct type *t,
                       const struct table_entry *entries, size_t count,
                       const struct interface *const *ifaces,
                       size_t iface_count, struct emitted *done)
{
    const struct item *made = construct_with_arguments(t);
    size_t i;

    for (i = 0; i < t->field_count; i++) {
        if (t->fields[i].form != FIELD_TABLE && !t->fields[i].hidden) {
            emit_uses(out, t->fields[i].type, ifaces, iface_count, done);
        }
    }
    for (i = 0; i < count; i++) {
        if (!root_entry(&entries[i]) && entries[i].fn->symbol != NULL) {
            signature_uses(out, entries[i].fn->symbol->type, ifaces,
                           iface_count, done);
        }
    }
    if (made != NULL && !t->has_abstract) {
        signature_uses(out, made->symbol->type, ifaces, iface_count, done);
    }
}

/* What the parts of the C view of one class share. */
struct class_parts {
    const struct type *t;
    struct table_entry *entries;    /* the entries of its table */
    size_t count;
    struct text *slots;             /* the name of the slot of each entry */
    struct text name;               /* the C name of the class */
    struct text to_root;            /* `base.` once per level to the root */
};

/* The type of the table of the class: a slot per entry. */
static void class_table(struct text *out, const struct class_parts *p)
{
    size_t i;

    text_appendf(out, "typedef struct %s_vtable {\n"
                      "    const void *descriptor;\n"
                      "    /* The seven functions and the nine hooks of "
                      "anti.lang.Object. They\n       take and give Anti "
                      "values, so C reads their slots and does not call\n"
                      "       them. */\n",
                 text_cstr(&p->name));
    for (i = 0; i < p->count; i++) {
        struct text callee = {0};
        if (root_entry(&p->entries[i])) {
            text_appendf(out, "    void *%s;\n", text_cstr(&p->slots[i]));
            continue;
        }
        text_appendf(&callee, "(*%s)", text_cstr(&p->slots[i]));
        text_append(out, "    ");
        member_signature(out, p->t, p->entries[i].fn, text_cstr(&callee));
        text_append(out, ";\n");
        text_free(&callee);
    }
    text_appendf(out, "} %s_vtable;\n\n", text_cstr(&p->name));
}

/* The struct of the class sym, its descriptor, its table and init, the
   counterpart of `Class(args)`, and delete, destroy and dup. */
static void class_struct(struct text *out, const struct symbol *sym,
                         const struct class_parts *p)
{
    const struct type *t = p->t;
    const char *name = text_cstr(&p->name);
    const struct item *made;

    doc_comment(out, &sym->doc, "");
    text_appendf(out, "struct %s {\n", name);
    class_fields(out, t);
    text_appendf(out, "};\n\n");

    text_appendf(out, "extern const anti_descriptor anti_%s_descriptor;\n",
                 name);
    if (t->has_abstract) {
        text_appendf(out, "/* %s is abstract: no table and no init, "
                          "because it has no complete value. */\n",
                     name);
    } else {
        text_appendf(out, "extern const %s_vtable anti_%s_vtable;\n"
                          "void anti_%s_init(%s *self);\n",
                     name, name, name, name);
        /* The counterpart of `Class(args)`, which prepares self as the
           init does and then runs `construct` with the arguments. */
        made = construct_with_arguments(t);
        if (made != NULL) {
            struct text callee = {0};
            text_appendf(&callee, "anti_%s_construct", name);
            doc_comment(out, &made->doc, "");
            text_appendf(out, "/* Prepares self as anti_%s_init does, then "
                              "runs construct. */\n",
                         name);
            may_fail_note(out, made->symbol, "");
            member_signature(out, t, made, text_cstr(&callee));
            text_append(out, ";\n");
            text_free(&callee);
        }
    }
    text_appendf(out,
                 "static inline void anti_%s_delete(%s *self)\n"
                 "{\n    anti_rt_delete(self, &anti_%s_descriptor);\n}\n"
                 "static inline void anti_%s_destroy(%s *self)\n"
                 "{\n    anti_rt_destroy(self, &anti_%s_descriptor);\n}\n"
                 "static inline %s *anti_%s_dup(%s *self)\n"
                 "{\n    return (%s *)anti_rt_dup(self, "
                 "&anti_%s_descriptor);\n}\n\n",
                 name, name, name, name, name, name, name, name, name, name,
                 name);
}

/* DESIGN: an interface sub-object is a field of the object, so C
   reaches the interface by taking its address. The table of that
   sub-object belongs to the class t and holds thunks. */
static void interface_views(struct text *out, const struct class_parts *p)
{
    const struct type *t = p->t;
    const struct type *up;
    size_t i;

    for (up = t; up != NULL; up = up->kind == TYPE_CLASS ? up->base : NULL) {
        for (i = 0; i < up->field_count; i++) {
            const struct struct_field *f = &up->fields[i];
            struct text buffer = {0};
            struct text iface = {0};
            if (f->form != FIELD_IMPL) {
                continue;
            }
            c_name(&buffer, &f->name);
            c_type_name(&iface, f->type);
            text_appendf(out,
                         "extern const %s_vtable anti_%s_%s_vtable;\n"
                         "static inline %s *anti_%s_as_%s(%s *self)\n"
                         "{\n    return &self->%s%s;\n}\n",
                         text_cstr(&iface), text_cstr(&p->name),
                         text_cstr(&iface), text_cstr(&iface),
                         text_cstr(&p->name), text_cstr(&iface),
                         text_cstr(&p->name), up == t ? "" : "base.",
                         text_cstr(&buffer));
            text_free(&iface);
            text_free(&buffer);
        }
    }
}

/* DESIGN: each prototype names a symbol the library holds, so a C
   call to it links. The section of the class that declares a function
   declares it once, and a class below does not repeat it. An abstract
   entry has no body and no prototype, and its wrapper carries its
   comment. */
static void class_prototypes(struct text *out, const struct class_parts *p)
{
    const struct type *t = p->t;
    size_t i;

    for (i = 0; i < p->count; i++) {
        const struct item *fn = p->entries[i].fn;
        struct text callee = {0};
        if (root_entry(&p->entries[i]) || fn->contract == FN_ABSTRACT ||
            types_member_level(t, fn) != t) {
            continue;
        }
        direct_name(&callee, t, fn);
        doc_comment(out, &fn->doc, "");
        /* DESIGN: the symbol of a function named `vtable` is the name of
           the table type, which C cannot declare twice. C calls it
           through its wrapper alone. */
        if (strcmp(text_cstr(&callee) + p->name.length, "_vtable") == 0) {
            text_appendf(out, "/* %s is the table type, so C calls this "
                              "function through its wrapper. */\n",
                         text_cstr(&callee));
            text_free(&callee);
            continue;
        }
        may_fail_note(out, fn->symbol, "");
        locked_note(out, t, fn);
        member_signature(out, t, fn, text_cstr(&callee));
        text_append(out, ";\n");
        text_free(&callee);
    }
    text_append(out, "\n");
}

/* The wrapper of entry i, which reads the table of the object. */
static void class_wrapper(struct text *out, const struct class_parts *p,
                          size_t i)
{
    const struct item *fn = p->entries[i].fn;
    struct text callee = {0};
    size_t j;

    if (fn->contract == FN_ABSTRACT) {
        doc_comment(out, &fn->doc, "");
    }
    wrapper_name(&callee, p->t, text_cstr(&p->slots[i]));
    text_append(out, "static inline ");
    member_signature(out, p->t, fn, text_cstr(&callee));
    text_appendf(out,
                 "\n{\n    %s ((const %s_vtable *)self->%svtable)"
                 "->%s(self",
                 fn->symbol->type->result->kind == TYPE_VOID ? ""
                                                             : "return",
                 text_cstr(&p->name), text_cstr(&p->to_root),
                 text_cstr(&p->slots[i]));
    for (j = 1; j < fn->symbol->type->param_count; j++) {
        struct text buffer = {0};
        size_t k = j - 1;
        if (fn->symbol->params != NULL && k < fn->param_count) {
            c_name(&buffer, &fn->symbol->params[k]);
        } else if (fn->params != NULL && k < fn->param_count) {
            c_name(&buffer, &fn->params[k].name);
        } else {
            text_appendf(&buffer, "a%zu", k);
        }
        text_appendf(out, ", %s", text_cstr(&buffer));
        text_free(&buffer);
    }
    text_append(out, ");\n}\n");
    text_free(&callee);
}

static void class_view(struct text *out, const struct symbol *sym,
                       const struct interface *const *ifaces,
                       size_t iface_count, struct emitted *done)
{
    struct class_parts p = {0};
    const struct type *up;
    size_t i;

    p.t = sym->type;
    p.entries = alloc_zeroed(sema_table_bound(p.t), sizeof *p.entries);
    p.count = sema_table_of(p.t, p.entries);
    p.slots = alloc_zeroed(p.count + 1, sizeof *p.slots);
    class_uses(out, p.t, p.entries, p.count, ifaces, iface_count, done);
    slot_names(p.entries, p.count, p.slots);
    c_type_name(&p.name, p.t);
    /* The table pointer sits in the root, so a wrapper reaches it
       through one `base` per level of the chain. */
    for (up = p.t; up != NULL && up->base != NULL; up = up->base) {
        text_append(&p.to_root, "base.");
    }

    text_appendf(out, "typedef struct %s %s;\n", text_cstr(&p.name),
                 text_cstr(&p.name));
    nested_views(out, p.t, ifaces, iface_count, done);
    class_table(out, &p);
    class_struct(out, sym, &p);
    interface_views(out, &p);
    class_prototypes(out, &p);
    /* A wrapper per entry with `self`, which reads the table of the
       object. A static has no object and no wrapper, and C calls it by
       its prototype. */
    for (i = 0; i < p.count; i++) {
        if (!root_entry(&p.entries[i]) && p.entries[i].fn->has_self) {
            class_wrapper(out, &p, i);
        }
    }
    text_append(out, "\n");
    for (i = 0; i < p.count; i++) {
        text_free(&p.slots[i]);
    }
    text_free(&p.to_root);
    text_free(&p.name);
    free(p.slots);
    free(p.entries);
}

static void constant(struct text *out, const struct symbol *sym)
{
    const struct const_value *v = sym->value;
    const struct type *t = sym->type;
    size_t i;

    doc_comment(out, &sym->doc, "");
    switch (v->kind) {
    case CONST_BOOL:
        text_appendf(out, "#define %.*s %s\n", (int)sym->name.length,
                     sym->name.text, v->as.boolean ? "true" : "false");
        return;
    /* DESIGN: infinity and NaN have no literal in C. The header writes
       INFINITY and NAN of <math.h>, which it then includes, as the
       float of the type, with the sign of an infinity. A NaN is written
       as NAN whatever its sign and payload, as antic prints it `nan`. */
    case CONST_FLOAT: {
        struct text digits = {0};
        const char *as_double = t->kind == TYPE_F32 ? "" : "(double)";
        if (isnan(v->as.floating)) {
            text_appendf(out, "#define %.*s (%sNAN)\n", (int)sym->name.length,
                         sym->name.text, as_double);
            return;
        }
        if (isinf(v->as.floating)) {
            text_appendf(out, "#define %.*s (%s%sINFINITY)\n",
                         (int)sym->name.length, sym->name.text,
                         v->as.floating < 0 ? "-" : "", as_double);
            return;
        }
        /* A whole value gets `.0`, since C reads `2` as an int and refuses
           `2f`. */
        text_appendf(&digits, "%.17g", v->as.floating);
        text_appendf(out, "#define %.*s %s%s%s\n", (int)sym->name.length,
                     sym->name.text, text_cstr(&digits),
                     strpbrk(text_cstr(&digits), ".e") == NULL ? ".0" : "",
                     t->kind == TYPE_F32 ? "f" : "");
        text_free(&digits);
        return;
    }
    case CONST_TEXT:
        text_appendf(out, "static const char %.*s[] = \"",
                     (int)sym->name.length, sym->name.text);
        for (i = 0; i < v->as.text.length; i++) {
            unsigned char c = (unsigned char)v->as.text.bytes[i];
            if (c == '"' || c == '\\') {
                text_appendf(out, "\\%c", c);
            } else if (c < 0x20 || c >= 0x7f) {
                text_appendf(out, "\\%03o", c);
            } else {
                text_appendf(out, "%c", c);
            }
        }
        text_append(out, "\";\n");
        return;
    default:
        text_appendf(out, "#define %.*s ((%s)", (int)sym->name.length,
                     sym->name.text, scalar_name(t));
        integer_literal(out, v->as.integer, types_is_signed(t));
        text_append(out, ")\n");
        return;
    }
}

/* A prototype names its parameters as the Anti function does. */
static void prototype(struct text *out, const struct symbol *sym)
{
    const struct type *t = sym->type;
    struct text inner = {0};
    struct text decl = {0};
    size_t i;

    doc_comment(out, &sym->doc, "");
    may_fail_note(out, sym, "");
    text_appendf(&inner, "%.*s(", (int)sym->name.length, sym->name.text);
    for (i = 0; i < t->param_count; i++) {
        struct text param = {0};
        struct text name = {0};
        c_name(&name, &sym->params[i]);
        declaration(&param, t->params[i], text_cstr(&name), NULL);
        text_appendf(&inner, "%s%s%s", i > 0 ? ", " : "", owned_note(sym, i),
                     text_cstr(&param));
        text_free(&param);
        text_free(&name);
    }
    text_append(&inner, t->param_count == 0 ? "void)" : ")");
    declaration(&decl, t->result, text_cstr(&inner), NULL);
    text_appendf(out, "%s;\n", text_cstr(&decl));
    text_free(&inner);
    text_free(&decl);
}

/* The include guard of the header name: the name in capitals, every
   other character but a digit as `_`, and `_H`. */
static void guard_name(struct text *out, const char *name)
{
    size_t i;

    for (i = 0; name[i] != '\0'; i++) {
        char c = name[i];
        text_appendf(out, "%c", (c >= 'a' && c <= 'z') ? (char)(c - 32)
                                : ((c >= 'A' && c <= 'Z') ||
                                   (c >= '0' && c <= '9'))
                                    ? c
                                    : '_');
    }
    text_append(out, "_H");
}

/* Whether an exported constant of the interfaces is a float that is not
   finite, which the header writes with <math.h>. */
static bool needs_math(const struct interface *const *ifaces, size_t count)
{
    size_t i;
    size_t j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < ifaces[i]->item_count; j++) {
            const struct symbol *sym = ifaces[i]->items[j];
            if (sym->exported && sym->kind == SYMBOL_CONST &&
                sym->value->kind == CONST_FLOAT &&
                !isfinite(sym->value->as.floating)) {
                return true;
            }
        }
    }
    return false;
}

/* Whether an exported class of the interfaces has an `own fn` field,
   whose snapshot C frees with anti_rt_snapshot_free. */
static bool owns_snapshots(const struct interface *const *ifaces,
                           size_t count)
{
    size_t i;
    size_t j;
    size_t k;

    for (i = 0; i < count; i++) {
        for (j = 0; j < ifaces[i]->item_count; j++) {
            const struct symbol *sym = ifaces[i]->items[j];
            if (!sym->exported || sym->kind != SYMBOL_STRUCT ||
                sym->type->kind != TYPE_CLASS) {
                continue;
            }
            for (k = 0; k < sym->type->field_count; k++) {
                const struct type *f = sym->type->fields[k].type;
                if (f != NULL && f->kind == TYPE_FN && f->owned) {
                    return true;
                }
            }
        }
    }
    return false;
}

/* The comment at the top, the include guard, the headers of C and the
   opening of `extern "C"`. */
static void write_preamble(struct text *out, const char *name,
                           const struct interface *const *ifaces,
                           size_t count, bool bundled)
{
    text_appendf(out, "/* %s.h, the C interface of %s, written by antic.\n"
                      "   Do not edit. A failure that Anti cannot report calls "
                      "abort().%s */\n",
                 name, count > 0 ? ifaces[count - 1]->module : name,
                 bundled ? "\n   The archive holds the Anti runtime. Link only "
                           "one archive\n   with a bundled runtime into a "
                           "program."
                         : "");
    text_append(out, "#ifndef ");
    guard_name(out, name);
    text_append(out, "\n#define ");
    guard_name(out, name);
    text_append(out, "\n\n");
    text_append(out, needs_math(ifaces, count) ? "#include <math.h>\n" : "");
    text_append(out, "#include <stdbool.h>\n"
                     "#include <stddef.h>\n"
                     "#include <stdint.h>\n\n"
                     "#ifdef __cplusplus\n"
                     "#define ANTI_ALIGNAS(n) alignas(n)\n"
                     "extern \"C\" {\n"
                     "#else\n"
                     "#define ANTI_ALIGNAS(n) _Alignas(n)\n"
                     "#endif\n\n");
}

/* The vector types of C, which a simd struct of 16 bytes is. Each
   architecture has its own header for them. */
static void write_vector_types(struct text *out,
                               const struct interface *const *ifaces,
                               size_t count)
{
    size_t i;
    size_t j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < ifaces[i]->item_count; j++) {
            const struct symbol *sym = ifaces[i]->items[j];
            if (sym->exported && sym->kind == SYMBOL_STRUCT &&
                types_is_simd(sym->type) &&
                types_simd_bytes(sym->type) == 16) {
                text_append(out,
                    "#if defined(__aarch64__) || defined(_M_ARM64)\n"
                    "#include <arm_neon.h>\n"
                    "#else\n"
                    "#include <immintrin.h>\n"
                    "#endif\n\n");
                return;
            }
        }
    }
}

/* The error class, declared once when an exported signature names it. A
   C caller passes the pointer on and never reads it. */
static void write_error_class(struct text *out,
                              const struct interface *const *ifaces,
                              size_t count)
{
    size_t i;
    size_t j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < ifaces[i]->item_count; j++) {
            const struct symbol *sym = ifaces[i]->items[j];
            if (sym->exported &&
                (sym->kind == SYMBOL_FN || sym->kind == SYMBOL_STRUCT) &&
                type_names_error(sym->type)) {
                text_append(out,
                    "/* An Anti error. A function that may fail returns a "
                    "pointer to one,\n   or NULL on success. C passes it on "
                    "and never reads its layout. */\n"
                    "struct anti_Error;\n\n");
                return;
            }
        }
    }
}

/* The root of every class chain, which the base of a class nests,
   written once when an exported class needs it. */
static void write_root(struct text *out,
                       const struct interface *const *ifaces, size_t count)
{
    size_t i;
    size_t j;

    for (i = 0; i < count; i++) {
        for (j = 0; j < ifaces[i]->item_count; j++) {
            const struct symbol *sym = ifaces[i]->items[j];
            if (!sym->exported || sym->kind != SYMBOL_STRUCT ||
                sym->type->kind != TYPE_CLASS) {
                continue;
            }
            text_append(out,
                "/* The root of every class chain, and the record at "
                "entry 0 of\n   every table. A C program reads the "
                "layout and never builds one. */\n"
                "typedef struct anti_descriptor anti_descriptor;\n"
                "typedef struct anti_Object {\n"
                "    const void *vtable;\n"
                "} anti_Object;\n\n"
                "/* Run the destruct chain of the object, free what it owns "
                "and free it. */\n"
                "void anti_rt_delete(void *object, "
                "const anti_descriptor *type);\n"
                "void anti_rt_destroy(void *object, "
                "const anti_descriptor *type);\n"
                "void *anti_rt_dup(void *object, "
                "const anti_descriptor *type);\n\n");
            if (owns_snapshots(ifaces, count)) {
                text_append(out,
                    "/* Free the snapshot of an `own fn` field, which "
                    "the object frees\n   with itself. NULL frees "
                    "nothing. */\n"
                    "void anti_rt_snapshot_free(void *snapshot);\n\n");
            }
            return;
        }
    }
}

/* One struct per distinct tuple of an exported signature, after the
   aggregates an element may hold by value. */
static void write_tuples(struct text *out,
                         const struct interface *const *ifaces, size_t count,
                         struct emitted *done)
{
    size_t i;
    size_t j;
    size_t k;

    for (i = 0; i < count; i++) {
        for (j = 0; j < ifaces[i]->item_count; j++) {
            const struct symbol *sym = ifaces[i]->items[j];
            if (!sym->exported || sym->kind != SYMBOL_FN) {
                continue;
            }
            for (k = 0; k < sym->type->param_count; k++) {
                emit_tuples(out, sym->type->params[k], ifaces, count, done);
            }
            emit_tuples(out, sym->type->result, ifaces, count, done);
        }
    }
}

/* Each exported symbol of kind: a constant as constant writes it, and a
   function as its prototype. A blank line follows when any stands. */
static void write_symbols(struct text *out,
                          const struct interface *const *ifaces, size_t count,
                          enum symbol_kind kind)
{
    size_t i;
    size_t j;
    bool any = false;

    for (i = 0; i < count; i++) {
        for (j = 0; j < ifaces[i]->item_count; j++) {
            const struct symbol *sym = ifaces[i]->items[j];
            if (sym->exported && sym->kind == kind) {
                if (kind == SYMBOL_CONST) {
                    constant(out, sym);
                } else {
                    prototype(out, sym);
                }
                any = true;
            }
        }
    }
    text_append(out, any ? "\n" : "");
}

void header_write(struct text *out, const char *name,
                  const struct interface *const *ifaces, size_t count,
                  bool bundled)
{
    struct emitted done = {0};
    size_t i;
    size_t j;

    write_preamble(out, name, ifaces, count, bundled);
    write_vector_types(out, ifaces, count);
    write_error_class(out, ifaces, count);
    write_root(out, ifaces, count);
    for (i = 0; i < count; i++) {
        for (j = 0; j < ifaces[i]->item_count; j++) {
            const struct symbol *sym = ifaces[i]->items[j];
            if (sym->exported && sym->kind == SYMBOL_STRUCT) {
                aggregate(out, sym, ifaces, count, &done);
            }
        }
    }
    write_tuples(out, ifaces, count, &done);
    write_symbols(out, ifaces, count, SYMBOL_CONST);
    write_symbols(out, ifaces, count, SYMBOL_FN);
    text_append(out, "#ifdef __cplusplus\n}\n#endif\n\n#endif\n");
    free((void *)done.items);
}

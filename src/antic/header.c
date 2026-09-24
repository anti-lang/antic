#include "header.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ir.h"

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
static bool is_error_class(const struct type *t)
{
    return types_is_lang_error(t);
}

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
    if (t->result->kind == TYPE_POINTER && is_error_class(t->result->element)) {
        return true;
    }
    for (i = 0; i < t->param_count; i++) {
        if (t->params[i]->kind == TYPE_POINTER &&
            is_error_class(t->params[i]->element)) {
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

/* DESIGN: a tuple has no name of its own, so the header makes one from
   its elements: `(int, str)` becomes `anti_tuple_int_str`. A pointer
   writes `ptr_` before what it points at and a nullable pointer `optr_`,
   an array its length, and a tuple its own elements. Two tuples of the
   same elements are one type, so one name stands for one type. */
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
        return;
    case TYPE_TUPLE:
        text_append(out, "tuple");
        for (i = 0; i < t->param_count; i++) {
            text_append(out, "_");
            element_c_name(out, t->params[i]);
        }
        return;
    default:
        type_name(out, t);
        return;
    }
}

static void tuple_c_name(struct text *out, const struct type *t)
{
    text_append(out, "anti_");
    element_c_name(out, t);
}

/* The comment before parameter i of sym when it is `own`. The function
   takes over what C passes there, as an `own` field is freed by its
   object. */
static const char *owned_note(const struct symbol *sym, size_t i)
{
    return sym != NULL && sym->owned != NULL && i < sym->owned_count &&
                   sym->owned[i]
               ? "/* own */ "
               : "";
}

/* DESIGN: the C name of a type is its name. A type nested in a class
   has the full name `PeopleList.Node`, and a dot is no C identifier. The
   header therefore writes `PeopleList_Node`, as c_symbol writes the
   symbol of a function of a class. */
static void c_type_name(struct text *out, const struct type *t)
{
    size_t i;

    for (i = 0; i < t->name.length; i++) {
        text_appendf(out, "%c", t->name.text[i] == '.' ? '_' : t->name.text[i]);
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
        if (is_error_class(t)) {
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
    e->items = ir_grow(e->items, &e->capacity, e->count, sizeof *e->items);
    e->items[e->count++] = t;
}

static void aggregate(struct text *out, const struct symbol *sym,
                      const struct interface *const *ifaces, size_t count,
                      struct emitted *done);
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

/* The export aggregate that type t holds by value, emitted first. */
static void emit_uses(struct text *out, const struct type *t,
                      const struct interface *const *ifaces, size_t count,
                      struct emitted *done)
{
    size_t i;
    size_t j;

    while (t->kind == TYPE_ARRAY) {
        t = t->element;
    }
    if (t->kind == TYPE_TUPLE) {
        emit_tuples(out, t, ifaces, count, done);
        return;
    }
    if (types_is_flags(t)) {
        flags_view(out, t, done);
        return;
    }
    if ((t->kind != TYPE_STRUCT && t->kind != TYPE_VARIANT) ||
        was_emitted(done, t)) {
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
   translation units agree on. What an element holds by value is written
   before it, so the definition stands complete. */
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
    for (i = 0; i < t->param_count; i++) {
        emit_uses(out, t->params[i], ifaces, count, done);
    }
    tuple_c_name(&tag, t);
    type_name(&written, t);
    text_appendf(out, "/* The tuple %s. */\nstruct %s {\n",
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
    int n = (int)t->name.length;
    size_t i;
    size_t j;

    for (i = 0; u != NULL && i < u->field_count; i++) {
        const struct type *payload = u->fields[i].type;
        for (j = 0; j < payload->field_count; j++) {
            emit_uses(out, payload->fields[j].type, ifaces, count, done);
        }
    }
    text_appendf(out, "/* The tags of %.*s. */\nenum %.*s_tag {\n", n,
                 t->name.text, n, t->name.text);
    for (i = 0; i < tag->field_count; i++) {
        struct text buffer = {0};
        c_name(&buffer, &tag->fields[i].name);
        doc_comment(out, &tag->fields[i].doc, "    ");
        text_appendf(out, "    %.*s_%s = %" PRIu64 "%s\n", n, t->name.text,
                     text_cstr(&buffer), tag->fields[i].number,
                     i + 1 < tag->field_count ? "," : "");
        text_free(&buffer);
    }
    text_append(out, "};\n\n");
    doc_comment(out, &sym->doc, "");
    if (t->packed) {
        text_append(out, "#pragma pack(push, 1)\n");
    }
    text_appendf(out, "typedef struct %.*s {\n    ", n, t->name.text);
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
    text_appendf(out, "} %.*s;\n", n, t->name.text);
    if (t->packed) {
        text_append(out, "#pragma pack(pop)\n");
    }
    text_append(out, "\n");
}

/* The vector type of C that a simd struct of 16 bytes is, one name per
   architecture. It is the NEON type on ARM64 and the SSE type on
   x86_64. The bits of f16 lanes cross as an integer vector, as a single
   f16 crosses as its sixteen bits. */
static void vector_typedef(struct text *out, const struct type *t)
{
    const struct type *lane = type_simd_lane(t);
    const char *neon = "uint8x16_t";
    const char *sse = "__m128i";

    switch (lane->kind) {
    case TYPE_F32: neon = "float32x4_t"; sse = "__m128"; break;
    case TYPE_F64: neon = "float64x2_t"; sse = "__m128d"; break;
    case TYPE_I8: neon = "int8x16_t"; break;
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

/* DESIGN: an enum becomes the C enum of its values, named as the type,
   and each value `T_Value`, as the tags of a variant are named. A field
   or a parameter of the type is its underlying integer, which
   declaration writes, because a C enum has the width of an int. */
static void enum_view(struct text *out, const struct type *t,
                      const struct doc_text *doc)
{
    struct text name = {0};
    bool is_signed = type_is_signed(t->base);
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
        if (is_signed) {
            text_appendf(out, "    %s_%s = %" PRId64 "%s\n", text_cstr(&name),
                         text_cstr(&buffer), (int64_t)t->fields[i].number,
                         i + 1 < t->field_count ? "," : "");
        } else {
            text_appendf(out, "    %s_%s = %" PRIu64 "%s\n", text_cstr(&name),
                         text_cstr(&buffer), t->fields[i].number,
                         i + 1 < t->field_count ? "," : "");
        }
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
            uint64_t bytes = type_simd_bytes(t);
            text_appendf(out, "ANTI_ALIGNAS(%" PRIu64 ") ",
                         bytes < 16 ? bytes : 16);
        } else if (i == 0 && t->align != 0) {
            text_appendf(out, "ANTI_ALIGNAS(%" PRIu64 ") ", t->align);
        }
        declaration(&field, t->fields[i].type,
                    type_field_is_unit_break(&t->fields[i])
                        ? ""
                        : text_cstr(&buffer),
                    t);
        text_append(out, text_cstr(&field));
        if (t->fields[i].bits != 0 || type_field_is_unit_break(&t->fields[i])) {
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
static void nested_views(struct text *out, const struct type *t)
{
    struct emitted seen = {0};
    struct emitted order = {0};
    size_t i;

    collect_nested(t, t, &seen, &order);
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
    if (t->simd && type_simd_bytes(t) == 16) {
        vector_typedef(out, t);
        return;
    }
    if (t->packed) {
        text_append(out, "#pragma pack(push, 1)\n");
    }
    text_appendf(out, "typedef %s %.*s {\n", kind, (int)t->name.length,
                 t->name.text);
    struct_fields(out, t);
    text_appendf(out, "} %.*s;\n", (int)t->name.length, t->name.text);
    if (t->packed) {
        text_append(out, "#pragma pack(pop)\n");
    }
    text_append(out, "\n");
}

/* The members of every level of the chain of t, which bounds the entries
   chain_functions writes. */
static size_t chain_members(const struct type *t)
{
    size_t count = 0;

    for (; t != NULL; t = t->kind == TYPE_CLASS ? t->base : NULL) {
        count += t->member_count;
    }
    return count;
}

/* The public functions of the chain of t, base first, in table order. A
   name that repeats replaces the entry it repeats, as the table does. A
   body qualified by an interface fills no entry here. One qualified by a
   base replaces a plain body of its level, as types_body_table says. out
   holds limit entries, and chain_members(t) of them hold every one. */
static size_t chain_functions(const struct type *t, const struct item **out,
                              size_t limit)
{
    size_t count = 0;
    int pass;
    size_t i;
    size_t j;

    if (t == NULL) {
        return 0;
    }
    if (t->kind == TYPE_CLASS) {
        count = chain_functions(t->base, out, limit);
    }
    for (pass = 0; pass < 2; pass++) {
        enum body_table fills = pass == 0 ? BODY_PLAIN : BODY_BASE;
        for (i = 0; i < t->member_count; i++) {
            const struct item *m = t->members[i];
            if (m->kind != ITEM_FN || !m->pub ||
                types_body_table(t, m) != fills) {
                continue;
            }
            for (j = 0; j < count; j++) {
                if (out[j]->name.length == m->name.length &&
                    memcmp(out[j]->name.text, m->name.text,
                           m->name.length) == 0) {
                    out[j] = m;
                    break;
                }
            }
            if (j == count && count < limit) {
                out[count++] = m;
            }
        }
    }
    return count;
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
   written out as the first parameter. */
static void member_signature(struct text *out, const struct type *owner,
                             const struct item *m, const char *prefix,
                             bool pointer)
{
    const struct type *t = m->symbol->type;
    struct text inner = {0};
    struct text decl = {0};
    size_t i;

    if (pointer) {
        text_appendf(&inner, "(*%.*s)(", (int)m->name.length, m->name.text);
    } else if (*prefix == '\0' && m->qualifier.length > 0 &&
               types_body_table(owner, m) == BODY_BASE) {
        /* A body qualified by a base has the symbol `T.Q.f`, so that
           a plain body of its name keeps `T.f`. */
        text_appendf(&inner, "%.*s_%.*s_%.*s(", (int)owner->name.length,
                     owner->name.text, (int)m->qualifier.length,
                     m->qualifier.text, (int)m->name.length, m->name.text);
    } else {
        text_appendf(&inner, "%s%.*s_%.*s(", prefix, (int)owner->name.length,
                     owner->name.text, (int)m->name.length, m->name.text);
    }
    for (i = 0; i < t->param_count; i++) {
        struct text param = {0};
        struct text buffer = {0};
        size_t k = i - (m->has_self ? 1 : 0);
        if (i == 0 && m->has_self) {
            text_appendf(&inner, "%.*s *self", (int)owner->name.length,
                         owner->name.text);
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
   class calls runs under the lock of its object, and the header says so
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

static void class_view(struct text *out, const struct symbol *sym)
{
    const struct type *t = sym->type;
    size_t limit = chain_members(t);
    const struct item **entries = ir_alloc(limit, sizeof *entries);
    int name_length = (int)t->name.length;
    const char *name_text = t->name.text;
    size_t count;
    struct text to_root = {0};
    const struct item *made;
    const struct type *up;
    size_t i;
    size_t j;

    count = chain_functions(t, entries, limit);
    /* The table pointer sits in the root, so a wrapper reaches it
       through one `base` per level of the chain. */
    for (up = t; up != NULL && up->base != NULL; up = up->base) {
        text_append(&to_root, "base.");
    }

    text_appendf(out, "typedef struct %.*s %.*s;\n", name_length, name_text,
                 name_length, name_text);
    nested_views(out, t);
    text_appendf(out, "typedef struct %.*s_vtable {\n"
                      "    const void *descriptor;\n"
                      "    /* The seven functions of anti.lang.Object. They "
                      "take and give Anti\n       values, so C reads their "
                      "slots and does not call them. */\n",
                 name_length, name_text);
    for (i = 0; i < count; i++) {
        if (entries[i]->runtime != NULL) {
            text_appendf(out, "    void *%.*s;\n",
                         (int)entries[i]->name.length, entries[i]->name.text);
            continue;
        }
        text_append(out, "    ");
        member_signature(out, t, entries[i], "", true);
        text_append(out, ";\n");
    }
    text_appendf(out, "} %.*s_vtable;\n\n", name_length, name_text);

    doc_comment(out, &sym->doc, "");
    text_appendf(out, "struct %.*s {\n", name_length, name_text);
    class_fields(out, t);
    text_appendf(out, "};\n\n");

    text_appendf(out, "extern const anti_descriptor anti_%.*s_descriptor;\n",
                 name_length, name_text);
    if (t->has_abstract) {
        text_appendf(out, "/* %.*s is abstract: no table and no init, "
                          "because it has no complete value. */\n",
                     name_length, name_text);
    } else {
        text_appendf(out, "extern const %.*s_vtable anti_%.*s_vtable;\n"
                          "void anti_%.*s_init(%.*s *self);\n",
                     name_length, name_text, name_length, name_text,
                     name_length, name_text, name_length, name_text);
        /* The counterpart of `Class(args)`, which prepares self as the
           init does and then runs `construct` with the arguments. */
        made = construct_with_arguments(t);
        if (made != NULL) {
            doc_comment(out, &made->doc, "");
            text_appendf(out, "/* Prepares self as anti_%.*s_init does, then "
                              "runs construct. */\n",
                         name_length, name_text);
            may_fail_note(out, made->symbol, "");
            member_signature(out, t, made, "anti_", false);
            text_append(out, ";\n");
        }
    }
    text_appendf(out,
                 "static inline void anti_%.*s_delete(%.*s *self)\n"
                 "{\n    anti_rt_delete(self, &anti_%.*s_descriptor);\n}\n"
                 "static inline void anti_%.*s_destroy(%.*s *self)\n"
                 "{\n    anti_rt_destroy(self, &anti_%.*s_descriptor);\n}\n"
                 "static inline %.*s *anti_%.*s_dup(%.*s *self)\n"
                 "{\n    return (%.*s *)anti_rt_dup(self, "
                 "&anti_%.*s_descriptor);\n}\n\n",
                 name_length, name_text, name_length, name_text,
                 name_length, name_text,
                 name_length, name_text, name_length, name_text,
                 name_length, name_text,
                 name_length, name_text, name_length, name_text,
                 name_length, name_text, name_length, name_text,
                 name_length, name_text);

    /* DESIGN: an interface sub-object is a field of the object, so C
       reaches the interface by taking its address. The table of that
       sub-object belongs to the class t and holds thunks. */
    for (up = t; up != NULL; up = up->kind == TYPE_CLASS ? up->base : NULL) {
        for (i = 0; i < up->field_count; i++) {
            const struct struct_field *f = &up->fields[i];
            struct text buffer = {0};
            if (f->form != FIELD_IMPL) {
                continue;
            }
            c_name(&buffer, &f->name);
            text_appendf(out,
                         "extern const %.*s_vtable anti_%.*s_%.*s_vtable;\n"
                         "static inline %.*s *anti_%.*s_as_%.*s(%.*s *self)\n"
                         "{\n    return &self->%s%s;\n}\n",
                         (int)f->type->name.length, f->type->name.text,
                         name_length, name_text,
                         (int)f->type->name.length, f->type->name.text,
                         (int)f->type->name.length, f->type->name.text,
                         name_length, name_text,
                         (int)f->type->name.length, f->type->name.text,
                         name_length, name_text,
                         up == t ? "" : "base.", text_cstr(&buffer));
            text_free(&buffer);
        }
    }
    /* DESIGN: each prototype names a symbol the library holds, so a C
       call to it links. The section of the class that declares a function
       declares it once, and a class below does not repeat it. An abstract
       entry has no body and no prototype, and its wrapper carries its
       comment. */
    for (i = 0; i < count; i++) {
        if (entries[i]->runtime != NULL ||
            entries[i]->contract == FN_ABSTRACT ||
            types_member_level(t, entries[i]) != t) {
            continue;
        }
        doc_comment(out, &entries[i]->doc, "");
        may_fail_note(out, entries[i]->symbol, "");
        locked_note(out, t, entries[i]);
        member_signature(out, t, entries[i], "", false);
        text_append(out, ";\n");
    }
    text_append(out, "\n");
    /* A wrapper per entry, which reads the table of the object. */
    for (i = 0; i < count; i++) {
        if (entries[i]->runtime != NULL) {
            continue;
        }
        if (entries[i]->contract == FN_ABSTRACT) {
            doc_comment(out, &entries[i]->doc, "");
        }
        text_append(out, "static inline ");
        member_signature(out, t, entries[i], "anti_", false);
        text_appendf(out,
                     "\n{\n    %s ((const %.*s_vtable *)self->%svtable)"
                     "->%.*s(self",
                     entries[i]->symbol->type->result->kind == TYPE_VOID
                         ? ""
                         : "return",
                     name_length, name_text, text_cstr(&to_root),
                     (int)entries[i]->name.length, entries[i]->name.text);
        for (j = 1; j < entries[i]->symbol->type->param_count; j++) {
            struct text buffer = {0};
            size_t k = j - 1;
            if (entries[i]->symbol->params != NULL &&
                k < entries[i]->param_count) {
                c_name(&buffer, &entries[i]->symbol->params[k]);
            } else if (entries[i]->params != NULL &&
                       k < entries[i]->param_count) {
                c_name(&buffer, &entries[i]->params[k].name);
            } else {
                text_appendf(&buffer, "a%zu", k);
            }
            text_appendf(out, ", %s", text_cstr(&buffer));
            text_free(&buffer);
        }
        text_append(out, ");\n}\n");
    }
    text_append(out, "\n");
    text_free(&to_root);
    free(entries);
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
    /* A whole value gets `.0`, since C reads `2` as an int and refuses
       `2f`. */
    case CONST_FLOAT: {
        struct text digits = {0};
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
        text_appendf(out, "#define %.*s ((%s)%" PRId64 ")\n",
                     (int)sym->name.length, sym->name.text, scalar_name(t),
                     (int64_t)v->as.integer);
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

void header_write(struct text *out, const char *name,
                  const struct interface *const *ifaces, size_t count,
                  bool bundled)
{
    struct emitted done = {0};
    size_t i;
    size_t j;
    bool any;

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
    text_append(out, "\n\n"
                     "#include <stdbool.h>\n"
                     "#include <stddef.h>\n"
                     "#include <stdint.h>\n\n"
                     "#ifdef __cplusplus\n"
                     "#define ANTI_ALIGNAS(n) alignas(n)\n"
                     "extern \"C\" {\n"
                     "#else\n"
                     "#define ANTI_ALIGNAS(n) _Alignas(n)\n"
                     "#endif\n\n");
    /* The vector types of C, which a simd struct of 16 bytes is. Each
       architecture has its own header for them. */
    for (i = 0; i < count; i++) {
        for (j = 0; j < ifaces[i]->item_count; j++) {
            const struct symbol *sym = ifaces[i]->items[j];
            if (sym->exported && sym->kind == SYMBOL_STRUCT &&
                type_is_simd(sym->type) &&
                type_simd_bytes(sym->type) == 16) {
                text_append(out,
                    "#if defined(__aarch64__) || defined(_M_ARM64)\n"
                    "#include <arm_neon.h>\n"
                    "#else\n"
                    "#include <immintrin.h>\n"
                    "#endif\n\n");
                i = count;
                break;
            }
        }
    }
    /* The error class, declared once when an exported signature names
       it. A C caller passes the pointer on and never reads it. */
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
                i = count;
                break;
            }
        }
    }
    /* The root of every class chain, which the base of a class nests. */
    for (i = 0; i < count; i++) {
        for (j = 0; j < ifaces[i]->item_count; j++) {
            const struct symbol *sym = ifaces[i]->items[j];
            if (sym->exported && sym->kind == SYMBOL_STRUCT &&
                sym->type->kind == TYPE_CLASS) {
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
                i = count;
                break;
            }
        }
    }
    for (i = 0; i < count; i++) {
        for (j = 0; j < ifaces[i]->item_count; j++) {
            const struct symbol *sym = ifaces[i]->items[j];
            if (sym->exported && sym->kind == SYMBOL_STRUCT) {
                if (sym->type->kind == TYPE_CLASS) {
                    class_view(out, sym);
                } else {
                    aggregate(out, sym, ifaces, count, &done);
                }
            }
        }
    }
    /* One struct per distinct tuple of an exported signature, after the
       aggregates an element may hold by value. */
    for (i = 0; i < count; i++) {
        for (j = 0; j < ifaces[i]->item_count; j++) {
            const struct symbol *sym = ifaces[i]->items[j];
            size_t k;
            if (!sym->exported || sym->kind != SYMBOL_FN) {
                continue;
            }
            for (k = 0; k < sym->type->param_count; k++) {
                emit_tuples(out, sym->type->params[k], ifaces, count, &done);
            }
            emit_tuples(out, sym->type->result, ifaces, count, &done);
        }
    }
    for (i = 0, any = false; i < count; i++) {
        for (j = 0; j < ifaces[i]->item_count; j++) {
            const struct symbol *sym = ifaces[i]->items[j];
            if (sym->exported && sym->kind == SYMBOL_CONST) {
                constant(out, sym);
                any = true;
            }
        }
    }
    text_append(out, any ? "\n" : "");
    for (i = 0, any = false; i < count; i++) {
        for (j = 0; j < ifaces[i]->item_count; j++) {
            const struct symbol *sym = ifaces[i]->items[j];
            if (sym->exported && sym->kind == SYMBOL_FN) {
                prototype(out, sym);
                any = true;
            }
        }
    }
    text_append(out, any ? "\n" : "");
    text_append(out, "#ifdef __cplusplus\n}\n#endif\n\n#endif\n");
    free((void *)done.items);
}

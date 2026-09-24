/* The pieces of anti bind that both readers share. They are the lists and
   texts of a binding, the fixed mappings of the C types and the parser of
   a C type spelling. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bindmodel.h"
#include "diagnostic.h"
#include "files.h"
#include "lexer.h"

void bind_list_add(struct bind_list *list, void *item)
{
    if (list->count == list->room) {
        size_t room = list->room == 0 ? 16 : list->room * 2;
        void **items = realloc(list->items, room * sizeof *items);
        if (items == NULL) {
            fputs("anti: out of memory\n", stderr);
            exit(70);
        }
        list->items = items;
        list->room = room;
    }
    list->items[list->count++] = item;
}

const char *bind_strndup(struct bind_module *b, const char *s, size_t n)
{
    char *copy = arena_alloc(&b->arena, n + 1);

    memcpy(copy, s, n);
    return copy;
}

const char *bind_strdup(struct bind_module *b, const char *s)
{
    return bind_strndup(b, s, strlen(s));
}

void bind_warn(struct bind_module *b, const char *format, ...)
{
    va_list args;

    fprintf(stderr, "anti: %s: warning: ", b->source);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
    b->warnings++;
}

struct bind_type *bind_type_new(struct bind_module *b, enum bind_kind kind)
{
    struct bind_type *t = arena_alloc(&b->arena, sizeof *t);

    t->kind = kind;
    return t;
}

static const struct bind_type *scalar(struct bind_module *b, const char *name)
{
    struct bind_type *t = bind_type_new(b, BIND_SCALAR);

    t->name = name;
    return t;
}

/* DESIGN: the typedefs of the C library that a header names stand for the
   Anti type of their meaning, never for the type the typedef has on the
   machine that ran clang. `int64_t` is `long` on Linux and `long long`
   on macOS, and both are `i64`. The six targets are 64-bit, so the
   pointer-sized integers are `i64` and `u64`. */
static const struct {
    const char *c;
    const char *anti;
} known[] = {
    {"size_t", "c_size_t"},   {"wchar_t", "c_wchar"},
    {"int8_t", "i8"},         {"int16_t", "i16"},
    {"int32_t", "i32"},       {"int64_t", "i64"},
    {"uint8_t", "u8"},        {"uint16_t", "u16"},
    {"uint32_t", "u32"},      {"uint64_t", "u64"},
    {"intptr_t", "i64"},      {"uintptr_t", "u64"},
    {"ptrdiff_t", "i64"},     {"ssize_t", "i64"},
    {"intmax_t", "i64"},      {"uintmax_t", "u64"},
    {"char16_t", "u16"},      {"char32_t", "u32"},
};

const struct bind_type *bind_known_name(struct bind_module *b, const char *name)
{
    size_t i;

    for (i = 0; i < sizeof known / sizeof known[0]; i++) {
        if (strcmp(known[i].c, name) == 0) {
            return scalar(b, known[i].anti);
        }
    }
    if (strcmp(name, "bool") == 0 || strcmp(name, "_Bool") == 0) {
        return bind_type_new(b, BIND_BOOL);
    }
    if (strcmp(name, "va_list") == 0 || strcmp(name, "__builtin_va_list") == 0 ||
        strcmp(name, "__gnuc_va_list") == 0) {
        return bind_type_new(b, BIND_VALIST);
    }
    return NULL;
}

bool bind_is_keyword(const char *name)
{
    struct arena arena = {0};
    struct diagnostics diags = {0};
    struct token_list tokens = {0};
    bool word;

    /* A name is free when the lexer reads it as one identifier. That
       covers the keywords, the reserved words and `null`. */
    word = lex(name, strlen(name), &arena, &diags, &tokens) &&
           tokens.count >= 1 && tokens.items[0].kind == TOKEN_IDENT &&
           (tokens.count == 1 || tokens.items[1].kind == TOKEN_EOF);
    token_list_free(&tokens);
    diagnostics_free(&diags);
    arena_free(&arena);
    return !word;
}

const char *bind_last_segment(const char *module)
{
    const char *dot = strrchr(module, '.');

    return dot != NULL ? dot + 1 : module;
}

/* DESIGN: the frameworks of Apple's SDK that each bundled library needs
   on macOS, which the binding names with `link framework`. raylib opens
   its window through GLFW over Cocoa and draws with OpenGL. miniaudio
   plays through Core Audio. A library outside the table names none. */
static const char *const raylib_frameworks[] = {
    "Cocoa", "CoreVideo", "IOKit", "OpenGL"
};
static const char *const miniaudio_frameworks[] = {
    "AudioToolbox", "CoreAudio", "CoreFoundation"
};

size_t bind_frameworks(const char *library, const char *const **names)
{
    if (strcmp(library, "raylib") == 0) {
        *names = raylib_frameworks;
        return sizeof raylib_frameworks / sizeof raylib_frameworks[0];
    }
    if (strcmp(library, "miniaudio") == 0) {
        *names = miniaudio_frameworks;
        return sizeof miniaudio_frameworks / sizeof miniaudio_frameworks[0];
    }
    *names = NULL;
    return 0;
}

/* The parser of a type spelling. */

enum type_token { T_WORD, T_NUMBER, T_STAR, T_OPEN, T_CLOSE, T_LBRACKET,
             T_RBRACKET, T_COMMA, T_DOTS, T_END };

struct parser {
    struct bind_module *b;
    const struct bind_names *names;
    const char *text;
    size_t pos;
    enum type_token kind;
    const char *word;           /* of the current token */
    size_t word_length;
};

/* DESIGN: the parser recurses once per array, parameter list and group
   of a declarator, and once per typedef a name leads it into. Every
   pointer nests the type the writer then recurses into. A spelling nested
   deeper than this bound, the parses of its typedefs counted, does not
   parse. One level takes about 1.3 KB of stack. The bound keeps under
   half of the 1 MB that Windows gives a main thread, and no header comes
   near it. */
#define TYPE_DEPTH 256

/* The levels of every parse in progress. It is global because the parse
   of a typedef starts in a callback of a reader, which cannot hand the
   count on. anti bind runs on one thread. */
static int nesting;

/* Enter one level of the type, or refuse at the bound. */
static bool deeper(void)
{
    if (nesting >= TYPE_DEPTH) {
        return false;
    }
    nesting++;
    return true;
}

static void next(struct parser *p)
{
    const char *s = p->text;
    size_t i;

    while (s[p->pos] == ' ') {
        p->pos++;
    }
    i = p->pos;
    p->word = s + i;
    p->word_length = 1;
    switch (s[i]) {
    case '\0':
        p->kind = T_END;
        p->word_length = 0;
        return;
    case '*': p->kind = T_STAR; break;
    case '(': p->kind = T_OPEN; break;
    case ')': p->kind = T_CLOSE; break;
    case '[': p->kind = T_LBRACKET; break;
    case ']': p->kind = T_RBRACKET; break;
    case ',': p->kind = T_COMMA; break;
    case '.':
        if (strncmp(s + i, "...", 3) != 0) {
            p->kind = T_END;
            p->word = NULL;
            p->word_length = 0;
            return;
        }
        p->kind = T_DOTS;
        p->word_length = 3;
        break;
    default:
        if ((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= 'A' && s[i] <= 'Z') ||
            s[i] == '_') {
            size_t j = i;
            while ((s[j] >= 'a' && s[j] <= 'z') || (s[j] >= 'A' && s[j] <= 'Z') ||
                   (s[j] >= '0' && s[j] <= '9') || s[j] == '_') {
                j++;
            }
            p->kind = T_WORD;
            p->word_length = j - i;
        } else if (s[i] >= '0' && s[i] <= '9') {
            size_t j = i;
            while ((s[j] >= '0' && s[j] <= '9') || s[j] == 'x' || s[j] == 'X' ||
                   (s[j] >= 'a' && s[j] <= 'f') || (s[j] >= 'A' && s[j] <= 'F') ||
                   s[j] == 'u' || s[j] == 'U' || s[j] == 'l' || s[j] == 'L') {
                j++;
            }
            p->kind = T_NUMBER;
            p->word_length = j - i;
        } else {
            /* A character no spelling of a type holds. */
            p->kind = T_END;
            p->word = NULL;
            p->word_length = 0;
            return;
        }
        break;
    }
    p->pos = i + p->word_length;
}

static bool is_word(const struct parser *p, const char *w)
{
    return p->kind == T_WORD && strlen(w) == p->word_length &&
           memcmp(p->word, w, p->word_length) == 0;
}

static bool is_qualifier(const struct parser *p)
{
    static const char *const words[] = {
        "const", "volatile", "restrict", "__restrict", "__restrict__",
        "_Nonnull", "_Nullable", "_Null_unspecified", "__unaligned",
    };
    size_t i;

    for (i = 0; i < sizeof words / sizeof words[0]; i++) {
        if (is_word(p, words[i])) {
            return true;
        }
    }
    return false;
}

static const struct bind_type *unsupported(struct parser *p)
{
    struct bind_type *t = bind_type_new(p->b, BIND_UNSUPPORTED);

    t->name = bind_strdup(p->b, p->text);
    return t;
}

/* The words of a builtin type, counted, as C lets them come in any
   order. */
struct words {
    int longs;
    bool is_signed, is_unsigned, is_char, is_short, is_int, is_float,
        is_double, is_void, is_bool, other;
};

static const struct bind_type *builtin(struct parser *p, const struct words *w)
{
    struct bind_module *b = p->b;

    if (w->other) {
        return unsupported(p);
    }
    if (w->is_void) {
        return bind_type_new(b, BIND_VOID);
    }
    if (w->is_bool) {
        return bind_type_new(b, BIND_BOOL);
    }
    if (w->is_float) {
        return scalar(b, "c_float");
    }
    if (w->is_double) {
        return w->longs > 0 ? unsupported(p) : scalar(b, "c_double");
    }
    if (w->is_char) {
        return scalar(b, w->is_unsigned ? "c_uchar" : "c_char");
    }
    if (w->is_short) {
        return scalar(b, w->is_unsigned ? "c_ushort" : "c_short");
    }
    if (w->longs == 1) {
        return scalar(b, w->is_unsigned ? "c_ulong" : "c_long");
    }
    if (w->longs == 2) {
        return scalar(b, w->is_unsigned ? "c_ulonglong" : "c_longlong");
    }
    if (w->is_int || w->is_signed || w->is_unsigned) {
        return scalar(b, w->is_unsigned ? "c_uint" : "c_int");
    }
    return NULL;
}

static const struct bind_type *parse_type(struct parser *p);

/* The base type of the specifiers, up to the first token of the
   declarator. */
static const struct bind_type *specifiers(struct parser *p)
{
    struct words w;
    const struct bind_type *named = NULL;
    bool any = false;

    memset(&w, 0, sizeof w);
    while (p->kind == T_WORD) {
        char word[128];
        if (is_qualifier(p)) {
            next(p);
            continue;
        }
        if (p->word_length >= sizeof word) {
            return NULL;
        }
        memcpy(word, p->word, p->word_length);
        word[p->word_length] = '\0';
        if (strcmp(word, "struct") == 0 || strcmp(word, "union") == 0 ||
            strcmp(word, "enum") == 0) {
            char tag[128];
            next(p);
            if (p->kind != T_WORD || p->word_length >= sizeof tag || any) {
                return NULL;
            }
            memcpy(tag, p->word, p->word_length);
            tag[p->word_length] = '\0';
            next(p);
            if (word[0] == 'e') {
                const char *name = p->names->enum_named(p->names->context, tag);
                struct bind_type *t;
                if (name == NULL) {
                    return unsupported(p);
                }
                t = bind_type_new(p->b, BIND_ENUM);
                t->name = name;
                named = t;
            } else {
                struct bind_record *r = p->names->record_named(
                    p->names->context, tag, word[0] == 'u');
                struct bind_type *t;
                if (r == NULL || !r->complete) {
                    t = bind_type_new(p->b, BIND_OPAQUE);
                    t->name = bind_strdup(p->b, tag);
                } else {
                    t = bind_type_new(p->b, BIND_RECORD);
                    t->name = r->name;
                    t->record = r;
                }
                named = t;
            }
            any = true;
            continue;
        }
        if (strcmp(word, "long") == 0) {
            w.longs++;
        } else if (strcmp(word, "signed") == 0) {
            w.is_signed = true;
        } else if (strcmp(word, "unsigned") == 0) {
            w.is_unsigned = true;
        } else if (strcmp(word, "char") == 0) {
            w.is_char = true;
        } else if (strcmp(word, "short") == 0) {
            w.is_short = true;
        } else if (strcmp(word, "int") == 0) {
            w.is_int = true;
        } else if (strcmp(word, "float") == 0) {
            w.is_float = true;
        } else if (strcmp(word, "double") == 0) {
            w.is_double = true;
        } else if (strcmp(word, "void") == 0) {
            w.is_void = true;
        } else if (strcmp(word, "_Bool") == 0) {
            w.is_bool = true;
        } else if (strcmp(word, "_Complex") == 0 || strcmp(word, "__int128") == 0 ||
                   strcmp(word, "_Atomic") == 0 || strcmp(word, "__attribute__") == 0) {
            w.other = true;
        } else if (!any && named == NULL) {
            /* A typedef name, which stands alone among the specifiers. */
            const struct bind_type *t = bind_known_name(p->b, word);
            if (t == NULL) {
                t = p->names->typedef_named(p->names->context, word);
            }
            if (t == NULL) {
                struct bind_type *u = bind_type_new(p->b, BIND_UNSUPPORTED);
                u->name = bind_strdup(p->b, word);
                t = u;
            }
            named = t;
            next(p);
            break;
        } else {
            return NULL;
        }
        any = true;
        next(p);
    }
    while (is_qualifier(p)) {
        next(p);
    }
    if (named != NULL) {
        return any && named->kind != BIND_RECORD && named->kind != BIND_ENUM &&
                       named->kind != BIND_OPAQUE
                   ? NULL
                   : named;
    }
    return any ? builtin(p, &w) : NULL;
}

static const struct bind_type *pointer_to(struct parser *p,
                                          const struct bind_type *to)
{
    struct bind_type *t = bind_type_new(p->b, BIND_POINTER);

    t->to = to;
    return t;
}

/* The parameter list after `(`, up to and with its `)`. */
static const struct bind_type *function_of(struct parser *p,
                                           const struct bind_type *result)
{
    struct bind_type *t = bind_type_new(p->b, BIND_FUNCTION);
    const struct bind_type *params[64];
    size_t count = 0;

    if (!deeper()) {
        return NULL;
    }
    t->to = result;
    next(p);
    if (p->kind == T_CLOSE) {
        next(p);
        nesting--;
        return t;
    }
    for (;;) {
        const struct bind_type *param;
        if (p->kind == T_DOTS) {
            t->variadic = true;
            next(p);
            break;
        }
        param = parse_type(p);
        if (param == NULL || count == sizeof params / sizeof params[0]) {
            return NULL;
        }
        params[count++] = param;
        if (p->kind != T_COMMA) {
            break;
        }
        next(p);
    }
    if (p->kind != T_CLOSE) {
        return NULL;
    }
    next(p);
    /* `(void)` takes no parameter. */
    if (count == 1 && params[0]->kind == BIND_VOID && !t->variadic) {
        count = 0;
    }
    if (count > 0) {
        const struct bind_type **kept =
            arena_alloc(&p->b->arena, count * sizeof *kept);
        memcpy((void *)kept, (void *)params, count * sizeof *kept);
        t->params = kept;
        t->param_count = count;
    }
    nesting--;
    return t;
}

/* The array and function suffixes after a declarator, applied to base in
   the order of C: `[2][3]` is an array of 2 arrays of 3. */
static const struct bind_type *suffixes(struct parser *p,
                                        const struct bind_type *base)
{
    if (p->kind == T_LBRACKET) {
        struct bind_type *t = bind_type_new(p->b, BIND_ARRAY);
        const struct bind_type *element;
        if (!deeper()) {
            return NULL;
        }
        t->length = -1;
        next(p);
        if (p->kind == T_NUMBER) {
            char digits[32];
            if (p->word_length >= sizeof digits) {
                return NULL;
            }
            memcpy(digits, p->word, p->word_length);
            digits[p->word_length] = '\0';
            t->length = strtoll(digits, NULL, 0);
            next(p);
        }
        if (p->kind != T_RBRACKET) {
            return NULL;
        }
        next(p);
        element = suffixes(p, base);
        if (element == NULL) {
            return NULL;
        }
        t->to = element;
        nesting--;
        return t;
    }
    if (p->kind == T_OPEN) {
        return function_of(p, base);
    }
    return base;
}

/* The end of the group that starts at the `(` of p, as the position
   after its `)`, or 0. A group nested deeper than the bound has no end,
   so the scan stops there. */
static size_t group_end(const struct parser *p)
{
    struct parser scan = *p;
    int depth = 0;

    do {
        if (scan.kind == T_OPEN) {
            depth++;
            if (depth > TYPE_DEPTH) {
                return 0;
            }
        } else if (scan.kind == T_CLOSE) {
            depth--;
        } else if (scan.kind == T_END) {
            return 0;
        }
        next(&scan);
    } while (depth > 0);
    return scan.pos - scan.word_length;
}

static const struct bind_type *declarator(struct parser *p,
                                          const struct bind_type *base)
{
    int depth = nesting;
    const struct bind_type *result;

    while (p->kind == T_STAR) {
        if (!deeper()) {
            return NULL;
        }
        next(p);
        while (is_qualifier(p)) {
            next(p);
        }
        base = pointer_to(p, base);
    }
    if (p->kind == T_OPEN) {
        /* A `(` before `*` or `(` groups a declarator, as in
           `void (*)(int)`. Its suffixes bind first. */
        struct parser look = *p;
        next(&look);
        if (look.kind == T_STAR || look.kind == T_OPEN) {
            struct parser inner;
            struct parser after = *p;
            const struct bind_type *outer;
            size_t end;
            if (!deeper() || (end = group_end(p)) == 0) {
                return NULL;
            }
            inner = look;
            after.pos = end;
            next(&after);
            outer = suffixes(&after, base);
            if (outer == NULL) {
                return NULL;
            }
            result = declarator(&inner, outer);
            if (result == NULL || inner.kind != T_CLOSE) {
                return NULL;
            }
            *p = after;
            nesting = depth;
            return result;
        }
    }
    result = suffixes(p, base);
    nesting = depth;
    return result;
}

static const struct bind_type *parse_type(struct parser *p)
{
    const struct bind_type *base = specifiers(p);

    if (base == NULL) {
        return NULL;
    }
    /* A parameter may carry a name in a spelling of raylib_api.json.
       clang prints none. */
    return declarator(p, base);
}

const struct bind_type *bind_parse_type(struct bind_module *b, const char *c,
                                        const struct bind_names *names)
{
    struct parser p;
    const struct bind_type *t;
    int depth = nesting;

    if (!deeper()) {
        return NULL;
    }
    memset(&p, 0, sizeof p);
    p.b = b;
    p.names = names;
    p.text = c;
    next(&p);
    t = parse_type(&p);
    /* A parse that failed inside leaves its levels counted. */
    nesting = depth;
    if (t == NULL || p.kind != T_END || p.word == NULL) {
        return NULL;
    }
    return t;
}

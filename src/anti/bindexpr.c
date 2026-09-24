/* The evaluator of the constant expressions of a binding. It gives the
   value of an object-like macro of a header and of a define of
   raylib_api.json. */
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bindexpr.h"

/* DESIGN: the evaluator recurses once per prefix operator, cast and
   parenthesis. It recurses once more per macro a name leads it into,
   which the lookup of a reader evaluates with a bind_eval of its own. An expression nested deeper than this bound, every evaluation
   counted, fails as one the evaluator does not know does. One level takes
   about 3.5 KB of stack. The bound keeps under half of the 1 MB that
   Windows gives a main thread, and no header comes near it. */
#define EVAL_DEPTH 128

/* The levels of every evaluation in progress. It is global because a
   nested bind_eval starts in the lookup of a reader, which cannot hand
   the count on. anti bind runs on one thread. */
static int nesting;

struct eval {
    struct bind_module *b;
    const char *s;
    size_t pos;
    bind_lookup lookup;
    void *context;
    bool failed;
};

static void space(struct eval *e)
{
    while (e->s[e->pos] == ' ' || e->s[e->pos] == '\t') {
        e->pos++;
    }
}

static bool take(struct eval *e, const char *op)
{
    size_t n = strlen(op);

    space(e);
    if (strncmp(e->s + e->pos, op, n) != 0) {
        return false;
    }
    /* `<` is not the start of `<<`, and `&` not of `&&`. */
    if (n == 1 && (op[0] == '<' || op[0] == '>' || op[0] == '&' ||
                   op[0] == '|') &&
        e->s[e->pos + 1] == op[0]) {
        return false;
    }
    e->pos += n;
    return true;
}

static bool is_float_type(const char *type)
{
    return strcmp(type, "c_float") == 0 || strcmp(type, "c_double") == 0;
}

static bool is_unsigned_type(const char *type)
{
    return type[0] == 'u' || strncmp(type, "c_u", 3) == 0 ||
           strcmp(type, "c_size_t") == 0;
}

static void fail(struct eval *e)
{
    e->failed = true;
}

/* Enter one level of nesting, or fail at the bound. A true result is
   followed by one call of leave. */
static bool enter(struct eval *e)
{
    if (nesting >= EVAL_DEPTH) {
        fail(e);
        return false;
    }
    nesting++;
    return true;
}

static void leave(void)
{
    nesting--;
}

int64_t bind_signed(uint64_t x)
{
    if (x <= (uint64_t)INT64_MAX) {
        return (int64_t)x;
    }
    return -(int64_t)(UINT64_MAX - x) - 1;
}

/* The low bits of x as a signed number of that width. */
static int64_t sign_extended(uint64_t x, unsigned bits)
{
    uint64_t sign = UINT64_C(1) << (bits - 1);
    uint64_t low = x & ((sign << 1) - 1);

    return bind_signed((low ^ sign) - sign);
}

/* An integer wraps at the width of its type, as C computes it. c_long
   and the wider types keep 64 bits. */
static void narrow(struct bind_eval *v)
{
    uint64_t x = (uint64_t)v->i;

    if (v->kind != BIND_EVAL_INT) {
        return;
    }
    if (strcmp(v->type, "c_int") == 0) {
        v->i = sign_extended(x, 32);
    } else if (strcmp(v->type, "c_uint") == 0) {
        v->i = (int64_t)(uint32_t)x;
    } else if (strcmp(v->type, "c_short") == 0) {
        v->i = sign_extended(x, 16);
    } else if (strcmp(v->type, "c_ushort") == 0) {
        v->i = (int64_t)(uint16_t)x;
    } else if (strcmp(v->type, "c_char") == 0) {
        v->i = sign_extended(x, 8);
    } else if (strcmp(v->type, "c_uchar") == 0) {
        v->i = (int64_t)(uint8_t)x;
    }
}

/* A numeric literal of C: a decimal, hexadecimal or octal integer with
   the suffixes u and l, or a decimal float with the suffix f. */
static struct bind_eval number(struct eval *e)
{
    struct bind_eval v;
    const char *start = e->s + e->pos;
    bool hex = start[0] == '0' && (start[1] == 'x' || start[1] == 'X');
    char *end;
    size_t n = 0;

    memset(&v, 0, sizeof v);
    while (!hex && start[n] >= '0' && start[n] <= '9') {
        n++;
    }
    if (!hex && (start[n] == '.' || start[n] == 'e' || start[n] == 'E')) {
        v.kind = BIND_EVAL_FLOAT;
        v.type = "c_double";
        errno = 0;
        v.f = strtod(start, &end);
        e->pos += (size_t)(end - start);
        /* A literal past the largest double is refused. One below the
           smallest reads as zero, or nearly so, as C reads it. */
        if (errno == ERANGE && v.f > DBL_MAX) {
            fail(e);
            v.f = 0.0;
        }
        if (e->s[e->pos] == 'f' || e->s[e->pos] == 'F') {
            v.type = "c_float";
            /* A double past the largest float has no float value in C. */
            if (v.f > FLT_MAX) {
                fail(e);
                v.f = 0.0;
            }
            v.f = (double)(float)v.f;
            e->pos++;
        } else if (e->s[e->pos] == 'l' || e->s[e->pos] == 'L') {
            /* A long double has no Anti type. */
            fail(e);
        }
        return v;
    }
    {
        unsigned long long u;
        int longs = 0;
        bool is_unsigned = false;
        errno = 0;
        u = strtoull(start, &end, 0);
        e->pos += (size_t)(end - start);
        /* A literal past 2^64 - 1 is refused, not taken as the largest
           value strtoull saturates to. */
        if (errno == ERANGE) {
            fail(e);
            u = 0;
        }
        for (;;) {
            char c = e->s[e->pos];
            if (c == 'u' || c == 'U') {
                is_unsigned = true;
            } else if (c == 'l' || c == 'L') {
                longs++;
            } else {
                break;
            }
            e->pos++;
        }
        v.kind = BIND_EVAL_INT;
        v.i = bind_signed(u);
        if (u > (unsigned long long)INT64_MAX) {
            is_unsigned = true;
        }
        if (longs == 2 || u > 0xFFFFFFFFull ||
            (!is_unsigned && !hex && u > 0x7FFFFFFFull)) {
            v.type = is_unsigned ? "c_ulonglong" : "c_longlong";
        } else if (longs == 1) {
            v.type = is_unsigned ? "c_ulong" : "c_long";
        } else if (hex && u > 0x7FFFFFFFull) {
            /* A hexadecimal constant that int cannot hold is unsigned
               int in C. */
            v.type = "c_uint";
        } else {
            v.type = is_unsigned ? "c_uint" : "c_int";
        }
    }
    return v;
}

/* A string literal of C, with adjacent literals joined. */
static struct bind_eval string(struct eval *e)
{
    struct bind_eval v;
    struct text bytes = {0};

    memset(&v, 0, sizeof v);
    v.kind = BIND_EVAL_STRING;
    v.type = "str";
    while (e->s[e->pos] == '"') {
        e->pos++;
        while (e->s[e->pos] != '"') {
            char c = e->s[e->pos];
            if (c == '\0') {
                fail(e);
                text_free(&bytes);
                return v;
            }
            if (c == '\\') {
                char d = e->s[e->pos + 1];
                e->pos += 2;
                switch (d) {
                case 'n': c = '\n'; break;
                case 't': c = '\t'; break;
                case 'r': c = '\r'; break;
                case '\\': c = '\\'; break;
                case '"': c = '"'; break;
                case '\'': c = '\''; break;
                default:
                    fail(e);
                    text_free(&bytes);
                    return v;
                }
                text_append_bytes(&bytes, &c, 1);
                continue;
            }
            text_append_bytes(&bytes, &c, 1);
            e->pos++;
        }
        e->pos++;
        space(e);
    }
    v.text = bind_strndup(e->b, text_cstr(&bytes), bytes.length);
    text_free(&bytes);
    return v;
}

/* The words of a cast that the evaluator knows, as `(float)` or
   `(unsigned int)`, with the type the value takes. */
static const char *cast_type(const char *words)
{
    static const struct {
        const char *c;
        const char *anti;
    } casts[] = {
        {"float", "c_float"},          {"double", "c_double"},
        {"int", "c_int"},              {"unsigned int", "c_uint"},
        {"unsigned", "c_uint"},        {"long", "c_long"},
        {"unsigned long", "c_ulong"},  {"long long", "c_longlong"},
        {"unsigned long long", "c_ulonglong"},
        {"short", "c_short"},          {"unsigned short", "c_ushort"},
        {"char", "c_char"},            {"unsigned char", "c_uchar"},
    };
    size_t i;

    for (i = 0; i < sizeof casts / sizeof casts[0]; i++) {
        if (strcmp(casts[i].c, words) == 0) {
            return casts[i].anti;
        }
    }
    return NULL;
}

static struct bind_eval conditional(struct eval *e);

/* v in the type, as a cast of C converts it. A float that no int64_t
   holds has no value in C, and the expression fails. */
static struct bind_eval converted(struct eval *e, struct bind_eval v,
                                  const char *type)
{
    if (is_float_type(type) && v.kind == BIND_EVAL_INT) {
        v.f = is_unsigned_type(v.type) ? (double)(uint64_t)v.i : (double)v.i;
    } else if (!is_float_type(type) && v.kind == BIND_EVAL_FLOAT) {
        if (!(v.f > -9223372036854775808.0 && v.f < 9223372036854775808.0)) {
            fail(e);
            v.f = 0.0;
        }
        v.i = (int64_t)v.f;
    }
    v.kind = is_float_type(type) ? BIND_EVAL_FLOAT : BIND_EVAL_INT;
    v.type = type;
    v.enum_type = NULL;
    narrow(&v);
    return v;
}

static struct bind_eval primary(struct eval *e)
{
    struct bind_eval v;
    char c;

    /* A value that fails still has a type, which narrow reads. */
    memset(&v, 0, sizeof v);
    v.type = "c_int";
    space(e);
    c = e->s[e->pos];
    if (c == '(') {
        /* A cast, or an expression in parentheses. */
        const char *close = strchr(e->s + e->pos, ')');
        if (close != NULL) {
            char words[64];
            size_t n = (size_t)(close - (e->s + e->pos + 1));
            const char *type = NULL;
            if (n < sizeof words) {
                memcpy(words, e->s + e->pos + 1, n);
                words[n] = '\0';
                type = cast_type(words);
            }
            if (type != NULL) {
                e->pos += n + 2;
                if (!enter(e)) {
                    return v;
                }
                v = primary(e);
                leave();
                if (v.kind == BIND_EVAL_STRING) {
                    fail(e);
                }
                return converted(e, v, type);
            }
        }
        e->pos++;
        if (!enter(e)) {
            return v;
        }
        v = conditional(e);
        leave();
        if (!take(e, ")")) {
            fail(e);
        }
        return v;
    }
    if (c == '"') {
        return string(e);
    }
    if ((c >= '0' && c <= '9') || c == '.') {
        return number(e);
    }
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
        size_t n = 0;
        char name[128];
        while ((e->s[e->pos + n] >= 'a' && e->s[e->pos + n] <= 'z') ||
               (e->s[e->pos + n] >= 'A' && e->s[e->pos + n] <= 'Z') ||
               (e->s[e->pos + n] >= '0' && e->s[e->pos + n] <= '9') ||
               e->s[e->pos + n] == '_') {
            n++;
        }
        if (n >= sizeof name) {
            fail(e);
            return v;
        }
        memcpy(name, e->s + e->pos, n);
        name[n] = '\0';
        e->pos += n;
        if (e->lookup == NULL || !e->lookup(e->context, name, &v)) {
            fail(e);
        }
        return v;
    }
    fail(e);
    return v;
}

static struct bind_eval unary(struct eval *e);

/* The operand of a prefix operator, one level deeper. */
static struct bind_eval operand(struct eval *e)
{
    struct bind_eval v;

    if (!enter(e)) {
        memset(&v, 0, sizeof v);
        v.type = "c_int";
        return v;
    }
    v = unary(e);
    leave();
    return v;
}

static struct bind_eval unary(struct eval *e)
{
    struct bind_eval v;

    if (take(e, "-")) {
        v = operand(e);
        if (v.kind == BIND_EVAL_FLOAT) {
            v.f = -v.f;
        } else if (v.kind == BIND_EVAL_INT) {
            v.i = bind_signed(0 - (uint64_t)v.i);
            narrow(&v);
        } else {
            fail(e);
        }
        v.enum_type = NULL;
        return v;
    }
    if (take(e, "+")) {
        v = operand(e);
        v.enum_type = NULL;
        return v;
    }
    if (take(e, "~")) {
        v = operand(e);
        if (v.kind != BIND_EVAL_INT) {
            fail(e);
        }
        v.i = ~v.i;
        narrow(&v);
        v.enum_type = NULL;
        return v;
    }
    return primary(e);
}

/* The type of a binary operation of C, from the usual arithmetic
   conversions: a float wins, and a wider or unsigned integer wins. */
static const char *common(const struct bind_eval *a, const struct bind_eval *b)
{
    static const char *const ranks[] = {
        "c_int", "c_uint", "c_long", "c_ulong", "c_longlong", "c_ulonglong",
    };
    size_t i;
    size_t ra = 0;
    size_t rb = 0;

    if (a->kind == BIND_EVAL_FLOAT || b->kind == BIND_EVAL_FLOAT) {
        return strcmp(a->type, "c_double") == 0 ||
                       strcmp(b->type, "c_double") == 0
                   ? "c_double"
                   : "c_float";
    }
    for (i = 0; i < sizeof ranks / sizeof ranks[0]; i++) {
        if (strcmp(ranks[i], a->type) == 0) {
            ra = i;
        }
        if (strcmp(ranks[i], b->type) == 0) {
            rb = i;
        }
    }
    return ranks[ra > rb ? ra : rb];
}

static struct bind_eval apply(struct eval *e, struct bind_eval a,
                              struct bind_eval b, const char *op)
{
    const char *type;

    if (a.kind == BIND_EVAL_STRING || b.kind == BIND_EVAL_STRING ||
        e->failed) {
        fail(e);
        return a;
    }
    type = common(&a, &b);
    a = converted(e, a, type);
    b = converted(e, b, type);
    if (a.kind == BIND_EVAL_FLOAT) {
        switch (op[0]) {
        case '*': a.f *= b.f; break;
        case '/': a.f /= b.f; break;
        case '+': a.f += b.f; break;
        case '-': a.f -= b.f; break;
        default: fail(e); break;
        }
        if (!isfinite(a.f)) {
            fail(e);
        }
        if (strcmp(type, "c_float") == 0) {
            a.f = (double)(float)a.f;
        }
        return a;
    }
    {
        uint64_t x = (uint64_t)a.i;
        uint64_t y = (uint64_t)b.i;
        bool is_unsigned = is_unsigned_type(type);
        switch (op[0]) {
        case '*': x *= y; break;
        case '/':
        case '%':
            if (y == 0 || (!is_unsigned && a.i == INT64_MIN && b.i == -1)) {
                fail(e);
                return a;
            }
            if (is_unsigned) {
                x = op[0] == '/' ? x / y : x % y;
            } else {
                x = (uint64_t)(op[0] == '/' ? a.i / b.i : a.i % b.i);
            }
            break;
        case '+': x += y; break;
        case '-': x -= y; break;
        case '<':
            if (y >= 64) {
                fail(e);
                return a;
            }
            x <<= y;
            break;
        case '>':
            if (y >= 64) {
                fail(e);
                return a;
            }
            /* A negative value shifts as the complement of the
               complement, which C11 defines, where >> of a negative
               int64_t is the implementation's. */
            x = is_unsigned || a.i >= 0 ? x >> y : ~(~x >> y);
            break;
        case '&': x &= y; break;
        case '|': x |= y; break;
        case '^': x ^= y; break;
        default: fail(e); break;
        }
        a.i = bind_signed(x);
        narrow(&a);
        return a;
    }
}

/* One level of the binary operators of C, from the tightest. */
static struct bind_eval binary(struct eval *e, int level)
{
    static const char *const levels[][3] = {
        {"*", "/", "%"}, {"+", "-", NULL}, {"<<", ">>", NULL},
        {"&", NULL, NULL}, {"^", NULL, NULL}, {"|", NULL, NULL},
    };
    struct bind_eval v;

    if (level < 0) {
        return unary(e);
    }
    v = binary(e, level - 1);
    for (;;) {
        const char *op = NULL;
        int k;
        for (k = 0; k < 3 && levels[level][k] != NULL; k++) {
            if (take(e, levels[level][k])) {
                op = levels[level][k];
                break;
            }
        }
        if (op == NULL) {
            return v;
        }
        v = apply(e, v, binary(e, level - 1), op);
        v.enum_type = NULL;
    }
}

static struct bind_eval conditional(struct eval *e)
{
    return binary(e, 5);
}

bool bind_eval(struct bind_module *b, const char *expr, bind_lookup lookup,
               void *context, struct bind_eval *out)
{
    struct eval e;

    memset(&e, 0, sizeof e);
    e.b = b;
    e.s = expr;
    e.lookup = lookup;
    e.context = context;
    space(&e);
    if (e.s[e.pos] == '\0' || !enter(&e)) {
        return false;
    }
    *out = conditional(&e);
    leave();
    space(&e);
    return !e.failed && e.s[e.pos] == '\0';
}

/* DESIGN: a float is written with the digits that give back the same
   value. printf's %g needs 9 of them for a c_float and 17 for a
   c_double. Anti reads a literal with a `.` or an exponent as a float, so one
   without either gains `.0`. */
void bind_float_text(double f, const char *type, struct text *out)
{
    size_t start = out->length;
    const char *digits;

    text_appendf(out, strcmp(type, "c_float") == 0 ? "%.9g" : "%.17g", f);
    digits = text_cstr(out) + start;
    if (strchr(digits, '.') == NULL && strchr(digits, 'e') == NULL) {
        text_append(out, ".0");
    }
}

static void string_text(const char *s, struct text *out)
{
    const unsigned char *p = (const unsigned char *)s;

    text_append(out, "\"");
    for (; *p != '\0'; p++) {
        switch (*p) {
        case '"': text_append(out, "\\\""); break;
        case '\\': text_append(out, "\\\\"); break;
        case '\n': text_append(out, "\\n"); break;
        case '\r': text_append(out, "\\r"); break;
        case '\t': text_append(out, "\\t"); break;
        default:
            if (*p < 0x20 || *p == 0x7F) {
                text_appendf(out, "\\x%02X", (unsigned)*p);
            } else {
                text_append_bytes(out, p, 1);
            }
            break;
        }
    }
    text_append(out, "\"");
}

bool bind_eval_const(struct bind_module *b, const char *name,
                     const struct bind_eval *v, const char *doc)
{
    struct bind_const *c;
    struct text value = {0};

    c = arena_alloc(&b->arena, sizeof *c);
    c->name = bind_strdup(b, name);
    c->doc = doc;
    {
        /* The names of an enum may point into the input of a reader, which
           goes before the module is written, so the constant keeps copies. */
        struct bind_eval *kept = arena_alloc(&b->arena, sizeof *kept);
        *kept = *v;
        if (v->enum_type != NULL) {
            kept->enum_type = bind_strdup(b, v->enum_type);
            kept->enum_value = bind_strdup(b, v->enum_value);
        }
        c->eval = kept;
        v = kept;
    }
    switch (v->kind) {
    case BIND_EVAL_INT:
        if (v->enum_type != NULL) {
            c->type = v->enum_type;
            text_appendf(&value, "%s.%s", v->enum_type, v->enum_value);
        } else {
            c->type = v->type;
            if (is_unsigned_type(v->type)) {
                text_appendf(&value, "%llu", (unsigned long long)v->i);
            } else {
                text_appendf(&value, "%lld", (long long)v->i);
            }
        }
        break;
    case BIND_EVAL_FLOAT:
        c->type = v->type;
        bind_float_text(v->f, v->type, &value);
        break;
    case BIND_EVAL_STRING:
        c->type = "str";
        string_text(v->text, &value);
        break;
    default:
        text_free(&value);
        return false;
    }
    c->value = bind_strdup(b, text_cstr(&value));
    text_free(&value);
    bind_list_add(&b->consts, c);
    return true;
}

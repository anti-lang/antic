#include "pattern.h"

#include <stdint.h>
#include <string.h>

#include "../rt/regex.h"
#include "arena.h"

bool pattern_compiles(const char *bytes, size_t length, size_t *offset,
                      char *message, size_t room)
{
    unsigned char text[ANTI_RT_REGEX_MESSAGE_ROOM];
    int32_t code = 0;
    int64_t at = 0;
    void *compiled = anti_rt_regex_compile((const unsigned char *)bytes,
                                           (int64_t)length, &code, &at);
    size_t n;

    if (compiled != NULL) {
        anti_rt_regex_free(compiled);
        return true;
    }
    anti_rt_regex_message_into(code, text, sizeof text);
    *offset = (size_t)at;
    if (room > 0) {
        n = strlen((const char *)text);
        n = n < room - 1 ? n : room - 1;
        memcpy(message, text, n);
        message[n] = '\0';
    }
    return false;
}

/* DESIGN: the check `exponential-pattern` reads the pattern into a tree
   of its own and asks one question of every repeat: can the text it
   repeats also start the next round of an unbounded repeat around it?
   When it can, the engine has two ways to split the same text at every
   round, and a failing match tries all of them. That is `(a+)+`, where
   `a` both continues the inner repeat and starts the outer one, and
   `(\w+\s?)*`, where the `\s?` may be empty. `(\d+\.)+` passes, since a
   round of the outer repeat ends at a `.` that the inner one cannot take.
   A possessive quantifier and an atomic group give the engine one way,
   so the check passes over a repeat inside either. Sets of characters
   are exact for ASCII. A character outside ASCII stands for every byte
   from 0x80 up, so two of them always overlap, which errs towards the
   warning. */

enum node_kind { NODE_SET, NODE_EMPTY, NODE_CAT, NODE_ALT, NODE_GROUP,
                 NODE_REPEAT };

/* The first bytes a node can match: one bit per byte value. */
struct set {
    uint64_t bits[4];
};

struct node {
    enum node_kind kind;
    struct node *parent;
    struct node **children;
    size_t count;
    size_t room;
    struct set set;             /* NODE_SET */
    bool atomic;                /* an atomic group, a possessive repeat */
    bool zero_width;            /* a lookaround, which consumes no text */
    long min;                   /* NODE_REPEAT, and max -1 for no bound */
    long max;
    struct pattern_span span;
};

struct reader {
    const unsigned char *s;
    size_t n;
    size_t pos;
    struct arena *arena;
    bool caseless;              /* `(?i)` */
    bool extended;              /* `(?x)` */
};

static void set_add(struct set *s, unsigned c)
{
    s->bits[c >> 6] |= (uint64_t)1 << (c & 63);
}

static void set_range(struct set *s, unsigned lo, unsigned hi)
{
    unsigned c;

    for (c = lo; c <= hi && c < 256; c++) {
        set_add(s, c);
    }
}

static void set_union(struct set *s, const struct set *t)
{
    size_t i;

    for (i = 0; i < 4; i++) {
        s->bits[i] |= t->bits[i];
    }
}

static void set_complement(struct set *s)
{
    size_t i;

    for (i = 0; i < 4; i++) {
        s->bits[i] = ~s->bits[i];
    }
    /* A character outside ASCII stays possible in a negated set. */
    set_range(s, 0x80, 0xFF);
}

static bool set_meets(const struct set *s, const struct set *t)
{
    size_t i;

    for (i = 0; i < 4; i++) {
        if ((s->bits[i] & t->bits[i]) != 0) {
            return true;
        }
    }
    return false;
}

static bool is_letter(unsigned c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool is_digit(unsigned c)
{
    return c >= '0' && c <= '9';
}

/* Add the character c, both cases under `(?i)`. */
static void add_char(const struct reader *r, struct set *s, uint32_t c)
{
    if (c >= 0x80) {
        set_range(s, 0x80, 0xFF);
        return;
    }
    set_add(s, c);
    if (r->caseless && is_letter(c)) {
        set_add(s, c ^ 0x20);
    }
}

/* The sets of the escapes of a class, `\d` and the rest. Returns false
   for a letter that names no set. */
static bool class_escape(unsigned c, struct set *s)
{
    struct set t;
    bool negate = c >= 'A' && c <= 'Z';

    memset(&t, 0, sizeof t);
    switch (c | 0x20) {
    case 'd':
        set_range(&t, '0', '9');
        break;
    case 'w':
        set_range(&t, '0', '9');
        set_range(&t, 'A', 'Z');
        set_range(&t, 'a', 'z');
        set_add(&t, '_');
        break;
    case 's':
        set_range(&t, '\t', '\r');
        set_add(&t, ' ');
        break;
    case 'h':
        set_add(&t, '\t');
        set_add(&t, ' ');
        set_range(&t, 0x80, 0xFF);
        break;
    case 'v':
        set_range(&t, '\n', '\r');
        set_range(&t, 0x80, 0xFF);
        break;
    default:
        return false;
    }
    if (negate) {
        set_complement(&t);
    }
    set_union(s, &t);
    return true;
}

static void set_all(struct set *s)
{
    memset(s, 0xFF, sizeof *s);
}

static int peek(const struct reader *r, size_t ahead)
{
    return r->pos + ahead < r->n ? r->s[r->pos + ahead] : -1;
}

/* Step over one character, all the bytes of its UTF-8 form, and return
   its first byte or the scalar of an ASCII one. */
static uint32_t take_char(struct reader *r)
{
    unsigned c = r->s[r->pos++];

    if (c >= 0x80) {
        while (r->pos < r->n && (r->s[r->pos] & 0xC0) == 0x80) {
            r->pos++;
        }
    }
    return c;
}

/* Step to the byte after the first close at or after the position, or to
   the end. */
static void skip_past(struct reader *r, int close)
{
    while (r->pos < r->n && r->s[r->pos] != close) {
        r->pos++;
    }
    if (r->pos < r->n) {
        r->pos++;
    }
}

static struct node *new_node(struct reader *r, enum node_kind kind,
                             size_t start)
{
    struct node *n = arena_alloc(r->arena, sizeof *n);

    n->kind = kind;
    n->span.start = start;
    n->span.end = start;
    return n;
}

static void add_child(struct reader *r, struct node *parent,
                      struct node *child)
{
    if (parent->count == parent->room) {
        size_t room = parent->room == 0 ? 4 : 2 * parent->room;
        struct node **grown = arena_alloc(r->arena, room * sizeof *grown);
        if (parent->count > 0) {
            memcpy(grown, parent->children,
                   parent->count * sizeof *grown);
        }
        parent->children = grown;
        parent->room = room;
    }
    parent->children[parent->count++] = child;
    child->parent = parent;
}

/* Whitespace and `#` comments under `(?x)`. */
static void skip_extended(struct reader *r)
{
    while (r->extended && r->pos < r->n) {
        int c = peek(r, 0);
        if (c == ' ' || (c >= '\t' && c <= '\r')) {
            r->pos++;
        } else if (c == '#') {
            skip_past(r, '\n');
        } else {
            return;
        }
    }
}

static struct node *parse_alt(struct reader *r);

/* The value of up to nine digits at the position, or -1. */
static long read_number(struct reader *r)
{
    long value = 0;
    int digits = 0;

    while (digits < 9 && peek(r, 0) >= 0 &&
           is_digit((unsigned)peek(r, 0))) {
        value = value * 10 + (peek(r, 0) - '0');
        r->pos++;
        digits++;
    }
    return digits > 0 ? value : -1;
}

/* The value of the hex or octal digits of an escape, `\x{...}`, `\xhh`,
   `\o{...}` and `\0oo`, as one character. */
static uint32_t coded_char(struct reader *r, unsigned kind)
{
    uint32_t value = 0;
    int base = kind == 'x' ? 16 : 8;
    int limit = kind == 'x' ? 2 : 3;
    bool braced = kind != '0' && peek(r, 0) == '{';
    int digits = 0;

    if (braced) {
        r->pos++;
    }
    while (peek(r, 0) >= 0 && (braced || digits < limit)) {
        unsigned c = (unsigned)peek(r, 0);
        unsigned lower = c | 0x20;
        unsigned d = is_digit(c)                     ? c - '0'
                     : lower >= 'a' && lower <= 'f' ? lower - 'a' + 10
                                                    : 99;
        if (d >= (unsigned)base) {
            break;
        }
        value = value * (uint32_t)base + d;
        r->pos++;
        digits++;
    }
    if (braced) {
        skip_past(r, '}');
    }
    return value;
}

/* One character of a class, after a `\` or as itself. Returns false when
   the escape named a set, which it added to s. */
static bool class_char(struct reader *r, struct set *s, uint32_t *c)
{
    unsigned e;

    if (peek(r, 0) != '\\') {
        *c = take_char(r);
        return true;
    }
    r->pos++;
    if (r->pos >= r->n) {
        *c = '\\';
        return true;
    }
    e = r->s[r->pos++];
    if (class_escape(e, s)) {
        return false;
    }
    switch (e) {
    case 'p':
    case 'P':
        if (peek(r, 0) == '{') {
            skip_past(r, '}');
        } else if (r->pos < r->n) {
            r->pos++;
        }
        set_all(s);
        return false;
    case 'x':
    case 'o':
    case '0':
        *c = coded_char(r, e);
        return true;
    case 'n': *c = '\n'; return true;
    case 'r': *c = '\r'; return true;
    case 't': *c = '\t'; return true;
    case 'f': *c = '\f'; return true;
    case 'e': *c = 27; return true;
    case 'a': *c = 7; return true;
    case 'b': *c = 8; return true;
    case 'c':
        *c = r->pos < r->n ? (uint32_t)(r->s[r->pos++] ^ 0x40) : 'c';
        return true;
    default:
        if (e >= 0x80) {
            r->pos--;
            *c = take_char(r);
        } else {
            *c = e;
        }
        return true;
    }
}

/* The ASCII members of a POSIX class name, `[:alpha:]` and the rest. */
static void posix_class(const unsigned char *name, size_t length,
                        struct set *s)
{
    struct set t;
    bool negate = length > 0 && name[0] == '^';
    unsigned c;

    if (negate) {
        name++;
        length--;
    }
    memset(&t, 0, sizeof t);
    for (c = 0; c < 128; c++) {
        bool in;
        if (length == 5 && memcmp(name, "alpha", 5) == 0) {
            in = is_letter(c);
        } else if (length == 5 && memcmp(name, "digit", 5) == 0) {
            in = is_digit(c);
        } else if (length == 5 && memcmp(name, "alnum", 5) == 0) {
            in = is_letter(c) || is_digit(c);
        } else if (length == 5 && memcmp(name, "upper", 5) == 0) {
            in = c >= 'A' && c <= 'Z';
        } else if (length == 5 && memcmp(name, "lower", 5) == 0) {
            in = c >= 'a' && c <= 'z';
        } else if (length == 5 && memcmp(name, "space", 5) == 0) {
            in = c == ' ' || (c >= '\t' && c <= '\r');
        } else if (length == 5 && memcmp(name, "blank", 5) == 0) {
            in = c == ' ' || c == '\t';
        } else if (length == 4 && memcmp(name, "word", 4) == 0) {
            in = is_letter(c) || is_digit(c) || c == '_';
        } else if (length == 6 && memcmp(name, "xdigit", 6) == 0) {
            in = is_digit(c) || ((c | 0x20) >= 'a' && (c | 0x20) <= 'f');
        } else {
            /* punct, print, graph, cntrl, ascii and any other: every
               byte, which errs towards the warning. */
            in = true;
        }
        if (in) {
            set_add(&t, c);
        }
    }
    if (negate) {
        set_complement(&t);
    }
    set_union(s, &t);
}

/* `[...]`, from the byte after the `[`. */
static struct node *parse_class(struct reader *r, size_t start)
{
    struct node *n = new_node(r, NODE_SET, start);
    bool negate = false;
    bool first = true;

    if (peek(r, 0) == '^') {
        negate = true;
        r->pos++;
    }
    while (r->pos < r->n) {
        uint32_t lo;
        uint32_t hi;
        if (peek(r, 0) == ']' && !first) {
            r->pos++;
            break;
        }
        first = false;
        if (peek(r, 0) == '[' && peek(r, 1) == ':') {
            size_t name = r->pos + 2;
            size_t end = name;
            while (end + 1 < r->n &&
                   !(r->s[end] == ':' && r->s[end + 1] == ']')) {
                end++;
            }
            posix_class(r->s + name, end - name, &n->set);
            r->pos = end + 2 < r->n ? end + 2 : r->n;
            continue;
        }
        if (peek(r, 0) == '\\' && peek(r, 1) == 'Q') {
            r->pos += 2;
            while (r->pos < r->n &&
                   !(peek(r, 0) == '\\' && peek(r, 1) == 'E')) {
                add_char(r, &n->set, take_char(r));
            }
            r->pos = r->pos + 2 <= r->n ? r->pos + 2 : r->n;
            continue;
        }
        if (!class_char(r, &n->set, &lo)) {
            continue;
        }
        if (peek(r, 0) == '-' && peek(r, 1) >= 0 && peek(r, 1) != ']') {
            r->pos++;
            if (!class_char(r, &n->set, &hi)) {
                add_char(r, &n->set, lo);
                add_char(r, &n->set, '-');
                continue;
            }
            if (hi >= 0x80) {
                set_range(&n->set, 0x80, 0xFF);
                hi = 0x7F;
            }
            for (; lo <= hi; lo++) {
                add_char(r, &n->set, lo);
            }
            continue;
        }
        add_char(r, &n->set, lo);
    }
    if (negate) {
        set_complement(&n->set);
    }
    n->span.end = r->pos;
    return n;
}

/* A node for text this reader does not follow, such as a backreference
   or a call of a group. It takes every byte and is never empty. */
static struct node *opaque(struct reader *r, size_t start)
{
    struct node *n = new_node(r, NODE_SET, start);

    set_all(&n->set);
    n->span.end = r->pos;
    return n;
}

/* An escape outside a class, from the byte after the `\`. Returns NULL
   for `\E`, which matches nothing and is no atom. */
static struct node *parse_escape(struct reader *r, size_t start)
{
    struct node *n;
    uint32_t c;
    unsigned e;

    if (r->pos >= r->n) {
        n = new_node(r, NODE_SET, start);
        add_char(r, &n->set, '\\');
        return n;
    }
    e = r->s[r->pos];
    switch (e) {
    case 'b': case 'B': case 'A': case 'Z': case 'z': case 'G': case 'K':
        r->pos++;
        n = new_node(r, NODE_EMPTY, start);
        n->span.end = r->pos;
        return n;
    case 'E':
        r->pos++;
        return NULL;
    case 'Q':
        r->pos++;
        n = new_node(r, NODE_CAT, start);
        while (r->pos < r->n &&
               !(peek(r, 0) == '\\' && peek(r, 1) == 'E')) {
            struct node *one = new_node(r, NODE_SET, r->pos);
            add_char(r, &one->set, take_char(r));
            one->span.end = r->pos;
            add_child(r, n, one);
        }
        r->pos = r->pos + 2 <= r->n ? r->pos + 2 : r->n;
        n->span.end = r->pos;
        return n;
    case 'N': case 'R': case 'X': case 'C':
        r->pos++;
        return opaque(r, start);
    case 'g': case 'k':
        r->pos++;
        c = (uint32_t)peek(r, 0);
        if (c == '{' || c == '<' || c == '\'') {
            skip_past(r, c == '{' ? '}' : c == '<' ? '>' : '\'');
        } else {
            if (peek(r, 0) == '-' || peek(r, 0) == '+') {
                r->pos++;
            }
            (void)read_number(r);
        }
        return opaque(r, start);
    default:
        break;
    }
    if (e >= '1' && e <= '9') {
        (void)read_number(r);
        return opaque(r, start);
    }
    /* The escape of one character reads as it does in a class. */
    r->pos = start;
    n = new_node(r, NODE_SET, start);
    if (class_char(r, &n->set, &c)) {
        add_char(r, &n->set, c);
    }
    n->span.end = r->pos;
    return n;
}

/* The names of the alphabetic assertions that consume no text. */
static bool lookaround_name(const unsigned char *name, size_t length)
{
    static const char *const names[] = {
        "pla", "plb", "nla", "nlb", "napla", "naplb", "positive_lookahead",
        "negative_lookahead", "positive_lookbehind", "negative_lookbehind",
        "non_atomic_positive_lookahead", "non_atomic_positive_lookbehind"
    };
    size_t i;

    for (i = 0; i < sizeof names / sizeof names[0]; i++) {
        if (strlen(names[i]) == length &&
            memcmp(names[i], name, length) == 0) {
            return true;
        }
    }
    return false;
}

/* The body of a group up to its `)`, under the flags of the group, which
   end with it. */
static struct node *group_body(struct reader *r, struct node *group)
{
    bool caseless = r->caseless;
    bool extended = r->extended;

    add_child(r, group, parse_alt(r));
    if (peek(r, 0) == ')') {
        r->pos++;
    }
    r->caseless = caseless;
    r->extended = extended;
    group->span.end = r->pos;
    return group;
}

/* Read the letters of `(?i)` or `(?i-x:` up to the `)` or the `:`, and
   apply them. Returns the byte that ended them. */
static int read_flags(struct reader *r)
{
    bool on = true;

    while (r->pos < r->n && peek(r, 0) != ')' && peek(r, 0) != ':') {
        int c = peek(r, 0);
        if (c == '-') {
            on = false;
        } else if (c == '^') {
            r->caseless = false;
            r->extended = false;
        } else if (c == 'i') {
            r->caseless = on;
        } else if (c == 'x') {
            r->extended = on;
        }
        r->pos++;
    }
    return peek(r, 0);
}

/* A group, from the byte after its `(`. Returns NULL for a comment and
   for flags that apply to the rest of the enclosing group. */
static struct node *parse_group(struct reader *r, size_t start)
{
    struct node *group = new_node(r, NODE_GROUP, start);
    int c = peek(r, 0);

    if (c == '*') {
        size_t name = r->pos + 1;
        size_t end = name;
        while (end < r->n && r->s[end] != ':' && r->s[end] != ')') {
            end++;
        }
        if (end < r->n && r->s[end] == ':') {
            group->atomic = end - name == 6 &&
                            memcmp(r->s + name, "atomic", 6) == 0;
            group->zero_width = lookaround_name(r->s + name, end - name);
            r->pos = end + 1;
            return group_body(r, group);
        }
        r->pos = end;
        skip_past(r, ')');
        group->kind = NODE_EMPTY;
        group->span.end = r->pos;
        return group;
    }
    if (c != '?') {
        return group_body(r, group);
    }
    r->pos++;
    c = peek(r, 0);
    switch (c) {
    case ':':
    case '|':
        r->pos++;
        return group_body(r, group);
    case '>':
        r->pos++;
        group->atomic = true;
        return group_body(r, group);
    case '=':
    case '!':
        r->pos++;
        group->zero_width = true;
        return group_body(r, group);
    case '<':
        if (peek(r, 1) == '=' || peek(r, 1) == '!') {
            r->pos += 2;
            group->zero_width = true;
            return group_body(r, group);
        }
        skip_past(r, '>');
        return group_body(r, group);
    case '\'':
        r->pos++;
        skip_past(r, '\'');
        return group_body(r, group);
    case 'P':
        if (peek(r, 1) == '<') {
            skip_past(r, '>');
            return group_body(r, group);
        }
        skip_past(r, ')');
        return opaque(r, start);
    case '#':
        skip_past(r, ')');
        return NULL;
    case 'C':
        skip_past(r, ')');
        group->kind = NODE_EMPTY;
        group->span.end = r->pos;
        return group;
    case '(': {
        /* A condition, which the engine tests and which consumes no
           text, then the branches. */
        int depth = 0;
        while (r->pos < r->n) {
            int d = peek(r, 0);
            r->pos++;
            if (d == '\\') {
                r->pos++;
            } else if (d == '(') {
                depth++;
            } else if (d == ')' && --depth == 0) {
                break;
            }
        }
        return group_body(r, group);
    }
    case '[':
        /* A class of set operations, which this reader does not follow. */
        while (r->pos + 1 < r->n &&
               !(peek(r, 0) == ']' && peek(r, 1) == ')')) {
            r->pos++;
        }
        r->pos = r->pos + 2 <= r->n ? r->pos + 2 : r->n;
        return opaque(r, start);
    default:
        break;
    }
    if (c == 'R' || c == '&' || (c >= 0 && is_digit((unsigned)c)) ||
        ((c == '+' || c == '-') && peek(r, 1) >= 0 &&
         is_digit((unsigned)peek(r, 1)))) {
        skip_past(r, ')');
        return opaque(r, start);
    }
    {
        bool caseless = r->caseless;
        bool extended = r->extended;
        if (read_flags(r) == ')') {
            /* The flags hold to the end of the enclosing group. */
            r->pos++;
            return NULL;
        }
        r->pos++;
        group_body(r, group);
        r->caseless = caseless;
        r->extended = extended;
        return group;
    }
}

static struct node *parse_atom(struct reader *r)
{
    size_t start = r->pos;
    int c = peek(r, 0);
    struct node *n;

    r->pos++;
    switch (c) {
    case '(':
        return parse_group(r, start);
    case '[':
        return parse_class(r, start);
    case '\\':
        return parse_escape(r, start);
    case '.':
        return opaque(r, start);
    case '^':
    case '$':
        n = new_node(r, NODE_EMPTY, start);
        n->span.end = r->pos;
        return n;
    default:
        r->pos = start;
        n = new_node(r, NODE_SET, start);
        add_char(r, &n->set, take_char(r));
        n->span.end = r->pos;
        return n;
    }
}

/* A quantifier after atom, or atom itself when none follows. */
static struct node *parse_repeat(struct reader *r, struct node *atom)
{
    size_t at = r->pos;
    long min;
    long max;
    struct node *n;

    skip_extended(r);
    switch (peek(r, 0)) {
    case '*':
        min = 0;
        max = -1;
        r->pos++;
        break;
    case '+':
        min = 1;
        max = -1;
        r->pos++;
        break;
    case '?':
        min = 0;
        max = 1;
        r->pos++;
        break;
    case '{':
        r->pos++;
        while (peek(r, 0) == ' ') {
            r->pos++;
        }
        min = read_number(r);
        while (peek(r, 0) == ' ') {
            r->pos++;
        }
        max = min;
        if (peek(r, 0) == ',') {
            r->pos++;
            while (peek(r, 0) == ' ') {
                r->pos++;
            }
            max = read_number(r);
            while (peek(r, 0) == ' ') {
                r->pos++;
            }
            if (min < 0) {
                min = 0;
                if (max < 0) {
                    r->pos = at;
                    return atom;
                }
            }
        }
        if (min < 0 || peek(r, 0) != '}') {
            /* No quantifier: the `{` is a character. */
            r->pos = at;
            return atom;
        }
        r->pos++;
        break;
    default:
        r->pos = at;
        return atom;
    }
    n = new_node(r, NODE_REPEAT, atom->span.start);
    n->min = min;
    n->max = max;
    if (peek(r, 0) == '+') {
        n->atomic = true;
        r->pos++;
    } else if (peek(r, 0) == '?') {
        r->pos++;
    }
    add_child(r, n, atom);
    n->span.end = r->pos;
    return n;
}

static struct node *parse_cat(struct reader *r)
{
    struct node *cat = new_node(r, NODE_CAT, r->pos);

    for (;;) {
        struct node *atom;
        skip_extended(r);
        if (r->pos >= r->n || peek(r, 0) == '|' || peek(r, 0) == ')') {
            break;
        }
        atom = parse_atom(r);
        if (atom == NULL) {
            continue;
        }
        for (;;) {
            struct node *repeat = parse_repeat(r, atom);
            if (repeat == atom) {
                break;
            }
            atom = repeat;
        }
        add_child(r, cat, atom);
    }
    cat->span.end = r->pos;
    return cat;
}

static struct node *parse_alt(struct reader *r)
{
    size_t start = r->pos;
    struct node *first = parse_cat(r);
    struct node *alt;

    if (peek(r, 0) != '|') {
        return first;
    }
    alt = new_node(r, NODE_ALT, start);
    add_child(r, alt, first);
    while (peek(r, 0) == '|') {
        r->pos++;
        add_child(r, alt, parse_cat(r));
    }
    alt->span.end = r->pos;
    return alt;
}

/* The first bytes n can match, added to s, and whether it can match the
   empty text. */
static bool first_of(const struct node *n, struct set *s)
{
    bool empty;
    size_t i;

    switch (n->kind) {
    case NODE_SET:
        set_union(s, &n->set);
        return false;
    case NODE_EMPTY:
        return true;
    case NODE_CAT:
        for (i = 0; i < n->count; i++) {
            if (!first_of(n->children[i], s)) {
                return false;
            }
        }
        return true;
    case NODE_ALT:
        empty = false;
        for (i = 0; i < n->count; i++) {
            empty = first_of(n->children[i], s) || empty;
        }
        return empty;
    case NODE_GROUP:
        if (n->zero_width) {
            return true;
        }
        return n->count == 0 || first_of(n->children[0], s);
    case NODE_REPEAT:
        empty = first_of(n->children[0], s);
        return empty || n->min == 0;
    }
    return true;
}

/* The bytes that can come right after inner inside one round of the
   repeat outer that holds it. The first bytes of the next round of outer,
   or of a repeat between the two, count as well. */
static struct set follow_within(const struct node *inner,
                                const struct node *outer)
{
    struct set f;
    const struct node *child = inner;
    const struct node *at = inner->parent;
    size_t i;

    memset(&f, 0, sizeof f);
    while (at != outer) {
        if (at->kind == NODE_CAT) {
            for (i = 0; at->children[i] != child; i++) {
            }
            for (i++; i < at->count; i++) {
                if (!first_of(at->children[i], &f)) {
                    return f;
                }
            }
        }
        if (at->kind == NODE_REPEAT && (at->max < 0 || at->max > 1)) {
            (void)first_of(at->children[0], &f);
        }
        child = at;
        at = at->parent;
    }
    (void)first_of(outer->children[0], &f);
    return f;
}

/* Whether a repeat can take a varying amount of text, which gives the
   engine a choice at each of its rounds. */
static bool varies(const struct node *n)
{
    return n->kind == NODE_REPEAT && !n->atomic &&
           (n->max < 0 || n->max > n->min);
}

static bool find_nested(const struct node *n, struct pattern_span *inner,
                        struct pattern_span *outer)
{
    const struct node *at;
    size_t i;

    if (varies(n)) {
        struct set repeated;
        memset(&repeated, 0, sizeof repeated);
        (void)first_of(n->children[0], &repeated);
        for (at = n->parent; at != NULL; at = at->parent) {
            if ((at->kind == NODE_GROUP && (at->atomic || at->zero_width)) ||
                (at->kind == NODE_REPEAT && at->atomic)) {
                break;
            }
            if (at->kind == NODE_REPEAT && at->max < 0) {
                struct set after = follow_within(n, at);
                if (set_meets(&repeated, &after)) {
                    *inner = n->span;
                    *outer = at->span;
                    return true;
                }
            }
        }
    }
    for (i = 0; i < n->count; i++) {
        if (find_nested(n->children[i], inner, outer)) {
            return true;
        }
    }
    return false;
}

bool pattern_exponential(const char *bytes, size_t length,
                         struct pattern_span *inner,
                         struct pattern_span *outer)
{
    struct arena arena = {0};
    struct reader r;
    struct node *root;
    bool found;

    memset(&r, 0, sizeof r);
    r.s = (const unsigned char *)bytes;
    r.n = length;
    r.arena = &arena;
    root = parse_alt(&r);
    /* A `)` that closes nothing cannot stand in a pattern that compiles,
       so the reader ends at the end of the pattern. */
    found = find_nested(root, inner, outer);
    arena_free(&arena);
    return found;
}

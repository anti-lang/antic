/* The patterns of a program: the literals it compiles before main, and
   the messages of the errors of a pattern compiled at run time. */
#include "atomic.h"
#include "cpu_level.h"
#include "regex.h"
#include "std.h"

#include <stdlib.h>
#include <string.h>

/* DESIGN: every pattern literal is compiled once, before main, by the
   constructor antic writes for each module that holds one. antic checked
   the pattern already. A literal that does not compile here means the
   memory ran out, and that ends the program as a refusal at start does.
   The constructor may run before main checks the processor. PCRE2 is
   built for the default level of its target, so the check runs here
   first. */
void *anti_rt_regex_literal(const unsigned char *bytes, int64_t length)
{
    unsigned char message[ANTI_RT_REGEX_MESSAGE_ROOM];
    int32_t code = 0;
    int64_t offset = 0;
    void *compiled;

    anti_rt_cpu_check();
    compiled = anti_rt_regex_compile(bytes, length, &code, &offset);
    if (compiled == NULL) {
        anti_rt_regex_message_into(code, message, sizeof message);
        anti_rt_fail_exit(70, "anti: the pattern `%.*s` does not compile at "
                          "start: %s", (int)length, (const char *)bytes,
                          (const char *)message);
    }
    return compiled;
}

/* The byte pattern literal of length bytes at bytes, compiled before main
   as anti_rt_regex_literal compiles a pattern of text. */
void *anti_rt_regex_literal_bytes(const unsigned char *bytes, int64_t length)
{
    unsigned char message[ANTI_RT_REGEX_MESSAGE_ROOM];
    int32_t code = 0;
    int64_t offset = 0;
    void *compiled;

    anti_rt_cpu_check();
    compiled = anti_rt_regex_compile_bytes(bytes, length, &code, &offset);
    if (compiled == NULL) {
        anti_rt_regex_message_into(code, message, sizeof message);
        anti_rt_fail_exit(70, "anti: the byte pattern `%.*s` does not "
                          "compile at start: %s", (int)length,
                          (const char *)bytes, (const char *)message);
    }
    return compiled;
}

/* The error numbers of PCRE2 lie between -256 and 511: the failures of a
   match below zero and those of a compile from 100 up. */
#define MESSAGE_LOW (-256)
#define MESSAGE_COUNT 768

/* DESIGN: PCRE2 writes a message into memory of the caller, and a `str`
   owns nothing. The text of each error number is therefore made once and
   kept for the life of the program. Two threads that ask at once for the
   same number race to one slot, and the one that loses frees its copy. */
static unsigned char *messages[MESSAGE_COUNT];

struct anti_text anti_rt_regex_message(int32_t code)
{
    struct anti_text text;
    unsigned char *have;
    unsigned char *made;
    void *slot;

    text.ptr = (const unsigned char *)"";
    text.len = 0;
    if (code < MESSAGE_LOW || code >= MESSAGE_LOW + MESSAGE_COUNT) {
        return text;
    }
    slot = &messages[code - MESSAGE_LOW];
    have = (unsigned char *)(intptr_t)anti_rt_atomic_load(
        slot, (int64_t)sizeof(void *));
    if (have == NULL) {
        made = malloc(ANTI_RT_REGEX_MESSAGE_ROOM);
        if (made == NULL) {
            return text;
        }
        anti_rt_regex_message_into(code, made, ANTI_RT_REGEX_MESSAGE_ROOM);
        if (anti_rt_atomic_compare_swap(slot, (int64_t)sizeof(void *), 0,
                                        (int64_t)(intptr_t)made)) {
            have = made;
        } else {
            free(made);
            have = (unsigned char *)(intptr_t)anti_rt_atomic_load(
                slot, (int64_t)sizeof(void *));
        }
    }
    text.ptr = have;
    text.len = (int64_t)strlen((const char *)have);
    return text;
}

/* The searches of the methods of `str`, which anti.regex calls. */

#define PCRE2_CODE_UNIT_WIDTH 8
#define PCRE2_STATIC
#include <pcre2.h>

/* The layout of anti.lang.Match, which types_match of src/antic/types.c
   declares in this order. from and options are the search that found the
   match, which group runs again. */
struct anti_match {
    const void *pattern;
    struct anti_text all;
    struct anti_text pre;
    struct anti_text post;
    int64_t count;
    struct anti_text subject;
    int64_t from;
    int64_t options;
};

/* The layout of a `?Match`, the match and the flag byte that says it is
   there, as C lays out every `?T` of a value. */
struct anti_maybe_match {
    struct anti_match value;
    uint8_t has;
};

/* The layout of anti.regex.Cursor. It holds where the next search starts
   and with which options, and the end of the last match given. skip and
   left count the matches to pass over and to give, and left is -1
   without a limit. */
struct anti_cursor {
    const void *pattern;
    struct anti_text subject;
    int64_t at;
    int64_t options;
    int64_t last;
    int64_t skip;
    int64_t left;
};

/* A walk and the match it stands at, as anti.regex.Matches holds them. */
struct anti_matches {
    struct anti_match current;
    struct anti_cursor cursor;
};

/* What a search gives, and what anti.regex reads from each function. */
#define FOUND 1
#define DONE 0
#define EXPENSIVE (-1)
#define NO_GROUP (-1)

/* The options of the search after an empty match: a match at the same
   place that is not empty, or else none there. */
#define RETRY (PCRE2_NOTEMPTY_ATSTART | PCRE2_ANCHORED)

static _Noreturn void out_of_memory(void)
{
    anti_rt_fail_abort("anti: out of memory in a search of a pattern");
}

/* DESIGN: a search that reaches a limit of PCRE2, of matching, of depth
   or of the heap, is too expensive. A literal turns that into a stop and
   a compiled pattern into `TooExpensive`. The limits are PCRE2's own,
   fixed when the pattern is compiled, where `(*LIMIT_MATCH=n)` may lower
   them. Any other failure means the text is no valid UTF-8, which a
   `str` never holds. It stops the program with PCRE2's message. */
static int64_t failure(int rc)
{
    unsigned char message[ANTI_RT_REGEX_MESSAGE_ROOM];

    if (rc == PCRE2_ERROR_MATCHLIMIT || rc == PCRE2_ERROR_DEPTHLIMIT ||
        rc == PCRE2_ERROR_HEAPLIMIT || rc == PCRE2_ERROR_NOMEMORY) {
        return EXPENSIVE;
    }
    anti_rt_regex_message_into(rc, message, sizeof message);
    anti_rt_fail_abort("anti: a search of a pattern failed: %s",
                       (const char *)message);
}

static pcre2_match_data *match_data(const void *pattern, int full)
{
    pcre2_match_data *md = full ? pcre2_match_data_create_from_pattern(
                                      ANTI_PATTERN_CODE(pattern), NULL)
                                : pcre2_match_data_create(1, NULL);

    if (md == NULL) {
        out_of_memory();
    }
    return md;
}

static void cursor_reset(struct anti_cursor *c, const void *pattern,
                         const unsigned char *s, int64_t length)
{
    c->pattern = pattern;
    c->subject.ptr = s;
    c->subject.len = length;
    c->at = 0;
    c->options = 0;
    c->last = 0;
    c->skip = 0;
    c->left = -1;
}

/* Whether the newline of the pattern takes CR LF as one, so that a step
   past an empty match steps over both. */
static int crlf_newline(const void *pattern)
{
    uint32_t newline = 0;

    (void)pcre2_pattern_info(ANTI_PATTERN_CODE(pattern), PCRE2_INFO_NEWLINE,
                             &newline);
    return newline == PCRE2_NEWLINE_CRLF || newline == PCRE2_NEWLINE_ANY ||
           newline == PCRE2_NEWLINE_ANYCRLF;
}

/* The offset of the character after the one at at. A byte pattern
   steps one byte, since its text holds no characters. */
static int64_t next_character(const struct anti_cursor *c, int64_t at)
{
    const unsigned char *s = c->subject.ptr;
    int64_t length = c->subject.len;

    if (at + 1 < length && s[at] == '\r' && s[at + 1] == '\n' &&
        crlf_newline(c->pattern)) {
        return at + 2;
    }
    at++;
    if (anti_rt_regex_is_bytes(c->pattern)) {
        return at;
    }
    while (at < length && (s[at] & 0xC0) == 0x80) {
        at++;
    }
    return at;
}

/* DESIGN: the search of every method runs from the start of the text to
   its end, as PCRE2's demonstration program does. After an empty match
   the next search asks for a match at the same place that is not empty.
   Without one it steps a character on. The first search checks the text
   as UTF-8. A later one that starts on the first byte of a character
   skips the check, so a walk over a text reads it once. */
static int64_t search(struct anti_cursor *c, pcre2_match_data *md,
                      int64_t *start, int64_t *end, int64_t *from,
                      int64_t *used)
{
    const unsigned char *s = c->subject.ptr;
    int64_t length = c->subject.len;

    for (;;) {
        PCRE2_SIZE *ov;
        int rc;
        if (c->at > length) {
            return DONE;
        }
        if (c->at < length && (s[c->at] & 0xC0) == 0x80) {
            c->options &= ~(int64_t)PCRE2_NO_UTF_CHECK;
        }
        rc = pcre2_match(ANTI_PATTERN_CODE(c->pattern), s, (PCRE2_SIZE)length, (PCRE2_SIZE)c->at,
                         (uint32_t)c->options, md, NULL);
        if (rc == PCRE2_ERROR_NOMATCH) {
            if ((c->options & RETRY) == 0) {
                c->at = length + 1;
                return DONE;
            }
            c->options = (c->options & ~(int64_t)RETRY) | PCRE2_NO_UTF_CHECK;
            c->at = next_character(c, c->at);
            continue;
        }
        if (rc < 0) {
            return failure(rc);
        }
        ov = pcre2_get_ovector_pointer(md);
        *from = c->at;
        *used = c->options;
        *start = (int64_t)ov[0];
        *end = (int64_t)ov[1];
        /* A match starts at or after the place its search starts, since
           `\K` in a lookaround is refused. The next search starts at
           its end. */
        c->at = *end;
        c->options = PCRE2_NO_UTF_CHECK | (*end == *start ? RETRY : 0);
        return FOUND;
    }
}

static struct anti_text slice(const struct anti_text *s, int64_t from,
                              int64_t to)
{
    struct anti_text t;

    if (to < from) {
        to = from;
    }
    t.ptr = s->ptr + from;
    t.len = to - from;
    return t;
}

static void fill(struct anti_match *m, const struct anti_cursor *c,
                 int64_t start, int64_t end, int64_t from, int64_t used)
{
    m->pattern = c->pattern;
    m->subject = c->subject;
    m->all = slice(&c->subject, start, end);
    m->pre = slice(&c->subject, c->last < start ? c->last : start, start);
    m->post = slice(&c->subject, end, c->subject.len);
    m->count = anti_rt_regex_group_count(c->pattern);
    m->from = from;
    m->options = used;
}

/* The count of the matches of the whole text. */
static int64_t count_all(const void *pattern, const unsigned char *s,
                         int64_t length, int64_t *total)
{
    struct anti_cursor c;
    pcre2_match_data *md = match_data(pattern, 0);
    int64_t start;
    int64_t end;
    int64_t from;
    int64_t used;
    int64_t status;

    cursor_reset(&c, pattern, s, length);
    *total = 0;
    while ((status = search(&c, md, &start, &end, &from, &used)) == FOUND) {
        (*total)++;
    }
    pcre2_match_data_free(md);
    return status;
}

/* DESIGN: a limit of 0 gives every match, a positive one that many from
   the start, and a negative one that many from the end. The search always
   runs from the start, so a negative limit counts the matches first and
   passes over all but the last of them. check counts them as well, so
   that a compiled pattern fails at the call and never in the walk. */
int64_t anti_rt_regex_begin(struct anti_cursor *c, const void *pattern,
                            const unsigned char *s, int64_t length,
                            int64_t limit, int64_t check)
{
    int64_t total = 0;
    int64_t status;

    cursor_reset(c, pattern, s, length);
    if (limit < 0 || check != 0) {
        status = count_all(pattern, s, length, &total);
        if (status < 0) {
            return status;
        }
    }
    if (limit > 0) {
        c->left = limit;
    } else if (limit < 0) {
        c->skip = total + limit > 0 ? total + limit : 0;
    }
    return DONE;
}

static int64_t advance(struct anti_cursor *c, struct anti_match *current,
                       pcre2_match_data *md)
{
    int64_t start;
    int64_t end;
    int64_t from;
    int64_t used;
    int64_t status;

    for (;;) {
        if (c->left == 0) {
            return DONE;
        }
        status = search(c, md, &start, &end, &from, &used);
        if (status != FOUND) {
            return status;
        }
        if (c->skip > 0) {
            c->skip--;
            continue;
        }
        if (c->left > 0) {
            c->left--;
        }
        fill(current, c, start, end, from, used);
        c->last = end;
        return FOUND;
    }
}

int64_t anti_rt_regex_next(struct anti_cursor *c,
                           struct anti_maybe_match *current)
{
    pcre2_match_data *md = match_data(c->pattern, 0);
    int64_t status = advance(c, &current->value, md);

    pcre2_match_data_free(md);
    if (status == FOUND) {
        current->has = 1;
    }
    return status;
}

/* DESIGN: the match a walk stands at is a `?Match` field, and the checker
   narrows names alone. `value` reads it through anti_rt_regex_current.
   It gives the address of the match the field holds, which lies at
   offset 0, or NULL where the walk has found none yet. */
struct anti_match *anti_rt_regex_current(struct anti_maybe_match *current)
{
    return current->has != 0 ? &current->value : NULL;
}

/* A walk of a search of the runtime's own. */
static int64_t walk_begin(struct anti_matches *it, const void *pattern,
                          const unsigned char *s, int64_t length,
                          int64_t limit)
{
    memset(&it->current, 0, sizeof it->current);
    return anti_rt_regex_begin(&it->cursor, pattern, s, length, limit, 0);
}

int64_t anti_rt_regex_first(const void *pattern, const unsigned char *s,
                            int64_t length, struct anti_maybe_match *out)
{
    struct anti_matches it;
    pcre2_match_data *md = match_data(pattern, 0);
    int64_t status;

    (void)walk_begin(&it, pattern, s, length, 1);
    status = advance(&it.cursor, &it.current, md);
    pcre2_match_data_free(md);
    memset(out, 0, sizeof *out);
    if (status == FOUND) {
        out->value = it.current;
        out->has = 1;
    }
    return status;
}

/* DESIGN: a match that is only tested asks PCRE2 for the whole match and
   no group, which is the cheaper path the specification names. */
int64_t anti_rt_regex_test(const void *pattern, const unsigned char *s,
                           int64_t length)
{
    pcre2_match_data *md = match_data(pattern, 0);
    int rc = pcre2_match(ANTI_PATTERN_CODE(pattern), s, (PCRE2_SIZE)length, 0,
                         0, md, NULL);

    pcre2_match_data_free(md);
    if (rc == PCRE2_ERROR_NOMATCH) {
        return DONE;
    }
    return rc < 0 ? failure(rc) : FOUND;
}

/* The text of group n in the ovector ov of the match m, and whether the
   group took part. */
static int64_t group_of(const struct anti_match *m, const PCRE2_SIZE *ov,
                        int64_t n, struct anti_text *out)
{
    if (ov[2 * n] == PCRE2_UNSET) {
        *out = slice(&m->subject, 0, 0);
        return DONE;
    }
    *out = slice(&m->subject, (int64_t)ov[2 * n], (int64_t)ov[2 * n + 1]);
    return FOUND;
}

/* The number of the group named by the length bytes at name that took
   part in the match with the ovector ov. When none did, the first group
   of that name. NO_GROUP when the pattern has no such name. */
static int64_t named_group(const void *pattern, const PCRE2_SIZE *ov,
                           const unsigned char *name, int64_t length)
{
    unsigned char text[ANTI_RT_REGEX_MESSAGE_ROOM];
    PCRE2_SPTR first = NULL;
    PCRE2_SPTR last = NULL;
    PCRE2_SPTR entry;
    uint32_t size = 0;
    int64_t number;

    if (length <= 0 || length >= (int64_t)sizeof text) {
        return NO_GROUP;
    }
    memcpy(text, name, (size_t)length);
    text[length] = '\0';
    if (pcre2_substring_nametable_scan(ANTI_PATTERN_CODE(pattern), text,
                                       &first, &last) < 0) {
        return NO_GROUP;
    }
    (void)pcre2_pattern_info(ANTI_PATTERN_CODE(pattern),
                             PCRE2_INFO_NAMEENTRYSIZE, &size);
    for (entry = first; entry <= last; entry += size) {
        number = (int64_t)((entry[0] << 8) | entry[1]);
        if (ov[2 * number] != PCRE2_UNSET) {
            return number;
        }
    }
    return (int64_t)((first[0] << 8) | first[1]);
}

/* The search that found m, run again with room for every group. */
static pcre2_match_data *search_again(const struct anti_match *m)
{
    pcre2_match_data *md = match_data(m->pattern, 1);
    int rc = pcre2_match(ANTI_PATTERN_CODE(m->pattern), m->subject.ptr,
                         (PCRE2_SIZE)m->subject.len, (PCRE2_SIZE)m->from,
                         (uint32_t)m->options, md, NULL);

    if (rc < 0) {
        (void)failure(rc);
        anti_rt_fail_abort("anti: a search of a pattern found its match "
                           "no longer");
    }
    return md;
}

/* DESIGN: a match holds the search that found it, not the places of its
   groups. It is so a value of fixed size that owns no memory. group runs
   that search again with room for every group. The search starts where
   the first one did with the same options, so it finds the same match. */
int64_t anti_rt_regex_group(const struct anti_match *m, int64_t n,
                            struct anti_text *out)
{
    pcre2_match_data *md;
    int64_t status;

    if (n < 0 || n > m->count) {
        *out = slice(&m->subject, 0, 0);
        return NO_GROUP;
    }
    if (n == 0) {
        *out = m->all;
        return FOUND;
    }
    md = search_again(m);
    status = group_of(m, pcre2_get_ovector_pointer(md), n, out);
    pcre2_match_data_free(md);
    return status;
}

int64_t anti_rt_regex_group_named(const struct anti_match *m,
                                  const unsigned char *name, int64_t length,
                                  struct anti_text *out)
{
    pcre2_match_data *md = search_again(m);
    const PCRE2_SIZE *ov = pcre2_get_ovector_pointer(md);
    int64_t number = named_group(m->pattern, ov, name, length);
    int64_t status = NO_GROUP;

    *out = slice(&m->subject, 0, 0);
    if (number != NO_GROUP) {
        status = group_of(m, ov, number, out);
    }
    pcre2_match_data_free(md);
    return status;
}

/* Whether every group the template names is a group of the pattern. */
int64_t anti_rt_regex_template_fits(const void *pattern,
                                    const unsigned char *bytes,
                                    int64_t length)
{
    struct anti_rt_piece piece;
    int64_t offset = 0;
    int64_t count = anti_rt_regex_group_count(pattern);

    while (anti_rt_regex_piece(bytes, length, &offset, &piece)) {
        if ((piece.kind == ANTI_RT_PIECE_NUMBER && piece.number > count) ||
            (piece.kind == ANTI_RT_PIECE_NAME &&
             anti_rt_regex_group_number(pattern, bytes + piece.start,
                                        piece.length) < 0)) {
            return 0;
        }
    }
    return 1;
}

/* Bytes that grow as they are appended to, in memory of the C library,
   in the layout of anti.regex.Growing. */
struct growing {
    unsigned char *bytes;
    int64_t length;
    int64_t capacity;
};

static void grow_append(struct growing *g, const unsigned char *bytes,
                        int64_t length)
{
    if (length <= 0) {
        return;
    }
    if (g->length + length > g->capacity) {
        int64_t capacity = g->capacity == 0 ? 64 : g->capacity;
        unsigned char *grown;
        while (capacity < g->length + length) {
            capacity *= 2;
        }
        grown = realloc(g->bytes, (size_t)capacity);
        if (grown == NULL) {
            out_of_memory();
        }
        g->bytes = grown;
        g->capacity = capacity;
    }
    memcpy(g->bytes + g->length, bytes, (size_t)length);
    g->length += length;
}

/* Append the template with the groups of the match m, whose ovector ov
   holds every group. */
static void expand(struct growing *g, const struct anti_match *m,
                   const PCRE2_SIZE *ov, const unsigned char *bytes,
                   int64_t length)
{
    struct anti_rt_piece piece;
    struct anti_text group;
    int64_t offset = 0;
    int64_t number;

    while (anti_rt_regex_piece(bytes, length, &offset, &piece)) {
        switch (piece.kind) {
        case ANTI_RT_PIECE_TEXT:
            grow_append(g, bytes + piece.start, piece.length);
            break;
        case ANTI_RT_PIECE_NUMBER:
        case ANTI_RT_PIECE_NAME:
            number = piece.kind == ANTI_RT_PIECE_NUMBER
                         ? piece.number
                         : named_group(m->pattern, ov, bytes + piece.start,
                                       piece.length);
            if (number >= 0 && number <= m->count) {
                (void)group_of(m, ov, number, &group);
                grow_append(g, group.ptr, group.len);
            }
            break;
        }
    }
}

/* DESIGN: `replace` with a template writes a new text in memory of the C
   library, which the program frees with `free(result.ptr)`, as it frees
   an `f"..."`. The text is new even when nothing matched, so the free is
   the same on every path. The groups of every match come from the search
   itself, whose match data has room for all of them. */
int64_t anti_rt_regex_replace(const void *pattern, const unsigned char *s,
                              int64_t length, const unsigned char *bytes,
                              int64_t template_length, int64_t limit,
                              struct anti_text *out)
{
    struct anti_matches it;
    struct growing g = {NULL, 0, 0};
    pcre2_match_data *md;
    int64_t rest = 0;
    int64_t status = walk_begin(&it, pattern, s, length, limit);

    out->ptr = NULL;
    out->len = 0;
    if (status < 0) {
        return status;
    }
    md = match_data(pattern, 1);
    while ((status = advance(&it.cursor, &it.current, md)) == FOUND) {
        const struct anti_match *m = &it.current;
        int64_t start = m->all.ptr - s;
        grow_append(&g, s + rest, start - rest);
        expand(&g, m, pcre2_get_ovector_pointer(md), bytes, template_length);
        rest = start + m->all.len;
    }
    pcre2_match_data_free(md);
    if (status < 0) {
        free(g.bytes);
        return status;
    }
    grow_append(&g, s + rest, length - rest);
    out->ptr = g.bytes;
    out->len = g.length;
    return DONE;
}

/* What `patch` gives beside the count of the places it patched. */
#define MISFIT (-2)
#define MISSING (-3)

/* DESIGN: `patch` with a pattern writes with at offset at inside the
   group into of every match the limit takes. The data never changes
   length. into 0 takes the one group of a pattern with one, or the whole
   match of a pattern without groups. A pattern with more needs into, by
   number or by the name of name_length bytes. The walk runs twice over
   the same matches: once to count and to find that every span takes
   with, once to write. The limit of a search, a missing group and a with
   that does not fit are so found before any byte changes. A match whose group did not take part is
   passed over and not counted. */
int64_t anti_rt_regex_patch(const void *pattern, unsigned char *data,
                            int64_t length, const unsigned char *with,
                            int64_t with_length, int64_t into,
                            const unsigned char *name, int64_t name_length,
                            int64_t at, int64_t limit)
{
    int64_t count = anti_rt_regex_group_count(pattern);
    int64_t group = into;
    int64_t places = 0;
    int pass;

    if (name_length > 0) {
        group = anti_rt_regex_group_number(pattern, name, name_length);
        if (group < 0) {
            return MISSING;
        }
    } else if (into == 0) {
        if (count > 1) {
            return MISSING;
        }
        group = count;
    } else if (into < 0 || into > count) {
        return MISSING;
    }
    if (at < 0) {
        return MISFIT;
    }
    for (pass = 0; pass < 2; pass++) {
        struct anti_matches it;
        pcre2_match_data *md = match_data(pattern, 1);
        int64_t status = walk_begin(&it, pattern, data, length, limit);
        places = 0;
        while (status >= 0 &&
               (status = advance(&it.cursor, &it.current, md)) == FOUND) {
            const PCRE2_SIZE *ov = pcre2_get_ovector_pointer(md);
            int64_t start = (int64_t)ov[2 * group];
            int64_t end = (int64_t)ov[2 * group + 1];
            if (ov[2 * group] == PCRE2_UNSET) {
                continue;
            }
            if (at + with_length > end - start) {
                status = MISFIT;
                break;
            }
            if (pass == 1 && with_length > 0) {
                memcpy(data + start + at, with, (size_t)with_length);
            }
            places++;
        }
        pcre2_match_data_free(md);
        if (status < 0) {
            return status;
        }
    }
    return places;
}

/* The first place from from on where the find_length bytes at find stand
   in the length bytes at data, or -1. */
static int64_t find_bytes(const unsigned char *data, int64_t length,
                          const unsigned char *find, int64_t find_length,
                          int64_t from)
{
    int64_t i;

    for (i = from; i + find_length <= length; i++) {
        if (data[i] == find[0] &&
            memcmp(data + i, find, (size_t)find_length) == 0) {
            return i;
        }
    }
    return -1;
}

/* DESIGN: `patch` with a byte sequence writes with at offset at inside
   every place the sequence stands, left to right and without overlap. A
   limit takes places as it takes matches. The span is the whole
   place, so a with that does not fit is found before any byte changes.
   An empty sequence stands nowhere, so it patches nothing. */
int64_t anti_rt_bytes_patch(unsigned char *data, int64_t length,
                            const unsigned char *find, int64_t find_length,
                            const unsigned char *with, int64_t with_length,
                            int64_t at, int64_t limit)
{
    int64_t total = 0;
    int64_t skip = 0;
    int64_t places = 0;
    int64_t i;

    if (at < 0 || at + with_length > find_length) {
        return MISFIT;
    }
    if (find_length == 0) {
        return 0;
    }
    if (limit < 0) {
        for (i = find_bytes(data, length, find, find_length, 0); i >= 0;
             i = find_bytes(data, length, find, find_length, i + find_length)) {
            total++;
        }
        skip = total + limit > 0 ? total + limit : 0;
    }
    for (i = find_bytes(data, length, find, find_length, 0);
         i >= 0 && (limit <= 0 || places < limit);
         i = find_bytes(data, length, find, find_length, i + find_length)) {
        if (skip > 0) {
            skip--;
            continue;
        }
        if (with_length > 0) {
            memcpy(data + i + at, with, (size_t)with_length);
        }
        places++;
    }
    return places;
}

/* DESIGN: a text that grows, as anti.regex.Growing holds it. `replace`
   with a function writes its result into one without anti.text, so a
   program of patterns links nothing it does not name. */
void anti_rt_regex_append(struct growing *g, const unsigned char *bytes,
                          int64_t length)
{
    grow_append(g, bytes, length);
}

/* The bytes of g, which the caller frees, and g empty. */
struct anti_text anti_rt_regex_take(struct growing *g)
{
    struct anti_text t;

    t.ptr = g->bytes;
    t.len = g->length;
    g->bytes = NULL;
    g->length = 0;
    g->capacity = 0;
    return t;
}

/* DESIGN: the stops of a search stand where the program wrote the call,
   in the form of a failed check, `file:line: what`, and abort as one
   does. */
_Noreturn void anti_rt_regex_stop_limit(const unsigned char *file,
                                        int64_t length, int64_t line,
                                        const unsigned char *pattern,
                                        int64_t pattern_length)
{
    if (pattern_length == 0) {
        anti_rt_fail_abort("%.*s:%lld: a search reached the match limit of "
                           "its pattern", (int)length, (const char *)file,
                           (long long)line);
    }
    anti_rt_fail_abort("%.*s:%lld: the pattern `%.*s` reached the match "
                       "limit", (int)length, (const char *)file,
                       (long long)line, (int)pattern_length,
                       (const char *)pattern);
}

_Noreturn void anti_rt_regex_stop_group(const unsigned char *file,
                                        int64_t length, int64_t line,
                                        int64_t count, int64_t n,
                                        const unsigned char *what,
                                        int64_t what_length)
{
    anti_rt_fail_abort("%.*s:%lld: the pattern has %lld group%s, and "
                       "`%.*s(%lld)` names none of them", (int)length,
                       (const char *)file, (long long)line, (long long)count,
                       count == 1 ? "" : "s", (int)what_length,
                       (const char *)what, (long long)n);
}

_Noreturn void anti_rt_regex_stop_name(const unsigned char *file,
                                       int64_t length, int64_t line,
                                       const unsigned char *name,
                                       int64_t name_length)
{
    anti_rt_fail_abort("%.*s:%lld: the pattern has no group `%.*s`",
                       (int)length, (const char *)file, (long long)line,
                       (int)name_length, (const char *)name);
}

_Noreturn void anti_rt_regex_stop_template(const unsigned char *file,
                                           int64_t length, int64_t line,
                                           const unsigned char *bytes,
                                           int64_t template_length,
                                           const unsigned char *pattern,
                                           int64_t pattern_length)
{
    anti_rt_fail_abort("%.*s:%lld: the template `%.*s` names a group the "
                       "pattern `%.*s` lacks", (int)length,
                       (const char *)file, (long long)line,
                       (int)template_length, (const char *)bytes,
                       (int)pattern_length, (const char *)pattern);
}

/* DESIGN: a `ByteMatch` has the layout of a `Match`, and a `[]byte` the
   layout of a `str`. The searches of a `[]byte` are therefore the same
   functions, under names of their own, since anti.regex declares each
   external function with one signature. */
int64_t anti_rt_regex_first_bytes(const void *pattern, const unsigned char *s,
                                  int64_t length,
                                  struct anti_maybe_match *out)
{
    return anti_rt_regex_first(pattern, s, length, out);
}

int64_t anti_rt_regex_next_bytes(struct anti_cursor *c,
                                 struct anti_maybe_match *current)
{
    return anti_rt_regex_next(c, current);
}

struct anti_match *anti_rt_regex_current_bytes(
    struct anti_maybe_match *current)
{
    return anti_rt_regex_current(current);
}

int64_t anti_rt_regex_group_bytes(const struct anti_match *m, int64_t n,
                                  struct anti_text *out)
{
    return anti_rt_regex_group(m, n, out);
}

int64_t anti_rt_regex_group_named_bytes(const struct anti_match *m,
                                        const unsigned char *name,
                                        int64_t length, struct anti_text *out)
{
    return anti_rt_regex_group_named(m, name, length, out);
}

int64_t anti_rt_regex_replace_bytes(const void *pattern,
                                    const unsigned char *s, int64_t length,
                                    const unsigned char *bytes,
                                    int64_t template_length, int64_t limit,
                                    struct anti_text *out)
{
    return anti_rt_regex_replace(pattern, s, length, bytes, template_length,
                                 limit, out);
}

struct anti_text anti_rt_regex_take_bytes(struct growing *g)
{
    return anti_rt_regex_take(g);
}

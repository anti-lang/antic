#include "regex.h"

#include <stdlib.h>
#include <string.h>

/* The 8-bit library, linked statically, as src/native/ builds it. */
#define PCRE2_CODE_UNIT_WIDTH 8
#define PCRE2_STATIC
#include <pcre2.h>

/* DESIGN: a pattern of text compiles in UTF mode and without PCRE2_UCP, so
   `\d`, `\w` and `\s` mean their ASCII sets, as "Regular expressions" in
   docs/anti-language-additions.md asks. A pattern that starts with
   `(*UCP)` gives the three their Unicode meaning, which PCRE2 reads
   itself. The flags `i`, `m`, `s` and `x` are PCRE2's inline flags, so no
   option of the compile stands for one. */
#define TEXT_OPTIONS PCRE2_UTF

void *anti_rt_regex_compile(const unsigned char *bytes, int64_t length,
                            int32_t *code, int64_t *offset)
{
    int error = 0;
    PCRE2_SIZE at = 0;
    pcre2_code *compiled = pcre2_compile(bytes, (PCRE2_SIZE)length,
                                         TEXT_OPTIONS, &error, &at, NULL);

    if (compiled == NULL) {
        *code = (int32_t)error;
        *offset = (int64_t)at;
    }
    return compiled;
}

/* DESIGN: a byte pattern compiles without UTF mode, so `.` and every
   escape stand for bytes and nothing needs to be valid UTF-8, as "Bytes"
   in docs/anti-language-additions.md asks. PCRE2_NEVER_UCP and
   PCRE2_NEVER_UTF refuse `(*UCP)` and `(*UTF)`, so `\d`, `\w` and `\s`
   stay their ASCII sets. PCRE2_DOTALL makes `.` match any byte, the
   newline included, and `(?-s)` still turns that off. */
#define BYTE_OPTIONS (PCRE2_NEVER_UTF | PCRE2_NEVER_UCP | PCRE2_DOTALL)

/* The pattern that PCRE2 compiles for a byte pattern, and for each of its
   bytes the byte of the written pattern it comes from. */
struct wide {
    unsigned char *out;
    int64_t *from;
    int64_t n;
    int64_t room;
    int failed;
};

static void put(struct wide *w, unsigned char c, int64_t from)
{
    if (w->failed) {
        return;
    }
    if (w->n == w->room) {
        int64_t room = w->room == 0 ? 64 : 2 * w->room;
        unsigned char *out = realloc(w->out, (size_t)room);
        int64_t *map;
        if (out == NULL) {
            w->failed = 1;
            return;
        }
        w->out = out;
        map = realloc(w->from, (size_t)room * sizeof *map);
        if (map == NULL) {
            w->failed = 1;
            return;
        }
        w->from = map;
        w->room = room;
    }
    w->out[w->n] = c;
    w->from[w->n] = from;
    w->n++;
}

static void put_span(struct wide *w, const unsigned char *s, int64_t from,
                     int64_t to)
{
    int64_t i;

    for (i = from; i < to; i++) {
        put(w, s[i], i);
    }
}

/* The byte after the character that starts at i. */
static int64_t char_end(const unsigned char *s, int64_t n, int64_t i)
{
    i++;
    while (i < n && (s[i] & 0xC0) == 0x80) {
        i++;
    }
    return i;
}

static int is_alnum(unsigned char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z');
}

/* The character from i to end as one atom of its bytes, `(?:\xC3\xB3)`,
   so a quantifier after it repeats the whole character. */
static void put_wide(struct wide *w, const unsigned char *s, int64_t i,
                     int64_t end)
{
    put(w, '(', i);
    put(w, '?', i);
    put(w, ':', i);
    put_span(w, s, i, end);
    put(w, ')', i);
}

/* `\Q...\E` from the byte after the `Q`, written out as escaped ASCII
   and atoms of wide characters. Returns the byte after the `\E`. */
static int64_t put_quoted(struct wide *w, const unsigned char *s, int64_t n,
                          int64_t i)
{
    while (i < n && !(s[i] == '\\' && i + 1 < n && s[i + 1] == 'E')) {
        if (s[i] >= 0x80) {
            int64_t end = char_end(s, n, i);
            put_wide(w, s, i, end);
            i = end;
        } else {
            if (!is_alnum(s[i])) {
                put(w, '\\', i);
            }
            put(w, s[i], i);
            i++;
        }
    }
    return i < n ? i + 2 : n;
}

/* One member of a class. */
enum member_kind {
    MEMBER_CHAR,        /* a character, which may start a range */
    MEMBER_SET,         /* `\d`, `[:alpha:]` and the other sets */
    MEMBER_WIDE         /* a character outside ASCII */
};

/* The end of the escape of a class at i, the `\`, and its kind. */
static int64_t class_escape_end(const unsigned char *s, int64_t n, int64_t i,
                                enum member_kind *kind)
{
    int64_t j = i + 1;
    unsigned char e;
    int count;

    *kind = MEMBER_CHAR;
    if (j >= n) {
        return n;
    }
    e = s[j];
    if (e >= 0x80) {
        *kind = MEMBER_WIDE;
        return char_end(s, n, j);
    }
    j++;
    switch (e) {
    case 'd': case 'D': case 'w': case 'W': case 's': case 'S':
    case 'h': case 'H': case 'v': case 'V':
        *kind = MEMBER_SET;
        return j;
    case 'p': case 'P':
        *kind = MEMBER_SET;
        /* fall through */
    case 'x': case 'o': case 'N':
        if (j < n && s[j] == '{') {
            while (j < n && s[j] != '}') {
                j++;
            }
            return j < n ? j + 1 : n;
        }
        if (e == 'x') {
            for (count = 0; count < 2 && j < n && is_alnum(s[j]); count++) {
                j++;
            }
        } else if (e == 'p' || e == 'P') {
            j = j < n ? j + 1 : n;
        }
        return j;
    case 'c':
        return j < n ? j + 1 : n;
    default:
        if (e >= '0' && e <= '7') {
            for (count = 1; count < 3 && j < n && s[j] >= '0' && s[j] <= '7';
                 count++) {
                j++;
            }
        }
        return j;
    }
}

/* The end of the member of a class at i and its kind, for the members
   outside `\Q...\E`. */
static int64_t member_end(const unsigned char *s, int64_t n, int64_t i,
                          enum member_kind *kind)
{
    if (s[i] == '[' && i + 1 < n &&
        (s[i + 1] == ':' || s[i + 1] == '.' || s[i + 1] == '=')) {
        int64_t j = i + 2;
        while (j + 1 < n && s[j] != ']' &&
               !(s[j] == s[i + 1] && s[j + 1] == ']')) {
            j++;
        }
        if (j + 1 < n && s[j] == s[i + 1] && s[j + 1] == ']') {
            *kind = MEMBER_SET;
            return j + 2;
        }
    }
    if (s[i] == '\\') {
        return class_escape_end(s, n, i, kind);
    }
    if (s[i] >= 0x80) {
        *kind = MEMBER_WIDE;
        return char_end(s, n, i);
    }
    *kind = MEMBER_CHAR;
    return i + 1;
}

/* What a walk over a class does with each member. */
enum class_pass {
    PASS_SCAN,          /* find the end, the wide members and the errors */
    PASS_NARROW,        /* write the members that are no wide character */
    PASS_WIDE           /* write the wide characters as a choice */
};

/* What the scan of a class found. end is the byte after the `]`, or -1
   without one. wide counts the wide characters and narrow the other
   members. code is a refusal or 0, and at its byte. */
struct class_scan {
    int64_t end;
    int64_t wide;
    int64_t narrow;
    int32_t code;
    int64_t at;
};

/* The start of a wide character member at i, past its `\`. */
static int64_t wide_start(const unsigned char *s, int64_t i)
{
    return s[i] == '\\' ? i + 1 : i;
}

static void class_wide(struct wide *w, const unsigned char *s, int64_t start,
                       int64_t end, enum class_pass pass,
                       struct class_scan *scan)
{
    if (pass == PASS_WIDE) {
        if (scan->wide > 0) {
            put(w, '|', start);
        }
        put_span(w, s, start, end);
    }
    scan->wide++;
}

/* DESIGN: a class of a byte pattern that lists a character outside ASCII
   becomes a choice of byte sequences: `[óa]` is `(?:[a]|\xC3\xB3)`. The
   members stand in the order of the class, the ASCII ones first. A byte
   escape, `\x89`, is a byte and no character, so `[^\x00]` and
   `[\x80-\xFF]` stay classes of bytes. A negated class that holds a
   written character outside ASCII is refused, and so is a range with one
   at either end, since byte mode cannot know where a character starts in
   either. The walk runs three times: to scan, to write the ASCII members
   and to write the wide ones. */
static void walk_class(struct wide *w, const unsigned char *s, int64_t n,
                       int64_t i, enum class_pass pass,
                       struct class_scan *scan)
{
    int64_t j = i + 1;
    int negated = 0;
    int first = 1;

    scan->end = -1;
    scan->wide = 0;
    scan->narrow = 0;
    if (j < n && s[j] == '^') {
        negated = 1;
        j++;
    }
    while (j < n) {
        enum member_kind kind;
        int64_t start = j;
        int64_t end;
        if (s[j] == ']' && !first) {
            scan->end = j + 1;
            return;
        }
        first = 0;
        if (s[j] == '\\' && j + 1 < n && s[j + 1] == 'Q') {
            j += 2;
            while (j < n && !(s[j] == '\\' && j + 1 < n && s[j + 1] == 'E')) {
                if (s[j] >= 0x80) {
                    end = char_end(s, n, j);
                    if (negated && scan->code == 0) {
                        scan->code = ANTI_RT_REGEX_WIDE_NEGATED;
                        scan->at = j;
                    }
                    class_wide(w, s, j, end, pass, scan);
                    j = end;
                } else {
                    if (pass == PASS_NARROW) {
                        if (!is_alnum(s[j])) {
                            put(w, '\\', j);
                        }
                        put(w, s[j], j);
                    }
                    scan->narrow++;
                    j++;
                }
            }
            j = j < n ? j + 2 : n;
            continue;
        }
        end = member_end(s, n, j, &kind);
        if (kind == MEMBER_WIDE && negated && scan->code == 0) {
            scan->code = ANTI_RT_REGEX_WIDE_NEGATED;
            scan->at = wide_start(s, start);
        }
        if (kind != MEMBER_SET && end + 1 < n && s[end] == '-' &&
            s[end + 1] != ']') {
            enum member_kind high;
            int64_t stop = member_end(s, n, end + 1, &high);
            if ((kind == MEMBER_WIDE || high == MEMBER_WIDE) &&
                scan->code == 0) {
                scan->code = ANTI_RT_REGEX_WIDE_RANGE;
                scan->at = kind == MEMBER_WIDE ? wide_start(s, start)
                                               : wide_start(s, end + 1);
            }
            if (high != MEMBER_SET) {
                if (pass == PASS_NARROW) {
                    put_span(w, s, start, stop);
                }
                scan->narrow++;
                j = stop;
                continue;
            }
        }
        if (kind == MEMBER_WIDE) {
            class_wide(w, s, wide_start(s, start), end, pass, scan);
        } else {
            if (pass == PASS_NARROW) {
                put_span(w, s, start, end);
            }
            scan->narrow++;
        }
        j = end;
    }
}

/* Write the class at i, and return the byte after it, or -1 after a
   refusal, which *code and *offset then hold. */
static int64_t put_class(struct wide *w, const unsigned char *s, int64_t n,
                         int64_t i, int32_t *code, int64_t *offset)
{
    struct class_scan scan;

    memset(&scan, 0, sizeof scan);
    walk_class(w, s, n, i, PASS_SCAN, &scan);
    if (scan.code != 0) {
        *code = scan.code;
        *offset = scan.at;
        return -1;
    }
    /* A class without its `]` goes to PCRE2 as it stands, which names
       the error. */
    if (scan.end < 0 || scan.wide == 0) {
        int64_t end = scan.end < 0 ? n : scan.end;
        put_span(w, s, i, end);
        return end;
    }
    put(w, '(', i);
    put(w, '?', i);
    put(w, ':', i);
    if (scan.narrow > 0) {
        put(w, '[', i);
        walk_class(w, s, n, i, PASS_NARROW, &scan);
        put(w, ']', scan.end - 1);
        put(w, '|', i);
    }
    walk_class(w, s, n, i, PASS_WIDE, &scan);
    put(w, ')', scan.end - 1);
    return scan.end;
}

/* Write into w the pattern PCRE2 compiles for the byte pattern s, and
   give 0 after a refusal. A character outside ASCII becomes an atom of
   its bytes, and a class that holds one a choice. */
static int widen(struct wide *w, const unsigned char *s, int64_t n,
                 int32_t *code, int64_t *offset)
{
    int64_t i = 0;

    while (i < n) {
        unsigned char c = s[i];
        if (c == '\\' && i + 1 < n) {
            if (s[i + 1] == 'Q') {
                i = put_quoted(w, s, n, i + 2);
            } else if (s[i + 1] >= 0x80) {
                int64_t end = char_end(s, n, i + 1);
                put_wide(w, s, i + 1, end);
                i = end;
            } else {
                int64_t end = s[i + 1] == 'c' && i + 2 < n ? i + 3 : i + 2;
                put_span(w, s, i, end);
                i = end;
            }
        } else if (c == '[') {
            i = put_class(w, s, n, i, code, offset);
            if (i < 0) {
                return 0;
            }
        } else if (c == '(' && i + 2 < n && s[i + 1] == '?' &&
                   s[i + 2] == '#') {
            int64_t end = i + 3;
            while (end < n && s[end] != ')') {
                end++;
            }
            end = end < n ? end + 1 : n;
            put_span(w, s, i, end);
            i = end;
        } else if (c >= 0x80) {
            int64_t end = char_end(s, n, i);
            put_wide(w, s, i, end);
            i = end;
        } else {
            put(w, c, i);
            i++;
        }
    }
    return 1;
}

void *anti_rt_regex_compile_bytes(const unsigned char *bytes, int64_t length,
                                  int32_t *code, int64_t *offset)
{
    struct wide w;
    pcre2_code *compiled = NULL;
    int error = 0;
    PCRE2_SIZE at = 0;

    memset(&w, 0, sizeof w);
    if (!widen(&w, bytes, length, code, offset)) {
        free(w.out);
        free(w.from);
        return NULL;
    }
    if (w.failed) {
        free(w.out);
        free(w.from);
        *code = PCRE2_ERROR_NOMEMORY;
        *offset = 0;
        return NULL;
    }
    compiled = pcre2_compile(w.n == 0 ? (PCRE2_SPTR) "" : w.out,
                             (PCRE2_SIZE)w.n, BYTE_OPTIONS, &error, &at,
                             NULL);
    if (compiled == NULL) {
        *code = (int32_t)error;
        *offset = (int64_t)at < w.n ? w.from[at] : length;
    }
    free(w.out);
    free(w.from);
    return compiled;
}

int anti_rt_regex_is_bytes(const void *compiled)
{
    uint32_t options = 0;

    (void)pcre2_pattern_info(compiled, PCRE2_INFO_ALLOPTIONS, &options);
    return (options & PCRE2_UTF) == 0;
}

void anti_rt_regex_free(void *compiled)
{
    pcre2_code_free(compiled);
}

void anti_rt_regex_message_into(int32_t code, unsigned char *out,
                                size_t room)
{
    if (room == 0) {
        return;
    }
    out[0] = '\0';
    if (code == ANTI_RT_REGEX_WIDE_NEGATED ||
        code == ANTI_RT_REGEX_WIDE_RANGE) {
        const char *text =
            code == ANTI_RT_REGEX_WIDE_NEGATED
                ? "a byte pattern cannot negate a class that holds a "
                  "character outside ASCII, which text mode can"
                : "a range of a byte pattern cannot reach beyond ASCII, "
                  "which text mode can";
        size_t n = strlen(text) < room - 1 ? strlen(text) : room - 1;
        memcpy(out, text, n);
        out[n] = '\0';
        return;
    }
    /* A message longer than room comes back cut and ended by a NUL, with
       PCRE2_ERROR_NOMEMORY, and a number PCRE2 does not know leaves out
       empty. Both are what the caller shows. */
    (void)pcre2_get_error_message(code, out, room);
}

int64_t anti_rt_regex_group_count(const void *compiled)
{
    uint32_t count = 0;

    (void)pcre2_pattern_info(compiled, PCRE2_INFO_CAPTURECOUNT, &count);
    return (int64_t)count;
}

/* The longest group name PCRE2 accepts, and room for its NUL. */
#define NAME_ROOM 256

int64_t anti_rt_regex_group_number(const void *compiled,
                                   const unsigned char *name, int64_t length)
{
    unsigned char text[NAME_ROOM];
    int number;

    if (length <= 0 || length >= NAME_ROOM) {
        return -1;
    }
    memcpy(text, name, (size_t)length);
    text[length] = '\0';
    number = pcre2_substring_number_from_name(compiled, text);
    if (number == PCRE2_ERROR_NOUNIQUESUBSTRING) {
        PCRE2_SPTR first = NULL;
        PCRE2_SPTR last = NULL;
        (void)pcre2_substring_nametable_scan(compiled, text, &first, &last);
        /* An entry starts with its number in two bytes, high first. */
        return first == NULL ? -1 : (int64_t)((first[0] << 8) | first[1]);
    }
    return number < 0 ? -1 : (int64_t)number;
}

static int is_digit(unsigned char c)
{
    return c >= '0' && c <= '9';
}

static int is_name_start(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

/* The largest group number a template reads. A larger one names a group
   no pattern has, and stays above every count. */
#define NUMBER_CAP 1000000000

/* The number of the digits from at to end, capped. */
static int64_t digits_value(const unsigned char *bytes, int64_t at,
                            int64_t end)
{
    int64_t value = 0;

    for (; at < end; at++) {
        value = value * 10 + (bytes[at] - '0');
        if (value > NUMBER_CAP) {
            value = NUMBER_CAP;
        }
    }
    return value;
}

/* DESIGN: `$` reads every digit that follows it, so `$10` is group 10,
   and `${1}0` is group 1 and a `0`. `${name}` names a group by its name
   and `$$` writes one `$`. A `$` that starts none of these forms is
   written as it stands, as the smallest reading that refuses nothing. */
int anti_rt_regex_piece(const unsigned char *bytes, int64_t length,
                        int64_t *offset, struct anti_rt_piece *piece)
{
    int64_t at = *offset;
    int64_t end;

    if (at >= length) {
        return 0;
    }
    piece->kind = ANTI_RT_PIECE_TEXT;
    piece->number = 0;
    if (bytes[at] != '$') {
        end = at;
        while (end < length && bytes[end] != '$') {
            end++;
        }
        piece->start = at;
        piece->length = end - at;
        *offset = end;
        return 1;
    }
    /* `$$`, and a `$` that ends the template. */
    piece->start = at + (at + 1 < length && bytes[at + 1] == '$' ? 1 : 0);
    piece->length = 1;
    *offset = piece->start + 1;
    if (at + 1 >= length) {
        return 1;
    }
    if (is_digit(bytes[at + 1])) {
        end = at + 1;
        while (end < length && is_digit(bytes[end])) {
            end++;
        }
        piece->kind = ANTI_RT_PIECE_NUMBER;
        piece->number = digits_value(bytes, at + 1, end);
        *offset = end;
        return 1;
    }
    if (bytes[at + 1] != '{') {
        return 1;
    }
    end = at + 2;
    if (end < length && is_digit(bytes[end])) {
        while (end < length && is_digit(bytes[end])) {
            end++;
        }
        if (end < length && bytes[end] == '}') {
            piece->kind = ANTI_RT_PIECE_NUMBER;
            piece->number = digits_value(bytes, at + 2, end);
            *offset = end + 1;
        }
        return 1;
    }
    if (end < length && is_name_start(bytes[end])) {
        while (end < length &&
               (is_name_start(bytes[end]) || is_digit(bytes[end]))) {
            end++;
        }
        if (end < length && bytes[end] == '}') {
            piece->kind = ANTI_RT_PIECE_NAME;
            piece->start = at + 2;
            piece->length = end - (at + 2);
            *offset = end + 1;
        }
    }
    return 1;
}

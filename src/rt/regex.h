/* The compile of a pattern with PCRE2, which antic and the runtime share.

   DESIGN: one definition read from both sides. antic checks every pattern
   literal with it, and the program compiles the same literal with it at
   start. The two read a pattern with the same options and the same
   library. antic compiles src/rt/regex.c into its own program. */
#ifndef ANTI_RT_REGEX_H
#define ANTI_RT_REGEX_H

#include <stddef.h>
#include <stdint.h>

/* DESIGN: the handle of a compiled pattern holds the code PCRE2 compiled,
   the pattern as it was written and whether it searches bytes. `==` of a
   Regex compares the text and the mode, which PCRE2 keeps no copy of, so
   the handle keeps them. ANTI_PATTERN_CODE gives the code to PCRE2. */
struct anti_pattern {
    void *code;
    unsigned char *text;
    int64_t length;
    int64_t bytes;
};

#define ANTI_PATTERN_CODE(p) (((const struct anti_pattern *)(p))->code)

/* The compiled form of the pattern of length bytes at bytes, or NULL when
   it does not compile. Then *code holds the error number of PCRE2 and
   *offset the byte of the pattern where PCRE2 stopped. */
void *anti_rt_regex_compile(const unsigned char *bytes, int64_t length,
                            int32_t *code, int64_t *offset);

/* The compiled form of the byte pattern of length bytes at bytes, or NULL
   when it does not compile. *code and *offset then hold what
   anti_rt_regex_compile writes. A character outside ASCII stands for its UTF-8
   bytes, and a class that holds one becomes a choice of byte sequences.
   A negated class that holds one is refused with
   ANTI_RT_REGEX_WIDE_NEGATED, and a range that reaches one with
   ANTI_RT_REGEX_WIDE_RANGE, at the character. */
void *anti_rt_regex_compile_bytes(const unsigned char *bytes, int64_t length,
                                  int32_t *code, int64_t *offset);

/* The numbers of the two refusals of a byte pattern. PCRE2 numbers its
   own errors of a compile from 101 to below 300, so these meet none. */
#define ANTI_RT_REGEX_WIDE_NEGATED 400
#define ANTI_RT_REGEX_WIDE_RANGE 401

/* Whether the compiled pattern searches bytes. */
int anti_rt_regex_is_bytes(const void *compiled);

/* Give back what anti_rt_regex_compile made. */
void anti_rt_regex_free(void *compiled);

/* Write the message PCRE2 gives for the error number code into out, a
   buffer of room bytes, ended by a NUL. The message is cut to fit. */
void anti_rt_regex_message_into(int32_t code, unsigned char *out,
                                size_t room);

/* The room one message of PCRE2 takes, its NUL included. */
#define ANTI_RT_REGEX_MESSAGE_ROOM 256

/* The number of groups of a compiled pattern, the whole match not
   counted. */
int64_t anti_rt_regex_group_count(const void *compiled);

/* The number of the group of a compiled pattern named by the length bytes
   at name, or -1 when it has none of that name. `(?J)` allows two groups
   of one name, and then it is the first. */
int64_t anti_rt_regex_group_number(const void *compiled,
                                   const unsigned char *name, int64_t length);

/* The kinds of piece a replacement template is made of. */
enum anti_rt_piece_kind {
    ANTI_RT_PIECE_TEXT,     /* bytes written as they stand */
    ANTI_RT_PIECE_NUMBER,   /* `$1` and `${1}`: a group by its number */
    ANTI_RT_PIECE_NAME      /* `${name}`: a group by its name */
};

/* One piece of a template. start and length give the bytes of a text
   piece and the name of a named group, both in the template. */
struct anti_rt_piece {
    enum anti_rt_piece_kind kind;
    int64_t start;
    int64_t length;
    int64_t number;
};

/* Read the piece of the template of length bytes at bytes that starts at
   *offset into *piece and move *offset past it. Returns 0 at the end of
   the template and 1 otherwise. */
int anti_rt_regex_piece(const unsigned char *bytes, int64_t length,
                        int64_t *offset, struct anti_rt_piece *piece);

#endif

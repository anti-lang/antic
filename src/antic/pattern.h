#ifndef ANTIC_PATTERN_H
#define ANTIC_PATTERN_H

/* The interface of pattern.c, the checks of a pattern literal: the
   compile with PCRE2, the count and the numbers of its groups, the least
   length of a match and the safety check `exponential-pattern`, and the
   pieces of a replacement template. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The room a message of PCRE2 takes in a diagnostic. */
#define ANTI_PATTERN_MESSAGE 112

/* A span of the pattern, in bytes. */
struct pattern_span {
    size_t start;
    size_t end;
};

/* Whether the pattern of length bytes compiles, as a byte pattern when
   bytes_mode holds. When it does not, *offset is the byte PCRE2 names and
   message holds its text, ended by a NUL and cut to room bytes. */
bool pattern_compiles(const char *bytes, size_t length, bool bytes_mode,
                      size_t *offset, char *message, size_t room);

/* The number of groups of the pattern of length bytes, which compiles. */
int64_t pattern_group_count(const char *bytes, size_t length,
                            bool bytes_mode);

/* The number of the group of the pattern named by the name_length bytes at
   name, or -1 when the pattern has no group of that name. */
int64_t pattern_group_number(const char *bytes, size_t length,
                             bool bytes_mode, const char *name,
                             size_t name_length);

/* The fewest bytes that group of the pattern, which compiles, can match,
   0 for the whole match. It may be below the true least, never above. */
int64_t pattern_least_bytes(const char *bytes, size_t length,
                            int64_t group);

/* The kinds of piece a replacement template is made of: text, a group by
   its number, `$1` and `${1}`, and a group by its name, `${name}`. */
enum pattern_piece_kind {
    PATTERN_PIECE_TEXT,
    PATTERN_PIECE_NUMBER,
    PATTERN_PIECE_NAME
};

/* One piece of a template. start and length give its bytes in the
   template, and the name of a named group. */
struct pattern_piece {
    enum pattern_piece_kind kind;
    size_t start;
    size_t length;
    int64_t number;
};

/* Read the piece of the template of length bytes at bytes that starts at
   *offset into *piece, as the runtime reads it, and move *offset past it.
   Returns false at the end of the template. */
bool pattern_template_piece(const char *bytes, size_t length, size_t *offset,
                            struct pattern_piece *piece);

/* Whether the pattern, which compiles, nests a repeat inside another over
   text that overlaps, which can take exponential time. Then *inner is the
   inner repeat and *outer the one around it. */
bool pattern_exponential(const char *bytes, size_t length,
                         struct pattern_span *inner,
                         struct pattern_span *outer);

#endif

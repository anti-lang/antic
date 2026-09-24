#ifndef ANTIC_PATTERN_H
#define ANTIC_PATTERN_H

#include <stdbool.h>
#include <stddef.h>

/* The checks of a pattern literal: the compile with PCRE2 and the safety
   check `exponential-pattern`. */

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
long pattern_group_count(const char *bytes, size_t length, bool bytes_mode);

/* The number of the group of the pattern named by the name_length bytes at
   name, or -1 when the pattern has no group of that name. */
long pattern_group_number(const char *bytes, size_t length, bool bytes_mode,
                          const char *name, size_t name_length);

/* The fewest bytes that group of the pattern, which compiles, can match,
   0 for the whole match. It may be below the true least, never above. */
long pattern_least_bytes(const char *bytes, size_t length, long group);

/* Whether the pattern, which compiles, nests a repeat inside another over
   text that overlaps, which can take exponential time. Then *inner is the
   inner repeat and *outer the one around it. */
bool pattern_exponential(const char *bytes, size_t length,
                         struct pattern_span *inner,
                         struct pattern_span *outer);

#endif

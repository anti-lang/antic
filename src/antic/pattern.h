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

/* Whether the pattern of length bytes compiles. When it does not,
   *offset is the byte PCRE2 names and message holds its text, ended by a
   NUL and cut to room bytes. */
bool pattern_compiles(const char *bytes, size_t length, size_t *offset,
                      char *message, size_t room);

/* Whether the pattern, which compiles, nests a repeat inside another over
   text that overlaps, which can take exponential time. Then *inner is the
   inner repeat and *outer the one around it. */
bool pattern_exponential(const char *bytes, size_t length,
                         struct pattern_span *inner,
                         struct pattern_span *outer);

#endif

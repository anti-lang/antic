#ifndef ANTI_RT_JSON_H
#define ANTI_RT_JSON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* A scanner of JSON text, at the level of tokens and positions. It knows
   no descriptor and builds no tree. Object.deserialize reads into objects
   on top of it. anti bind builds a tree of the output of clang with it.

   Every function reads only between at and end, and a function that
   fails leaves at somewhere inside that range. A caller that wants to
   look ahead copies the scanner, which is two pointers. */
struct anti_json {
    const unsigned char *at;
    const unsigned char *end;
};

/* How deep anti_rt_json_skip lets objects and arrays nest. */
#define ANTI_JSON_DEPTH 64

/* Move at past white space. */
void anti_rt_json_space(struct anti_json *s);

/* Skip white space and take c when it comes next. */
bool anti_rt_json_take(struct anti_json *s, unsigned char c);

/* Skip white space and take the bytes of word when they come next. */
bool anti_rt_json_word(struct anti_json *s, const char *word);

/* Skip white space and read a string. The decoded bytes go to out when
   out is not NULL, and length counts them in either case. The read
   refuses a string that needs more than room bytes in out. It refuses an
   unterminated string, a control byte and an unknown escape, and a \u
   escape of a surrogate half. */
bool anti_rt_json_string(struct anti_json *s, unsigned char *out,
                         size_t room, size_t *length);

/* Skip white space and take the bytes that may form a number. They stay
   in the input. The span is not checked against the grammar. */
bool anti_rt_json_number(struct anti_json *s, const unsigned char **start,
                         int64_t *length);

/* Whether the bytes form a JSON number. */
bool anti_rt_json_valid_number(const unsigned char *start, int64_t length);

/* The integer the bytes write, refused unless they are an integer of the
   JSON grammar that an int64_t holds. */
bool anti_rt_json_integer(const unsigned char *start, int64_t length,
                          int64_t *value);

/* Skip one value of any kind, which starts depth levels deep. A value
   that nests deeper than ANTI_JSON_DEPTH is refused. */
bool anti_rt_json_skip(struct anti_json *s, int depth);

#endif

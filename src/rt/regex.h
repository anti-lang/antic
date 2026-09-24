/* The compile of a pattern with PCRE2, which antic and the runtime share.

   DESIGN: one definition read from both sides. antic checks every pattern
   literal with it, and the program compiles the same literal with it at
   start. The two read a pattern with the same options and the same
   library. antic compiles src/rt/regex.c into its own program. */
#ifndef ANTI_RT_REGEX_H
#define ANTI_RT_REGEX_H

#include <stddef.h>
#include <stdint.h>

/* The compiled form of the pattern of length bytes at bytes, or NULL when
   it does not compile. Then *code holds the error number of PCRE2 and
   *offset the byte of the pattern where PCRE2 stopped. */
void *anti_rt_regex_compile(const unsigned char *bytes, int64_t length,
                            int32_t *code, int64_t *offset);

/* Give back what anti_rt_regex_compile made. */
void anti_rt_regex_free(void *compiled);

/* Write the message PCRE2 gives for the error number code into out, a
   buffer of room bytes, ended by a NUL. The message is cut to fit. */
void anti_rt_regex_message_into(int32_t code, unsigned char *out,
                                size_t room);

/* The room one message of PCRE2 takes, its NUL included. */
#define ANTI_RT_REGEX_MESSAGE_ROOM 256

#endif

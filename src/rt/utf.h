/* Text conversions of the runtime entry, and the UTF-8 encoder and
   decoder of the runtime and antic.

   DESIGN: one definition read from both sides. The lexer of antic encodes
   an escape and checks a source with the functions below, and antic
   compiles src/rt/utf.c for it. They use no platform functions, so the
   unit tests check them on any host. */
#ifndef ANTI_UTF_H
#define ANTI_UTF_H

#include <stddef.h>
#include <stdint.h>

size_t anti_rt_utf8_repair(const unsigned char *in, size_t n,
                           unsigned char *out);
size_t anti_rt_utf16_to_utf8(const uint16_t *in, size_t n, unsigned char *out);
size_t anti_rt_split_command_line(const uint16_t *line, uint16_t *out);

/* The UTF-8 bytes of the scalar value c, written to out, which holds 4.
   Returns their count. */
size_t anti_rt_utf8_encode(uint32_t c, unsigned char *out);

/* The scalar value of the well-formed sequence that starts in, of n
   bytes. Its length goes to length, which is 0 when the bytes start no
   well-formed sequence. */
uint32_t anti_rt_utf8_decode(const unsigned char *in, size_t n, size_t *length);

#endif

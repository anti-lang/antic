/* Text conversions of the runtime entry, and the UTF-8 encoder and
   decoder of the runtime and antic.

   DESIGN: one definition read from both sides. The lexer of antic encodes
   an escape and checks a source with the functions below, and antic
   compiles src/rt/utf.c for it. They use no platform functions, so the
   unit tests check them on any host. */

#ifndef ANTI_RT_UTF_H
#define ANTI_RT_UTF_H

#include <stddef.h>
#include <stdint.h>

/* The n bytes at in as UTF-8 into out, without a NUL. Each maximal
   subpart of an ill-formed sequence becomes one U+FFFD. out holds
   3 * n bytes. Gives the count of bytes written. */
size_t anti_rt_utf8_repair(const unsigned char *in, size_t n,
                           unsigned char *out);

/* The n UTF-16 units at in as UTF-8 into out, without a NUL. A surrogate
   pair gives one scalar value, and any other surrogate U+FFFD. out holds
   3 * n bytes. Gives the count of bytes written. */
size_t anti_rt_utf16_to_utf8(const uint16_t *in, size_t n, unsigned char *out);

/* Split the Windows command line at line, ended by a 0 unit, by the
   rules of the Microsoft C startup code. out takes each argument followed
   by a 0 unit, and holds 2 * n + 2 units for a line of n units. Gives
   the count of arguments, the name of the program included. */
size_t anti_rt_split_command_line(const uint16_t *line, uint16_t *out);

/* DESIGN: decision 7 of "Decisions of 2026-10-11". A file name keeps
   what the system holds, so a name that a listing gives opens its file.
   Windows holds a name as UTF-16, and a surrogate of it that pairs with
   no other becomes the three bytes of its value, as any other unit below
   U+10000 does, which is no UTF-8. The two functions below go between
   the units of a name and those bytes, and each gives back what the
   other took. */

/* The bytes of the n units at in, into the room bytes at out, without
   a NUL. Gives their count, or SIZE_MAX when they do not fit. */
size_t anti_rt_name_bytes(const uint16_t *in, size_t n, unsigned char *out,
                          size_t room);

/* The units of the n bytes at in, into out, which holds n units. Gives
   their count, or SIZE_MAX for bytes that are neither UTF-8 nor the three
   bytes of a surrogate. */
size_t anti_rt_name_units(const unsigned char *in, size_t n, uint16_t *out);

/* The UTF-8 bytes of the scalar value c, written to out, which holds 4.
   Returns their count. */
size_t anti_rt_utf8_encode(uint32_t c, unsigned char *out);

/* The scalar value of the well-formed sequence that starts in, of n
   bytes. Its length goes to length, which is 0 when the bytes start no
   well-formed sequence. */
uint32_t anti_rt_utf8_decode(const unsigned char *in, size_t n, size_t *length);

#endif

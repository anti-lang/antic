/* bits/alltypes.h of the compiles of the libm of musl, the one header of
   musl that its build generates for each processor. The libm needs four
   of its definitions: float_t and double_t, which are float and double
   on both processors of Anti, and the byte order, little endian on both.
   The integer types come from the stdint.h of clang, which gives each
   its width on every target, the LLP64 one of Windows included.
   src/native/libm.cmake copies this file as bits/alltypes.h. */

#include <stdint.h>

#ifndef ANTI_LIBM_TYPES
#define ANTI_LIBM_TYPES
typedef float float_t;
typedef double double_t;
#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN 4321
#define __BYTE_ORDER __LITTLE_ENDIAN
#endif

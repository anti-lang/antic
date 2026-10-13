/* The values of macros that tests/bind/layout.h does not hold, for
   `anti bind --clang`: casts to the short types, the suffixes of long,
   the escapes of a text, the operators over floats and over unsigned
   integers, and after them the values the evaluator skips. */
#ifndef VALUES_H
#define VALUES_H

#define VALUES_SHORT ((short)70000)
#define VALUES_USHORT ((unsigned short)70000)
#define VALUES_LONG 5L
#define VALUES_ULONG 5UL
#define VALUES_ESCAPES "tab\t line\n return\r slash\\ quote\" tick\'"
#define VALUES_BELL "bell\a"
#define VALUES_FLOAT_OF_UNSIGNED ((float)3000000u)
#define VALUES_FLOAT_OF_SIGNED ((double)(-3))
#define VALUES_FLOAT_SUM (1.5 + 2.25)
#define VALUES_FLOAT_DIFFERENCE (1.5 - 2.25)
#define VALUES_FLOAT_NEGATED (-1.5)
#define VALUES_WHOLE_FLOAT 3e0
#define VALUES_UNSIGNED_PRODUCT (3000000000u * 2u)
#define VALUES_UNSIGNED_QUOTIENT (4000000000u / 3u)
#define VALUES_UNSIGNED_REST (4000000000u % 7u)
#define VALUES_SHIFT_OF_NEGATIVE (-16 >> 2)

#define VALUES_NOT_ZERO (!0)
#define VALUES_FLOAT_TOO_LARGE 1e39f
#define VALUES_FLOAT_REST (1.5 % 2.0)
#define VALUES_FLOAT_COMPLEMENT (~1.5)
#define VALUES_INFINITE (1e308 + 1e308)
#define VALUES_SHIFT_TOO_FAR (1 << 64)
#define VALUES_TEXT_NEGATED (-"text")
#define VALUES_TEXT_IN_SUM ("text" + 1)

#endif

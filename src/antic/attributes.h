#ifndef ANTIC_ATTRIBUTES_H
#define ANTIC_ATTRIBUTES_H

/* DESIGN: rule 1 of docs/c-guidelines.md keeps a compiler extension to a
   file that exists to hold one, and this header is that file for antic.
   ATTRIBUTE_PRINTF(f, a) marks a function that formats like printf. Its
   format is parameter f, and its arguments start at parameter a, which is
   0 for a va_list. clang and gcc then check every call. Other compilers get
   nothing. */
#if defined(__GNUC__) || defined(__clang__)
#define ATTRIBUTE_PRINTF(f, a) __attribute__((format(printf, f, a)))
#else
#define ATTRIBUTE_PRINTF(f, a)
#endif

#endif

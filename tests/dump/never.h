/* never.h, the C interface of com.example.never, written by antic.
   Do not edit. A failure that Anti cannot report calls abort(). */
#ifndef NEVER_H
#define NEVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#define ANTI_ALIGNAS(n) alignas(n)
extern "C" {
#else
#define ANTI_ALIGNAS(n) _Alignas(n)
#endif

#ifdef __cplusplus
#define ANTI_NORETURN [[noreturn]]
#else
#define ANTI_NORETURN _Noreturn
#endif

/** Leave the program with the code. */
ANTI_NORETURN void leave(int32_t code);

#ifdef __cplusplus
}
#endif

#endif

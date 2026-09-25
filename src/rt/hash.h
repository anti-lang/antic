/* The hash of a value, shared by antic and the runtime.

   DESIGN: one definition read from both sides. antic writes the default
   hash of a value inline with these constants, and the runtime hashes
   text and bytes with them, so a value and the text it holds hash the
   same way on every target. A scalar is its 64 bits through the
   finalizer of MurmurHash3. A value of parts starts at the offset basis
   of FNV-1a and takes each part as the mix of the hash so far xor the
   hash of the part. */
#ifndef ANTI_HASH_H
#define ANTI_HASH_H

#include <stdint.h>

/* The two multipliers of the finalizer of MurmurHash3. */
#define ANTI_HASH_MUL1 0xff51afd7ed558ccdu
#define ANTI_HASH_MUL2 0xc4ceb9fe1a85ec53u
/* The hash of a value of parts before its first part: the offset basis
   of FNV-1a. */
#define ANTI_HASH_START 0xcbf29ce484222325u

/* The finalizer, which spreads every bit of x over every bit of the
   result. It is a bijection, so two words never give one hash. */
static inline uint64_t anti_rt_hash_mix(uint64_t x)
{
    x ^= x >> 33;
    x *= ANTI_HASH_MUL1;
    x ^= x >> 33;
    x *= ANTI_HASH_MUL2;
    x ^= x >> 33;
    return x;
}

#endif

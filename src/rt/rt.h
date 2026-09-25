#ifndef ANTI_RT_H
#define ANTI_RT_H

#include <stdint.h>

/* The state of the runtime. An executable initialises it in main. A shared
   library initialises it in a constructor that runs when it loads. A
   static library for C initialises it on the first use of the runtime. */
void anti_rt_init(void);

/* 1 after anti_rt_init, else 0. */
int anti_rt_ready(void);

/* --anti.backtrace of the command line: 1 on, 0 off and -1 when the
   command line did not name it. src/rt/start.c writes it before main. */
extern int anti_rt_option_backtrace;

/* The hash of text and bytes, and the seed of the hashing collections, in
   src/rt/hash.c. start chooses the seed from the random source of the
   system, and set fixes it for the key `hash_seed`. */
uint64_t anti_rt_hash_bytes(const unsigned char *bytes, int64_t count);
uint64_t anti_rt_hash_seed(void);
uint64_t anti_rt_hash_seeded(uint64_t hash, uint64_t with);
void anti_rt_hash_seed_set(uint64_t value);
void anti_rt_hash_seed_start(void);

#endif

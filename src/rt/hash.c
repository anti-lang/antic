/* The hash of text and bytes, and the seed of the hashing collections.

   DESIGN: the seed is chosen from the random source of the system when
   the runtime starts, so the keys an attacker picks cannot all land in
   one bucket of a collection. The key `hash_seed` of the configuration,
   `--anti.hash_seed=<n>` on the command line, fixes it, so a test sees
   the same buckets on every run. A collection reads the seed once, when
   it is made, and mixes it into the hash of every key. A later change
   of the key therefore reaches the collections made after it. */
#include <stdint.h>
#include <string.h>

#include "atomic.h"
#include "hash.h"
#include "platform.h"
#include "rt.h"
#include "std.h"

/* The seed of the program, written at start and by the configuration,
   and read by any thread. */
static int64_t seed = (int64_t)ANTI_HASH_START;

uint64_t anti_rt_hash_bytes(const unsigned char *bytes, int64_t count)
{
    uint64_t h = ANTI_HASH_START;
    uint64_t word;
    int64_t i = 0;

    for (; count - i >= 8; i += 8) {
        memcpy(&word, bytes + i, sizeof word);
        h = anti_rt_hash_mix(h ^ anti_rt_hash_mix(word));
    }
    if (i < count) {
        word = 0;
        memcpy(&word, bytes + i, (size_t)(count - i));
        h = anti_rt_hash_mix(h ^ anti_rt_hash_mix(word));
    }
    return anti_rt_hash_mix(h ^ (uint64_t)count);
}

uint64_t anti_rt_hash_seed(void)
{
    return (uint64_t)anti_rt_atomic_load(&seed, (int64_t)sizeof seed);
}

uint64_t anti_rt_hash_seeded(uint64_t hash, uint64_t with)
{
    return anti_rt_hash_mix(hash ^ anti_rt_hash_mix(with));
}

void anti_rt_hash_seed_set(uint64_t value)
{
    anti_rt_atomic_store(&seed, (int64_t)sizeof seed, (int64_t)value);
}

/* DESIGN: a system that gives no random bytes still starts the program,
   with a seed of the wall clock and the monotonic clock. The buckets are
   then foreseeable to someone who knows the time, which is less than the
   system gives and more than a constant. */
void anti_rt_hash_seed_start(void)
{
    uint64_t value;

    if (anti_rt_entropy(&value, sizeof value) != 0) {
        value = anti_rt_hash_mix((uint64_t)anti_rt_wall()) ^
                (uint64_t)anti_rt_monotonic();
    }
    anti_rt_hash_seed_set(value);
}

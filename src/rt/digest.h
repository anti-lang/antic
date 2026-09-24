/* The SHA-256 of FIPS 180-4, fed in pieces.

   DESIGN: one definition read from both sides. Discovery in the runtime
   digests a library before it opens one. antic digests the code of a
   binary for its build id, and anti checks a downloaded file. antic and
   anti compile src/rt/digest.c into their own programs. It calls nothing
   of the platform, so each side opens its files its own way and hands
   the stream to anti_rt_sha256_stream. */
#ifndef ANTI_RT_DIGEST_H
#define ANTI_RT_DIGEST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

struct anti_sha256 {
    uint32_t hash[8];
    unsigned char block[64];
    size_t used;                    /* the bytes of block that are filled */
    uint64_t length;                /* the bytes fed so far */
};

void anti_rt_sha256_init(struct anti_sha256 *s);
void anti_rt_sha256_update(struct anti_sha256 *s, const void *bytes,
                           size_t count);
/* Write the digest as 64 lowercase hexadecimal digits and a NUL. The
   state is spent afterwards. */
void anti_rt_sha256_hex(struct anti_sha256 *s, char hex[65]);

/* Write the digest of what is left of the stream f into hex. Returns
   false when a read fails. The caller closes f. */
bool anti_rt_sha256_stream(FILE *f, char hex[65]);

#endif

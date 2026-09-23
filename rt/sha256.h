/* The SHA-256 of FIPS 180-4, fed in pieces. Discovery digests a library
   before it opens one. */
#ifndef ANTI_RT_SHA256_H
#define ANTI_RT_SHA256_H

#include <stddef.h>
#include <stdint.h>

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

/* Write the digest of the file at path into hex. Gives 0 when the file
   cannot be read. */
int anti_rt_sha256_file(const char *path, char hex[65]);

#endif

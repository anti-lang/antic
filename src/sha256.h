#ifndef ANTIC_SHA256_H
#define ANTIC_SHA256_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The SHA-256 of FIPS 180-4, fed in pieces. antic digests the code of a
   binary for its build id, and anti checks a downloaded file. */
struct sha256 {
    uint32_t hash[8];
    unsigned char block[64];
    size_t used;                    /* the bytes of block that are filled */
    uint64_t length;                /* the bytes fed so far */
};

void sha256_init(struct sha256 *s);
void sha256_update(struct sha256 *s, const void *bytes, size_t count);
/* Write the digest as 64 lowercase hexadecimal digits and a NUL. The
   state is spent afterwards. */
void sha256_hex(struct sha256 *s, char hex[65]);

/* Write the SHA-256 digest of the file at path into hex as 64 lowercase
   hexadecimal digits and a NUL. Returns false when the file cannot be
   read. */
bool sha256_file(const char *path, char hex[65]);

#endif

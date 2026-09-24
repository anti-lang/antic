/* The SHA-256 of FIPS 180-4, for the runtime, antic and anti. */
#include "digest.h"

#include <string.h>

/* The round constants and the initial hash of FIPS 180-4, section 4.2.2
   and 5.3.3. */
static const uint32_t rounds[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

static const uint32_t initial[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
};

static uint32_t rotate(uint32_t x, unsigned n)
{
    return (x >> n) | (x << (32 - n));
}

/* Fold one block of 64 bytes into the hash. */
static void compress(uint32_t hash[8], const unsigned char block[64])
{
    uint32_t w[64];
    uint32_t v[8];
    int i;

    for (i = 0; i < 16; i++) {
        w[i] = (uint32_t)block[4 * i] << 24 | (uint32_t)block[4 * i + 1] << 16 |
               (uint32_t)block[4 * i + 2] << 8 | (uint32_t)block[4 * i + 3];
    }
    for (i = 16; i < 64; i++) {
        uint32_t s0 = rotate(w[i - 15], 7) ^ rotate(w[i - 15], 18) ^
                      (w[i - 15] >> 3);
        uint32_t s1 = rotate(w[i - 2], 17) ^ rotate(w[i - 2], 19) ^
                      (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    memcpy(v, hash, sizeof v);
    for (i = 0; i < 64; i++) {
        uint32_t s1 = rotate(v[4], 6) ^ rotate(v[4], 11) ^ rotate(v[4], 25);
        uint32_t choose = (v[4] & v[5]) ^ (~v[4] & v[6]);
        uint32_t t1 = v[7] + s1 + choose + rounds[i] + w[i];
        uint32_t s0 = rotate(v[0], 2) ^ rotate(v[0], 13) ^ rotate(v[0], 22);
        uint32_t majority = (v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]);
        uint32_t t2 = s0 + majority;

        v[7] = v[6];
        v[6] = v[5];
        v[5] = v[4];
        v[4] = v[3] + t1;
        v[3] = v[2];
        v[2] = v[1];
        v[1] = v[0];
        v[0] = t1 + t2;
    }
    for (i = 0; i < 8; i++) {
        hash[i] += v[i];
    }
}

void anti_rt_sha256_init(struct anti_sha256 *s)
{
    memcpy(s->hash, initial, sizeof s->hash);
    s->used = 0;
    s->length = 0;
}

void anti_rt_sha256_update(struct anti_sha256 *s, const void *bytes,
                           size_t count)
{
    const unsigned char *from = bytes;

    while (count > 0) {
        size_t n = sizeof s->block - s->used;
        if (n > count) {
            n = count;
        }
        memcpy(s->block + s->used, from, n);
        s->used += n;
        s->length += n;
        from += n;
        count -= n;
        if (s->used == sizeof s->block) {
            compress(s->hash, s->block);
            s->used = 0;
        }
    }
}

void anti_rt_sha256_hex(struct anti_sha256 *s, char hex[65])
{
    uint64_t bits = s->length * 8;
    int i;

    /* The padding: a one bit, zeros, and the length in bits in the last
       eight bytes of a block. */
    s->block[s->used++] = 0x80;
    if (s->used > 56) {
        memset(s->block + s->used, 0, sizeof s->block - s->used);
        compress(s->hash, s->block);
        s->used = 0;
    }
    memset(s->block + s->used, 0, 56 - s->used);
    for (i = 0; i < 8; i++) {
        s->block[56 + i] = (unsigned char)(bits >> (56 - 8 * i));
    }
    compress(s->hash, s->block);
    for (i = 0; i < 64; i++) {
        hex[i] = "0123456789abcdef"[(s->hash[i / 8] >> (28 - 4 * (i % 8))) &
                                    0xf];
    }
    hex[64] = '\0';
}

bool anti_rt_sha256_stream(FILE *f, char hex[65])
{
    struct anti_sha256 s;
    unsigned char bytes[4096];
    size_t n;

    anti_rt_sha256_init(&s);
    while ((n = fread(bytes, 1, sizeof bytes, f)) > 0) {
        anti_rt_sha256_update(&s, bytes, n);
    }
    if (ferror(f)) {
        return false;
    }
    anti_rt_sha256_hex(&s, hex);
    return true;
}

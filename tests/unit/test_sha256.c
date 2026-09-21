#include <string.h>

#include "../binary_stdio.h"
#include "check.h"
#include "sha256.h"

/* The digest of the bytes given in pieces of step, as 64 digits. */
static void digest_of(const char *bytes, size_t length, size_t step,
                      char hex[65])
{
    struct sha256 s;
    size_t at;

    sha256_init(&s);
    for (at = 0; at < length; at += step) {
        size_t n = length - at < step ? length - at : step;
        sha256_update(&s, bytes + at, n);
    }
    sha256_hex(&s, hex);
}

/* The vectors of FIPS 180-4 and its examples, whole and in pieces that
   cross the blocks of 64 bytes. */
void test_sha256(void)
{
    static const char two_blocks[] =
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    char hex[65];
    char million[1000];
    struct sha256 s;
    int i;

    digest_of("", 0, 1, hex);
    CHECK_STR(hex, "e3b0c44298fc1c149afbf4c8996fb924"
                   "27ae41e4649b934ca495991b7852b855");
    digest_of("abc", 3, 3, hex);
    CHECK_STR(hex, "ba7816bf8f01cfea414140de5dae2223"
                   "b00361a396177a9cb410ff61f20015ad");
    digest_of(two_blocks, sizeof two_blocks - 1, sizeof two_blocks, hex);
    CHECK_STR(hex, "248d6a61d20638b8e5c026930c3e6039"
                   "a33ce45964ff2167f6ecedd419db06c1");
    digest_of(two_blocks, sizeof two_blocks - 1, 7, hex);
    CHECK_STR(hex, "248d6a61d20638b8e5c026930c3e6039"
                   "a33ce45964ff2167f6ecedd419db06c1");
    memset(million, 'a', sizeof million);
    sha256_init(&s);
    for (i = 0; i < 1000; i++) {
        sha256_update(&s, million, sizeof million);
    }
    sha256_hex(&s, hex);
    CHECK_STR(hex, "cdc76e5c9914fb9281a1c7e284d73e67"
                   "f1809a48a497200e046d39ccc7112cd0");
}

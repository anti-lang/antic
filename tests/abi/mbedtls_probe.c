/* The C half of the tests mbedtls_run and mbedtls_link_<target>. It sets
   up a TLS client context with the Mbed TLS of the runtime tree, without a
   connection. The functions give plain integers, so the Anti half needs
   no binding of Mbed TLS. */
#include "../binary_stdio.h"
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/version.h>
#include <mbedtls/x509_crt.h>
#include <psa/crypto.h>
#include <stdint.h>
#include <string.h>

/* The entropy of Windows is BCryptGenRandom, which entropy_poll.c calls
   without naming its library. net_sockets.c names ws2_32 itself. */
#if defined(_WIN32)
#pragma comment(lib, "bcrypt.lib")
#endif

int32_t mbedtls_probe_version(void);
int32_t mbedtls_probe_sha256(void);
int32_t mbedtls_probe_tls(void);
int32_t mbedtls_probe_linked(void);

/* 1 when the library reports the version of its header. */
int32_t mbedtls_probe_version(void)
{
    return mbedtls_version_get_number() == MBEDTLS_VERSION_NUMBER;
}

/* 1 when PSA hashes "abc" to the SHA-256 digest of FIPS 180-2. */
int32_t mbedtls_probe_sha256(void)
{
    static const uint8_t expected[32] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40,
        0xde, 0x5d, 0xae, 0x22, 0x23, 0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17,
        0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};
    uint8_t digest[32];
    size_t length = 0;

    if (psa_crypto_init() != PSA_SUCCESS) {
        return -1;
    }
    if (psa_hash_compute(PSA_ALG_SHA_256, (const uint8_t *)"abc", 3, digest,
                         sizeof digest, &length) != PSA_SUCCESS) {
        return -2;
    }
    return length == sizeof digest && memcmp(digest, expected, length) == 0;
}

/* Sets up a TLS client. A random generator is seeded from the entropy of
   the operating system. The configuration is the default of a client over
   a stream, with verification required, and the context carries the name
   of the server. 0 is success, and a failing step gives the Mbed TLS
   error code. */
int32_t mbedtls_probe_tls(void)
{
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context drbg;
    mbedtls_ssl_config config;
    mbedtls_ssl_context ssl;
    mbedtls_x509_crt ca;
    int result;

    if (psa_crypto_init() != PSA_SUCCESS) {
        return -1;
    }
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&drbg);
    mbedtls_ssl_config_init(&config);
    mbedtls_ssl_init(&ssl);
    mbedtls_x509_crt_init(&ca);
    result = mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy,
                                   (const unsigned char *)"anti", 4);
    if (result == 0) {
        result = mbedtls_ssl_config_defaults(&config, MBEDTLS_SSL_IS_CLIENT,
                                             MBEDTLS_SSL_TRANSPORT_STREAM,
                                             MBEDTLS_SSL_PRESET_DEFAULT);
    }
    if (result == 0) {
        mbedtls_ssl_conf_authmode(&config, MBEDTLS_SSL_VERIFY_REQUIRED);
        mbedtls_ssl_conf_ca_chain(&config, &ca, NULL);
        mbedtls_ssl_conf_rng(&config, mbedtls_ctr_drbg_random, &drbg);
        result = mbedtls_ssl_setup(&ssl, &config);
    }
    if (result == 0) {
        result = mbedtls_ssl_set_hostname(&ssl, "anti-lang.com");
    }
    mbedtls_x509_crt_free(&ca);
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&config);
    mbedtls_ctr_drbg_free(&drbg);
    mbedtls_entropy_free(&entropy);
    return result;
}

/* 1 when the functions of a connection resolve at link: the socket of
   net_sockets.c, the handshake and the reading of a CA bundle. */
int32_t mbedtls_probe_linked(void)
{
    int (*volatile connect)(mbedtls_net_context *, const char *, const char *,
                            int) = mbedtls_net_connect;
    int (*volatile handshake)(mbedtls_ssl_context *) = mbedtls_ssl_handshake;
    int (*volatile bundle)(mbedtls_x509_crt *, const char *) =
        mbedtls_x509_crt_parse_file;

    return connect != NULL && handshake != NULL && bundle != NULL;
}

/* The SHA-256 of FIPS 180-4. Discovery digests a library before it
   opens one. */
#ifndef ANTI_RT_DIGEST_H
#define ANTI_RT_DIGEST_H

/* Write the digest of the file at path into hex. Gives 0 when the file
   cannot be read. */
int anti_rt_sha256_file(const char *path, char hex[65]);

#endif

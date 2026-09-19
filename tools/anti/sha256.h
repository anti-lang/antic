#ifndef ANTI_SHA256_H
#define ANTI_SHA256_H

#include <stdbool.h>

/* Write the SHA-256 digest of the file at path into hex as 64 lowercase
   hexadecimal digits and a NUL. Returns false when the file cannot be
   read. */
bool sha256_file(const char *path, char hex[65]);

#endif

/* The SHA-256 digest of a file, by the hash of the runtime. */

#include "sha256.h"

#include <stdio.h>

#include "platform.h"

/* The hash is the one of the runtime, which antic and anti compile in. */
#include "../rt/digest.h"
#include "../rt/fault.h"

bool sha256_file(const char *path, char hex[65])
{
    FILE *f = platform_open(path, false);
    bool ok;

    if (f == NULL) {
        return false;
    }
    ok = anti_rt_sha256_stream(f, hex);
    ANTI_FCLOSE(f);
    return ok;
}

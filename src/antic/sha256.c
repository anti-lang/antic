#include "sha256.h"

#include <stdio.h>

#include "platform.h"

bool sha256_file(const char *path, char hex[65])
{
    FILE *f = platform_open(path, false);
    bool ok;

    if (f == NULL) {
        return false;
    }
    ok = anti_rt_sha256_stream(f, hex);
    fclose(f);
    return ok;
}

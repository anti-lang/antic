#include "sha256.h"

#include <stdio.h>

bool sha256_file(const char *path, char hex[65])
{
    FILE *f = fopen(path, "rb");
    bool ok;

    if (f == NULL) {
        return false;
    }
    ok = anti_rt_sha256_stream(f, hex);
    fclose(f);
    return ok;
}

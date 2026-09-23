#include "applesdk.h"

#include <stdio.h>
#include <string.h>
#if defined(__APPLE__)
#include <dirent.h>
#endif

bool apple_clt_sdk(struct text *path, struct text *version)
{
#if defined(__APPLE__)
    DIR *dir = opendir(APPLE_CLT_SDKS);
    struct dirent *entry;
    int best_major = -1;
    int best_minor = -1;

    if (dir == NULL) {
        return false;
    }
    while ((entry = readdir(dir)) != NULL) {
        int major;
        int minor;
        char rest[8];

        if (sscanf(entry->d_name, "MacOSX%d.%d%7s", &major, &minor, rest) != 3 ||
            strcmp(rest, ".sdk") != 0 || major > APPLE_SDK_NEWEST_MAJOR ||
            major < best_major || (major == best_major && minor <= best_minor)) {
            continue;
        }
        best_major = major;
        best_minor = minor;
    }
    closedir(dir);
    if (best_major < 0) {
        return false;
    }
    text_appendf(path, "%s/MacOSX%d.%d.sdk", APPLE_CLT_SDKS, best_major,
                 best_minor);
    text_appendf(version, "%d.%d", best_major, best_minor);
    return true;
#else
    (void)path;
    (void)version;
    return false;
#endif
}

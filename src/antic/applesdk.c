#include "applesdk.h"

#include <limits.h>
#include <string.h>
#if defined(__APPLE__)
#include <dirent.h>
#endif

/* Read the decimal digits at *s into *value and move *s past them. At
   least one digit, and a value that fits an int. */
static bool version_number(const char **s, int *value)
{
    int n = 0;

    if (**s < '0' || **s > '9') {
        return false;
    }
    while (**s >= '0' && **s <= '9') {
        int digit = **s - '0';
        if (n > (INT_MAX - digit) / 10) {
            return false;
        }
        n = n * 10 + digit;
        (*s)++;
    }
    *value = n;
    return true;
}

/* DESIGN: the numbers are read digit by digit. The %d of sscanf is
   undefined on a value that does not fit an int, and it takes a sign
   and leading spaces as well. */
bool apple_sdk_version(const char *name, int *major, int *minor)
{
    static const char prefix[] = "MacOSX";
    const char *s = name;

    if (strncmp(s, prefix, sizeof prefix - 1) != 0) {
        return false;
    }
    s += sizeof prefix - 1;
    if (!version_number(&s, major) || *s != '.') {
        return false;
    }
    s++;
    return version_number(&s, minor) && strcmp(s, ".sdk") == 0;
}

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

        if (!apple_sdk_version(entry->d_name, &major, &minor) ||
            major > APPLE_SDK_NEWEST_MAJOR ||
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

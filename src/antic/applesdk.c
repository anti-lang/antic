#include "applesdk.h"

#include <limits.h>
#include <string.h>

#include "platform.h"
#include "target.h"

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

/* The newest version of an SDK name that ld64.lld reads, so far. */
struct newest {
    int major;
    int minor;
};

static void consider(void *context, const char *name)
{
    struct newest *best = context;
    int major;
    int minor;

    if (!apple_sdk_version(name, &major, &minor) ||
        major > APPLE_SDK_NEWEST_MAJOR || major < best->major ||
        (major == best->major && minor <= best->minor)) {
        return;
    }
    best->major = major;
    best->minor = minor;
}

bool apple_clt_sdk(struct text *path, struct text *version)
{
    struct newest best = {-1, -1};
    enum target host;

    if (!target_host(&host) || target_info(host)->os != OS_MACOS ||
        !platform_list_directory(APPLE_CLT_SDKS, consider, &best) ||
        best.major < 0) {
        return false;
    }
    text_appendf(path, "%s/MacOSX%d.%d.sdk", APPLE_CLT_SDKS, best.major,
                 best.minor);
    text_appendf(version, "%d.%d", best.major, best.minor);
    return true;
}

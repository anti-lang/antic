#include <string.h>
#include "std.h"

/* The notice that antic links into every executable and shared library. */
extern const char anti_licenses[];

static const char begin[] = "ANTI_LICENSES_BEGIN\n";
static const char end[] = "ANTI_LICENSES_END\n";
static const char build[] = "build ";

/* DESIGN: the licence text is the notice between the markers without the
   build id, which is a fact of the binary and no licence. */
struct anti_text anti_rt_license_text(void)
{
    struct anti_text text;
    const char *from = anti_licenses;
    const char *to;

    if (strncmp(from, begin, sizeof begin - 1) == 0) {
        from += sizeof begin - 1;
    }
    if (strncmp(from, build, sizeof build - 1) == 0 &&
        strchr(from, '\n') != NULL) {
        from = strchr(from, '\n') + 1;
    }
    to = strstr(from, end);
    text.ptr = (const unsigned char *)from;
    text.len = to != NULL ? (int64_t)(to - from) : (int64_t)strlen(from);
    return text;
}

/* DESIGN: the version is the one antic wrote into the notice for the
   package anti.rt. The runtime needs no version of its own from the
   build. --anti.inspect prints it. */
struct anti_text anti_rt_runtime_version(void)
{
    static const char package[] = "package anti.rt ";
    const char *at = strstr(anti_licenses, package);
    const char *from = at != NULL ? at + sizeof package - 1 : NULL;
    const char *to = from != NULL ? strchr(from, ' ') : NULL;
    struct anti_text text;

    text.ptr = (const unsigned char *)(from != NULL ? from : "");
    text.len = to != NULL ? (int64_t)(to - from) : 0;
    return text;
}

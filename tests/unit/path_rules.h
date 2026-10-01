/* The path rules of the host, which the platform layer of the runtime,
   src/rt/platform.h, and the one of the tools, src/antic/platform.h, both
   follow. The test of each layer runs its own two functions over these
   cases, so the two layers cannot answer a path differently. */
#ifndef ANTIC_TEST_PATH_RULES_H
#define ANTIC_TEST_PATH_RULES_H

/* A path, whether it is absolute, and the offset of its last separator,
   or -1 when it has none. */
struct path_rule {
    const char *path;
    int absolute;
    int separator;
};

static const struct path_rule path_rules[] = {
    {"/etc/anti.toml", 1, 4},
    {"anti.toml", 0, -1},
    {"a/anti.toml", 0, 1},
    {"a/b/c.toml", 0, 3},
#if defined(_WIN32)
    /* A drive, a root of the drive and a share are absolute, and a
       backslash separates as a slash does, whichever comes last. A drive
       without a separator names a file on that drive, which no directory
       can be put before. */
    {"C:\\anti\\anti.toml", 1, 7},
    {"c:/anti.toml", 1, 2},
    {"C:anti.toml", 1, -1},
    {"\\anti.toml", 1, 0},
    {"\\\\server\\share\\anti.toml", 1, 14},
    {"anti\\anti.toml", 0, 4},
    {"1:anti.toml", 0, -1},
    {"a\\b/c\\d.toml", 0, 5},
    {"a\\b/c.toml", 0, 3},
#else
    /* A backslash is a byte of a name, and a drive is a name. */
    {"a\\b.toml", 0, -1},
    {"C:\\anti.toml", 0, -1},
    {"C:/anti.toml", 0, 2},
#endif
};

#define PATH_RULE_COUNT (sizeof path_rules / sizeof path_rules[0])

#endif

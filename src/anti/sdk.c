/* lstat and stat are POSIX, outside the C11 library. */
#define _POSIX_C_SOURCE 200809L

#include "sdk.h"

#include <stdio.h>
#include <string.h>

#include "applesdk.h"
#include "files.h"
#include "linker.h"
#include "process.h"
#include "sha256.h"
#include "text.h"

#if defined(__APPLE__)
#include <dirent.h>
#include <sys/stat.h>
#endif

/* The macOS sysroots that an import fills. */
static const char *const macos_targets[] = {"macos-arm64", "macos-x86_64"};

/* Write line and a line feed as the whole file at path. */
static bool write_line(const char *path, const char *line)
{
    struct text bytes = {0};
    bool ok;

    text_appendf(&bytes, "%s\n", line);
    ok = write_file(path, &bytes);
    text_free(&bytes);
    return ok;
}

#if defined(__APPLE__)
/* Append the value of "Version" in the SDKSettings.json of sdk to out. */
static bool sdk_version(const char *sdk, struct text *out)
{
    struct text path = {0};
    struct text json = {0};
    const char *key;
    const char *start;
    const char *end;
    bool ok = false;

    text_appendf(&path, "%s/SDKSettings.json", sdk);
    if (read_file(text_cstr(&path), &json)) {
        key = strstr(text_cstr(&json), "\"Version\"");
        start = key != NULL ? strchr(key + strlen("\"Version\""), '"') : NULL;
        end = start != NULL ? strchr(start + 1, '"') : NULL;
        if (end != NULL) {
            text_append_bytes(out, start + 1, (size_t)(end - start - 1));
            ok = out->length > 0;
        }
    }
    text_free(&path);
    text_free(&json);
    return ok;
}

/* Whether version is digits in parts joined by single dots, as 15.2 is.
   S45: the version names the directory that export empties and removes,
   and the bundle it writes. SDKSettings.json lies under a path the user
   names. */
static bool sdk_version_valid(const char *version)
{
    const char *p = version;

    for (;;) {
        if (*p < '0' || *p > '9') {
            return false;
        }
        while (*p >= '0' && *p <= '9') {
            p++;
        }
        if (*p == '\0') {
            return true;
        }
        if (*p != '.') {
            return false;
        }
        p++;
    }
}

static bool is_stub(const char *name)
{
    size_t n = strlen(name);
    return n > 4 && strcmp(name + n - 4, ".tbd") == 0;
}

/* Copy the stubs below dir, whose path in the SDK is rel, to stage/rel.
   A linked stub becomes a copy, as the top-level stub of a framework is.
   A linked directory stays out, since lld reads no path through
   Versions/Current. */
static bool copy_stubs(const char *dir, const char *rel, const char *stage)
{
    DIR *d = opendir(dir);
    struct dirent *entry;
    bool ok = true;

    if (d == NULL) {
        return false;
    }
    while (ok && (entry = readdir(d)) != NULL) {
        struct text from = {0};
        struct text inside = {0};
        struct stat link_st;
        struct stat st;

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        text_appendf(&from, "%s/%s", dir, entry->d_name);
        text_appendf(&inside, "%s/%s", rel, entry->d_name);
        if (lstat(text_cstr(&from), &link_st) == 0) {
            if (S_ISDIR(link_st.st_mode)) {
                ok = copy_stubs(text_cstr(&from), text_cstr(&inside), stage);
            } else if (is_stub(entry->d_name) && stat(text_cstr(&from), &st) == 0 &&
                       S_ISREG(st.st_mode)) {
                struct text to = {0};
                struct text parent = {0};
                text_appendf(&to, "%s/%s", stage, text_cstr(&inside));
                text_appendf(&parent, "%s/%s", stage, rel);
                ok = make_dirs(text_cstr(&parent)) &&
                     copy_file(text_cstr(&from), text_cstr(&to));
                text_free(&to);
                text_free(&parent);
            }
        }
        text_free(&from);
        text_free(&inside);
    }
    closedir(d);
    return ok;
}
#endif

int sdk_export(const char *sdk, const char *out)
{
#if !defined(__APPLE__)
    (void)sdk;
    (void)out;
    fputs("anti: sdk export runs on a Mac, which holds Apple's SDK. Run it on "
          "a Mac you own and anti sdk import here.\n", stderr);
    return 1;
#else
    struct text path = {0};
    struct text version = {0};
    struct text stage = {0};
    struct text bundle = {0};
    struct text file = {0};
    int status = 1;
    static const char *const dirs[] = {"usr/lib", "System/Library/Frameworks"};
    size_t i;

    if (sdk != NULL) {
        text_append(&path, sdk);
        if (!sdk_version(sdk, &version)) {
            fprintf(stderr, "anti: %s holds no SDKSettings.json with a Version, "
                            "so it is no MacOSX.sdk\n", sdk);
            goto done;
        }
    } else if (!apple_clt_sdk(&path, &version)) {
        fputs("anti: " APPLE_CLT_SDKS " holds no SDK. Run xcode-select "
              "--install, or name one with --sdk\n", stderr);
        goto done;
    }
    if (!sdk_version_valid(text_cstr(&version))) {
        fprintf(stderr, "anti: %s names the Version %s, which is no version\n",
                text_cstr(&path), text_cstr(&version));
        goto done;
    }
    text_appendf(&stage, "%s/.apple-sdk-%s", out, text_cstr(&version));
    text_appendf(&bundle, "%s/apple-sdk-%s.tar.xz", out, text_cstr(&version));
    remove_tree(text_cstr(&stage));
    if (!make_dirs(text_cstr(&stage))) {
        fprintf(stderr, "anti: cannot make %s\n", text_cstr(&stage));
        goto done;
    }
    for (i = 0; i < sizeof dirs / sizeof dirs[0]; i++) {
        struct text dir = {0};
        bool ok;
        text_appendf(&dir, "%s/%s", text_cstr(&path), dirs[i]);
        ok = copy_stubs(text_cstr(&dir), dirs[i], text_cstr(&stage));
        text_free(&dir);
        if (!ok) {
            fprintf(stderr, "anti: cannot copy the stubs of %s/%s\n",
                    text_cstr(&path), dirs[i]);
            goto done;
        }
    }
    text_appendf(&file, "%s/%s", text_cstr(&stage), SYSROOT_SDK_VERSION);
    if (!write_line(text_cstr(&file), text_cstr(&version))) {
        goto done;
    }
    remove_tree(text_cstr(&bundle));
    {
        const char *argv[] = {"tar", "-cJf", text_cstr(&bundle), "-C",
                              text_cstr(&stage), ".", NULL};
        if (process_run(argv) != 0) {
            fprintf(stderr, "anti: tar could not write %s\n", text_cstr(&bundle));
            goto done;
        }
    }
    printf("anti: %s holds the stubs of the SDK %s. Apple's licence governs "
           "where it may be used.\n", text_cstr(&bundle), text_cstr(&version));
    status = 0;
done:
    if (stage.length > 0) {
        remove_tree(text_cstr(&stage));
    }
    text_free(&path);
    text_free(&version);
    text_free(&stage);
    text_free(&bundle);
    text_free(&file);
    return status;
#endif
}

/* Unpack the bundle into <dir>/sdk through <dir>/sdk.part, so that a file
   that is no bundle leaves the SDK before it in place. */
static bool unpack(const char *bundle, const char *dir, const char *digest)
{
    struct text part = {0};
    struct text final = {0};
    struct text marker = {0};
    bool ok = false;

    text_appendf(&part, "%s/%s.part", dir, SYSROOT_APPLE_SDK_DIR);
    text_appendf(&final, "%s/%s", dir, SYSROOT_APPLE_SDK_DIR);
    text_appendf(&marker, "%s/%s", text_cstr(&part), SYSROOT_SDK_VERSION);
    remove_tree(text_cstr(&part));
    if (!make_dirs(text_cstr(&part))) {
        fprintf(stderr, "anti: cannot make %s\n", text_cstr(&part));
        goto done;
    }
    {
        const char *argv[] = {"tar", "-xJf", bundle, "-C", text_cstr(&part), NULL};
        if (process_run(argv) != 0) {
            fprintf(stderr, "anti: tar could not unpack %s\n", bundle);
            goto done;
        }
    }
    if (!path_exists(text_cstr(&marker))) {
        fprintf(stderr, "anti: %s holds no %s, so anti sdk export did not "
                        "write it\n", bundle, SYSROOT_SDK_VERSION);
        goto done;
    }
    if (!remove_tree(text_cstr(&final)) ||
        rename(text_cstr(&part), text_cstr(&final)) != 0) {
        fprintf(stderr, "anti: cannot replace %s\n", text_cstr(&final));
        goto done;
    }
    marker.length = 0;
    text_appendf(&marker, "%s/digest", text_cstr(&final));
    ok = write_line(text_cstr(&marker), digest);
done:
    remove_tree(text_cstr(&part));
    text_free(&part);
    text_free(&final);
    text_free(&marker);
    return ok;
}

int sdk_import(const char *bundle, const char *sysroot)
{
    char digest[65];
    struct text version = {0};
    struct text path = {0};
    size_t i;

    if (!sha256_file(bundle, digest)) {
        fprintf(stderr, "anti: cannot read %s\n", bundle);
        return 1;
    }
    for (i = 0; i < sizeof macos_targets / sizeof macos_targets[0]; i++) {
        struct text dir = {0};
        bool ok;
        text_appendf(&dir, "%s/%s", sysroot, macos_targets[i]);
        ok = make_dirs(text_cstr(&dir)) && unpack(bundle, text_cstr(&dir), digest);
        text_free(&dir);
        if (!ok) {
            return 1;
        }
    }
    text_appendf(&path, "%s/%s/%s/%s", sysroot, macos_targets[0],
                 SYSROOT_APPLE_SDK_DIR, SYSROOT_SDK_VERSION);
    read_file(text_cstr(&path), &version);
    while (version.length > 0 && (version.data[version.length - 1] == '\n' ||
                                   version.data[version.length - 1] == '\r')) {
        version.data[--version.length] = '\0';
    }
    printf("anti: the stubs of the SDK %s in %s, digest %s\n", text_cstr(&version),
           sysroot, digest);
    text_free(&version);
    text_free(&path);
    return 0;
}

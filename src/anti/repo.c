/* The repositories a build fetches from and the cache it fetches into.

   DESIGN: a repository is a URL prefix over static files, so the
   transport is a file copy or one HTTP request per file. antic links no
   TLS library and `anti.net` over Mbed TLS is not built, so an
   `https://` fetch runs curl. macOS, every Linux and Windows 10 and
   later carry it. The cache lives in the user's cache directory of the
   platform, the one `anti.os` gives a program, so nothing outside the
   user's profile is written. */
#include "repo.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "antl.h"
#include "deps.h"
#include "files.h"
#include "process.h"
#include "sha256.h"
#include "text.h"
#include "userdirs.h"

/* A cached index file is checked against its repository at most once in
   this many seconds, which docs/tooling.md sets to an hour. */
enum { REPO_INDEX_SECONDS = 3600 };

/* The stamp of the last check and the entity tag of a cached index stand
   beside it. */
#define REPO_STAMP_SUFFIX ".checked"
#define REPO_ETAG_SUFFIX ".etag"

bool repo_name_valid(const char *name)
{
    const char *p = name;

    for (;;) {
        if (!((*p >= 'a' && *p <= 'z') || *p == '_')) {
            return false;
        }
        while ((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') ||
               *p == '_') {
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

bool repo_digest_valid(const char *digest)
{
    size_t i;

    for (i = 0; i < 64; i++) {
        if (!((digest[i] >= '0' && digest[i] <= '9') ||
              (digest[i] >= 'a' && digest[i] <= 'f'))) {
            return false;
        }
    }
    return digest[64] == '\0';
}

/* The cache of a user, which is per user and not per project. It holds
   the index files under index/<digest of the prefix>/<name>/ and the
   library files under pkg/<name>/<version>/. */
static bool repo_cache_dir(struct text *out)
{
    if (!user_dir(out, USER_DIR_CACHE, USER_DIR_APP)) {
        fputs("anti: the environment does not say where the cache of the user "
              "is\n", stderr);
        return false;
    }
    return true;
}

/* Whether the authority of a URL, the text after `http://` up to the
   first `/`, `?` or `#`, is the loopback of the machine, the one host a
   plain `http://` URL may name. A port of digits alone may follow it. An
   authority with an `@` is refused whatever it holds, since curl reads
   the text before one as a user and fetches from the host after it. */
static bool is_loopback(const char *authority)
{
    static const char *const names[] = {"127.0.0.1", "localhost", "[::1]"};
    size_t length = strcspn(authority, "/?#");
    size_t i;

    if (memchr(authority, '@', length) != NULL) {
        return false;
    }
    for (i = 0; i < sizeof names / sizeof *names; i++) {
        size_t n = strlen(names[i]);
        size_t at;
        if (length < n || memcmp(authority, names[i], n) != 0) {
            continue;
        }
        if (length == n) {
            return true;
        }
        if (authority[n] != ':') {
            continue;
        }
        for (at = n + 1; at < length; at++) {
            if (authority[at] < '0' || authority[at] > '9') {
                break;
            }
        }
        if (at == length) {
            return true;
        }
    }
    return false;
}

bool repo_url_allowed(const char *url)
{
    static const char http[] = "http://";

    if (strncmp(url, "https://", 8) == 0 || strncmp(url, "file://", 7) == 0) {
        return true;
    }
    if (strncmp(url, http, sizeof http - 1) == 0) {
        if (is_loopback(url + sizeof http - 1)) {
            return true;
        }
        fprintf(stderr, "anti: %s: http:// names a repository of 127.0.0.1 or "
                        "localhost alone\n", url);
        return false;
    }
    fprintf(stderr, "anti: %s: a repository URL is https:// or file://\n", url);
    return false;
}

/* The path a `file://` URL names. The two forms `file:///tmp/repo` and
   `file://localhost/tmp/repo` both give `/tmp/repo`. */
static bool file_url_path(const char *url, struct text *out)
{
    const char *rest = url + 7;

    if (strncmp(rest, "localhost/", 10) == 0) {
        rest += 9;
    }
    if (*rest != '/') {
        fprintf(stderr, "anti: %s: a file:// URL names an absolute path\n",
                url);
        return false;
    }
    /* A Windows path stands as file:///C:/dir, whose leading slash is no
       part of it. */
    if (rest[1] != '\0' && rest[2] == ':') {
        rest++;
    }
    text_append(out, rest);
    return true;
}

/* Fetch url into the file at destination. etag names the file that holds
   the entity tag of an earlier fetch, or NULL for a file that is fetched
   once. Returns false when the fetch failed, and leaves an unchanged
   destination where the server answered that nothing changed. */
static bool fetch(const char *url, const char *destination, const char *etag)
{
    struct text temporary = {0};
    struct text source = {0};
    struct text compare = {0};
    struct text save = {0};
    const char *argv[10];
    size_t argc = 0;
    bool ok = false;

    if (!repo_url_allowed(url)) {
        return false;
    }
    text_appendf(&temporary, "%s.new", destination);
    if (strncmp(url, "file://", 7) == 0) {
        if (file_url_path(url, &source)) {
            ok = files_copy(text_cstr(&source), text_cstr(&temporary));
            if (!ok) {
                fprintf(stderr, "anti: %s holds no %s\n", url,
                        text_cstr(&source));
            }
        }
    } else {
        /* DESIGN: curl is the one HTTP client every host of Anti has. The
           entity tag of a cached index goes out with the request, so a
           warm cache costs one round trip and no transfer. */
        argv[argc++] = "curl";
        argv[argc++] = "-fsSL";
        if (etag != NULL) {
            if (files_exists(etag)) {
                text_appendf(&compare, "%s", etag);
                argv[argc++] = "--etag-compare";
                argv[argc++] = text_cstr(&compare);
            }
            text_appendf(&save, "%s", etag);
            argv[argc++] = "--etag-save";
            argv[argc++] = text_cstr(&save);
        }
        argv[argc++] = "-o";
        argv[argc++] = text_cstr(&temporary);
        argv[argc++] = url;
        argv[argc] = NULL;
        ok = process_run(argv) == 0;
        if (!ok) {
            fprintf(stderr, "anti: %s: curl fetched nothing\n", url);
        }
    }
    if (ok) {
        struct text bytes = {0};
        /* An answer of no change writes no byte, and the cached file
           stays as it was. */
        if (files_read(text_cstr(&temporary), &bytes) && bytes.length == 0 &&
            files_exists(destination)) {
            ok = true;
        } else {
            ok = files_copy(text_cstr(&temporary), destination);
        }
        text_free(&bytes);
    }
    remove(text_cstr(&temporary));
    text_free(&temporary);
    text_free(&source);
    text_free(&compare);
    text_free(&save);
    return ok;
}

/* Whether the stamp beside a cached index says it was checked within the
   hour. */
static bool checked_recently(const char *stamp)
{
    struct text bytes = {0};
    bool fresh = false;

    if (files_read(stamp, &bytes) && bytes.length > 0) {
        long long then;
        long long now = (long long)time(NULL);
        errno = 0;
        then = strtoll(text_cstr(&bytes), NULL, 10);
        /* A stamp out of range is no stamp, and the index is checked. */
        fresh = errno != ERANGE && now >= then &&
                now - then < REPO_INDEX_SECONDS;
    }
    text_free(&bytes);
    return fresh;
}

static void write_stamp(const char *stamp)
{
    struct text bytes = {0};

    text_appendf(&bytes, "%lld\n", (long long)time(NULL));
    files_write(stamp, &bytes);
    text_free(&bytes);
}

/* The directory of the cached index files of one prefix. The digest of
   the prefix names it, so two repositories of one package name stay
   apart. */
static bool index_dir(const char *prefix, const char *name, struct text *out)
{
    struct sha256 digest;
    char hex[65];

    if (!repo_name_valid(name)) {
        fprintf(stderr, "anti: %s names the package %s, which is no package "
                        "name\n", prefix, name);
        return false;
    }
    if (!repo_cache_dir(out)) {
        return false;
    }
    sha256_init(&digest);
    sha256_update(&digest, prefix, strlen(prefix));
    sha256_hex(&digest, hex);
    text_appendf(out, "/index/%s/%s", hex, name);
    return files_make_dirs(text_cstr(out));
}

bool repo_index(const char *prefix, const char *name, bool offline,
                struct text *out)
{
    struct text directory = {0};
    struct text stamp = {0};
    struct text etag = {0};
    struct text url = {0};
    bool ok = false;

    if (!index_dir(prefix, name, &directory)) {
        goto done;
    }
    text_appendf(out, "%s/%s", text_cstr(&directory), REPO_INDEX_FILE);
    text_appendf(&stamp, "%s%s", text_cstr(out), REPO_STAMP_SUFFIX);
    text_appendf(&etag, "%s%s", text_cstr(out), REPO_ETAG_SUFFIX);
    if (files_exists(text_cstr(out)) &&
        (offline || checked_recently(text_cstr(&stamp)))) {
        ok = true;
        goto done;
    }
    if (offline) {
        fprintf(stderr, "anti: --offline and no cached index of %s from %s\n",
                name, prefix);
        goto done;
    }
    text_appendf(&url, "%s/%s/%s", prefix, name, REPO_INDEX_FILE);
    ok = fetch(text_cstr(&url), text_cstr(out), text_cstr(&etag));
    if (ok) {
        write_stamp(text_cstr(&stamp));
    }
done:
    text_free(&directory);
    text_free(&stamp);
    text_free(&etag);
    text_free(&url);
    return ok;
}

bool repo_module(const char *prefix, const char *name, const char *version,
                 const char *module, const char *digest, bool offline,
                 struct text *out)
{
    struct text directory = {0};
    struct text url = {0};
    char hex[65];
    bool ok = false;

    /* S45: each of the four stands in a path of the cache or in the
       check after the fetch. Each may come from a downloaded index or a
       cloned lock file. */
    if (!repo_name_valid(name)) {
        fprintf(stderr, "anti: %s names the package %s, which is no package "
                        "name\n", prefix, name);
        goto done;
    }
    if (!deps_version_valid(version)) {
        fprintf(stderr, "anti: %s names %s %s, which is no version\n", prefix,
                name, version);
        goto done;
    }
    if (!repo_name_valid(module)) {
        fprintf(stderr, "anti: %s names the module %s of %s, which is no "
                        "module path\n", prefix, module, name);
        goto done;
    }
    if (!repo_digest_valid(digest)) {
        fprintf(stderr, "anti: %s names the digest %s of %s, which is no "
                        "SHA-256 digest\n", prefix, digest, module);
        goto done;
    }
    if (!repo_cache_dir(&directory)) {
        goto done;
    }
    text_appendf(&directory, "/pkg/%s/%s", name, version);
    if (!files_make_dirs(text_cstr(&directory))) {
        goto done;
    }
    text_appendf(out, "%s/%s%s", text_cstr(&directory), module, ANTL_SUFFIX);
    /* A package file is immutable once cached, so a file that is there
       and carries its digest is fetched no second time. */
    if (!files_exists(text_cstr(out))) {
        if (offline) {
            fprintf(stderr, "anti: --offline and no cached %s of %s %s\n",
                    module, name, version);
            goto done;
        }
        text_appendf(&url, "%s/%s/%s/%s%s", prefix, name, version, module,
                     ANTL_SUFFIX);
        if (!fetch(text_cstr(&url), text_cstr(out), NULL)) {
            goto done;
        }
    }
    /* The digest of the index is checked at download time and again
       before every build. */
    if (!sha256_file(text_cstr(out), hex)) {
        fprintf(stderr, "anti: cannot read %s\n", text_cstr(out));
        goto done;
    }
    if (strcmp(hex, digest) != 0) {
        fprintf(stderr, "anti: %s of %s %s has the digest %s and the index "
                        "names %s\n", module, name, version, hex, digest);
        /* A cached file is fetched no second time, so one that fails its
           digest leaves the cache and the next build fetches it again. */
        remove(text_cstr(out));
        goto done;
    }
    ok = true;
done:
    text_free(&directory);
    text_free(&url);
    return ok;
}

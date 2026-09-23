/* The repositories a build fetches from and the cache it fetches into.

   DESIGN: a repository is a URL prefix over static files, so the
   transport is a file copy or one HTTP request per file. antic links no
   TLS library and `anti.net` over Mbed TLS is not built, so an
   `https://` fetch runs curl. macOS, every Linux and Windows 10 and
   later carry it. The cache lives in the user's cache directory of the
   platform, the one `anti.os` gives a program, so nothing outside the
   user's profile is written. */
#include "repo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "antl.h"
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

static bool read_file(const char *path, struct text *out)
{
    FILE *f = fopen(path, "rb");
    char buffer[8192];
    size_t n;

    if (f == NULL) {
        return false;
    }
    while ((n = fread(buffer, 1, sizeof buffer, f)) > 0) {
        text_append_bytes(out, buffer, n);
    }
    fclose(f);
    return true;
}

static bool write_file(const char *path, const char *bytes, size_t length)
{
    FILE *f = fopen(path, "wb");

    if (f == NULL) {
        fprintf(stderr, "anti: cannot write %s\n", path);
        return false;
    }
    if (length > 0 && fwrite(bytes, 1, length, f) != length) {
        fclose(f);
        fprintf(stderr, "anti: cannot write %s\n", path);
        return false;
    }
    fclose(f);
    return true;
}

bool repo_cache_dir(struct text *out)
{
    if (!user_dir(out, USER_DIR_CACHE, USER_DIR_APP)) {
        fputs("anti: the environment does not say where the cache of the user "
              "is\n", stderr);
        return false;
    }
    return true;
}

/* Whether host is the loopback of the machine, the one host that a plain
   `http://` URL may name. */
static bool is_loopback(const char *host, size_t length)
{
    static const char *const names[] = {"127.0.0.1", "localhost", "[::1]"};
    size_t i;

    for (i = 0; i < sizeof names / sizeof *names; i++) {
        size_t n = strlen(names[i]);
        if (length >= n && memcmp(host, names[i], n) == 0 &&
            (length == n || host[n] == ':' || host[n] == '/')) {
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
        const char *host = url + sizeof http - 1;
        if (is_loopback(host, strlen(host))) {
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
            ok = copy_file(text_cstr(&source), text_cstr(&temporary));
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
            if (path_exists(etag)) {
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
        if (read_file(text_cstr(&temporary), &bytes) && bytes.length == 0 &&
            path_exists(destination)) {
            ok = true;
        } else {
            ok = copy_file(text_cstr(&temporary), destination);
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

    if (read_file(stamp, &bytes) && bytes.length > 0) {
        long long then = strtoll(text_cstr(&bytes), NULL, 10);
        long long now = (long long)time(NULL);
        fresh = now >= then && now - then < REPO_INDEX_SECONDS;
    }
    text_free(&bytes);
    return fresh;
}

static void write_stamp(const char *stamp)
{
    struct text bytes = {0};

    text_appendf(&bytes, "%lld\n", (long long)time(NULL));
    write_file(stamp, bytes.data, bytes.length);
    text_free(&bytes);
}

/* The directory of the cached index files of one prefix. The digest of
   the prefix names it, so two repositories of one package name stay
   apart. */
static bool index_dir(const char *prefix, const char *name, struct text *out)
{
    struct sha256 digest;
    char hex[65];

    if (!repo_cache_dir(out)) {
        return false;
    }
    sha256_init(&digest);
    sha256_update(&digest, prefix, strlen(prefix));
    sha256_hex(&digest, hex);
    text_appendf(out, "/index/%s/%s", hex, name);
    return make_dirs(text_cstr(out));
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
    if (path_exists(text_cstr(out)) &&
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

    if (!repo_cache_dir(&directory)) {
        goto done;
    }
    text_appendf(&directory, "/pkg/%s/%s", name, version);
    if (!make_dirs(text_cstr(&directory))) {
        goto done;
    }
    text_appendf(out, "%s/%s%s", text_cstr(&directory), module, ANTL_SUFFIX);
    /* A package file is immutable once cached, so a file that is there
       and carries its digest is fetched no second time. */
    if (!path_exists(text_cstr(out))) {
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
        goto done;
    }
    ok = true;
done:
    text_free(&directory);
    text_free(&url);
    return ok;
}

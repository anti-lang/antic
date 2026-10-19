#ifndef ANTI_REPO_H
#define ANTI_REPO_H

/* The interface of repo.c: the rule of a repository URL and the fetch
   into the cache of the user. A repository is a URL prefix serving
   static files, so a fetch is one request per file. `docs/tooling.md`
   gives the three paths under a prefix, and `docs/decisions.md` allows
   `https://` and `file://`, with `http://` for `127.0.0.1`, `localhost`
   and `[::1]` alone. */

#include <stdbool.h>

#include "text.h"

/* The index file of a package under a prefix. */
#define REPO_INDEX_FILE "index.toml"

/* Whether url is a repository URL that the rules allow. Writes a message
   naming the URL and the rule otherwise. */
bool repo_url_allowed(const char *url);

/* What a repository answered for one file. */
enum repo_answer {
    REPO_HELD,      /* the file is there */
    REPO_ABSENT,    /* the repository holds no such file */
    REPO_FAILED     /* no answer, which a message names */
};

/* The index file of package name under prefix, fetched or refreshed into
   the cache. The check of a cached index runs at most once an hour.
   offline contacts no repository and fails when the file is not cached.
   out holds the path of the file where the answer is REPO_HELD. out may
   hold text after another answer too, and the caller frees it with
   text_free either way. */
enum repo_answer repo_index(const char *prefix, const char *name,
                            bool offline, struct text *out);

/* The library file of one module of one version, fetched into the cache
   when it is not there and verified against digest either way. Returns
   false when the file cannot be had or its digest differs, and out holds
   its path otherwise. The caller frees out with text_free. */
bool repo_module(const char *prefix, const char *name, const char *version,
                 const char *module, const char *digest, bool offline,
                 struct text *out);

#endif

#ifndef ANTI_REPO_H
#define ANTI_REPO_H

#include <stdbool.h>
#include <stddef.h>

#include "text.h"

/* A repository is a URL prefix serving static files, so a fetch is one
   request per file. `docs/tooling.md` gives the three paths under a
   prefix, and `docs/decisions.md` allows `https://` and `file://`, with
   `http://` for `127.0.0.1` and `localhost` alone. */

/* The index file of a package under a prefix, and the directory of the
   library files of one version. */
#define REPO_INDEX_FILE "index.toml"

/* Whether name is a module path: lowercase ASCII identifiers joined by
   single dots. A package name and the module path of a library file both
   take this form, and a string that does not stands in no path. */
bool repo_name_valid(const char *name);

/* Whether digest is a SHA-256 digest in 64 lowercase hex digits. */
bool repo_digest_valid(const char *digest);

/* The cache of a user, which is per user and not per project. It holds
   the index files under index/<digest of the prefix>/<name>/ and the
   library files under pkg/<name>/<version>/. */
bool repo_cache_dir(struct text *out);

/* Whether url is a repository URL that the rules allow. Writes a message
   naming the URL and the rule otherwise. */
bool repo_url_allowed(const char *url);

/* The index file of package name under prefix, fetched or refreshed into
   the cache. The check of a cached index runs at most once an hour.
   offline contacts no repository and fails when the file is not cached.
   Returns false when the file cannot be had, and out holds its path
   otherwise. */
bool repo_index(const char *prefix, const char *name, bool offline,
                struct text *out);

/* The library file of one module of one version, fetched into the cache
   when it is not there and verified against digest either way. Returns
   false when the file cannot be had or its digest differs, and out holds
   its path otherwise. */
bool repo_module(const char *prefix, const char *name, const char *version,
                 const char *module, const char *digest, bool offline,
                 struct text *out);

#endif

/* The dependency graph of a project: the lock file, the index of a
   repository and the walk that closes the graph, with the comparison of
   versions and constraints and the reader of the header of a library
   file.

   DESIGN: resolution follows "Resolution" in docs/tooling.md. The lock
   file answers the manifest or it does not, and a lock that does answer
   skips the indexes. A dependency of a path is read from disk on every
   build. The files under it are the developer's own and change without
   a version. A dependency of a repository is pinned by the lock, and
   its digest is checked at every build. */

#include "deps.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "antic.h"
#include "arena.h"
#include "files.h"
#include "modpath.h"
#include "repo.h"
#include "say.h"
#include "sha256.h"
#include "text.h"
#include "toml.h"
#include "units.h"

/* The walk stops after this many rounds. A round resolves every package
   whose pick no longer satisfies every constraint on it. A graph that
   still moves here holds a pair of constraints that pull at each
   other. */
enum { DEPS_ROUNDS = 64 };

/* The largest part of a version, the one of MANIFEST_VERSION_DIGITS nines. */
#define DEPS_VERSION_PART_MAX INT64_C(999999999)

/* One part of a version, and where the next part starts. A part above
   DEPS_VERSION_PART_MAX reads as one above it, so no digit string
   overflows and `next_major` adds 1 to a bounded value. */
static int64_t version_part(const char **p)
{
    int64_t value = 0;

    while (**p >= '0' && **p <= '9') {
        if (value <= DEPS_VERSION_PART_MAX) {
            value = value * 10 + (**p - '0');
        }
        (*p)++;
    }
    if (value > DEPS_VERSION_PART_MAX) {
        value = DEPS_VERSION_PART_MAX + 1;
    }
    if (**p == '.') {
        (*p)++;
    }
    return value;
}

int deps_version_compare(const char *a, const char *b)
{
    int i;

    for (i = 0; i < 3; i++) {
        int64_t left = version_part(&a);
        int64_t right = version_part(&b);
        if (left != right) {
            return left < right ? -1 : 1;
        }
    }
    return 0;
}

/* The version above the upper bound of a bare constraint, which is the
   next major version. */
static void next_major(const char *version, struct text *out)
{
    const char *p = version;
    int64_t major = version_part(&p);

    text_appendf(out, "%" PRId64 ".0.0", major + 1);
}

bool deps_constraint_valid(const char *constraint)
{
    if (constraint[0] == '=') {
        constraint++;
    } else if (constraint[0] == '>' && constraint[1] == '=') {
        constraint += 2;
    }
    return manifest_version_valid(constraint);
}

bool deps_satisfies(const char *constraint, const char *version)
{
    struct text bound = {0};
    bool ok;

    if (!manifest_version_valid(version)) {
        return false;
    }
    if (constraint == NULL || constraint[0] == '\0') {
        return true;
    }
    if (!deps_constraint_valid(constraint)) {
        return false;
    }
    if (constraint[0] == '=') {
        return deps_version_compare(version, constraint + 1) == 0;
    }
    if (constraint[0] == '>' && constraint[1] == '=') {
        return deps_version_compare(version, constraint + 2) >= 0;
    }
    next_major(constraint, &bound);
    ok = deps_version_compare(version, constraint) >= 0 &&
         deps_version_compare(version, text_cstr(&bound)) < 0;
    text_free(&bound);
    return ok;
}

/* One requirement on a package, from the manifest or from the index
   entry of another package. */
struct requirement {
    struct text name;
    struct text constraint;
    struct text repo;           /* a URL, empty when none was named */
    struct text path;           /* a directory, empty otherwise */
    struct text from;           /* the package whose pick needs it, empty
                                   for the manifest */
};

struct resolver {
    const struct manifest *m;
    const char *root;
    bool offline;
    dep_path_fn path_of;
    void *context;
    struct requirement *requirements;
    size_t requirement_count;
    struct dep_graph graph;
};

static void requirement_free(struct requirement *r)
{
    text_free(&r->name);
    text_free(&r->constraint);
    text_free(&r->repo);
    text_free(&r->path);
    text_free(&r->from);
}

static void package_free(struct dep_package *p)
{
    size_t i;

    for (i = 0; i < p->module_count; i++) {
        text_free(&p->modules[i].path);
        text_free(&p->modules[i].digest);
        text_free(&p->modules[i].file);
    }
    free(p->modules);
    for (i = 0; i < p->need_count; i++) {
        text_free(&p->needs[i]);
    }
    free(p->needs);
    text_free(&p->name);
    text_free(&p->version);
    text_free(&p->repo);
    text_free(&p->path);
    memset(p, 0, sizeof *p);
}

void deps_free(struct dep_graph *g)
{
    size_t i;

    for (i = 0; i < g->count; i++) {
        package_free(&g->packages[i]);
    }
    free(g->packages);
    memset(g, 0, sizeof *g);
}

static struct dep_package *graph_package(struct dep_graph *g, const char *name)
{
    size_t i;

    for (i = 0; i < g->count; i++) {
        if (strcmp(text_cstr(&g->packages[i].name), name) == 0) {
            return &g->packages[i];
        }
    }
    return NULL;
}

static struct dep_package *graph_add(struct dep_graph *g, const char *name)
{
    struct dep_package *p = graph_package(g, name);

    if (p != NULL) {
        return p;
    }
    g->packages = files_resize(g->packages, g->count + 1,
                               sizeof *g->packages);
    p = &g->packages[g->count++];
    memset(p, 0, sizeof *p);
    text_append(&p->name, name);
    return p;
}

static struct dep_module *package_module(struct dep_package *p,
                                         const char *module)
{
    size_t i;

    for (i = 0; i < p->module_count; i++) {
        if (strcmp(text_cstr(&p->modules[i].path), module) == 0) {
            return &p->modules[i];
        }
    }
    p->modules = files_resize(p->modules, p->module_count + 1,
                              sizeof *p->modules);
    memset(&p->modules[p->module_count], 0, sizeof *p->modules);
    text_append(&p->modules[p->module_count].path, module);
    return &p->modules[p->module_count++];
}

/* Add one requirement of from, or widen the one already there. Two
   requirements on one package keep both constraints, so the pick
   satisfies each. from is the package whose pick needs it, or "" for the
   manifest. */
static void require(struct resolver *r, const char *name,
                    const char *constraint, const char *repo, const char *path,
                    const char *from)
{
    struct requirement *item;
    size_t i;

    for (i = 0; i < r->requirement_count; i++) {
        if (strcmp(text_cstr(&r->requirements[i].name), name) == 0 &&
            strcmp(text_cstr(&r->requirements[i].constraint), constraint) == 0 &&
            strcmp(text_cstr(&r->requirements[i].from), from) == 0) {
            return;
        }
    }
    r->requirements = files_resize(r->requirements, r->requirement_count + 1,
                                   sizeof *r->requirements);
    item = &r->requirements[r->requirement_count++];
    memset(item, 0, sizeof *item);
    text_append(&item->name, name);
    text_append(&item->constraint, constraint);
    if (repo != NULL) {
        text_append(&item->repo, repo);
    }
    if (path != NULL) {
        text_append(&item->path, path);
    }
    text_append(&item->from, from);
}

/* Remove the pick of name from the graph. */
static void graph_remove(struct dep_graph *g, const char *name)
{
    struct dep_package *p = graph_package(g, name);
    size_t at;

    if (p == NULL) {
        return;
    }
    at = (size_t)(p - g->packages);
    package_free(p);
    memmove(&g->packages[at], &g->packages[at + 1],
            (g->count - at - 1) * sizeof *g->packages);
    g->count--;
}

/* DESIGN: a pick holds the requirements its own dependencies make, and
   nothing else does. Taking the pick of name back drops them. A package
   that one of them constrained may then take a higher version, or be
   needed by nothing, so its pick goes back as well, and the walk picks
   again what is still required. The worklist holds the names whose pick
   goes back, and a name already out of the graph adds nothing, so a
   cycle of dependencies ends. */
static void unpick(struct resolver *r, const char *name)
{
    struct text *work = NULL;
    size_t count = 0;
    size_t i;

    work = files_resize(work, 1, sizeof *work);
    memset(&work[0], 0, sizeof *work);
    text_append(&work[count++], name);
    while (count > 0) {
        struct text one = work[--count];
        const char *gone = text_cstr(&one);
        graph_remove(&r->graph, gone);
        for (i = 0; i < r->requirement_count;) {
            struct requirement *req = &r->requirements[i];
            if (strcmp(text_cstr(&req->from), gone) != 0) {
                i++;
                continue;
            }
            if (graph_package(&r->graph, text_cstr(&req->name)) != NULL) {
                work = files_resize(work, count + 1, sizeof *work);
                memset(&work[count], 0, sizeof *work);
                text_append(&work[count++], text_cstr(&req->name));
            }
            requirement_free(req);
            memmove(req, req + 1,
                    (r->requirement_count - i - 1) * sizeof *req);
            r->requirement_count--;
        }
        text_free(&one);
    }
    free(work);
}

/* Whether version satisfies every requirement on name. */
static bool satisfies_all(const struct resolver *r, const char *name,
                          const char *version)
{
    size_t i;

    for (i = 0; i < r->requirement_count; i++) {
        if (strcmp(text_cstr(&r->requirements[i].name), name) == 0 &&
            !deps_satisfies(text_cstr(&r->requirements[i].constraint),
                            version)) {
            return false;
        }
    }
    return true;
}

/* The directory a path requirement names, which is relative to the
   directory of the manifest. */
static void resolve_path(const struct resolver *r, const char *path,
                         struct text *out)
{
    if (path[0] == '/' || (path[0] != '\0' && path[1] == ':')) {
        text_append(out, path);
        return;
    }
    text_appendf(out, "%s/%s", r->root, path);
}

/* The package header of a library file: its package name, its version,
   its module path and, with r, the dependencies it carries as
   requirements of the package from. */
static bool library_header(const char *file, struct text *name,
                           struct text *version, struct text *module_path,
                           struct resolver *r, const char *from)
{
    struct arena arena = {0};
    struct package package;
    const char *module;
    size_t i;
    bool ok = false;

    if (!unit_library_header(file, &arena, &package, &module)) {
        goto done;
    }
    name->length = 0;
    version->length = 0;
    module_path->length = 0;
    text_append(name, package.name);
    text_append(version, package.version);
    text_append(module_path, module);
    for (i = 0; r != NULL && i < package.dependency_count; i++) {
        const struct package_dependency *d = &package.dependencies[i];
        require(r, d->name, d->constraint == NULL ? "" : d->constraint,
                d->url, NULL, from);
    }
    ok = true;
done:
    arena_free(&arena);
    return ok;
}

/* DESIGN: a project of a path dependency names its own dependencies in
   its manifest, and its library files carry them by name, constraint and
   URL alone, with no path. A dependency of the project by a path is
   therefore required here by that path, joined to the path of the
   project, so a chain of projects resolves at any depth, by R165 of
   "Rules of 2026-10-13" and "Dependencies" in docs/tooling.md. T169 and
   T179 of docs/audit/tests-findings.md. */
static bool require_project_paths(struct resolver *r,
                                  const struct requirement *req,
                                  const char *given)
{
    struct text manifest_path = {0};
    struct manifest m;
    size_t i;
    bool ok = true;

    memset(&m, 0, sizeof m);
    text_appendf(&manifest_path, "%s/%s", given, MANIFEST_FILE);
    if (files_exists(text_cstr(&manifest_path))) {
        ok = manifest_read(text_cstr(&manifest_path), false, &m);
    }
    for (i = 0; ok && i < m.dependency_count; i++) {
        const struct manifest_dependency *d = &m.dependencies[i];
        const char *path = text_cstr(&d->path);
        struct text joined = {0};
        if (d->path.length == 0) {
            continue;
        }
        if (path[0] == '/' || (path[0] != '\0' && path[1] == ':')) {
            text_append(&joined, path);
        } else {
            text_appendf(&joined, "%s/%s", text_cstr(&req->path), path);
        }
        require(r, text_cstr(&d->name), text_cstr(&d->version), NULL,
                text_cstr(&joined), text_cstr(&req->name));
        text_free(&joined);
    }
    manifest_free(&m);
    text_free(&manifest_path);
    return ok;
}

/* Read a package from a directory of library files. Every `.antl` under
   it whose header names this package belongs to it, as the module its
   header names. M339 of the audit: the module path came from the place
   of the file, and a dev build reads it from the header, so a file under
   another name gave two answers. */
static bool resolve_from_path(struct resolver *r, const struct requirement *req,
                              struct dep_package *out)
{
    struct text directory = {0};
    struct text given = {0};
    struct text name = {0};
    struct text version = {0};
    struct text module = {0};
    struct files_list files = {0};
    size_t i;
    size_t found = 0;
    bool ok = false;

    resolve_path(r, text_cstr(&req->path), &given);
    /* A directory that holds a manifest is a project, which the caller
       builds first and answers with its dist directory. A directory of
       library files is given as it is. */
    if (!r->path_of(r->context, text_cstr(&given), &directory)) {
        goto done;
    }
    if (!files_list_tree(text_cstr(&directory), ANTL_SUFFIX, &files)) {
        goto done;
    }
    /* library_header empties name, version and module before each file. */
    for (i = 0; i < files.count; i++) {
        const char *file = text_cstr(&files.items[i]);
        char hex[65];
        if (!library_header(file, &name, &version, &module, NULL, NULL)) {
            goto done;
        }
        if (strcmp(text_cstr(&name), text_cstr(&req->name)) == 0) {
            /* The first file of the package gives its version and its
               dependencies, which every file of one package carries. */
            if (found == 0) {
                out->version.length = 0;
                text_append(&out->version, text_cstr(&version));
                out->path.length = 0;
                text_append(&out->path, text_cstr(&directory));
                if (!library_header(file, &name, &version, &module, r,
                                    text_cstr(&req->name)) ||
                    !require_project_paths(r, req, text_cstr(&given))) {
                    goto done;
                }
            }
            if (!sha256_file(file, hex)) {
                say_anti("cannot read %s\n", file);
                goto done;
            }
            {
                struct dep_module *m = package_module(out, text_cstr(&module));
                /* The package is new to this walk, so a module that holds
                   a file already came from another file of the directory,
                   and a build would take one of the two. */
                if (m->file.length > 0) {
                    say_anti("%s and %s both hold the module "
                             "%s\n", text_cstr(&m->file), file,
                             text_cstr(&module));
                    goto done;
                }
                m->digest.length = 0;
                text_append(&m->digest, hex);
                m->file.length = 0;
                text_append(&m->file, file);
            }
            found++;
        }
    }
    if (found == 0) {
        say_anti("%s holds no library file of the package %s\n",
                 text_cstr(&directory), text_cstr(&req->name));
        goto done;
    }
    if (!satisfies_all(r, text_cstr(&req->name), text_cstr(&out->version))) {
        say_anti("%s holds %s %s, which no constraint on the "
                 "package takes\n", text_cstr(&directory),
                 text_cstr(&req->name), text_cstr(&out->version));
        goto done;
    }
    ok = true;
done:
    files_list_free(&files);
    text_free(&directory);
    text_free(&given);
    text_free(&name);
    text_free(&version);
    text_free(&module);
    return ok;
}

/* The value of the key `<head>.<index><tail>`, which reads one element of
   an array of inline tables. */
static const char *value_at(const struct anti_toml *doc, const char *head,
                            size_t index, const char *tail)
{
    struct text key = {0};
    const char *value;

    text_appendf(&key, "%s.%zu%s", head, index, tail);
    value = manifest_value(doc, text_cstr(&key));
    text_free(&key);
    return value;
}

/* The highest version of the index doc of the package of req, read
   from file, that is not yanked and satisfies every requirement on the
   package, as its place in best. have says whether one answers. Returns
   false where the index names a version that is no version. */
static bool best_version(const struct resolver *r,
                         const struct requirement *req,
                         const struct anti_toml *doc, const char *file,
                         size_t *best, bool *have)
{
    size_t i;

    for (i = 0;; i++) {
        const char *version = value_at(doc, "version", i, ".version");
        const char *yanked = value_at(doc, "version", i, ".yanked");
        if (version == NULL) {
            break;
        }
        /* S40, S45 and M13: a version reaches the comparison, a path of
           the cache and the lock file. */
        if (!manifest_version_valid(version)) {
            say_anti("%s names the version %s of %s, which is "
                     "no version\n", file, version,
                     text_cstr(&req->name));
            return false;
        }
        if (yanked != NULL && strcmp(yanked, "true") == 0) {
            continue;
        }
        if (!satisfies_all(r, text_cstr(&req->name), version)) {
            continue;
        }
        if (!*have ||
            deps_version_compare(version,
                                 value_at(doc, "version", *best,
                                          ".version")) > 0) {
            *best = i;
            *have = true;
        }
    }
    return true;
}

/* The modules of version best of the index doc, read from file, with
   the digest of each, into out. Returns false where one is no module
   path or its digest no SHA-256 digest. */
static bool index_modules(const struct anti_toml *doc, const char *file,
                          size_t best, struct dep_package *out)
{
    size_t i;

    for (i = 0;; i++) {
        struct text head = {0};
        const char *module;
        const char *digest;
        struct dep_module *m;
        text_appendf(&head, "version.%zu.modules", best);
        module = value_at(doc, text_cstr(&head), i, ".path");
        digest = value_at(doc, text_cstr(&head), i, ".sha256");
        text_free(&head);
        if (module == NULL || digest == NULL) {
            break;
        }
        if (!manifest_name_valid(module)) {
            say_anti("%s names the module %s, which is no "
                     "module path\n", file, module);
            return false;
        }
        if (!manifest_digest_valid(digest)) {
            say_anti("%s names the digest %s of %s, which is no "
                     "SHA-256 digest\n", file, digest, module);
            return false;
        }
        m = package_module(out, module);
        m->digest.length = 0;
        text_append(&m->digest, digest);
    }
    return true;
}

/* Require each dependency of version best of the index doc of the
   package of req, read from file. Returns false where one names no
   package, no constraint or a repository anti refuses. */
static bool index_dependencies(struct resolver *r,
                               const struct requirement *req,
                               const struct anti_toml *doc, const char *file,
                               size_t best)
{
    size_t i;

    for (i = 0;; i++) {
        struct text head = {0};
        const char *name;
        const char *constraint;
        const char *url;
        text_appendf(&head, "version.%zu.dependencies", best);
        name = value_at(doc, text_cstr(&head), i, ".name");
        constraint = value_at(doc, text_cstr(&head), i, ".version");
        url = value_at(doc, text_cstr(&head), i, ".repo");
        text_free(&head);
        if (name == NULL) {
            break;
        }
        if (!manifest_name_valid(name)) {
            say_anti("%s names the dependency %s, which is no "
                     "package name\n", file, name);
            return false;
        }
        if (constraint != NULL && !deps_constraint_valid(constraint)) {
            say_anti("%s names the version %s of %s, which is "
                     "no constraint\n", file, constraint, name);
            return false;
        }
        if (url != NULL && !repo_url_allowed(url)) {
            return false;
        }
        require(r, name, constraint == NULL ? "" : constraint, url, NULL,
                text_cstr(&req->name));
    }
    return true;
}

/* Read one package from the index of a repository: the highest version
   that is not yanked and satisfies every requirement on the package.
   found is false where the repository holds no index of the package or
   no version of it answers. Returns false where the repository gives no
   answer or an index that anti refuses. */
static bool resolve_from_index(struct resolver *r, const struct requirement *req,
                               const char *prefix, struct dep_package *out,
                               bool *found)
{
    struct text file = {0};
    struct text bytes = {0};
    struct anti_toml *doc = NULL;
    size_t best = 0;
    bool have_best = false;
    bool ok = false;

    *found = false;
    switch (repo_index(prefix, text_cstr(&req->name), r->offline, &file)) {
    case REPO_HELD:
        break;
    case REPO_ABSENT:
        ok = true;
        goto done;
    case REPO_FAILED:
        goto done;
    }
    if (!files_read(text_cstr(&file), &bytes)) {
        say_anti("cannot read %s\n", text_cstr(&file));
        goto done;
    }
    doc = anti_rt_toml_read((const unsigned char *)bytes.data,
                            (int64_t)bytes.length);
    if (doc == NULL) {
        say_anti("%s is no index that anti reads\n",
                 text_cstr(&file));
        goto done;
    }
    if (!best_version(r, req, doc, text_cstr(&file), &best, &have_best)) {
        goto done;
    }
    /* An index that holds no version that answers is an answer. The
       repository has no version of the package, as one without the index
       has none, and the search goes on to the next one. */
    if (!have_best) {
        ok = true;
        goto done;
    }
    *found = true;
    out->version.length = 0;
    text_append(&out->version, value_at(doc, "version", best, ".version"));
    out->repo.length = 0;
    text_append(&out->repo, prefix);
    if (!index_modules(doc, text_cstr(&file), best, out)) {
        goto done;
    }
    if (out->module_count == 0) {
        say_anti("%s names no module of %s %s\n",
                 text_cstr(&file), text_cstr(&req->name),
                 text_cstr(&out->version));
        goto done;
    }
    ok = index_dependencies(r, req, doc, text_cstr(&file), best);
done:
    anti_rt_toml_free(doc);
    text_free(&bytes);
    text_free(&file);
    return ok;
}

/* Resolve one package from its repository, or from every repository of
   the manifest in order when the requirement names none. */
static bool resolve_from_repo(struct resolver *r, const struct requirement *req,
                              struct dep_package *out)
{
    const char *named = text_cstr(&req->repo);
    struct text winner = {0};
    size_t i;
    bool found = false;
    bool ok = false;

    if (named[0] != '\0') {
        if (!resolve_from_index(r, req, named, out, &found) || !found) {
            say_anti("%s has no version of %s that satisfies "
                     "every constraint on it\n", named,
                     text_cstr(&req->name));
            return false;
        }
        return true;
    }
    /* A dependency that names no repository is searched for in every
       repository of the manifest. Two that hold it is an error rather
       than a choice this tool makes. */
    for (i = 0; i < r->m->repository_count; i++) {
        const char *prefix = text_cstr(&r->m->repositories[i].url);
        struct dep_package one;
        bool here = false;
        memset(&one, 0, sizeof one);
        text_append(&one.name, text_cstr(&req->name));
        /* A repository that gives no answer may hold the package, so
           which one does stays unknown. */
        if (!resolve_from_index(r, req, prefix, &one, &here)) {
            say_anti("%s gave no index of %s that anti reads, "
                     "so the build cannot tell which repository "
                     "holds it\n",
                     prefix, text_cstr(&req->name));
            package_free(&one);
            goto done;
        }
        if (here) {
            if (winner.length > 0) {
                say_anti("%s and %s both hold %s, so the "
                         "dependency names which one\n",
                         text_cstr(&winner), prefix, text_cstr(&req->name));
                package_free(&one);
                goto done;
            }
            text_append(&winner, prefix);
            package_free(out);
            text_append(&out->name, text_cstr(&req->name));
            out->version = one.version;
            out->repo = one.repo;
            out->modules = one.modules;
            out->module_count = one.module_count;
            /* The rest of one moved into out, and its name is a copy. */
            text_free(&one.name);
            memset(&one, 0, sizeof one);
        }
        package_free(&one);
    }
    if (winner.length == 0) {
        say_anti("no repository of the manifest has a version of "
                 "%s that satisfies every constraint on it\n",
                 text_cstr(&req->name));
        goto done;
    }
    ok = true;
done:
    text_free(&winner);
    return ok;
}

/* The requirement of name that names a source, which is the first one
   that carries a path or a repository. */
static const struct requirement *source_of(const struct resolver *r,
                                           const char *name)
{
    size_t i;

    for (i = 0; i < r->requirement_count; i++) {
        const struct requirement *req = &r->requirements[i];
        if (strcmp(text_cstr(&req->name), name) == 0 &&
            (req->path.length > 0 || req->repo.length > 0)) {
            return req;
        }
    }
    for (i = 0; i < r->requirement_count; i++) {
        if (strcmp(text_cstr(&r->requirements[i].name), name) == 0) {
            return &r->requirements[i];
        }
    }
    return NULL;
}

/* One round of the walk: resolve every package that has no pick or whose
   pick no longer satisfies every constraint on it. A new pick of a
   package first takes its old pick back, with the requirements that one
   made. The round walks the names required when it starts, since a pick
   adds requirements and taking one back removes them. */
static bool walk_round(struct resolver *r, bool *changed)
{
    struct text *names = NULL;
    size_t count = 0;
    size_t i;
    size_t j;
    bool ok = true;

    *changed = false;
    if (r->requirement_count > 0) {
        names = files_array(r->requirement_count, sizeof *names);
    }
    for (i = 0; i < r->requirement_count; i++) {
        const char *name = text_cstr(&r->requirements[i].name);
        bool seen = false;
        for (j = 0; j < count && !seen; j++) {
            seen = strcmp(text_cstr(&names[j]), name) == 0;
        }
        if (!seen) {
            text_append(&names[count++], name);
        }
    }
    for (i = 0; ok && i < count; i++) {
        const char *name = text_cstr(&names[i]);
        struct requirement source;
        struct dep_package *p = graph_package(&r->graph, name);
        const struct requirement *req;
        if (p != NULL && satisfies_all(r, name, text_cstr(&p->version))) {
            continue;
        }
        req = source_of(r, name);
        if (req == NULL) {
            continue;
        }
        /* The pick answers every constraint on the package, so the copy
           that resolution reads carries the source alone. Resolution
           adds requirements, which moves the list, so nothing below
           points into it. */
        memset(&source, 0, sizeof source);
        text_append(&source.name, name);
        text_append(&source.repo, text_cstr(&req->repo));
        text_append(&source.path, text_cstr(&req->path));
        if (p != NULL) {
            unpick(r, name);
        }
        p = graph_add(&r->graph, name);
        ok = source.path.length > 0 ? resolve_from_path(r, &source, p)
                                    : resolve_from_repo(r, &source, p);
        requirement_free(&source);
        *changed = true;
    }
    for (i = 0; i < count; i++) {
        text_free(&names[i]);
    }
    free(names);
    return ok;
}

/* Add the name need to the packages p needs, once. */
static void package_need(struct dep_package *p, const char *need)
{
    size_t i;

    for (i = 0; i < p->need_count; i++) {
        if (strcmp(text_cstr(&p->needs[i]), need) == 0) {
            return;
        }
    }
    p->needs = files_resize(p->needs, p->need_count + 1, sizeof *p->needs);
    memset(&p->needs[p->need_count], 0, sizeof *p->needs);
    text_append(&p->needs[p->need_count++], need);
}

/* Give every package of the graph of a settled walk the packages it
   needs, which are the requirements its pick made. */
static void record_needs(struct resolver *r)
{
    size_t i;
    size_t j;

    for (i = 0; i < r->graph.count; i++) {
        struct dep_package *p = &r->graph.packages[i];
        for (j = 0; j < r->requirement_count; j++) {
            if (strcmp(text_cstr(&r->requirements[j].from),
                       text_cstr(&p->name)) == 0) {
                package_need(p, text_cstr(&r->requirements[j].name));
            }
        }
    }
}

/* Whether the strings of one package of the lock file follow the grammar
   of their kind, which the index they came from followed. A lock file is
   committed and cloned, so it is read as foreign input. */
static bool lock_package_valid(const char *path, const char *name,
                               const char *version, const char *repo)
{
    const char *kind = NULL;
    const char *what = NULL;

    if (!manifest_name_valid(name)) {
        kind = "package name";
        what = name;
    } else if (!manifest_version_valid(version)) {
        kind = "version";
        what = version;
    } else if (repo != NULL && strchr(repo, '"') != NULL) {
        kind = "repository URL";
        what = repo;
    }
    if (kind != NULL) {
        say_anti("%s names %s, which is no %s, so %s is not "
                 "read\n", path, what, kind, path);
        return false;
    }
    return true;
}

static bool lock_module_valid(const char *path, const char *module,
                              const char *digest)
{
    if (!manifest_name_valid(module)) {
        say_anti("%s names %s, which is no module path, so %s "
                 "is not read\n", path, module, path);
        return false;
    }
    if (!manifest_digest_valid(digest)) {
        say_anti("%s names %s, which is no SHA-256 digest, so "
                 "%s is not read\n", path, digest, path);
        return false;
    }
    return true;
}

/* Read the lock file into a graph. A file that is not there gives an
   empty graph and is no error. A file that holds a string that is not the
   grammar of its kind reads as none, with a message naming it. A file
   that is no TOML is an error that names it, by R167 of "Rules of
   2026-10-13", and gives false. */
static bool lock_read(const char *path, struct dep_graph *out)
{
    struct text bytes = {0};
    struct anti_toml *doc;
    size_t i;
    bool valid = true;

    memset(out, 0, sizeof *out);
    if (!files_read(path, &bytes)) {
        text_free(&bytes);
        return true;
    }
    doc = anti_rt_toml_read((const unsigned char *)bytes.data,
                            (int64_t)bytes.length);
    text_free(&bytes);
    if (doc == NULL) {
        say_anti("%s is no TOML that anti reads\n", path);
        return false;
    }
    for (i = 0;; i++) {
        const char *name = value_at(doc, "package", i, ".name");
        const char *version = value_at(doc, "package", i, ".version");
        const char *repo = value_at(doc, "package", i, ".repo");
        const char *path_value = value_at(doc, "package", i, ".path");
        struct dep_package *p;
        size_t j;
        if (name == NULL || version == NULL) {
            break;
        }
        if (!lock_package_valid(path, name, version, repo)) {
            deps_free(out);
            break;
        }
        p = graph_add(out, name);
        text_append(&p->version, version);
        if (repo != NULL) {
            text_append(&p->repo, repo);
        }
        if (path_value != NULL) {
            text_append(&p->path, path_value);
        }
        for (j = 0;; j++) {
            struct text head = {0};
            const char *module;
            const char *digest;
            text_appendf(&head, "package.%zu.modules", i);
            module = value_at(doc, text_cstr(&head), j, ".path");
            digest = value_at(doc, text_cstr(&head), j, ".sha256");
            text_free(&head);
            if (module == NULL || digest == NULL) {
                break;
            }
            if (!lock_module_valid(path, module, digest)) {
                valid = false;
                break;
            }
            text_append(&package_module(p, module)->digest, digest);
        }
        for (j = 0; valid; j++) {
            struct text head = {0};
            const char *need;
            text_appendf(&head, "package.%zu.needs", i);
            need = value_at(doc, text_cstr(&head), j, "");
            text_free(&head);
            if (need == NULL) {
                break;
            }
            if (!manifest_name_valid(need)) {
                say_anti("%s names %s, which is no package "
                         "name, so %s is not read\n", path, need, path);
                valid = false;
                break;
            }
            package_need(p, need);
        }
        if (!valid) {
            deps_free(out);
            break;
        }
    }
    anti_rt_toml_free(doc);
    return true;
}

/* Append `key = "value"` to out. M13: the TOML that anti reads takes no
   escape, and a backslash stands for itself, so a Windows path reads
   back as written. A value that holds a double quote or a control byte
   cannot be written. The lock file is then refused, and no value writes
   a key of its own choosing. The strings of an index are refused on
   read, so a value here that fails comes from a path on this machine. */
static bool lock_string(struct text *out, const char *key, const char *value,
                        const char *path)
{
    const char *p;

    for (p = value; *p != '\0'; p++) {
        if (*p == '"' || (unsigned char)*p < 0x20 || *p == 0x7f) {
            say_anti("%s cannot hold the %s %s, which holds a "
                     "double quote or a control byte\n", path, key,
                     value);
            return false;
        }
    }
    text_appendf(out, "%s = \"%s\"", key, value);
    return true;
}

static bool lock_write(const char *path, const struct dep_graph *g)
{
    struct text out = {0};
    size_t i;
    size_t j;
    bool ok = true;

    text_append(&out, "# Written by `anti build`. Commit it for an "
                      "application.\n");
    text_append(&out, "version = 1\n");
    for (i = 0; ok && i < g->count; i++) {
        const struct dep_package *p = &g->packages[i];
        text_append(&out, "\n[[package]]\n");
        ok = lock_string(&out, "name", text_cstr(&p->name), path);
        text_append(&out, "\n");
        ok = ok && lock_string(&out, "version", text_cstr(&p->version), path);
        text_append(&out, "\n");
        if (ok && p->repo.length > 0) {
            ok = lock_string(&out, "repo", text_cstr(&p->repo), path);
            text_append(&out, "\n");
        }
        if (ok && p->path.length > 0) {
            ok = lock_string(&out, "path", text_cstr(&p->path), path);
            text_append(&out, "\n");
        }
        text_append(&out, "modules = [\n");
        for (j = 0; ok && j < p->module_count; j++) {
            text_append(&out, "    { ");
            ok = lock_string(&out, "path", text_cstr(&p->modules[j].path),
                             path);
            text_append(&out, ", ");
            ok = ok && lock_string(&out, "sha256",
                                   text_cstr(&p->modules[j].digest), path);
            text_append(&out, " },\n");
        }
        text_append(&out, "]\n");
        /* The lock reader takes no escape, and a name of the graph is a
           package name, which holds neither a quote nor a control byte. */
        if (p->need_count > 0) {
            text_append(&out, "needs = [");
            for (j = 0; j < p->need_count; j++) {
                text_appendf(&out, "%s\"%s\"", j > 0 ? ", " : "",
                             text_cstr(&p->needs[j]));
            }
            text_append(&out, "]\n");
        }
    }
    if (!ok) {
        text_free(&out);
        return false;
    }
    ok = files_write(path, &out);
    text_free(&out);
    return ok;
}

/* Whether the dependencies of the manifest reach every package of g
   through the packages each one needs. A package that nothing reaches is
   one the manifest no longer asks for. The lock kept it in the graph, so
   it was fetched and named in every library file and in the notice, by
   defect 9 of docs/audit/anti-tool.md. */
static bool lock_reached(const struct manifest *m, const struct dep_graph *g)
{
    bool *reached = files_array(g->count + 1, sizeof *reached);
    size_t i;
    size_t j;
    size_t k;
    bool grew = true;
    bool all = true;

    for (i = 0; i < g->count; i++) {
        for (j = 0; j < m->dependency_count && !reached[i]; j++) {
            reached[i] = strcmp(text_cstr(&g->packages[i].name),
                                text_cstr(&m->dependencies[j].name)) == 0;
        }
    }
    while (grew) {
        grew = false;
        for (i = 0; i < g->count; i++) {
            const struct dep_package *p = &g->packages[i];
            for (j = 0; reached[i] && j < p->need_count; j++) {
                for (k = 0; k < g->count; k++) {
                    if (!reached[k] &&
                        strcmp(text_cstr(&g->packages[k].name),
                               text_cstr(&p->needs[j])) == 0) {
                        reached[k] = true;
                        grew = true;
                    }
                }
            }
        }
    }
    for (i = 0; i < g->count; i++) {
        all = all && reached[i];
    }
    free(reached);
    return all;
}

/* Whether the locked graph answers the manifest. Every dependency of a
   repository stands in it, with a version its constraint takes and the
   repository it names. A dependency of a path is read from disk on
   every build, so the lock does not answer for one, unless paths is
   false, which asks about the dependencies of a repository alone. */
static bool lock_answers(const struct manifest *m, const struct dep_graph *g,
                         bool paths)
{
    size_t i;

    if (g->count == 0) {
        return m->dependency_count == 0;
    }
    for (i = 0; i < m->dependency_count; i++) {
        const struct manifest_dependency *d = &m->dependencies[i];
        const struct dep_package *p;
        size_t j;
        if (d->path.length > 0) {
            if (paths) {
                return false;
            }
            continue;
        }
        p = NULL;
        for (j = 0; j < g->count; j++) {
            if (strcmp(text_cstr(&g->packages[j].name),
                       text_cstr(&d->name)) == 0) {
                p = &g->packages[j];
                break;
            }
        }
        if (p == NULL || p->path.length > 0 ||
            !deps_satisfies(text_cstr(&d->version), text_cstr(&p->version))) {
            return false;
        }
        if (d->repo.length > 0) {
            const char *url = manifest_repository_url(m, text_cstr(&d->repo));
            if (url == NULL || strcmp(url, text_cstr(&p->repo)) != 0) {
                return false;
            }
        }
    }
    return lock_reached(m, g);
}

/* Put every library file of the graph on this machine and check its
   digest. A package of a repository is fetched into the cache, and a
   package of a path already names its files. */
static bool fetch_all(struct dep_graph *g, bool offline)
{
    size_t i;
    size_t j;

    for (i = 0; i < g->count; i++) {
        struct dep_package *p = &g->packages[i];
        for (j = 0; j < p->module_count; j++) {
            struct dep_module *m = &p->modules[j];
            if (m->file.length > 0) {
                continue;
            }
            if (p->repo.length == 0) {
                say_anti("the lock file names %s of %s with no "
                         "repository and no path\n",
                         text_cstr(&m->path), text_cstr(&p->name));
                return false;
            }
            if (!repo_module(text_cstr(&p->repo), text_cstr(&p->name),
                             text_cstr(&p->version), text_cstr(&m->path),
                             text_cstr(&m->digest), offline, &m->file)) {
                return false;
            }
        }
    }
    return true;
}

/* Whether two graphs hold the same packages, versions, digests and needs,
   which decides whether the lock file is written again. */
static bool graphs_equal(const struct dep_graph *a, const struct dep_graph *b)
{
    size_t i;
    size_t j;

    if (a->count != b->count) {
        return false;
    }
    for (i = 0; i < a->count; i++) {
        const struct dep_package *x = &a->packages[i];
        const struct dep_package *y = &b->packages[i];
        if (strcmp(text_cstr(&x->name), text_cstr(&y->name)) != 0 ||
            strcmp(text_cstr(&x->version), text_cstr(&y->version)) != 0 ||
            strcmp(text_cstr(&x->repo), text_cstr(&y->repo)) != 0 ||
            x->module_count != y->module_count ||
            x->need_count != y->need_count) {
            return false;
        }
        for (j = 0; j < x->need_count; j++) {
            if (strcmp(text_cstr(&x->needs[j]), text_cstr(&y->needs[j])) != 0) {
                return false;
            }
        }
        for (j = 0; j < x->module_count; j++) {
            if (strcmp(text_cstr(&x->modules[j].path),
                       text_cstr(&y->modules[j].path)) != 0 ||
                strcmp(text_cstr(&x->modules[j].digest),
                       text_cstr(&y->modules[j].digest)) != 0) {
                return false;
            }
        }
    }
    return true;
}

bool deps_resolve(const struct manifest *m, const char *root, bool offline,
                  dep_path_fn path_of, void *context, struct dep_graph *out)
{
    struct resolver r;
    struct dep_graph locked = {0};
    struct text lock_path = {0};
    size_t i;
    int round;
    bool ok = false;

    memset(out, 0, sizeof *out);
    memset(&r, 0, sizeof r);
    r.m = m;
    r.root = root;
    r.offline = offline;
    r.path_of = path_of;
    r.context = context;
    text_appendf(&lock_path, "%s/%s", root, LOCK_FILE);
    if (!lock_read(text_cstr(&lock_path), &locked)) {
        goto done;
    }
    if (lock_answers(m, &locked, true)) {
        /* The lock file answers, so the indexes are not read at all. A
           project with no dependency still gets one, because the file is
           what `anti build` writes beside the manifest. */
        *out = locked;
        memset(&locked, 0, sizeof locked);
        ok = fetch_all(out, offline) &&
             (files_exists(text_cstr(&lock_path)) ||
              lock_write(text_cstr(&lock_path), out));
        goto done;
    }
    /* R167 of "Rules of 2026-10-13": a lock whose entries no longer
       answer a dependency of a repository says so. One whose project has
       a path dependency is read again on every build, which is no news. */
    if (locked.count > 0 && !lock_answers(m, &locked, false)) {
        say_anti("%s no longer answers the manifest, so the "
                 "build writes it again\n", text_cstr(&lock_path));
    }
    for (i = 0; i < m->dependency_count; i++) {
        const struct manifest_dependency *d = &m->dependencies[i];
        const char *url = d->repo.length == 0
                              ? NULL
                              : manifest_repository_url(m, text_cstr(&d->repo));
        require(&r, text_cstr(&d->name), text_cstr(&d->version), url,
                d->path.length == 0 ? NULL : text_cstr(&d->path), "");
    }
    for (round = 0; round < DEPS_ROUNDS; round++) {
        bool changed = false;
        if (!walk_round(&r, &changed)) {
            goto done;
        }
        if (!changed) {
            break;
        }
    }
    if (round == DEPS_ROUNDS) {
        say_anti("the dependencies of this project do not settle on one "
                 "version each\n");
        goto done;
    }
    record_needs(&r);
    if (!fetch_all(&r.graph, offline)) {
        goto done;
    }
    /* The lock file is written when the graph differs from the one it
       held, and when there is no lock file yet. */
    if ((!graphs_equal(&r.graph, &locked) ||
         !files_exists(text_cstr(&lock_path))) &&
        !lock_write(text_cstr(&lock_path), &r.graph)) {
        goto done;
    }
    *out = r.graph;
    memset(&r.graph, 0, sizeof r.graph);
    ok = true;
done:
    for (i = 0; i < r.requirement_count; i++) {
        requirement_free(&r.requirements[i]);
    }
    free(r.requirements);
    deps_free(&r.graph);
    deps_free(&locked);
    text_free(&lock_path);
    if (!ok) {
        deps_free(out);
    }
    return ok;
}

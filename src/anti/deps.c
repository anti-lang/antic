/* The dependency graph of a project: the lock file, the index of a
   repository and the walk that closes the graph.

   DESIGN: resolution follows "Resolution" in docs/tooling.md. The lock
   file answers the manifest or it does not, and a lock that does answer
   skips the indexes. A dependency of a path is read from disk on every
   build. The files under it are the developer's own and change without
   a version. A dependency of a repository is pinned by the lock, and
   its digest is checked at every build. */
#include "deps.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "antl.h"
#include "arena.h"
#include "files.h"
#include "modpath.h"
#include "repo.h"
#include "sema.h"
#include "sha256.h"
#include "text.h"
#include "toml.h"

/* The walk stops after this many rounds. A round resolves every package
   whose pick no longer satisfies every constraint on it. A graph that
   still moves here holds a pair of constraints that pull at each
   other. */
enum { DEPS_ROUNDS = 64 };

static void die_out_of_memory(void)
{
    fputs("anti: out of memory\n", stderr);
    exit(70);
}

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

/* One part of a version, and where the next part starts. */
static long version_part(const char **p)
{
    long value = 0;

    while (**p >= '0' && **p <= '9') {
        value = value * 10 + (**p - '0');
        (*p)++;
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
        long left = version_part(&a);
        long right = version_part(&b);
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
    long major = version_part(&p);

    text_appendf(out, "%ld.0.0", major + 1);
}

bool deps_satisfies(const char *constraint, const char *version)
{
    struct text bound = {0};
    bool ok;

    if (constraint == NULL || constraint[0] == '\0') {
        return true;
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
    g->packages = realloc(g->packages, (g->count + 1) * sizeof *g->packages);
    if (g->packages == NULL) {
        die_out_of_memory();
    }
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
    p->modules = realloc(p->modules, (p->module_count + 1) * sizeof *p->modules);
    if (p->modules == NULL) {
        die_out_of_memory();
    }
    memset(&p->modules[p->module_count], 0, sizeof *p->modules);
    text_append(&p->modules[p->module_count].path, module);
    return &p->modules[p->module_count++];
}

/* Add one requirement, or widen the one already there. Two requirements
   on one package keep both constraints, so the pick satisfies each. */
static void require(struct resolver *r, const char *name,
                    const char *constraint, const char *repo, const char *path)
{
    struct requirement *item;
    size_t i;

    for (i = 0; i < r->requirement_count; i++) {
        if (strcmp(text_cstr(&r->requirements[i].name), name) == 0 &&
            strcmp(text_cstr(&r->requirements[i].constraint), constraint) == 0) {
            return;
        }
    }
    r->requirements = realloc(r->requirements,
                              (r->requirement_count + 1) *
                                  sizeof *r->requirements);
    if (r->requirements == NULL) {
        die_out_of_memory();
    }
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

/* The package header of a library file: its package name, its version
   and, with r, the dependencies it carries as requirements. */
static bool library_header(const char *file, struct text *name,
                           struct text *version, struct resolver *r)
{
    struct arena arena = {0};
    struct interface iface;
    struct text bytes = {0};
    char message[256];
    size_t i;
    bool ok = false;

    if (!read_file(file, &bytes)) {
        fprintf(stderr, "anti: cannot read %s\n", file);
        goto done;
    }
    memset(&iface, 0, sizeof iface);
    if (!antl_header((const uint8_t *)bytes.data, bytes.length, &arena, &iface,
                     message, sizeof message)) {
        fprintf(stderr, "anti: %s: %s\n", file, message);
        goto done;
    }
    name->length = 0;
    version->length = 0;
    text_append(name, iface.package.name);
    text_append(version, iface.package.version);
    for (i = 0; r != NULL && i < iface.package.dependency_count; i++) {
        const struct package_dependency *d = &iface.package.dependencies[i];
        require(r, d->name, d->constraint == NULL ? "" : d->constraint,
                d->url, NULL);
    }
    ok = true;
done:
    text_free(&bytes);
    arena_free(&arena);
    return ok;
}

/* The module path of a library file under directory: its path below that
   directory, without the suffix and with a dot for every separator. */
static void module_of_file(const char *directory, const char *file,
                           struct text *out)
{
    size_t skip = strlen(directory);
    size_t length;
    size_t i;

    while (skip > 0 && directory[skip - 1] == '/') {
        skip--;
    }
    if (strncmp(file, directory, skip) == 0 && file[skip] == '/') {
        file += skip + 1;
    }
    text_append(out, file);
    length = strlen(ANTL_SUFFIX);
    if (out->length >= length &&
        strcmp(out->data + out->length - length, ANTL_SUFFIX) == 0) {
        out->length -= length;
        out->data[out->length] = '\0';
    }
    for (i = 0; i < out->length; i++) {
        if (out->data[i] == '/') {
            out->data[i] = '.';
        }
    }
}

/* Read a package from a directory of library files. Every `.antl` under
   it whose header names this package belongs to it. */
static bool resolve_from_path(struct resolver *r, const struct requirement *req,
                              struct dep_package *out)
{
    struct text directory = {0};
    struct text given = {0};
    struct file_list files = {0};
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
    if (!list_tree(text_cstr(&directory), ANTL_SUFFIX, &files)) {
        goto done;
    }
    for (i = 0; i < files.count; i++) {
        struct text name = {0};
        struct text version = {0};
        struct text module = {0};
        const char *file = text_cstr(&files.items[i]);
        char hex[65];
        if (!library_header(file, &name, &version, NULL)) {
            text_free(&name);
            text_free(&version);
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
                if (!library_header(file, &name, &version, r)) {
                    text_free(&name);
                    text_free(&version);
                    text_free(&module);
                    goto done;
                }
            }
            module_of_file(text_cstr(&directory), file, &module);
            if (!sha256_file(file, hex)) {
                fprintf(stderr, "anti: cannot read %s\n", file);
                text_free(&name);
                text_free(&version);
                text_free(&module);
                goto done;
            }
            {
                struct dep_module *m = package_module(out, text_cstr(&module));
                m->digest.length = 0;
                text_append(&m->digest, hex);
                m->file.length = 0;
                text_append(&m->file, file);
            }
            found++;
        }
        text_free(&name);
        text_free(&version);
        text_free(&module);
    }
    if (found == 0) {
        fprintf(stderr, "anti: %s holds no library file of the package %s\n",
                text_cstr(&directory), text_cstr(&req->name));
        goto done;
    }
    if (!satisfies_all(r, text_cstr(&req->name), text_cstr(&out->version))) {
        fprintf(stderr, "anti: %s holds %s %s, which no constraint on the "
                        "package takes\n", text_cstr(&directory),
                text_cstr(&req->name), text_cstr(&out->version));
        goto done;
    }
    ok = true;
done:
    file_list_free(&files);
    text_free(&directory);
    text_free(&given);
    return ok;
}

/* The value of one key of a document, or NULL. */
static const char *value_of(const struct anti_toml *doc, const char *key)
{
    int64_t at = anti_rt_toml_find(doc, (const unsigned char *)key,
                                   (int64_t)strlen(key));
    struct anti_text value;

    if (at < 0) {
        return NULL;
    }
    value = anti_rt_toml_value(doc, at);
    return value.ptr == NULL ? NULL : (const char *)value.ptr;
}

/* The value of the key `<head>.<index><tail>`, which reads one element of
   an array of inline tables. */
static const char *value_at(const struct anti_toml *doc, const char *head,
                            size_t index, const char *tail)
{
    struct text key = {0};
    const char *value;

    text_appendf(&key, "%s.%zu%s", head, index, tail);
    value = value_of(doc, text_cstr(&key));
    text_free(&key);
    return value;
}

/* Read one package from the index of a repository: the highest version
   that is not yanked and satisfies every requirement on the package. */
static bool resolve_from_index(struct resolver *r, const struct requirement *req,
                               const char *prefix, struct dep_package *out,
                               bool *found)
{
    struct text file = {0};
    struct text bytes = {0};
    struct anti_toml *doc = NULL;
    size_t best = 0;
    bool have_best = false;
    size_t i;
    bool ok = false;

    *found = false;
    if (!repo_index(prefix, text_cstr(&req->name), r->offline, &file)) {
        goto done;
    }
    if (!read_file(text_cstr(&file), &bytes)) {
        fprintf(stderr, "anti: cannot read %s\n", text_cstr(&file));
        goto done;
    }
    doc = anti_rt_toml_read((const unsigned char *)bytes.data,
                            (int64_t)bytes.length);
    if (doc == NULL) {
        fprintf(stderr, "anti: %s is no index that anti reads\n",
                text_cstr(&file));
        goto done;
    }
    for (i = 0;; i++) {
        const char *version = value_at(doc, "version", i, ".version");
        const char *yanked = value_at(doc, "version", i, ".yanked");
        if (version == NULL) {
            break;
        }
        if (yanked != NULL && strcmp(yanked, "true") == 0) {
            continue;
        }
        if (!satisfies_all(r, text_cstr(&req->name), version)) {
            continue;
        }
        if (!have_best ||
            deps_version_compare(version,
                                 value_at(doc, "version", best,
                                          ".version")) > 0) {
            best = i;
            have_best = true;
        }
    }
    if (!have_best) {
        goto done;
    }
    *found = true;
    out->version.length = 0;
    text_append(&out->version, value_at(doc, "version", best, ".version"));
    out->repo.length = 0;
    text_append(&out->repo, prefix);
    for (i = 0;; i++) {
        struct text head = {0};
        const char *module;
        const char *digest;
        text_appendf(&head, "version.%zu.modules", best);
        module = value_at(doc, text_cstr(&head), i, ".path");
        digest = value_at(doc, text_cstr(&head), i, ".sha256");
        text_free(&head);
        if (module == NULL || digest == NULL) {
            break;
        }
        {
            struct dep_module *m = package_module(out, module);
            m->digest.length = 0;
            text_append(&m->digest, digest);
        }
    }
    if (out->module_count == 0) {
        fprintf(stderr, "anti: %s names no module of %s %s\n",
                text_cstr(&file), text_cstr(&req->name),
                text_cstr(&out->version));
        goto done;
    }
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
        require(r, name, constraint == NULL ? "" : constraint, url, NULL);
    }
    ok = true;
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
            fprintf(stderr, "anti: %s has no version of %s that satisfies "
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
        if (resolve_from_index(r, req, prefix, &one, &here) && here) {
            if (winner.length > 0) {
                fprintf(stderr, "anti: %s and %s both hold %s, so the "
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
            memset(&one, 0, sizeof one);
        }
        package_free(&one);
    }
    if (winner.length == 0) {
        fprintf(stderr, "anti: no repository of the manifest has a version of "
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
   pick no longer satisfies every constraint on it. */
static bool walk_round(struct resolver *r, bool *changed)
{
    size_t i;

    *changed = false;
    for (i = 0; i < r->requirement_count; i++) {
        struct requirement source;
        struct dep_package *p;
        const struct requirement *req;
        struct text name = {0};
        bool ok;
        text_append(&name, text_cstr(&r->requirements[i].name));
        p = graph_package(&r->graph, text_cstr(&name));
        if (p != NULL &&
            satisfies_all(r, text_cstr(&name), text_cstr(&p->version))) {
            text_free(&name);
            continue;
        }
        req = source_of(r, text_cstr(&name));
        if (req == NULL) {
            text_free(&name);
            continue;
        }
        /* The pick answers every constraint on the package, so the copy
           that resolution reads carries the source alone. Resolution
           adds requirements, which moves the list, so nothing below
           points into it. */
        memset(&source, 0, sizeof source);
        text_append(&source.name, text_cstr(&name));
        text_append(&source.repo, text_cstr(&req->repo));
        text_append(&source.path, text_cstr(&req->path));
        p = graph_add(&r->graph, text_cstr(&name));
        package_free(p);
        text_append(&p->name, text_cstr(&name));
        ok = source.path.length > 0 ? resolve_from_path(r, &source, p)
                                    : resolve_from_repo(r, &source, p);
        requirement_free(&source);
        text_free(&name);
        if (!ok) {
            return false;
        }
        *changed = true;
    }
    return true;
}

/* Read the lock file into a graph. A file that is not there, or that no
   longer reads, gives an empty graph and is no error. */
static void lock_read(const char *path, struct dep_graph *out)
{
    struct text bytes = {0};
    struct anti_toml *doc;
    size_t i;

    memset(out, 0, sizeof *out);
    if (!read_file(path, &bytes)) {
        text_free(&bytes);
        return;
    }
    doc = anti_rt_toml_read((const unsigned char *)bytes.data,
                            (int64_t)bytes.length);
    text_free(&bytes);
    if (doc == NULL) {
        return;
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
            text_append(&package_module(p, module)->digest, digest);
        }
    }
    anti_rt_toml_free(doc);
}

static bool lock_write(const char *path, const struct dep_graph *g)
{
    struct text out = {0};
    FILE *f;
    size_t i;
    size_t j;
    bool ok;

    text_append(&out, "# Written by `anti build`. Commit it for an "
                      "application.\n");
    text_append(&out, "version = 1\n");
    for (i = 0; i < g->count; i++) {
        const struct dep_package *p = &g->packages[i];
        text_append(&out, "\n[[package]]\n");
        text_appendf(&out, "name = \"%s\"\n", text_cstr(&p->name));
        text_appendf(&out, "version = \"%s\"\n", text_cstr(&p->version));
        if (p->repo.length > 0) {
            text_appendf(&out, "repo = \"%s\"\n", text_cstr(&p->repo));
        }
        if (p->path.length > 0) {
            text_appendf(&out, "path = \"%s\"\n", text_cstr(&p->path));
        }
        text_append(&out, "modules = [\n");
        for (j = 0; j < p->module_count; j++) {
            text_appendf(&out, "    { path = \"%s\", sha256 = \"%s\" },\n",
                         text_cstr(&p->modules[j].path),
                         text_cstr(&p->modules[j].digest));
        }
        text_append(&out, "]\n");
    }
    f = fopen(path, "wb");
    if (f == NULL) {
        fprintf(stderr, "anti: cannot write %s\n", path);
        text_free(&out);
        return false;
    }
    ok = out.length == 0 || fwrite(out.data, 1, out.length, f) == out.length;
    fclose(f);
    if (!ok) {
        fprintf(stderr, "anti: cannot write %s\n", path);
    }
    text_free(&out);
    return ok;
}

/* Whether the locked graph answers the manifest. Every dependency of a
   repository stands in it, with a version its constraint takes and the
   repository it names. A dependency of a path is read from disk on
   every build, so the lock does not answer for one. */
static bool lock_answers(const struct manifest *m, const struct dep_graph *g,
                         const char *root)
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
            return false;
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
    (void)root;
    return true;
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
                fprintf(stderr, "anti: the lock file names %s of %s with no "
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

/* Whether two graphs hold the same packages, versions and digests, which
   decides whether the lock file is written again. */
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
            x->module_count != y->module_count) {
            return false;
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
    lock_read(text_cstr(&lock_path), &locked);
    if (lock_answers(m, &locked, root)) {
        /* The lock file answers, so the indexes are not read at all. A
           project with no dependency still gets one, because the file is
           what `anti build` writes beside the manifest. */
        *out = locked;
        memset(&locked, 0, sizeof locked);
        ok = fetch_all(out, offline) &&
             (path_exists(text_cstr(&lock_path)) ||
              lock_write(text_cstr(&lock_path), out));
        goto done;
    }
    for (i = 0; i < m->dependency_count; i++) {
        const struct manifest_dependency *d = &m->dependencies[i];
        const char *url = d->repo.length == 0
                              ? NULL
                              : manifest_repository_url(m, text_cstr(&d->repo));
        require(&r, text_cstr(&d->name), text_cstr(&d->version), url,
                d->path.length == 0 ? NULL : text_cstr(&d->path));
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
        fputs("anti: the dependencies of this project do not settle on one "
              "version each\n", stderr);
        goto done;
    }
    if (!fetch_all(&r.graph, offline)) {
        goto done;
    }
    /* The lock file is written when the graph differs from the one it
       held, and when there is no lock file yet. */
    if ((!graphs_equal(&r.graph, &locked) ||
         !path_exists(text_cstr(&lock_path))) &&
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

/* The manifest of a project, the version constraints of its dependencies
   and the repository URLs the rules allow. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../binary_stdio.h"
#include "check.h"
#include "deps.h"
#include "manifest.h"
#include "repo.h"

/* Write text into a file of the working directory and give its name. */
static const char *fixture(const char *name, const char *text)
{
    FILE *f = fopen(name, "wb");

    if (f == NULL) {
        return NULL;
    }
    fwrite(text, 1, strlen(text), f);
    fclose(f);
    return name;
}

static void versions(void)
{
    CHECK(deps_version_compare("1.2.4", "1.2.4") == 0);
    /* A missing part is zero, so `2.0` and `2.0.0` are one version. */
    CHECK(deps_version_compare("2.0", "2.0.0") == 0);
    CHECK(deps_version_compare("1.2.4", "1.2.10") < 0);
    CHECK(deps_version_compare("1.10.0", "1.9.0") > 0);
    CHECK(deps_version_compare("2.0.0", "10.0.0") < 0);
}

static void constraints(void)
{
    /* `1.2.4` is `>= 1.2.4` and `< 2.0.0`. */
    CHECK(deps_satisfies("1.2.4", "1.2.4"));
    CHECK(deps_satisfies("1.2.4", "1.9.0"));
    CHECK(!deps_satisfies("1.2.4", "1.2.3"));
    CHECK(!deps_satisfies("1.2.4", "2.0.0"));
    /* `=1.2.4` is that version alone. */
    CHECK(deps_satisfies("=1.2.4", "1.2.4"));
    CHECK(!deps_satisfies("=1.2.4", "1.2.5"));
    /* `>=1.2.4` has no upper bound. */
    CHECK(deps_satisfies(">=1.2.4", "9.9.9"));
    CHECK(!deps_satisfies(">=1.2.4", "1.2.3"));
    /* A dependency with a path alone carries no constraint. */
    CHECK(deps_satisfies("", "0.0.0"));
    /* A zero major version keeps the upper bound of its own major. */
    CHECK(deps_satisfies("0.3.0", "0.9.0"));
    CHECK(!deps_satisfies("0.3.0", "1.0.0"));
}

static void urls(void)
{
    CHECK(repo_url_allowed("https://anti.example.com/repo"));
    CHECK(repo_url_allowed("file:///tmp/repo"));
    CHECK(repo_url_allowed("http://127.0.0.1:8080/repo"));
    CHECK(repo_url_allowed("http://localhost/repo"));
    CHECK(!repo_url_allowed("http://example.com/repo"));
    CHECK(!repo_url_allowed("ftp://example.com/repo"));
}

/* Every field of the manifest, with the four directories defaulted. */
static void manifest_fields(void)
{
    static const char text[] =
        "[package]\n"
        "name = \"com.example.app\"\n"
        "version = \"0.3.0\"\n"
        "antic = \"0.5\"\n"
        "license = \"MIT\"\n"
        "\n"
        "[targets]\n"
        "default = [\"macos-arm64\"]\n"
        "all = [\"linux-x86_64\", \"macos-arm64\"]\n"
        "\n"
        "[repositories]\n"
        "ff = \"https://anti.example.com/repo\"\n"
        "\n"
        "[dependencies]\n"
        "\"com.example.tree\" = { version = \"1.2.4\", repo = \"ff\" }\n"
        "\"com.example.matrix\" = { path = \"../libs\" }\n";
    const char *path = fixture("unit-manifest.toml", text);
    struct manifest m;

    CHECK(path != NULL);
    if (path == NULL) {
        return;
    }
    CHECK(manifest_read(path, false, &m));
    CHECK_STR(text_cstr(&m.name), "com.example.app");
    CHECK_STR(text_cstr(&m.version), "0.3.0");
    CHECK_STR(text_cstr(&m.antic), "0.5");
    CHECK_STR(text_cstr(&m.license), "MIT");
    CHECK_STR(text_cstr(&m.src), "src");
    CHECK_STR(text_cstr(&m.test), "test");
    CHECK_STR(text_cstr(&m.build), "build");
    CHECK_STR(text_cstr(&m.dist), "dist");
    CHECK(m.default_target_count == 1);
    CHECK(m.all_target_count == 2);
    CHECK(m.repository_count == 1);
    CHECK_STR(manifest_repository_url(&m, "ff"), "https://anti.example.com/repo");
    CHECK(manifest_repository_url(&m, "other") == NULL);
    CHECK(m.dependency_count == 2);
    if (m.dependency_count == 2) {
        CHECK_STR(text_cstr(&m.dependencies[0].name), "com.example.tree");
        CHECK_STR(text_cstr(&m.dependencies[0].version), "1.2.4");
        CHECK_STR(text_cstr(&m.dependencies[0].repo), "ff");
        CHECK_STR(text_cstr(&m.dependencies[1].name), "com.example.matrix");
        CHECK_STR(text_cstr(&m.dependencies[1].path), "../libs");
    }
    manifest_free(&m);
    remove(path);
}

/* `[layout]` names another directory for any of the four. */
static void manifest_layout(void)
{
    static const char text[] =
        "[package]\n"
        "name = \"com.example.app\"\n"
        "\n"
        "[layout]\n"
        "src = \"source\"\n"
        "dist = \"out\"\n";
    const char *path = fixture("unit-layout.toml", text);
    struct manifest m;

    CHECK(path != NULL);
    if (path == NULL) {
        return;
    }
    CHECK(manifest_read(path, false, &m));
    CHECK_STR(text_cstr(&m.src), "source");
    CHECK_STR(text_cstr(&m.test), "test");
    CHECK_STR(text_cstr(&m.dist), "out");
    manifest_free(&m);
    remove(path);
}

/* The three refusals: a dependency of a repository and a path, one with
   no version and one naming a repository that `[repositories]` does not.
   A file without `[package] name` is no manifest either. */
static void manifest_refusals(void)
{
    static const char both[] =
        "[package]\nname = \"com.example.app\"\n"
        "[dependencies]\n"
        "\"com.example.tree\" = { version = \"1.0.0\", repo = \"ff\", "
        "path = \"../libs\" }\n"
        "[repositories]\nff = \"file:///tmp/repo\"\n";
    static const char no_version[] =
        "[package]\nname = \"com.example.app\"\n"
        "[dependencies]\n"
        "\"com.example.tree\" = { repo = \"ff\" }\n"
        "[repositories]\nff = \"file:///tmp/repo\"\n";
    static const char no_repo[] =
        "[package]\nname = \"com.example.app\"\n"
        "[dependencies]\n"
        "\"com.example.tree\" = { version = \"1.0.0\", repo = \"nowhere\" }\n";
    static const char no_name[] = "[package]\nversion = \"1.0.0\"\n";
    struct manifest m;
    size_t i;
    const char *texts[4];

    texts[0] = both;
    texts[1] = no_version;
    texts[2] = no_repo;
    texts[3] = no_name;
    for (i = 0; i < 4; i++) {
        const char *path = fixture("unit-refused.toml", texts[i]);
        CHECK(path != NULL);
        if (path == NULL) {
            continue;
        }
        fputs("anti test: the message below is expected\n", stderr);
        CHECK(!manifest_read(path, false, &m));
        remove(path);
    }
}

/* A manifest that is not there is no project. */
static void manifest_missing(void)
{
    struct manifest m;

    remove("unit-absent.toml");
    fputs("anti test: the message below is expected\n", stderr);
    CHECK(!manifest_read("unit-absent.toml", false, &m));
}

void test_deps(void)
{
    versions();
    constraints();
    urls();
    manifest_fields();
    manifest_layout();
    manifest_refusals();
    manifest_missing();
}

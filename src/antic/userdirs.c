#include "userdirs.h"

#include <stdlib.h>
#include <string.h>

#include "linker.h"
#include "selfpath.h"

/* DESIGN: an XDG variable counts only when it holds an absolute path.
   The specification says so. The reason that matters here is another
   one: a relative value resolves against the working directory of
   whoever started the program. That is no directory of the user's
   profile. The default of HOME then stands. */
static bool absolute(const char *path)
{
    return path != NULL && path[0] == '/';
}

/* The value of an XDG root, or NULL when it is unset or relative. */
static const char *xdg(user_dir_env env, const char *name)
{
    const char *value = env(name);

    return absolute(value) ? value : NULL;
}

/* Linux and macOS: the four roots of the XDG base directories, each with
   the fallback the specification names. XDG_BIN_HOME is the one of the
   four that carries no application directory below it. */
static bool unix_dir(struct text *out, enum user_dir which, const char *app,
                     user_dir_env env)
{
    const char *home = env("HOME");
    const char *root;

    switch (which) {
    case USER_DIR_BIN:
        root = xdg(env, "XDG_BIN_HOME");
        if (root != NULL) {
            text_appendf(out, "%s", root);
            return true;
        }
        if (home == NULL) {
            return false;
        }
        text_appendf(out, "%s/.local/bin", home);
        return true;
    case USER_DIR_DATA:
        root = xdg(env, "XDG_DATA_HOME");
        if (root == NULL && home == NULL) {
            return false;
        }
        if (root != NULL) {
            text_appendf(out, "%s/%s", root, app);
        } else {
            text_appendf(out, "%s/.local/share/%s", home, app);
        }
        return true;
    case USER_DIR_CONFIG:
        root = xdg(env, "XDG_CONFIG_HOME");
        if (root == NULL && home == NULL) {
            return false;
        }
        if (root != NULL) {
            text_appendf(out, "%s/%s", root, app);
        } else {
            text_appendf(out, "%s/.config/%s", home, app);
        }
        return true;
    case USER_DIR_CACHE:
        root = xdg(env, "XDG_CACHE_HOME");
        if (root == NULL && home == NULL) {
            return false;
        }
        if (root != NULL) {
            text_appendf(out, "%s/%s", root, app);
        } else {
            text_appendf(out, "%s/.cache/%s", home, app);
        }
        return true;
    }
    return false;
}

/* DESIGN: Windows has one root for the per-user files of an application
   that is not roamed, and LOCALAPPDATA names it. The executables stand
   under Programs\ of it. That is where a user-local install goes, and
   what the Start menu and the PATH of the account expect. The toolchain,
   the configuration and the cache stand under the application directory
   itself. Windows reads none of the XDG variables. */
static bool windows_dir(struct text *out, enum user_dir which, const char *app,
                        user_dir_env env)
{
    const char *local = env("LOCALAPPDATA");

    if (local == NULL) {
        return false;
    }
    switch (which) {
    case USER_DIR_BIN:
        text_appendf(out, "%s\\Programs\\%s\\bin", local, app);
        return true;
    case USER_DIR_DATA:
        text_appendf(out, "%s\\%s", local, app);
        return true;
    case USER_DIR_CONFIG:
        text_appendf(out, "%s\\%s\\config", local, app);
        return true;
    case USER_DIR_CACHE:
        text_appendf(out, "%s\\%s\\cache", local, app);
        return true;
    }
    return false;
}

bool user_dir_of(struct text *out, enum user_dir_os os, enum user_dir which,
                 const char *app, user_dir_env env)
{
    return os == USER_DIR_WINDOWS ? windows_dir(out, which, app, env)
                                  : unix_dir(out, which, app, env);
}

/* An empty variable is unset. Windows leaves a variable of no value
   behind where a shell would have removed it. */
static const char *from_environment(const char *name)
{
    const char *value = getenv(name);

    return value != NULL && value[0] != '\0' ? value : NULL;
}

bool user_dir(struct text *out, enum user_dir which, const char *app)
{
#ifdef _WIN32
    enum user_dir_os os = USER_DIR_WINDOWS;
#else
    enum user_dir_os os = USER_DIR_UNIX;
#endif
    return user_dir_of(out, os, which, app, from_environment);
}

/* The directory above path, without its trailing separator. */
static bool parent_of(struct text *out, const struct text *path)
{
    size_t cut = path->length;

    while (cut > 0 && path->data[cut - 1] != '/' && path->data[cut - 1] != '\\') {
        cut--;
    }
    if (cut <= 1) {
        return false;
    }
    text_append_bytes(out, path->data, cut - 1);
    return true;
}

/* Whether dir holds the lib/ of a runtime archive. */
static bool holds_runtime(const char *dir)
{
    struct text lib = {0};
    bool found;

    text_appendf(&lib, "%s/%s", dir, RUNTIME_LIB_DIR);
    found = directory_exists(text_cstr(&lib));
    text_free(&lib);
    return found;
}

bool runtime_archive(struct text *out)
{
    struct text bin = {0};
    struct text found = {0};

    if (self_directory(&bin) && parent_of(&found, &bin) &&
        holds_runtime(text_cstr(&found))) {
        text_append_bytes(out, found.data, found.length);
        text_free(&bin);
        text_free(&found);
        return true;
    }
    text_free(&bin);
    text_free(&found);
    if (user_dir(&found, USER_DIR_DATA, USER_DIR_APP) &&
        holds_runtime(text_cstr(&found))) {
        text_append_bytes(out, found.data, found.length);
        text_free(&found);
        return true;
    }
    text_free(&found);
    return false;
}

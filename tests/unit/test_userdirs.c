#include "../binary_stdio.h"
#include "check.h"
#include <string.h>

#include "text.h"
#include "userdirs.h"

/* The environment of one case, as pairs of name and value ending in NULL.
   A test passes its own rather than the one of the machine, so both
   platforms are read on either host. */
static const char *const *test_env;

static const char *lookup(const char *name)
{
    size_t i;

    for (i = 0; test_env[i] != NULL; i += 2) {
        if (strcmp(test_env[i], name) == 0) {
            return test_env[i + 1][0] == '\0' ? NULL : test_env[i + 1];
        }
    }
    return NULL;
}

/* Build one directory and compare it. */
static void dir_is(enum user_dir_os os, enum user_dir which, const char *app,
                   const char *const *env, const char *expected)
{
    struct text out = {0};
    bool found;

    test_env = env;
    found = user_dir_of(&out, os, which, app, lookup);
    CHECK(found == (expected != NULL));
    if (expected != NULL) {
        CHECK_STR(text_cstr(&out), expected);
    }
    text_free(&out);
}

/* DESIGN: an install of Anti is local to the user and follows the
   conventions of the platform. The four roots of Linux and macOS are the
   XDG ones, and Windows keeps everything under LOCALAPPDATA. Nothing
   outside the user's profile is ever named. That is why an XDG variable
   holding a relative path is ignored: it would resolve against the
   working directory of whoever started the program. */
static void user_dirs(void)
{
    static const char *const plain[] = {
        "HOME", "/home/eddie", "LOCALAPPDATA", "C:\\Users\\Eddie\\AppData\\Local",
        NULL
    };
    static const char *const xdg[] = {
        "HOME", "/home/eddie", "XDG_BIN_HOME", "/opt/bin",
        "XDG_DATA_HOME", "/data", "XDG_CONFIG_HOME", "/conf",
        "XDG_CACHE_HOME", "/tmp/cache", NULL
    };
    /* A relative value is no root, so the default of HOME stands. */
    static const char *const relative[] = {
        "HOME", "/home/eddie", "XDG_DATA_HOME", "data",
        "XDG_CONFIG_HOME", ".config", NULL
    };
    static const char *const empty[] = {NULL};

    dir_is(USER_DIR_UNIX, USER_DIR_BIN, "anti", plain, "/home/eddie/.local/bin");
    dir_is(USER_DIR_UNIX, USER_DIR_DATA, "anti", plain,
           "/home/eddie/.local/share/anti");
    dir_is(USER_DIR_UNIX, USER_DIR_CONFIG, "anti", plain,
           "/home/eddie/.config/anti");
    dir_is(USER_DIR_UNIX, USER_DIR_CACHE, "anti", plain,
           "/home/eddie/.cache/anti");

    dir_is(USER_DIR_UNIX, USER_DIR_BIN, "anti", xdg, "/opt/bin");
    dir_is(USER_DIR_UNIX, USER_DIR_DATA, "anti", xdg, "/data/anti");
    dir_is(USER_DIR_UNIX, USER_DIR_CONFIG, "anti", xdg, "/conf/anti");
    dir_is(USER_DIR_UNIX, USER_DIR_CACHE, "anti", xdg, "/tmp/cache/anti");

    dir_is(USER_DIR_UNIX, USER_DIR_DATA, "anti", relative,
           "/home/eddie/.local/share/anti");
    dir_is(USER_DIR_UNIX, USER_DIR_CONFIG, "anti", relative,
           "/home/eddie/.config/anti");

    dir_is(USER_DIR_WINDOWS, USER_DIR_BIN, "anti", plain,
           "C:\\Users\\Eddie\\AppData\\Local\\Programs\\anti\\bin");
    dir_is(USER_DIR_WINDOWS, USER_DIR_DATA, "anti", plain,
           "C:\\Users\\Eddie\\AppData\\Local\\anti");
    dir_is(USER_DIR_WINDOWS, USER_DIR_CONFIG, "anti", plain,
           "C:\\Users\\Eddie\\AppData\\Local\\anti\\config");
    dir_is(USER_DIR_WINDOWS, USER_DIR_CACHE, "anti", plain,
           "C:\\Users\\Eddie\\AppData\\Local\\anti\\cache");

    /* The XDG variables are of Linux and macOS, and Windows reads none
       of them. */
    dir_is(USER_DIR_WINDOWS, USER_DIR_DATA, "anti", xdg, NULL);

    /* A package of the other processor stands beside the native one. */
    dir_is(USER_DIR_UNIX, USER_DIR_DATA, "anti-x86_64", plain,
           "/home/eddie/.local/share/anti-x86_64");
    dir_is(USER_DIR_WINDOWS, USER_DIR_DATA, "anti-x86_64", plain,
           "C:\\Users\\Eddie\\AppData\\Local\\anti-x86_64");

    /* Without a home there is no answer, and the caller says so rather
       than writing into a directory of its own choosing. */
    dir_is(USER_DIR_UNIX, USER_DIR_DATA, "anti", empty, NULL);
    dir_is(USER_DIR_WINDOWS, USER_DIR_DATA, "anti", empty, NULL);
}

void test_userdirs(void)
{
    user_dirs();
}

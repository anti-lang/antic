#ifndef ANTIC_USERDIRS_H
#define ANTIC_USERDIRS_H

#include <stdbool.h>

#include "text.h"

/* DESIGN: an install of Anti is local to the user and follows the
   conventions of the platform. Nothing under / or C:\ outside the user's
   profile is written, so a reset of a user is these directories and never
   the machine. "Names and publication" in docs/decisions.md holds the
   rule, and tools/install.sh and tools/install.ps1 write the same four.
   anti.os gives the same three to a program of Anti. */
enum user_dir {
    USER_DIR_BIN,    /* the executables anti and antic */
    USER_DIR_DATA,   /* the toolchain and the runtime archive */
    USER_DIR_CONFIG,
    USER_DIR_CACHE   /* the script-mode cache and the downloads */
};

/* The conventions of a platform, so that one test reads both on either
   host. */
enum user_dir_os { USER_DIR_UNIX, USER_DIR_WINDOWS };

/* Reads one variable of the environment, or NULL when it is unset or
   empty. */
typedef const char *(*user_dir_env)(const char *name);

/* Append the directory to out, without a trailing separator. app names
   the install, "anti" or "anti-<cpu>" for a package of the other
   processor. Returns false when the environment says nothing about where
   the user's files are, and out is then untouched. */
bool user_dir_of(struct text *out, enum user_dir_os os, enum user_dir which,
                 const char *app, user_dir_env env);

/* The same for this host, from its own environment. */
bool user_dir(struct text *out, enum user_dir which, const char *app);

/* The name of the install that antic and anti look for. */
#define USER_DIR_APP "anti"

/* Append the runtime archive of this machine to out, without a trailing
   separator. It is the directory above the running executable when that
   one holds lib/, which is a build tree and an uninstalled package.
   Otherwise it is the user's data directory, when that one holds lib/.
   Returns false when neither does, and out is then untouched. */
bool runtime_archive(struct text *out);

#endif

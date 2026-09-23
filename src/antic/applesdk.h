#ifndef ANTIC_APPLESDK_H
#define ANTIC_APPLESDK_H

#include <stdbool.h>

#include "text.h"

/* DESIGN: Anti never fetches Apple's SDK. A Mac keeps it in
   APPLE_CLT_SDKS, and antic takes the newest one up to
   APPLE_SDK_NEWEST_MAJOR, the newest that ld64.lld of the pinned LLVM
   reads. ld64.lld 23.1.1 refuses the target arm64e.x1 in the stubs of
   27.0. */
#define APPLE_SDK_NEWEST_MAJOR 26
#define APPLE_CLT_SDKS "/Library/Developer/CommandLineTools/SDKs"

/* Append the path of the newest SDK of the Command Line Tools that
   ld64.lld reads to path, and its version to version. Only a Mac has
   one, so elsewhere it returns false. */
bool apple_clt_sdk(struct text *path, struct text *version);

/* Read the version of an SDK directory name of the form
   MacOSX<major>.<minor>.sdk. Each number is decimal digits alone and
   fits an int. Any other name returns false. */
bool apple_sdk_version(const char *name, int *major, int *minor);

#endif

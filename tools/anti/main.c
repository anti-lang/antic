#include <stdio.h>
#include <string.h>

#include "linker.h"
#include "sdk.h"
#include "selfpath.h"
#include "text.h"

static int usage(FILE *out)
{
    fputs("usage: anti sdk export [--sdk <MacOSX.sdk>] [-o <dir>]\n"
          "       anti sdk import <bundle> [--sysroot <dir>]\n"
          "\n"
          "sdk export packs the .tbd stubs and the version of Apple's SDK into\n"
          "apple-sdk-<version>.tar.xz, on a Mac. sdk import unpacks that bundle\n"
          "into the sysroot of Anti on any host, for a program that names a\n"
          "framework. Apple's licence governs where the bundle may be used.\n",
          out);
    return out == stdout ? 0 : 2;
}

/* DESIGN: an installed anti sits in bin/ beside antic, and the sysroot of
   the runtime archive lies beside bin/. --sysroot names another one. */
static bool default_sysroot(struct text *out)
{
    struct text bin = {0};
    size_t cut;

    if (!self_directory(&bin)) {
        text_free(&bin);
        return false;
    }
    cut = bin.length;
    while (cut > 0 && bin.data[cut - 1] != '/' && bin.data[cut - 1] != '\\') {
        cut--;
    }
    text_append_bytes(out, bin.data, cut);
    text_append(out, RUNTIME_SYSROOT_DIR);
    text_free(&bin);
    return true;
}

int main(int argc, char **argv)
{
    int i;

    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        return usage(stdout);
    }
    if (argc < 3 || strcmp(argv[1], "sdk") != 0) {
        return usage(stderr);
    }
    if (strcmp(argv[2], "export") == 0) {
        const char *sdk = NULL;
        const char *out = ".";
        for (i = 3; i < argc; i++) {
            if (strcmp(argv[i], "--sdk") == 0 && i + 1 < argc) {
                sdk = argv[++i];
            } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
                out = argv[++i];
            } else {
                return usage(stderr);
            }
        }
        return sdk_export(sdk, out);
    }
    if (strcmp(argv[2], "import") == 0 && argc >= 4) {
        const char *bundle = argv[3];
        struct text sysroot = {0};
        int status;
        for (i = 4; i < argc; i++) {
            if (strcmp(argv[i], "--sysroot") == 0 && i + 1 < argc) {
                text_append(&sysroot, argv[++i]);
            } else {
                text_free(&sysroot);
                return usage(stderr);
            }
        }
        if (sysroot.length == 0 && !default_sysroot(&sysroot)) {
            fputs("anti: the system does not say where anti is, so pass "
                  "--sysroot\n", stderr);
            return 1;
        }
        status = sdk_import(bundle, text_cstr(&sysroot));
        text_free(&sysroot);
        return status;
    }
    return usage(stderr);
}

#include "notice.h"

#include <string.h>

static const char *or_empty(const char *s)
{
    return s != NULL ? s : "";
}

/* Whether a package before index i has the name of package i. */
static bool repeated(const struct package *const *packages, size_t i)
{
    size_t j;

    for (j = 0; j < i; j++) {
        if (strcmp(or_empty(packages[j]->name),
                   or_empty(packages[i]->name)) == 0) {
            return true;
        }
    }
    return false;
}

/* DESIGN: the identifier of a component stands here once, for the notice
   of a binary and for `anti license`. It is the SPDX expression that the
   text of the component names: the licence of most of glibc and of the
   kernel headers, as the text of each says first, and the choice that
   the texts of Mbed TLS and miniaudio offer. A component the table lacks
   is LicenseRef-<component>, the form of SPDX for a text that is no
   licence of its list. That is the copyright file of a package of Ubuntu
   and the text of mingw-w64, which each hold the notices of several
   licences, and the note on the SDK of Apple. */
static const struct {
    const char *component;
    const char *license;
} component_licenses[] = {
    {RUNTIME_LICENSE_NAME, "0BSD"},
    {MUSL_PACKAGE, "MIT"},
    {MIMALLOC_PACKAGE, "MIT"},
    {"zig", "MIT"},
    {"apsl", "APSL-2.0"},
    {"compiler-rt", "Apache-2.0 WITH LLVM-exception"},
    {"glibc", "LGPL-2.1-or-later"},
    {"linux-headers", "GPL-2.0-only"},
    {"pcre2", "BSD-3-Clause WITH PCRE2-exception"},
    {"sqlite", "blessing"},
    {"mbedtls", "Apache-2.0 OR GPL-2.0-or-later"},
    {"miniaudio", "Unlicense OR MIT-0"},
    {"raylib", "Zlib"},
};

void notice_component_license(const char *component, struct text *out)
{
    size_t i;

    for (i = 0; i < sizeof component_licenses / sizeof component_licenses[0];
         i++) {
        if (strcmp(component_licenses[i].component, component) == 0) {
            text_append(out, component_licenses[i].license);
            return;
        }
    }
    text_appendf(out, "LicenseRef-%s", component);
}

void notice_text(struct text *out, const struct package *const *packages,
                 size_t count)
{
    size_t i;
    size_t j;
    size_t k;

    text_append(out, ANTI_NOTICE_BEGIN);
    for (i = 0; i < count; i++) {
        const struct package *p = packages[i];
        if (repeated(packages, i)) {
            continue;
        }
        text_appendf(out, "package %s %s %s\n", or_empty(p->name),
                     or_empty(p->version), or_empty(p->license));
        for (k = 0; k < p->attribution_count; k++) {
            text_appendf(out, "attribution %s\n", p->attribution[k]);
        }
    }
    for (i = 0; i < count; i++) {
        const char *text = or_empty(packages[i]->license_text);
        if (text[0] == '\0' || repeated(packages, i)) {
            continue;
        }
        for (j = 0; j < i; j++) {
            if (strcmp(or_empty(packages[j]->license_text), text) == 0) {
                break;
            }
        }
        if (j < i) {
            continue;
        }
        text_append(out, "text for");
        for (j = i; j < count; j++) {
            if (strcmp(or_empty(packages[j]->license_text), text) == 0 &&
                !repeated(packages, j)) {
                text_appendf(out, " %s", or_empty(packages[j]->name));
            }
        }
        text_appendf(out, "\n%s", text);
        if (text[strlen(text) - 1] != '\n') {
            text_append(out, "\n");
        }
    }
    text_append(out, ANTI_NOTICE_END);
}

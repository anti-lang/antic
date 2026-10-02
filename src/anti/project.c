/* `anti new`, which writes a project of the default layout. */
#include "project.h"

#include <stdio.h>

#include "files.h"
#include "manifest.h"
#include "modpath.h"
#include "text.h"
#include "units.h"

/* The starter module of `anti new`, which prints and returns. */
static void starter_module(const char *name, struct text *out)
{
    text_appendf(out, "//! The program of the package `%s`.\n\n", name);
    text_append(out, "import anti.io;\n\n");
    text_append(out, "fn main() -> int\n{\n");
    text_append(out, "\tio.println(\"hello\");\n");
    text_append(out, "\treturn 0;\n}\n");
}

int project_new(const char *name)
{
    struct text manifest = {0};
    struct text source = {0};
    struct text path = {0};
    struct text file = {0};
    struct text directory = {0};
    const char *last = modpath_last(name);
    int status = 1;

    if (modpath_segments(name) < 2) {
        fprintf(stderr, "anti: `%s` is one segment, and a package name is a "
                        "module path of at least two, as com.example.%s\n",
                name, name);
        goto done;
    }
    text_append(&directory, last);
    if (files_exists(text_cstr(&directory))) {
        fprintf(stderr, "anti: %s is there already\n", text_cstr(&directory));
        goto done;
    }
    text_appendf(&manifest, "[package]\nname = \"%s\"\nversion = \"0.1.0\"\n",
                 name);
    starter_module(name, &source);
    /* The default layout: the manifest, `src/` with the one module whose
       path is the package name, and `test/`. */
    text_appendf(&path, "%s/src", text_cstr(&directory));
    text_appendf(&file, "%s/%s", text_cstr(&directory), MANIFEST_FILE);
    if (!files_make_dirs(text_cstr(&directory)) ||
        !files_write(text_cstr(&file), &manifest)) {
        goto done;
    }
    file.length = 0;
    if (!unit_file(text_cstr(&path), name, SOURCE_SUFFIX, &file) ||
        !files_write(text_cstr(&file), &source)) {
        goto done;
    }
    path.length = 0;
    text_appendf(&path, "%s/test", text_cstr(&directory));
    if (!files_make_dirs(text_cstr(&path))) {
        goto done;
    }
    printf("anti: %s holds the project %s\n", text_cstr(&directory), name);
    status = 0;
done:
    text_free(&manifest);
    text_free(&source);
    text_free(&path);
    text_free(&file);
    text_free(&directory);
    return status;
}

/* anti bind: bindings of C libraries, and the C header of a library
   file. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bind.h"
#include "driver.h"
#include "files.h"
#include "header.h"
#include "text.h"

static bool write_file(const char *path, const struct text *bytes)
{
    FILE *f = fopen(path, "wb");

    if (f == NULL) {
        fprintf(stderr, "anti: cannot write %s\n", path);
        return false;
    }
    if (bytes->length > 0 &&
        fwrite(bytes->data, 1, bytes->length, f) != bytes->length) {
        fclose(f);
        fprintf(stderr, "anti: cannot write %s\n", path);
        return false;
    }
    fclose(f);
    return true;
}

/* DESIGN: the header comes from the same driver call that `antic --lib`
   writes its header with, over the interfaces of the library file and
   its imports. The two cannot drift, and the test anti_bind_header
   compares their bytes. The file takes the name of the last segment of
   the module, as the library of --lib does without -o. */
int bind_header(const char *library, const char *out_dir,
                const char *runtime, const char **roots, size_t root_count)
{
    struct options o;
    struct text header = {0};
    struct text path = {0};
    const char *base;
    const char *dot;
    int status = 1;

    memset(&o, 0, sizeof o);
    o.input = library;
    o.roots = roots;
    o.root_count = root_count;
    o.runtime = runtime;
    if (!target_host(&o.target)) {
        fputs("anti: unknown host target\n", stderr);
        return 2;
    }
    o.cpu = cpu_default(o.target);
    if (!make_dirs(out_dir) || !driver_library_header(&o, &header)) {
        goto done;
    }
    base = strrchr(library, '/');
    base = base != NULL ? base + 1 : library;
    dot = strrchr(base, '.');
    text_appendf(&path, "%s/%.*s%s", out_dir,
                 (int)(dot != NULL ? (size_t)(dot - base) : strlen(base)),
                 base, HEADER_SUFFIX);
    if (write_file(text_cstr(&path), &header)) {
        status = 0;
    }

done:
    text_free(&header);
    text_free(&path);
    return status;
}

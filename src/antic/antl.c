/* The format of the library file and the top of its reader: the magic,
   the checks that tie the format to the enums it stores, the helpers the
   files share, and antl_header and antl_read, which read the header and
   hand the type table, the items and the IR to antl_read.c and
   antl_read_ir.c. */

#include "antl.h"
#include "antl_io.h"

#include <string.h>

/* DESIGN: the file is a flat sequence of little-endian integers of fixed
   width. A string is a u32 byte count and the bytes. A float is the u64
   of its IEEE 754 bits. No pointer, padding or host byte order reaches the
   file. It stores enum values of types.h, sema.h and ir.h. These checks
   fail when one of them changes, and the version changes with it. */
_Static_assert(TYPE_STRUCT == 24, "raise ANTL_VERSION, then update this");
_Static_assert(TYPE_VARIANT == 28, "raise ANTL_VERSION, then update this");
_Static_assert(TYPE_OPTIONAL == 30, "raise ANTL_VERSION, then update this");
_Static_assert(SYMBOL_CONSTRAINT == 8, "raise ANTL_VERSION, then update this");
_Static_assert(CONST_SYMBOLIC == 8, "raise ANTL_VERSION, then update this");
_Static_assert(SYMBOLIC_CAST == 4, "raise ANTL_VERSION, then update this");
_Static_assert(TOKEN_KIND_COUNT == 180, "raise ANTL_VERSION, then update this");
_Static_assert(IR_LOCK == 11, "raise ANTL_VERSION, then update this");
_Static_assert(IR_UNREACHABLE == 86, "raise ANTL_VERSION, then update this");
_Static_assert(IR_FAIL_GUARD == 3, "raise ANTL_VERSION, then update this");
_Static_assert(IR_SYM == 7, "raise ANTL_VERSION, then update this");
_Static_assert(IR_EXT_ZERO == 2, "raise ANTL_VERSION, then update this");
_Static_assert(IR_CONST_AGG == 6, "raise ANTL_VERSION, then update this");
_Static_assert(IR_AGG_ARRAY == 2, "raise ANTL_VERSION, then update this");
_Static_assert(IR_SYM_OP == 3, "raise ANTL_VERSION, then update this");

const uint8_t antl_magic[4] = {'A', 'N', 'T', 'L'};

bool antl_name_equals(const struct name *n, const char *s)
{
    return n->length == strlen(s) && memcmp(n->text, s, n->length) == 0;
}

const struct interface *antl_library(const struct reader *r,
                                     const struct name *module)
{
    size_t i;

    for (i = 0; i < r->library_count; i++) {
        if (antl_name_equals(module, r->libraries[i]->module)) {
            return r->libraries[i];
        }
    }
    return NULL;
}

static bool read_magic(struct reader *r)
{
    uint32_t version;

    if (r->size < sizeof antl_magic ||
        memcmp(r->data, antl_magic, sizeof antl_magic) != 0) {
        antl_fail(r, "is not a library file");
        return false;
    }
    r->pos = sizeof antl_magic;
    version = antl_get_u32(r);
    if (!r->failed && version != ANTL_VERSION) {
        antl_fail(r, "has format version %u, and antic reads version %u",
                  (unsigned)version, (unsigned)ANTL_VERSION);
    }
    return !r->failed;
}

static void read_header(struct reader *r, struct interface *out)
{
    struct package *p = &out->package;
    struct package_dependency *deps;
    const char **lines;
    uint32_t i;

    memset(out, 0, sizeof *out);
    if (!read_magic(r)) {
        return;
    }
    p->name = antl_get_cstr(r);
    p->version = antl_get_cstr(r);
    p->dependency_count = antl_get_count(r, 12);
    deps = antl_allocate(r, p->dependency_count, sizeof *deps);
    for (i = 0; i < p->dependency_count && !r->failed; i++) {
        deps[i].name = antl_get_cstr(r);
        deps[i].constraint = antl_get_cstr(r);
        deps[i].url = antl_get_cstr(r);
    }
    p->dependencies = deps;
    p->license = antl_get_cstr(r);
    p->license_text = antl_get_cstr(r);
    p->attribution_count = antl_get_count(r, 4);
    lines = antl_allocate(r, p->attribution_count, sizeof *lines);
    for (i = 0; i < p->attribution_count && !r->failed; i++) {
        lines[i] = antl_get_cstr(r);
    }
    p->attribution = lines;
    out->module = antl_get_cstr(r);
    out->import_count = antl_get_count(r, 4);
    out->imports = antl_allocate(r, out->import_count, sizeof *out->imports);
    for (i = 0; i < out->import_count && !r->failed; i++) {
        out->imports[i] = antl_get_cstr(r);
    }
    out->framework_count = antl_get_count(r, 4);
    out->frameworks = antl_allocate(r, out->framework_count,
                                    sizeof *out->frameworks);
    for (i = 0; i < out->framework_count && !r->failed; i++) {
        out->frameworks[i] = antl_get_cstr(r);
    }
    out->linux_library_count = antl_get_count(r, 4);
    out->linux_libraries = antl_allocate(r, out->linux_library_count,
                                         sizeof *out->linux_libraries);
    for (i = 0; i < out->linux_library_count && !r->failed; i++) {
        out->linux_libraries[i] = antl_get_cstr(r);
    }
    out->doc = antl_get_cstr(r);
}

bool antl_header(const uint8_t *data, size_t size, struct arena *arena,
                 struct interface *out, char *error, size_t error_size)
{
    struct reader r;

    memset(&r, 0, sizeof r);
    r.data = data;
    r.size = size;
    r.error = error;
    r.error_size = error_size;
    r.arena = arena;
    read_header(&r, out);
    return !r.failed;
}

struct interface *antl_read(const uint8_t *data, size_t size,
                            const struct interface *const *libraries,
                            size_t library_count, struct types *types,
                            struct arena *arena, struct ir_module *program,
                            char *error, size_t error_size)
{
    struct reader r;
    size_t i;

    memset(&r, 0, sizeof r);
    r.data = data;
    r.size = size;
    r.error = error;
    r.error_size = error_size;
    r.arena = arena;
    r.types = types;
    r.libraries = libraries;
    r.library_count = library_count;
    r.iface = arena_alloc(arena, sizeof *r.iface);
    read_header(&r, r.iface);
    for (i = 0; i < r.iface->import_count && !r.failed; i++) {
        struct name imported;
        imported.text = r.iface->imports[i];
        imported.length = strlen(imported.text);
        if (antl_library(&r, &imported) == NULL) {
            antl_fail(&r, "needs module `%s`", imported.text);
        }
    }
    if (!r.failed) {
        antl_read_types(&r);
    }
    if (!r.failed) {
        antl_read_items(&r);
    }
    if (!r.failed) {
        antl_read_generics(&r);
    }
    if (!r.failed) {
        antl_read_ir(&r, program);
    }
    if (!r.failed && r.pos != r.size) {
        antl_damaged(&r);
    }
    return r.failed ? NULL : r.iface;
}

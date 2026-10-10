/* `anti license` over the runtime archive, a project and a static
   archive.

   DESIGN: every form writes the lines of the notice of a binary, through
   antic_notice_lines, so one reader reads them all and NOTICE.txt of a
   project is the text `anti license --from` prints for its program. The
   markers and the build id are facts of a binary and stand in none of
   them. */
#include "licensing.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "deps.h"
#include "driver.h"
#include "files.h"
#include "symmap.h"

/* The licence of antic and anti, which docs/distribution.md gives under
   "Licence choices for Anti itself", and their names in the plain
   form. */
#define OWN_LICENSE "MIT"
#define OWN_COMPILER "antic"
#define OWN_TOOL "anti"
/* The version of a component that no record gives one. */
#define NO_VERSION "-"

/* A copy of the bytes of t in the memory pool, as a C string. */
static const char *pooled(struct arena *arena, const struct text *t)
{
    char *copy = arena_alloc(arena, t->length + 1);

    if (t->length > 0) {
        memcpy(copy, t->data, t->length);
    }
    copy[t->length] = '\0';
    return copy;
}

/* The component whose text is the file at path of licenses/, or false
   for a file that is no text of a component: the record of upstream
   sources and anything without the suffix of a text. */
static bool component_of(const char *path, struct text *name)
{
    const char *slash = strrchr(path, '/');
    const char *file = slash != NULL ? slash + 1 : path;

    if (strcmp(file, RUNTIME_SOURCES_FILE) == 0 ||
        !files_ends_with(file, RUNTIME_LICENSE_SUFFIX)) {
        return false;
    }
    text_append(name, file);
    files_cut_suffix(name, RUNTIME_LICENSE_SUFFIX);
    return true;
}

/* The package of the component whose text stands at path: its name, the
   version of the record of upstream sources, its identifier and its
   text. */
static bool component_package(const char *path, const char *component,
                              const struct text *sources, struct arena *arena,
                              struct package *out)
{
    struct text text = {0};
    struct text field = {0};
    bool ok = files_read_reported(path, &text);

    memset(out, 0, sizeof *out);
    if (ok) {
        out->license_text = pooled(arena, &text);
        text_append(&field, component);
        out->name = pooled(arena, &field);
        field.length = 0;
        if (!symmap_source_version(sources, component, &field)) {
            text_append(&field, NO_VERSION);
        }
        out->version = pooled(arena, &field);
        field.length = 0;
        antic_component_license(component, &field);
        out->license = pooled(arena, &field);
    }
    text_free(&text);
    text_free(&field);
    return ok;
}

int licensing_plain(const char *runtime)
{
    struct arena arena = {0};
    struct files_list found = {0};
    struct text dir = {0};
    struct text path = {0};
    struct text own = {0};
    struct text sources = {0};
    struct text notice = {0};
    struct package *packages = NULL;
    const struct package **list = NULL;
    size_t count = 0;
    size_t i;
    bool ok;

    text_appendf(&dir, "%s/%s", runtime, RUNTIME_LICENSES_DIR);
    ok = files_list_dir(text_cstr(&dir), &found);
    if (ok && found.count == 0) {
        fprintf(stderr, "anti: %s holds no licence text, so %s is no runtime "
                        "archive\n", text_cstr(&dir), runtime);
        ok = false;
    }
    if (ok) {
        text_appendf(&path, "%s/%s", text_cstr(&dir), RUNTIME_SOURCES_FILE);
        ok = files_read_reported(text_cstr(&path), &sources);
        path.length = 0;
        text_appendf(&path, "%s/%s", runtime, RUNTIME_OWN_LICENSE);
        ok = ok && files_read_reported(text_cstr(&path), &own);
    }
    if (ok) {
        /* antic and anti, then the runtime under the name, the version
           and the text it has in every notice, then every other component
           in the order of the names of the texts. */
        packages = files_array(found.count + 3, sizeof *packages);
        list = files_array(found.count + 3, sizeof *list);
        packages[0].name = OWN_COMPILER;
        packages[1].name = OWN_TOOL;
        for (i = 0; i < 2; i++) {
            packages[i].version = ANTIC_VERSION;
            packages[i].license = OWN_LICENSE;
            packages[i].license_text = pooled(&arena, &own);
        }
        count = 2 + driver_runtime_packages(runtime, false, &arena,
                                            &packages[2]);
        for (i = 0; ok && i < found.count; i++) {
            struct text name = {0};
            if (component_of(text_cstr(&found.items[i]), &name) &&
                strcmp(text_cstr(&name), RUNTIME_LICENSE_NAME) != 0) {
                ok = component_package(text_cstr(&found.items[i]),
                                       text_cstr(&name), &sources, &arena,
                                       &packages[count++]);
            }
            text_free(&name);
        }
    }
    if (ok) {
        for (i = 0; i < count; i++) {
            list[i] = &packages[i];
        }
        antic_notice_lines(&notice, list, count);
        ok = symmap_license_sources(&notice, runtime);
    }
    if (ok) {
        fputs(text_cstr(&notice), stdout);
    }
    free(packages);
    free((void *)list);
    files_list_free(&found);
    text_free(&dir);
    text_free(&path);
    text_free(&own);
    text_free(&sources);
    text_free(&notice);
    arena_free(&arena);
    return ok ? 0 : 1;
}

bool licensing_project(const struct licensing_project *p, struct text *out)
{
    struct arena arena = {0};
    struct package archive[DRIVER_RUNTIME_PACKAGES];
    struct package *found =
        files_array(p->library_count + 1, sizeof *found);
    const struct package **list =
        files_array(p->library_count + DRIVER_RUNTIME_PACKAGES + 1,
                    sizeof *list);
    size_t archived =
        driver_runtime_packages(p->runtime, p->musl, &arena, archive);
    size_t n = 0;
    size_t i;
    bool ok = true;

    for (i = 0; i < archived; i++) {
        list[n++] = &archive[i];
    }
    /* DESIGN: the package of the project is the last `package` line,
       also where a library file of the same package came first, as in
       the notice of a binary. notice_text names every other package
       once. */
    for (i = 0; ok && i < p->library_count; i++) {
        const char *module;
        ok = deps_library_header(p->libraries[i], &arena, &found[i], &module);
        if (ok && (p->own->name == NULL ||
                   strcmp(found[i].name, p->own->name) != 0)) {
            list[n++] = &found[i];
        }
    }
    if (ok) {
        struct text notice = {0};
        list[n++] = p->own;
        antic_notice_lines(&notice, list, n);
        ok = symmap_license_sources(&notice, p->runtime);
        if (ok) {
            text_append_bytes(out, notice.data, notice.length);
        }
        text_free(&notice);
    }
    free(found);
    free((void *)list);
    arena_free(&arena);
    return ok;
}

/* The archive format that llvm-ar writes: the magic, then one header of
   AR_HEADER bytes before the bytes of each member, which start at an even
   offset. The name stands in the first AR_NAME bytes of a header and the
   size in decimal at AR_SIZE_AT. */
#define AR_MAGIC "!<arch>\n"
enum { AR_HEADER = 60, AR_NAME = 16, AR_SIZE_AT = 48, AR_SIZE = 10 };
/* The name of a member of the BSD form is `#1/<length>`, with the name
   itself in the first <length> bytes of the member. */
#define AR_BSD_NAME "#1/"

/* The decimal number in the length bytes at field, which spaces pad.
   Returns false for a field that holds no number. */
static bool ar_number(const char *field, size_t length, size_t *out)
{
    size_t value = 0;
    size_t i = 0;

    while (i < length && field[i] >= '0' && field[i] <= '9') {
        if (value > (SIZE_MAX - 9) / 10) {
            return false;
        }
        value = value * 10 + (size_t)(field[i] - '0');
        i++;
    }
    if (i == 0) {
        return false;
    }
    for (; i < length; i++) {
        if (field[i] != ' ') {
            return false;
        }
    }
    *out = value;
    return true;
}

/* One member of an archive: its name and its bytes. */
struct ar_member {
    const char *name;
    size_t name_length;
    const char *data;
    size_t size;
};

/* DESIGN: a member carries its name in one of three ways. A short name
   stands in the header, ended by `/` in the GNU and COFF forms and
   padded with spaces in the BSD form. A long name of the GNU and COFF
   forms is `/<offset>` into the member `//`, where it ends with `/` and
   a newline, or with a NUL. A long name of the BSD form stands before
   the bytes of the member. The index `/` and the table `//` are members
   without a name of their own. */
/* Read the member whose header stands at offset of the archive in bytes.
   names and names_size hold the member `//` once the walk passed it.
   Returns false when the bytes at offset are no member. */
static bool ar_member_at(const struct text *bytes, size_t offset,
                         const char *names, size_t names_size,
                         struct ar_member *out)
{
    const char *header = bytes->data + offset;
    size_t size;
    size_t length = AR_NAME;

    if (bytes->length - offset < AR_HEADER ||
        !ar_number(header + AR_SIZE_AT, AR_SIZE, &size) ||
        size > bytes->length - offset - AR_HEADER) {
        return false;
    }
    out->data = header + AR_HEADER;
    out->size = size;
    out->name = header;
    if (memcmp(header, AR_BSD_NAME, sizeof AR_BSD_NAME - 1) == 0) {
        size_t skip = sizeof AR_BSD_NAME - 1;
        if (!ar_number(header + skip, AR_NAME - skip, &length) ||
            length > size) {
            return false;
        }
        out->name = out->data;
        out->data += length;
        out->size -= length;
        while (length > 0 && out->name[length - 1] == '\0') {
            length--;
        }
    } else if (header[0] == '/' && header[1] >= '0' && header[1] <= '9') {
        size_t at;
        if (!ar_number(header + 1, AR_NAME - 1, &at) || at >= names_size) {
            return false;
        }
        out->name = names + at;
        length = 0;
        while (at + length < names_size && out->name[length] != '\n' &&
               out->name[length] != '\0') {
            length++;
        }
        if (length > 0 && out->name[length - 1] == '/') {
            length--;
        }
    } else {
        while (length > 0 && header[length - 1] == ' ') {
            length--;
        }
        if (header[0] != '/' && length > 1 && header[length - 1] == '/') {
            length--;
        }
    }
    out->name_length = length;
    return true;
}

/* Whether the member is the one of the package header copy: its name
   without the object suffix ends with LIBRARY_PACKAGE_SUFFIX. */
static bool is_package_member(const struct ar_member *m)
{
    size_t suffix = sizeof LIBRARY_PACKAGE_SUFFIX - 1;
    size_t stem = m->name_length;

    while (stem > 0 && m->name[stem - 1] != '.') {
        stem--;
    }
    if (stem == 0) {
        return false;
    }
    stem--;
    return stem >= suffix && memcmp(m->name + stem - suffix,
                                    LIBRARY_PACKAGE_SUFFIX, suffix) == 0;
}

/* The most places of one member where a package header is read. */
enum { PACKAGE_TRIES = 8 };

/* The package header that the bytes of a member hold: the object keeps
   the copy as one run of bytes in a section of its own, so it starts
   where the bytes of a library file start.

   DESIGN: an archive is input that anti did not write. A member that
   repeats the first bytes of a library file would have every one of its
   offsets read as a header, each into the same memory pool. A read that
   fails therefore goes into a pool of its own, which is freed, and a
   member is given up after PACKAGE_TRIES places. The object antic writes
   holds those bytes once, at the start of the copy. */
static bool member_package(const struct ar_member *m, struct arena *arena,
                           struct package *out)
{
    const uint8_t *data = (const uint8_t *)m->data;
    char message[256];
    const char *module;
    size_t tries = 0;
    size_t i;

    for (i = 0; i < m->size && tries < PACKAGE_TRIES; i++) {
        struct arena scratch = {0};
        struct package probe;
        bool found;
        if (!antic_library_starts(data + i, m->size - i)) {
            continue;
        }
        tries++;
        found = antic_library_header(data + i, m->size - i, &scratch, &probe,
                                     &module, message, sizeof message);
        arena_free(&scratch);
        if (found) {
            return antic_library_header(data + i, m->size - i, arena, out,
                                        &module, message, sizeof message);
        }
    }
    return false;
}

int licensing_from_archive(const char *archive)
{
    struct arena arena = {0};
    struct text bytes = {0};
    struct text notice = {0};
    struct package package;
    const struct package *list[1];
    const char *names = NULL;
    size_t names_size = 0;
    size_t magic = sizeof AR_MAGIC - 1;
    size_t offset = magic;
    bool found = false;

    if (!files_read_reported(archive, &bytes)) {
        return 1;
    }
    if (bytes.length < magic || memcmp(bytes.data, AR_MAGIC, magic) != 0) {
        fprintf(stderr, "anti: %s is no archive\n", archive);
        text_free(&bytes);
        return 1;
    }
    while (!found && offset < bytes.length) {
        struct ar_member m;
        if (!ar_member_at(&bytes, offset, names, names_size, &m)) {
            break;
        }
        if (m.name_length == 2 && memcmp(m.name, "//", 2) == 0) {
            names = m.data;
            names_size = m.size;
        } else if (is_package_member(&m)) {
            found = member_package(&m, &arena, &package);
        }
        offset = (size_t)(m.data - bytes.data) + m.size;
        offset += offset % 2;
    }
    if (found) {
        list[0] = &package;
        antic_notice_lines(&notice, list, 1);
        fputs(text_cstr(&notice), stdout);
    } else {
        fprintf(stderr, "anti: %s carries no package header, so it is no "
                        "static library of `--lib static`\n", archive);
    }
    text_free(&bytes);
    text_free(&notice);
    arena_free(&arena);
    return found ? 0 : 1;
}

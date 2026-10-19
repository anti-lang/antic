/* Derives the module path and the root-relative file name of a source
   path, and reads the segments of a module path. */

#include "modpath.h"

#include <string.h>

#include "lexer.h"
#include "platform.h"

static bool is_lower_identifier(const char *s, size_t n)
{
    size_t i;

    if (n == 0 || (s[0] >= '0' && s[0] <= '9')) {
        return false;
    }
    for (i = 0; i < n; i++) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) {
            return false;
        }
    }
    return true;
}

/* Whether c separates the parts of a path on the host: `/` on every
   host, and `\` as well on Windows, as platform_last_separator says. */
static bool separates(char c)
{
    return c == '/' || c == platform_separator();
}

/* The first separator of the n bytes at s, or NULL when they hold none. */
static const char *first_separator(const char *s, size_t n)
{
    size_t i;

    for (i = 0; i < n; i++) {
        if (separates(s[i])) {
            return s + i;
        }
    }
    return NULL;
}

/* The part of source under root, or NULL when root does not hold it. */
static const char *under(const char *source, const char *root)
{
    size_t n = strlen(root);

    while (n > 0 && separates(root[n - 1])) {
        n--;
    }
    if (n == 0 || strncmp(source, root, n) != 0 || !separates(source[n])) {
        return NULL;
    }
    return source + n + 1;
}

/* Append the absolute form of the directory of path, with its file name
   after it, to out. The directory is resolved and the name is not, so a
   source that is a link stays where it is written. Returns false when the
   directory cannot be resolved. */
static bool normal_file(const char *path, struct text *out)
{
    const char *slash = platform_last_separator(path);
    struct text directory = {0};
    bool ok;

    if (slash == NULL) {
        text_append(&directory, ".");
    } else if (slash == path || (slash == path + 2 && path[1] == ':')) {
        /* The root of the file system, or of a drive of Windows. */
        text_appendf(&directory, "%.*s", (int)(slash - path) + 1, path);
    } else {
        text_appendf(&directory, "%.*s", (int)(slash - path), path);
    }
    ok = platform_absolute_path(text_cstr(&directory), out);
    text_free(&directory);
    if (ok) {
        if (out->length == 0 || out->data[out->length - 1] != '/') {
            text_append(out, "/");
        }
        text_append(out, slash != NULL ? slash + 1 : path);
    }
    return ok;
}

/* DESIGN: by R147 of "Rules of 2026-10-13" the part of a source under a
   root is found after both are made absolute and normalised, so
   `-I . com/example/lib.anti` and `-I src ./src/com/x.anti` agree with
   the absolute forms. A root or a directory that cannot be resolved is
   compared as written. The part goes into the text part, and the pointer
   returned points into it, or NULL when no root holds the source. */
static const char *under_roots(const char *source, const char *const *roots,
                               size_t root_count, struct text *part)
{
    struct text file = {0};
    bool resolved = normal_file(source, &file);
    const char *rest = NULL;
    size_t i;

    for (i = 0; i < root_count && rest == NULL; i++) {
        struct text root = {0};
        if (resolved && platform_absolute_path(roots[i], &root)) {
            rest = under(text_cstr(&file), text_cstr(&root));
        }
        if (rest == NULL) {
            rest = under(source, roots[i]);
        }
        if (rest != NULL) {
            text_append(part, rest);
        }
        text_free(&root);
    }
    text_free(&file);
    return rest != NULL ? text_cstr(part) : NULL;
}

void modpath_file_of_source(const char *source, const char *const *roots,
                            size_t root_count, struct text *out)
{
    const char *slash;

    if (under_roots(source, roots, root_count, out) != NULL) {
        return;
    }
    slash = platform_last_separator(source);
    text_append(out, slash != NULL ? slash + 1 : source);
}

bool modpath_of_source(const char *source, const char *const *roots,
                           size_t root_count, struct text *out, char *error,
                           size_t error_size)
{
    struct text part = {0};
    const char *rest;
    const char *dot;
    const char *end;
    bool ok = true;

    modpath_file_of_source(source, roots, root_count, &part);
    rest = text_cstr(&part);
    dot = strrchr(rest, '.');
    end = dot != NULL && strcmp(dot, SOURCE_SUFFIX) == 0 ? dot
                                                         : rest + strlen(rest);
    /* The loop reads one name at least, so a file named `.anti` gives an
       empty name, which is refused, and never an empty module path. */
    do {
        const char *slash = first_separator(rest, (size_t)(end - rest));
        size_t n = slash != NULL ? (size_t)(slash - rest)
                                 : (size_t)(end - rest);
        if (!is_lower_identifier(rest, n)) {
            text_format(error, error_size,
                        "`%.*s` in %s is not a lowercase identifier", (int)n,
                        rest, source);
            ok = false;
        } else if (lexer_is_keyword(rest, n)) {
            text_format(error, error_size, "`%.*s` in %s is a keyword", (int)n,
                        rest, source);
            ok = false;
        } else {
            text_appendf(out, "%s%.*s", out->length > 0 ? "." : "", (int)n,
                         rest);
        }
        rest += n + 1;
    } while (ok && rest < end);
    text_free(&part);
    return ok;
}

bool modpath_reserved(const char *path)
{
    return strncmp(path, "anti", 4) == 0 && (path[4] == '\0' || path[4] == '.');
}

size_t modpath_segments(const char *path)
{
    size_t n = 1;

    for (; *path != '\0'; path++) {
        n += *path == '.';
    }
    return n;
}

const char *modpath_last(const char *path)
{
    const char *dot = strrchr(path, '.');
    return dot != NULL ? dot + 1 : path;
}

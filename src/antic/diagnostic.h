#ifndef ANTIC_DIAGNOSTIC_H
#define ANTIC_DIAGNOSTIC_H

#include <stdbool.h>
#include <stddef.h>

/* DESIGN: every warning and every safety check has a stable name, printed
   at the end of its message. The table of src/antic/warnings.c is the
   one place that spells the names. docs/notes/warnings.md lists them with their meaning and their
   fix, and the test `warning_names` holds the two lists equal. An error
   has no name, so `allow` and `unchecked` cannot reach one. */
enum diag_name {
    NAME_NONE,
    NAME_SHADOWED_CATCH,
    NAME_NEVER_FAILS,
    NAME_ASSERT_CALL,
    NAME_UNFILLED_ABSTRACT,
    NAME_ABOVE_VECTOR_CAP,
    NAME_SINGLE_SEGMENT_PATH,
    NAME_DOC_MARKUP,
    NAME_DOC_UNRESOLVED,
    NAME_DOC_NOTE_ONLY,
    NAME_UNDOCUMENTED,
    NAME_DOC_DROPPED,
    NAME_UNUSED_ALLOW,
    NAME_UNUSED_UNCHECKED,
    NAME_UNGUARDED_FIELD,
    NAME_EXPONENTIAL_PATTERN,
    NAME_COUNT
};

/* A compile error or warning at a position in the source. Lines and
   columns count from 1, and a column counts bytes. A warning does not
   stop the compilation. */
struct diagnostic {
    int line;
    int column;
    bool warning;
    /* A warning about documentation, which `anti check` counts as its own
       class. Only --doc-warnings produces one. */
    bool doc;
    /* A warning that --warnings-as-errors or a release build turned into
       an error. `anti check` still counts it as a warning. */
    bool promoted;
    enum diag_name name;            /* NAME_NONE for an error */
    char message[160];
};

/* The errors of one compilation, in the order they were found. A
   zero-initialised struct diagnostics is empty and valid. */
struct diagnostics {
    struct diagnostic *items;
    size_t count;
    size_t capacity;
};

void diagnostics_add(struct diagnostics *d, int line, int column,
                     const char *format, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 4, 5)))
#endif
    ;
void diagnostics_warn(struct diagnostics *d, enum diag_name name, int line,
                      int column, const char *format, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 5, 6)))
#endif
    ;
/* A safety check, an error with a name, which `unchecked` overrules. */
void diagnostics_check(struct diagnostics *d, enum diag_name name, int line,
                       int column, const char *format, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 5, 6)))
#endif
    ;
/* A warning about documentation, which belongs to the doc class of
   `anti check`. */
void diagnostics_doc(struct diagnostics *d, enum diag_name name, int line,
                     int column, const char *format, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 5, 6)))
#endif
    ;
void diagnostics_free(struct diagnostics *d);

#endif

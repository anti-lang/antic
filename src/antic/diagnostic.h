#ifndef ANTIC_DIAGNOSTIC_H
#define ANTIC_DIAGNOSTIC_H

/* The record of one diagnostic with its stable name, and the list of the
   diagnostics of one compilation. */

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

#include "attributes.h"

/* DESIGN: every warning and every safety check has a stable name, printed
   at the end of its message. The table of src/antic/warnings.c is the
   one place that spells the names. docs/notes/warnings.md lists them with their meaning and their
   fix, and the test `warning_names` holds the two lists equal. An error
   has no name, so `allow` and `unchecked` cannot reach one. */
enum diag_name {
    NAME_NONE,
    NAME_SHADOWED_CATCH,
    NAME_NEVER_FAILS,
    NAME_UNDO_NEVER_RUNS,
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
    NAME_NOTHING_EXPORTED,
    NAME_UNGUARDED_FIELD,
    NAME_EXPONENTIAL_PATTERN,
    NAME_ESCAPING_LOCAL,
    NAME_USE_AFTER_FREE,
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

/* The errors, warnings, safety checks and doc warnings of one
   compilation, in the order they were found. A zero-initialised struct
   diagnostics is empty and valid. */
struct diagnostics {
    struct diagnostic *items;
    size_t count;
    size_t capacity;
};

/* Append an error at line and column, with the message format and its
   arguments write. A message longer than the room of a diagnostic is cut
   and ends in three dots. The list grows as it needs, and a failed
   allocation ends the run with status 70. */
void diagnostics_add(struct diagnostics *d, int line, int column,
                     const char *format, ...)
    ATTRIBUTE_PRINTF(4, 5);
/* diagnostics_add with the arguments as a va_list, for a function that
   takes a format of its own and hands it on. */
void diagnostics_vadd(struct diagnostics *d, int line, int column,
                      const char *format, va_list args)
    ATTRIBUTE_PRINTF(4, 0);
/* diagnostics_check with the arguments as a va_list, as
   diagnostics_vadd. */
void diagnostics_vcheck(struct diagnostics *d, enum diag_name name, int line,
                        int column, const char *format, va_list args)
    ATTRIBUTE_PRINTF(5, 0);
/* Append a warning with its stable name, which `allow` silences and
   --warnings-as-errors turns into an error. The message and the memory
   follow diagnostics_add. */
void diagnostics_warn(struct diagnostics *d, enum diag_name name, int line,
                      int column, const char *format, ...)
    ATTRIBUTE_PRINTF(5, 6);
/* A safety check, an error with a name, which `unchecked` overrules. */
void diagnostics_check(struct diagnostics *d, enum diag_name name, int line,
                       int column, const char *format, ...)
    ATTRIBUTE_PRINTF(5, 6);
/* A warning about documentation, which belongs to the doc class of
   `anti check`. */
void diagnostics_doc(struct diagnostics *d, enum diag_name name, int line,
                     int column, const char *format, ...)
    ATTRIBUTE_PRINTF(5, 6);
/* Free the items of d and leave it empty and valid, as a
   zero-initialised list is. The struct itself belongs to the caller. */
void diagnostics_free(struct diagnostics *d);

/* Print each diagnostic of d on standard error, one line each, as
   `path:line:column: kind: message`, with ` [name]` after a message that
   has a name. It is the one printer of a message at a position of a
   source, which the test `message_forms` holds. */
void diagnostics_print(const char *path, const struct diagnostics *d);

#endif

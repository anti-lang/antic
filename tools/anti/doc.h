#ifndef ANTI_DOC_H
#define ANTI_DOC_H

#include <stdbool.h>
#include <stddef.h>

/* What a run writes. HTML is the form of the reference on anti-lang.com
   and Markdown the form a Hugo site mounts. */
enum doc_form { DOC_HTML, DOC_MARKDOWN };

/* `anti doc`: one page per module and an index of them, into out. Each
   source is a `.anti` file or a `.antl` file. dev builds the developer's
   docs, which need the source and carry the private items and the `//#`
   notes. private keeps the private items in the user docs. roots are the
   search roots of the module paths, work takes the interface file of
   every source, and runtime holds the library files of the runtime
   archive. Returns the exit status of the command. */
int doc_run(const char **sources, size_t count, const char **roots,
            size_t root_count, const char *out, const char *work,
            const char *runtime, enum doc_form form, bool dev,
            bool private_items);

#endif

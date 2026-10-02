#ifndef ANTI_DOC_H
#define ANTI_DOC_H

#include <stdbool.h>
#include <stddef.h>

/* What a run writes. HTML is the form of the reference on anti-lang.com
   and Markdown the form a Hugo site mounts. */
enum doc_form { DOC_HTML, DOC_MARKDOWN };

/* One run of `anti doc`. Each source is a `.anti` file or a `.antl`
   file. roots are the search roots of the module paths, out takes the
   pages, work takes the interface file of every source, package is the
   package name of the manifest or NULL outside a project, and runtime
   holds the library files of the runtime archive. dev builds the
   developer's docs, which need the source and carry the private items
   and the `//#` notes. private_items keeps the private items in the user
   docs. */
struct doc_request {
    const char *const *sources;
    size_t source_count;
    const char *const *roots;
    size_t root_count;
    const char *out;
    const char *work;
    const char *package;
    const char *runtime;
    enum doc_form form;
    bool dev;
    bool private_items;
};

/* `anti doc`: one page per module of r and an index of them, into its
   out. Returns the exit status of the command. */
int doc_run(const struct doc_request *r);

#endif

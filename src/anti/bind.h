#ifndef ANTI_BIND_H
#define ANTI_BIND_H

#include <stdbool.h>
#include <stddef.h>

/* `anti bind --header <name>.antl`: write `<name>.h` into out_dir from
   the public interface of the library file, as `antic --lib` writes it
   for the same module. roots are the search roots of the modules it
   imports, and runtime holds the library files of the runtime archive.
   Returns the exit status of the command. */
int bind_header(const char *library, const char *out_dir,
                const char *runtime, const char **roots, size_t root_count);

/* `anti bind <api.json>` and `anti bind --clang <header>`. */
struct bind_request {
    const char *input;          /* the API description or the header */
    bool clang;                 /* input is a header */
    const char *module;         /* NULL: anti.<name of the input> */
    const char *out_dir;
    const char *runtime;
    const char *target;         /* NULL: the host */
    const char **includes;
    size_t include_count;
    const char *const *defines;
    size_t define_count;
    bool probe;                 /* write probe_<library>.c and .anti too */
};

/* Write <library>.anti into out_dir, with shim_<library>.c when a
   function of the header is inline. Returns the exit status of the
   command. */
int bind_run(const struct bind_request *request);

#endif

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

#endif

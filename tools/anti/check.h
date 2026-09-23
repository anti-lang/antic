#ifndef ANTI_CHECK_H
#define ANTI_CHECK_H

#include <stdbool.h>
#include <stddef.h>

/* Run every check that writes no artifact of a program, in the order of
   "Check command" in docs/tooling-addendum.md, and report one line per
   class. The first failing class ends the run.

   sources are the files to check, and none means every `.anti` file under
   the source and test directories of the manifest. roots are the -I search
   roots, which also give each module its path. work holds the interface
   files and the modules of the doc blocks. runtime names the runtime
   archive. undocumented reports every `pub` item without a `///` comment,
   and all_targets runs the front end once per target instead of once for
   the host. Returns the exit status of anti. */
int check_run(const char *const *sources, size_t source_count,
              const char *const *roots, size_t root_count, const char *work,
              const char *runtime, bool undocumented, bool all_targets);

#endif

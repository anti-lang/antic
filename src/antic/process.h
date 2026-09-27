#ifndef ANTIC_PROCESS_H
#define ANTIC_PROCESS_H

#include "text.h"

/* Run the program argv[0], found through PATH when it contains no '/',
   with the NULL-terminated argument list argv. Return its exit status, or
   -1 when it could not start or ended by a signal. The program inherits
   standard output and standard error. */
int process_run(const char *const argv[]);

/* Run a program as process_run does, with directory as its working
   directory, where every relative path of its arguments then starts. A
   relative argv[0] with a separator names the program from directory as
   well, and a bare name is found through PATH. */
int process_run_in(const char *directory, const char *const argv[]);

/* Run a program as process_run does and append its standard output to
   out. */
int process_capture(const char *const argv[], struct text *out);

#if !defined(_WIN32)
/* Run a program as process_run does, with the environment variable name
   set to value, and hand each line of its standard error to each, with
   its newline. A last line without one comes last. Standard output stays
   the one of antic. */
int process_run_lines(const char *const argv[], const char *name,
                      const char *value,
                      void (*each)(void *context, const char *line,
                                   size_t length),
                      void *context);
#endif

#endif

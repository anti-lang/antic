#ifndef ANTI_MEMREPORT_H
#define ANTI_MEMREPORT_H

/* Run the program argv of --memory-checks as `anti run` and `anti test`
   do and return its exit status. The program runs with the symbolizing
   of AddressSanitizer off, and each frame of its report comes out with
   the function, the file and the line that Anti's symbolizer reads from
   its module. */
int memreport_run(const char *const argv[]);

#endif

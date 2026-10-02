/* The bound and the message of the includes of a runtime configuration,
   shared by the runtime and `anti symbols`.

   DESIGN: the runtime and `anti symbols` keep their own walks of the
   includes, and no code is shared across the boundary between the
   runtime and the tool. The two walks agree on what this header holds:
   how deep a file may stand and the message that a cycle or a deeper
   file gives. Eddie decided it on 2026-09-30, for audit finding M25.
   They also agree on how many files one configuration may read, and
   on the message past it. */
#ifndef ANTI_RT_CONF_INCLUDE_H
#define ANTI_RT_CONF_INCLUDE_H

/* The most files that may stand above one file of the configuration. A
   file below more of them counts as a cycle. */
#define ANTI_CONF_INCLUDE_DEPTH 32

/* The message of a cycle and of a file below more than
   ANTI_CONF_INCLUDE_DEPTH others, with the path of the file. */
#define ANTI_CONF_INCLUDE_CYCLE \
    "the configuration files include one another at %s"

/* The most files one configuration may read, the file it starts at
   included. A file read again counts again, so includes that fan out,
   each file naming the next ten times, end here rather than read 10^32
   files. */
#define ANTI_CONF_INCLUDE_FILES 256

/* The message of the file read past ANTI_CONF_INCLUDE_FILES, with that
   count and the path of the file. */
#define ANTI_CONF_INCLUDE_MANY \
    "the configuration reads more than %d files, the last at %s"

#endif

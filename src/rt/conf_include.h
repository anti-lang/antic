/* The bound and the message of the includes of a runtime configuration,
   shared by the runtime and `anti symbols`.

   DESIGN: the runtime and `anti symbols` keep their own walks of the
   includes, and no code is shared across the boundary between the
   runtime and the tool. The two walks agree on what this header holds:
   how deep a file may stand and the message that a cycle or a deeper
   file gives. Eddie decided it on 2026-09-30, for audit finding M25. */
#ifndef ANTI_RT_CONF_INCLUDE_H
#define ANTI_RT_CONF_INCLUDE_H

/* The most files that may stand above one file of the configuration. A
   file below more of them counts as a cycle. */
#define ANTI_CONF_INCLUDE_DEPTH 32

/* The message of a cycle and of a file below more than
   ANTI_CONF_INCLUDE_DEPTH others, with the path of the file. */
#define ANTI_CONF_INCLUDE_CYCLE \
    "the configuration files include one another at %s"

#endif

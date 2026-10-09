/* The entry point of a Windows program. See platform.h.

   DESIGN: the linker names mainCRTStartup, and the static C runtime of
   Microsoft gave it before. It stands in an object of its own, which only
   the link of a program pulls in. In platform_windows.c every link would
   pull it in, and it names main, which start.c defines for a program of
   Anti and which names the program: a DLL would then name a program. In
   start.c beside main, a program of C, which defines main itself and
   links the runtime, would get a second main. This file holds the one
   #if on a system outside the files named platform_<system>.c, as rule 22
   of docs/c-guidelines.md says. */
#if defined(_WIN32)

#include "platform.h"

int main(int argc, char **argv);

int mainCRTStartup(void);
int mainCRTStartup(void)
{
    anti_rt_windows_start(main);
}

#else

/* ISO C wants a declaration in every file, and elsewhere this one holds
   no other. */
typedef int anti_rt_platform_entry_unused;

#endif

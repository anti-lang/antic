/* Facts about the platform the program runs on, which the standard
   library asks rather than guessing from the environment. */
#include <stdint.h>

/* DESIGN: anti.os builds the per-user directories, and the conventions
   differ between Windows and the two Unix platforms. The runtime is
   compiled once per target, so the answer is a constant of the build.
   Reading it from the environment instead would take the layout of
   Windows on any machine that happens to carry LOCALAPPDATA. */
int64_t anti_rt_is_windows(void)
{
#ifdef _WIN32
    return 1;
#else
    return 0;
#endif
}

/* A C program that loads a shared Anti library at run time. The first call
   finds the runtime initialised by the library's constructor. Windows
   loads a DLL with LoadLibrary and GetProcAddress, and every other host
   loads with dlopen and dlsym. */
#include "../binary_stdio.h"
#include <stdio.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

int main(int argc, char **argv)
{
    int (*ready)(void);
#if defined(_WIN32)
    HMODULE library;

    if (argc < 2 || (library = LoadLibraryA(argv[1])) == NULL) {
        fprintf(stderr, "cannot load the library\n");
        return 2;
    }
    ready = (int (*)(void))(void (*)(void))GetProcAddress(library,
                                                          "geo_ready");
#else
    void *library;

    if (argc < 2 || (library = dlopen(argv[1], RTLD_NOW)) == NULL) {
        fprintf(stderr, "cannot load the library\n");
        return 2;
    }
    ready = (int (*)(void))dlsym(library, "geo_ready");
#endif
    if (ready == NULL) {
        fprintf(stderr, "no geo_ready\n");
        return 3;
    }
    printf("%d\n", ready());
#if defined(_WIN32)
    return FreeLibrary(library) ? 0 : 4;
#else
    return dlclose(library);
#endif
}

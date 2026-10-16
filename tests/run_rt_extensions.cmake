# Rule 1 of docs/c-guidelines.md for the runtime: a compiler extension
# stands only in a file that exists to hold one. That is the platform
# layer, src/rt/platform.h with platform_posix.c, platform_windows.c and
# platform_stdio.c, and src/rt/atomic.c, which holds the atomic
# operations of every compiler. Every other file of src/rt is C11 alone. The spellings below
# are those of the extensions of clang, gcc and MSVC the runtime has
# used. Decision 14 of "Decisions of 2026-10-11" has every host build the
# runtime with the pinned clang, so no file of src/rt holds a form of
# Microsoft's compiler, the files that hold an extension included: its
# version macro, its Interlocked intrinsics with their header, and its
# names of the architecture, which clang defines beside its own. Run with
# cmake -P and ROOT, the root of the repository.

cmake_minimum_required(VERSION 3.21)

set(holders "platform.h" "platform_posix.c" "platform_windows.c"
    "platform_stdio.c" "atomic.c")
set(extensions "__attribute__|__asm__|__asm[ (]|__atomic_|__sync_|__builtin_|__declspec|__int128|__typeof__|__extension__|_Interlocked|__iso_volatile|__cpuid|_xgetbv|__dmb|^[ ]*#[ ]*pragma")
set(microsoft "_MSC_VER|_Interlocked|<intrin[.]h>|_M_X64|_M_ARM64")
set(failures "")

set(src "${ROOT}/src/rt")
file(GLOB sources RELATIVE "${src}" "${src}/*.c" "${src}/*.h")
foreach(source IN LISTS sources)
    # file(STRINGS) drops empty lines, so a finding names the line by its
    # text rather than by a number.
    file(STRINGS "${src}/${source}" lines)
    foreach(line IN LISTS lines)
        string(STRIP "${line}" shown)
        if(line MATCHES "(${microsoft})")
            string(APPEND failures
                "\nsrc/rt/${source}: a form of Microsoft's compiler: ${shown}")
        endif()
        if(NOT source IN_LIST holders AND line MATCHES "(${extensions})")
            string(APPEND failures "\nsrc/rt/${source}: ${shown}")
        endif()
    endforeach()
endforeach()

if(NOT failures STREQUAL "")
    message(FATAL_ERROR "a compiler extension outside the files that hold one, or a form of Microsoft's compiler:${failures}")
endif()

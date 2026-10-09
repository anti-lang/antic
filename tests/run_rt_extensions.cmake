# Rule 1 of docs/c-guidelines.md for the runtime: a compiler extension
# stands only in a file that exists to hold one. That is the platform
# layer, src/rt/platform.h with platform_posix.c, platform_windows.c and
# platform_stdio.c, and src/rt/atomic.c, which holds the atomic
# operations of every compiler. Every other file of src/rt is C11 alone. The spellings below
# are those of the extensions of clang, gcc and MSVC the runtime has
# used. Run with cmake -P and ROOT, the root of the repository.

cmake_minimum_required(VERSION 3.21)

set(holders "platform.h" "platform_posix.c" "platform_windows.c"
    "platform_stdio.c" "atomic.c")
set(extensions "__attribute__|__asm__|__asm[ (]|__atomic_|__sync_|__builtin_|__declspec|__int128|__typeof__|__extension__|_Interlocked|__iso_volatile|__cpuid|_xgetbv|__dmb|^[ ]*#[ ]*pragma")
set(failures "")

set(src "${ROOT}/src/rt")
file(GLOB sources RELATIVE "${src}" "${src}/*.c" "${src}/*.h")
foreach(source IN LISTS sources)
    if(source IN_LIST holders)
        continue()
    endif()
    # file(STRINGS) drops empty lines, so a finding names the line by its
    # text rather than by a number.
    file(STRINGS "${src}/${source}" lines)
    foreach(line IN LISTS lines)
        if(line MATCHES "(${extensions})")
            string(STRIP "${line}" shown)
            string(APPEND failures "\nsrc/rt/${source}: ${shown}")
        endif()
    endforeach()
endforeach()

if(NOT failures STREQUAL "")
    message(FATAL_ERROR "a compiler extension outside the files that hold one:${failures}")
endif()

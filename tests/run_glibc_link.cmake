# Link the C probe of a native library for a Linux target against the
# glibc sysroot, with the pinned clang and lld, as a program that imports
# anti.raylib or anti.miniaudio links on Linux. A symbol that neither the
# library nor the system libraries define fails the link. On a host of
# that target the program runs and its output is compared with the
# expected file. Run with cmake -P and these values:
#   CLANG     the pinned clang
#   LLD       ld.lld of the pinned LLVM tools
#   TRIPLE    the target triple
#   SYSROOT   the glibc sysroot of the target
#   LIBDIR    the directory of its C library, under the multiarch name
#   OBJECT    the object of the probe, which holds main
#   LIBRARY   the static libraries of the runtime tree, separated by
#             commas
#   LIBS      the system libraries, separated by commas, without -l
#   EXPECTED  the expected file: "exit N" and then the standard output
#   HOST      the target of this host
#   TARGET    the target name
#   WORK      a directory for the executable

file(MAKE_DIRECTORY "${WORK}")
set(exe "${WORK}/probe-${TARGET}")
string(REPLACE "," ";" libraries "${LIBRARY}")
string(REPLACE "," ";" libs "${LIBS}")
set(flags "")
foreach(lib IN LISTS libs)
    list(APPEND flags "-l${lib}")
endforeach()
file(REMOVE "${exe}")
# DESIGN: -nostdlib names every start file and library itself. The pinned
# clang ships no builtins and no crtbegin.o for the gnu triples, and
# nothing here needs either: the objects are C, and glibc holds what they
# call.
execute_process(
    COMMAND "${CLANG}" "--target=${TRIPLE}" "--sysroot=${SYSROOT}"
            -fuse-ld=lld "--ld-path=${LLD}" -nostdlib -pie
            "${LIBDIR}/Scrt1.o" "${LIBDIR}/crti.o" "${OBJECT}" ${libraries}
            ${flags} -lc "${LIBDIR}/crtn.o" -o "${exe}"
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
    ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "the link failed for ${TARGET}\n${out}${err}")
endif()
# Warnings are errors: clang and lld print nothing.
if(NOT out STREQUAL "" OR NOT err STREQUAL "")
    message(FATAL_ERROR "the link printed output for ${TARGET}\n${out}${err}")
endif()
if(NOT EXISTS "${exe}")
    message(FATAL_ERROR "the link wrote no executable for ${TARGET}")
endif()

# The program loads glibc through its dynamic loader and names libc.so.6.
set(loader "")
foreach(name ld-linux-x86-64.so.2 ld-linux-aarch64.so.1)
    file(STRINGS "${exe}" found REGEX "^/lib(64)?/${name}$" LIMIT_COUNT 1)
    if(found)
        set(loader "${found}")
    endif()
endforeach()
if(loader STREQUAL "")
    message(FATAL_ERROR "${exe} names no dynamic loader of glibc")
endif()
file(STRINGS "${exe}" needed REGEX "^libc\\.so\\.6$" LIMIT_COUNT 1)
if(NOT needed)
    message(FATAL_ERROR "${exe} does not name libc.so.6")
endif()

if(NOT HOST STREQUAL TARGET)
    return()
endif()
execute_process(COMMAND "${exe}" RESULT_VARIABLE status
                OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
file(READ "${EXPECTED}" expected)
if(NOT expected MATCHES "^exit ([0-9]+)\n(.*)$")
    message(FATAL_ERROR "${EXPECTED} does not start with an exit line")
endif()
set(want_status "${CMAKE_MATCH_1}")
set(want_out "${CMAKE_MATCH_2}")
if(NOT status STREQUAL want_status OR NOT out STREQUAL want_out)
    message(FATAL_ERROR "${exe} exited ${status} and wrote\n${out}${err}\n"
                        "expected exit ${want_status} and\n${want_out}")
endif()

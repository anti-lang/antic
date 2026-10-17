# The package of this host holds what a package holds: antic and anti
# with the LLVM tools of the host in bin/, the sysroots of every target,
# every licence and every header of the runtime tree, and a runtime with
# its bitcode of full LTO for every target and level. It holds no tools/,
# no stubs of Apple's SDK and no bitcode of ThinLTO.
#
#   cmake -DROOT=<repository> -DHOST=<host> -DRUNTIME=<dir>
#         -DPACKAGE=<dir of package_pack> -DWORK=<dir>
#         -P tests/runners/run_package_contents.cmake

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../package_tree.cmake")

# DESIGN: the LLVM tools travel in bin/ of the package beside antic and
# anti, copied from bin/ of the runtime archive with llvm-version, which
# Eddie decided on 2026-10-08 in docs/work-order-distribution.md. The
# installer downloads none and runs no script of the package, so the
# package holds no tools/. VERSION in the root names the version of the
# package.
include("${ROOT}/tools/pinned-tools.cmake")
antic_llvm_archive_tools(tools)
set(expected anti/bin/antic${suffix}
             anti/bin/anti${suffix} anti/bin/llvm-version anti/VERSION
             anti/sysroot/macos-arm64/usr/lib/libSystem.tbd
             anti/sysroot/macos-x86_64/usr/lib/libSystem.tbd
             anti/sysroot/macos-arm64/sdk-version
             anti/licenses/zig.txt anti/licenses/apsl.txt
             anti/licenses/sources.txt anti/licenses/mingw-w64.txt)
# DESIGN: every package carries the two Windows sysroots of mingw-w64 as
# the runtime archive holds them: the headers, the import libraries that
# llvm-dlltool wrote from the .def files and the builtins of the pinned
# clang, which Eddie decided on 2026-09-27 under "Binary distribution" in
# docs/decisions.md and the step mingw of docs/work-order-distribution.md
# built.
foreach(cpu x86_64 arm64)
    set(windows "anti/sysroot/windows-${cpu}")
    list(APPEND expected "${windows}/include/_mingw.h"
         "${windows}/include/windows.h" "${windows}/lib/ucrtbase.lib"
         "${windows}/lib/kernel32.lib" "${windows}/lib/ntdll.lib"
         "${windows}/lib/clang_rt.builtins.lib")
endforeach()
foreach(tool IN LISTS tools)
    list(APPEND expected "anti/bin/${tool}${suffix}")
endforeach()
# DESIGN: every package carries the two glibc sysroots of the runtime
# archive as they are, with their X11 and OpenGL development files, and
# the glibc runtime of both Linux targets, which Eddie decided on
# 2026-09-27 under "Binary distribution" in docs/decisions.md.
foreach(cpu x86_64 arm64)
    set(multiarch x86_64-linux-gnu)
    set(level v1)
    if(cpu STREQUAL "arm64")
        set(multiarch aarch64-linux-gnu)
        set(level armv8.0)
    endif()
    set(glibc "anti/sysroot/linux-${cpu}-glibc")
    list(APPEND expected "${glibc}/lib/${multiarch}/libc.so.6"
         "${glibc}/usr/lib/${multiarch}/Scrt1.o"
         "${glibc}/usr/lib/${multiarch}/libX11.so"
         "${glibc}/usr/lib/${multiarch}/libGL.so"
         "${glibc}/usr/include/X11/Xlib.h" "${glibc}/usr/include/GL/gl.h"
         "anti/lib/linux-${cpu}-glibc/${level}/libanti_rt.a"
         "anti/lib/linux-${cpu}-glibc/libunwind.a")
endforeach()
# DESIGN: the headers of the native libraries go into include/<library>/
# of the runtime archive and the package, which Eddie decided on
# 2026-10-08 in docs/work-order-distribution.md. Each directory is the one
# a C compile names with -I, so Mbed TLS keeps mbedtls/ and psa/ below it.
foreach(header pcre2/pcre2.h sqlite3/sqlite3.h sqlite3/sqlite3ext.h
        mbedtls/mbedtls/ssl.h mbedtls/mbedtls/mbedtls_config.h
        mbedtls/psa/crypto.h miniaudio/miniaudio.h raylib/raylib.h
        raylib/raymath.h raylib/rlgl.h raylib/rcamera.h)
    list(APPEND expected "anti/include/${header}")
endforeach()
foreach(entry IN LISTS expected)
    if(NOT package_entries MATCHES "(^|\n)${entry}\n")
        message(FATAL_ERROR "${package_archive} lacks ${entry}")
    endif()
endforeach()
# DESIGN: the package holds no tools/, since the installers download the
# package and run nothing of it, which Eddie decided on 2026-10-08 in
# decision 7 of docs/work-order-distribution.md. The LLVM tools travel in
# bin/, and the pins and the scripts of the sysroot stay in the tree.
if(package_entries MATCHES "(^|\n)anti/tools/")
    message(FATAL_ERROR "${package_archive} carries tools/, and no installer "
                        "runs a script or reads a pin of the package")
endif()
# Every licence of the runtime tree goes in, those of the native libraries
# among them.
file(GLOB licences RELATIVE "${RUNTIME}/licenses" "${RUNTIME}/licenses/*.txt")
foreach(licence IN LISTS licences)
    string(REPLACE "." "\\." pattern "${licence}")
    if(NOT package_entries MATCHES "(^|\n)anti/licenses/${pattern}\n")
        message(FATAL_ERROR "${package_archive} lacks anti/licenses/${licence}")
    endif()
endforeach()
# Every header of the runtime tree goes in.
file(GLOB_RECURSE headers RELATIVE "${RUNTIME}/include" "${RUNTIME}/include/*")
if(headers STREQUAL "")
    message(FATAL_ERROR "${RUNTIME}/include holds no header")
endif()
foreach(header IN LISTS headers)
    string(REPLACE "." "\\." pattern "${header}")
    if(NOT package_entries MATCHES "(^|\n)anti/include/${pattern}\n")
        message(FATAL_ERROR "${package_archive} lacks anti/include/${header}")
    endif()
endforeach()
if(package_entries MATCHES "(^|\n)anti/sysroot/macos-[^/\n]+/sdk/")
    message(FATAL_ERROR "${package_archive} carries stubs of Apple's SDK")
endif()
# DESIGN: a release build links the runtime as bitcode through full LTO
# by default, which Eddie decided on 2026-10-07, so every runtime of the
# package has its bitcode of full LTO beside it. The bitcode of ThinLTO
# stays out, and `--lto thin` then names the archive it lacks.
string(REGEX MATCHALL "anti/lib/[^/\n]+/[^/\n]+/(lib)?anti_rt\\.(a|lib)\n"
       runtimes "${package_entries}")
if(runtimes STREQUAL "")
    message(FATAL_ERROR "${package_archive} carries no runtime")
endif()
foreach(runtime IN LISTS runtimes)
    string(STRIP "${runtime}" runtime)
    get_filename_component(level "${runtime}" DIRECTORY)
    get_filename_component(library "${runtime}" NAME)
    string(REPLACE "." "\\." pattern "${level}/bitcode/full/${library}")
    if(NOT package_entries MATCHES "(^|\n)${pattern}\n")
        message(FATAL_ERROR "${package_archive} lacks "
                            "${level}/bitcode/full/${library}")
    endif()
endforeach()
if(package_entries MATCHES "(^|\n)anti/lib/[^\n]*/bitcode/thin/")
    message(FATAL_ERROR "${package_archive} carries the bitcode of ThinLTO")
endif()

file(REMOVE_RECURSE "${WORK}")

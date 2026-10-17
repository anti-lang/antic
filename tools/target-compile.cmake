# The compile of C for every target: the triple clang compiles it for and
# the headers of its sysroot. One definition serves the cross builds of
# the runtime and of the native libraries, the packer, tools/get-sysroot.cmake
# and the C of the tests. tools/windows-compile.cmake holds the Windows
# half, which this file includes, and the test target_triples refuses a
# triple of a target spelled anywhere else.

include("${CMAKE_CURRENT_LIST_DIR}/windows-compile.cmake")

# The triple the C of <target> is compiled for. <target> is a target of
# antic or the glibc mode of a Linux target, linux-<cpu>-glibc. The triple
# of a Linux target names the vendor, so it is also the directory of the
# runtimes of the target in the resource directory of clang.
#
# The minimum macOS version of the triple is 11.0, the value of
# MACOS_MIN_MAJOR and MACOS_MIN_MINOR in src/antic/target.h.
function(antic_target_triple out target)
    if(target MATCHES "^windows-")
        antic_windows_triple(triple "${target}")
        set(${out} "${triple}" PARENT_SCOPE)
        return()
    endif()
    set(triples
        "linux-x86_64=x86_64-unknown-linux-musl"
        "linux-arm64=aarch64-unknown-linux-musl"
        "linux-x86_64-glibc=x86_64-unknown-linux-gnu"
        "linux-arm64-glibc=aarch64-unknown-linux-gnu"
        "macos-arm64=arm64-apple-macos11"
        "macos-x86_64=x86_64-apple-macos11")
    foreach(row IN LISTS triples)
        if(row MATCHES "^${target}=(.*)$")
            set(${out} "${CMAKE_MATCH_1}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    message(FATAL_ERROR "${target} is no target")
endfunction()

# The options of a compile of C for <target> against <sysroot>, the
# directory of the target among the sysroots, after --target=. <resource>
# is the resource directory of the pinned clang. Linux takes -nostdinc and
# the headers of its C library before those of clang, as the driver of
# clang orders them for musl. The glibc mode reads the headers of the
# multiarch directory of Debian first. macOS takes the sysroot as it is,
# and Windows the options of tools/windows-compile.cmake. Each -isystem is
# one word with its directory, since CMake drops a repeated word from the
# compile options of a target.
function(antic_target_c_options out target sysroot resource)
    if(target MATCHES "^linux-(x86_64|arm64)-glibc$")
        # DESIGN: the runtime of the glibc mode compiles against the
        # headers of glibc 2.35 on every host, the Linux one as well, so
        # it names no symbol version above the sysroot's.
        string(REPLACE "arm64" "aarch64" arch "${CMAKE_MATCH_1}")
        set(options -fPIC -nostdinc
            "-isystem${sysroot}/usr/include/${arch}-linux-gnu"
            "-isystem${sysroot}/usr/include" "-isystem${resource}/include")
    elseif(target MATCHES "^linux-")
        set(options -fPIC -nostdinc "-isystem${sysroot}/usr/include"
            "-isystem${resource}/include")
    elseif(target MATCHES "^macos-")
        set(options -isysroot "${sysroot}")
    else()
        antic_windows_compile_options(options "${sysroot}" "${resource}")
    endif()
    set(${out} "${options}" PARENT_SCOPE)
endfunction()

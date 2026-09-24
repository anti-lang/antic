# The targets and the compile flags of the two media libraries, miniaudio
# and raylib, which src/native/miniaudio.cmake and src/native/raylib.cmake
# build.
#
# DESIGN: a program that imports anti.raylib or anti.miniaudio links
# dynamically against glibc 2.35 on Linux, as "Runtime archive" in
# docs/decisions.md settles, so both Linux libraries compile against the
# glibc sysroot and not against musl. tools/get-sysroot.cmake installs
# that sysroot with the X11 and OpenGL development files of
# tools/sysroot-pins, and the build copies it to sysroot/ of the runtime
# tree. A Linux target whose glibc sysroot is missing builds neither
# library, as the other native libraries skip a target without a sysroot.
# On macOS both libraries compile against the Apple SDK that
# tools/macos-sdk-pin names, since the zig stubs of the sysroot carry no
# framework headers. A host without that SDK builds no macOS library of
# the two.

execute_process(COMMAND "${CMAKE_COMMAND}"
                        "-DPIN=${PROJECT_SOURCE_DIR}/tools/macos-sdk-pin"
                        -P "${PROJECT_SOURCE_DIR}/tools/macos-sdk.cmake"
                OUTPUT_VARIABLE ANTIC_MEDIA_APPLE_SDK
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_VARIABLE antic_media_sdk_error
                RESULT_VARIABLE antic_media_sdk_failed)
if(antic_media_sdk_failed)
    set(ANTIC_MEDIA_APPLE_SDK "")
endif()

set(ANTIC_MEDIA_TARGETS "")
foreach(target IN LISTS ANTIC_TARGETS)
    if(target MATCHES "^linux-")
        set(sysroot "${ANTIC_RUNTIME_DIR}/sysroot/${target}-glibc")
        if(NOT EXISTS "${sysroot}")
            message(STATUS "raylib and miniaudio for ${target}: skipped, "
                           "there is no glibc sysroot")
        elseif(NOT EXISTS "${sysroot}/usr/include/X11/Xlib.h")
            message(FATAL_ERROR "${sysroot} holds no X11 headers. Run "
                                "tools/get-sysroot.cmake for ${target}-glibc "
                                "again.")
        else()
            list(APPEND ANTIC_MEDIA_TARGETS "${target}")
        endif()
    elseif(NOT target IN_LIST ANTIC_NATIVE_TARGETS)
        # antic links the test of the library, which needs the sysroot.
    elseif(target MATCHES "^macos-")
        if(ANTIC_MEDIA_APPLE_SDK STREQUAL "")
            message(STATUS "raylib and miniaudio for ${target}: skipped, "
                           "tools/macos-sdk-pin names an SDK this host lacks")
        else()
            list(APPEND ANTIC_MEDIA_TARGETS "${target}")
        endif()
    else()
        list(APPEND ANTIC_MEDIA_TARGETS "${target}")
    endif()
endforeach()

# The glibc sysroot of a Linux target, and the directory of its libraries
# under the multiarch name of Debian.
function(antic_media_glibc out_sysroot out_libdir target)
    set(sysroot "${ANTIC_RUNTIME_DIR}/sysroot/${target}-glibc")
    if(target STREQUAL "linux-x86_64")
        set(multiarch x86_64-linux-gnu)
    else()
        set(multiarch aarch64-linux-gnu)
    endif()
    set(${out_sysroot} "${sysroot}" PARENT_SCOPE)
    set(${out_libdir} "${sysroot}/usr/lib/${multiarch}" PARENT_SCOPE)
endfunction()

# The target triple and the compile flags of a media library for <target>.
# Windows takes the flags of every native library.
function(antic_media_target out_triple out_flags target)
    antic_cpu_level(level "${target}")
    antic_cpu_march(march "${level}")
    string(REGEX REPLACE "-arm64$" "-aarch64" arch "${target}")
    string(REGEX REPLACE "^[a-z]+-" "" arch "${arch}")
    if(target MATCHES "^linux-")
        antic_media_glibc(sysroot libdir "${target}")
        set(triple "${arch}-linux-gnu")
        set(flags -fPIC "--sysroot=${sysroot}" "${march}")
    elseif(target MATCHES "^macos-")
        # The minimum macOS version is the one of antic_native_target.
        string(REPLACE "aarch64" "arm64" arch "${arch}")
        set(triple "${arch}-apple-macos11")
        set(flags -isysroot "${ANTIC_MEDIA_APPLE_SDK}" "${march}")
    else()
        antic_native_target(triple flags "${target}")
    endif()
    set(${out_triple} "${triple}" PARENT_SCOPE)
    set(${out_flags} "${flags}" PARENT_SCOPE)
endfunction()

# The link test of a media library on Linux: the object of the probe,
# compiled with its own main, linked by the pinned clang and lld against
# the glibc sysroot, as tests/run_glibc_link.cmake describes. <libs> are
# the system libraries of the link, separated by commas.
function(antic_media_glibc_test name target object library libs expected)
    antic_media_glibc(sysroot libdir "${target}")
    antic_media_target(triple flags "${target}")
    add_test(NAME ${name}
        COMMAND "${CMAKE_COMMAND}"
            "-DCLANG=${CMAKE_C_COMPILER}"
            "-DLLD=${ANTIC_LLVM_DIR}/bin/ld.lld"
            "-DTRIPLE=${triple}"
            "-DSYSROOT=${sysroot}"
            "-DLIBDIR=${libdir}"
            "-DOBJECT=${object}"
            "-DLIBRARY=${library}"
            "-DLIBS=${libs}"
            "-DEXPECTED=${expected}"
            "-DHOST=${ANTIC_HOST_TARGET}"
            "-DTARGET=${target}"
            "-DWORK=${CMAKE_BINARY_DIR}/native/link/${name}"
            -P "${PROJECT_SOURCE_DIR}/tests/run_glibc_link.cmake")
endfunction()

# The targets and the compile flags of the two media libraries, miniaudio
# and raylib, which src/native/miniaudio.cmake and src/native/raylib.cmake
# build.
#
# DESIGN: a program that imports anti.raylib or anti.miniaudio links
# dynamically against glibc 2.35 on Linux, as "Runtime archive" in
# docs/decisions.md settles, so both Linux libraries compile against the
# glibc sysroot and not against musl. get-media-sysroot.cmake extends that
# sysroot with the X11 and OpenGL development files of tools/sysroot-pins.
# On macOS both libraries compile against the Apple SDK that
# tools/macos-sdk-pin names, since the zig stubs of the sysroot carry no
# framework headers. A host without that SDK builds no macOS library of
# the two.

antic_shared_path(ANTIC_GLIBC_SYSROOT_DIR "${ANTIC_DEPS_DIR}/sysroot"
    "glibc sysroots from src/native/get-media-sysroot.cmake")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DDEST=${ANTIC_GLIBC_SYSROOT_DIR}"
                        "-DLLVM_BIN=${ANTIC_LLVM_DIR}/bin"
                        "-DTARGETS=linux-x86_64-glibc;linux-arm64-glibc"
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/get-media-sysroot.cmake"
                RESULT_VARIABLE antic_media_fetched)
if(NOT antic_media_fetched EQUAL 0)
    message(FATAL_ERROR "src/native/get-media-sysroot.cmake failed, so the "
                        "build has no glibc sysroot for raylib and miniaudio")
endif()

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
        list(APPEND ANTIC_MEDIA_TARGETS "${target}")
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
    set(sysroot "${ANTIC_GLIBC_SYSROOT_DIR}/${target}-glibc")
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

add_test(NAME media_sysroot
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
            "-DWORK=${CMAKE_BINARY_DIR}/native/media_sysroot"
            "-DLLVM_AR=${ANTIC_LLVM_AR}"
            -P "${PROJECT_SOURCE_DIR}/tests/run_media_sysroot.cmake")

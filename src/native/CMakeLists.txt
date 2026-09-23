# Builds the third-party libraries of the runtime archive as static
# libraries, one per target, from the pinned sources with the pinned clang.
# CMakeLists.txt at the top adds this directory after the cross builds of
# anti_rt and the tests. The targets, the sysroots of the runtime tree, the
# processor levels, llvm-ar and llvm-mc come from there.
# Libraries are added in the order of docs/decisions.md: PCRE2, then Mbed
# TLS, miniaudio and raylib. media.cmake holds what the last two share.
#
# DESIGN: this is a subdirectory of the build of antic and no project of
# its own, so one configure builds every target that has a sysroot, as the
# cross builds of anti_rt do. See docs/decisions-native.md.

# DESIGN: every native library of the runtime archive is built for the
# default level of its target, as docs/anti-language-additions.md settles
# under "CPU levels", and lies in lib/<target>/ beside the level
# directories of anti_rt. The levels come from tools/cpu-levels through
# antic_cpu_level and antic_cpu_march of the top-level file.

if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang" OR NOT ANTIC_LLVM_AR)
    message(STATUS "Native libraries: skipped, they need clang and llvm-ar")
    return()
endif()

execute_process(COMMAND "${CMAKE_C_COMPILER}" -print-resource-dir
    OUTPUT_VARIABLE antic_native_resource OUTPUT_STRIP_TRAILING_WHITESPACE)

# The target triple and the flags of a compile for <target>, with the
# headers of its sysroot in the runtime tree. They are the flags the cross
# builds of anti_rt pass for the same target.
function(antic_native_target out_triple out_flags target)
    set(sysroot "${ANTIC_RUNTIME_DIR}/sysroot/${target}")
    string(REGEX REPLACE "-arm64$" "-aarch64" arch "${target}")
    string(REGEX REPLACE "^[a-z]+-" "" arch "${arch}")
    if(target MATCHES "^linux-")
        set(triple "${arch}-linux-musl")
        set(flags -fPIC -nostdinc -isystem "${sysroot}/usr/include"
            -isystem "${antic_native_resource}/include")
    elseif(target MATCHES "^macos-")
        # The minimum macOS version of the triple is 11.0, the value of
        # MACOS_MIN_MAJOR and MACOS_MIN_MINOR in src/antic/target.h.
        string(REPLACE "aarch64" "arm64" arch "${arch}")
        set(triple "${arch}-apple-macos11")
        set(flags -isysroot "${sysroot}")
    else()
        # DESIGN: the headers of clang come before those of the MSVC CRT,
        # the order clang-cl searches. The CRT holds its own immintrin.h,
        # whose __m256i is the union of MSVC, and the SIMD constants of
        # stb_image_resize2.h in raylib are then initialised byte by byte
        # from 64-bit values, which clang truncates.
        set(triple "${arch}-pc-windows-msvc")
        set(flags -fms-runtime-lib=dll
            -isystem "${antic_native_resource}/include"
            -isystem "${sysroot}/crt/include"
            -isystem "${sysroot}/sdk/include/ucrt"
            -isystem "${sysroot}/sdk/include/um"
            -isystem "${sysroot}/sdk/include/shared")
    endif()
    antic_cpu_level(level "${target}")
    antic_cpu_march(march "${level}")
    list(APPEND flags "${march}")
    set(${out_triple} "${triple}" PARENT_SCOPE)
    set(${out_flags} "${flags}" PARENT_SCOPE)
endfunction()

# The file name of a static library <name> on <target>.
function(antic_native_library out target name)
    if(target MATCHES "^windows-")
        set(${out} "${name}.lib" PARENT_SCOPE)
    else()
        set(${out} "lib${name}.a" PARENT_SCOPE)
    endif()
endfunction()

set(ANTIC_NATIVE_TARGETS "")
foreach(target IN LISTS ANTIC_TARGETS)
    if(EXISTS "${ANTIC_RUNTIME_DIR}/sysroot/${target}")
        list(APPEND ANTIC_NATIVE_TARGETS "${target}")
    endif()
endforeach()

include(pcre2.cmake)
include(media.cmake)
include(miniaudio.cmake)
include(raylib.cmake)

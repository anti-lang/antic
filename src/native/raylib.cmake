# raylib, the library of anti.raylib, built for every target of
# ANTIC_MEDIA_TARGETS as lib/<target>/libraylib.a, or raylib.lib on
# Windows.
#
# The source is the release that tools/raylib-pin names, which the
# bindings use as well. tools/get-raylib.cmake downloads it and checks the
# pinned digest, and ANTIC_RAYLIB_DIR names the extracted release.

if(NOT EXISTS "${ANTIC_RAYLIB_DIR}/src/rcore.c")
    message(FATAL_ERROR "ANTIC_RAYLIB_DIR is ${ANTIC_RAYLIB_DIR}, which holds "
                        "no raylib source. Run tools/get-raylib.cmake.")
endif()
set(ANTIC_RAYLIB_SOURCE "${ANTIC_RAYLIB_DIR}/src")
set(ANTIC_RAYLIB_WORK "${CMAKE_BINARY_DIR}/native/raylib")
antic_native_license(raylib "${ANTIC_RAYLIB_DIR}/LICENSE")

# The seven modules of the library. rglfw.c holds GLFW, and raudio.c the
# audio module over miniaudio.
set(ANTIC_RAYLIB_SOURCES rcore rshapes rtextures rtext rmodels raudio rglfw)

# DESIGN: raylib takes the ma_ functions from libminiaudio of the runtime
# tree, so a program that calls raylib's audio and miniaudio of its own
# holds one copy of each. raudio.c defines MINIAUDIO_IMPLEMENTATION before
# it includes miniaudio.h, and -Dminiaudio_c is the guard with which that
# header marks its implementation as included already, so raudio.c reads
# the declarations alone. The header it reads is raylib's copy in
# src/external/, which the test raylib_miniaudio_pin holds byte for byte
# equal to the header of the pinned miniaudio. The defines of raudio.c
# that shape the implementation, MA_NO_JACK, MA_NO_GENERATION and the
# others, and MA_COINIT_VALUE of 2, then shape nothing: the library is
# miniaudio.c as released, with every back end and COM initialised as a
# multithreaded apartment on Windows. A program of raylib links
# libminiaudio after libraylib.
set(ANTIC_RAYLIB_AUDIO -Dminiaudio_c)

# DESIGN: the desktop back end over GLFW with OpenGL 3.3, the default of
# raylib's own Makefile, with its flags. On Linux GLFW takes X11 alone and
# Wayland stays out, as docs/decisions.md records. The
# configuration is raylib's config.h, unchanged.
set(ANTIC_RAYLIB_DEFINES -DPLATFORM_DESKTOP_GLFW -DGRAPHICS_API_OPENGL_33)

# DESIGN: raylib compiles with -Wall and warnings are errors, with the two
# exceptions of raylib's own build. -Wmissing-braces is off in its
# Makefile. -Wunused-function is what rtextures.c and rtext.c silence with
# a pragma for __GNUC__, which clang for MSVC does not define. -Wextra and
# -Wpedantic raise hundreds of findings in the bundled stb and GLFW
# sources, which are not ours to change. -fwrapv-pointer keeps the bounds
# check of stb_vorbis.c, which compares a pointer after an addition that
# may overflow, and which clang would otherwise fold to false.
set(ANTIC_RAYLIB_WARNINGS -Wall -Wno-missing-braces -Wno-unused-function
    -Werror)
set(ANTIC_RAYLIB_OPTIONS -fno-strict-aliasing -fwrapv-pointer)

# The system libraries a program of <target> links for raylib, beyond the
# C library that antic links for every program. GLFW loads the extensions
# of X11 at run time, and rlgl takes the functions of OpenGL through GLFW.
# libX11 itself is linked, since rcore.c calls it for the clipboard. On
# macOS Cocoa and IOKit resolve every symbol, and GLFW loads OpenGL at run
# time. The frameworks below are the ones anti bind names for raylib.
set(ANTIC_RAYLIB_LIBS_linux X11,m,pthread,dl)
set(ANTIC_RAYLIB_FRAMEWORKS Cocoa IOKit CoreVideo OpenGL)

set(antic_raylib_probe "${PROJECT_SOURCE_DIR}/tests/abi/raylib_probe.c")
foreach(target IN LISTS ANTIC_MEDIA_TARGETS)
    antic_media_target(triple flags "${target}")
    antic_native_library(name "${target}" raylib)
    set(work "${ANTIC_RAYLIB_WORK}/${target}")
    set(library "${ANTIC_RUNTIME_DIR}/lib/${target}/${name}")
    set(platform "")
    if(target MATCHES "^linux-")
        set(platform -D_GLFW_X11)
    elseif(target MATCHES "^macos-")
        set(platform -DGL_SILENCE_DEPRECATION)
    else()
        set(platform -D_CRT_SECURE_NO_WARNINGS -DUNICODE)
    endif()
    set(compile "${CMAKE_C_COMPILER}" --target=${triple} -std=c99 -O2
        ${flags}
        "-ffile-prefix-map=${ANTIC_RAYLIB_DIR}=."
        "-ffile-prefix-map=${CMAKE_BINARY_DIR}=."
        "-ffile-prefix-map=${PROJECT_SOURCE_DIR}=.")
    set(objects "")
    foreach(source IN LISTS ANTIC_RAYLIB_SOURCES)
        set(input "${ANTIC_RAYLIB_SOURCE}/${source}.c")
        set(object "${work}/${source}.o")
        # raudio.c reads the declarations of miniaudio alone.
        set(module "")
        if(source STREQUAL "raudio")
            set(module ${ANTIC_RAYLIB_AUDIO})
        endif()
        # GLFW's Cocoa back end is Objective-C, which rglfw.c includes.
        # DESIGN: the pinned clang calls a class method through a stub
        # objc_msgSendClass$<selector>$<class> that the linker writes.
        # ld64.lld 23.1.1 writes the stubs of objc_msgSend$ and not these,
        # so the calls go through objc_msgSend as Apple's clang compiles
        # them for macOS 11.
        if(source STREQUAL "rglfw" AND target MATCHES "^macos-")
            set(module -x objective-c
                -fno-objc-msgsend-class-selector-stubs)
        endif()
        add_custom_command(OUTPUT "${object}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${work}"
            COMMAND ${compile} ${ANTIC_RAYLIB_WARNINGS} ${ANTIC_RAYLIB_OPTIONS}
                ${ANTIC_RAYLIB_DEFINES} ${platform}
                -I "${ANTIC_RAYLIB_SOURCE}"
                -I "${ANTIC_RAYLIB_SOURCE}/external/glfw/include"
                ${module} -c "${input}" -o "${object}"
            DEPENDS "${input}"
            VERBATIM)
        list(APPEND objects "${object}")
    endforeach()
    add_custom_command(OUTPUT "${library}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${ANTIC_RUNTIME_DIR}/lib/${target}"
        COMMAND "${CMAKE_COMMAND}" -E rm -f "${library}"
        COMMAND "${ANTIC_LLVM_AR}" rcs "${library}" ${objects}
        DEPENDS ${objects}
        VERBATIM)

    # The C half of the test, compiled as a program of that target would
    # compile C against the library. On Linux it carries its own main.
    set(probe "${work}/raylib_probe.o")
    set(probe_main "")
    if(target MATCHES "^linux-")
        set(probe_main -DRAYLIB_PROBE_MAIN)
    endif()
    add_custom_command(OUTPUT "${probe}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${work}"
        COMMAND ${compile} -Wall -Wextra -Wpedantic -Werror -Wshadow
            -Wconversion -Wstrict-prototypes ${probe_main}
            -I "${ANTIC_RAYLIB_SOURCE}"
            -c "${antic_raylib_probe}" -o "${probe}"
        DEPENDS "${antic_raylib_probe}" "${ANTIC_RAYLIB_SOURCE}/raylib.h"
        VERBATIM)
    add_custom_target(raylib_${target} ALL DEPENDS "${library}" "${probe}")
    antic_native_library(miniaudio_name "${target}" miniaudio)
    set(miniaudio "${ANTIC_RUNTIME_DIR}/lib/${target}/${miniaudio_name}")

    set(expected "${PROJECT_SOURCE_DIR}/tests/abi/raylib_link.expected")
    if(target MATCHES "^linux-")
        antic_media_glibc_test(raylib_link_${target} "${target}"
            "${probe}" "${library},${miniaudio}" "${ANTIC_RAYLIB_LIBS_linux}"
            "${expected}")
        continue()
    endif()
    # The library links for every other target through antic, and on the
    # host the program runs.
    set(source "${PROJECT_SOURCE_DIR}/tests/abi/raylib_link.anti")
    set(options "")
    if(target MATCHES "^macos-")
        foreach(framework IN LISTS ANTIC_RAYLIB_FRAMEWORKS)
            string(APPEND options ",--framework,${framework}")
        endforeach()
        string(SUBSTRING "${options}" 1 -1 options)
    endif()
    if(target STREQUAL ANTIC_HOST_TARGET)
        add_test(NAME raylib_run
            COMMAND "${CMAKE_COMMAND}"
                "-DANTIC=$<TARGET_FILE:antic>"
                "-DLLVM_MC=${ANTIC_LLVM_MC}"
                "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
                "-DSOURCE=${source}"
                "-DOBJECTS=${probe},${library},${miniaudio}"
                "-DWORK=${ANTIC_RAYLIB_WORK}/run"
                "-DOPTIONS=${options}"
                -P "${PROJECT_SOURCE_DIR}/tests/run_program.cmake")
    endif()
    add_test(NAME raylib_link_${target}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${source}"
            "-DOBJECTS=${probe},${library},${miniaudio}"
            "-DWORK=${ANTIC_RAYLIB_WORK}/link"
            "-DTARGET=${target}"
            "-DOPTIONS=${options}"
            -P "${PROJECT_SOURCE_DIR}/tests/run_native_link.cmake")
endforeach()

# A program that calls raylib's audio and miniaudio of its own links both
# libraries. The test media_audio_link_<target> links such a program for
# every target, and media_audio_run runs the one of the host.
set(antic_media_audio_probe "${PROJECT_SOURCE_DIR}/tests/abi/media_audio_probe.c")
foreach(target IN LISTS ANTIC_MEDIA_TARGETS)
    antic_media_target(triple flags "${target}")
    antic_native_library(raylib_name "${target}" raylib)
    antic_native_library(miniaudio_name "${target}" miniaudio)
    set(work "${ANTIC_RAYLIB_WORK}/${target}")
    set(libraries "${ANTIC_RUNTIME_DIR}/lib/${target}/${raylib_name}"
        "${ANTIC_RUNTIME_DIR}/lib/${target}/${miniaudio_name}")
    set(probe "${work}/media_audio_probe.o")
    set(probe_main "")
    if(target MATCHES "^linux-")
        set(probe_main -DMEDIA_AUDIO_PROBE_MAIN)
    endif()
    add_custom_command(OUTPUT "${probe}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${work}"
        COMMAND "${CMAKE_C_COMPILER}" --target=${triple} -std=c99 -O2 ${flags}
            "-ffile-prefix-map=${CMAKE_BINARY_DIR}=."
            "-ffile-prefix-map=${PROJECT_SOURCE_DIR}=."
            -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion
            -Wstrict-prototypes ${probe_main}
            -I "${ANTIC_RAYLIB_SOURCE}" -I "${ANTIC_MINIAUDIO_SOURCE}"
            -c "${antic_media_audio_probe}" -o "${probe}"
        DEPENDS "${antic_media_audio_probe}" "${ANTIC_RAYLIB_SOURCE}/raylib.h"
            "${ANTIC_MINIAUDIO_SOURCE}/miniaudio.h"
        VERBATIM)
    add_custom_target(media_audio_${target} ALL DEPENDS "${probe}")

    set(expected "${PROJECT_SOURCE_DIR}/tests/abi/media_audio_link.expected")
    if(target MATCHES "^linux-")
        list(JOIN libraries "," libraries)
        antic_media_glibc_test(media_audio_link_${target} "${target}"
            "${probe}" "${libraries}" "${ANTIC_RAYLIB_LIBS_linux}"
            "${expected}")
        continue()
    endif()
    set(source "${PROJECT_SOURCE_DIR}/tests/abi/media_audio_link.anti")
    set(options "")
    if(target MATCHES "^macos-")
        foreach(framework IN LISTS ANTIC_RAYLIB_FRAMEWORKS)
            string(APPEND options ",--framework,${framework}")
        endforeach()
        string(SUBSTRING "${options}" 1 -1 options)
    endif()
    list(JOIN libraries "," libraries)
    if(target STREQUAL ANTIC_HOST_TARGET)
        add_test(NAME media_audio_run
            COMMAND "${CMAKE_COMMAND}"
                "-DANTIC=$<TARGET_FILE:antic>"
                "-DLLVM_MC=${ANTIC_LLVM_MC}"
                "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
                "-DSOURCE=${source}"
                "-DOBJECTS=${probe},${libraries}"
                "-DWORK=${ANTIC_RAYLIB_WORK}/media_audio_run"
                "-DOPTIONS=${options}"
                -P "${PROJECT_SOURCE_DIR}/tests/run_program.cmake")
    endif()
    add_test(NAME media_audio_link_${target}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${source}"
            "-DOBJECTS=${probe},${libraries}"
            "-DWORK=${ANTIC_RAYLIB_WORK}/media_audio_link"
            "-DTARGET=${target}"
            "-DOPTIONS=${options}"
            -P "${PROJECT_SOURCE_DIR}/tests/run_native_link.cmake")
endforeach()

add_test(NAME raylib_miniaudio_pin
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
            "-DRAYLIB=${ANTIC_RAYLIB_DIR}"
            "-DMINIAUDIO=${ANTIC_MINIAUDIO_SOURCE}"
            -P "${PROJECT_SOURCE_DIR}/tests/run_raylib_miniaudio_pin.cmake")

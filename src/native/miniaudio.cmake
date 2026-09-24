# miniaudio, the audio library of anti.miniaudio, built for every target
# of ANTIC_MEDIA_TARGETS as lib/<target>/libminiaudio.a, or miniaudio.lib
# on Windows.
#
# The source comes from the release that tools/miniaudio-pin names, which
# get-miniaudio.cmake downloads into build/deps/miniaudio and checks
# against the pinned digest.

antic_shared_path(ANTIC_MINIAUDIO_DIR "${ANTIC_DEPS_DIR}/miniaudio"
    "the miniaudio source from src/native/get-miniaudio.cmake")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DDEST=${ANTIC_MINIAUDIO_DIR}"
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/get-miniaudio.cmake"
                RESULT_VARIABLE antic_miniaudio_fetched)
if(NOT antic_miniaudio_fetched EQUAL 0)
    message(FATAL_ERROR "src/native/get-miniaudio.cmake failed, so the build "
                        "has no miniaudio source")
endif()
file(STRINGS "${PROJECT_SOURCE_DIR}/tools/miniaudio-pin"
     antic_miniaudio_version REGEX "^MINIAUDIO_VERSION=")
string(REGEX REPLACE "^MINIAUDIO_VERSION=" "" antic_miniaudio_version
       "${antic_miniaudio_version}")
set(ANTIC_MINIAUDIO_SOURCE
    "${ANTIC_MINIAUDIO_DIR}/miniaudio-${antic_miniaudio_version}")
set(ANTIC_MINIAUDIO_WORK "${CMAKE_BINARY_DIR}/native/miniaudio")
antic_native_license(miniaudio "${ANTIC_MINIAUDIO_SOURCE}/LICENSE")

# DESIGN: miniaudio.c of the release, with no definition of our own. Every
# back end stays in, and miniaudio loads the one it uses at run time: ALSA,
# PulseAudio or JACK on Linux, Core Audio on macOS and WASAPI, DirectSound
# or WinMM on Windows. So it compiles against the C library alone, and a
# program links no audio library. It compiles under the warnings of
# anti_rt, which raise nothing on any of the six targets.
set(ANTIC_MINIAUDIO_WARNINGS -Wall -Wextra -Wpedantic -Werror)

# The system libraries a program of <target> links for miniaudio, beyond
# the C library that antic links for every program.
set(ANTIC_MINIAUDIO_LIBS_linux m,pthread,dl)

set(antic_miniaudio_probe "${PROJECT_SOURCE_DIR}/tests/abi/miniaudio_probe.c")
foreach(target IN LISTS ANTIC_MEDIA_TARGETS)
    antic_media_target(triple flags "${target}")
    antic_native_library(name "${target}" miniaudio)
    set(work "${ANTIC_MINIAUDIO_WORK}/${target}")
    set(library "${ANTIC_RUNTIME_DIR}/lib/${target}/${name}")
    set(compile "${CMAKE_C_COMPILER}" --target=${triple} -std=c99 -O2
        ${flags}
        "-ffile-prefix-map=${ANTIC_MINIAUDIO_SOURCE}=."
        "-ffile-prefix-map=${CMAKE_BINARY_DIR}=."
        "-ffile-prefix-map=${PROJECT_SOURCE_DIR}=.")
    set(object "${work}/miniaudio.o")
    add_custom_command(OUTPUT "${object}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${work}"
        COMMAND ${compile} ${ANTIC_MINIAUDIO_WARNINGS}
            -c "${ANTIC_MINIAUDIO_SOURCE}/miniaudio.c" -o "${object}"
        DEPENDS "${ANTIC_MINIAUDIO_SOURCE}/miniaudio.c"
            "${ANTIC_MINIAUDIO_SOURCE}/miniaudio.h"
        VERBATIM)
    add_custom_command(OUTPUT "${library}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${ANTIC_RUNTIME_DIR}/lib/${target}"
        COMMAND "${CMAKE_COMMAND}" -E rm -f "${library}"
        COMMAND "${ANTIC_LLVM_AR}" rcs "${library}" "${object}"
        DEPENDS "${object}"
        VERBATIM)

    # The C half of the test, compiled as a program of that target would
    # compile C against the library. On Linux it carries its own main.
    set(probe "${work}/miniaudio_probe.o")
    set(probe_main "")
    if(target MATCHES "^linux-")
        set(probe_main -DMINIAUDIO_PROBE_MAIN)
    endif()
    add_custom_command(OUTPUT "${probe}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${work}"
        COMMAND ${compile} ${ANTIC_MINIAUDIO_WARNINGS} -Wshadow -Wconversion
            -Wstrict-prototypes ${probe_main}
            -I "${ANTIC_MINIAUDIO_SOURCE}"
            -c "${antic_miniaudio_probe}" -o "${probe}"
        DEPENDS "${antic_miniaudio_probe}"
            "${ANTIC_MINIAUDIO_SOURCE}/miniaudio.h"
        VERBATIM)
    add_custom_target(miniaudio_${target} ALL DEPENDS "${library}" "${probe}")

    set(expected "${PROJECT_SOURCE_DIR}/tests/abi/miniaudio_link.expected")
    if(target MATCHES "^linux-")
        antic_media_glibc_test(miniaudio_link_${target} "${target}"
            "${probe}" "${library}" "${ANTIC_MINIAUDIO_LIBS_linux}"
            "${expected}")
        continue()
    endif()
    # The library links for every other target through antic, and on the
    # host the program runs.
    set(source "${PROJECT_SOURCE_DIR}/tests/abi/miniaudio_link.anti")
    if(target STREQUAL ANTIC_HOST_TARGET)
        add_test(NAME miniaudio_run
            COMMAND "${CMAKE_COMMAND}"
                "-DANTIC=$<TARGET_FILE:antic>"
                "-DLLVM_MC=${ANTIC_LLVM_MC}"
                "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
                "-DSOURCE=${source}"
                "-DOBJECTS=${probe},${library}"
                "-DWORK=${ANTIC_MINIAUDIO_WORK}/run"
                -P "${PROJECT_SOURCE_DIR}/tests/run_program.cmake")
    endif()
    add_test(NAME miniaudio_link_${target}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${source}"
            "-DOBJECTS=${probe},${library}"
            "-DWORK=${ANTIC_MINIAUDIO_WORK}/link"
            "-DTARGET=${target}"
            -P "${PROJECT_SOURCE_DIR}/tests/run_native_link.cmake")
endforeach()

add_test(NAME miniaudio_pin
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
            "-DRAYLIB=${ANTIC_RAYLIB_DIR}"
            -P "${PROJECT_SOURCE_DIR}/tests/run_miniaudio_pin.cmake")

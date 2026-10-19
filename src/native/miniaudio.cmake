# miniaudio, an audio library of the runtime archive that no module binds
# yet, built for every target of ANTIC_MEDIA_TARGETS as
# lib/<target>/libminiaudio.a, or miniaudio.lib on Windows. The module
# anti.miniaudio of docs/decisions.md is not built.
#
# The source comes from the release that tools/miniaudio-pin names, which
# antic_get_source of get-source.cmake downloads into
# build/deps/miniaudio and checks against the pinned digest.

antic_shared_path(ANTIC_MINIAUDIO_DIR "${ANTIC_DEPS_DIR}/miniaudio"
    "the miniaudio source from tools/miniaudio-pin")
antic_get_source(ANTIC_MINIAUDIO_SOURCE miniaudio "${ANTIC_MINIAUDIO_DIR}"
    SHA256 miniaudio.c)
set(ANTIC_MINIAUDIO_WORK "${CMAKE_BINARY_DIR}/native/miniaudio")
antic_native_license(miniaudio "${ANTIC_MINIAUDIO_SOURCE}/LICENSE")
antic_native_headers(miniaudio "${ANTIC_MINIAUDIO_SOURCE}" miniaudio.h)

# DESIGN: miniaudio.c of the release, with no definition of our own. Every
# back end stays in, and miniaudio loads the one it uses at run time: ALSA,
# PulseAudio or JACK on Linux, Core Audio on macOS and WASAPI, DirectSound
# or WinMM on Windows. So it compiles against the C library alone, and a
# program links no audio library. It compiles with the warnings of
# miniaudio's own build, ANTIC_MINIAUDIO_WARNINGS of
# src/native/warnings.cmake.

# The system libraries a program of <target> links for miniaudio, beyond
# the C library that antic links for every program.
set(ANTIC_MINIAUDIO_LIBS_linux m,pthread,dl)

set(antic_miniaudio_probe "${PROJECT_SOURCE_DIR}/tests/abi/miniaudio_probe.c")
foreach(target IN LISTS ANTIC_MEDIA_TARGETS)
    antic_media_target(triple flags "${target}")
    antic_static_library(name miniaudio "${target}")
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
    # compile C against the library, with the header of include/miniaudio/
    # of the runtime tree. On Linux it carries its own main.
    set(probe "${work}/miniaudio_probe.o")
    set(probe_main "")
    if(target MATCHES "^linux-")
        set(probe_main -DMINIAUDIO_PROBE_MAIN)
    endif()
    add_custom_command(OUTPUT "${probe}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${work}"
        COMMAND ${compile} ${ANTIC_C_WARNINGS} ${probe_main}
            -isystem "${ANTIC_RUNTIME_DIR}/include/miniaudio"
            -c "${antic_miniaudio_probe}" -o "${probe}"
        DEPENDS "${antic_miniaudio_probe}"
            "${ANTIC_RUNTIME_DIR}/include/miniaudio/miniaudio.h"
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
    antic_native_tests(miniaudio_run miniaudio_link "${target}"
        SOURCE "${source}"
        OBJECTS "${probe}" "${library}"
        WORK "${ANTIC_MINIAUDIO_WORK}")
endforeach()

add_test(NAME miniaudio_pin
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
            -DNAME=miniaudio
            -DSCRIPT=src/native/get-source.cmake
            -DCALLER=src/native/miniaudio.cmake
            -DHASH=SHA256 -DPARTS=3
            -P "${ANTIC_RUNNERS}/run_native_pin.cmake")

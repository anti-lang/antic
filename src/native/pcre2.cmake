# PCRE2, the engine of anti.regex and of the check of every pattern literal.
# The 8-bit library with UTF support and JIT off, as "Runtime archive" in
# docs/decisions.md settles, built for every target of the runtime tree as
# lib/<target>/libpcre2-8.a, or pcre2-8.lib on Windows.
#
# The source, the list of its files and the definitions come from
# pcre2-source.cmake, which CMakeLists.txt at the top includes before antic.

antic_native_license(pcre2 "${ANTIC_PCRE2_SOURCE}/LICENCE.md")

foreach(target IN LISTS ANTIC_NATIVE_TARGETS)
    antic_native_target(triple flags "${target}")
    antic_native_library(name "${target}" pcre2-8)
    set(work "${ANTIC_PCRE2_WORK}/${target}")
    set(library "${ANTIC_RUNTIME_DIR}/lib/${target}/${name}")
    set(compile "${CMAKE_C_COMPILER}" --target=${triple} -std=c11 -O2
        ${ANTIC_PCRE2_WARNINGS} ${flags}
        "-ffile-prefix-map=${ANTIC_PCRE2_SOURCE}=."
        "-ffile-prefix-map=${CMAKE_BINARY_DIR}=."
        "-ffile-prefix-map=${PROJECT_SOURCE_DIR}=.")
    set(objects "")
    foreach(input IN LISTS ANTIC_PCRE2_FILES)
        get_filename_component(source "${input}" NAME_WE)
        set(object "${work}/${source}.o")
        add_custom_command(OUTPUT "${object}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${work}"
            COMMAND ${compile} ${ANTIC_PCRE2_DEFINES}
                -I "${ANTIC_PCRE2_INCLUDE}" -I "${ANTIC_PCRE2_SOURCE}/src"
                -c "${input}" -o "${object}"
            DEPENDS "${input}" "${ANTIC_PCRE2_INCLUDE}/config.h"
                "${ANTIC_PCRE2_INCLUDE}/pcre2.h"
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

    # The C half of the test pcre2_link_<target>, compiled as a program of
    # that target would compile C against the library.
    set(probe "${work}/pcre2_probe.o")
    add_custom_command(OUTPUT "${probe}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${work}"
        COMMAND ${compile} -Wshadow -Wconversion -Wstrict-prototypes
            -I "${ANTIC_PCRE2_INCLUDE}"
            -c "${PROJECT_SOURCE_DIR}/tests/abi/pcre2_probe.c" -o "${probe}"
        DEPENDS "${PROJECT_SOURCE_DIR}/tests/abi/pcre2_probe.c"
            "${ANTIC_PCRE2_INCLUDE}/pcre2.h"
        VERBATIM)
    add_custom_target(pcre2_${target} ALL DEPENDS "${library}" "${probe}")

    # The library links for every target, and on the host the program runs.
    if(target STREQUAL ANTIC_HOST_TARGET)
        add_test(NAME pcre2_match
            COMMAND "${CMAKE_COMMAND}"
                "-DANTIC=$<TARGET_FILE:antic>"
                "-DLLVM_MC=${ANTIC_LLVM_MC}"
                "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
                "-DSOURCE=${PROJECT_SOURCE_DIR}/tests/abi/pcre2_match.anti"
                "-DOBJECTS=${probe},${library}"
                "-DWORK=${ANTIC_PCRE2_WORK}/run"
                -P "${PROJECT_SOURCE_DIR}/tests/run_program.cmake")
    endif()
    add_test(NAME pcre2_link_${target}
        COMMAND "${CMAKE_COMMAND}"
            "-DANTIC=$<TARGET_FILE:antic>"
            "-DLLVM_MC=${ANTIC_LLVM_MC}"
            "-DRUNTIME=${ANTIC_RUNTIME_DIR}"
            "-DSOURCE=${PROJECT_SOURCE_DIR}/tests/abi/pcre2_match.anti"
            "-DOBJECTS=${probe},${library}"
            "-DWORK=${ANTIC_PCRE2_WORK}/link"
            "-DTARGET=${target}"
            -P "${PROJECT_SOURCE_DIR}/tests/run_native_link.cmake")
endforeach()

add_test(NAME pcre2_pin
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
            -P "${PROJECT_SOURCE_DIR}/tests/run_pcre2_pin.cmake")

# PCRE2, the engine of anti.regex and of the check of every pattern literal.
# The 8-bit library with UTF support and JIT off, as "Runtime archive" in
# docs/decisions.md settles, built for every target of the runtime tree as
# lib/<target>/libpcre2-8.a, or pcre2-8.lib on Windows.
#
# The source, the list of its files and the definitions come from
# pcre2-source.cmake, which CMakeLists.txt at the top includes before antic.

antic_native_license(pcre2 "${ANTIC_PCRE2_SOURCE}/LICENCE.md")
# The header PCRE2 installs, generated from pcre2.h.generic. The POSIX
# wrapper and its header stay out, as its library does.
antic_native_headers(pcre2 "${ANTIC_PCRE2_INCLUDE}" pcre2.h)

foreach(target IN LISTS ANTIC_NATIVE_TARGETS)
    antic_native_target(triple flags "${target}")
    antic_native_library(name "${target}" pcre2-8)
    set(work "${ANTIC_PCRE2_WORK}/${target}")
    set(library "${ANTIC_RUNTIME_DIR}/lib/${target}/${name}")
    set(compile "${CMAKE_C_COMPILER}" --target=${triple} -std=c11 -O2
        ${flags}
        "-ffile-prefix-map=${ANTIC_PCRE2_SOURCE}=."
        "-ffile-prefix-map=${CMAKE_BINARY_DIR}=."
        "-ffile-prefix-map=${PROJECT_SOURCE_DIR}=.")
    set(objects "")
    foreach(input IN LISTS ANTIC_PCRE2_FILES)
        get_filename_component(source "${input}" NAME_WE)
        set(object "${work}/${source}.o")
        add_custom_command(OUTPUT "${object}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${work}"
            COMMAND ${compile} ${ANTIC_PCRE2_WARNINGS} ${ANTIC_PCRE2_DEFINES}
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
    # that target would compile C against the library, with the header of
    # include/pcre2/ of the runtime tree.
    set(probe "${work}/pcre2_probe.o")
    add_custom_command(OUTPUT "${probe}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${work}"
        COMMAND ${compile} ${ANTIC_C_WARNINGS}
            -isystem "${ANTIC_RUNTIME_DIR}/include/pcre2"
            -c "${PROJECT_SOURCE_DIR}/tests/abi/pcre2_probe.c" -o "${probe}"
        DEPENDS "${PROJECT_SOURCE_DIR}/tests/abi/pcre2_probe.c"
            "${ANTIC_RUNTIME_DIR}/include/pcre2/pcre2.h"
        VERBATIM)
    # DESIGN: the glue of anti.regex, src/rt/regex.c and src/rt/patterns.c,
    # is a runtime library of its own beside PCRE2 and no part of anti_rt.
    # It stands at the default level of the target, as PCRE2 does, and a
    # program that holds anti.regex links both. anti_rt stays the same
    # bytes for every program that writes no pattern. The compiler names the
    # headers of the glue in a dependency file, as for anti_rt, and pcre2.h
    # stands in DEPENDS because the dependency file leaves out a system
    # header.
    antic_native_library(glue_name "${target}" anti_rt_regex)
    set(glue "${ANTIC_RUNTIME_DIR}/lib/${target}/${glue_name}")
    set(glue_objects "")
    antic_runtime_sections(sections "${target}")
    foreach(source regex patterns)
        set(object "${work}/rt_${source}.o")
        add_custom_command(OUTPUT "${object}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${work}"
            COMMAND "${CMAKE_C_COMPILER}" --target=${triple} -std=c11 -O2
                ${ANTIC_C_WARNINGS} ${flags} ${sections}
                "-ffile-prefix-map=${PROJECT_SOURCE_DIR}=."
                -isystem "${ANTIC_PCRE2_INCLUDE}" -MMD -MF "${object}.d"
                -c "${PROJECT_SOURCE_DIR}/src/rt/${source}.c" -o "${object}"
            DEPENDS "${PROJECT_SOURCE_DIR}/src/rt/${source}.c"
                "${ANTIC_PCRE2_INCLUDE}/pcre2.h"
            DEPFILE "${object}.d"
            VERBATIM)
        list(APPEND glue_objects "${object}")
    endforeach()
    add_custom_command(OUTPUT "${glue}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${ANTIC_RUNTIME_DIR}/lib/${target}"
        COMMAND "${CMAKE_COMMAND}" -E rm -f "${glue}"
        COMMAND "${ANTIC_LLVM_AR}" rcs "${glue}" ${glue_objects}
        DEPENDS ${glue_objects}
        VERBATIM)
    add_custom_target(pcre2_${target} ALL
        DEPENDS "${library}" "${probe}" "${glue}")

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

# JIT stays off: the definitions of the build never name SUPPORT_JIT.
add_test(NAME pcre2_pin
    COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}"
            -DNAME=pcre2
            -DSCRIPT=src/native/get-pcre2.cmake
            "-DFILES=src/native/pcre2.cmake,src/native/pcre2-source.cmake,src/native/pcre2-files.cmake"
            -DHASH=SHA256 -DPARTS=2
            "-DABSENT=-DSUPPORT_JIT"
            -DABSENT_IN=src/native/pcre2-files.cmake
            -P "${PROJECT_SOURCE_DIR}/tests/run_native_pin.cmake")

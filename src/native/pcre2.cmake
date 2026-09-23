# PCRE2, the engine of anti.regex and of the pattern check of anti check.
# The 8-bit library with UTF support and JIT off, as "Runtime archive" in
# docs/decisions.md settles, built for every target of the runtime tree as
# lib/<target>/libpcre2-8.a, or pcre2-8.lib on Windows.
#
# The source comes from the release that tools/pcre2-pin names, which
# get-pcre2.cmake downloads into build/deps/pcre2 and checks against the
# pinned digest.

antic_shared_path(ANTIC_PCRE2_DIR "${ANTIC_DEPS_DIR}/pcre2"
    "the PCRE2 source from src/native/get-pcre2.cmake")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DDEST=${ANTIC_PCRE2_DIR}"
                        -P "${CMAKE_CURRENT_SOURCE_DIR}/get-pcre2.cmake"
                RESULT_VARIABLE antic_pcre2_fetched)
if(NOT antic_pcre2_fetched EQUAL 0)
    message(FATAL_ERROR "src/native/get-pcre2.cmake failed, so the build has "
                        "no PCRE2 source")
endif()
file(STRINGS "${PROJECT_SOURCE_DIR}/tools/pcre2-pin" antic_pcre2_version
     REGEX "^PCRE2_VERSION=")
string(REGEX REPLACE "^PCRE2_VERSION=" "" antic_pcre2_version
       "${antic_pcre2_version}")
set(ANTIC_PCRE2_SOURCE "${ANTIC_PCRE2_DIR}/pcre2-${antic_pcre2_version}")

# The three files a build without autotools writes first, as
# NON-AUTOTOOLS-BUILD of the release describes: config.h and pcre2.h from
# their generic forms and the character tables from their default.
set(ANTIC_PCRE2_WORK "${CMAKE_BINARY_DIR}/native/pcre2")
set(ANTIC_PCRE2_INCLUDE "${ANTIC_PCRE2_WORK}/include")
configure_file("${ANTIC_PCRE2_SOURCE}/src/config.h.generic"
    "${ANTIC_PCRE2_INCLUDE}/config.h" COPYONLY)
configure_file("${ANTIC_PCRE2_SOURCE}/src/pcre2.h.generic"
    "${ANTIC_PCRE2_INCLUDE}/pcre2.h" COPYONLY)
configure_file("${ANTIC_PCRE2_SOURCE}/src/pcre2_chartables.c.dist"
    "${ANTIC_PCRE2_WORK}/pcre2_chartables.c" COPYONLY)
configure_file("${ANTIC_PCRE2_SOURCE}/LICENCE.md"
    "${ANTIC_RUNTIME_DIR}/licenses/pcre2.txt" COPYONLY)

# The sources of the 8-bit library, the list of NON-AUTOTOOLS-BUILD.
# pcre2_jit_compile.c is one of them with JIT off, when it holds the stubs
# of the JIT functions.
set(ANTIC_PCRE2_SOURCES auto_possess chkdint compile compile_cgroup
    compile_class config context convert dfa_match error extuni find_bracket
    jit_compile maketables match match_data match_next newline ord2utf
    pattern_info script_run serialize string_utils study substitute
    substring tables ucd valid_utf xclass)

# DESIGN: the generic config.h with four definitions and nothing else.
# PCRE2_CODE_UNIT_WIDTH=8 is the 8-bit library, SUPPORT_UNICODE the UTF
# support, and PCRE2_STATIC drops the dllimport of pcre2.h on Windows. JIT
# is off because SUPPORT_JIT is not defined. The limits stay the defaults
# of the release.
set(ANTIC_PCRE2_DEFINES -DHAVE_CONFIG_H -DPCRE2_CODE_UNIT_WIDTH=8
    -DSUPPORT_UNICODE -DPCRE2_STATIC)

# DESIGN: PCRE2 compiles under the warnings of anti_rt, and warnings are
# errors. The one exception is -Woverlength-strings of -Wpedantic, which
# the table of error messages in pcre2_error.c raises as one literal of
# 5,686 bytes. C11 only guarantees 4,095 bytes, and every compiler of the
# six targets is clang, which takes it.
set(ANTIC_PCRE2_WARNINGS -Wall -Wextra -Wpedantic -Werror
    -Wno-overlength-strings)

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
    foreach(source IN LISTS ANTIC_PCRE2_SOURCES ITEMS chartables)
        if(source STREQUAL "chartables")
            set(input "${ANTIC_PCRE2_WORK}/pcre2_chartables.c")
        else()
            set(input "${ANTIC_PCRE2_SOURCE}/src/pcre2_${source}.c")
        endif()
        set(object "${work}/pcre2_${source}.o")
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

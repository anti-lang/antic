# The source of PCRE2 and the parts of its build that more than one program
# shares: the files a build without autotools writes first, the list of the
# sources and the definitions. CMakeLists.txt at the top includes this file
# before antic, because antic checks every pattern literal with PCRE2, and
# anti_rt compiles a pattern with the same library. pcre2.cmake builds the
# library of each target from what this file sets, and tools/pack-anti.cmake
# compiles the PCRE2 of a packaged antic from the same list.
#
# The source comes from the release that tools/pcre2-pin names, which
# get-pcre2.cmake downloads into build/deps/pcre2 and checks against the
# pinned digest.

antic_shared_path(ANTIC_PCRE2_DIR "${ANTIC_DEPS_DIR}/pcre2"
    "the PCRE2 source from src/native/get-pcre2.cmake")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DDEST=${ANTIC_PCRE2_DIR}"
                        -P "${CMAKE_CURRENT_LIST_DIR}/get-pcre2.cmake"
                RESULT_VARIABLE antic_pcre2_fetched)
if(NOT antic_pcre2_fetched EQUAL 0)
    message(FATAL_ERROR "src/native/get-pcre2.cmake failed, so the build has "
                        "no PCRE2 source")
endif()
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../../tools/pcre2-pin"
     antic_pcre2_version REGEX "^PCRE2_VERSION=")
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

# The sources of the 8-bit library, the list of NON-AUTOTOOLS-BUILD.
# pcre2_jit_compile.c is one of them with JIT off, when it holds the stubs
# of the JIT functions.
set(ANTIC_PCRE2_SOURCES auto_possess chkdint compile compile_cgroup
    compile_class config context convert dfa_match error extuni find_bracket
    jit_compile maketables match match_data match_next newline ord2utf
    pattern_info script_run serialize string_utils study substitute
    substring tables ucd valid_utf xclass)

# The path of each source, the character tables from the work directory.
set(ANTIC_PCRE2_FILES "")
foreach(source IN LISTS ANTIC_PCRE2_SOURCES)
    list(APPEND ANTIC_PCRE2_FILES "${ANTIC_PCRE2_SOURCE}/src/pcre2_${source}.c")
endforeach()
list(APPEND ANTIC_PCRE2_FILES "${ANTIC_PCRE2_WORK}/pcre2_chartables.c")

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

# DESIGN: antic links the PCRE2 of its host, compiled from the same files
# with the compiler of antic, so the check of a pattern literal and the
# program that compiles it at start read the pattern with one library. The
# library takes the warnings of anti_rt above and not the stricter set of
# antic, which is written for the code of this repository.
add_library(antic_pcre2 STATIC ${ANTIC_PCRE2_FILES})
if(MSVC)
    set_property(TARGET antic_pcre2 PROPERTY COMPILE_OPTIONS /W4 /WX /utf-8)
else()
    set_property(TARGET antic_pcre2 PROPERTY COMPILE_OPTIONS
        ${ANTIC_PCRE2_WARNINGS}
        "-ffile-prefix-map=${ANTIC_PCRE2_SOURCE}=."
        "-ffile-prefix-map=${CMAKE_BINARY_DIR}=.")
endif()
string(REPLACE "-D" "" antic_pcre2_definitions "${ANTIC_PCRE2_DEFINES}")
target_compile_definitions(antic_pcre2 PRIVATE ${antic_pcre2_definitions})
target_include_directories(antic_pcre2 PRIVATE "${ANTIC_PCRE2_INCLUDE}"
    "${ANTIC_PCRE2_SOURCE}/src")

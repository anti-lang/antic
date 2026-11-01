# A name that the tools and the runtime both spell stands in one header,
# and every other file of src/ takes it from there. A flag of the build
# stands once in the CMake files, as the end of this file gives. Each
# entry is a regular expression over the lines of a source and the one
# file that may hold a match. Run with cmake -P and ROOT, the root of the
# repository.

cmake_minimum_required(VERSION 3.21)

set(names
    # The markers of the licence notice.
    "ANTI_LICENSES_|src/rt/license.h"
    # The index of plugins, as a string.
    "\"anti-plugins\\.toml\"|src/rt/plugin_index.h"
    # The suffix of a symbols archive, as the end of a string.
    "-symbols\\.zip\"|src/anti/syms.h"
    # The sysroot directory of the runtime archive inside a path.
    "\"[^\"]*/sysroot[/\"]|src/antic/shared.h")

file(GLOB_RECURSE sources RELATIVE "${ROOT}" "${ROOT}/src/*.c"
     "${ROOT}/src/*.h")
set(failures "")
foreach(entry IN LISTS names)
    string(FIND "${entry}" "|" bar REVERSE)
    string(SUBSTRING "${entry}" 0 ${bar} pattern)
    math(EXPR after "${bar} + 1")
    string(SUBSTRING "${entry}" ${after} -1 owner)
    if(NOT EXISTS "${ROOT}/${owner}")
        string(APPEND failures "\n${owner} is missing")
    endif()
    foreach(source IN LISTS sources)
        if(source STREQUAL owner OR source MATCHES "^src/native/")
            continue()
        endif()
        file(STRINGS "${ROOT}/${source}" lines REGEX "${pattern}")
        foreach(line IN LISTS lines)
            string(STRIP "${line}" line)
            string(APPEND failures
                   "\n${source} spells what ${owner} defines: ${line}")
        endforeach()
    endforeach()
endforeach()
# A flag of the build stands in one CMake file, and every other file of
# the build takes it from the variable there. A line of a comment names it
# freely, and the runners under tests/runners/ spell what they check.
set(flags
    # The contraction of a multiply and an add, ANTIC_FP_OPTIONS.
    "^[^#]*-ffp-contract=|CMakeLists.txt")
file(GLOB_RECURSE cmake_files RELATIVE "${ROOT}" "${ROOT}/src/*.cmake"
     "${ROOT}/src/*CMakeLists.txt" "${ROOT}/tools/*.cmake"
     "${ROOT}/tests/*.cmake" "${ROOT}/tests/*CMakeLists.txt")
list(APPEND cmake_files CMakeLists.txt)
foreach(entry IN LISTS flags)
    string(FIND "${entry}" "|" bar REVERSE)
    string(SUBSTRING "${entry}" 0 ${bar} pattern)
    math(EXPR after "${bar} + 1")
    string(SUBSTRING "${entry}" ${after} -1 owner)
    set(found 0)
    foreach(file IN LISTS cmake_files)
        if(file MATCHES "^tests/runners/")
            continue()
        endif()
        file(STRINGS "${ROOT}/${file}" lines REGEX "${pattern}")
        foreach(line IN LISTS lines)
            string(STRIP "${line}" line)
            if(file STREQUAL owner AND found EQUAL 0)
                set(found 1)
                continue()
            endif()
            string(APPEND failures
                   "\n${file} spells a flag that ${owner} defines once: ${line}")
        endforeach()
    endforeach()
    if(found EQUAL 0)
        string(APPEND failures "\n${owner} defines no ${pattern}")
    endif()
endforeach()
if(NOT failures STREQUAL "")
    message(FATAL_ERROR "names spelled twice:${failures}")
endif()

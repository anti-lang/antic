# A name that the tools and the runtime both spell stands in one header,
# and every other file of src/ takes it from there. Each entry is a
# regular expression over the lines of a source and the one file that
# may hold a match. Run with cmake -P and ROOT, the root of the
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
    "\"[^\"]*/sysroot[/\"]|src/antic/antic.h")

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
if(NOT failures STREQUAL "")
    message(FATAL_ERROR "names spelled twice:${failures}")
endif()

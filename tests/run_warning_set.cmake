# Every warning flag of the build stands in one of two files:
# tools/warnings.cmake, the set of our own code, and
# src/native/warnings.cmake, the sets of the third-party projects. Any
# other build file that spells a warning flag or a definition that turns
# warnings off is refused, and so is a C file that defines one. The rules
# are in docs/c-guidelines.md. Run with cmake -P and ROOT, the root of the
# repository.

set(allowed "tools/warnings.cmake" "src/native/warnings.cmake"
    "tests/run_warning_set.cmake")

file(GLOB build_files RELATIVE "${ROOT}"
    "${ROOT}/CMakeLists.txt" "${ROOT}/CMakePresets.json" "${ROOT}/r"
    "${ROOT}/tools/*.cmake" "${ROOT}/tools/*.sh" "${ROOT}/tools/*.ps1"
    "${ROOT}/src/native/*.cmake" "${ROOT}/tests/*.cmake"
    "${ROOT}/tests/CMakeLists.txt" "${ROOT}/.github/workflows/*.yml")
file(GLOB_RECURSE c_files RELATIVE "${ROOT}"
    "${ROOT}/src/*.c" "${ROOT}/src/*.h" "${ROOT}/tests/*.c" "${ROOT}/tests/*.h"
    "${ROOT}/tests/*.cpp")

# A flag of clang or gcc that names a warning, -w, a flag of MSVC that
# sets or turns off a warning, or a definition of a system header that
# turns its warnings off. -Wl, -Wa and -Wp pass options to other tools.
set(flag "(^|[ \t\"(;=])(-W[a-z][^ \t\")]*|-w|/W[0-9X]|/wd[0-9]+|-D_CRT_SECURE_NO_[A-Z]+|-DGL_SILENCE_DEPRECATION)([ \t\")]|$)")
set(failures "")
foreach(path IN LISTS build_files)
    if(path IN_LIST allowed)
        continue()
    endif()
    file(STRINGS "${ROOT}/${path}" lines)
    set(number 0)
    foreach(line IN LISTS lines)
        math(EXPR number "${number} + 1")
        # A comment may name a flag.
        string(REGEX REPLACE "#.*" "" code "${line}")
        if(code MATCHES "${flag}")
            set(found "${CMAKE_MATCH_2}")
            if(NOT found MATCHES "^-W[alp],")
                string(APPEND failures "\n${path}:${number}: ${found}")
            endif()
        endif()
    endforeach()
endforeach()
foreach(path IN LISTS c_files)
    file(STRINGS "${ROOT}/${path}" lines REGEX "_CRT_SECURE_NO_|_CRT_NONSTDC_NO_")
    foreach(line IN LISTS lines)
        if(line MATCHES "^[ \t]*#[ \t]*define")
            string(APPEND failures "\n${path}: ${line}")
        endif()
    endforeach()
endforeach()
if(NOT failures STREQUAL "")
    message(FATAL_ERROR "warning flags outside tools/warnings.cmake and "
                        "src/native/warnings.cmake:${failures}")
endif()

cmake_minimum_required(VERSION 3.21)

# Every script that runs as `cmake -P` names the minimum version of CMake
# that the root CMakeLists.txt names. Without the line the script sets no
# policy, and CMake 3 keeps the old meaning of `IN_LIST` and of a quoted
# argument of `if()`, which CMake 4 dropped. A GitHub runner with CMake
# 3.31 failed 24 tests that the hosts of CMake 4 passed. A lower version
# lets a script call a command that version lacks, as `file(COPY_FILE)`
# of 3.21. The scripts are those that a build file, a recipe of
# src/native/, a runner, a tool or README.md names after `-P`, found by
# their file name under tests/, tools/ and src/native/. Run with cmake -P
# and ROOT, the root of the repository. MIN, 100 without it, is the least
# number of scripts the test must find.

if(NOT DEFINED MIN)
    set(MIN 100)
endif()

file(STRINGS "${ROOT}/CMakeLists.txt" root_line
     REGEX "^cmake_minimum_required\\(")
if(NOT root_line MATCHES "^cmake_minimum_required\\(VERSION ([0-9.]+)\\)$")
    message(FATAL_ERROR "CMakeLists.txt names no minimum version of CMake")
endif()
set(want "cmake_minimum_required(VERSION ${CMAKE_MATCH_1})")

file(GLOB_RECURSE readers
     "${ROOT}/tests/*.cmake" "${ROOT}/tests/CMakeLists.txt"
     "${ROOT}/tools/*" "${ROOT}/src/native/*.cmake"
     "${ROOT}/src/native/CMakeLists.txt")
list(APPEND readers "${ROOT}/CMakeLists.txt" "${ROOT}/README.md")
file(GLOB_RECURSE candidates "${ROOT}/tests/*.cmake" "${ROOT}/tools/*.cmake"
     "${ROOT}/src/native/*.cmake")

set(names "")
foreach(reader IN LISTS readers)
    if(IS_DIRECTORY "${reader}" OR reader MATCHES "/tools/keys/")
        continue()
    endif()
    file(READ "${reader}" text)
    string(REGEX MATCHALL "-P[ \t]+\"?[^\" \t\n]*/[A-Za-z0-9_.-]+\\.cmake"
           calls "${text}")
    foreach(call IN LISTS calls)
        string(REGEX REPLACE "^.*/" "" name "${call}")
        list(APPEND names "${name}")
    endforeach()
endforeach()
list(REMOVE_DUPLICATES names)

set(missing "")
set(found 0)
foreach(path IN LISTS candidates)
    get_filename_component(name "${path}" NAME)
    if(NOT name IN_LIST names)
        continue()
    endif()
    math(EXPR found "${found} + 1")
    file(STRINGS "${path}" lines REGEX "^cmake_minimum_required\\(")
    file(RELATIVE_PATH shown "${ROOT}" "${path}")
    if(lines STREQUAL "")
        list(APPEND missing "${shown}: no cmake_minimum_required")
    elseif(NOT lines STREQUAL want)
        list(APPEND missing "${shown}: ${lines}")
    endif()
endforeach()
if(found LESS MIN)
    message(FATAL_ERROR "found ${found} scripts of cmake -P, fewer than the "
                        "tree holds")
endif()
if(missing)
    list(JOIN missing "\n" missing)
    message(FATAL_ERROR "these scripts of cmake -P do not name ${want}:\n"
                        "${missing}")
endif()

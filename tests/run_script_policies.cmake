cmake_minimum_required(VERSION 3.21)

# Every script that runs as `cmake -P` names the minimum version of CMake.
# Without it the script sets no policy, and CMake 3 keeps the old meaning
# of `IN_LIST` and of a quoted argument of `if()`, which CMake 4 dropped.
# A GitHub runner with CMake 3.31 failed 24 tests that the hosts of CMake 4
# passed. The scripts are those that CMakeLists.txt and tests/CMakeLists.txt
# name after `-P`. Run with cmake -P and ROOT, the root of the repository.

set(scripts "")
foreach(list_file "${ROOT}/CMakeLists.txt" "${ROOT}/tests/CMakeLists.txt")
    file(READ "${list_file}" text)
    string(REGEX MATCHALL "-P \"[^\"]*/([a-z0-9_-]+\\.cmake)\"" calls "${text}")
    foreach(call IN LISTS calls)
        string(REGEX REPLACE "^-P \"[^\"]*/" "" name "${call}")
        string(REPLACE "\"" "" name "${name}")
        list(APPEND scripts "${name}")
    endforeach()
endforeach()
list(REMOVE_DUPLICATES scripts)
set(missing "")
set(found 0)
foreach(name IN LISTS scripts)
    foreach(dir tests tools)
        set(path "${ROOT}/${dir}/${name}")
        if(EXISTS "${path}")
            math(EXPR found "${found} + 1")
            file(STRINGS "${path}" first REGEX "^cmake_minimum_required\\(")
            if(first STREQUAL "")
                list(APPEND missing "${dir}/${name}")
            endif()
        endif()
    endforeach()
endforeach()
if(found LESS 100)
    message(FATAL_ERROR "found ${found} scripts of cmake -P, fewer than the tree holds")
endif()
if(missing)
    list(JOIN missing "\n" missing)
    message(FATAL_ERROR "these scripts of cmake -P name no minimum version:\n${missing}")
endif()

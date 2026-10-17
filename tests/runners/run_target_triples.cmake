# The triple the C of a target is compiled for stands in one table,
# antic_target_triple of tools/target-compile.cmake, with the Windows half
# in tools/windows-compile.cmake. The cross builds of the runtime and of
# the native libraries, the packer, the sysroot script and the C of the
# tests spelled the same mapping in eight places, two of them in another
# form. The test refuses a triple of musl, of macOS 11 or of mingw-w64
# written in a CMake file outside those two. Comments are prose and stay
# out of the search. Run with cmake -P and ROOT, the root of the
# repository.

cmake_minimum_required(VERSION 3.21)

file(GLOB files LIST_DIRECTORIES false RELATIVE "${ROOT}"
     "${ROOT}/tools/*.cmake" "${ROOT}/tests/*.cmake"
     "${ROOT}/tests/runners/*.cmake")
file(GLOB_RECURSE native LIST_DIRECTORIES false RELATIVE "${ROOT}"
     "${ROOT}/src/*.cmake" "${ROOT}/src/*CMakeLists.txt")
list(APPEND files ${native} CMakeLists.txt tests/CMakeLists.txt)
list(REMOVE_ITEM files tools/target-compile.cmake
     tools/windows-compile.cmake)
list(SORT files)
# Each name holds one bracket, so this line does not match itself.
set(triples "linux-mus[l]|apple-maco[s][0-9]|w64-windows-gn[u]")
set(found "")
foreach(file IN LISTS files)
    file(STRINGS "${ROOT}/${file}" lines)
    foreach(line IN LISTS lines)
        if(NOT line MATCHES "^[ \t]*#"
           AND line MATCHES "${triples}")
            string(APPEND found "\n  ${file}: ${line}")
        endif()
    endforeach()
endforeach()
if(NOT found STREQUAL "")
    message(FATAL_ERROR "these lines spell the triple of a target outside "
                        "tools/target-compile.cmake:${found}")
endif()
if(NOT EXISTS "${ROOT}/tools/target-compile.cmake")
    message(FATAL_ERROR "tools/target-compile.cmake is missing")
endif()

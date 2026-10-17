# The LLVM tools of the build are named in one list, ANTIC_LLVM_TOOLS of
# tools/pinned-tools.cmake, and the names lld runs under in one more,
# ANTIC_LLD_NAMES. tools/check-llvm.cmake checked nine names of a list of
# its own and copied eleven of a third, and the tests spelled the eleven
# twice more. The test refuses a line outside tools/pinned-tools.cmake
# that spells a run of those names, and a check-llvm.cmake that does not
# check through antic_llvm_tool_problems, the one check. Comments are
# prose and stay out of the search. Run with cmake -P and ROOT, the root
# of the repository.

cmake_minimum_required(VERSION 3.21)

file(GLOB files LIST_DIRECTORIES false RELATIVE "${ROOT}"
     "${ROOT}/tools/*.cmake" "${ROOT}/tests/*.cmake")
file(GLOB_RECURSE native LIST_DIRECTORIES false RELATIVE "${ROOT}"
     "${ROOT}/src/*.cmake" "${ROOT}/src/*CMakeLists.txt")
list(APPEND files ${native} CMakeLists.txt tests/CMakeLists.txt)
list(SORT files)
set(runs "llvm-ar[ \t]+llvm-readobj" "llvm-objdump[ \t]+llvm-readobj"
    "ld\\.lld[ \t]+ld64\\.lld[ \t]+lld-link")
set(found "")
foreach(file IN LISTS files)
    if(file STREQUAL "tools/pinned-tools.cmake")
        continue()
    endif()
    file(STRINGS "${ROOT}/${file}" lines)
    foreach(line IN LISTS lines)
        if(line MATCHES "^[ \t]*#")
            continue()
        endif()
        foreach(run IN LISTS runs)
            if(line MATCHES "${run}")
                string(APPEND found "\n  ${file}: ${line}")
                break()
            endif()
        endforeach()
    endforeach()
endforeach()
if(NOT found STREQUAL "")
    message(FATAL_ERROR "these lines name the LLVM tools outside "
                        "tools/pinned-tools.cmake:${found}")
endif()
file(STRINGS "${ROOT}/tools/check-llvm.cmake" checks
     REGEX "^[ \t]*antic_llvm_tool_problems\\(")
if(NOT checks)
    message(FATAL_ERROR "tools/check-llvm.cmake checks the tools without "
                        "antic_llvm_tool_problems of tools/pinned-tools.cmake")
endif()

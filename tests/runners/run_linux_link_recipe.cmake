# The link of a Linux program of a release stands in one function,
# antic_host_linux_link of tools/host-compile.cmake. tools/pack-anti.cmake
# links antic and anti with it, and the test linux_libc links its
# stand-in with it, so a change of the recipe that reaches the libc of
# the machine fails the suite before it fails a published package. The
# test held a hand copy of the ld.lld line that a change of the packer
# never reached. The test refuses the options of that line in a CMake
# file outside tools/host-compile.cmake. Comments are prose and stay out
# of the search. Run with cmake -P and ROOT, the root of the repository.

cmake_minimum_required(VERSION 3.21)

file(GLOB files LIST_DIRECTORIES false RELATIVE "${ROOT}"
     "${ROOT}/tools/*.cmake" "${ROOT}/tests/*.cmake"
     "${ROOT}/tests/runners/*.cmake")
list(APPEND files CMakeLists.txt tests/CMakeLists.txt)
list(REMOVE_ITEM files tools/host-compile.cmake)
list(SORT files)
# The name holds a bracket, so this line does not match itself.
set(recipe "--no-dynamic-linke[r]")
set(found "")
foreach(file IN LISTS files)
    file(STRINGS "${ROOT}/${file}" lines)
    foreach(line IN LISTS lines)
        if(NOT line MATCHES "^[ \t]*#" AND line MATCHES "${recipe}")
            string(APPEND found "\n  ${file}: ${line}")
        endif()
    endforeach()
endforeach()
if(NOT found STREQUAL "")
    message(FATAL_ERROR "these lines link a Linux program outside "
                        "antic_host_linux_link of tools/host-compile.cmake:"
                        "${found}")
endif()
foreach(file tools/pack-anti.cmake tests/runners/run_linux_libc.cmake)
    file(READ "${ROOT}/${file}" text)
    if(NOT text MATCHES "antic_host_linux_link\\(")
        message(FATAL_ERROR "${file} links a Linux program without "
                            "antic_host_linux_link")
    endif()
endforeach()

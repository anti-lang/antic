# No header of ours shares its name with a header that one of our sources
# includes with <>. A target that puts the directory of such a header on
# its include path hides the header of the C library behind it. src/rt
# held a signal.h, and the unit tests then compiled the POSIX platform
# layer without the declaration of signal on musl, while the Mac took it
# from another header of its own. Run with cmake -P and ROOT, the root of
# the repository.

cmake_minimum_required(VERSION 3.21)

file(GLOB_RECURSE c_files RELATIVE "${ROOT}"
    "${ROOT}/src/*.c" "${ROOT}/src/*.h" "${ROOT}/tests/*.c" "${ROOT}/tests/*.h"
    "${ROOT}/tests/*.cpp")

set(ours "")
foreach(path IN LISTS c_files)
    if(path MATCHES "\\.h$")
        get_filename_component(name "${path}" NAME)
        list(APPEND ours "${name}")
    endif()
endforeach()

set(failures "")
foreach(path IN LISTS c_files)
    file(STRINGS "${ROOT}/${path}" lines REGEX "^[ \t]*#[ \t]*include[ \t]*<")
    foreach(line IN LISTS lines)
        if(line MATCHES "<([^>]+)>" AND CMAKE_MATCH_1 IN_LIST ours)
            string(APPEND failures
                "  ${path} includes <${CMAKE_MATCH_1}>, the name of a header of ours\n")
        endif()
    endforeach()
endforeach()

if(failures)
    message(FATAL_ERROR "a header of ours hides one of the system:\n${failures}")
endif()
message(STATUS "no header of ours shares a name with a system header")

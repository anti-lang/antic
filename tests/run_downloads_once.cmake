# Every download of the build is antic_download of tools/download.cmake.
# The pinned sources, the sysroots and the LLVM release take it alike, so
# a file kept on disk, the check of its digest and the message of a
# failure are written once. The test refuses file(DOWNLOAD in the root
# CMakeLists.txt, in a CMake file under tools/ and in a recipe of
# src/native/ outside that file, and an antic_download that checks no
# digest it is given.
# Run with cmake -P and ROOT, the root of the repository.

cmake_minimum_required(VERSION 3.21)

file(GLOB_RECURSE files LIST_DIRECTORIES false RELATIVE "${ROOT}"
     "${ROOT}/tools/*.cmake" "${ROOT}/src/native/*.cmake")
list(APPEND files CMakeLists.txt src/native/CMakeLists.txt)
list(SORT files)
set(found "")
foreach(file IN LISTS files)
    if(file STREQUAL "tools/download.cmake")
        continue()
    endif()
    file(STRINGS "${ROOT}/${file}" lines REGEX "file\\(DOWNLOAD")
    if(lines)
        string(APPEND found "\n  ${file}")
    endif()
endforeach()
if(NOT found STREQUAL "")
    message(FATAL_ERROR "these files download outside antic_download of "
                        "tools/download.cmake:${found}")
endif()
file(STRINGS "${ROOT}/tools/download.cmake" lines REGEX "file\\(DOWNLOAD")
list(LENGTH lines count)
if(NOT count EQUAL 1)
    message(FATAL_ERROR "tools/download.cmake holds ${count} calls of "
                        "file(DOWNLOAD, and antic_download is the one")
endif()
file(READ "${ROOT}/tools/download.cmake" text)
string(FIND "${text}" "EXPECTED_HASH \"\${algorithm}=\${digest}\"" at)
if(at EQUAL -1)
    message(FATAL_ERROR "antic_download of tools/download.cmake does not "
                        "check a download against the digest it is given")
endif()

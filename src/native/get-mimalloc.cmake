# Download the pinned mimalloc release source into
# <dir>/mimalloc-<version>.
#
#   cmake -DDEST=<dir> -P src/native/get-mimalloc.cmake
#
# CMake downloads, checks the digest and unpacks the archive itself, so the
# same script runs on macOS, Linux and Windows without a shell. An archive
# already in <dir> with the pinned digest is not downloaded again.
cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED DEST)
    message(FATAL_ERROR "usage: cmake -DDEST=<dir> -P src/native/get-mimalloc.cmake")
endif()

foreach(key VERSION DIGEST)
    file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../../tools/mimalloc-pin" line
         REGEX "^MIMALLOC_${key}=")
    string(REGEX REPLACE "^MIMALLOC_${key}=" "" MIMALLOC_${key} "${line}")
    if(MIMALLOC_${key} STREQUAL "")
        message(FATAL_ERROR "tools/mimalloc-pin has no MIMALLOC_${key}")
    endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/../../tools/upstream-sources.cmake")
antic_pin_url(url mimalloc)
set(name "mimalloc-${MIMALLOC_VERSION}")
set(archive "${DEST}/${name}.tar.gz")
file(MAKE_DIRECTORY "${DEST}")
set(have "")
if(EXISTS "${archive}")
    file(SHA256 "${archive}" have)
endif()
if(NOT have STREQUAL MIMALLOC_DIGEST)
    file(DOWNLOAD "${url}" "${archive}" STATUS status
         EXPECTED_HASH "SHA256=${MIMALLOC_DIGEST}")
    list(GET status 0 code)
    list(GET status 1 text)
    if(NOT code EQUAL 0)
        message(FATAL_ERROR "${url}: ${text}")
    endif()
endif()

# The unpacked tree is read and never written, so an existing one stays.
if(NOT EXISTS "${DEST}/${name}/src/static.c")
    file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${DEST}")
endif()
message(STATUS "${DEST}/${name}")

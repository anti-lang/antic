# Download the pinned miniaudio release source into
# <dir>/miniaudio-<version>.
#
#   cmake -DDEST=<dir> -P src/native/get-miniaudio.cmake
#
# CMake downloads, checks the digest and unpacks the archive itself, so the
# same script runs on macOS, Linux and Windows without a shell. An archive
# already in <dir> with the pinned digest is not downloaded again.
cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED DEST)
    message(FATAL_ERROR "usage: cmake -DDEST=<dir> -P src/native/get-miniaudio.cmake")
endif()

foreach(key VERSION DIGEST)
    file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../../tools/miniaudio-pin" line
         REGEX "^MINIAUDIO_${key}=")
    string(REGEX REPLACE "^MINIAUDIO_${key}=" "" MINIAUDIO_${key} "${line}")
    if(MINIAUDIO_${key} STREQUAL "")
        message(FATAL_ERROR "tools/miniaudio-pin has no MINIAUDIO_${key}")
    endif()
endforeach()

set(name "miniaudio-${MINIAUDIO_VERSION}")
set(url
    "https://github.com/mackron/miniaudio/archive/refs/tags/${MINIAUDIO_VERSION}.tar.gz")
set(archive "${DEST}/${name}.tar.gz")
file(MAKE_DIRECTORY "${DEST}")
set(have "")
if(EXISTS "${archive}")
    file(SHA256 "${archive}" have)
endif()
if(NOT have STREQUAL MINIAUDIO_DIGEST)
    file(DOWNLOAD "${url}" "${archive}" STATUS status
         EXPECTED_HASH "SHA256=${MINIAUDIO_DIGEST}")
    list(GET status 0 code)
    list(GET status 1 text)
    if(NOT code EQUAL 0)
        message(FATAL_ERROR "${url}: ${text}")
    endif()
endif()

# The unpacked tree is read and never written, so an existing one stays.
if(NOT EXISTS "${DEST}/${name}/miniaudio.c")
    file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${DEST}")
endif()
message(STATUS "${DEST}/${name}")

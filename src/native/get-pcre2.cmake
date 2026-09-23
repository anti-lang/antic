# Download the pinned PCRE2 release source into <dir>/pcre2-<version>.
#
#   cmake -DDEST=<dir> -P src/native/get-pcre2.cmake
#
# CMake downloads, checks the digest and unpacks the archive itself, so the
# same script runs on macOS, Linux and Windows without a shell. An archive
# already in <dir> with the pinned digest is not downloaded again.
cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED DEST)
    message(FATAL_ERROR "usage: cmake -DDEST=<dir> -P src/native/get-pcre2.cmake")
endif()

foreach(key VERSION DIGEST)
    file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../../tools/pcre2-pin" line
         REGEX "^PCRE2_${key}=")
    string(REGEX REPLACE "^PCRE2_${key}=" "" PCRE2_${key} "${line}")
    if(PCRE2_${key} STREQUAL "")
        message(FATAL_ERROR "tools/pcre2-pin has no PCRE2_${key}")
    endif()
endforeach()

set(name "pcre2-${PCRE2_VERSION}")
set(url
    "https://github.com/PCRE2Project/pcre2/releases/download/${name}/${name}.tar.gz")
set(archive "${DEST}/${name}.tar.gz")
file(MAKE_DIRECTORY "${DEST}")
set(have "")
if(EXISTS "${archive}")
    file(SHA256 "${archive}" have)
endif()
if(NOT have STREQUAL PCRE2_DIGEST)
    file(DOWNLOAD "${url}" "${archive}" STATUS status
         EXPECTED_HASH "SHA256=${PCRE2_DIGEST}")
    list(GET status 0 code)
    list(GET status 1 text)
    if(NOT code EQUAL 0)
        message(FATAL_ERROR "${url}: ${text}")
    endif()
endif()

# The unpacked tree is read and never written, so an existing one stays.
if(NOT EXISTS "${DEST}/${name}/src/pcre2.h.generic")
    file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${DEST}")
endif()
message(STATUS "${DEST}/${name}")

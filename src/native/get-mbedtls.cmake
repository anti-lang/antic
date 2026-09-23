# Download the pinned Mbed TLS release source into <dir>/mbedtls-<version>.
#
#   cmake -DDEST=<dir> -P src/native/get-mbedtls.cmake
#
# CMake downloads, checks the digest and unpacks the archive itself, so the
# same script runs on macOS, Linux and Windows without a shell. An archive
# already in <dir> with the pinned digest is not downloaded again.
cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED DEST)
    message(FATAL_ERROR "usage: cmake -DDEST=<dir> -P src/native/get-mbedtls.cmake")
endif()

foreach(key VERSION DIGEST)
    file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../../tools/mbedtls-pin" line
         REGEX "^MBEDTLS_${key}=")
    string(REGEX REPLACE "^MBEDTLS_${key}=" "" MBEDTLS_${key} "${line}")
    if(MBEDTLS_${key} STREQUAL "")
        message(FATAL_ERROR "tools/mbedtls-pin has no MBEDTLS_${key}")
    endif()
endforeach()

set(name "mbedtls-${MBEDTLS_VERSION}")
set(url
    "https://github.com/Mbed-TLS/mbedtls/releases/download/${name}/${name}.tar.bz2")
set(archive "${DEST}/${name}.tar.bz2")
file(MAKE_DIRECTORY "${DEST}")
set(have "")
if(EXISTS "${archive}")
    file(SHA256 "${archive}" have)
endif()
if(NOT have STREQUAL MBEDTLS_DIGEST)
    file(DOWNLOAD "${url}" "${archive}" STATUS status
         EXPECTED_HASH "SHA256=${MBEDTLS_DIGEST}")
    list(GET status 0 code)
    list(GET status 1 text)
    if(NOT code EQUAL 0)
        message(FATAL_ERROR "${url}: ${text}")
    endif()
endif()

# The unpacked tree is read and never written, so an existing one stays.
if(NOT EXISTS "${DEST}/${name}/include/mbedtls/mbedtls_config.h")
    file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${DEST}")
endif()
message(STATUS "${DEST}/${name}")

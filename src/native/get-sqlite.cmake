# Download the pinned SQLite amalgamation into
# <dir>/sqlite-amalgamation-<number>, where <number> is the version written
# as 3XXYYZZ, the form of the file names of sqlite.org.
#
#   cmake -DDEST=<dir> -P src/native/get-sqlite.cmake
#
# CMake downloads, checks the digest and unpacks the archive itself, so the
# same script runs on macOS, Linux and Windows without a shell. An archive
# already in <dir> with the pinned digest is not downloaded again.
cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED DEST)
    message(FATAL_ERROR "usage: cmake -DDEST=<dir> -P src/native/get-sqlite.cmake")
endif()

foreach(key VERSION YEAR DIGEST)
    file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../../tools/sqlite-pin" line
         REGEX "^SQLITE_${key}=")
    string(REGEX REPLACE "^SQLITE_${key}=" "" SQLITE_${key} "${line}")
    if(SQLITE_${key} STREQUAL "")
        message(FATAL_ERROR "tools/sqlite-pin has no SQLITE_${key}")
    endif()
endforeach()

# The URL of the pin names the archive sqlite-amalgamation-<number>.zip,
# whose number tools/upstream-sources.cmake writes from the version, and
# the unpacked directory carries the same name.
include("${CMAKE_CURRENT_LIST_DIR}/../../tools/upstream-sources.cmake")
antic_pin_url(url sqlite)
get_filename_component(name "${url}" NAME_WE)
set(archive "${DEST}/${name}.zip")
file(MAKE_DIRECTORY "${DEST}")
set(have "")
if(EXISTS "${archive}")
    file(SHA3_256 "${archive}" have)
endif()
if(NOT have STREQUAL SQLITE_DIGEST)
    file(DOWNLOAD "${url}" "${archive}" STATUS status
         EXPECTED_HASH "SHA3_256=${SQLITE_DIGEST}")
    list(GET status 0 code)
    list(GET status 1 text)
    if(NOT code EQUAL 0)
        message(FATAL_ERROR "${url}: ${text}")
    endif()
endif()

# The unpacked tree is read and never written, so an existing one stays.
if(NOT EXISTS "${DEST}/${name}/sqlite3.c")
    file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${DEST}")
endif()
message(STATUS "${DEST}/${name}")

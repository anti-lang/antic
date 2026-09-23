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

# X.Y.Z is written as X, then Y and Z in two digits each, then 00, so
# 3.8.2 would be 3080200.
if(NOT SQLITE_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
    message(FATAL_ERROR "SQLITE_VERSION is `${SQLITE_VERSION}`, expected X.Y.Z")
endif()
set(number "${CMAKE_MATCH_1}")
foreach(part "${CMAKE_MATCH_2}" "${CMAKE_MATCH_3}")
    if(part LESS 10)
        string(APPEND number "0")
    endif()
    string(APPEND number "${part}")
endforeach()
string(APPEND number "00")

set(name "sqlite-amalgamation-${number}")
set(url "https://sqlite.org/${SQLITE_YEAR}/${name}.zip")
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

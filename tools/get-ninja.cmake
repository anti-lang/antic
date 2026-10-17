# Install the pinned Ninja of tools/ninja-pin into a directory.
#
#   cmake -DDEST=<directory> -P tools/get-ninja.cmake
#
# DEST then holds ninja.exe and the archive it came from. The Windows VM
# installs it into %LOCALAPPDATA%\anti-vm\ninja, as docs/vm-setup.md
# says, and step 5 of ./r configures its tree with that ninja.exe.
#
# DESIGN: the pinned clang needs the generator Ninja on Windows, because
# the Visual Studio generator takes the compiler of its own toolset. The
# VM took the Ninja of the Build Tools, which tools/release.sh loaded
# vcvarsall.bat for, the whole environment of MSVC for one build tool.
# Decision 16 of "Decisions of 2026-10-11" in docs/decisions.md pins a
# Ninja of its own instead. A second run keeps an archive whose digest is
# right, and every run checks that ninja.exe reports the version of the
# pin.
cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED DEST)
    message(FATAL_ERROR "usage: cmake -DDEST=<directory> -P tools/get-ninja.cmake")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/download.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/upstream-sources.cmake")
antic_read_pins("${CMAKE_CURRENT_LIST_DIR}/ninja-pin")
antic_pin_url(url ninja)
if("${NINJA_DIGEST}" STREQUAL "")
    message(FATAL_ERROR "tools/ninja-pin has no NINJA_DIGEST")
endif()

get_filename_component(dest "${DEST}" ABSOLUTE)
get_filename_component(archive "${url}" NAME)
set(archive "${dest}/${archive}")
antic_download("${url}" "${archive}" SHA256 "${NINJA_DIGEST}")
file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${dest}")
if(NOT EXISTS "${dest}/ninja.exe")
    message(FATAL_ERROR "${archive} unpacked no ninja.exe")
endif()
execute_process(COMMAND "${dest}/ninja.exe" --version
                OUTPUT_VARIABLE reported OUTPUT_STRIP_TRAILING_WHITESPACE
                RESULT_VARIABLE failed)
if(failed OR NOT reported STREQUAL NINJA_VERSION)
    message(FATAL_ERROR "${dest}/ninja.exe reports `${reported}`, and "
                        "tools/ninja-pin names ${NINJA_VERSION}")
endif()
message(STATUS "Ninja ${NINJA_VERSION} stands in ${dest}")

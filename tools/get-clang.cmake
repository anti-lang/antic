# Install the pinned clang for this host into <dir>: bin/clang, and
# lib/clang/<major>/ with the built-in headers and the compiler-rt builtins
# of all six targets. It compiles antic, the runtime and the libraries of
# src/native/, and it compiles the runtime for every other target.
#
#   cmake [-DDEST=<dir>] [-DARCHIVE=<file>] -P tools/get-clang.cmake
#
# clang comes from the release that tools/clang-pin names, whose recipe
# builds it from the LLVM source of tools/llvm-version.
# tools/fetch-release.cmake downloads the archive of this host and checks
# it against the pin, SHA256SUMS and its signature. DEST defaults to
# build/deps/clang of the repository, and the configure step of CMakeLists.txt
# runs this script. The installers never run it: a user of Anti gets
# binaries. ARCHIVE names an archive already downloaded.
cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/deps-dir.cmake")
if(NOT DEFINED DEST)
    set(DEST "${ANTIC_DEPS_DIR}/clang")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/fetch-release.cmake")
fetch_release("${CMAKE_CURRENT_LIST_DIR}/clang-pin" "${DEST}")

if(CMAKE_HOST_WIN32)
    set(exe ".exe")
endif()
execute_process(COMMAND "${DEST}/bin/clang${exe}" --version
                OUTPUT_VARIABLE out RESULT_VARIABLE status)
if(NOT status EQUAL 0 OR NOT out MATCHES "clang version ${fetched_version}[ \n]")
    message(FATAL_ERROR "${DEST}/bin/clang${exe} is not clang ${fetched_version}: ${out}")
endif()
message(STATUS "clang ${fetched_version} in ${DEST}/bin")

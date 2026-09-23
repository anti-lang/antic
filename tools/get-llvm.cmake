# Install llvm-mc, lld, llvm-ar, llvm-objdump and llvm-readobj for this
# host into <dir>/bin. llvm-mc assembles, lld links under the names ld.lld,
# ld64.lld and lld-link, and llvm-ar writes static libraries. llvm-objdump
# reads the format and architecture of an output, and llvm-readobj decodes
# the Windows unwind data for the tests.
#
#   cmake [-DDEST=<dir>] [-DARCHIVE=<file>] -P tools/get-llvm.cmake
#
# The tools come from the release that tools/llvm-pin names, whose recipe
# builds them from the LLVM source of tools/llvm-version.
# tools/fetch-release.cmake downloads the archive of this host and checks
# it against the pin, SHA256SUMS and its signature. DEST defaults to
# build/deps/llvm of the repository, and the configure step of CMakeLists.txt
# runs this script. ARCHIVE names an archive already downloaded.
cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/deps-dir.cmake")
if(NOT DEFINED DEST)
    set(DEST "${ANTIC_DEPS_DIR}/llvm")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/fetch-release.cmake")
fetch_release("${CMAKE_CURRENT_LIST_DIR}/llvm-pin" "${DEST}")

# lld answers to its four names through argv[0]. The archive carries one
# copy, because Windows has no symbolic link without a privilege.
if(CMAKE_HOST_WIN32)
    set(exe ".exe")
endif()
foreach(name ld.lld ld64.lld lld-link)
    file(COPY_FILE "${DEST}/bin/lld${exe}" "${DEST}/bin/${name}${exe}")
endforeach()

execute_process(COMMAND "${CMAKE_COMMAND}" "-DLLVM_BIN=${DEST}/bin"
                        -P "${CMAKE_CURRENT_LIST_DIR}/check-llvm.cmake"
                RESULT_VARIABLE checked)
if(NOT checked EQUAL 0)
    message(FATAL_ERROR "the tools in ${DEST}/bin are not LLVM ${fetched_version}")
endif()

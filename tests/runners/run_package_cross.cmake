# On a host that is not Linux the packer packs linux-arm64 as well. It
# then compiles a Linux anti from the source list of the CMake build, and
# the link fails when a source is missing. It takes the LLVM tools of
# linux-arm64 from TOOLS and refuses a run without them.
#
#   cmake -DROOT=<repository> -DCLANG=<clang> -DLLVM_BIN=<dir>
#         -DRUNTIME=<dir> -DSYSROOT=<dir> -DWORK=<dir>
#         -P tests/runners/run_package_cross.cmake

# DESIGN: the runtime archive holds the tools of this machine alone. The
# tools of another host come from TOOLS/<host>/bin, which
# tools/get-llvm.cmake -DHOST=<host> lays out as bin/ of the runtime
# archive. A stand-in of that layout keeps the test off the network, and
# a host without its directory is refused before anything is compiled.

cmake_minimum_required(VERSION 3.21)

include("${ROOT}/tools/pinned-tools.cmake")
antic_llvm_archive_tools(tools)
file(READ "${ROOT}/tools/llvm-version" llvm_version)
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

execute_process(COMMAND "${CMAKE_COMMAND}" "-DDEST=${WORK}/linux"
                        "-DCLANG=${CLANG}" "-DLLVM_BIN=${LLVM_BIN}"
                        "-DHOSTS=linux-arm64" "-DSYSROOT=${SYSROOT}"
                        "-DRUNTIME=${RUNTIME}"
                        -P "${ROOT}/tools/pack-anti.cmake"
                RESULT_VARIABLE refused ERROR_VARIABLE err ENCODING NONE)
if(refused EQUAL 0 OR NOT err MATCHES "get-llvm.cmake")
    message(FATAL_ERROR "tools/pack-anti.cmake packed linux-arm64 without "
                        "the tools of linux-arm64: ${err}")
endif()
set(stand_in "${WORK}/tools/linux-arm64/bin")
file(MAKE_DIRECTORY "${stand_in}")
foreach(tool IN LISTS tools)
    file(WRITE "${stand_in}/${tool}" "stand-in ${tool} of linux-arm64\n")
endforeach()
file(WRITE "${stand_in}/llvm-version" "${llvm_version}")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DDEST=${WORK}/linux"
                        "-DCLANG=${CLANG}" "-DLLVM_BIN=${LLVM_BIN}"
                        "-DHOSTS=linux-arm64" "-DSYSROOT=${SYSROOT}"
                        "-DRUNTIME=${RUNTIME}" "-DTOOLS=${WORK}/tools"
                        -P "${ROOT}/tools/pack-anti.cmake"
                RESULT_VARIABLE packed)
file(GLOB archive "${WORK}/linux/anti-*-linux-arm64.tar.xz")
if(NOT packed EQUAL 0 OR NOT archive)
    message(FATAL_ERROR "tools/pack-anti.cmake packed no linux-arm64")
endif()
file(MAKE_DIRECTORY "${WORK}/linux/unpacked")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf "${archive}"
                        anti/bin/ld.lld anti/bin/llvm-version
                WORKING_DIRECTORY "${WORK}/linux/unpacked"
                RESULT_VARIABLE unpacked)
if(NOT unpacked EQUAL 0)
    message(FATAL_ERROR "${archive} holds no bin/ld.lld of linux-arm64")
endif()
file(READ "${WORK}/linux/unpacked/anti/bin/ld.lld" packed_lld)
if(NOT packed_lld STREQUAL "stand-in ld.lld of linux-arm64\n")
    message(FATAL_ERROR "${archive} carries another ld.lld than the one of "
                        "linux-arm64")
endif()

file(REMOVE_RECURSE "${WORK}")

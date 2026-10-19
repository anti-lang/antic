# Pack the package of this host from the antic and the anti of the build,
# list it into entries.txt and unpack it into unpacked/, all in WORK. The
# tests package_* read what this leaves, through tests/package_tree.cmake,
# and package_clean removes it.
#
#   cmake -DROOT=<repository> -DANTIC=<antic> -DANTI=<anti> -DHOST=<host>
#         -DSYSROOT=<dir> -DRUNTIME=<dir> -DCLANG=<clang> -DLLVM_BIN=<dir>
#         -DWORK=<dir> -P tests/runners/run_package.cmake

cmake_minimum_required(VERSION 3.21)

file(REMOVE_RECURSE "${WORK}")
# The sysroots that a package carries, and an SDK of Apple beside the
# stubs of a macOS sysroot, as APPLE_SDK or anti sdk import leave it.
include("${CMAKE_CURRENT_LIST_DIR}/../../tools/cpu-levels.cmake")
antic_read_cpu_levels("${CMAKE_CURRENT_LIST_DIR}/../../tools/cpu-levels")
foreach(name IN LISTS ANTIC_TARGETS ITEMS licenses)
    file(COPY "${SYSROOT}/${name}" DESTINATION "${WORK}/sysroot")
endforeach()
file(WRITE "${WORK}/sysroot/macos-arm64/sdk/sdk-version" "26.5\n")
file(WRITE "${WORK}/sysroot/macos-arm64/sdk/usr/lib/libSystem.tbd" "Apple's\n")
# DESIGN: the antic of this build links the libc of the machine, which is
# right for a host build and wrong for a package. On Linux that binary
# names the glibc of the builder, and tools/pack-anti.cmake refuses it, so
# the packer compiles the two programs against the pinned sysroot instead.
# That takes a few seconds and is the recipe a release runs. Every other
# host takes the faster path, where the machine's libc is the one the
# package ships.
set(programs "-DANTIC=${ANTIC}" "-DANTI=${ANTI}")
if(HOST MATCHES "^linux-")
    set(programs "-DCLANG=${CLANG}" "-DLLVM_BIN=${LLVM_BIN}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" "-DDEST=${WORK}/out" ${programs}
                        "-DHOSTS=${HOST}"
                        "-DSYSROOT=${WORK}/sysroot" "-DRUNTIME=${RUNTIME}"
                        -P "${ROOT}/tools/pack-anti.cmake"
                RESULT_VARIABLE packed)
if(NOT packed EQUAL 0)
    message(FATAL_ERROR "tools/pack-anti.cmake failed for ${HOST}")
endif()
file(GLOB archive "${WORK}/out/anti-*-${HOST}.tar.xz")
if(NOT archive)
    message(FATAL_ERROR "tools/pack-anti.cmake wrote no archive for ${HOST}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar tf "${archive}"
                OUTPUT_VARIABLE entries RESULT_VARIABLE listed ENCODING NONE)
if(NOT listed EQUAL 0)
    message(FATAL_ERROR "${archive} does not list")
endif()
file(WRITE "${WORK}/entries.txt" "${entries}")
file(MAKE_DIRECTORY "${WORK}/unpacked")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf "${archive}"
                WORKING_DIRECTORY "${WORK}/unpacked" RESULT_VARIABLE unpacked)
if(NOT unpacked EQUAL 0)
    message(FATAL_ERROR "${archive} does not unpack")
endif()

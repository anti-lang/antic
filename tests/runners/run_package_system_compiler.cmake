# A release takes the pinned compiler. The cache beside the runtime names
# the compiler of the build, and tools/pack-anti.cmake refuses a build
# with the compiler of the machine.
#
#   cmake -DROOT=<repository> -DANTIC=<antic> -DHOST=<host> -DSYSROOT=<dir>
#         -DWORK=<dir> -P tests/runners/run_package_system_compiler.cmake

cmake_minimum_required(VERSION 3.21)

file(REMOVE_RECURSE "${WORK}")
set(system "${WORK}/system-build")
file(MAKE_DIRECTORY "${system}/runtime")
file(WRITE "${system}/CMakeCache.txt" "ANTIC_SYSTEM_COMPILER:BOOL=ON\n")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DDEST=${WORK}/refused"
                        "-DANTIC=${ANTIC}" "-DHOSTS=${HOST}"
                        "-DSYSROOT=${SYSROOT}" "-DRUNTIME=${system}/runtime"
                        -P "${ROOT}/tools/pack-anti.cmake"
                RESULT_VARIABLE refused ERROR_VARIABLE err ENCODING NONE)
if(refused EQUAL 0 OR NOT err MATCHES "ANTIC_SYSTEM_COMPILER")
    message(FATAL_ERROR "tools/pack-anti.cmake packed a build with the compiler "
                        "of the machine: ${err}")
endif()
file(REMOVE_RECURSE "${WORK}")

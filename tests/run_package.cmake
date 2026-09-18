# Pack the package of this host from the antic of the build, and check that
# the archive carries no key. The installer holds the key that checks the
# LLVM tools, and the package holds the pin that names them. A package of a
# build with the compiler of the machine is refused.
#
#   cmake -DROOT=<repository> -DANTIC=<antic> -DHOST=<host> -DSYSROOT=<dir>
#         -DRUNTIME=<dir> -DWORK=<dir> -P tests/run_package.cmake

file(REMOVE_RECURSE "${WORK}")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DDEST=${WORK}" "-DANTIC=${ANTIC}"
                        "-DHOSTS=${HOST}" "-DSYSROOT=${SYSROOT}"
                        "-DRUNTIME=${RUNTIME}"
                        -P "${ROOT}/tools/pack-anti.cmake"
                RESULT_VARIABLE packed)
if(NOT packed EQUAL 0)
    message(FATAL_ERROR "tools/pack-anti.cmake failed for ${HOST}")
endif()
file(GLOB archive "${WORK}/anti-*-${HOST}.tar.xz")
if(NOT archive)
    message(FATAL_ERROR "tools/pack-anti.cmake wrote no archive for ${HOST}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar tf "${archive}"
                OUTPUT_VARIABLE entries RESULT_VARIABLE listed)
if(NOT listed EQUAL 0)
    message(FATAL_ERROR "${archive} does not list")
endif()
string(REGEX MATCHALL "[^\n]+\\.(pem|gpg|asc)\n" keys "${entries}")
if(keys)
    message(FATAL_ERROR "${archive} carries ${keys}")
endif()
foreach(entry anti/tools/llvm-pin anti/tools/llvm-version)
    if(NOT entries MATCHES "(^|\n)${entry}\n")
        message(FATAL_ERROR "${archive} lacks ${entry}, which the installer reads")
    endif()
endforeach()

# A release takes the pinned compiler. The cache beside the runtime names
# the compiler of the build, and a build with the one of the machine stops.
set(system "${WORK}/system-build")
file(MAKE_DIRECTORY "${system}/runtime")
file(WRITE "${system}/CMakeCache.txt" "ANTIC_SYSTEM_COMPILER:BOOL=ON\n")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DDEST=${WORK}/refused"
                        "-DANTIC=${ANTIC}" "-DHOSTS=${HOST}"
                        "-DSYSROOT=${SYSROOT}" "-DRUNTIME=${system}/runtime"
                        -P "${ROOT}/tools/pack-anti.cmake"
                RESULT_VARIABLE refused ERROR_VARIABLE err)
if(refused EQUAL 0 OR NOT err MATCHES "ANTIC_SYSTEM_COMPILER")
    message(FATAL_ERROR "tools/pack-anti.cmake packed a build with the compiler "
                        "of the machine: ${err}")
endif()
file(REMOVE_RECURSE "${WORK}")

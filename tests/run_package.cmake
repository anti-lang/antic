# Pack the package of this host from the antic and the anti of the build,
# and check that the archive carries no key. The installer holds the key
# that checks the LLVM tools, and the package holds the pin that names
# them. Both macOS sysroots of Zig's stubs go in, and the stubs of Apple's
# SDK in sdk/ stay out. A package of a build with the compiler of the
# machine is refused.
#
#   cmake -DROOT=<repository> -DANTIC=<antic> -DANTI=<anti> -DHOST=<host>
#         -DSYSROOT=<dir> -DRUNTIME=<dir> -DWORK=<dir>
#         -P tests/run_package.cmake

file(REMOVE_RECURSE "${WORK}")
# The sysroots that a package carries, and an SDK of Apple beside the
# stubs of a macOS sysroot, as APPLE_SDK or anti sdk import leave it.
foreach(name linux-x86_64 linux-arm64 macos-arm64 macos-x86_64 licenses)
    file(COPY "${SYSROOT}/${name}" DESTINATION "${WORK}/sysroot")
endforeach()
file(WRITE "${WORK}/sysroot/macos-arm64/sdk/sdk-version" "26.5\n")
file(WRITE "${WORK}/sysroot/macos-arm64/sdk/usr/lib/libSystem.tbd" "Apple's\n")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DDEST=${WORK}/out" "-DANTIC=${ANTIC}"
                        "-DANTI=${ANTI}" "-DHOSTS=${HOST}"
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
string(REGEX MATCHALL "[^\n]+\\.(pem|gpg|asc)\n" keys "${entries}")
if(keys)
    message(FATAL_ERROR "${archive} carries ${keys}")
endif()
set(suffix "")
if(HOST MATCHES "^windows-")
    set(suffix ".exe")
endif()
foreach(entry anti/tools/llvm-pin anti/tools/llvm-version
              anti/tools/zig-stubs-pin anti/bin/antic${suffix}
              anti/bin/anti${suffix}
              anti/sysroot/macos-arm64/usr/lib/libSystem.tbd
              anti/sysroot/macos-x86_64/usr/lib/libSystem.tbd
              anti/sysroot/macos-arm64/sdk-version
              anti/licenses/zig.txt anti/licenses/apsl.txt)
    if(NOT entries MATCHES "(^|\n)${entry}\n")
        message(FATAL_ERROR "${archive} lacks ${entry}")
    endif()
endforeach()
if(entries MATCHES "(^|\n)anti/sysroot/macos-[^/\n]+/sdk/")
    message(FATAL_ERROR "${archive} carries stubs of Apple's SDK")
endif()

# A release takes the pinned compiler. The cache beside the runtime names
# the compiler of the build, and a build with the one of the machine stops.
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

# Link tests/link-identity/distinct.anti for TARGET in release mode and run
# it where this host can: its own target, macos-x86_64 under Rosetta at v1
# on macos-arm64, and windows-x86_64 under the x64 emulation on
# windows-arm64. The program compares the addresses of two functions and of
# two descriptors whose contents could be folded, and prints `0 0 0 0`
# while each pair stays apart. A link that folds identical code shares an
# address there, and the test fails. See the entry on folding under "Scope
# and toolchain" in docs/decisions.md. Run with cmake -P and these values:
#   ANTIC         the antic executable
#   LLVM_MC       the llvm-mc executable
#   RUNTIME       the runtime directory
#   SOURCE        the .anti file
#   WORK          a directory for the executable
#   TARGET        the target name
#   ROOT          the repository

cmake_minimum_required(VERSION 3.21)

include("${ROOT}/tests/program_output.cmake")

if(NOT EXISTS "${RUNTIME}/sysroot/${TARGET}")
    message("SKIP: the runtime archive has no sysroot for ${TARGET}")
    return()
endif()
execute_process(COMMAND "${ANTIC}" --print-host-target
    RESULT_VARIABLE status OUTPUT_VARIABLE host ENCODING NONE
    OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic --print-host-target failed")
endif()
set(runner "")
set(options "")
set(runs FALSE)
if("${TARGET}" STREQUAL "${host}")
    set(runs TRUE)
elseif(host STREQUAL "macos-arm64" AND "${TARGET}" STREQUAL "macos-x86_64")
    # Rosetta has no AVX2.
    set(runs TRUE)
    set(runner arch -x86_64)
    set(options --cpu v1)
elseif(host STREQUAL "windows-arm64" AND "${TARGET}" STREQUAL "windows-x86_64")
    set(runs TRUE)
endif()

file(MAKE_DIRECTORY "${WORK}")
get_filename_component(program "${SOURCE}" NAME_WE)
set(exe "${WORK}/${program}-${TARGET}")
if("${TARGET}" MATCHES "^windows-")
    string(APPEND exe ".exe")
endif()
file(REMOVE "${exe}")
execute_process(
    COMMAND "${ANTIC}" --target "${TARGET}" ${options} --llvm-mc "${LLVM_MC}"
            --runtime "${RUNTIME}" -o "${exe}" "${SOURCE}"
    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0 OR NOT err STREQUAL "")
    message(FATAL_ERROR "antic failed for ${TARGET}\n${err}")
endif()
if(NOT runs)
    message(STATUS "linked for ${TARGET}, which does not run on ${host}")
    return()
endif()
program_output(got status "${WORK}/${program}-${TARGET}.out" ${runner} "${exe}")
# "0 0 0 0\n14\n" in hexadecimal.
if(NOT status EQUAL 0 OR NOT got STREQUAL "302030203020300a31340a")
    message(FATAL_ERROR "${exe} ended with ${status} and printed the bytes "
                        "${got}. A 1 is a pair that shares an address.")
endif()

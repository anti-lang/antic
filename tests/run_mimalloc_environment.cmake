# A program of musl reads no MIMALLOC_* variable of the environment. Run
# with MIMALLOC_VERBOSE=1 and MIMALLOC_SHOW_STATS=1, it prints what it
# prints without them, and nothing of mimalloc reaches standard output or
# standard error. The program runs where TARGET is the host, or under
# EMULATOR. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   RUNTIME   the runtime directory
#   SOURCE    a program of the C allocator, with its .expected file beside
#             it
#   TARGET    linux-x86_64 or linux-arm64
#   HOST      the target antic runs on
#   EMULATOR  optional, the program that runs TARGET on this host
#   WORK      a directory for the files

cmake_minimum_required(VERSION 3.21)

if(NOT EXISTS "${RUNTIME}/sysroot/${TARGET}")
    message("SKIP: the runtime archive has no sysroot for ${TARGET}")
    return()
endif()
if(NOT "${HOST}" STREQUAL "${TARGET}" AND NOT EMULATOR)
    message("SKIP: ${HOST} runs no program of ${TARGET}")
    return()
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")
string(REGEX REPLACE "\\.anti$" ".expected" expected_file "${SOURCE}")

foreach(build "release" "dev,--dev")
    string(REPLACE "," ";" options "${build}")
    list(POP_FRONT options name)
    set(program "${WORK}/${name}")
    execute_process(COMMAND "${ANTIC}" --target "${TARGET}"
                            --runtime "${RUNTIME}" ${options}
                            -o "${program}" "${SOURCE}"
                    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "antic ${options} failed\n${err}")
    endif()
    set(runner "${program}")
    if(NOT "${HOST}" STREQUAL "${TARGET}")
        set(runner "${EMULATOR}" "${program}")
    endif()
    program_expect("${name}"
        COMMAND "${CMAKE_COMMAND}" -E env MIMALLOC_VERBOSE=1
                MIMALLOC_SHOW_STATS=1 ${runner}
        EXPECTED "${expected_file}")
endforeach()

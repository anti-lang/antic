# An ABI probe on the host: compile NAME.c with the C compiler and
# NAME.anti with antic, run both and compare their output byte for byte.
# Run with cmake -P and these values:
#   NAME      the name of the pair, probe when it is not given
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime directory
#   SOURCES   tests/abi
#   WORK      a directory for the executables
#   CC        the C compiler of the build, with its options
#   HOST_LINK the options of a link of a program of this host

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../tools/warnings.cmake")

if(NOT DEFINED NAME OR NAME STREQUAL "")
    set(NAME probe)
endif()
file(MAKE_DIRECTORY "${WORK}")
execute_process(COMMAND ${CC} ${ANTIC_C_WARNINGS} -std=c11 ${HOST_LINK}
        -o "${WORK}/${NAME}_c" "${SOURCES}/${NAME}.c"
    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "${CC} ${NAME}.c failed\n${err}")
endif()
execute_process(
    COMMAND "${ANTIC}" --llvm-mc "${LLVM_MC}" --runtime "${RUNTIME}"
            -o "${WORK}/${NAME}_anti" "${SOURCES}/${NAME}.anti"
    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic ${NAME}.anti failed\n${err}")
endif()
program_output(c_hex c_status "${WORK}/${NAME}_c.stdout" "${WORK}/${NAME}_c")
program_output(anti_hex anti_status "${WORK}/${NAME}_anti.stdout" "${WORK}/${NAME}_anti")
if(NOT c_hex STREQUAL anti_hex)
    file(READ "${WORK}/${NAME}_c.stdout" from_c)
    file(READ "${WORK}/${NAME}_anti.stdout" from_anti)
    message(FATAL_ERROR "the layouts differ\nC:\n${from_c}\nAnti:\n${from_anti}")
endif()

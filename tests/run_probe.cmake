# The ABI probe on the host: compile probe.c with the C compiler and
# probe.anti with antic, run both and compare their output byte for byte.
# Run with cmake -P and these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime directory
#   SOURCES   tests/abi
#   WORK      a directory for the executables
#   CC        the C compiler of the build, with its options
#   HOST_LINK the options of a link of a program of this host

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../tools/warnings.cmake")

file(MAKE_DIRECTORY "${WORK}")
execute_process(COMMAND ${CC} ${ANTIC_C_WARNINGS} -std=c11 ${HOST_LINK}
        -o "${WORK}/probe_c" "${SOURCES}/probe.c"
    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "${CC} probe.c failed\n${err}")
endif()
execute_process(
    COMMAND "${ANTIC}" --llvm-mc "${LLVM_MC}" --runtime "${RUNTIME}"
            -o "${WORK}/probe_anti" "${SOURCES}/probe.anti"
    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic probe.anti failed\n${err}")
endif()
program_output(c_hex c_status "${WORK}/probe_c.stdout" "${WORK}/probe_c")
program_output(anti_hex anti_status "${WORK}/probe_anti.stdout" "${WORK}/probe_anti")
if(NOT c_hex STREQUAL anti_hex)
    file(READ "${WORK}/probe_c.stdout" from_c)
    file(READ "${WORK}/probe_anti.stdout" from_anti)
    message(FATAL_ERROR "the layouts differ\nC:\n${from_c}\nAnti:\n${from_anti}")
endif()

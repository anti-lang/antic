# Compile a library whose interface holds a class and an enum, then a
# program that uses it, and run the program. Run with cmake -P and these
# values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime directory
#   ROOT      the search root that holds the sources
#   WORK      a directory for the outputs

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

file(MAKE_DIRECTORY "${WORK}")
execute_process(
    COMMAND "${ANTIC}" -c -I "${ROOT}" -o "${WORK}/shapes.antl"
            "${ROOT}/com/example/shapes.anti"
    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic -c failed with ${status}\n${err}")
endif()
execute_process(
    COMMAND "${ANTIC}" -I "${ROOT}" --llvm-mc "${LLVM_MC}"
            --runtime "${RUNTIME}" -o "${WORK}/client" "${ROOT}/client.anti"
            "${WORK}/shapes.antl"
    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic failed with ${status}\n${err}")
endif()
program_output(out_hex status "${WORK}/client.stdout" "${WORK}/client")
file(READ "${WORK}/client.stdout" out)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "the program exited with ${status}")
endif()
string(HEX "5 0 25\n200 1\n" wanted)
if(NOT out_hex STREQUAL wanted)
    message(FATAL_ERROR "the program printed\n${out}")
endif()

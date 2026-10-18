# Compile a library whose interface holds a class and an enum, then a
# program that uses it, and run the program. Run with cmake -P and these
# values:
#   ANTIC     the antic executable
#   RUNTIME   the runtime directory
#   ROOT      the search root that holds the sources
#   WORK      a directory for the outputs

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

file(MAKE_DIRECTORY "${WORK}")
antic_build(-c -I "${ROOT}" -o "${WORK}/shapes.antl"
            "${ROOT}/com/example/shapes.anti")
antic_build(-I "${ROOT}" --runtime "${RUNTIME}" -o
            "${WORK}/client" "${ROOT}/client.anti" "${WORK}/shapes.antl")
program_output(out_hex status "${WORK}/client.stdout" "${WORK}/client")
file(READ "${WORK}/client.stdout" out)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "the program exited with ${status}")
endif()
string(HEX "5 0 25\n200 1\n" wanted)
expect_same_bytes("the output of the client" "${wanted}" "${out_hex}")

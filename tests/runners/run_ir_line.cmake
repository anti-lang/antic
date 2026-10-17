# Run antic with --dump-ir and require one line of the IR, byte for byte,
# such as the signature of a function. Run with cmake -P and these values:
#   ANTIC    the antic executable
#   COMMAND  the arguments after --dump-ir, separated by commas
#   LINE     the line the IR must hold

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

string(REPLACE "," ";" arguments "${COMMAND}")
antic_build(OUTPUT out COMMAND --dump-ir ${arguments})
string(REPLACE "\n" ";" lines "${out}")
if(NOT LINE IN_LIST lines)
    message(FATAL_ERROR "the IR holds no line '${LINE}'")
endif()

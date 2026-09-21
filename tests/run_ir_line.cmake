# Run antic with --dump-ir and require one line of the IR, byte for byte,
# such as the signature of a function. Run with cmake -P and these values:
#   ANTIC    the antic executable
#   COMMAND  the arguments after --dump-ir, separated by commas
#   LINE     the line the IR must hold

string(REPLACE "," ";" arguments "${COMMAND}")
execute_process(
    COMMAND "${ANTIC}" --dump-ir ${arguments}
    RESULT_VARIABLE status
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
    ENCODING NONE)
if(NOT status EQUAL 0 OR NOT err STREQUAL "")
    message(FATAL_ERROR "antic failed with ${status}\n${err}")
endif()
string(REPLACE "\n" ";" lines "${out}")
if(NOT LINE IN_LIST lines)
    message(FATAL_ERROR "the IR holds no line '${LINE}'")
endif()

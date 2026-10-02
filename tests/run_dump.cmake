# Run antic and compare its standard output byte for byte with an
# expected file. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   COMMAND   the arguments of antic, separated by commas
#   EXPECTED  the file with the expected output
#   OUTPUT    optional file that the command writes, compared instead of
#             the standard output, which must then be empty

string(REPLACE "," ";" arguments "${COMMAND}")
execute_process(
    COMMAND "${ANTIC}" ${arguments}
    RESULT_VARIABLE status
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
    ENCODING NONE)
if(NOT status EQUAL 0 OR NOT err STREQUAL "")
    message(FATAL_ERROR "antic ${COMMAND} failed with ${status}\n${err}")
endif()
file(READ "${EXPECTED}" expected)
if(DEFINED OUTPUT)
    if(NOT out STREQUAL "")
        message(FATAL_ERROR "antic printed output\n${out}")
    endif()
    file(READ "${OUTPUT}" out)
endif()
if(NOT out STREQUAL expected)
    message(FATAL_ERROR "output differs from ${EXPECTED}\ngot:\n${out}")
endif()

# Run antic and compare its standard output byte for byte with an
# expected file. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   COMMAND   the arguments of antic, separated by commas
#   EXPECTED  the file with the expected output
#   VERSION   optional version of antic, which the output names as
#             `antic <version>` and the expected file as `antic VERSION`
#
# The lines of the types the runtime declares, which start with
# "type anti.rt.", are left out of the output before the comparison. Their
# layout is the subject of runtime_types in tests/unit/test_lower.c alone,
# so a change of the descriptor rewrites one expected text.

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
if(DEFINED VERSION)
    string(REPLACE "antic ${VERSION}" "antic VERSION" out "${out}")
endif()
string(REGEX REPLACE "\ntype anti\\.rt\\.[^\n]*" "" out "\n${out}")
string(SUBSTRING "${out}" 1 -1 out)
if(NOT out STREQUAL expected)
    message(FATAL_ERROR "output differs from ${EXPECTED}\ngot:\n${out}")
endif()

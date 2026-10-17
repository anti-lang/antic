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

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

string(REPLACE "," ";" arguments "${COMMAND}")
antic_build(OUTPUT out COMMAND ${arguments})
file(READ "${EXPECTED}" expected)
if(DEFINED VERSION)
    string(REPLACE "antic ${VERSION}" "antic VERSION" out "${out}")
endif()
string(REGEX REPLACE "\ntype anti\\.rt\\.[^\n]*" "" out "\n${out}")
string(SUBSTRING "${out}" 1 -1 out)
expect_same_text("the output for ${EXPECTED}" "${expected}" "${out}")

# Compile a library with antic -c and compare its bytes with a hex listing,
# 16 bytes per line. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   SOURCE    the .anti file
#   ROOT      the search root that holds SOURCE
#   OPTIONS   more options of antic, separated by |
#   OUTPUT    the .antl file to write
#   EXPECTED  the hex listing

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

string(REPLACE "|" ";" options "${OPTIONS}")
antic_build(-c -I "${ROOT}" ${options} -o "${OUTPUT}" "${SOURCE}")
file(READ "${OUTPUT}" hex HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "\\1 " hex "${hex}")
string(REGEX REPLACE "(([0-9a-f][0-9a-f] )([0-9a-f][0-9a-f] )([0-9a-f][0-9a-f] )([0-9a-f][0-9a-f] )([0-9a-f][0-9a-f] )([0-9a-f][0-9a-f] )([0-9a-f][0-9a-f] )([0-9a-f][0-9a-f] )([0-9a-f][0-9a-f] )([0-9a-f][0-9a-f] )([0-9a-f][0-9a-f] )([0-9a-f][0-9a-f] )([0-9a-f][0-9a-f] )([0-9a-f][0-9a-f] )([0-9a-f][0-9a-f] )([0-9a-f][0-9a-f] ))" "\\1\n" hex "${hex}")
string(REGEX REPLACE " \n" "\n" hex "${hex}")
string(REGEX REPLACE " $" "\n" hex "${hex}")
file(READ "${EXPECTED}" expected)
expect_same_text("the bytes of ${OUTPUT} for ${EXPECTED}" "${expected}"
                 "${hex}")

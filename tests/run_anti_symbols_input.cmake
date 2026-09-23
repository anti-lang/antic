# Drive `anti symbols resolve` with a map and a trace whose numbers do
# not fit 64 bits. Run with cmake -P and these values:
#   ANTI  the anti executable
#   WORK  a directory this run writes into
#
# The archive holds a twin that is no program, so every frame is
# answered by the map. A number out of range refuses its line. The map
# line `0-<21 digits>` would otherwise claim every address. A frame
# offset of 21 digits would otherwise land on the last function.

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/archive")

set(id "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef")
string(ASCII 127 del)
file(WRITE "${WORK}/archive/prog.debug" "${del}ELFxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx")
file(WRITE "${WORK}/archive/prog.map"
"# The map of a symbols archive of Anti.
# build ${id}
# target linux-x86_64
0000000000000000-fffffffffffffffffffff huge
0000000000001000-0000000000002000 good src/good.anti:7
00000000000020000000000000000000-0000000000003000 wide
0000000000003000-0000000000004000 next
fffffffffffffff0-ffffffffffffffff top
")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar cf "${WORK}/prog-symbols.zip"
                        --format=zip prog.debug prog.map
                WORKING_DIRECTORY "${WORK}/archive" RESULT_VARIABLE status)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "the archive was not written")
endif()

file(WRITE "${WORK}/trace.txt"
"module 18446744073709551616 ${id} 0x0000000000000000 prog
module 0 ${id} 0x0000000000000000 prog
0x0000000000001001 0+0x1001
0x0000000000003001 0+0x3001
0x0000000000000000 0+0x1fffffffffffffff001
0x0000000000000000 0+0xffffffffffffffff
0x0000000000000000 18446744073709551616+0x1001
0x0000000000000000 0+-0x1001
")
set(expected
"module 18446744073709551616 ${id} 0x0000000000000000 prog
module 0 ${id} 0x0000000000000000 prog
0x0000000000001001 0+0x1001 good src/good.anti:7
0x0000000000003001 0+0x3001 next
0x0000000000000000 0+0x1fffffffffffffff001
0x0000000000000000 0+0xffffffffffffffff top
0x0000000000000000 18446744073709551616+0x1001
0x0000000000000000 0+-0x1001
")

execute_process(COMMAND "${ANTI}" symbols resolve "${WORK}/trace.txt"
                        --symbols "${WORK}/prog-symbols.zip"
                RESULT_VARIABLE status OUTPUT_VARIABLE out
                ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0 OR NOT err STREQUAL "")
    message(FATAL_ERROR "resolve failed with ${status}\n${out}${err}")
endif()
if(NOT out STREQUAL expected)
    message(FATAL_ERROR "resolve gave\n${out}\nexpected\n${expected}")
endif()

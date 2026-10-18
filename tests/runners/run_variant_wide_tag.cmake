# Compile and run a program whose variant has 65536 cases, one more than a
# tag of 16 bits counts. The tag is then a `u32`: the variant measures 4
# bytes, and its last case has the tag 65535. The run writes the source,
# since a checkout holds no file of 65536 cases. Run with cmake -P and
# these values:
#   ANTIC    the antic executable
#   RUNTIME  the runtime directory, holding lib/<target>/<level>/
#   WORK     a directory this run writes into

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

set(cases "")
foreach(n RANGE 0 65535)
    string(APPEND cases "\tC${n},\n")
endforeach()
file(WRITE "${WORK}/variant_wide_tag.anti"
    "extern fn printf(format: ?*byte, ...) -> c_int;\n\n"
    "variant Wide\n{\n${cases}}\n\n"
    "fn main() -> int\n{\n"
    "\tlet w = Wide.C65535;\n"
    "\tprintf(\"%d %d\\n\".ptr, size_of(Wide) as c_int, w.tag as c_int);\n"
    "\treturn 0;\n}\n")
# The run compares the output with the bytes of the expected file.
write_bytes("${WORK}/variant_wide_tag.expected" "exit 0\n4 65535\n")

set(SOURCE "${WORK}/variant_wide_tag.anti")
include("${CMAKE_CURRENT_LIST_DIR}/run_program.cmake")

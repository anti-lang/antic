# Run antic on a source of the largest size it reads, which passes, and on
# one a byte longer, which is refused before it is lexed. Run with cmake -P
# and these values:
#   ANTIC  the antic executable
#   WORK   a directory for the two sources
#
# The largest size is LEX_SOURCE_MAX of src/antic/lexer.h. Spaces make a
# module with no items, which the front end takes.

set(limit 67108864)
file(MAKE_DIRECTORY "${WORK}")
string(REPEAT " " 1048576 mebibyte)
string(REPEAT "${mebibyte}" 64 source)
file(WRITE "${WORK}/largest.anti" "${source}")
file(WRITE "${WORK}/larger.anti" "${source} ")
file(SIZE "${WORK}/largest.anti" size)
if(NOT size EQUAL limit)
    message(FATAL_ERROR "largest.anti holds ${size} bytes, not ${limit}")
endif()

execute_process(
    COMMAND "${ANTIC}" --front-end "${WORK}/largest.anti"
    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic refused a source of ${limit} bytes\n${err}")
endif()

execute_process(
    COMMAND "${ANTIC}" --front-end "${WORK}/larger.anti"
    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(status EQUAL 0)
    message(FATAL_ERROR "antic took a source of more than ${limit} bytes")
endif()
if(NOT err STREQUAL "antic: ${WORK}/larger.anti is larger than 64 MiB\n")
    message(FATAL_ERROR "antic did not name the size of the source\n${err}")
endif()
file(REMOVE "${WORK}/largest.anti" "${WORK}/larger.anti")

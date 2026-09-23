# `antic --front-end` runs the lexer, the parser and the checker, writes
# nothing and reports what the checker found. `anti check` passes it. Run
# with cmake -P and these values:
#   ANTIC     the antic executable
#   RUNTIME   the runtime archive
#   GOOD      a program the front end accepts
#   BAD       a program the checker refuses
#   WORK      a directory the run may write into

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
execute_process(
    COMMAND "${ANTIC}" --front-end --runtime "${RUNTIME}"
            -o "${WORK}/good" "${GOOD}"
    RESULT_VARIABLE status
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
    ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "the front end refused ${GOOD} with ${status}\n${err}")
endif()
file(GLOB written "${WORK}/*")
if(written)
    message(FATAL_ERROR "the front end wrote ${written}")
endif()

# The same run without --runtime needs no linker and no runtime archive,
# because nothing below the checker runs.
execute_process(
    COMMAND "${ANTIC}" --front-end "${GOOD}"
    RESULT_VARIABLE status
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
    ENCODING NONE)
if(status EQUAL 0)
    message(FATAL_ERROR "a program that imports anti.lang needs the archive")
endif()
string(FIND "${err}" "lib/<target>" at)
if(NOT at LESS 0)
    message(FATAL_ERROR "the front end asked for a link\n${err}")
endif()

# A program the checker refuses still exits non-zero.
execute_process(
    COMMAND "${ANTIC}" --front-end --runtime "${RUNTIME}" "${BAD}"
    RESULT_VARIABLE status
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
    ENCODING NONE)
if(status EQUAL 0)
    message(FATAL_ERROR "the front end accepted ${BAD}")
endif()

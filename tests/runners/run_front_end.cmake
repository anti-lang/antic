# `antic --front-end` runs the lexer, the parser and the checker, writes
# nothing and reports what the checker found. `anti check` passes it. Run
# with cmake -P and these values:
#   ANTIC     the antic executable
#   RUNTIME   the runtime archive
#   GOOD      a program the front end accepts
#   BAD       a program the checker refuses
#   WORK      a directory the run may write into

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
antic_build(--front-end --runtime "${RUNTIME}" -o "${WORK}/good" "${GOOD}")
file(GLOB written "${WORK}/*")
if(written)
    message(FATAL_ERROR "the front end wrote ${written}")
endif()

# The same run without --runtime fails, since the library file of
# anti.lang stands in the runtime archive. It asks for no link, because
# nothing below the checker runs.
antic_run(status out err COMMAND --front-end "${GOOD}")
if(status EQUAL 0)
    message(FATAL_ERROR "a program that imports anti.lang needs the archive")
endif()
string(FIND "${err}" "lib/<target>" at)
if(NOT at LESS 0)
    message(FATAL_ERROR "the front end asked for a link\n${err}")
endif()

# A program the checker refuses still exits non-zero.
antic_run(status out err COMMAND --front-end --runtime "${RUNTIME}" "${BAD}")
if(status EQUAL 0)
    message(FATAL_ERROR "the front end accepted ${BAD}")
endif()

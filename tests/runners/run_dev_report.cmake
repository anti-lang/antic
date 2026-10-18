# What a dev build reports on standard error while a program runs, and that
# a release build reports nothing. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   RUNTIME   the runtime archive
#   SOURCE    the program
#   WORK      a directory for the files
#   TIMEOUT   the seconds a run may take. A program that hangs is stopped
#             and fails the test, where a time limit of ctest fails it
#             whatever WILL_FAIL says
#   PATTERN   optional pattern of the message the dev build prints before
#             it aborts. The release build is then built and not run
#
# Without PATTERN both builds run to their end. NAME.expected beside the
# source holds the exit status and the standard output of both, and
# NAME.report the standard error of the dev build. The release build
# prints nothing there.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

get_filename_component(name "${SOURCE}" NAME_WE)
get_filename_component(dir "${SOURCE}" DIRECTORY)
file(MAKE_DIRECTORY "${WORK}")
antic_program("${WORK}/${name}.dev" "${SOURCE}" --dev)
antic_program("${WORK}/${name}.release" "${SOURCE}")
if(DEFINED PATTERN AND NOT PATTERN STREQUAL "")
    program_expect("${name} dev" COMMAND "${WORK}/${name}.dev" ABORTS
                   ERR_MATCH "${PATTERN}" TIMEOUT "${TIMEOUT}")
    return()
endif()
program_expect("${name} dev" COMMAND "${WORK}/${name}.dev"
               EXPECTED "${dir}/${name}.expected"
               ERR_FILE "${dir}/${name}.report" TIMEOUT "${TIMEOUT}")
program_expect("${name} release" COMMAND "${WORK}/${name}.release"
               EXPECTED "${dir}/${name}.expected" TIMEOUT "${TIMEOUT}")

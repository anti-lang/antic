# One dev-mode check, in a build that has it and a build that does not.
# Run with cmake -P and these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime archive
#   SOURCE    the check program
#   PHRASE    the text of the check, which the release build does not carry
#   PATTERN   the message the dev build prints before it aborts
#   RUNS      ON when the release build runs to its end and exits with 7,
#             OFF when the operation traps by itself without the check, so
#             the release build is not run
#   OBJECTS   optional dev objects that the dev build links, separated by
#             commas
#   WORK      a directory for the files
#
# With the check the program prints the file, the line, the operation and
# the values, then aborts. Release mode drops the check and dev mode keeps
# it.

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

get_filename_component(name "${SOURCE}" NAME_WE)
string(REPLACE "," ";" objects "${OBJECTS}")
file(MAKE_DIRECTORY "${WORK}")
antic_program("${WORK}/${name}.release" "${SOURCE}")
file(STRINGS "${WORK}/${name}.release" text)
if(text MATCHES "${PHRASE}")
    message(FATAL_ERROR "the release build of ${name} carries `${PHRASE}`")
endif()
if(RUNS)
    program_expect("${name} release" COMMAND "${WORK}/${name}.release"
                   STATUS 7)
endif()
antic_program("${WORK}/${name}.dev" "${SOURCE}" --dev ${objects})
program_expect("${name} dev" COMMAND "${WORK}/${name}.dev" ABORTS
               ERR_MATCH "${PATTERN}")

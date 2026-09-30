# Build a program that must stop, run it and compare its standard error
# with <name>.err beside the source. The program aborts and writes nothing
# to its standard output. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime archive
#   SOURCE    the program, tests/traps/<name>.anti
#   OPTIONS   optional options of antic, separated by commas
#   WORK      a directory for the files

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

get_filename_component(name "${SOURCE}" NAME_WE)
get_filename_component(dir "${SOURCE}" DIRECTORY)
string(REPLACE "," ";" options "${OPTIONS}")
file(MAKE_DIRECTORY "${WORK}")
antic_program("${WORK}/${name}" "${SOURCE}" ${options})
program_expect("${name}" COMMAND "${WORK}/${name}" ABORTS
               ERR_FILE "${dir}/${name}.err")

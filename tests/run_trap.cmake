# Build a program that must stop, run it and compare its standard error
# with <name>.err beside the source. The program aborts, or exits with
# STATUS where it is given, and writes nothing to its standard output. Run
# with cmake -P and these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime archive
#   SOURCE    the program, tests/traps/<name>.anti
#   OPTIONS   optional options of antic, separated by commas
#   STATUS    optional exit status of a program that ends without aborting
#   WORK      a directory for the files

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

get_filename_component(name "${SOURCE}" NAME_WE)
get_filename_component(dir "${SOURCE}" DIRECTORY)
string(REPLACE "," ";" options "${OPTIONS}")
file(MAKE_DIRECTORY "${WORK}")
antic_program("${WORK}/${name}" "${SOURCE}" ${options})
set(ends ABORTS)
if(DEFINED STATUS)
    set(ends STATUS "${STATUS}")
endif()
program_expect("${name}" COMMAND "${WORK}/${name}" ${ends}
               ERR_FILE "${dir}/${name}.err")

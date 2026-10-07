# Compile the program of the seed and run it twice without a fixed seed.
# The runtime chooses the seed from the random source of the system at
# every start, so the two runs print two seeds. Run with cmake -P and
# these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime directory
#   SOURCE    the .anti file, which prints its seed
#   WORK      a directory for the executable

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

get_filename_component(name "${SOURCE}" NAME_WE)
file(MAKE_DIRECTORY "${WORK}")
set(exe "${WORK}/${name}")
antic_program("${exe}" "${SOURCE}")
set(seeds "")
foreach(run 1 2)
    program_expect("run ${run}" COMMAND "${exe}" ANY_OUT)
    if(NOT program_stdout MATCHES "^seed ([0-9]+)\n")
        message(FATAL_ERROR "run ${run} printed no seed\n${program_stdout}")
    endif()
    list(APPEND seeds "${CMAKE_MATCH_1}")
endforeach()
list(GET seeds 0 first)
list(GET seeds 1 second)
if(first STREQUAL second)
    message(FATAL_ERROR "two runs chose the same seed ${first}")
endif()

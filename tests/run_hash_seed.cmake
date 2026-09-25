# Compile the program of the seed and run it twice without a fixed seed.
# The runtime chooses the seed from the random source of the system at
# every start, so the two runs print two seeds. Run with cmake -P and
# these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime directory
#   SOURCE    the .anti file, which prints its seed
#   WORK      a directory for the executable

get_filename_component(name "${SOURCE}" NAME_WE)
file(MAKE_DIRECTORY "${WORK}")
set(exe "${WORK}/${name}")
execute_process(
    COMMAND "${ANTIC}" --llvm-mc "${LLVM_MC}" --runtime "${RUNTIME}"
            -o "${exe}" "${SOURCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
    ENCODING NONE)
if(NOT status EQUAL 0 OR NOT out STREQUAL "" OR NOT err STREQUAL "")
    message(FATAL_ERROR "antic failed with ${status}\n${out}${err}")
endif()
set(seeds "")
foreach(run 1 2)
    execute_process(COMMAND "${exe}"
        RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT status EQUAL 0 OR NOT err STREQUAL "")
        message(FATAL_ERROR "run ${run} exited with ${status}\n${out}${err}")
    endif()
    if(NOT out MATCHES "^seed ([0-9]+)\n")
        message(FATAL_ERROR "run ${run} printed no seed\n${out}")
    endif()
    list(APPEND seeds "${CMAKE_MATCH_1}")
endforeach()
list(GET seeds 0 first)
list(GET seeds 1 second)
if(first STREQUAL second)
    message(FATAL_ERROR "two runs chose the same seed ${first}")
endif()

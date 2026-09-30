# The start-up check refuses a machine below the level of the program.
# Compile a program for TARGET at its default level, run it and expect the
# message of the section and the exit code of a refusal. Run with cmake -P
# and these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime directory
#   TARGET    the antic target name
#   SOURCE    the .anti file
#   WORK      a directory for the executable
#   NEEDS     the text the message must name

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

file(GLOB runtime_library "${RUNTIME}/lib/${TARGET}/*/libanti_rt.a")
if(runtime_library STREQUAL "")
    message("SKIP: the runtime archive has no runtime for ${TARGET}")
    return()
endif()
file(MAKE_DIRECTORY "${WORK}")
set(exe "${WORK}/cpu_check-${TARGET}")
execute_process(
    COMMAND "${ANTIC}" --target "${TARGET}" --llvm-mc "${LLVM_MC}"
            --runtime "${RUNTIME}" -o "${exe}" "${SOURCE}"
    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic failed for ${TARGET}\n${err}")
endif()
# The runtime refuses at start with the status of a startup error.
program_expect("cpu check ${TARGET}" COMMAND "${exe}" STATUS 70
               ERR_MATCH "this program needs a processor with ${NEEDS}")

# The start-up check refuses a machine below the level of the program.
# Compile a program for TARGET at its default level, run it and expect the
# message of the section and the exit code of a refusal. Run with cmake -P
# and these values:
#   ANTIC     the antic executable
#   RUNTIME   the runtime directory
#   TARGET    the antic target name
#   SOURCE    the .anti file
#   WORK      a directory for the executable
#   NEEDS     the text the message must name

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/../../tools/host.cmake")
antic_static_library(library_name anti_rt "${TARGET}")
file(GLOB runtime_library "${RUNTIME}/lib/${TARGET}/*/${library_name}")
if(runtime_library STREQUAL "")
    message("SKIP: the runtime archive has no runtime for ${TARGET}")
    return()
endif()
file(MAKE_DIRECTORY "${WORK}")
set(exe "${WORK}/cpu_check-${TARGET}")
antic_build(--target "${TARGET}" --runtime "${RUNTIME}"
            -o "${exe}" "${SOURCE}")
# The runtime refuses at start with the status of a startup error.
program_expect("cpu check ${TARGET}" COMMAND "${exe}" STATUS 70
               ERR_MATCH "this program needs a processor with ${NEEDS}")

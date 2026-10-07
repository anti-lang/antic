# Run the program that `anti build` wrote into dist/ and build again at
# once, many times in a row, as a user does. Windows maps a new program
# that just ran for up to 1.6 s, through its Application Identity
# service, and the copy into dist/ then failed with "cannot write". Each
# turn starts from an empty dist/, so it runs a program file that the
# build has just made, as the first run of a user does. Run with cmake
# -P and these values:
#   ANTI     the anti executable
#   RUNTIME  the runtime archive
#   LLVM_MC  the assembler
#   FIXTURE  the directory that holds app/
#   HOST     the target name of this host
#   WORK     a directory this run writes into
#   TURNS    how many times to run and build

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

set(project "${WORK}/app")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(COPY "${FIXTURE}/app" DESTINATION "${WORK}")

# The cache of the repositories is the user's, so the run names its own.
function(build what)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
                "XDG_CACHE_HOME=${WORK}/cache"
                "LOCALAPPDATA=${WORK}/cache"
                "${ANTI}" build --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}"
        WORKING_DIRECTORY "${project}"
        RESULT_VARIABLE status
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "${what} failed with ${status}\n${out}${err}")
    endif()
endfunction()

set(program "${project}/dist/${HOST}/dev/app.exe")
foreach(turn RANGE 1 ${TURNS})
    file(REMOVE_RECURSE "${project}/dist")
    build("the first build of turn ${turn}")
    program_output(hex code "${WORK}/out.txt" "${program}")
    exit_status(code "${code}")
    if(NOT code EQUAL 7)
        message(FATAL_ERROR "turn ${turn}: the program ended with ${code}")
    endif()
    build("the second build of turn ${turn}")
endforeach()

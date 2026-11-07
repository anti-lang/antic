# Build tests/anti-symbols/moved with `anti build --release` for TARGET in
# two directories whose paths differ in length, and compare the two
# programs of dist/ byte for byte. The one link of a macOS release program
# names its LTO object in the debug map relative to the build directory,
# so the UUID that ld64.lld computes over the link and the signature over
# it do not depend on the directory, by Q2 of "Answers of 2026-11-07, sixth
# set". Run with cmake -P and these values:
#   ANTI      the anti executable
#   RUNTIME   the runtime directory
#   FIXTURE   tests/anti-symbols
#   TARGET    the target to build for
#   WORK      a directory this run writes into

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

if(NOT EXISTS "${RUNTIME}/sysroot/${TARGET}")
    message("SKIP: the runtime archive has no sysroot for ${TARGET}")
    return()
endif()
set(name moved)
file(REMOVE_RECURSE "${WORK}")
set(programs "")
foreach(place a another-directory)
    file(MAKE_DIRECTORY "${WORK}/${place}")
    file(COPY "${FIXTURE}/${name}" DESTINATION "${WORK}/${place}")
    anti_run(status out err "${WORK}/${place}/${name}" build --release
             --target "${TARGET}" --runtime "${RUNTIME}")
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "anti build --release in ${place} failed with "
                            "${status}\n${out}${err}")
    endif()
    list(APPEND programs
         "${WORK}/${place}/${name}/dist/${TARGET}/release/${name}")
endforeach()
list(GET programs 0 first)
list(GET programs 1 second)
execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files "${first}"
                        "${second}"
                RESULT_VARIABLE differ)
if(NOT differ EQUAL 0)
    file(READ "${first}" one HEX)
    file(READ "${second}" two HEX)
    string(LENGTH "${one}" length)
    set(count 0)
    set(at 0)
    while(at LESS length)
        string(SUBSTRING "${one}" ${at} 2 x)
        string(SUBSTRING "${two}" ${at} 2 y)
        if(NOT x STREQUAL y)
            math(EXPR count "${count} + 1")
        endif()
        math(EXPR at "${at} + 2")
    endwhile()
    message(FATAL_ERROR "the program of ${TARGET} built in two directories "
                        "differs in ${count} bytes: ${first} and ${second}")
endif()
file(REMOVE_RECURSE "${WORK}")

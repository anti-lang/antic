# Build tests/anti-symbols/moved with `anti build --release` for the host,
# run it and resolve the first frame of the trace it prints through its
# symbols archive. Run with cmake -P and these values:
#   ANTI     the anti executable
#   RUNTIME  the runtime archive
#   FIXTURE  tests/anti-symbols
#   HOST     the target name of this host
#   WORK     a directory this run writes into
#   CLEAN    set to remove the build directory of the project before the
#            resolve, so the archive answers alone
#
# The frame lies in `stop`, at the line of its call of
# `lang.StackTrace.capture`. The map of the archive must name that function
# and that line at the address the shipped program holds them, whatever the
# debug link of the archive lays out. With CLEAN the LTO object that the
# debug map of a Mach-O link names is gone, and the line comes from the
# DWARF of the dSYM in the archive, by Q1 of "Answers of 2026-11-07, sixth
# set". A frame then resolves on a machine that did not build the program.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

set(name moved)
set(project "${WORK}/${name}")
set(source "${project}/src/com/example/${name}.anti")
file(REMOVE_RECURSE "${WORK}")
file(COPY "${FIXTURE}/${name}" DESTINATION "${WORK}")
anti_run(status out err "${project}" build --release --runtime "${RUNTIME}")
if(NOT status EQUAL 0)
    message(FATAL_ERROR "anti build --release failed with ${status}\n${out}${err}")
endif()

# The line of the capture in the source.
file(STRINGS "${source}" lines)
set(number 0)
set(capture 0)
foreach(line IN LISTS lines)
    math(EXPR number "${number} + 1")
    if(line MATCHES "StackTrace\\.capture")
        set(capture ${number})
    endif()
endforeach()
if(capture EQUAL 0)
    message(FATAL_ERROR "${source} holds no capture")
endif()

set(dist "${project}/dist/${HOST}/release")
if(CLEAN)
    file(REMOVE_RECURSE "${project}/build")
endif()
program_expect("trace" COMMAND "${dist}/${name}" ANY_OUT)
file(WRITE "${WORK}/trace.txt" "${program_stdout}")
anti_run(status out err "${WORK}" symbols resolve trace.txt
         --symbols "${dist}/${name}-symbols.zip")
if(NOT status EQUAL 0)
    message(FATAL_ERROR "resolve gave ${status}\n${out}${err}")
endif()

# The first frame of module 0 is the one of the capture.
string(REGEX MATCH "\n0x[0-9a-f]+ 0\\+0x[0-9a-f]+[^\n]*" first "\n${out}")
string(STRIP "${first}" first)
if(NOT first MATCHES " com\\.example\\.${name}\\.stop ${name}\\.anti:${capture}$")
    message(FATAL_ERROR "the frame of the capture resolves to '${first}', "
                        "and stop ${name}.anti:${capture} is where the "
                        "program stood\n${program_stdout}\nbecame\n${out}")
endif()

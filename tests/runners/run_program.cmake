# Compile one test program with antic, run it and compare the result with
# the expected file beside the source. The LLVM text and the bitcode
# beside the executable are gone after the build, unless OPTIONS holds
# --keep-llvm. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   RUNTIME   the runtime directory, holding lib/<target>/<level>/
#   SOURCE    the .anti file
#   WORK      a directory for the executable and intermediate files
#   OBJECTS   optional object files to link, separated by commas
#   OPTIONS   optional options of antic, separated by commas
#   EXPECTED  optional expected file, instead of NAME.expected beside the
#             source, for a target whose output differs
#   UNSET     optional environment variables the program runs without,
#             separated by commas, for a program whose subject is what it
#             does when one of them is absent
#   WARNING   optional text that antic prints, for a program whose subject
#             is a warning. Without it antic prints nothing
#   EMULATOR  optional program that runs the executable, for a target the
#             host runs under emulation alone
#   SKIP_RUN  optional reason the program is linked and not run. The test
#             then reports itself skipped with it
#   TIMEOUT   optional seconds that antic may take, and that the program
#             may take. Either one is stopped after them and fails the
#             test, where a time limit of ctest fails it whatever
#             WILL_FAIL says
#   FAULT     optional value of ANTI_FAULT for the run of the program, for
#             a case of fault injection against the runtime of
#             runtime-faults/, see src/rt/fault.h
#   ERRORS    optional file of the whole standard error of the program,
#             which is otherwise not compared
#
# The expected file starts with the line "exit N", the process exit code.
# Every byte after that line is the expected standard output. An optional
# NAME.args file beside the source holds one command-line argument per line.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

get_filename_component(name "${SOURCE}" NAME_WE)
get_filename_component(dir "${SOURCE}" DIRECTORY)
set(expected_file "${dir}/${name}.expected")
if(DEFINED EXPECTED)
    set(expected_file "${EXPECTED}")
endif()
set(exe "${WORK}/${name}")
file(MAKE_DIRECTORY "${WORK}")
file(REMOVE "${exe}.ll" "${exe}.bc")

string(REPLACE "," ";" objects "${OBJECTS}")
string(REPLACE "," ";" options "${OPTIONS}")
set(limit "")
if(DEFINED TIMEOUT AND NOT TIMEOUT STREQUAL "")
    set(limit TIMEOUT "${TIMEOUT}")
endif()
set(build --runtime "${RUNTIME}" ${options}
          -o "${exe}" "${SOURCE}" ${objects})
# Warnings are errors: antic, opt, llc and the linker must print nothing
# but the warning that the program is about.
if(DEFINED WARNING AND NOT WARNING STREQUAL "")
    antic_run(status out err ${limit} COMMAND ${build})
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "antic failed with ${status}\n${out}${err}")
    endif()
    string(FIND "${err}" "${WARNING}" at)
    if(NOT out STREQUAL "" OR at EQUAL -1)
        message(FATAL_ERROR "antic printed other output\n${out}${err}")
    endif()
else()
    antic_build(${limit} COMMAND ${build})
endif()
if(NOT "--keep-llvm" IN_LIST options)
    foreach(intermediate "${exe}.ll" "${exe}.bc")
        if(EXISTS "${intermediate}")
            message(FATAL_ERROR "${intermediate} stays without --keep-llvm")
        endif()
    endforeach()
endif()

if(DEFINED SKIP_RUN AND NOT SKIP_RUN STREQUAL "")
    message("SKIP: ${SKIP_RUN}")
    return()
endif()

set(program_args "")
if(EXISTS "${dir}/${name}.args")
    file(STRINGS "${dir}/${name}.args" program_args ENCODING UTF-8)
endif()
set(runner "${CMAKE_COMMAND}" -E env)
if(DEFINED UNSET AND NOT UNSET STREQUAL "")
    string(REPLACE "," ";" unset_names "${UNSET}")
    foreach(name IN LISTS unset_names)
        list(APPEND runner "--unset=${name}")
    endforeach()
endif()
if(DEFINED FAULT AND NOT FAULT STREQUAL "")
    list(APPEND runner "ANTI_FAULT=${FAULT}")
endif()
if(runner STREQUAL "${CMAKE_COMMAND};-E;env")
    set(runner "")
endif()
# The standard error of a test program is its own business: a program
# that reports an error writes it there. A case of fault injection names
# what it holds.
set(errors ANY_ERR)
if(DEFINED ERRORS AND NOT ERRORS STREQUAL "")
    set(errors ERR_FILE "${ERRORS}")
endif()
program_expect("${name}" COMMAND ${runner} ${EMULATOR} "${exe}" ${program_args}
               EXPECTED "${expected_file}" ${errors} ${limit})

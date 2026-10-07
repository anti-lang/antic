# Compile one test program with antic, run it and compare the result with
# the expected file beside the source. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
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
#
# The expected file starts with the line "exit N", the process exit code.
# Every byte after that line is the expected standard output. An optional
# NAME.args file beside the source holds one command-line argument per line.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

if(NOT EXISTS "${LLVM_MC}")
    message(FATAL_ERROR
        "llvm-mc not found at '${LLVM_MC}'. Configure with -DANTIC_LLVM_MC=<path>.")
endif()

get_filename_component(name "${SOURCE}" NAME_WE)
get_filename_component(dir "${SOURCE}" DIRECTORY)
set(expected_file "${dir}/${name}.expected")
if(DEFINED EXPECTED)
    set(expected_file "${EXPECTED}")
endif()
set(exe "${WORK}/${name}")
file(MAKE_DIRECTORY "${WORK}")

string(REPLACE "," ";" objects "${OBJECTS}")
string(REPLACE "," ";" options "${OPTIONS}")
execute_process(
    COMMAND "${ANTIC}" --llvm-mc "${LLVM_MC}" --runtime "${RUNTIME}" ${options}
            -o "${exe}" "${SOURCE}" ${objects}
    RESULT_VARIABLE status
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
    ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic failed with ${status}\n${out}${err}")
endif()
# Warnings are errors: antic, llvm-mc and the linker must print nothing
# but the warning that the program is about.
if(DEFINED WARNING AND NOT WARNING STREQUAL "")
    string(FIND "${err}" "${WARNING}" at)
    if(NOT out STREQUAL "" OR at EQUAL -1)
        message(FATAL_ERROR "antic printed other output\n${out}${err}")
    endif()
elseif(NOT out STREQUAL "" OR NOT err STREQUAL "")
    message(FATAL_ERROR "antic printed output\n${out}${err}")
endif()

set(program_args "")
if(EXISTS "${dir}/${name}.args")
    file(STRINGS "${dir}/${name}.args" program_args ENCODING UTF-8)
endif()
set(runner "")
if(DEFINED UNSET AND NOT UNSET STREQUAL "")
    string(REPLACE "," ";" unset_names "${UNSET}")
    set(runner "${CMAKE_COMMAND}" -E env)
    foreach(name IN LISTS unset_names)
        list(APPEND runner "--unset=${name}")
    endforeach()
endif()
# The standard error of a test program is its own business: a program
# that reports an error writes it there.
program_expect("${name}" COMMAND ${runner} "${exe}" ${program_args}
               EXPECTED "${expected_file}" ANY_ERR)

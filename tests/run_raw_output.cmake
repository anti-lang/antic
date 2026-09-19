# The raw-bytes rule: an Anti program writes the bytes it was given, and
# the harness reads them as bytes on every host. The test runs a child that
# writes a CR, an LF and a NUL, then a program that writes them.
#
#   cmake -DROOT=<repository> -DANTIC=<antic> -DLLVM_MC=<llvm-mc>
#         -DRUNTIME=<runtime directory> -DC_PROGRAM=<raw_bytes_c>
#         -DWORK=<dir> -P tests/run_raw_output.cmake

include("${ROOT}/tests/program_output.cmake")
file(MAKE_DIRECTORY "${WORK}")

# The harness reads what a child wrote, byte for byte.
set(bytes_file "${ROOT}/tests/raw/raw_bytes.expected")
program_output(got status "${WORK}/cat.out"
               "${CMAKE_COMMAND}" -E cat "${bytes_file}")
file(READ "${bytes_file}" wanted HEX)
if(NOT status EQUAL 0 OR NOT got STREQUAL wanted)
    message(FATAL_ERROR "the harness read ${got}\nwhere the child wrote ${wanted}")
endif()

# A program writes the same bytes through the runtime, and the harness
# compares them with its expected file.
execute_process(COMMAND "${CMAKE_COMMAND}" "-DANTIC=${ANTIC}"
                        "-DLLVM_MC=${LLVM_MC}" "-DRUNTIME=${RUNTIME}"
                        "-DSOURCE=${ROOT}/tests/raw/raw_bytes.anti"
                        "-DWORK=${WORK}" -P "${ROOT}/tests/run_program.cmake"
                RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
                ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "tests/raw/raw_bytes.anti failed:\n${out}${err}")
endif()

# A C program writes the same bytes. Every C test file includes
# tests/binary_stdio.h, which keeps the C streams of Windows from
# translating them.
program_output(got status "${WORK}/c.out" "${C_PROGRAM}")
string(LENGTH "exit 0\n" skip)
file(READ "${bytes_file}" wanted OFFSET ${skip} HEX)
if(NOT status EQUAL 0 OR NOT got STREQUAL wanted)
    message(FATAL_ERROR "the C program wrote ${got}\nwhere it should write ${wanted}")
endif()
file(GLOB_RECURSE c_files "${ROOT}/tests/*.c" "${ROOT}/tests/*.cpp")
foreach(c_file IN LISTS c_files)
    file(STRINGS "${c_file}" found REGEX "^#include \"\\.\\./binary_stdio\\.h\"$")
    if(NOT found)
        message(FATAL_ERROR "${c_file} does not include ../binary_stdio.h")
    endif()
endforeach()

# CMake decodes the output of execute_process on Windows by the console
# code page unless ENCODING NONE says otherwise. Every other call of a test
# script that captures output names it.
file(GLOB scripts "${ROOT}/tests/run_*.cmake")
set(missing "")
foreach(script IN LISTS scripts)
    file(READ "${script}" text)
    get_filename_component(name "${script}" NAME)
    string(LENGTH "${text}" size)
    set(from 0)
    while(TRUE)
        string(SUBSTRING "${text}" ${from} -1 rest)
        string(FIND "${rest}" "execute_process(" at)
        if(at EQUAL -1)
            break()
        endif()
        math(EXPR start "${from} + ${at}")
        # The call ends at the parenthesis that closes it.
        set(depth 0)
        set(i ${start})
        while(i LESS size)
            string(SUBSTRING "${text}" ${i} 1 c)
            if(c STREQUAL "(")
                math(EXPR depth "${depth} + 1")
            elseif(c STREQUAL ")")
                math(EXPR depth "${depth} - 1")
                if(depth EQUAL 0)
                    break()
                endif()
            endif()
            math(EXPR i "${i} + 1")
        endwhile()
        math(EXPR length "${i} - ${start} + 1")
        string(SUBSTRING "${text}" ${start} ${length} call)
        if(call MATCHES "(OUTPUT|ERROR)_VARIABLE" AND NOT call MATCHES "ENCODING NONE")
            string(SUBSTRING "${text}" 0 ${start} before)
            string(REGEX MATCHALL "\n" lines "${before}")
            list(LENGTH lines line)
            math(EXPR line "${line} + 1")
            list(APPEND missing "${name}:${line}")
        endif()
        math(EXPR from "${i} + 1")
    endwhile()
endforeach()
if(missing)
    list(JOIN missing ", " missing)
    message(FATAL_ERROR "these calls capture output without ENCODING NONE, so "
                        "Windows decodes it through the console: ${missing}")
endif()

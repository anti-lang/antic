# The raw-bytes rule: an Anti program writes the bytes it was given, and
# the harness reads them as bytes on every host. The test runs a child that
# writes a CR, an LF and a NUL, then a program that writes them.
#
#   cmake -DROOT=<repository> -DANTIC=<antic> -DLLVM_MC=<llvm-mc>
#         -DRUNTIME=<runtime directory> -DC_PROGRAM=<raw_bytes_c>
#         -DWORK=<dir> -P tests/run_raw_output.cmake

cmake_minimum_required(VERSION 3.21)

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

# program_expect compares both outputs as bytes and holds the status. Each
# case runs it in a script of its own over a child that must be refused:
# one that writes a CRLF where an LF is expected, one that prints what it
# was not asked to, one that exits with 1 where an abort is expected and
# one that exits with 1 where 0 is. file(WRITE) writes a line feed as a
# CRLF on Windows, so the two inputs name their line ends.
file(CONFIGURE OUTPUT "${WORK}/crlf.txt" CONTENT "2\n" NEWLINE_STYLE CRLF)
set(cat "\"${CMAKE_COMMAND}\" -E cat \"${WORK}/crlf.txt\"")
set(echo "\"${CMAKE_COMMAND}\" -E echo stray")
set(false "\"${CMAKE_COMMAND}\" -E false")
set(refused
    "crlf|COMMAND ${cat} OUT \"2\\n\"|the standard output differs"
    "stray|COMMAND ${echo}|the standard output differs"
    "abort|COMMAND ${false} ABORTS|expected an abort"
    "status|COMMAND ${false}|expected the status 0")
foreach(case IN LISTS refused)
    string(REPLACE "|" ";" parts "${case}")
    list(GET parts 0 name)
    list(GET parts 1 call)
    list(GET parts 2 refusal)
    file(WRITE "${WORK}/expect_${name}.cmake"
         "set(WORK \"${WORK}\")\n"
         "include(\"${ROOT}/tests/program_output.cmake\")\n"
         "program_expect(${name} ${call})\n")
    execute_process(COMMAND "${CMAKE_COMMAND}" -P "${WORK}/expect_${name}.cmake"
                    RESULT_VARIABLE status OUTPUT_VARIABLE out
                    ERROR_VARIABLE err ENCODING NONE)
    # CMake wraps the text of an error, so the lines are joined first.
    string(REGEX REPLACE "[ \n]+" " " err "${err}")
    string(FIND "${err}" "${refusal}" at)
    if(status EQUAL 0 OR at EQUAL -1)
        message(FATAL_ERROR "program_expect gave ${status} for ${name}, "
                            "expected `${refusal}`\n${out}${err}")
    endif()
endforeach()
file(CONFIGURE OUTPUT "${WORK}/expect_lf.txt" CONTENT "2\n" NEWLINE_STYLE UNIX)
program_expect(lf COMMAND "${CMAKE_COMMAND}" -E cat "${WORK}/expect_lf.txt"
               OUT "2\n")

# CMake decodes the output of execute_process on Windows by the console
# code page unless ENCODING NONE says otherwise. Every other call of a test
# script that captures output names it.
#
# A call that captures output in a variable runs a tool, named by the
# variable that holds it. A program the test built goes through
# program_expect or program_output of tests/program_output.cmake, which
# read its output from a file. ARGN and command stand for the helpers of
# a script that run a tool given as their arguments, and PROBE and RENAME
# for C tools of the harness.
set(tools ANTI ANTIC ARGN CC CLANG CMAKE_COMMAND GIT LLD_LINK LLVM_AR
          LLVM_BIN LLVM_DIR LLVM_MC LLVM_OBJDUMP OBJDUMP PROBE READOBJ RENAME
          command compiler copy git)
file(GLOB scripts "${ROOT}/tests/run_*.cmake")
set(missing "")
set(captured "")
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
        string(SUBSTRING "${text}" 0 ${start} before)
        string(REGEX MATCHALL "\n" lines "${before}")
        list(LENGTH lines line)
        math(EXPR line "${line} + 1")
        if(call MATCHES "(OUTPUT|ERROR)_VARIABLE" AND NOT call MATCHES "ENCODING NONE")
            list(APPEND missing "${name}:${line}")
        endif()
        if(call MATCHES "(OUTPUT|ERROR)_VARIABLE" AND
           call MATCHES "COMMAND[ \t\n]+\"?\\$\\{([A-Za-z_]+)\\}" AND
           NOT CMAKE_MATCH_1 IN_LIST tools)
            list(APPEND captured "${name}:${line}")
        endif()
        math(EXPR from "${i} + 1")
    endwhile()
endforeach()
if(missing)
    list(JOIN missing ", " missing)
    message(FATAL_ERROR "these calls capture output without ENCODING NONE, so "
                        "Windows decodes it through the console: ${missing}")
endif()
if(captured)
    list(JOIN captured ", " captured)
    message(FATAL_ERROR "these calls read the output of a program into a "
                        "variable, which loses a CR or a NUL: ${captured}")
endif()

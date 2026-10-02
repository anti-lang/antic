# Compile a program, run it and match its output against a pattern file,
# one regular expression per line. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime directory
#   SOURCE    the .anti file
#   OPTIONS   optional options of antic, separated by commas
#   STD_OBJECTS  optional dev objects of the modules of the standard
#             library that a dev build links, separated by commas
#   ARGS      optional arguments of the program, separated by commas
#   EXPECTED  the patterns of the standard output
#   ERRORS    optional patterns of the standard error, which is empty
#             without them
#   STATUS    the exit status, 0 when it is not given
#   WORK      a directory for the executable
#
# A trace holds addresses that differ from run to run, so the output is
# matched rather than compared. Each pattern matches a whole line, and
# @BUILD_ID@ stands for the build id in the notice of the program. A
# pattern that starts with "+ " matches one line or more, and one that
# starts with "* " matches any number of lines.

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

if(NOT DEFINED STATUS)
    set(STATUS 0)
endif()
get_filename_component(name "${SOURCE}" NAME_WE)
file(MAKE_DIRECTORY "${WORK}")
set(exe "${WORK}/${name}")
string(REPLACE "," ";" options "${OPTIONS}")
string(REPLACE "," ";" arguments "${ARGS}")
string(REPLACE "," ";" objects "${STD_OBJECTS}")
execute_process(
    COMMAND "${ANTIC}" --llvm-mc "${LLVM_MC}" --runtime "${RUNTIME}" ${options}
            -o "${exe}" "${SOURCE}" ${objects}
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
    ENCODING NONE)
if(NOT status EQUAL 0 OR NOT out STREQUAL "" OR NOT err STREQUAL "")
    message(FATAL_ERROR "antic failed with ${status}\n${out}${err}")
endif()
file(STRINGS "${exe}" ids REGEX "^build [0-9a-f]+$")
list(GET ids 0 id)
string(REPLACE "build " "" id "${id}")

program_expect("${name}" COMMAND "${exe}" ${arguments} STATUS ${STATUS}
               ANY_OUT ANY_ERR)
set(out "${program_stdout}")
set(err "${program_stderr}")

# Match the lines of text against the patterns of the file, and stop with
# what differs.
function(match_lines text file what)
    set(lines "")
    if(NOT text STREQUAL "")
        string(REGEX REPLACE "\n$" "" text "${text}")
        string(REPLACE ";" "\;" text "${text}")
        string(REPLACE "\n" ";" lines "${text}")
    endif()
    file(STRINGS "${file}" patterns)
    list(LENGTH lines count)
    set(at 0)
    foreach(pattern IN LISTS patterns)
        string(REPLACE "@BUILD_ID@" "${id}" pattern "${pattern}")
        set(least 1)
        set(most 1)
        if(pattern MATCHES "^\\+ ")
            string(SUBSTRING "${pattern}" 2 -1 pattern)
            set(most -1)
        elseif(pattern MATCHES "^\\* ")
            string(SUBSTRING "${pattern}" 2 -1 pattern)
            set(least 0)
            set(most -1)
        endif()
        set(taken 0)
        while(at LESS count AND (most EQUAL -1 OR taken LESS most))
            list(GET lines ${at} line)
            if(NOT line MATCHES "^${pattern}$")
                break()
            endif()
            math(EXPR at "${at} + 1")
            math(EXPR taken "${taken} + 1")
        endwhile()
        if(taken LESS least)
            message(FATAL_ERROR "line ${at} of the ${what} does not match "
                                "'${pattern}'\n${text}")
        endif()
    endforeach()
    if(at LESS count)
        message(FATAL_ERROR "the ${what} has more lines than ${file}\n${text}")
    endif()
endfunction()

match_lines("${out}" "${EXPECTED}" "output")
if(DEFINED ERRORS)
    match_lines("${err}" "${ERRORS}" "error output")
elseif(NOT err STREQUAL "")
    message(FATAL_ERROR "the program wrote to standard error\n${err}")
endif()

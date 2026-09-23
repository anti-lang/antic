# Run antic on a program it accepts and check every warning it must
# report. Run with cmake -P and these values:
#   ANTIC      the antic executable
#   DIRECTORY  the directory to run in, so that messages name the file alone
#   COMMAND    the arguments, separated by commas
#   NEEDLES    the messages the run must print, separated by semicolons
#   ABSENT     messages the run must not print, separated by semicolons

string(REPLACE "," ";" arguments "${COMMAND}")
execute_process(
    COMMAND "${ANTIC}" ${arguments}
    WORKING_DIRECTORY "${DIRECTORY}"
    RESULT_VARIABLE status
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
    ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic refused the program with ${status}\n${err}")
endif()
foreach(needle IN LISTS NEEDLES)
    string(FIND "${err}" "${needle}" at)
    if(at LESS 0)
        message(FATAL_ERROR "no `${needle}` in\n${err}")
    endif()
endforeach()
foreach(needle IN LISTS ABSENT)
    string(FIND "${err}" "${needle}" at)
    if(NOT at LESS 0)
        message(FATAL_ERROR "`${needle}` is in\n${err}")
    endif()
endforeach()

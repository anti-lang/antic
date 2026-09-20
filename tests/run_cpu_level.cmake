# Write the assembly of a program for one processor level and check that
# it holds the instructions of that level and none of a higher one. Run
# with cmake -P and these values:
#   ANTIC     the antic executable
#   TARGET    the antic target name
#   LEVEL     the --cpu value
#   SOURCE    the .anti file
#   WORK      a directory for the assembly
#   USES      patterns the assembly must hold, separated by |
#   AVOIDS    patterns it must not hold, separated by |
#
# A pattern carries the four spaces of an instruction line, so that
# "    addsd" does not match "    vaddsd".

get_filename_component(name "${SOURCE}" NAME_WE)
set(assembly "${WORK}/${name}.${TARGET}.${LEVEL}.s")
file(MAKE_DIRECTORY "${WORK}")
execute_process(
    COMMAND "${ANTIC}" -S --target "${TARGET}" --cpu "${LEVEL}"
            -o "${assembly}" "${SOURCE}"
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
    ENCODING NONE)
if(NOT status EQUAL 0 OR NOT out STREQUAL "" OR NOT err STREQUAL "")
    message(FATAL_ERROR "antic failed with ${status}\n${out}${err}")
endif()

file(READ "${assembly}" text)
string(REPLACE "|" ";" uses "${USES}")
string(REPLACE "|" ";" avoids "${AVOIDS}")
foreach(pattern IN LISTS uses)
    string(FIND "${text}" "${pattern}" at)
    if(at EQUAL -1)
        message(FATAL_ERROR
            "${TARGET} at ${LEVEL} writes no '${pattern}' in ${assembly}")
    endif()
endforeach()
foreach(pattern IN LISTS avoids)
    string(FIND "${text}" "${pattern}" at)
    if(NOT at EQUAL -1)
        message(FATAL_ERROR
            "${TARGET} at ${LEVEL} writes '${pattern}' in ${assembly}")
    endif()
endforeach()

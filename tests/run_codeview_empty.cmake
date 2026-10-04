# A module that writes no function, as the dev object of
# anti.collection.tree, whose functions are all generic, writes no
# CodeView subsection of symbols: lld-link warns of an empty one, and
# the dev programs that link the object then print a warning. Run with
# cmake -P and these values:
#   ANTIC   the antic executable
#   RUNTIME the runtime archive, whose bin/ holds llc
#   STD     the directory of the library files of the standard library
#   SOURCE  the library file of the module
#   TARGET  a Windows target
#   WORK    a directory for the assembly

file(MAKE_DIRECTORY "${WORK}")
execute_process(COMMAND "${ANTIC}" --runtime "${RUNTIME}" --dev
                        --target "${TARGET}" -S -I "${STD}" -o "${WORK}/empty.${TARGET}.s" "${SOURCE}"
                RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic failed with ${status}\n${err}")
endif()
file(READ "${WORK}/empty.${TARGET}.s" assembly)
string(FIND "${assembly}" "DEBUG_S_SYMBOLS" subsection)
string(FIND "${assembly}" "S_LPROC32" record)
if(NOT subsection EQUAL -1 AND record EQUAL -1)
    message(FATAL_ERROR "the assembly for ${TARGET} opens a subsection of "
                        "symbols that holds no record")
endif()

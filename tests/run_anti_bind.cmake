# The tests of anti bind. Run with cmake -P and these values:
#   ANTI      the anti executable
#   ANTIC     the antic executable
#   RUNTIME   the runtime directory
#   WORK      a directory for the output
#   CASE      header
#   SOURCES   tests/clib, for the case header
#   DUMP      tests/dump, the headers that antic --lib writes

function(run)
    execute_process(COMMAND ${ARGN}
        RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "${ARGN} failed with ${status}\n${out}${err}")
    endif()
endfunction()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

if(CASE STREQUAL "header")
    # The header of each library file equals the header that antic --lib
    # writes for the same module, which tests/dump holds.
    foreach(name geo shapes canvas failing tuples flags variants simdlib)
        run("${ANTIC}" -c --runtime "${RUNTIME}" -I "${SOURCES}"
            -o "${WORK}/${name}.antl" "${SOURCES}/com/example/${name}.anti")
        run("${ANTI}" bind --header "${WORK}/${name}.antl" -o "${WORK}/out"
            --runtime "${RUNTIME}" -I "${SOURCES}")
        file(READ "${WORK}/out/${name}.h" got)
        file(READ "${DUMP}/${name}.h" wanted)
        if(NOT got STREQUAL wanted)
            message(FATAL_ERROR "${name}.h differs from ${DUMP}/${name}.h\n${got}")
        endif()
    endforeach()
else()
    message(FATAL_ERROR "unknown case ${CASE}")
endif()

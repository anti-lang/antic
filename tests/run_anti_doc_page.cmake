# Write the page of one module with `anti doc` and compare it with a file.
# Run with cmake -P and these values:
#   ANTI      the anti executable
#   RUNTIME   the runtime archive, which holds the library file of anti.lang
#   ROOT      the search root of the sources
#   SOURCES   the sources of the run, with | between them. The page is
#             that of the first
#   PAGE      the name of the page anti doc writes for the first source
#   EXPECTED  the file that holds the page
#   OPTIONS   further options of anti doc, with | between them
#   WORK      a directory this run writes into
#
# DESIGN: tests/run_anti_doc.cmake holds the rules of `anti doc` over one
# fixture. This runner holds one page, so a module that shows the forms of
# an item needs no line there.

cmake_minimum_required(VERSION 3.21)

foreach(value ANTI RUNTIME ROOT SOURCES PAGE EXPECTED WORK)
    if(NOT DEFINED ${value})
        message(FATAL_ERROR "${value} is not set")
    endif()
endforeach()

string(REPLACE "|" ";" sources "${SOURCES}")
string(REPLACE "|" ";" options "${OPTIONS}")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
execute_process(
    COMMAND "${ANTI}" doc --runtime "${RUNTIME}" ${options} -I "${ROOT}"
            --work "${WORK}/work" -o "${WORK}/out" ${sources}
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT status EQUAL 0 OR NOT err STREQUAL "")
    message(FATAL_ERROR "anti doc ended with ${status}\n${out}${err}")
endif()
file(READ "${WORK}/out/${PAGE}" got)
file(READ "${EXPECTED}" wanted)
if(NOT got STREQUAL wanted)
    message(FATAL_ERROR "${PAGE} differs from ${EXPECTED}:\n${got}")
endif()

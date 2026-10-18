# The internal level of a module item. A module of the same package sees
# an internal function, a generic one included, and a module of another
# package does not. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   RUNTIME   the runtime directory
#   ROOT      the search root that holds the sources
#   WORK      a directory for the outputs

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

file(MAKE_DIRECTORY "${WORK}")
antic_build(-c -I "${ROOT}" --package-name com.example -o "${WORK}/core.antl"
            "${ROOT}/com/example/core.anti")
# A module of the same package compiles against it.
antic_build(-c -I "${ROOT}" --package-name com.example -o "${WORK}/near.antl"
            "${ROOT}/com/example/near.anti" "${WORK}/core.antl")
# A module of another package does not.
antic_run(status ignored_out err COMMAND -S -I "${ROOT}" -o "${WORK}/far.s"
          "${ROOT}/far.anti" "${WORK}/core.antl")
if(status EQUAL 0)
    message(FATAL_ERROR "another package reached the internal item")
endif()
if(NOT err MATCHES "has no public item `tuning`")
    message(FATAL_ERROR "unexpected message\n${err}")
endif()

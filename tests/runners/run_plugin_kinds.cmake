# Build a host and a plugin whose interface takes and gives every kind of
# value, and run the host. The library is compiled with --lib shared
# --no-runtime and is bound against the host that loads it. A Windows
# library links against the import library of its host. Run with cmake -P
# and these values:
#   ANTIC     the antic executable
#   RUNTIME   the runtime directory
#   SOURCES   tests/plugin
#   SUFFIX    the suffix of a shared library on this host
#   EXE       the suffix of an executable on this host
#   WORK      a directory for the objects and the executables
#   HOST      the name of the host's source in SOURCES, without .anti
#   LIBRARY   the module of the plugin under net/example
#   INTERFACE the module of the interface under net/example
#   EXPECTED  the output of the host
#   FAULT     optional value of ANTI_FAULT for the run of the host, for a
#             case of fault injection against the runtime of
#             runtime-faults/, see src/rt/fault.h. RUNTIME then names that
#             directory, the host builds with --lto none, and EXPECTED
#             starts with the line "exit N" or "abort", as the expected
#             file of tests/runners/run_program.cmake does
#   ERRORS    optional file of the whole standard error of the host in a
#             case of FAULT, which is otherwise not compared

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/net/example" "${WORK}/lib")

# The interface is a library file that both sides import.
antic_build(-c -I "${SOURCES}" --runtime "${RUNTIME}" -o "${WORK}/net/example/${INTERFACE}.antl"
            "${SOURCES}/net/example/${INTERFACE}.anti")
set(options "")
if(DEFINED FAULT)
    set(options --lto none)
endif()
antic_build(--runtime "${RUNTIME}" ${options} -I "${SOURCES}"
            -I "${WORK}" -o "${WORK}/${HOST}${EXE}" "${SOURCES}/${HOST}.anti")
set(import_library "")
if(SUFFIX STREQUAL ".dll")
    set(import_library "${WORK}/${HOST}.lib")
endif()
set(library "${WORK}/lib/lib${LIBRARY}${SUFFIX}")
antic_build(--lib shared --no-runtime --runtime "${RUNTIME}"
            -I "${SOURCES}" -I "${WORK}" -o "${library}"
            "${SOURCES}/net/example/${LIBRARY}.anti" ${import_library})
if(NOT DEFINED FAULT)
    program_expect("the host" COMMAND "${WORK}/${HOST}${EXE}" "${library}"
                   OUT_FILE "${EXPECTED}")
    return()
endif()
# DESIGN: the variable reaches the host from the environment of this
# script, set after the builds, as in run_program.cmake, so that the
# abort of a case of memory that runs out stays an abort.
set(ENV{ANTI_FAULT} "${FAULT}")
set(errors ANY_ERR)
if(DEFINED ERRORS)
    set(errors ERR_FILE "${ERRORS}")
endif()
program_expect("the host" COMMAND "${WORK}/${HOST}${EXE}" "${library}"
               EXPECTED "${EXPECTED}" ${errors})

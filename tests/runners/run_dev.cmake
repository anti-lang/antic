# Build a program of three modules in dev mode: each library module into
# its own object, then the main module, linked with those objects. Run the
# result and check its exit status. Then build the library modules again
# from their library files, compare the LLVM text of each with the one
# from its source, and link and run that program too. Run with cmake -P
# and these values:
#   ANTIC     the antic executable
#   RUNTIME   the runtime directory
#   MODULES   the search root of the sources
#   LIBS      the search root of the library files
#   WORK      a directory for the objects and the executable
#   OBJECT    the suffix antic gives an object of the host, .o or .obj

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

file(MAKE_DIRECTORY "${WORK}")
foreach(module scale twice)
    antic_build(--dev --runtime "${RUNTIME}" --keep-llvm
                -I "${MODULES}" -I "${LIBS}" -o "${WORK}/${module}"
                "${MODULES}/com/example/${module}.anti")
endforeach()
antic_build(--dev --runtime "${RUNTIME}" -I "${LIBS}" -o
            "${WORK}/main" "${MODULES}/main.anti" "${WORK}/scale${OBJECT}"
            "${WORK}/twice${OBJECT}")
execute_process(COMMAND "${WORK}/main" RESULT_VARIABLE status)
if(NOT status EQUAL 42)
    message(FATAL_ERROR "the program exits with ${status}, expected 42")
endif()

foreach(module scale twice)
    antic_build(--dev --runtime "${RUNTIME}" --keep-llvm
                -I "${LIBS}" -o "${WORK}/${module}_antl"
                "${LIBS}/com/example/${module}.antl")
    # The text of --keep-llvm stands beside the object.
    set(text .ll)
    file(READ "${WORK}/${module}${text}" from_source)
    file(READ "${WORK}/${module}_antl${text}" from_library)
    expect_same_text("${module}_antl${text}, against ${module}${text},"
                     "${from_source}" "${from_library}")
endforeach()
antic_build(--dev --runtime "${RUNTIME}" -I "${LIBS}" -o
            "${WORK}/main_antl" "${MODULES}/main.anti"
            "${WORK}/scale_antl${OBJECT}" "${WORK}/twice_antl${OBJECT}")
execute_process(COMMAND "${WORK}/main_antl" RESULT_VARIABLE status)
if(NOT status EQUAL 42)
    message(FATAL_ERROR "the program from library objects exits with ${status}, expected 42")
endif()

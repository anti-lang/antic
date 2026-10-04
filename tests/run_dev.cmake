# Build a program of three modules in dev mode: each library module into
# its own object, then the main module, linked with those objects. Run the
# result and check its exit status. Then build the library modules again
# from their library files, which gives the same assembly, and link and
# run that program too. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime directory
#   MODULES   the search root of the sources
#   LIBS      the search root of the library files
#   WORK      a directory for the objects and the executable
#   OBJECT    the suffix antic gives an object of the host, .o or .obj

file(MAKE_DIRECTORY "${WORK}")
foreach(module scale twice)
    execute_process(
        COMMAND "${ANTIC}" --dev --llvm-mc "${LLVM_MC}" --runtime "${RUNTIME}"
                --keep-llvm -I "${MODULES}" -I "${LIBS}"
                -o "${WORK}/${module}"
                "${MODULES}/com/example/${module}.anti"
        RESULT_VARIABLE status
        ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "antic --dev ${module}.anti failed\n${err}")
    endif()
endforeach()
execute_process(
    COMMAND "${ANTIC}" --dev --llvm-mc "${LLVM_MC}" --runtime "${RUNTIME}"
            -I "${LIBS}" -o "${WORK}/main" "${MODULES}/main.anti"
            "${WORK}/scale${OBJECT}" "${WORK}/twice${OBJECT}"
    RESULT_VARIABLE status
    ERROR_VARIABLE err
    ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic --dev main.anti failed\n${err}")
endif()
execute_process(COMMAND "${WORK}/main" RESULT_VARIABLE status)
if(NOT status EQUAL 42)
    message(FATAL_ERROR "the program exits with ${status}, expected 42")
endif()

foreach(module scale twice)
    execute_process(
        COMMAND "${ANTIC}" --dev --llvm-mc "${LLVM_MC}" --runtime "${RUNTIME}"
                --keep-llvm -I "${LIBS}" -o "${WORK}/${module}_antl"
                "${LIBS}/com/example/${module}.antl"
        RESULT_VARIABLE status
        ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "antic --dev ${module}.antl failed\n${err}")
    endif()
    # The text of --keep-llvm stands beside the object.
    set(text .ll)
    file(READ "${WORK}/${module}${text}" from_source)
    file(READ "${WORK}/${module}_antl${text}" from_library)
    if(NOT from_source STREQUAL from_library)
        message(FATAL_ERROR
            "${module}_antl${text} differs from ${module}${text}")
    endif()
endforeach()
execute_process(
    COMMAND "${ANTIC}" --dev --llvm-mc "${LLVM_MC}" --runtime "${RUNTIME}"
            -I "${LIBS}" -o "${WORK}/main_antl" "${MODULES}/main.anti"
            "${WORK}/scale_antl${OBJECT}" "${WORK}/twice_antl${OBJECT}"
    RESULT_VARIABLE status
    ERROR_VARIABLE err
    ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic --dev main.anti with library objects failed\n${err}")
endif()
execute_process(COMMAND "${WORK}/main_antl" RESULT_VARIABLE status)
if(NOT status EQUAL 42)
    message(FATAL_ERROR "the program from library objects exits with ${status}, expected 42")
endif()

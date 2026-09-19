# Build a library module and a program that tests its class, then run the
# program. Release mode links the library file with the program. Dev mode
# compiles each module to its own object and links the two.
# Run with cmake -P and these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime directory
#   ROOT      the search root that holds the sources
#   MODE      release or dev
#   WORK      a directory for the outputs

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

file(MAKE_DIRECTORY "${WORK}/com/example")
execute_process(
    COMMAND "${ANTIC}" -c -I "${ROOT}" -o "${WORK}/com/example/points.antl"
            "${ROOT}/com/example/points.anti"
    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR
        "antic -c of the library failed with ${status}\n${err}")
endif()
if(MODE STREQUAL "dev")
    execute_process(
        COMMAND "${ANTIC}" --dev --llvm-mc "${LLVM_MC}" -I "${ROOT}"
                -I "${WORK}" -o "${WORK}/points"
                "${ROOT}/com/example/points.anti"
        RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR
            "antic --dev of the library failed with ${status}\n${err}")
    endif()
    # No tool builds the objects of the standard library in dev mode yet,
    # so the test builds one for each module that the program reaches.
    set(program_inputs --dev "${ROOT}/client.anti" "${WORK}/points.o")
    foreach(module text reflect)
        execute_process(
            COMMAND "${ANTIC}" --dev --llvm-mc "${LLVM_MC}"
                    --runtime "${RUNTIME}" -o "${WORK}/std_${module}"
                    "${RUNTIME}/std/anti/${module}.antl"
            RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
        if(NOT status EQUAL 0)
            message(FATAL_ERROR
                "antic --dev of anti.${module} failed with ${status}\n${err}")
        endif()
        list(APPEND program_inputs "${WORK}/std_${module}.o")
    endforeach()
else()
    set(program_inputs "${ROOT}/client.anti"
        "${WORK}/com/example/points.antl")
endif()
execute_process(
    COMMAND "${ANTIC}" -I "${WORK}" --llvm-mc "${LLVM_MC}"
            --runtime "${RUNTIME}" -o "${WORK}/client" ${program_inputs}
    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic of the program failed with ${status}\n${err}")
endif()
program_output(out_hex status "${WORK}/client.stdout" "${WORK}/client")
file(READ "${WORK}/client.stdout" out)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "the program exited with ${status}\n${out}")
endif()
string(HEX "1 1\n1 0 1\n1 1\n" wanted)
if(NOT out_hex STREQUAL wanted)
    message(FATAL_ERROR "the program printed\n${out}")
endif()

# Build library modules and a program that uses them, then run the program
# and compare its output with <PROGRAM>.expected beside the source. Release
# mode links the library files with the program. Dev mode compiles each
# module to its own object and links the objects. Run with cmake -P and
# these values:
#   ANTIC      the antic executable
#   LLVM_MC    the llvm-mc executable
#   RUNTIME    the runtime directory
#   ROOT       the search root that holds the sources
#   LIBRARIES  the library modules under com/example, separated by commas,
#              each after the ones it imports
#   STD_OBJECTS  the dev objects of the modules of the standard library
#              that the program reaches, separated by commas, which the dev
#              link takes
#   PROGRAM    the name of the program's source in ROOT, without .anti
#   MODE       release or dev
#   WORK       a directory for the outputs
#   OPTIONS    optional options of antic for the program, separated by
#              commas
#   LIB_OPTIONS  optional options of antic for the library modules
#   OBJECT     the suffix antic gives an object of the host, .o or .obj

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

string(REPLACE "," ";" libraries "${LIBRARIES}")
string(REPLACE "," ";" std_objects "${STD_OBJECTS}")
string(REPLACE "," ";" options "${OPTIONS}")
string(REPLACE "," ";" lib_options "${LIB_OPTIONS}")
file(MAKE_DIRECTORY "${WORK}/com/example")
set(program_inputs "${ROOT}/${PROGRAM}.anti")
if(MODE STREQUAL "dev")
    set(program_inputs --dev "${ROOT}/${PROGRAM}.anti")
endif()
foreach(library IN LISTS libraries)
    set(source "${ROOT}/com/example/${library}.anti")
    execute_process(
        COMMAND "${ANTIC}" -c -I "${ROOT}" -I "${WORK}"
                --runtime "${RUNTIME}" ${lib_options}
                -o "${WORK}/com/example/${library}.antl" "${source}"
        RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR
            "antic -c of ${library} failed with ${status}\n${err}")
    endif()
    if(MODE STREQUAL "dev")
        execute_process(
            COMMAND "${ANTIC}" --dev --llvm-mc "${LLVM_MC}" -I "${ROOT}"
                    -I "${WORK}" --runtime "${RUNTIME}"
                    -o "${WORK}/${library}" "${source}"
            RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
        if(NOT status EQUAL 0)
            message(FATAL_ERROR
                "antic --dev of ${library} failed with ${status}\n${err}")
        endif()
        list(APPEND program_inputs "${WORK}/${library}${OBJECT}")
    else()
        list(APPEND program_inputs "${WORK}/com/example/${library}.antl")
    endif()
endforeach()
if(MODE STREQUAL "dev")
    list(APPEND program_inputs ${std_objects})
endif()
execute_process(
    COMMAND "${ANTIC}" -I "${WORK}" --llvm-mc "${LLVM_MC}"
            --runtime "${RUNTIME}" ${options}
            -o "${WORK}/${PROGRAM}" ${program_inputs}
    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic of the program failed with ${status}\n${err}")
endif()
program_output(out_hex status "${WORK}/${PROGRAM}.stdout"
               "${WORK}/${PROGRAM}")
file(READ "${WORK}/${PROGRAM}.stdout" out)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "the program exited with ${status}\n${out}")
endif()
file(READ "${ROOT}/${PROGRAM}.expected" wanted)
string(HEX "${wanted}" wanted_hex)
if(NOT out_hex STREQUAL wanted_hex)
    message(FATAL_ERROR "the program printed\n${out}")
endif()

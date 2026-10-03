# Build one test program through the LLVM back end and compare its result
# with the expected file beside the source, as run_program.cmake does for
# the native back end. antic runs opt and llc of the runtime archive and
# links the object, as "Integration route" of
# docs/work-order-llvm-back-end.md gives it, and no step runs by hand. The
# text and the bitcode beside the output are gone afterwards, since no
# --keep-llvm asks for them. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   RUNTIME   the runtime directory, holding bin/, lib/<target>/<level>/
#             and sysroot/<target>/
#   SOURCE    the .anti file
#   TARGET_NAME  macos-arm64 or macos-x86_64, the targets a Mac runs
#   MODE      release or dev
#   WORK      a directory for the objects and the executable
#   OPTIONS   optional options of antic, separated by commas
#   EXPECTED  optional expected file, instead of NAME.expected beside the
#             source
#   OBJECTS   optional objects of C that the program links, separated by
#             commas

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

get_filename_component(name "${SOURCE}" NAME_WE)
get_filename_component(dir "${SOURCE}" DIRECTORY)
set(expected_file "${dir}/${name}.expected")
if(DEFINED EXPECTED)
    set(expected_file "${EXPECTED}")
endif()
set(base "${WORK}/${name}.${MODE}.${TARGET_NAME}")
file(MAKE_DIRECTORY "${WORK}")
file(REMOVE "${base}" "${base}.ll" "${base}.bc")

string(REPLACE "," ";" options "${OPTIONS}")
string(REPLACE "," ";" objects "${OBJECTS}")
set(mode_options "")
if(MODE STREQUAL "dev")
    set(mode_options --dev)
endif()
execute_process(
    COMMAND "${ANTIC}" --backend llvm --runtime "${RUNTIME}"
            --target "${TARGET_NAME}" ${mode_options} ${options}
            -o "${base}" "${SOURCE}" ${objects}
    RESULT_VARIABLE status
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
    ENCODING NONE)
if(NOT status EQUAL 0 OR NOT out STREQUAL "" OR NOT err STREQUAL "")
    message(FATAL_ERROR "antic --backend llvm gave ${status} for ${base}\n${out}${err}")
endif()
foreach(intermediate "${base}.ll" "${base}.bc")
    if(EXISTS "${intermediate}")
        message(FATAL_ERROR "${intermediate} stays without --keep-llvm")
    endif()
endforeach()

set(program_args "")
if(EXISTS "${dir}/${name}.args")
    file(STRINGS "${dir}/${name}.args" program_args ENCODING UTF-8)
endif()
program_expect("${name}" COMMAND "${base}" ${program_args}
               EXPECTED "${expected_file}" ANY_ERR)

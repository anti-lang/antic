# Link a test program for TARGET against a native library of the runtime
# tree, with antic and the linker a program of that target uses. A symbol
# the library lacks, or one it cannot resolve in the sysroot of the
# target, fails the link. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime directory
#   SOURCE    the .anti file
#   OBJECTS   the objects and libraries to link, separated by commas
#   WORK      a directory for the executable
#   TARGET    the target name

file(MAKE_DIRECTORY "${WORK}")
get_filename_component(program "${SOURCE}" NAME_WE)
set(exe "${WORK}/${program}-${TARGET}")
string(REPLACE "," ";" objects "${OBJECTS}")
file(REMOVE "${exe}")
execute_process(
    COMMAND "${ANTIC}" --target "${TARGET}" --llvm-mc "${LLVM_MC}"
            --runtime "${RUNTIME}" -o "${exe}" "${SOURCE}" ${objects}
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
    ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic failed for ${TARGET}\n${out}${err}")
endif()
# Warnings are errors: antic, llvm-mc and the linker print nothing.
if(NOT out STREQUAL "" OR NOT err STREQUAL "")
    message(FATAL_ERROR "antic printed output for ${TARGET}\n${out}${err}")
endif()
if(NOT EXISTS "${exe}")
    message(FATAL_ERROR "antic wrote no executable for ${TARGET}")
endif()

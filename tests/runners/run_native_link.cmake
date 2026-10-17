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
#   OPTIONS   optional options of antic, separated by commas

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

file(MAKE_DIRECTORY "${WORK}")
get_filename_component(program "${SOURCE}" NAME_WE)
set(exe "${WORK}/${program}-${TARGET}")
string(REPLACE "," ";" objects "${OBJECTS}")
set(options "")
if(DEFINED OPTIONS)
    string(REPLACE "," ";" options "${OPTIONS}")
endif()
file(REMOVE "${exe}")
antic_build(--target "${TARGET}" --llvm-mc "${LLVM_MC}" --runtime "${RUNTIME}"
            ${options} -o "${exe}" "${SOURCE}" ${objects})
if(NOT EXISTS "${exe}")
    message(FATAL_ERROR "antic wrote no executable for ${TARGET}")
endif()

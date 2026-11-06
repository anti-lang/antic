# Link the programs of anti.graphics for TARGET with antic and no option
# beyond the target and the runtime archive: the first program of
# docs/notes/graphics.md and the Pythagoras tree of its sketch. Each
# imports anti.graphics, whose import of anti.raylib takes libraylib,
# libminiaudio and the system libraries of raylib into the link. The suite
# opens no window, so no program runs. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   RUNTIME   the runtime directory
#   SOURCES   the programs, separated by commas
#   TARGET    the target name
#   WORK      a directory for the executables

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
string(REPLACE "," ";" sources "${SOURCES}")
foreach(source IN LISTS sources)
    get_filename_component(name "${source}" NAME_WE)
    set(exe "${WORK}/${name}-${TARGET}")
    if("${TARGET}" MATCHES "^windows-")
        string(APPEND exe ".exe")
    endif()
    antic_build(--target "${TARGET}" --runtime "${RUNTIME}" -o "${exe}"
                "${source}")
    if(NOT EXISTS "${exe}")
        message(FATAL_ERROR "antic wrote no executable of ${name} for ${TARGET}")
    endif()
endforeach()

# Link tests/std/raylib.anti for TARGET with antic and no option beyond the
# target and the runtime archive, and read what the executable loads. The
# program imports anti.raylib, so the link takes libraylib and libminiaudio
# of lib/<target>/ and the system libraries of raylib by itself. A Linux
# program links dynamically against glibc and loads libX11, a macOS one
# loads the frameworks Cocoa and IOKit, and a Windows one imports
# user32.dll and gdi32.dll. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   RUNTIME   the runtime directory
#   SOURCE    tests/std/raylib.anti
#   OBJDUMP   llvm-objdump of the runtime archive
#   READOBJ   llvm-readobj of the runtime archive
#   TARGET    the target name
#   WORK      a directory for the executable

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(exe "${WORK}/raylib-${TARGET}")
if("${TARGET}" MATCHES "^windows-")
    string(APPEND exe ".exe")
endif()
antic_build(--target "${TARGET}" --runtime "${RUNTIME}" -o "${exe}"
            "${SOURCE}")
if(NOT EXISTS "${exe}")
    message(FATAL_ERROR "antic wrote no executable for ${TARGET}")
endif()

if("${TARGET}" MATCHES "^linux-")
    set(command "${READOBJ}" --needed-libs --program-headers "${exe}")
    set(wanted pt_interp libx11\\.so\\.6 libm\\.so\\.6 libc\\.so\\.6)
elseif("${TARGET}" MATCHES "^macos-")
    set(command "${OBJDUMP}" --macho --dylibs-used "${exe}")
    set(wanted cocoa\\.framework iokit\\.framework)
else()
    set(command "${READOBJ}" --coff-imports "${exe}")
    set(wanted user32\\.dll gdi32\\.dll)
endif()
execute_process(COMMAND ${command}
    RESULT_VARIABLE status OUTPUT_VARIABLE text ERROR_VARIABLE err
    ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "${command} failed with ${status}\n${text}${err}")
endif()
# The import libraries of mingw-w64 spell the names of the DLLs in capitals.
string(TOLOWER "${text}" lower)
foreach(name IN LISTS wanted)
    if(NOT lower MATCHES "${name}")
        message(FATAL_ERROR "${exe} lacks ${name}\n${text}")
    endif()
endforeach()

# Build tests/anti-symbols/tracer with `anti build --release` for a Linux
# TARGET and resolve a trace of it through an archive that holds the debug
# twin alone. With no map to read, `anti symbols resolve` takes the names
# and the lines from the ELF program, syms_resolve_binary of syms.c, on
# every host. Run with cmake -P and these values:
#   ANTI      the anti executable
#   RUNTIME   the runtime directory
#   FIXTURE   the directory that holds tracer/
#   TARGET    the Linux target to build for
#   WORK      a directory this run writes into
#
# The map of the archive that the build writes gives the build id and the
# address of `main`, into which the release link inlines the other
# functions of the program. The frame lies 4 bytes into it, at a module
# loaded at address 0, and resolves to the name of the function and a line
# of tracer.anti.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

if(NOT EXISTS "${RUNTIME}/sysroot/${TARGET}")
    message("SKIP: the runtime archive has no sysroot for ${TARGET}")
    return()
endif()
set(name tracer)
set(project "${WORK}/${name}")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/archive")
file(COPY "${FIXTURE}/${name}" DESTINATION "${WORK}")
anti_run(status out err "${project}" build --release --target "${TARGET}"
         --runtime "${RUNTIME}")
if(NOT status EQUAL 0)
    message(FATAL_ERROR "anti build --release failed with ${status}\n${out}${err}")
endif()

set(built "${project}/dist/${TARGET}/release/${name}-symbols.zip")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf "${built}"
                WORKING_DIRECTORY "${WORK}/archive" RESULT_VARIABLE status)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "${built} was not unpacked")
endif()
file(READ "${WORK}/archive/${name}.map" map)
if(NOT map MATCHES "\n# build ([0-9a-f]+)\n")
    message(FATAL_ERROR "the map names no build id\n${map}")
endif()
set(id "${CMAKE_MATCH_1}")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar cf "${WORK}/twin.zip"
                        --format=zip ${name}.debug
                WORKING_DIRECTORY "${WORK}/archive" RESULT_VARIABLE status)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "the archive of the twin was not written")
endif()

# The address 4 bytes into the function of the line of the map that names
# it, as 16 hexadecimal digits.
function(frame_of out function)
    string(REPLACE "." "\\." pattern "${function}")
    if(NOT map MATCHES "\n([0-9a-f]+)-[0-9a-f]+ ${pattern}[ \n]")
        message(FATAL_ERROR "the map names no ${function}\n${map}")
    endif()
    math(EXPR address "0x${CMAKE_MATCH_1} + 4" OUTPUT_FORMAT HEXADECIMAL)
    string(REGEX REPLACE "^0x" "" address "${address}")
    string(LENGTH "${address}" length)
    while(length LESS 16)
        string(PREPEND address "0")
        math(EXPR length "${length} + 1")
    endwhile()
    set(${out} "${address}" PARENT_SCOPE)
endfunction()

set(functions "com.example.tracer.main")
set(trace "module 0 ${id} 0x0000000000000000 /opt/${name}\n")
foreach(function IN LISTS functions)
    frame_of(address "${function}")
    string(REGEX REPLACE "^0+" "" offset "${address}")
    string(APPEND trace "0x${address} 0+0x${offset}\n")
endforeach()
file(WRITE "${WORK}/trace.txt" "${trace}")
anti_run(status out err "${WORK}" symbols resolve trace.txt
         --symbols twin.zip)
if(NOT status EQUAL 0 OR NOT err STREQUAL "")
    message(FATAL_ERROR "resolve gave ${status}\n${out}${err}")
endif()
string(REGEX MATCHALL "[^\n]+" lines "${out}")
list(LENGTH lines count)
if(NOT count EQUAL 2)
    message(FATAL_ERROR "resolve gave other lines\n${trace}\n${out}")
endif()
set(i 1)
foreach(function IN LISTS functions)
    list(GET lines ${i} line)
    string(REPLACE "." "\\." pattern "${function}")
    if(NOT line MATCHES " 0\\+0x[0-9a-f]+ ${pattern} ${name}\\.anti:[0-9]+$")
        message(FATAL_ERROR "the frame of ${function} resolved as '${line}'\n${out}")
    endif()
    math(EXPR i "${i} + 1")
endforeach()

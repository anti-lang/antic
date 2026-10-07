# Build tests/anti-symbols/tracer with `anti build --release` for TARGET
# and compare the sections of the program in dist/ with those of the link
# that its symbols archive holds. The archive answers for the program only
# when both hold the same code at the same addresses, so every section
# that the program loads must stand at the same address with the same
# size in both. Run with cmake -P and these values:
#   ANTI      the anti executable
#   RUNTIME   the runtime directory
#   OBJDUMP   the llvm-objdump of the runtime directory
#   FIXTURE   the directory that holds tracer/
#   TARGET    the target to build for
#   WORK      a directory this run writes into

cmake_minimum_required(VERSION 3.21)

if(NOT EXISTS "${RUNTIME}/sysroot/${TARGET}")
    message("SKIP: the runtime archive has no sysroot for ${TARGET}")
    return()
endif()
set(project "${WORK}/tracer")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(COPY "${FIXTURE}/tracer" DESTINATION "${WORK}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "XDG_CACHE_HOME=${WORK}/cache"
            "LOCALAPPDATA=${WORK}/cache"
            "${ANTI}" build --release --target "${TARGET}"
            --runtime "${RUNTIME}"
    WORKING_DIRECTORY "${project}"
    RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
    ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "anti build --release failed with ${status}\n${out}${err}")
endif()

# The name, the size and the address of each section that has an address.
function(loaded out file)
    execute_process(COMMAND "${OBJDUMP}" -h "${file}"
        RESULT_VARIABLE status OUTPUT_VARIABLE listing ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "llvm-objdump failed on ${file}\n${err}")
    endif()
    string(REGEX MATCHALL "\n *[0-9]+ [^ \n]+ +[0-9a-f]+ [0-9a-f]+" rows
           "${listing}")
    set(sections "")
    foreach(row IN LISTS rows)
        string(REGEX REPLACE "^\n *[0-9]+ " "" row "${row}")
        string(REGEX REPLACE " +" " " row "${row}")
        if(NOT row MATCHES " 0+$")
            list(APPEND sections "${row}")
        endif()
    endforeach()
    set(${out} "${sections}" PARENT_SCOPE)
endfunction()

set(program "${project}/dist/${TARGET}/release/tracer")
set(debug "${project}/build/${TARGET}/release/tracer.debug")
loaded(shipped "${program}")
loaded(symbols "${debug}")
if(shipped STREQUAL "")
    message(FATAL_ERROR "${program} lists no section with an address")
endif()
if(NOT shipped STREQUAL symbols)
    string(REPLACE ";" "\n" shipped "${shipped}")
    string(REPLACE ";" "\n" symbols "${symbols}")
    message(FATAL_ERROR "the link of the symbols archive lays the program "
                        "out otherwise.\nshipped:\n${shipped}\n"
                        "archive:\n${symbols}")
endif()

# Build tests/anti-symbols/moved with `anti build --release` for TARGET
# and compare every section that the program in dist/ loads with the same
# section of the link that its symbols archive holds: the name, the
# address, the size and every byte. The archive answers for the program
# only when the two hold the same code and data, so a release program is
# the one link of the archive with its debug information stripped, by Q1
# of "Answers of 2026-11-07, fourth set". Run with cmake -P and these
# values:
#   ANTI      the anti executable
#   RUNTIME   the runtime directory
#   OBJDUMP   the llvm-objdump of the runtime directory
#   FIXTURE   tests/anti-symbols
#   TARGET    the target to build for
#   WORK      a directory this run writes into

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

if(NOT EXISTS "${RUNTIME}/sysroot/${TARGET}")
    message("SKIP: the runtime archive has no sysroot for ${TARGET}")
    return()
endif()
set(name moved)
set(project "${WORK}/${name}")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(COPY "${FIXTURE}/${name}" DESTINATION "${WORK}")
anti_run(status out err "${project}" build --release --target "${TARGET}"
         --runtime "${RUNTIME}")
if(NOT status EQUAL 0)
    message(FATAL_ERROR "anti build --release failed with ${status}\n${out}${err}")
endif()

# The sections of file that have an address, as `name size address`, and
# the bytes of each of them as llvm-objdump -s prints them. A Mach-O file
# names a section after its segment there, so the name is the part after
# the comma.
function(loaded out_list out_bytes file)
    execute_process(COMMAND "${OBJDUMP}" -h "${file}"
        RESULT_VARIABLE status OUTPUT_VARIABLE listing ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "llvm-objdump -h failed on ${file}\n${err}")
    endif()
    string(REGEX MATCHALL "\n *[0-9]+ [^ \n]+ +[0-9a-f]+ [0-9a-f]+" rows
           "${listing}")
    set(sections "")
    set(names "")
    foreach(row IN LISTS rows)
        string(REGEX REPLACE "^\n *[0-9]+ " "" row "${row}")
        string(REGEX REPLACE " +" " " row "${row}")
        if(NOT row MATCHES " 0+$")
            list(APPEND sections "${row}")
            string(REGEX REPLACE " .*$" "" section "${row}")
            list(APPEND names "${section}")
        endif()
    endforeach()
    execute_process(COMMAND "${OBJDUMP}" -s "${file}"
        RESULT_VARIABLE status OUTPUT_VARIABLE dump ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "llvm-objdump -s failed on ${file}\n${err}")
    endif()
    # The text column of the dump may hold any byte. A CMake list reads a
    # backslash, a semicolon and a bracket there, and the skipped range of
    # a bss section opens a bracket that nothing closes, so each of the
    # four becomes a word before the dump is cut into blocks.
    string(REPLACE "\\" "<backslash>" dump "${dump}")
    string(REPLACE ";" "<semicolon>" dump "${dump}")
    string(REPLACE "[" "<open>" dump "${dump}")
    string(REPLACE "]" "<close>" dump "${dump}")
    string(REPLACE "\nContents of section " ";" blocks "${dump}")
    set(bytes "")
    foreach(block IN LISTS blocks)
        if(NOT block MATCHES "^([^\n]+):\n")
            continue()
        endif()
        string(REGEX REPLACE "^.*," "" section "${CMAKE_MATCH_1}")
        list(FIND names "${section}" at)
        if(NOT at EQUAL -1)
            string(APPEND bytes "${block}\n")
        endif()
    endforeach()
    set(${out_list} "${sections}" PARENT_SCOPE)
    set(${out_bytes} "${bytes}" PARENT_SCOPE)
endfunction()

set(suffix "")
if("${TARGET}" MATCHES "^windows-")
    set(suffix ".exe")
endif()
set(program "${project}/dist/${TARGET}/release/${name}${suffix}")
set(debug "${project}/build/${TARGET}/release/${name}.debug${suffix}")
loaded(shipped shipped_bytes "${program}")
loaded(symbols symbols_bytes "${debug}")
if(shipped STREQUAL "" OR shipped_bytes STREQUAL "")
    message(FATAL_ERROR "${program} lists no section with an address")
endif()
if(NOT shipped STREQUAL symbols)
    string(REPLACE ";" "\n" shipped "${shipped}")
    string(REPLACE ";" "\n" symbols "${symbols}")
    message(FATAL_ERROR "the link of the symbols archive lays the program "
                        "out otherwise.\nshipped:\n${shipped}\n"
                        "archive:\n${symbols}")
endif()
if(NOT shipped_bytes STREQUAL symbols_bytes)
    file(WRITE "${WORK}/shipped.txt" "${shipped_bytes}")
    file(WRITE "${WORK}/archive.txt" "${symbols_bytes}")
    message(FATAL_ERROR "the link of the symbols archive holds other bytes "
                        "than the program. ${WORK}/shipped.txt and "
                        "${WORK}/archive.txt hold both.")
endif()
message("${TARGET}: the sections of the program equal those of the link "
        "of its symbols archive:\n${shipped}")

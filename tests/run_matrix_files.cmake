# Every file of tests/matrix registers a test. tests/CMakeLists.txt reads
# the directory and registers a program for <name>.anti with
# <name>.expected and a refusal for <name>.anti with <name>.err, so a
# source without either, or with both, would check nothing and the suite
# would pass without it. Run with cmake -P and this value:
#   MATRIX  the directory tests/matrix
#
# The test fails when
#   - a file is no <name>.anti, .expected, .err or .args, with a name of
#     small letters, digits and `_`
#   - a source has no .expected and no .err, or has both
#   - an .expected, an .err or an .args has no source
#   - an .args stands beside a refusal, which takes no arguments

cmake_minimum_required(VERSION 3.21)

file(GLOB files RELATIVE "${MATRIX}" "${MATRIX}/*" "${MATRIX}/.*")
set(faults "")
set(programs 0)
set(refusals 0)
foreach(file IN LISTS files)
    if(IS_DIRECTORY "${MATRIX}/${file}")
        if(NOT file MATCHES "^\\.\\.?$")
            string(APPEND faults "${file}: a directory\n")
        endif()
        continue()
    endif()
    if(NOT file MATCHES "^([a-z0-9_]+)\\.(anti|expected|err|args)$")
        string(APPEND faults "${file}: no <name>.anti, .expected, .err or "
                             ".args with a name of small letters, digits and `_`\n")
        continue()
    endif()
    set(name "${CMAKE_MATCH_1}")
    set(kind "${CMAKE_MATCH_2}")
    set(expected OFF)
    set(refused OFF)
    if(EXISTS "${MATRIX}/${name}.expected")
        set(expected ON)
    endif()
    if(EXISTS "${MATRIX}/${name}.err")
        set(refused ON)
    endif()
    if(kind STREQUAL "anti")
        if(expected AND refused)
            string(APPEND faults "${file}: has an .expected and an .err\n")
        elseif(expected)
            math(EXPR programs "${programs} + 1")
        elseif(refused)
            math(EXPR refusals "${refusals} + 1")
        else()
            string(APPEND faults "${file}: has no .expected and no .err\n")
        endif()
    elseif(NOT EXISTS "${MATRIX}/${name}.anti")
        string(APPEND faults "${file}: has no ${name}.anti\n")
    elseif(kind STREQUAL "args" AND NOT expected)
        string(APPEND faults "${file}: stands beside no program\n")
    endif()
endforeach()
if(NOT faults STREQUAL "")
    message(FATAL_ERROR "files of tests/matrix that register no test:\n${faults}")
endif()
message(STATUS "${programs} programs and ${refusals} refusals")

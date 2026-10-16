# read_build_id of tests/build_id.cmake gives the id of a file with one
# line "build <64 digits>", and an error for a file with no such line,
# with two, or with an id of another length. A file without the line gave
# run_anti_build.cmake an empty id, which matched the head of any map.
# Run with cmake -P and WORK, a directory for the files.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/build_id.cmake")

file(REMOVE_RECURSE "${WORK}")
string(REPEAT "0123456789abcdef" 4 id)
set(failures "")
# reader_case(<name> <text of the file> <id or the text of the error>)
function(reader_case name text expected)
    file(WRITE "${WORK}/${name}" "${text}")
    read_build_id(got error "${WORK}/${name}")
    if(expected STREQUAL id)
        if(NOT got STREQUAL id OR NOT error STREQUAL "")
            set(failures "${failures}${name}: `${got}`, ${error}\n"
                PARENT_SCOPE)
        endif()
        return()
    endif()
    string(FIND "${error}" "${expected}" at)
    if(NOT got STREQUAL "" OR at EQUAL -1)
        set(failures
            "${failures}${name}: `${got}` without `${expected}`, ${error}\n"
            PARENT_SCOPE)
    endif()
endfunction()

reader_case(one "notice\nbuild ${id}\nend\n" "${id}")
reader_case(none "notice\nend\n" "holds 0 build lines")
reader_case(two "build ${id}\nbuild ${id}\n" "holds 2 build lines")
reader_case(short "build 0123\n" "is not 64 digits")

if(NOT failures STREQUAL "")
    message(FATAL_ERROR "read_build_id:\n${failures}")
endif()
file(REMOVE_RECURSE "${WORK}")

# The build id of a program or of its debug link: the one line
# "build <64 hexadecimal digits>" of its licence notice. See
# tests/run_build_id.cmake for why the search takes that whole form.

# Set <out> to the id that <file> holds and <error> to the empty text, or
# <out> to the empty text and <error> to what is wrong: no such line, more
# than one, or an id that is not 64 digits.
function(read_build_id out error file)
    set(${out} "" PARENT_SCOPE)
    file(STRINGS "${file}" lines REGEX "^build [0-9a-f]+$")
    list(LENGTH lines count)
    if(NOT count EQUAL 1)
        set(${error} "${file} holds ${count} build lines: ${lines}"
            PARENT_SCOPE)
        return()
    endif()
    string(REGEX MATCH "^build ([0-9a-f]+)$" line "${lines}")
    string(LENGTH "${CMAKE_MATCH_1}" length)
    if(NOT length EQUAL 64)
        set(${error} "the build id of ${file} is not 64 digits: ${lines}"
            PARENT_SCOPE)
        return()
    endif()
    set(${out} "${CMAKE_MATCH_1}" PARENT_SCOPE)
    set(${error} "" PARENT_SCOPE)
endfunction()

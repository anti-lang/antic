# Hold the names of the compiler's table equal to the names the note lists.
# Run with cmake -P and these values:
#   TABLE  src/antic/warnings.c, whose table spells every name once
#   NOTE   docs/notes/warnings.md, whose tables list each with its meaning
#          and its fix
#
# A row of the note opens with the name in backticks, `| `name` |`.

file(READ "${TABLE}" table)
# The match leaves out the `]` before the `=`, since a bracket in a list
# element keeps CMake from splitting the list there.
string(REGEX MATCHALL " = {\"[a-z0-9-]+\", KIND_[A-Z_]+}" entries "${table}")
set(compiler "")
foreach(entry IN LISTS entries)
    string(REGEX REPLACE "^ = {\"([a-z0-9-]+)\", KIND_([A-Z_]+)}$" "\\1 \\2"
           pair "${entry}")
    list(APPEND compiler "${pair}")
endforeach()
list(LENGTH compiler count)
if(count EQUAL 0)
    message(FATAL_ERROR "no name in ${TABLE}")
endif()

file(STRINGS "${NOTE}" lines)
set(noted "")
set(kind "")
foreach(line IN LISTS lines)
    if(line STREQUAL "## Warnings")
        set(kind "WARNING")
    elseif(line STREQUAL "## Safety checks")
        set(kind "SAFETY_CHECK")
    elseif(line MATCHES "^## ")
        set(kind "")
    elseif(NOT kind STREQUAL "" AND line MATCHES "^\\| `([a-z0-9-]+)` \\|")
        list(APPEND noted "${CMAKE_MATCH_1} ${kind}")
    endif()
endforeach()

foreach(pair IN LISTS compiler)
    list(FIND noted "${pair}" at)
    if(at LESS 0)
        message(FATAL_ERROR "${NOTE} lacks `${pair}` of ${TABLE}")
    endif()
endforeach()
foreach(pair IN LISTS noted)
    list(FIND compiler "${pair}" at)
    if(at LESS 0)
        message(FATAL_ERROR "${NOTE} lists `${pair}`, which ${TABLE} lacks")
    endif()
endforeach()

# antic, anti and every runtime library of runtime/ hold no seam of fault
# injection, and with WITH set the copies of tools/faults.cmake hold it.
# The seam reads the variable ANTI_FAULT, so its name stands among the
# bytes of every binary that holds the seam, with symbols or without, and
# of none other. Run with cmake -P and these values:
#   PROGRAMS  antic and anti, separated by commas
#   RUNTIME   the runtime archive, holding lib/<target>/<level>/
#   WITH      optional copies that must hold the seam, separated by commas

cmake_minimum_required(VERSION 3.21)

set(name "ANTI_FAULT")

function(holds_seam out path)
    file(STRINGS "${path}" found REGEX "${name}" LIMIT_COUNT 1)
    if(found)
        set(${out} TRUE PARENT_SCOPE)
    else()
        set(${out} FALSE PARENT_SCOPE)
    endif()
endfunction()

string(REPLACE "," ";" programs "${PROGRAMS}")
file(GLOB_RECURSE libraries "${RUNTIME}/lib/*anti_rt*.a"
     "${RUNTIME}/lib/*anti_rt*.lib")
if(NOT libraries)
    message(FATAL_ERROR "${RUNTIME}/lib holds no runtime library")
endif()
set(holding "")
foreach(path IN LISTS programs libraries)
    holds_seam(seam "${path}")
    if(seam)
        list(APPEND holding "${path}")
    endif()
endforeach()
if(holding)
    list(JOIN holding "\n  " holding)
    message(FATAL_ERROR "the seam of src/rt/fault.h stands in what ships:\n"
                        "  ${holding}")
endif()

if(DEFINED WITH AND NOT WITH STREQUAL "")
    string(REPLACE "," ";" copies "${WITH}")
    foreach(path IN LISTS copies)
        holds_seam(seam "${path}")
        if(NOT seam)
            message(FATAL_ERROR "${path} holds no `${name}`, so this test "
                                "would not see the seam where it stands")
        endif()
    endforeach()
endif()

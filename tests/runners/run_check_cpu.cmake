# tools/check-cpu.cmake reads the levels of a package and refuses one that
# lacks a level of tools/cpu-levels, in lib/<target>/ and, for a Linux
# target, in lib/<target>-glibc/ as well. The package is a tree of empty
# runtimes, one per target and level.
#
#   cmake -DROOT=<repository> -DWORK=<dir> -P tests/runners/run_check_cpu.cmake

cmake_minimum_required(VERSION 3.21)

file(REMOVE_RECURSE "${WORK}")
set(tree "${WORK}/package")
include("${ROOT}/tools/cpu-levels.cmake")
include("${ROOT}/tools/host.cmake")
antic_read_cpu_levels("${ROOT}/tools/cpu-levels")
foreach(target IN LISTS ANTIC_TARGETS)
    set(levels v1 v2 v3)
    antic_static_library(library anti_rt "${target}")
    if(target MATCHES "arm64$")
        set(levels armv8.0 armv8.2 armv8.5)
    endif()
    foreach(level IN LISTS levels)
        file(WRITE "${tree}/anti/lib/${target}/${level}/${library}" "")
        if(target MATCHES "^linux-")
            file(WRITE "${tree}/anti/lib/${target}-glibc/${level}/${library}" "")
        endif()
    endforeach()
endforeach()
execute_process(COMMAND "${CMAKE_COMMAND}" "-DTREE=${tree}"
                        "-DLEVELS=${ROOT}/tools/cpu-levels"
                        -P "${ROOT}/tools/check-cpu.cmake"
                RESULT_VARIABLE failed OUTPUT_VARIABLE out ERROR_VARIABLE err
                ENCODING NONE)
if(NOT failed EQUAL 0)
    message(FATAL_ERROR "tools/check-cpu.cmake refused a whole package\n"
                        "${out}${err}")
endif()
file(REMOVE_RECURSE "${tree}/anti/lib/linux-arm64/armv8.2")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DTREE=${tree}"
                        "-DLEVELS=${ROOT}/tools/cpu-levels"
                        -P "${ROOT}/tools/check-cpu.cmake"
                RESULT_VARIABLE refused OUTPUT_VARIABLE out ERROR_VARIABLE err
                ENCODING NONE)
if(refused EQUAL 0 OR NOT "${out}${err}" MATCHES "armv8.2")
    message(FATAL_ERROR "tools/check-cpu.cmake took a package without the "
                        "armv8.2 runtime of linux-arm64\n${out}${err}")
endif()

file(REMOVE_RECURSE "${WORK}")

# tools/check-offline.cmake, the check that step 5 of a release runs on
# each VM with the network off, passes on the package of this host, and
# refuses a directory that holds no antic as an install.
#
#   cmake -DROOT=<repository> -DHOST=<host> -DPACKAGE=<dir of package_pack>
#         -DWORK=<dir> -P tests/runners/run_package_offline.cmake

# DESIGN: the test of a release is what a fresh install builds with the
# network off, which Eddie decided on 2026-09-27 under "Binary
# distribution" in docs/decisions.md and decision 8 of
# docs/work-order-distribution.md places in step 5 of ./r. That step runs
# tools/check-offline.cmake on each VM, and this runs the same script on
# the package of this host: a raylib program and a plugin host build and
# run for the host, and a hello program links for each of the other five
# targets. The script names every tool by its path and gives each a PATH
# of bin/ of the package alone.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../package_tree.cmake")

function(run_offline status_variable out_variable)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
                "XDG_CACHE_HOME=${WORK}/cache" "LOCALAPPDATA=${WORK}/cache"
                "${CMAKE_COMMAND}" ${ARGN} "-DROOT=${ROOT}" "-DHOST=${HOST}"
                "-DWORK=${WORK}/offline"
                -P "${ROOT}/tools/check-offline.cmake"
        RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
        ENCODING NONE)
    set("${status_variable}" "${status}" PARENT_SCOPE)
    set("${out_variable}" "${out}${err}" PARENT_SCOPE)
endfunction()
run_offline(status offline_out "-DBIN=${package_bin}" "-DARCHIVE=${package_dir}")
if(NOT status EQUAL 0)
    message(FATAL_ERROR "tools/check-offline.cmake failed on the package of "
                        "${HOST} with ${status}\n${offline_out}")
endif()
set(offline_lines "a raylib program of ${HOST} builds and runs"
    "a plugin host of ${HOST} loads its plugin"
    "the install builds for ${HOST} and links for the other five targets")
include("${CMAKE_CURRENT_LIST_DIR}/../../tools/cpu-levels.cmake")
antic_read_cpu_levels("${CMAKE_CURRENT_LIST_DIR}/../../tools/cpu-levels")
foreach(target IN LISTS ANTIC_TARGETS)
    if(NOT target STREQUAL HOST)
        list(APPEND offline_lines "a hello program links for ${target}: ")
    endif()
endforeach()
foreach(line IN LISTS offline_lines)
    string(FIND "${offline_out}" "${line}" at)
    if(at EQUAL -1)
        message(FATAL_ERROR "tools/check-offline.cmake prints no `${line}` for "
                            "the package of ${HOST}\n${offline_out}")
    endif()
endforeach()
string(FIND "${offline_out}" "a hello program links for ${HOST}: " at)
if(NOT at EQUAL -1)
    message(FATAL_ERROR "tools/check-offline.cmake cross-builds for ${HOST}, "
                        "which is the host\n${offline_out}")
endif()
# A directory that holds no antic is no install, and the check names it.
file(MAKE_DIRECTORY "${WORK}/empty")
run_offline(status offline_out "-DBIN=${WORK}/empty"
            "-DARCHIVE=${package_dir}")
# CMake wraps the text of an error at a width, wherever the path ends.
string(REGEX REPLACE "[ \n]+" " " offline_out "${offline_out}")
if(status EQUAL 0 OR NOT offline_out MATCHES "holds no antic")
    message(FATAL_ERROR "tools/check-offline.cmake took a directory without "
                        "antic as an install\n${offline_out}")
endif()

file(REMOVE_RECURSE "${WORK}")

# The package alone builds and runs a hello program with bin/ of the
# package as the whole PATH, and links a program for every target that
# names no framework with an empty PATH and no LIB.
#
#   cmake -DROOT=<repository> -DHOST=<host> -DPACKAGE=<dir of package_pack>
#         -DWORK=<dir> -P tests/runners/run_package_targets.cmake

# DESIGN: the package alone links a program for every target that names
# no framework, with an empty PATH and no LIB, which Eddie decided on
# 2026-09-27 under "Binary distribution" in docs/decisions.md and the
# step own-tools of docs/work-order-distribution.md built. The Windows
# sysroots of mingw-w64 travel in the package since the step mingw.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../package_tree.cmake")

file(WRITE "${WORK}/hello.anti"
     "import anti.io;\n\nfn main() -> int\n{\n    io.print(\"hello\");\n"
     "    return 0;\n}\n")
run_alone("antic hello.anti" "${WORK}" "${package_bin}/antic${suffix}"
          hello.anti -o "${WORK}/hello${suffix}")
run_alone("hello" "${WORK}" "${WORK}/hello${suffix}")
if(NOT alone_out MATCHES "hello")
    message(FATAL_ERROR "the hello program of the package prints "
                        "'${alone_out}'")
endif()

set(ANTIC "${package_bin}/antic${suffix}")
include("${CMAKE_CURRENT_LIST_DIR}/../../tools/cpu-levels.cmake")
antic_read_cpu_levels("${CMAKE_CURRENT_LIST_DIR}/../../tools/cpu-levels")
foreach(target IN LISTS ANTIC_TARGETS)
    set(exe "${WORK}/return42-${target}")
    if(target MATCHES "^windows-")
        string(APPEND exe ".exe")
    endif()
    antic_build(IN "${WORK}"
                THROUGH "${CMAKE_COMMAND}" -E env "PATH=" --unset=LIB
                        "XDG_CACHE_HOME=${WORK}/cache"
                        "LOCALAPPDATA=${WORK}/cache"
                COMMAND --target "${target}" -o "${exe}"
                        "${ROOT}/tests/programs/return42.anti")
    if(NOT EXISTS "${exe}")
        message(FATAL_ERROR "the package alone links no program for ${target}")
    endif()
endforeach()

file(REMOVE_RECURSE "${WORK}")

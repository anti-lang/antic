# Build the plugin host of tests/plugin and its library for a Windows
# target, and check what the link of each side reads. Every name the
# library imports through an __imp_ entry stands in the host's .def file,
# the library exports anti_rt_imports, anti_rt_provides and the notice
# anti_licenses, whose build id a trace reads by the export, and the count
# that leads anti_rt_imports is the number of its places. Neither program
# runs here: the VM run runs them. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   RUNTIME   the runtime archive
#   READOBJ   the llvm-readobj of the runtime archive
#   SOURCES   tests/plugin
#   TARGET    windows-x86_64 or windows-arm64
#   WORK      a directory for the files
#   LIBRARY   the module of SOURCES/net/example that is the library,
#             `fancy` without it
#   CPU       the level of the processor of both builds, the default of
#             the target without it

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

if(NOT EXISTS "${RUNTIME}/sysroot/${TARGET}")
    message("SKIP: the runtime archive has no sysroot for ${TARGET}")
    return()
endif()

set(dir "${WORK}")
file(REMOVE_RECURSE "${dir}")
file(MAKE_DIRECTORY "${dir}/net/example" "${dir}/lib")
set(common --target ${TARGET})
if(DEFINED CPU)
    list(APPEND common --cpu ${CPU})
endif()
if(NOT DEFINED LIBRARY)
    set(LIBRARY fancy)
endif()
antic_build(${common} -c -I "${SOURCES}" -o "${dir}/net/example/greet.antl"
            "${SOURCES}/net/example/greet.anti")
antic_build(${common} --runtime "${RUNTIME}" --keep-llvm -I "${SOURCES}"
            -I "${dir}" -o "${dir}/host.exe" "${SOURCES}/host.anti")
antic_build(${common} --lib shared --no-runtime --runtime "${RUNTIME}"
            --keep-llvm -I "${SOURCES}" -I "${dir}" -o "${dir}/lib/lib${LIBRARY}.dll"
            "${SOURCES}/net/example/${LIBRARY}.anti" "${dir}/host.lib")

file(STRINGS "${dir}/host.def" def)
file(READ "${dir}/lib/lib${LIBRARY}.ll" llvm)
string(REGEX MATCHALL "@__imp_[^ ,)\n]+" imports "${llvm}")
list(REMOVE_DUPLICATES imports)
if(imports STREQUAL "")
    message(FATAL_ERROR "the library imports nothing from its host")
endif()
foreach(import IN LISTS imports)
    string(REGEX REPLACE "^@__imp_" "" name "${import}")
    if(NOT "    ${name}" IN_LIST def AND NOT "    ${name} DATA" IN_LIST def)
        message(FATAL_ERROR "the host exports no ${name}, which the library "
                            "imports")
    endif()
endforeach()

execute_process(COMMAND "${READOBJ}" --coff-exports "${dir}/lib/lib${LIBRARY}.dll"
                OUTPUT_VARIABLE exports ENCODING NONE)
foreach(name anti_rt_imports anti_rt_provides anti_licenses)
    if(NOT exports MATCHES "Name: ${name}\n")
        message(FATAL_ERROR "the library exports no ${name}:\n${exports}")
    endif()
endforeach()

# The constant of the table: its count, a word, then the places.
set(table "@anti_rt_imports = dso_local global ")
string(APPEND table "<{ i64, i64, \\[([0-9]+) x ptr\\] }> <{ i64 ([0-9]+),")
if(NOT llvm MATCHES "${table}")
    message(FATAL_ERROR "the library holds no anti_rt_imports")
endif()
if(NOT CMAKE_MATCH_1 EQUAL CMAKE_MATCH_2 OR CMAKE_MATCH_2 EQUAL 0)
    message(FATAL_ERROR "the library lists ${CMAKE_MATCH_2} places in a "
                        "table of ${CMAKE_MATCH_1}")
endif()

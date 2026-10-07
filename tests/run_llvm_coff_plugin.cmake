# Build the plugin host of tests/plugin and its library for a Windows
# target, and check what the link of each side reads. Every name the
# library imports through an __imp_ entry stands in the host's .def file,
# the library exports anti_rt_imports and anti_rt_provides, and the count
# that leads anti_rt_imports is the number of its places. Neither program
# runs here: the VM run runs them. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   RUNTIME   the runtime archive
#   READOBJ   the llvm-readobj of the runtime archive
#   SOURCES   tests/plugin
#   TARGET    windows-x86_64 or windows-arm64
#   WORK      a directory for the files

cmake_minimum_required(VERSION 3.21)

if(NOT EXISTS "${RUNTIME}/sysroot/${TARGET}")
    message("SKIP: the runtime archive has no sysroot for ${TARGET}")
    return()
endif()

function(run)
    execute_process(COMMAND "${ANTIC}" ${ARGV} RESULT_VARIABLE status
                    OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0 OR NOT out STREQUAL "" OR NOT err STREQUAL "")
        message(FATAL_ERROR "antic failed with ${status}: ${ARGV}\n${out}${err}")
    endif()
endfunction()

set(dir "${WORK}")
file(REMOVE_RECURSE "${dir}")
file(MAKE_DIRECTORY "${dir}/net/example" "${dir}/lib")
set(common --target ${TARGET})
run(${common} -c -I "${SOURCES}" -o "${dir}/net/example/greet.antl"
    "${SOURCES}/net/example/greet.anti")
run(${common} --runtime "${RUNTIME}" --keep-llvm -I "${SOURCES}"
    -I "${dir}" -o "${dir}/host.exe" "${SOURCES}/host.anti")
run(${common} --lib shared --no-runtime --runtime "${RUNTIME}"
    --keep-llvm -I "${SOURCES}" -I "${dir}" -o "${dir}/lib/libfancy.dll"
    "${SOURCES}/net/example/fancy.anti" "${dir}/host.lib")

file(STRINGS "${dir}/host.def" def)
file(READ "${dir}/lib/libfancy.ll" llvm)
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

execute_process(COMMAND "${READOBJ}" --coff-exports "${dir}/lib/libfancy.dll"
                OUTPUT_VARIABLE exports ENCODING NONE)
foreach(name anti_rt_imports anti_rt_provides)
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

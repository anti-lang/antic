# Build the plugin host of tests/plugin and its library for a Windows
# target under both back ends, and compare what the link of each side
# reads. The host's .def file exports every name the program defines, and
# the library exports anti_rt_imports and anti_rt_provides, with the same
# number of places that hold an __imp_ entry. Neither runs here: the VM run
# of the step vm runs them. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   RUNTIME   the runtime archive
#   READOBJ   the llvm-readobj of the runtime archive
#   SOURCES   tests/plugin
#   TARGET    windows-x86_64 or windows-arm64
#   WORK      a directory for the files

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

foreach(backend native llvm)
    set(dir "${WORK}/${backend}")
    file(REMOVE_RECURSE "${dir}")
    file(MAKE_DIRECTORY "${dir}/net/example" "${dir}/lib")
    set(common --backend ${backend} --target ${TARGET})
    run(${common} -c -I "${SOURCES}" -o "${dir}/net/example/greet.antl"
        "${SOURCES}/net/example/greet.anti")
    run(${common} --runtime "${RUNTIME}" --keep-llvm -I "${SOURCES}"
        -I "${dir}" -o "${dir}/host.exe" "${SOURCES}/host.anti")
    run(${common} --lib shared --no-runtime --runtime "${RUNTIME}"
        --keep-llvm -I "${SOURCES}" -I "${dir}" -o "${dir}/lib/libfancy.dll"
        "${SOURCES}/net/example/fancy.anti" "${dir}/host.lib")
    file(STRINGS "${dir}/host.def" def_${backend} REGEX "^[^N]")
    execute_process(COMMAND "${READOBJ}" --coff-exports
                            "${dir}/lib/libfancy.dll"
                    OUTPUT_VARIABLE exports_${backend} ENCODING NONE)
endforeach()

if(NOT def_native STREQUAL def_llvm)
    message(FATAL_ERROR "the host of the LLVM back end exports other names")
endif()
foreach(name anti_rt_imports anti_rt_provides)
    if(NOT exports_llvm MATCHES "Name: ${name}\n")
        message(FATAL_ERROR "the library exports no ${name}:\n${exports_llvm}")
    endif()
endforeach()

# The count of places: the first word of the table in the native
# assembly, and the first element of its constant in the LLVM text.
file(READ "${WORK}/native/lib/libfancy.s" native)
file(READ "${WORK}/llvm/lib/libfancy.ll" llvm)
if(NOT native MATCHES "anti_rt_imports:\n    \\.quad ([0-9]+)\n")
    message(FATAL_ERROR "the native library holds no anti_rt_imports")
endif()
set(count "${CMAKE_MATCH_1}")
set(table "@anti_rt_imports = dso_local global ")
string(APPEND table "<{ i64, i64(, \\[[0-9]+ x ptr\\])? }> <{ i64 ([0-9]+),")
if(NOT llvm MATCHES "${table}")
    message(FATAL_ERROR "the LLVM library holds no anti_rt_imports")
endif()
if(NOT CMAKE_MATCH_2 EQUAL count)
    message(FATAL_ERROR "the LLVM library lists ${CMAKE_MATCH_2} places, "
                        "the native one ${count}")
endif()

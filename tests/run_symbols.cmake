# Link tests/trace/symbols.anti with -g for a target and look its
# functions up with the probe, which reads the file with the readers the
# runtime of that target uses. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime directory
#   PROBE     the symbols_probe executable
#   SOURCE    tests/trace/symbols.anti
#   TARGET    the target
#   WORK      a directory for the files
#
# An ELF binary carries its symbols and its line table, so the probe
# reads the executable. A Mach-O link leaves the line table in the
# object, which the probe reads. The first address of a function lies on
# the line of its declaration when it has a prologue. A leaf has none, so
# its first address lies on its first statement.

file(GLOB runtime_library "${RUNTIME}/lib/${TARGET}/*/libanti_rt.a")
if(NOT EXISTS "${RUNTIME}/sysroot/${TARGET}" OR runtime_library STREQUAL "")
    message("SKIP: the runtime archive has no sysroot or runtime for ${TARGET}")
    return()
endif()
file(MAKE_DIRECTORY "${WORK}")
set(exe "${WORK}/symbols")
execute_process(
    COMMAND "${ANTIC}" -g --target ${TARGET} --llvm-mc "${LLVM_MC}"
            --runtime "${RUNTIME}" -o "${exe}" "${SOURCE}"
    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic -g failed for ${TARGET}\n${err}")
endif()

function(probe form file symbol offset wanted)
    execute_process(COMMAND "${PROBE}" ${form} "${file}" ${symbol} ${offset}
        RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
        ENCODING NONE)
    string(STRIP "${out}" out)
    if(NOT status EQUAL 0 OR NOT out STREQUAL wanted)
        message(FATAL_ERROR "${form} ${symbol}+${offset} of ${TARGET} gives "
                            "'${out}', not '${wanted}'\n${err}")
    endif()
endfunction()

if("${TARGET}" MATCHES "^linux-")
    probe(elf "${exe}" symbols.inner 0 "symbols.inner symbols.anti:8")
    probe(elf "${exe}" symbols.main 0 "symbols.main symbols.anti:12")
    # A dev build of linux-arm64 puts the mapping symbol `$x` of the object
    # at the address of its first function, where it names no function.
    # Dev mode gives every function a prologue, so its first address lies
    # on the line of its declaration.
    execute_process(
        COMMAND "${ANTIC}" --dev -g --target ${TARGET} --llvm-mc "${LLVM_MC}"
                --runtime "${RUNTIME}" -o "${exe}_dev" "${SOURCE}"
        RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "antic --dev -g failed for ${TARGET}\n${err}")
    endif()
    probe(elf "${exe}_dev" symbols.inner 0 "symbols.inner symbols.anti:6")
elseif("${TARGET}" MATCHES "^macos-")
    probe(macho "${exe}.o" _symbols.inner 0 "symbols.anti:8")
    probe(macho "${exe}.o" _symbols.main 0 "symbols.anti:12")
endif()

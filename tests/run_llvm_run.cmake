# The tool run of the LLVM back end, "Integration route" of
# docs/work-order-llvm-back-end.md: --keep-llvm keeps the text and the
# bitcode beside the output, a dev build writes no bitcode, and -S writes
# the assembly of llc and keeps neither. Run with cmake -P and these
# values:
#   ANTIC     the antic executable
#   RUNTIME   the runtime directory, whose bin/ holds opt and llc
#   SOURCE    a program that exits with 42
#   WORK      a directory for the output

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

# Run antic with ARGN, which must succeed and print nothing.
function(antic)
    execute_process(COMMAND "${ANTIC}" --backend llvm --runtime "${RUNTIME}"
                            ${ARGN}
                    RESULT_VARIABLE status OUTPUT_VARIABLE out
                    ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0 OR NOT out STREQUAL "" OR NOT err STREQUAL "")
        message(FATAL_ERROR "antic ${ARGN} gave ${status}\n${out}${err}")
    endif()
endfunction()

# Fail unless path exists, or with ABSENT unless it does not.
function(expect_file path)
    cmake_parse_arguments(PARSE_ARGV 1 arg "ABSENT" "" "")
    if(arg_ABSENT AND EXISTS "${path}")
        message(FATAL_ERROR "${path} exists")
    elseif(NOT arg_ABSENT AND NOT EXISTS "${path}")
        message(FATAL_ERROR "${path} is missing")
    endif()
endfunction()

# Run the program at path, which must exit with 42.
function(expect_run path)
    execute_process(COMMAND "${path}" RESULT_VARIABLE status)
    if(NOT status EQUAL 42)
        message(FATAL_ERROR "${path} exits with ${status}, expected 42")
    endif()
endfunction()

# A release build keeps the text and the bitcode of opt.
antic(--keep-llvm -o "${WORK}/release" "${SOURCE}")
expect_run("${WORK}/release")
file(STRINGS "${WORK}/release.ll" triple REGEX "^target triple = ")
if(triple STREQUAL "")
    message(FATAL_ERROR "release.ll holds no target triple")
endif()
# Bitcode starts with `BC` 0xC0DE, or on Darwin with the magic of the
# wrapper around it, 0x0B17C0DE in little-endian order.
file(READ "${WORK}/release.bc" magic LIMIT 4 HEX)
if(NOT magic MATCHES "^(4243c0de|dec0170b)$")
    message(FATAL_ERROR "release.bc starts with ${magic}, not the magic of bitcode")
endif()

# A dev build runs llc alone, so it writes no bitcode.
antic(--dev --keep-llvm -o "${WORK}/dev" "${SOURCE}")
expect_run("${WORK}/dev")
expect_file("${WORK}/dev.ll")
expect_file("${WORK}/dev.bc" ABSENT)

# -S writes the assembly of llc, in release and in dev mode, and deletes
# the text and the bitcode.
foreach(mode release dev)
    set(mode_options "")
    if(mode STREQUAL "dev")
        set(mode_options --dev)
    endif()
    antic(-S ${mode_options} -o "${WORK}/${mode}.s" "${SOURCE}")
    file(STRINGS "${WORK}/${mode}.s" labels REGEX "main:")
    if(labels STREQUAL "")
        message(FATAL_ERROR "${mode}.s defines no main")
    endif()
    expect_file("${WORK}/${mode}.s.ll" ABSENT)
    expect_file("${WORK}/${mode}.s.bc" ABSENT)
endforeach()

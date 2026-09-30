# --memory-checks finds a pointer into a List kept across a push that grows
# it. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime archive
#   SOURCE    tests/traps/memory_checks_list.anti
#   WORK      a directory for the files
#   HOST      the host target
#
# The program reads through a pointer the walk lent after the push freed
# the room it points into. The report names a read after free and ends
# the program with status 1. On macOS, whose runtime symbolizes the frames,
# it also names the function that read, and `push` of the list among the
# frames that freed the room. windows-arm64 has no runtime of
# AddressSanitizer, so the test reports itself skipped there.

if(HOST STREQUAL "windows-arm64")
    message("SKIP: windows-arm64 has no runtime of AddressSanitizer")
    return()
endif()
file(MAKE_DIRECTORY "${WORK}")

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

set(program "${WORK}/memory_checks_list")
execute_process(COMMAND "${ANTIC}" --memory-checks --llvm-mc "${LLVM_MC}"
                        --runtime "${RUNTIME}" -o "${program}" "${SOURCE}"
                RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic failed\n${err}")
endif()

program_expect("memory checks list" COMMAND "${program}" STATUS 1 ANY_ERR)
set(err "${program_stderr}")
set(patterns
    "ERROR: AddressSanitizer: heap-use-after-free"
    "READ of size 8")
if(HOST MATCHES "^macos")
    list(APPEND patterns
        "#0 0x[0-9a-f]+ in memory_checks_list\\.kept_across_push"
        "freed by thread T0 here:\n( +#[^\n]*\n)* +#[^\n]* in anti\\.collection\\.list\\.List[^ \n]*\\.push")
endif()
foreach(pattern IN LISTS patterns)
    if(NOT err MATCHES "${pattern}")
        message(FATAL_ERROR "the report holds no `${pattern}`:\n${err}")
    endif()
endforeach()

# --memory-checks finds a read of a bitfield after free. Run with cmake -P
# and these values:
#   ANTIC     the antic executable
#   RUNTIME   the runtime archive
#   SOURCE    tests/traps/memory_checks_bitfield.anti
#   WORK      a directory for the files
#   HOST      the host target
#
# The program reads a bitfield through a pointer to a block it freed. The
# report names a read after free and ends the program with status 1, as
# for the read of a plain field in tests/traps/memory_checks.anti. The size
# of the read is that of the unit of the bitfield, which the target lays
# out, so the test does not name it. windows-arm64 has no runtime of
# AddressSanitizer, so the test reports itself skipped there.

cmake_minimum_required(VERSION 3.21)

if(HOST STREQUAL "windows-arm64")
    message("SKIP: windows-arm64 has no runtime of AddressSanitizer")
    return()
endif()
file(MAKE_DIRECTORY "${WORK}")

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

set(program "${WORK}/memory_checks_bitfield")
antic_build(--memory-checks --runtime "${RUNTIME}" -o "${program}" "${SOURCE}")

program_expect("memory checks bitfield" COMMAND "${program}" STATUS 1
               ANY_ERR)
foreach(pattern "ERROR: AddressSanitizer: heap-use-after-free"
                "READ of size [0-9]+")
    if(NOT program_stderr MATCHES "${pattern}")
        message(FATAL_ERROR "the report holds no `${pattern}`:\n"
                            "${program_stderr}")
    endif()
endforeach()

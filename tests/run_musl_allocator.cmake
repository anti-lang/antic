# A program of musl takes malloc, free, calloc, realloc and aligned_alloc
# from mimalloc of the runtime archive, in release mode with each mode of
# --lto and in dev mode. A program of --memory-checks links in the glibc
# mode and keeps the allocator of AddressSanitizer. Run with cmake -P and
# these values:
#   ANTIC         the antic executable
#   RUNTIME       the runtime directory
#   LLVM_OBJDUMP  the llvm-objdump executable
#   SOURCE        a program that calls the five functions, with its
#                 .expected file beside it
#   TARGET        linux-x86_64 or linux-arm64
#   HOST          the target antic runs on, where each program also runs
#   WORK          a directory for the files
#
# The symbol table of a Linux program stays after --strip-debug. mimalloc
# defines malloc, free, calloc and realloc as aliases of mi_malloc,
# mi_free, mi_calloc and mi_realloc, so each pair shares one address.
# aligned_alloc is a function of its own there. Its file symbol tells it
# apart from musl's: the musl of the sysroot names each source file of its
# allocator, and mimalloc is the one file static.c.
#
# mimalloc builds with MI_STATS=0, so a program keeps none of its
# detailed statistics. Eddie decided it on 2026-10-08: under
# MI_NO_GETENV no program can print them. The labels "binned" and
# "malloc req~" are strings that mi_stats_print writes for those
# statistics alone, so a program that links them still carries the code.

cmake_minimum_required(VERSION 3.21)

if(NOT EXISTS "${RUNTIME}/sysroot/${TARGET}")
    message("SKIP: the runtime archive has no sysroot for ${TARGET}")
    return()
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

# Set <out> to the symbol table of the program that antic links from
# SOURCE with the options after <name>.
function(link_program out name)
    set(program "${WORK}/${name}")
    execute_process(COMMAND "${ANTIC}" --target "${TARGET}"
                            --runtime "${RUNTIME}" ${ARGN}
                            -o "${program}" "${SOURCE}"
                    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "antic ${ARGN} failed\n${err}")
    endif()
    execute_process(COMMAND "${LLVM_OBJDUMP}" -t "${program}"
                    RESULT_VARIABLE status OUTPUT_VARIABLE table
                    ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "llvm-objdump -t ${program} failed\n${err}")
    endif()
    set(${out} "${table}" PARENT_SCOPE)
endfunction()

# Set <out> to the address of the function <symbol> in <table>, or to
# the empty string when the table has none.
function(address out table symbol)
    string(REGEX MATCH "\n([0-9a-f]+) [^\n]* F [^\n]* (\\.hidden )?${symbol}\n"
           found "\n${table}")
    set(${out} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")
string(REGEX REPLACE "\\.anti$" ".expected" expected_file "${SOURCE}")

# The source files of musl's allocator, as its file symbols name them.
set(musl_files lite_malloc.c aligned_alloc.c calloc.c realloc.c)

foreach(build "release" "lto_none,--lto,none" "lto_thin,--lto,thin"
        "dev,--dev")
    string(REPLACE "," ";" options "${build}")
    list(POP_FRONT options name)
    link_program(table "${name}" ${options})
    foreach(pair malloc,mi_malloc free,mi_free calloc,mi_calloc
            realloc,mi_realloc)
        string(REPLACE "," ";" pair "${pair}")
        list(GET pair 0 c_name)
        list(GET pair 1 mi_name)
        address(c_at "${table}" "${c_name}")
        address(mi_at "${table}" "${mi_name}")
        if(c_at STREQUAL "" OR NOT c_at STREQUAL mi_at)
            message(FATAL_ERROR "${name}: ${c_name} at `${c_at}` is not "
                                "${mi_name} at `${mi_at}` of mimalloc")
        endif()
    endforeach()
    address(aligned "${table}" aligned_alloc)
    if(aligned STREQUAL "")
        message(FATAL_ERROR "${name}: the program defines no aligned_alloc")
    endif()
    file(STRINGS "${WORK}/${name}" labels
         REGEX "^(binned|malloc req~)$" ENCODING UTF-8)
    if(labels)
        message(FATAL_ERROR "${name}: the program holds the detailed "
                            "statistics of mimalloc: ${labels}")
    endif()
    foreach(file IN LISTS musl_files)
        string(FIND "${table}" " ${file}\n" at)
        if(NOT at EQUAL -1)
            message(FATAL_ERROR "${name}: the program links ${file} of musl")
        endif()
    endforeach()
    if(HOST STREQUAL TARGET)
        program_expect("${name}" COMMAND "${WORK}/${name}"
                       EXPECTED "${expected_file}")
    endif()
endforeach()

# --memory-checks links against glibc, where AddressSanitizer defines
# malloc and nothing of mimalloc goes in.
if(EXISTS "${RUNTIME}/sysroot/${TARGET}-glibc")
    link_program(table memory_checks --memory-checks)
    address(c_at "${table}" malloc)
    if(c_at STREQUAL "")
        message(FATAL_ERROR "memory_checks: the program defines no malloc")
    endif()
    string(FIND "${table}" " mi_malloc\n" at)
    if(NOT at EQUAL -1)
        message(FATAL_ERROR "memory_checks: the program links mimalloc")
    endif()
endif()

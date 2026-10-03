# --memory-checks finds memory errors while the program runs. Run with
# cmake -P and these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime archive
#   SOURCE    tests/traps/memory_checks.anti
#   WORK      a directory for the files
#   HOST      the host target
#
# A build without the option carries no check, and a build with it
# carries the checks, for the host or, on a windows-arm64 host, for
# windows-x86_64. windows-arm64 refuses the option with its message, and
# a windows-arm64 host reports the test skipped after that check, since
# it has no runtime of AddressSanitizer. The module that links gets the
# options hook and the function that marks the blocks of the runtime as
# kept, except on Windows, whose runtime has no leak check. On every
# other host a build with the option reports a read after free, a double
# free and a write past the end of a heap block, each ending the program
# with status 1, and on macOS and Linux a leak. The clean case compiles a
# pattern literal and runs the worker pool, and runs to its end with the
# leak check on and no report. On macOS each report names the function
# of the program that made the error.

file(MAKE_DIRECTORY "${WORK}")

set(checked "${HOST}")
if(HOST STREQUAL "windows-arm64")
    set(checked windows-x86_64)
endif()
execute_process(COMMAND "${ANTIC}" --target ${checked} --runtime "${RUNTIME}"
                        -S -o "${WORK}/memory_checks_off.s" "${SOURCE}"
                RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic failed without --memory-checks\n${err}")
endif()
file(READ "${WORK}/memory_checks_off.s" assembly)
if(assembly MATCHES "__asan_")
    message(FATAL_ERROR "a build without --memory-checks carries a check")
endif()
execute_process(COMMAND "${ANTIC}" --memory-checks --target ${checked}
                        --runtime "${RUNTIME}" -S
                        -o "${WORK}/memory_checks_on.s" "${SOURCE}"
                RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic failed with --memory-checks\n${err}")
endif()
file(READ "${WORK}/memory_checks_on.s" assembly)
if(NOT assembly MATCHES "__asan_loadN" OR
   NOT assembly MATCHES "__asan_storeN")
    message(FATAL_ERROR "a build with --memory-checks carries no check")
endif()

# The hook of the options and the marking of the runtime's blocks, which
# the linking module of each target carries or lacks.
foreach(target linux-x86_64 macos-arm64 windows-x86_64)
    execute_process(COMMAND "${ANTIC}" --memory-checks --target ${target}
                            --runtime "${RUNTIME}" -S
                            -o "${WORK}/memory_checks_${target}.s"
                            "${SOURCE}"
                    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "antic failed for ${target}\n${err}")
    endif()
    file(READ "${WORK}/memory_checks_${target}.s" assembly)
    set(found OFF)
    if(assembly MATCHES "__asan_default_options" AND
       assembly MATCHES "anti_rt_memory_kept" AND
       assembly MATCHES "__lsan_ignore_object")
        set(found ON)
    endif()
    if(target MATCHES "^windows" AND
       (assembly MATCHES "__asan_default_options" OR
        assembly MATCHES "anti_rt_memory_kept"))
        message(FATAL_ERROR "${target} asks for a leak check its runtime "
                            "does not have")
    elseif(NOT target MATCHES "^windows" AND NOT found)
        message(FATAL_ERROR "${target} carries no options hook or no "
                            "marking of the kept blocks")
    endif()
    # A report walks the Anti frames by their frame pointers. antic
    # writes no CFI on ELF, so the unwinder stops at the first one. The
    # native back end writes the options as `.byte` lines after their
    # label, which this reads back into text up to the terminating zero.
    # llc writes one `.asciz` line.
    if(NOT target MATCHES "^windows")
        string(REGEX MATCH "memory_check_options:\n((    \\.byte [^\n]*\n)+)"
               lines "${assembly}")
        string(REGEX MATCHALL "0x[0-9a-f][0-9a-f]" bytes "${CMAKE_MATCH_1}")
        set(options "")
        foreach(byte IN LISTS bytes)
            math(EXPR code "${byte}")
            if(code EQUAL 0)
                break()
            endif()
            string(ASCII ${code} letter)
            string(APPEND options "${letter}")
        endforeach()
        if(options STREQUAL "" AND assembly MATCHES
           "memory_check_options:[^\n]*\n[ \t]*\\.asciz[ \t]+\"([^\"]*)\"")
            set(options "${CMAKE_MATCH_1}")
        endif()
        if(NOT options MATCHES "(^|:)fast_unwind_on_fatal=1(:|$)")
            message(FATAL_ERROR "${target} does not ask for the fast "
                                "unwinder of a report: `${options}`")
        endif()
    endif()
endforeach()

execute_process(COMMAND "${ANTIC}" --memory-checks --target windows-arm64
                        --runtime "${RUNTIME}" -S
                        -o "${WORK}/memory_checks_arm.s" "${SOURCE}"
                RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(status EQUAL 0 OR NOT err STREQUAL
   "antic: `--memory-checks` is not available for windows-arm64\n")
    message(FATAL_ERROR "windows-arm64 exited with ${status} and printed "
                        "`${err}`")
endif()

if(HOST STREQUAL "windows-arm64")
    message("SKIP: windows-arm64 has no runtime of AddressSanitizer")
    return()
endif()

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

set(program "${WORK}/memory_checks_run")
execute_process(COMMAND "${ANTIC}" --memory-checks --llvm-mc "${LLVM_MC}"
                        --runtime "${RUNTIME}" -o "${program}" "${SOURCE}"
                RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic failed\n${err}")
endif()

# The names of the frames come from the symbolizer of the runtime, which
# has one on macOS alone. `anti run` puts the report through Anti's
# symbolizer on the other hosts, which run_anti_memory_checks.cmake checks.
function(reports case)
    program_expect("memory checks ${case}" COMMAND "${program}" ${case}
                   STATUS 1 ANY_ERR)
    set(err "${program_stderr}")
    foreach(pattern IN LISTS ARGN)
        if(NOT HOST MATCHES "^macos" AND pattern MATCHES "memory_checks")
            continue()
        endif()
        if(NOT err MATCHES "${pattern}")
            message(FATAL_ERROR "${case} printed no "
                                "`${pattern}`:\n${err}")
        endif()
    endforeach()
endfunction()

reports(use_after_free
    "ERROR: AddressSanitizer: heap-use-after-free"
    "READ of size 8"
    "#0 0x[0-9a-f]+ in memory_checks\\.read_after_free")
reports(double_free
    "ERROR: AddressSanitizer: attempting double-free"
    "in memory_checks\\.free_twice")
reports(overflow
    "ERROR: AddressSanitizer: heap-buffer-overflow"
    "WRITE of size 8"
    "#0 0x[0-9a-f]+ in memory_checks\\.write_past_end")
if(NOT HOST MATCHES "^windows")
    reports(leak
        "ERROR: LeakSanitizer: detected memory leaks"
        "Direct leak of 8 byte\\(s\\) in 1 object\\(s\\) allocated from:"
        "allocated from:\n[^\n]*malloc[^\n]*\n[^\n]*in memory_checks\\.kept"
        "SUMMARY: AddressSanitizer: 8 byte\\(s\\) leaked in 1 allocation\\(s\\)")
endif()

# The leak check is on: the runtime marks the arguments, the environment,
# the pattern literal and the blocks of the pool as kept until exit.
program_expect("memory checks clean" COMMAND "${program}" clean)

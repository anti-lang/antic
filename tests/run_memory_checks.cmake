# --memory-checks finds memory errors while the program runs. Run with
# cmake -P and these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime archive
#   SOURCE    tests/traps/memory_checks.anti
#   WORK      a directory for the files
#   RUN       ON where the runtime archive holds AddressSanitizer for the host
#
# A build without the option carries no check. windows-arm64 refuses the
# option with its message. Where the host has the runtime, a build with it
# reports a read after free, a double free, a write past the end of a heap
# block and a leak, each naming the function of the program that made it.
# The clean case runs to its end with no report of an access.

file(MAKE_DIRECTORY "${WORK}")

execute_process(COMMAND "${ANTIC}" --runtime "${RUNTIME}" -S
                        -o "${WORK}/memory_checks_off.s" "${SOURCE}"
                RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic failed without --memory-checks\n${err}")
endif()
file(READ "${WORK}/memory_checks_off.s" assembly)
if(assembly MATCHES "__asan_")
    message(FATAL_ERROR "a build without --memory-checks carries a check")
endif()
execute_process(COMMAND "${ANTIC}" --memory-checks --runtime "${RUNTIME}"
                        -S -o "${WORK}/memory_checks_on.s" "${SOURCE}"
                RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic failed with --memory-checks\n${err}")
endif()
file(READ "${WORK}/memory_checks_on.s" assembly)
if(NOT assembly MATCHES "__asan_loadN" OR
   NOT assembly MATCHES "__asan_storeN" OR
   NOT assembly MATCHES "__asan_default_options")
    message(FATAL_ERROR "a build with --memory-checks carries no check")
endif()

execute_process(COMMAND "${ANTIC}" --memory-checks --target windows-arm64
                        --runtime "${RUNTIME}" -S
                        -o "${WORK}/memory_checks_arm.s" "${SOURCE}"
                RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(status EQUAL 0 OR NOT err STREQUAL
   "antic: `--memory-checks` is not available for windows-arm64\n")
    message(FATAL_ERROR "windows-arm64 exited with ${status} and printed "
                        "`${err}`")
endif()

if(NOT RUN)
    return()
endif()

set(program "${WORK}/memory_checks_run")
execute_process(COMMAND "${ANTIC}" --memory-checks --llvm-mc "${LLVM_MC}"
                        --runtime "${RUNTIME}" -o "${program}" "${SOURCE}"
                RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic failed\n${err}")
endif()

function(reports case)
    execute_process(COMMAND "${program}" ${case} RESULT_VARIABLE code
                    OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
    if(code EQUAL 0)
        message(FATAL_ERROR "${case} exited with 0 and "
                            "printed `${err}`")
    endif()
    foreach(pattern IN LISTS ARGN)
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
reports(leak
    "ERROR: LeakSanitizer: detected memory leaks"
    "Direct leak of 8 byte\\(s\\) in 1 object\\(s\\) allocated from:\n[^\n]*malloc[^\n]*\n[^\n]*in memory_checks\\.kept")

# The leak check stays off here: the runtime keeps the arguments and
# the environment of the program without a pointer that outlives
# main, and the check reports them.
set(ENV{ASAN_OPTIONS} "detect_leaks=0")
execute_process(COMMAND "${program}" clean RESULT_VARIABLE code
                OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
unset(ENV{ASAN_OPTIONS})
if(NOT code EQUAL 0 OR NOT err STREQUAL "")
    message(FATAL_ERROR "clean exited with ${code} and "
                        "printed `${err}`")
endif()

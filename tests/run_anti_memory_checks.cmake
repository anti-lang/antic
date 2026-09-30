# Drive `anti run --memory-checks` and `anti test --memory-checks` and read
# the reports. Run with cmake -P and these values:
#   ANTI      the anti executable
#   RUNTIME   the runtime archive
#   LLVM_MC   the llvm-mc executable
#   SOURCE    tests/traps/memory_checks.anti
#   TESTS     tests/traps/memory_checks_tests.anti
#   WORK      a directory this run writes into
#   HOST      the host target
#
# The project holds SOURCE as its module, with the case the program takes
# written in place of its argument, since `anti run` passes none. `anti
# run` puts each report through Anti's symbolizer, so a frame of the
# program names its function, its file and its line. The lines are the
# ones of SOURCE and TESTS. A build without the option before and after
# shows that the cache keeps the objects of the two apart. Windows keeps
# the symbolizer of AddressSanitizer and has no leak check, so there the
# run reads the kind of each error alone. windows-arm64 has no runtime of
# AddressSanitizer, so the test reports itself skipped there.

if(HOST STREQUAL "windows-arm64")
    message("SKIP: windows-arm64 has no runtime of AddressSanitizer")
    return()
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/project/src/memcheck")
file(WRITE "${WORK}/project/anti.toml"
     "[package]\nname = \"memcheck.demo\"\nversion = \"0.1.0\"\n")
file(READ "${SOURCE}" program)
set(named OFF)
if(NOT HOST MATCHES "^windows")
    set(named ON)
endif()

function(write_case case)
    string(REPLACE "switch args[1]" "switch \"${case}\"" text "${program}")
    if(text STREQUAL program)
        message(FATAL_ERROR "the program takes no case from `args[1]`")
    endif()
    file(WRITE "${WORK}/project/src/memcheck/demo.anti" "${text}")
endfunction()

function(run_anti out_status out_err)
    execute_process(COMMAND "${ANTI}" ${ARGN} --runtime "${RUNTIME}"
                            --llvm-mc "${LLVM_MC}"
                    WORKING_DIRECTORY "${WORK}/project"
                    RESULT_VARIABLE status OUTPUT_VARIABLE out
                    ERROR_VARIABLE err ENCODING NONE)
    set(${out_status} "${status}" PARENT_SCOPE)
    set(${out_err} "${err}" PARENT_SCOPE)
endfunction()

# Each pattern after the case is one the report must hold. A pattern
# that names a frame is read on macOS and Linux alone.
function(reports mode case)
    write_case(${case})
    run_anti(status err run ${mode} --memory-checks)
    if(NOT status EQUAL 1)
        message(FATAL_ERROR "anti run ${mode} of ${case} ended with "
                            "${status}:\n${err}")
    endif()
    foreach(pattern IN LISTS ARGN)
        if(NOT named AND pattern MATCHES "memcheck")
            continue()
        endif()
        if(NOT err MATCHES "${pattern}")
            message(FATAL_ERROR "anti run ${mode} of ${case} printed no "
                                "`${pattern}`:\n${err}")
        endif()
    endforeach()
endfunction()

set(at "#[0-9]+ 0x[0-9a-f]+ in memcheck\\.demo\\.")
set(source "[^ \n]*demo\\.anti")

# A dev build without the option comes first, so the objects of the run
# with it cannot come from the cache.
write_case(use_after_free)
run_anti(status err build)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "anti build ended with ${status}:\n${err}")
endif()

foreach(mode "" --release)
    reports("${mode}" use_after_free
        "ERROR: AddressSanitizer: heap-use-after-free"
        "READ of size 8"
        "#0 0x[0-9a-f]+ in memcheck\\.demo\\.read_after_free ${source}:29\n"
        "freed by thread T0 here:\n[^\n]*\n *${at}read_after_free ${source}:28\n"
        "previously allocated by thread T0 here:\n[^\n]*\n *${at}read_after_free ${source}:26\n")
endforeach()
reports("" double_free
    "ERROR: AddressSanitizer: attempting double-free"
    "#1 0x[0-9a-f]+ in memcheck\\.demo\\.free_twice ${source}:37\n")
reports("" overflow
    "ERROR: AddressSanitizer: heap-buffer-overflow"
    "WRITE of size 8"
    "#0 0x[0-9a-f]+ in memcheck\\.demo\\.write_past_end ${source}:44\n")
if(named)
    reports("" leak
        "ERROR: LeakSanitizer: detected memory leaks"
        "Direct leak of 8 byte\\(s\\) in 1 object\\(s\\) allocated from:\n[^\n]*\n *#1 0x[0-9a-f]+ in memcheck\\.demo\\.kept ${source}:50\n"
        "SUMMARY: AddressSanitizer: 8 byte\\(s\\) leaked in 1 allocation\\(s\\)")
endif()

# The clean case runs to its end, with the leak check on where there is
# one, and reports nothing.
write_case(clean)
run_anti(status err run --memory-checks)
if(NOT status EQUAL 0 OR NOT err STREQUAL "")
    message(FATAL_ERROR "anti run of clean ended with ${status}:\n${err}")
endif()

# A dev build without the option after one with it takes objects without
# the checks, so the read after free goes unreported.
write_case(use_after_free)
run_anti(status err run)
if(err MATCHES "AddressSanitizer")
    message(FATAL_ERROR "anti run without --memory-checks reported:\n${err}")
endif()

# `anti test` reports the second free of a test with its frames.
get_filename_component(tests_root "${TESTS}" DIRECTORY)
execute_process(COMMAND "${ANTI}" test --memory-checks --work "${WORK}/tests"
                        --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}"
                        -I "${tests_root}" "${TESTS}"
                RESULT_VARIABLE status OUTPUT_VARIABLE out
                ERROR_VARIABLE err ENCODING NONE)
if(status EQUAL 0 OR NOT err MATCHES
   "ERROR: AddressSanitizer: attempting double-free")
    message(FATAL_ERROR "anti test ended with ${status}:\n${out}${err}")
endif()
if(named AND NOT err MATCHES
   "#1 0x[0-9a-f]+ in memory_checks_tests\\.frees_twice [^ \n]*memory_checks_tests\\.anti:22\n")
    message(FATAL_ERROR "anti test named no frame of the test:\n${err}")
endif()

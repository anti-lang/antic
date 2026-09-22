# Run `anti test` over one module of the standard library, first in dev
# mode and then in release mode, and refuse a run that reported a failed
# module. Run with cmake -P and these values:
#   ANTI      the anti executable
#   RUNTIME   the runtime archive
#   LLVM_MC   the llvm-mc executable
#   STD       the source root of the standard library
#   SOURCE    the module under test
#   WORK      a directory for the library files, the objects and the runner
#
# The module imports other modules of the standard library, so the dev
# run links an object of each. Both runs report the same tests, and the
# release run has the assertions off.

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

function(run_tests release out_text)
    set(extra "")
    if(release)
        set(extra --release)
    endif()
    execute_process(
        COMMAND "${ANTI}" test ${extra} --work "${WORK}/${release}"
                -I "${STD}" --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}"
                "${SOURCE}"
        RESULT_VARIABLE status
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "anti test failed with ${status}\n${out}${err}")
    endif()
    set(${out_text} "${out}${err}" PARENT_SCOPE)
endfunction()

function(ok_names text out_names)
    string(REGEX MATCHALL "ok [A-Za-z0-9_.]+" found "${text}")
    set(${out_names} "${found}" PARENT_SCOPE)
endfunction()

run_tests(OFF dev_text)
ok_names("${dev_text}" dev_names)
list(LENGTH dev_names dev_count)
if(dev_count LESS 1)
    message(FATAL_ERROR "the dev run reported no test\n${dev_text}")
endif()

run_tests(ON release_text)
ok_names("${release_text}" release_names)
if(NOT dev_names STREQUAL release_names)
    message(FATAL_ERROR
        "the release run reported other tests than the dev run\n"
        "dev: ${dev_names}\nrelease: ${release_names}")
endif()

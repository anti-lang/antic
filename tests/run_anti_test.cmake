# Drive `anti test` over the modules of tests/anti-test and check what it
# reports. Run with cmake -P and these values:
#   ANTI      the anti executable
#   ANTIC     the antic executable
#   RUNTIME   the runtime archive
#   LLVM_MC   the llvm-mc executable
#   MODULES   the directory that holds com/example/*.anti
#   WORK      a directory for the library files, the objects and the runner
#
# The run covers the five things the blocks promise: a module with both
# blocks, a fixture two tests share, `setup` and `teardown` around every
# test, a module whose tests reach another module, and the report of a
# failed assertion with its file and its line.
# It then compiles the same module without --tests and shows that no name
# of either block reaches the assembly or the library file.

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

function(run_tests source release out_status out_text)
    set(extra "")
    if(release)
        set(extra --release)
    endif()
    execute_process(
        COMMAND "${ANTI}" test ${extra} --work "${WORK}" -I "${MODULES}"
                --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}"
                "${MODULES}/${source}"
        RESULT_VARIABLE status
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
        ENCODING NONE)
    set(${out_status} "${status}" PARENT_SCOPE)
    set(${out_text} "${out}${err}" PARENT_SCOPE)
endfunction()

function(expect text needle what)
    string(FIND "${text}" "${needle}" at)
    if(at LESS 0)
        message(FATAL_ERROR "${what}: no `${needle}` in\n${text}")
    endif()
endfunction()

function(refuse text needle what)
    string(FIND "${text}" "${needle}" at)
    if(NOT at LESS 0)
        message(FATAL_ERROR "${what}: `${needle}` is in\n${text}")
    endif()
endfunction()

# A module with both blocks. Both tests take the one fixture, and setup
# and teardown run around each of them.
run_tests(com/example/stack.anti OFF status text)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "anti test failed with ${status}\n${text}")
endif()
expect("${text}" "ok com.example.stack.push_grows" "the first test")
expect("${text}" "ok com.example.stack.top_is_last" "the second test")
string(REGEX MATCHALL "setup" setups "${text}")
list(LENGTH setups setup_count)
if(NOT setup_count EQUAL 2)
    message(FATAL_ERROR "setup ran ${setup_count} times, expected 2\n${text}")
endif()
string(REGEX MATCHALL "teardown" teardowns "${text}")
list(LENGTH teardowns teardown_count)
if(NOT teardown_count EQUAL 2)
    message(FATAL_ERROR
        "teardown ran ${teardown_count} times, expected 2\n${text}")
endif()

# A module whose tests reach another module. A dev build compiles one
# module into its own object, so the runner links an object of every
# library file the module needs.
run_tests(com/example/greeting.anti OFF status text)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "anti test failed with ${status}\n${text}")
endif()
expect("${text}" "ok com.example.greeting.greeting_knows_its_word"
       "the test of an importing module")

# A failed assertion names the test, then the file and the line the
# compiler wrote into the message.
run_tests(com/example/failing.anti OFF status text)
if(status EQUAL 0)
    message(FATAL_ERROR "a failing test reported success\n${text}")
endif()
expect("${text}" "ok com.example.failing.two_is_two" "the passing test")
expect("${text}" "FAIL com.example.failing.two_is_three" "the failing test")
expect("${text}" "com/example/failing.anti:25: assertion failed: n == 3"
       "the position of the assertion")

# --release runs the same tests with the assertions off, so the module
# that fails in dev mode passes.
run_tests(com/example/failing.anti ON status text)
if(NOT status EQUAL 0)
    message(FATAL_ERROR
        "--release ran the assertions after all, with ${status}\n${text}")
endif()
refuse("${text}" "FAIL" "the release run")

# A build that is not `anti test` drops both blocks. Neither the assembly
# nor the library file holds a name of either.
execute_process(
    COMMAND "${ANTIC}" --runtime "${RUNTIME}" -S
            -o "${WORK}/plain.s" "${MODULES}/com/example/stack.anti"
    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic -S failed with ${status}\n${err}")
endif()
file(READ "${WORK}/plain.s" assembly)
foreach(name push_grows top_is_last full_stack setup teardown)
    refuse("${assembly}" "${name}" "the assembly without --tests")
endforeach()

execute_process(
    COMMAND "${ANTIC}" --runtime "${RUNTIME}" -c
            -o "${WORK}/plain.antl" "${MODULES}/com/example/stack.anti"
    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic -c failed with ${status}\n${err}")
endif()
file(READ "${WORK}/plain.antl" library HEX)
foreach(name push_grows top_is_last full_stack)
    string(HEX "${name}" needle)
    refuse("${library}" "${needle}" "the library file without --tests")
endforeach()

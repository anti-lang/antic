# Step 5 of ./r --dry-run on the copy of tests/release_copy.cmake stops
# when the check with the network off fails on a VM or when the network
# of a VM does not go off, brings the network back, and runs again on
# the rerun.
#
#   cmake -DROOT=<repository> -DWORK=<dir> -DCC=<compiler>
#         -DHOST_LINK=<options> -DSDK=<Apple SDK> -DLLVM_BIN=<dir>
#         -P tests/runners/run_release_vm_failures.cmake
#
# tests/release_copy.cmake describes the values.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../release_copy.cmake")
if(release_skipped)
    return()
endif()

# Run step 5 of the copy again with <variable> set to <value> for the
# stand-in of ssh, and answer with the status, the output and the calls.
function(vm_run variable value status_variable out_variable calls_variable)
    file(REMOVE "${copy}/build/dist/dry-run/state/05-vms" "${WORK}/ssh.log")
    set(ENV{${variable}} "${value}")
    set(ENV{PATH} "${WORK}/bin:${saved_path}")
    execute_process(COMMAND "${copy}/r" --dry-run WORKING_DIRECTORY "${copy}"
                    RESULT_VARIABLE failed OUTPUT_VARIABLE out
                    ERROR_VARIABLE err ENCODING NONE)
    set(ENV{PATH} "${saved_path}")
    unset(ENV{${variable}})
    file(READ "${WORK}/ssh.log" calls)
    set("${status_variable}" "${failed}" PARENT_SCOPE)
    set("${out_variable}" "${out}${err}" PARENT_SCOPE)
    set("${calls_variable}" "${calls}" PARENT_SCOPE)
endfunction()

# A check that fails with the network off stops the step, and the network
# of that VM comes back all the same. The suite does not run, and the step
# leaves no stamp.
vm_run(SSH_FAILS "run.sh offline" refused log calls)
if(refused EQUAL 0 OR NOT log MATCHES "step 5: anti-linux failed")
    message(FATAL_ERROR "./r went on after the check with the network off "
                        "failed on anti-linux\n${log}")
endif()
if(NOT calls MATCHES "anti-linux off [^\n]*run\\.sh offline\n.*anti-linux on [^\n]*destroy table inet anti_offline"
   OR EXISTS "${WORK}/offline-anti-linux")
    message(FATAL_ERROR "a failed check left anti-linux off the network:\n"
                        "${calls}")
endif()
if(calls MATCHES "run\\.sh suite" OR
   EXISTS "${copy}/build/dist/dry-run/state/05-vms")
    message(FATAL_ERROR "step 5 ran the suite or left its stamp after a failed "
                        "check:\n${calls}")
endif()

# A command that turns the network off and changes nothing is found by the
# probe, and the check does not run on a VM that reaches the network.
vm_run(SSH_OFF_IGNORED yes refused log calls)
if(refused EQUAL 0 OR NOT log MATCHES "step 5: anti-linux still reaches the network")
    message(FATAL_ERROR "./r ran the check on a VM whose network is on\n${log}")
endif()
if(calls MATCHES "run\\.sh offline")
    message(FATAL_ERROR "the check ran with the network of anti-linux on:\n"
                        "${calls}")
endif()

# A rerun after either failure runs the step again, and it passes.
vm_run(SSH_NOTHING "" failed log calls)
if(NOT failed EQUAL 0 OR
   NOT EXISTS "${copy}/build/dist/dry-run/state/05-vms")
    message(FATAL_ERROR "./r did not run step 5 again after it failed\n${log}")
endif()

file(REMOVE_RECURSE "${WORK}")

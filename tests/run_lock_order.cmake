# The lock-order check of a dev build. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime archive
#   SOURCE    tests/traps/lock_order.anti
#   WORK      a directory for the files
#
# The dev build reports each pair of locks taken in opposite orders once,
# with the four sites, and runs to its end. The release build records
# nothing and prints nothing.

file(MAKE_DIRECTORY "${WORK}")
function(build name)
    execute_process(COMMAND "${ANTIC}" ${ARGN} --llvm-mc "${LLVM_MC}"
                            --runtime "${RUNTIME}" -o "${WORK}/${name}"
                            "${SOURCE}"
                    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0 OR NOT err STREQUAL "")
        message(FATAL_ERROR "antic failed for ${name}\n${err}")
    endif()
endfunction()

build(lock_order_dev --dev)
execute_process(COMMAND "${WORK}/lock_order_dev" RESULT_VARIABLE code
                OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
set(head "anti: two locks are taken in opposite orders and can deadlock: ")
set(mutexes "${head}lock_order.anti:53 takes one while holding the one of ")
string(APPEND mutexes "lock_order.anti:52, and lock_order.anti:42 takes that ")
string(APPEND mutexes "one while holding the one of lock_order.anti:41\n")
set(object "${head}lock_order.anti:28 takes one while holding the one of ")
string(APPEND object "lock_order.anti:57, and lock_order.anti:23 takes that ")
string(APPEND object "one while holding the one of lock_order.anti:21\n")
if(NOT code EQUAL 0 OR NOT out STREQUAL "2\n" OR
   NOT err STREQUAL "${mutexes}${object}")
    message(FATAL_ERROR "the dev build exited with ${code} and printed "
                        "`${out}` and `${err}`")
endif()

build(lock_order_release)
execute_process(COMMAND "${WORK}/lock_order_release" RESULT_VARIABLE code
                OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
if(NOT code EQUAL 0 OR NOT out STREQUAL "2\n" OR NOT err STREQUAL "")
    message(FATAL_ERROR "the release build exited with ${code} and printed "
                        "`${out}` and `${err}`")
endif()

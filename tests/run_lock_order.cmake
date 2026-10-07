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

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

file(MAKE_DIRECTORY "${WORK}")
antic_program("${WORK}/lock_order_dev" "${SOURCE}" --dev)
set(head "anti: two locks are taken in opposite orders and can deadlock: ")
set(mutexes "${head}lock_order.anti:53 takes one while holding the one of ")
string(APPEND mutexes "lock_order.anti:52, and lock_order.anti:42 takes that ")
string(APPEND mutexes "one while holding the one of lock_order.anti:41\n")
set(object "${head}lock_order.anti:28 takes one while holding the one of ")
string(APPEND object "lock_order.anti:57, and lock_order.anti:23 takes that ")
string(APPEND object "one while holding the one of lock_order.anti:21\n")
program_expect("lock order dev" COMMAND "${WORK}/lock_order_dev"
               OUT "2\n" ERR "${mutexes}${object}")

antic_program("${WORK}/lock_order_release" "${SOURCE}")
program_expect("lock order release" COMMAND "${WORK}/lock_order_release"
               OUT "2\n")

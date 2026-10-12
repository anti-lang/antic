# Run one group of the unit tests and compare what it writes to the
# standard error. Run with cmake -P and these values:
#   PROGRAM   antic_unit_tests
#   GROUP     the group, tests/unit/test_<group>.c
#   EXPECTED  the file that holds the whole standard error
#   WORK      a directory this run writes into
#
# DESIGN: a unit test of anti reads what a function gives back, and the
# function writes its message to the standard error. The group runs in a
# directory of its own and names its files without a directory, so every
# line is the same on every host. The file is an expected output that the
# test diagnostics_covered reads.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

# The group writes its files into WORK/run, and program_expect the two
# outputs beside it.
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/run")
program_expect("unit_${GROUP}"
               COMMAND "${CMAKE_COMMAND}" -E chdir "${WORK}/run"
                       "${PROGRAM}" "${GROUP}"
               ANY_OUT ERR_FILE "${EXPECTED}")

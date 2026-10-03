# An object whose table is zero traps with the name of its class. Run with
# cmake -P and these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime archive
#   SOURCE    tests/traps/table.anti
#   WORK      a directory for the files
#
# delete, destroy and dup are calls into the runtime, which checks in
# every mode, and so do the teardown and the copy of a class value held
# inline. A dispatch, `is` and `as` check in dev mode, and release mode
# keeps the raw load.

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

function(traps build case class)
    program_expect("table ${build} ${case}"
                   COMMAND "${WORK}/table_${build}" ${case} ABORTS
                   ERR "table not set: the object is no ${class}\n")
endfunction()

file(MAKE_DIRECTORY "${WORK}")
antic_program("${WORK}/table_release" "${SOURCE}")
antic_program("${WORK}/table_dev" "${SOURCE}" --dev)
foreach(case delete destroy dup)
    traps(release ${case} Square)
    traps(dev ${case} Square)
endforeach()
foreach(case call is as)
    traps(dev ${case} Shape)
endforeach()
foreach(case inline copy_inline)
    traps(release ${case} Part)
    traps(dev ${case} Part)
endforeach()

# --backend native: the labels below are the ones of the native back end,
# whichever back end ANTIC_BACKEND names. The step switch rewrites it.
execute_process(COMMAND "${ANTIC}" --backend native --runtime "${RUNTIME}"
                        -S -o "${WORK}/release.s" "${SOURCE}"
                RESULT_VARIABLE status)
# The function run holds the dispatch, `is` and `as`. The teardown and
# the copy of Holder, which follow it, check in every mode.
#
# Each host spells the two labels its own way: `table.run:` on ELF,
# `_table.run:` on Mach-O and `_A5table_run:` on COFF, which writes the
# last segment of a dotted name after an `_`. The character class takes
# the dot and the underscore, so one pattern reads every host.
file(READ "${WORK}/release.s" release)
string(REGEX MATCH "table[._]run:.*table[._]main:" run "${release}")
if(NOT status EQUAL 0 OR run STREQUAL "" OR
   run MATCHES "anti_rt_table_unset")
    message(FATAL_ERROR "release mode checks a table in a dispatch")
endif()

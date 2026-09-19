# An object whose table is zero traps with the name of its class. Run with
# cmake -P and these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime archive
#   SOURCE    tests/traps/table.anti
#   WORK      a directory for the files
#
# delete, destroy and dup are calls into the runtime, which checks in
# every mode. A dispatch, `is` and `as` check in dev mode, and release
# mode keeps the raw load.

function(build name)
    execute_process(COMMAND "${ANTIC}" ${ARGN} --llvm-mc "${LLVM_MC}"
                            --runtime "${RUNTIME}" -o "${WORK}/${name}"
                            "${SOURCE}"
                    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "antic failed for ${name}\n${err}")
    endif()
endfunction()

function(traps build case class)
    execute_process(COMMAND "${WORK}/${build}" ${case} RESULT_VARIABLE code
                    OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
    if(code EQUAL 0 OR
       NOT err MATCHES "^table not set: the object is no ${class}\n$")
        message(FATAL_ERROR "${case} in ${build} exited with ${code} and "
                            "printed `${err}`")
    endif()
endfunction()

file(MAKE_DIRECTORY "${WORK}")
build(release)
build(dev --dev)
foreach(case delete destroy dup)
    traps(release ${case} Square)
    traps(dev ${case} Square)
endforeach()
foreach(case call is as)
    traps(dev ${case} Shape)
endforeach()

execute_process(COMMAND "${ANTIC}" --runtime "${RUNTIME}" -S
                        -o "${WORK}/release.s" "${SOURCE}"
                RESULT_VARIABLE status)
file(READ "${WORK}/release.s" release)
if(NOT status EQUAL 0 OR release MATCHES "anti_rt_table_unset")
    message(FATAL_ERROR "release mode checks a table")
endif()

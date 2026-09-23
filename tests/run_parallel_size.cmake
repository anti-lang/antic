# A `parallel` whose results no size holds stops before a worker runs. Run
# with cmake -P and these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime archive
#   SOURCE    tests/traps/parallel_size.anti
#   WORK      a directory for the files

file(MAKE_DIRECTORY "${WORK}")
execute_process(COMMAND "${ANTIC}" --llvm-mc "${LLVM_MC}"
                        --runtime "${RUNTIME}" -o "${WORK}/parallel_size"
                        "${SOURCE}"
                RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic failed\n${err}")
endif()
execute_process(COMMAND "${WORK}/parallel_size" RESULT_VARIABLE code
                OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
set(expected "^anti: the results of 2305843009213693952 chunks of 8 bytes ")
string(APPEND expected "each do not fit in memory\n$")
if(code EQUAL 0 OR NOT err MATCHES "${expected}")
    message(FATAL_ERROR "the program exited with ${code} and printed "
                        "`${out}${err}`")
endif()

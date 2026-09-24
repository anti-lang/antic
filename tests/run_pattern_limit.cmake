# A search with a pattern literal that reaches the match limit stops the
# program with the pattern and the line. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime archive
#   SOURCE    tests/traps/pattern_limit.anti
#   WORK      a directory for the files

file(MAKE_DIRECTORY "${WORK}")
execute_process(COMMAND "${ANTIC}" --llvm-mc "${LLVM_MC}"
                        --runtime "${RUNTIME}" -o "${WORK}/pattern_limit"
                        "${SOURCE}"
                RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic failed\n${err}")
endif()
execute_process(COMMAND "${WORK}/pattern_limit" RESULT_VARIABLE code
                OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
set(expected "pattern_limit\\.anti:13: the pattern ")
string(APPEND expected "`\\(\\*LIMIT_MATCH=1000\\)\\^\\(\\\\w\\+\\\\s\\?\\)\\*\\$` ")
string(APPEND expected "reached the match limit\n$")
if(code EQUAL 0 OR code EQUAL 1 OR NOT err MATCHES "${expected}")
    message(FATAL_ERROR "the program exited with ${code} and printed "
                        "`${out}${err}`")
endif()

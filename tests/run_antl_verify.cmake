# M7: the IR of a library file that antic --dev compiles passes ir_verify.
# The copy of scale.antl returns i32 from a function that returns i64,
# which the reader accepts and the verifier refuses. Run with cmake -P and
# these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   POKE      the poke_byte executable
#   LIBS      the directory of the library files
#   WORK      a scratch directory

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(scale "${LIBS}/com/example/scale.antl")
file(READ "${scale}" hex HEX)
# ret i64 on line 4, with no result.
string(FIND "${hex}" "550404000000ffffffff" at)
if(at LESS 0)
    message(FATAL_ERROR "no ret i64 in ${scale}")
endif()
math(EXPR type_byte "${at} / 2 + 1")
execute_process(
    COMMAND "${POKE}" "${scale}" "${WORK}/scale.antl" "${type_byte}" 3
    RESULT_VARIABLE status)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "poke_byte failed with ${status}")
endif()
execute_process(
    COMMAND "${ANTIC}" --dev --llvm-mc "${LLVM_MC}" -I "${LIBS}"
            -o "${WORK}/scale_antl" "${WORK}/scale.antl"
    RESULT_VARIABLE status
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
    ENCODING NONE)
if(status EQUAL 0)
    message(FATAL_ERROR "antic --dev compiled a library file that fails "
                        "verification\n${out}${err}")
endif()
string(FIND "${err}" "ret i32 in a function that returns i64" found)
if(found LESS 0)
    message(FATAL_ERROR "antic --dev failed with ${status} without the "
                        "verifier's message\n${out}${err}")
endif()

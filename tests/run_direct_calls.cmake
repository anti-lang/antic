# A release build calls the function of an object of a known class
# directly, as "Tables and dispatch" of
# docs/work-order-llvm-optimization.md asks: the table facts forward the
# table pointer of the object to the call through its table, across the
# call of the `created` hook, and opt then calls the function directly or
# inlines it. The test reads the bitcode that opt wrote, as text, and
# refuses a call through a pointer in the function FUNCTION. Run with
# cmake -P and these values:
#   ANTIC     the antic executable
#   RUNTIME   the runtime directory, whose bin/ holds opt and llc
#   OPT       the opt executable
#   SOURCE    the program
#   FUNCTION  the LLVM name of the function, without the @
#   WORK      a directory for the files

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
get_filename_component(name "${SOURCE}" NAME_WE)
set(program "${WORK}/${name}")
execute_process(COMMAND "${ANTIC}" --runtime "${RUNTIME}" --keep-llvm
                        -o "${program}" "${SOURCE}"
                RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic failed\n${err}")
endif()
execute_process(COMMAND "${OPT}" -S -o "${program}.opt.ll" "${program}.bc"
                RESULT_VARIABLE status)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "opt -S failed")
endif()
file(READ "${program}.opt.ll" text)
string(FIND "${text}" "@${FUNCTION}(" start)
if(start EQUAL -1)
    message(FATAL_ERROR "${program}.opt.ll defines no ${FUNCTION}")
endif()
string(SUBSTRING "${text}" ${start} -1 body)
string(FIND "${body}" "\n}\n" end)
string(SUBSTRING "${body}" 0 ${end} body)
# A call through a pointer names a value where a direct call names @.
if(body MATCHES "call [^@%\n]*%[^ (\n]+\\(")
    message(FATAL_ERROR "${FUNCTION} calls through a table after opt, "
                        "${program}.opt.ll:\n${body}")
endif()

# A release build calls the function of an object of a known class
# directly, as "Tables and dispatch" of
# docs/work-order-llvm-optimization.md asks: the table facts forward the
# table pointer of the object to the call through its table, across the
# call of the `created` hook, and opt then calls the function directly or
# inlines it. The test reads the bitcode that opt wrote, as text, and
# refuses a call through a pointer in the function FUNCTION. The build
# takes --lto none, whose opt runs the whole pipeline before llc. Run with
# cmake -P and these values:
#   ANTIC     the antic executable
#   RUNTIME   the runtime directory, whose bin/ holds opt and llc
#   OPT       the opt executable
#   SOURCE    the program
#   FUNCTION  the LLVM name of the function on ELF, without the @
#   TARGET    the target the program is built for
#   WORK      a directory for the files

if(NOT EXISTS "${RUNTIME}/sysroot/${TARGET}")
    message("SKIP: the runtime archive has no sysroot for ${TARGET}")
    return()
endif()
file(GLOB runtime_library "${RUNTIME}/lib/${TARGET}/*/libanti_rt.a"
     "${RUNTIME}/lib/${TARGET}/*/anti_rt.lib")
if(runtime_library STREQUAL "")
    message("SKIP: the runtime archive has no runtime library for ${TARGET}")
    return()
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
get_filename_component(name "${SOURCE}" NAME_WE)
set(program "${WORK}/${name}")
execute_process(COMMAND "${ANTIC}" --target "${TARGET}" --runtime "${RUNTIME}"
                        --lto none --keep-llvm -o "${program}" "${SOURCE}"
                RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic failed\n${err}")
endif()
execute_process(COMMAND "${OPT}" -S -o "${program}.opt.ll" "${program}.bc"
                RESULT_VARIABLE status)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "opt -S failed")
endif()
# Each format spells the name its own way: `table_hook.main` on ELF and
# Mach-O, whose leading `_` llc writes, and `_A10table_hook_main` on COFF,
# which puts the length before the module and an `_` after it, as
# target_mangle of src/antic/target.c does. The pattern takes a module of
# one segment, which every program of tests/programs is.
string(REGEX MATCH "^([^.]+)\\.([^.]+)$" parts "${FUNCTION}")
if(parts STREQUAL "")
    message(FATAL_ERROR "FUNCTION ${FUNCTION} is no module.name")
endif()
set(pattern "@(_A[0-9]+)?${CMAKE_MATCH_1}[._]${CMAKE_MATCH_2}\\(")
file(READ "${program}.opt.ll" text)
string(REGEX MATCH "${pattern}" found "${text}")
if(found STREQUAL "")
    message(FATAL_ERROR "${program}.opt.ll defines no ${FUNCTION}")
endif()
string(FIND "${text}" "${found}" start)
string(SUBSTRING "${text}" ${start} -1 body)
string(FIND "${body}" "\n}\n" end)
string(SUBSTRING "${body}" 0 ${end} body)
# A call through a pointer names a value where a direct call names @.
if(body MATCHES "call [^@%\n]*%[^ (\n]+\\(")
    message(FATAL_ERROR "${FUNCTION} calls through a table after opt, "
                        "${program}.opt.ll:\n${body}")
endif()

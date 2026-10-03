# A release build with -g under the LLVM back end inlines a small callee
# and keeps the line of the callee in the line table of the caller, as
# "Debug information" of docs/work-order-llvm-back-end.md says. The pinned
# set holds no llvm-dwarfdump, so llvm-objdump reads the table: `-l`
# prints the file and the line of each run of instructions. Run with
# cmake -P and these values:
#   ANTIC     the antic executable
#   RUNTIME   the runtime archive
#   OBJDUMP   the llvm-objdump of the runtime archive
#   SOURCE    tests/programs/llvm_inline_lines.anti
#   HOST      the target of the host
#   WORK      a directory for the files

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

file(MAKE_DIRECTORY "${WORK}")
file(STRINGS "${SOURCE}" lines)
set(callee 0)
set(number 0)
foreach(line IN LISTS lines)
    math(EXPR number "${number} + 1")
    if(line MATCHES "return n \\* 3 \\+ 1;")
        set(callee ${number})
    endif()
endforeach()
get_filename_component(file "${SOURCE}" NAME)

# The host's program, and a Linux program, whose executable holds its
# line table. A Mach-O executable leaves the table in the object.
set(targets ${HOST} linux-x86_64)
list(REMOVE_DUPLICATES targets)
foreach(target IN LISTS targets)
    set(program "${WORK}/${target}")
    execute_process(COMMAND "${ANTIC}" --backend llvm -g --target ${target}
                            --runtime "${RUNTIME}" -o "${program}"
                            "${SOURCE}"
                    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "antic failed for ${target}\n${err}")
    endif()
    set(table "${program}")
    if(target MATCHES "^macos-")
        set(table "${program}.o")
    endif()
    execute_process(COMMAND "${OBJDUMP}" -t "${table}"
                    OUTPUT_VARIABLE symbols ENCODING NONE)
    if(symbols MATCHES "thrice")
        message(FATAL_ERROR "${target} kept the callee:\n${symbols}")
    endif()
    execute_process(COMMAND "${OBJDUMP}" -d -l "${table}"
                    RESULT_VARIABLE status OUTPUT_VARIABLE listing
                    ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0 OR NOT listing MATCHES "${file}:${callee}\n")
        message(FATAL_ERROR "the line table of ${target} names no line "
                            "${callee} of ${file}:\n${listing}${err}")
    endif()
endforeach()

program_expect("the host's program" COMMAND "${WORK}/${HOST}"
               OUT_MATCH "^x\n7\n$")

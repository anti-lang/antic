# Every object of a program and of its runtime carries the
# address-significance table, so the safe folding of lld folds only what no
# program compares by address. The test reads the object antic writes beside
# a program of --lto none, the object runtime of every level of the target,
# its licence stub and the glue of anti.regex. llc writes the table with
# -addrsig and clang with -faddrsig. See the entry on folding under "Scope
# and toolchain" in docs/decisions.md. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   RUNTIME   the runtime directory
#   OBJDUMP   the llvm-objdump executable
#   SOURCE    a program
#   TARGET    the target name
#   WORK      a directory for the files
#
# The table is `.llvm_addrsig` in an ELF or COFF object and `__llvm_addrsig`
# in a Mach-O one. llvm-objdump -h names every member of an archive with
# `file format`, so each member holds one table where both counts agree.

cmake_minimum_required(VERSION 3.21)

if(NOT EXISTS "${RUNTIME}/sysroot/${TARGET}")
    message("SKIP: the runtime archive has no sysroot for ${TARGET}")
    return()
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

set(program "${WORK}/program")
execute_process(COMMAND "${ANTIC}" --target "${TARGET}" --runtime "${RUNTIME}"
                        --lto none -o "${program}" "${SOURCE}"
                RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic --lto none failed\n${err}")
endif()
if("${TARGET}" MATCHES "^windows-")
    set(objects "${program}.obj")
    set(library anti_rt.lib)
    set(regex anti_rt_regex.lib)
    set(stub anti_rt_license_stub.obj)
else()
    set(objects "${program}.o")
    set(library libanti_rt.a)
    set(regex libanti_rt_regex.a)
    set(stub anti_rt_license_stub.o)
endif()
# The object runtime of each level, and on Linux that of the glibc mode.
file(GLOB runtimes "${RUNTIME}/lib/${TARGET}/*/${library}"
     "${RUNTIME}/lib/${TARGET}/*/${stub}"
     "${RUNTIME}/lib/${TARGET}-glibc/*/${library}"
     "${RUNTIME}/lib/${TARGET}/${regex}")
if(runtimes STREQUAL "")
    message(FATAL_ERROR "the runtime archive has no ${library} for ${TARGET}")
endif()
list(APPEND objects ${runtimes})

foreach(file IN LISTS objects)
    execute_process(COMMAND "${OBJDUMP}" -h "${file}"
                    RESULT_VARIABLE status OUTPUT_VARIABLE text
                    ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "llvm-objdump -h ${file} failed\n${err}")
    endif()
    string(REGEX MATCHALL "file format" members "${text}")
    string(REGEX MATCHALL " (\\.|__)llvm_addrsig " tables "${text}")
    list(LENGTH members member_count)
    list(LENGTH tables table_count)
    if(member_count EQUAL 0 OR NOT member_count EQUAL table_count)
        message(FATAL_ERROR "${file}: ${table_count} of ${member_count} "
                            "objects carry an address-significance table")
    endif()
endforeach()

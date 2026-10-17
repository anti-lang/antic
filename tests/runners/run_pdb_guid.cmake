# Link a program for a Windows target on this host and check the PDB that
# lld-link wrote beside it. A Windows executable carries no symbol table,
# so what names a frame of a report from a user is the PDB, and what ties
# the two together is the CodeView record of the debug directory. The link
# passes /DEBUG with or without -g, so both builds are read here.
#
#   cmake -DANTIC=<antic> -DLLVM_MC=<llvm-mc> -DREADOBJ=<llvm-readobj>
#         -DCHECK=<tools/check-pdb.cmake> -DRUNTIME=<runtime archive>
#         -DSOURCE=<file.anti> -DWORK=<dir> -DTARGET=<target>
#         -P tests/runners/run_pdb_guid.cmake

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

if(NOT EXISTS "${RUNTIME}/sysroot/${TARGET}")
    message("SKIP: the runtime archive has no sysroot for ${TARGET}")
    return()
endif()
file(GLOB runtime_library "${RUNTIME}/lib/${TARGET}/*/anti_rt.lib")
if(runtime_library STREQUAL "")
    message("SKIP: the runtime archive has no runtime library for ${TARGET}")
    return()
endif()

file(MAKE_DIRECTORY "${WORK}")
foreach(mode release debug)
    set(exe "${WORK}/${TARGET}-${mode}.exe")
    set(pdb "${WORK}/${TARGET}-${mode}.pdb")
    set(flags "")
    if(mode STREQUAL "debug")
        set(flags -g)
    endif()
    file(REMOVE "${exe}" "${pdb}")
    antic_build(--target "${TARGET}" --llvm-mc "${LLVM_MC}" --runtime
                "${RUNTIME}" ${flags} -o "${exe}" "${SOURCE}")
    if(NOT EXISTS "${pdb}")
        message(FATAL_ERROR "the ${mode} link for ${TARGET} wrote no ${pdb}")
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DBINARY=${exe}" "-DPDB=${pdb}"
                            "-DREADOBJ=${READOBJ}" -P "${CHECK}"
                    RESULT_VARIABLE refused OUTPUT_VARIABLE out
                    ERROR_VARIABLE err ENCODING NONE)
    if(NOT refused EQUAL 0)
        message(FATAL_ERROR "${TARGET} ${mode}\n${out}${err}")
    endif()
    message(STATUS "${TARGET} ${mode}: ${out}${err}")
endforeach()

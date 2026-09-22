# Link a program for a Windows target on this host, without -g and with it,
# and check that the PDB names every function of the program. A release
# writes each function as a static COFF symbol, and lld-link carries only
# external symbols into the PDB. The name reaches it through the symbol
# record that antic writes for each function into .debug$S.
#
#   cmake -DANTIC=<antic> -DLLVM_MC=<llvm-mc> -DRUNTIME=<runtime archive>
#         -DSOURCE=<file.anti> -DNAMES=<symbol;...> -DWORK=<dir>
#         -DTARGET=<target> -P tests/run_pdb_names.cmake

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
    execute_process(
        COMMAND "${ANTIC}" --target "${TARGET}" --llvm-mc "${LLVM_MC}"
                --runtime "${RUNTIME}" ${flags} -o "${exe}" "${SOURCE}"
        RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT status EQUAL 0 OR NOT "${out}${err}" STREQUAL "")
        message(FATAL_ERROR "antic failed for ${TARGET} ${mode}\n${out}${err}")
    endif()
    if(NOT EXISTS "${pdb}")
        message(FATAL_ERROR "the ${mode} link for ${TARGET} wrote no ${pdb}")
    endif()
    set(missing "")
    foreach(name IN LISTS NAMES)
        file(STRINGS "${pdb}" found REGEX "^${name}$")
        if(found STREQUAL "")
            list(APPEND missing "${name}")
        endif()
    endforeach()
    if(NOT missing STREQUAL "")
        message(FATAL_ERROR "the ${mode} PDB for ${TARGET} does not name "
                            "${missing}")
    endif()
endforeach()

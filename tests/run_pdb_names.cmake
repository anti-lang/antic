# Link a program for a Windows target on this host, without -g and with it,
# and check that the PDB names every function of the program. A release
# writes each function as a static COFF symbol, and lld-link carries only
# external symbols into the PDB. The name reaches it through the symbol
# record that antic writes for each function into .debug$S.
#
# Neither the PDB nor the executable holds a path of the build: the
# directory the program is built in, or the one of its source. The link
# runs in the directory of its output and names its files from there. Each
# mode links twice, with the absolute runtime archive and with a relative
# one. The runtime stays as given, so the second link holds no path of
# the checkout at all.
#
#   cmake -DANTIC=<antic> -DLLVM_MC=<llvm-mc> -DRUNTIME=<runtime archive>
#         -DSOURCE=<file.anti> -DNAMES=<symbol;...> -DWORK=<dir>
#         -DTARGET=<target> -DROOT=<checkout> -P tests/run_pdb_names.cmake

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
file(RELATIVE_PATH relative_runtime "${WORK}" "${RUNTIME}")
foreach(run release release-relative debug debug-relative)
    string(REGEX REPLACE "-relative$" "" mode "${run}")
    set(exe "${WORK}/${TARGET}-${run}.exe")
    set(pdb "${WORK}/${TARGET}-${run}.pdb")
    set(flags "")
    if(mode STREQUAL "debug")
        set(flags -g)
    endif()
    set(runtime "${RUNTIME}")
    set(checked "${WORK}" "${SOURCE}")
    if(NOT run STREQUAL mode)
        set(runtime "${relative_runtime}")
        list(APPEND checked "${RUNTIME}" "${ROOT}")
    endif()
    file(REMOVE "${exe}" "${pdb}")
    execute_process(
        COMMAND "${ANTIC}" --target "${TARGET}" --llvm-mc "${LLVM_MC}"
                --runtime "${runtime}" ${flags} -o "${exe}" "${SOURCE}"
        WORKING_DIRECTORY "${WORK}"
        RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT status EQUAL 0 OR NOT "${out}${err}" STREQUAL "")
        message(FATAL_ERROR "antic failed for ${TARGET} ${run}\n${out}${err}")
    endif()
    if(NOT EXISTS "${pdb}")
        message(FATAL_ERROR "the ${run} link for ${TARGET} wrote no ${pdb}")
    endif()
    set(missing "")
    foreach(name IN LISTS NAMES)
        file(STRINGS "${pdb}" found REGEX "^${name}$")
        if(found STREQUAL "")
            list(APPEND missing "${name}")
        endif()
    endforeach()
    if(NOT missing STREQUAL "")
        message(FATAL_ERROR "the ${run} PDB for ${TARGET} does not name "
                            "${missing}")
    endif()
    # Each path of the build in both spellings of its separator, and in
    # the form with its links resolved, which a POSIX host writes.
    set(build_paths "")
    foreach(path IN LISTS checked)
        if(path STREQUAL SOURCE)
            get_filename_component(path "${SOURCE}" DIRECTORY)
        endif()
        file(REAL_PATH "${path}" real)
        foreach(form "${path}" "${real}")
            string(REPLACE "/" "\\" native "${form}")
            list(APPEND build_paths "${form}" "${native}")
        endforeach()
    endforeach()
    foreach(file "${exe}" "${pdb}")
        file(STRINGS "${file}" strings)
        foreach(path IN LISTS build_paths)
            string(FIND "${strings}" "${path}" at)
            if(NOT at EQUAL -1)
                message(FATAL_ERROR "${file} holds the path ${path}")
            endif()
        endforeach()
    endforeach()
endforeach()

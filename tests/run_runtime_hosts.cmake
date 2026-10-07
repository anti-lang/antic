# The runtime of linux-arm64 and of its glibc mode is the same whichever
# host built it. A Linux host once built it with outline atomics, since the
# clang driver adds them for a triple that names glibc when it finds a GCC
# of the host, and a Mac built it without. The test refuses a call of the
# helpers of outline atomics, `__aarch64_*`, in every library of objects,
# and the feature `+outline-atomics` in every member of the bitcode, which
# the opt of the runtime archive reads. Run with cmake -P and these values:
#   RUNTIME   the runtime archive
#   LLVM_AR   the llvm-ar executable
#   OBJDUMP   the llvm-objdump executable
#   WORK      a directory for the members

cmake_minimum_required(VERSION 3.21)

set(opt "${RUNTIME}/bin/opt${CMAKE_EXECUTABLE_SUFFIX}")
file(GLOB libraries "${RUNTIME}/lib/linux-arm64*/*/libanti_rt.a")
file(GLOB archives "${RUNTIME}/lib/linux-arm64*/*/bitcode/*/libanti_rt.a")
if(libraries STREQUAL "")
    message("SKIP: the runtime archive holds no runtime of linux-arm64")
    return()
endif()
foreach(library IN LISTS libraries)
    execute_process(COMMAND "${OBJDUMP}" -dr "${library}"
        RESULT_VARIABLE status OUTPUT_FILE "${WORK}.listing"
        ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "llvm-objdump failed on ${library}\n${err}")
    endif()
    file(STRINGS "${WORK}.listing" calls REGEX "__aarch64_")
    if(calls)
        list(GET calls 0 first)
        message(FATAL_ERROR "${library} calls the outline atomics: ${first}")
    endif()
endforeach()
file(REMOVE_RECURSE "${WORK}")
set(index 0)
foreach(archive IN LISTS archives)
    math(EXPR index "${index} + 1")
    set(dir "${WORK}/${index}")
    file(MAKE_DIRECTORY "${dir}")
    execute_process(COMMAND "${LLVM_AR}" x "${archive}"
                    WORKING_DIRECTORY "${dir}" RESULT_VARIABLE status)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "llvm-ar x failed on ${archive}")
    endif()
    file(GLOB members "${dir}/*.o")
    foreach(member IN LISTS members)
        execute_process(COMMAND "${opt}" -S -o "${member}.ll" "${member}"
                        RESULT_VARIABLE status ERROR_FILE "${member}.err")
        if(NOT status EQUAL 0)
            message(FATAL_ERROR "opt failed on ${member} of ${archive}, "
                                "see ${member}.err")
        endif()
        file(STRINGS "${member}.ll" outline REGEX "\\+outline-atomics")
        if(outline)
            get_filename_component(name "${member}" NAME)
            message(FATAL_ERROR "${name} of ${archive} names +outline-atomics")
        endif()
    endforeach()
endforeach()

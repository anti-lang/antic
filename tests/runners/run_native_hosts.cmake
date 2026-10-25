# The native libraries of linux-arm64 call no helper of the outline
# atomics, `__aarch64_*`, whichever host built them. They take
# -mno-outline-atomics from antic_linux_code_options of CMakeLists.txt, as
# the runtime of Linux does, by Q6 of "Answers of 2026-10-24" in
# docs/decisions.md. The driver of clang adds the outline atomics where it
# finds a GCC of the host, and this test keeps them out. The libraries of
# clang that the archive copies stand outside it. Run with cmake -P and
# these values:
#   RUNTIME   the runtime archive
#   OBJDUMP   the llvm-objdump executable
#   WORK      a file for the listing

cmake_minimum_required(VERSION 3.21)

file(GLOB libraries "${RUNTIME}/lib/linux-arm64/*.a"
     "${RUNTIME}/lib/linux-arm64/*/libmimalloc.a")
list(FILTER libraries EXCLUDE REGEX "/libclang_rt\\.[^/]*$")
if(libraries STREQUAL "")
    message("SKIP: the runtime archive holds no native library of "
            "linux-arm64")
    return()
endif()
foreach(library IN LISTS libraries)
    execute_process(COMMAND "${OBJDUMP}" -dr "${library}"
        RESULT_VARIABLE status OUTPUT_FILE "${WORK}"
        ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "llvm-objdump failed on ${library}\n${err}")
    endif()
    file(STRINGS "${WORK}" calls REGEX "__aarch64_")
    if(calls)
        list(GET calls 0 first)
        message(FATAL_ERROR "${library} calls the outline atomics: ${first}")
    endif()
endforeach()
file(REMOVE "${WORK}")

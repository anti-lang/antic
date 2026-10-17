# Link a hello-world program for windows-x86_64 and check that its object
# holds no 32-bit absolute address. lld-link puts a 64-bit image at
# 0x140000000, so an IMAGE_REL_AMD64_ADDR32 relocation keeps the low 32
# bits of the address alone. The program then hands the C library a
# pointer outside the image, and fwrite ends it with 0xC0000005. The
# object comes from the link of --lto none. A default release build links
# through LTO, whose object is bitcode, and lld-link writes its code. On a
# Windows host both programs also run and must print their line: natively
# on x86_64, under the x64 emulation on arm64. Run with cmake -P and these
# values:
#   ANTIC         the antic executable
#   LLVM_MC       the llvm-mc executable
#   LLVM_OBJDUMP  the llvm-objdump executable
#   RUNTIME       the runtime directory
#   SOURCE        the .anti file, which prints `hello`
#   WORK          a directory for the executable
#   ROOT          the repository

cmake_minimum_required(VERSION 3.21)

include("${ROOT}/tests/program_output.cmake")

set(target windows-x86_64)
if(NOT EXISTS "${RUNTIME}/sysroot/${target}")
    message("SKIP: the runtime archive has no sysroot for ${target}")
    return()
endif()
file(MAKE_DIRECTORY "${WORK}")
get_filename_component(program "${SOURCE}" NAME_WE)
set(exe "${WORK}/${program}.exe")
set(object "${exe}.obj")
set(lto_exe "${WORK}/${program}-lto.exe")
file(REMOVE "${exe}" "${object}" "${lto_exe}")
foreach(build "${exe}|--lto|none" "${lto_exe}")
    string(REPLACE "|" ";" build "${build}")
    list(POP_FRONT build output)
    execute_process(
        COMMAND "${ANTIC}" --target ${target} --llvm-mc "${LLVM_MC}"
                --runtime "${RUNTIME}" ${build} -o "${output}" "${SOURCE}"
        RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "antic ${build} failed for ${target}\n${err}")
    endif()
endforeach()

execute_process(COMMAND "${LLVM_OBJDUMP}" -r "${object}"
    RESULT_VARIABLE status OUTPUT_VARIABLE listing ERROR_VARIABLE err
    ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "llvm-objdump failed on ${object}\n${err}")
endif()
if(NOT listing MATCHES "IMAGE_REL_AMD64_REL32")
    message(FATAL_ERROR "the object for ${target} holds no relative "
                        "relocation, so the listing proves nothing\n${listing}")
endif()
string(REGEX MATCHALL "[^\n]*IMAGE_REL_AMD64_ADDR32 [^\n]*" absolute
       "${listing}")
if(absolute)
    string(REPLACE ";" "\n" absolute "${absolute}")
    message(FATAL_ERROR "the object for ${target} holds 32-bit absolute "
                        "addresses, which a 64-bit image above 4 GiB cuts "
                        "short:\n${absolute}")
endif()

if(NOT CMAKE_HOST_WIN32)
    message(STATUS "the object holds no 32-bit absolute address. The "
                   "program runs on a Windows host alone.")
    return()
endif()
foreach(run "${exe}" "${lto_exe}")
    program_output(got status "${run}.out" "${run}")
    # "hello\n" in hexadecimal.
    if(NOT status EQUAL 0 OR NOT got STREQUAL "68656c6c6f0a")
        message(FATAL_ERROR "${run} ended with ${status} and printed the "
                            "bytes ${got}")
    endif()
endforeach()

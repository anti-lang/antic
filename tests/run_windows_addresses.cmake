# Link a hello-world program for windows-x86_64 and check that its object
# holds no 32-bit absolute address. lld-link puts a 64-bit image at
# 0x140000000, so an IMAGE_REL_AMD64_ADDR32 relocation keeps the low 32
# bits of the address alone. The program then hands the C library a
# pointer outside the image, and fwrite ends it with 0xC0000005. On a
# Windows host the program also runs and must print its line: natively on
# x86_64, under the x64 emulation on arm64. Run with cmake -P and these
# values:
#   ANTIC         the antic executable
#   LLVM_MC       the llvm-mc executable
#   LLVM_OBJDUMP  the llvm-objdump executable
#   RUNTIME       the runtime directory
#   SOURCE        the .anti file, which prints `hello`
#   WORK          a directory for the executable
#   ROOT          the repository

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
file(REMOVE "${exe}" "${object}")
execute_process(
    COMMAND "${ANTIC}" --target ${target} --llvm-mc "${LLVM_MC}"
            --runtime "${RUNTIME}" -o "${exe}" "${SOURCE}"
    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic failed for ${target}\n${err}")
endif()

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
program_output(got status "${WORK}/${program}.out" "${exe}")
# "hello\n" in hexadecimal.
if(NOT status EQUAL 0 OR NOT got STREQUAL "68656c6c6f0a")
    message(FATAL_ERROR "${exe} ended with ${status} and printed the bytes "
                        "${got}")
endif()

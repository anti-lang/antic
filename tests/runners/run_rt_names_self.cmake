# tests/runners/run_rt_names.cmake refuses a weak definition outside the
# prefixes of the runtime on ELF and on Mach-O, as it refuses a global
# one, and accepts a library that defines anti_rt_ names alone. Run with
# cmake -P and these values:
#   RUNNER    tests/runners/run_rt_names.cmake
#   WORK      a directory for the archives
#   LLVM_MC   the pinned llvm-mc, which assembles each object
#   LLVM_AR   the pinned llvm-ar
#   OBJDUMP   the llvm-objdump the check reads the symbols with
#
# Each case writes a runtime archive with one library, runs the check on
# it and states whether it must accept the archive. A case that must be
# refused names a text of the message, so a refusal for another reason
# fails the case.

cmake_minimum_required(VERSION 3.21)

file(REMOVE_RECURSE "${WORK}")

set(failures "")
# names_case(<name> accept|<text of the refusal> <directory> <triple> <asm>)
# The library stands at lib/<directory>/libanti_rt.a of the archive.
function(names_case name verdict directory triple assembly)
    set(archive "${WORK}/${name}")
    set(library "${archive}/lib/${directory}/libanti_rt.a")
    file(WRITE "${archive}/a.s" "${assembly}")
    execute_process(COMMAND "${LLVM_MC}" -triple "${triple}" -filetype=obj
                            "${archive}/a.s" -o "${archive}/a.o"
                    RESULT_VARIABLE status ERROR_VARIABLE err)
    if(NOT status EQUAL 0)
        set(failures "${failures}${name}: llvm-mc failed\n${err}\n"
            PARENT_SCOPE)
        return()
    endif()
    get_filename_component(where "${library}" DIRECTORY)
    file(MAKE_DIRECTORY "${where}")
    execute_process(COMMAND "${LLVM_AR}" rcs "${library}" "${archive}/a.o"
                    RESULT_VARIABLE status ERROR_VARIABLE err)
    if(NOT status EQUAL 0)
        set(failures "${failures}${name}: llvm-ar failed\n${err}\n"
            PARENT_SCOPE)
        return()
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DOBJDUMP=${OBJDUMP}"
                            "-DRUNTIME=${archive}" -P "${RUNNER}"
                    RESULT_VARIABLE status OUTPUT_VARIABLE out
                    ERROR_VARIABLE err ENCODING NONE)
    if(verdict STREQUAL "accept")
        if(NOT status EQUAL 0 OR "${out}${err}" MATCHES "SKIP: ")
            set(failures "${failures}${name}: not accepted\n${out}${err}\n"
                PARENT_SCOPE)
        endif()
        return()
    endif()
    if(status EQUAL 0)
        set(failures "${failures}${name}: accepted\n" PARENT_SCOPE)
        return()
    endif()
    string(FIND "${out}${err}" "${verdict}" at)
    if(at EQUAL -1)
        set(failures
            "${failures}${name}: refused without `${verdict}`\n${out}${err}\n"
            PARENT_SCOPE)
    endif()
endfunction()

set(elf_ok ".text\n.globl anti_rt_ok\nanti_rt_ok:\n ret\n")
set(macho_ok ".text\n.globl _anti_rt_ok\n_anti_rt_ok:\n ret\n")

names_case(elf_prefixed accept linux-arm64/armv8.0 aarch64-linux-musl
           "${elf_ok}")
names_case(elf_global "linux-arm64/armv8.0/libanti_rt.a: bad_global"
           linux-arm64/armv8.0 aarch64-linux-musl
           "${elf_ok}.globl bad_global\nbad_global:\n ret\n")
names_case(elf_weak "linux-arm64/armv8.0/libanti_rt.a: bad_weak"
           linux-arm64/armv8.0 aarch64-linux-musl
           "${elf_ok}.weak bad_weak\nbad_weak:\n ret\n")
names_case(macho_prefixed accept macos-arm64/armv8.5 arm64-apple-macos11
           "${macho_ok}")
names_case(macho_weak "macos-arm64/armv8.5/libanti_rt.a: bad_weak"
           macos-arm64/armv8.5 arm64-apple-macos11
           "${macho_ok}.globl _bad_weak\n.weak_definition _bad_weak\n_bad_weak:\n ret\n")

if(NOT failures STREQUAL "")
    message(FATAL_ERROR "run_rt_names.cmake:\n${failures}")
endif()

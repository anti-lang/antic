# Link a hello-world program for TARGET on this host and check that the
# link dropped a runtime function the program never calls. Eddie decided
# on 2026-10-06 that an executable carries no code it never uses. See the
# entry on dropped code under "Scope and toolchain" in docs/decisions.md.
#
# KEPT and DROPPED stand in one object file of the runtime, src/rt/io.c.
# The program calls KEPT, so the linker takes the object, and DROPPED must
# still be gone. The test then shows the strip within an object, which a
# link that only chooses objects does not give. It links the runtime as
# objects with --lto none, since the LTO of a default release build
# inlines KEPT into the program and leaves no name to read.
#
# ELF and Mach-O executables keep their symbol table, which llvm-objdump
# lists. A Windows executable holds none, and its PDB names every external
# function the image holds, so the test reads the names there, as
# run_pdb_names.cmake does. Run with cmake -P and these values:
#   ANTIC         the antic executable
#   LLVM_MC       the llvm-mc executable
#   LLVM_OBJDUMP  the llvm-objdump executable
#   RUNTIME       the runtime directory
#   SOURCE        the .anti file
#   WORK          a directory for the executable
#   TARGET        the target name
#   KEPT          the runtime function the program calls
#   DROPPED       the runtime function of the same object it never calls

if(NOT EXISTS "${RUNTIME}/sysroot/${TARGET}")
    message("SKIP: the runtime archive has no sysroot for ${TARGET}")
    return()
endif()
file(GLOB runtime_library "${RUNTIME}/lib/${TARGET}/*/libanti_rt.a"
     "${RUNTIME}/lib/${TARGET}/*/anti_rt.lib")
if(runtime_library STREQUAL "")
    message("SKIP: the runtime archive has no runtime library for ${TARGET}")
    return()
endif()
file(MAKE_DIRECTORY "${WORK}")
get_filename_component(program "${SOURCE}" NAME_WE)
set(windows FALSE)
if("${TARGET}" MATCHES "^windows-")
    set(windows TRUE)
    set(exe "${WORK}/${program}-${TARGET}.exe")
    set(pdb "${WORK}/${program}-${TARGET}.pdb")
else()
    set(exe "${WORK}/${program}-${TARGET}")
endif()
file(REMOVE "${exe}")
execute_process(
    COMMAND "${ANTIC}" --target "${TARGET}" --llvm-mc "${LLVM_MC}"
            --runtime "${RUNTIME}" --lto none -o "${exe}" "${SOURCE}"
    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic failed for ${TARGET}\n${err}")
endif()

# The names the image defines, one per line. Mach-O writes a C name with
# a leading underscore, and the pattern below takes both spellings.
if(windows)
    file(STRINGS "${pdb}" names REGEX "^(${KEPT}|${DROPPED})$")
else()
    execute_process(COMMAND "${LLVM_OBJDUMP}" --syms "${exe}"
        RESULT_VARIABLE status OUTPUT_VARIABLE listing ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "llvm-objdump failed on ${exe}\n${err}")
    endif()
    string(REGEX MATCHALL "[ _](${KEPT}|${DROPPED})\n" names "${listing}")
endif()
string(REGEX REPLACE "[ _\n]*(${KEPT}|${DROPPED})[\n]*" "\\1" names
       "${names}")
if(NOT KEPT IN_LIST names)
    message(FATAL_ERROR "the executable for ${TARGET} does not name ${KEPT}, "
                        "which the program calls, so the listing proves "
                        "nothing")
endif()
if(DROPPED IN_LIST names)
    message(FATAL_ERROR "the executable for ${TARGET} holds ${DROPPED}, a "
                        "runtime function the program never calls. The "
                        "link keeps unused code.")
endif()

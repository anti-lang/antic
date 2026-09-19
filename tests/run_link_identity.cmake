# Link tests/programs/return42.anti for macos-arm64 and compare the SHA-256
# digest of the executable with the one that EXPECTED holds, which a run
# on the Mac wrote. A run on Linux or Windows then checks that one object
# links to the same bytes on every host: the stubs are Zig's everywhere,
# and the runtime library comes from one cross build. Run with cmake -P
# and these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime directory
#   SOURCE    the .anti file
#   EXPECTED  the file of the digest
#   WORK      a directory for the executable

file(GLOB runtime_library "${RUNTIME}/lib/macos-arm64/libanti_rt.a")
if(NOT EXISTS "${RUNTIME}/sysroot/macos-arm64/usr/lib/libSystem.tbd" OR
   runtime_library STREQUAL "")
    message("SKIP: the runtime archive has no macOS stubs or runtime")
    return()
endif()
file(MAKE_DIRECTORY "${WORK}")
# The name of the output stands in the ad-hoc signature, so it is fixed.
set(exe "${WORK}/identity")
file(REMOVE "${exe}")
execute_process(
    COMMAND "${ANTIC}" --target macos-arm64 --llvm-mc "${LLVM_MC}"
            --runtime "${RUNTIME}" -o "${exe}" "${SOURCE}"
    RESULT_VARIABLE status ERROR_VARIABLE err)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic failed for macos-arm64\n${err}")
endif()
file(SHA256 "${exe}" actual)
file(STRINGS "${EXPECTED}" expected LIMIT_COUNT 1)
if(NOT actual STREQUAL expected)
    message(FATAL_ERROR "the executable of ${SOURCE} for macos-arm64 has the "
                        "SHA-256 ${actual}, and ${EXPECTED} holds '${expected}'. "
                        "A change to antic, the runtime or a pin that changes "
                        "the executable writes the new digest there from a "
                        "run on the Mac.")
endif()

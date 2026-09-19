# tools/get-sysroot.cmake takes a DEST relative to the directory it runs
# in, as the build steps of the README pass it. The digest of a Windows
# sysroot then covers the files of the tree, as it does for an absolute
# DEST. SPLAT=done checks a tree without running xwin.
#
#   cmake -DROOT=<repository> -DWORK=<dir> -P tests/run_sysroot_digest.cmake

# A Windows host takes its Windows sysroots from the Build Tools and hashes
# no tree of xwin.
if(CMAKE_HOST_WIN32)
    message("SKIP: a Windows host takes the Build Tools rather than xwin")
    return()
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/sysroot/windows-x86_64/crt")
file(WRITE "${WORK}/sysroot/windows-x86_64/crt/one.lib" "one\n")
file(SHA256 "${WORK}/sysroot/windows-x86_64/crt/one.lib" one)
string(SHA256 expected "${one}  ./crt/one.lib\n")

execute_process(COMMAND "${CMAKE_COMMAND}" -DDEST=sysroot -DLLVM_BIN=bin
                        -DTARGETS=windows-x86_64 -DSPLAT=done
                        -DACCEPT_LICENSE=yes -P "${ROOT}/tools/get-sysroot.cmake"
                WORKING_DIRECTORY "${WORK}"
                RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err
                ENCODING NONE)
# The tree is not the pinned one, so the script refuses it and names the
# digest it computed.
if(status EQUAL 0)
    message(FATAL_ERROR "tools/get-sysroot.cmake accepted a tree that is not "
                        "the pinned one:\n${out}${err}")
endif()
if(NOT "${out}${err}" MATCHES "${expected}")
    message(FATAL_ERROR "tools/get-sysroot.cmake did not hash the tree of a "
                        "relative DEST, whose digest is ${expected}:\n${out}${err}")
endif()

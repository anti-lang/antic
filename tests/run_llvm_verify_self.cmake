# tests/run_llvm_verify.cmake fails for an antic that a signal ends, and
# for a file that the front end refuses and NO_PROGRAM does not name. Run
# with cmake -P and these values:
#   RUNNER   tests/run_llvm_verify.cmake
#   ANTIC    the antic executable
#   OPT      opt of the pinned release
#   RUNTIME  the runtime directory
#   TARGET   a target
#   WORK     a directory for the files

cmake_minimum_required(VERSION 3.21)

file(REMOVE_RECURSE "${WORK}")
file(WRITE "${WORK}/dump/refused.anti" "fn main( {\n")

set(failures "")
# verify_case(<name> <antic> <text of the failure>)
function(verify_case name antic expected)
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DANTIC=${antic}"
                            "-DOPT=${OPT}" "-DRUNTIME=${RUNTIME}"
                            "-DSOURCES=${WORK}/dump" "-DTARGETS=${TARGET}"
                            "-DWORK=${WORK}/run-${name}" -P "${RUNNER}"
                    RESULT_VARIABLE status OUTPUT_VARIABLE out
                    ERROR_VARIABLE err ENCODING NONE)
    set(said "${out}${err}")
    string(FIND "${said}" "${expected}" at)
    if(status EQUAL 0 OR at EQUAL -1)
        set(failures
            "${failures}${name}: no failure with `${expected}`\n${said}\n"
            PARENT_SCOPE)
    endif()
endfunction()

verify_case(unnamed "${ANTIC}"
            "dump/refused: the front end refuses it, and NO_PROGRAM")
if(NOT CMAKE_HOST_WIN32)
    file(WRITE "${WORK}/antic" "#!/bin/sh\nkill -KILL $$\n")
    file(CHMOD "${WORK}/antic" PERMISSIONS OWNER_READ OWNER_WRITE
                                           OWNER_EXECUTE)
    verify_case(signal "${WORK}/antic"
                "refused release ${TARGET}: antic failed with")
endif()

if(NOT failures STREQUAL "")
    message(FATAL_ERROR "run_llvm_verify.cmake:\n${failures}")
endif()
file(REMOVE_RECURSE "${WORK}")

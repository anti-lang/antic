# tests/run_release_dry_run.cmake fails, and prints the output of the
# command, when a fixture it builds does not compile or link. A warning
# of the pinned clang under -Werror skipped the whole test of ./r before.
# Run with cmake -P and these values:
#   RUNNER    tests/run_release_dry_run.cmake
#   ROOT      the repository
#   CC        the C compiler of the tree
#   HOST_LINK the options of a link of a program of this host
#   SDK       the Apple SDK of the build
#   LLVM_BIN  the directory of lld-link
#   WORK      a directory for the files
#
# Each case puts a stand-in in front of one command, which refuses the
# fixture of one target with the text `refused by the case` and passes
# every other call on to the real command.

cmake_minimum_required(VERSION 3.21)

if(NOT APPLE)
    message("SKIP: a release is made on the development Mac")
    return()
endif()
if(NOT EXISTS "${ROOT}/.git")
    message("SKIP: an exported tree has no history to clone")
    return()
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

# Writes <path>, a shell script that fails for an argument that matches
# the shell pattern <refuse> and runs <real> with its arguments otherwise.
function(stand_in path refuse real)
    file(WRITE "${path}"
         "#!/bin/sh\n"
         "for a in \"$@\"; do\n"
         "    case \"$a\" in\n"
         "        ${refuse}) echo 'refused by the case' >&2; exit 1 ;;\n"
         "    esac\n"
         "done\n"
         "exec '${real}' \"$@\"\n")
    file(CHMOD "${path}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
endfunction()

set(failures "")
# fixture_case(<name> <cc> <llvm bin>)
function(fixture_case name cc bin)
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DROOT=${ROOT}" "-DCC=${cc}"
                            "-DHOST_LINK=${HOST_LINK}" "-DSDK=${SDK}"
                            "-DLLVM_BIN=${bin}"
                            "-DWORK=${WORK}/run-${name}"
                            -P "${RUNNER}"
                    RESULT_VARIABLE status OUTPUT_VARIABLE out
                    ERROR_VARIABLE err ENCODING NONE)
    # CMake wraps a long message, so the text is read with its spaces and
    # its line ends made one space.
    set(said "${out}${err}")
    string(REGEX REPLACE "[ \t\r\n]+" " " flat "${out}${err}")
    if(status EQUAL 0 OR flat MATCHES "SKIP: " OR
       NOT flat MATCHES "refused by the case")
        set(failures
            "${failures}${name}: did not fail with the output\n${said}\n"
            PARENT_SCOPE)
    endif()
endfunction()

stand_in("${WORK}/cc-x86_64" "x86_64" "${CC}")
fixture_case(x86_64 "${WORK}/cc-x86_64" "${LLVM_BIN}")
stand_in("${WORK}/cc-windows" "--target=x86_64-pc-windows-msvc" "${CC}")
fixture_case(windows "${WORK}/cc-windows" "${LLVM_BIN}")
file(MAKE_DIRECTORY "${WORK}/bin")
stand_in("${WORK}/bin/lld-link" "*" "${LLVM_BIN}/lld-link")
fixture_case(lld_link "${CC}" "${WORK}/bin")

if(NOT failures STREQUAL "")
    message(FATAL_ERROR "run_release_dry_run.cmake:\n${failures}")
endif()
file(REMOVE_RECURSE "${WORK}")

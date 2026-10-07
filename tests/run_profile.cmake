# A program built with a profile of its own run passes its test, the step
# pgo of docs/work-order-llvm-optimization.md. Run with cmake -P and these
# values:
#   ANTIC     the antic executable
#   RUNTIME   the runtime archive, which holds bin/llvm-profdata and bin/opt
#   SOURCE    tests/bench/ablate/mixed_work.anti, whose .expected is beside it
#   WORK      a directory for the files
#   HOST      the host target
#
# antic refuses each option where it cannot work: in dev mode, beside
# --lto, and both options together. --profile-generate refuses every
# build that writes no program, and --profile-use a file that is missing
# or a raw profile. The instrumented program prints the expected output
# and writes its raw profile, to the file LLVM_PROFILE_FILE names and by
# default to default_<signature>.profraw in its working directory.
# llvm-profdata of the runtime archive merges it, and the build of
# --profile-use prints nothing, so opt found no function whose code
# differs from its profile. Its bitcode carries the counts of the run,
# and the program prints the expected output again.

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

if(CMAKE_HOST_WIN32)
    set(exe ".exe")
else()
    set(exe "")
endif()
get_filename_component(name "${SOURCE}" NAME_WE)
get_filename_component(dir "${SOURCE}" DIRECTORY)
set(expected "${dir}/${name}.expected")
# The LLVM tools of the runtime archive.
set(LLVM_BIN "${RUNTIME}/bin")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

# antic with the options, which must fail with status 2 and a message
# that matches the pattern.
function(refused pattern)
    execute_process(COMMAND "${ANTIC}" --runtime "${RUNTIME}" ${ARGN}
                            -o "${WORK}/refused" "${SOURCE}"
                    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 2 OR NOT err MATCHES "${pattern}")
        message(FATAL_ERROR "antic ${ARGN} gave ${status}, not 2 with "
                            "'${pattern}':\n${err}")
    endif()
endfunction()

# antic with the options, which must succeed and print nothing.
function(built output)
    execute_process(COMMAND "${ANTIC}" --runtime "${RUNTIME}" ${ARGN}
                            -o "${output}" "${SOURCE}"
                    RESULT_VARIABLE status OUTPUT_VARIABLE out
                    ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0 OR NOT out STREQUAL "" OR NOT err STREQUAL "")
        message(FATAL_ERROR "antic ${ARGN} gave ${status}\n${out}${err}")
    endif()
endfunction()

file(WRITE "${WORK}/empty.profdata" "")
set(release "release mode, without --dev or --lto")
refused("--profile-generate builds in ${release}" --profile-generate --dev)
refused("--profile-use builds in ${release}"
        --profile-use "${WORK}/empty.profdata" --dev)
refused("--profile-generate builds in ${release}"
        --profile-generate --lto full)
refused("--profile-use builds in ${release}"
        --profile-use "${WORK}/empty.profdata" --lto thin)
refused("--profile-generate links a program" --profile-generate -S)
refused("--profile-generate links a program" --profile-generate -c)
refused("--profile-generate links a program" --profile-generate --lib static)
refused("--profile-generate and --profile-use exclude each other"
        --profile-generate --profile-use "${WORK}/empty.profdata")
refused("cannot read .*missing.profdata"
        --profile-use "${WORK}/missing.profdata")
refused("empty.profdata is no profile of llvm-profdata merge"
        --profile-use "${WORK}/empty.profdata")

# The instrumented program and the raw profile of its run.
set(generated "${WORK}/generated${exe}")
built("${generated}" --profile-generate)
set(ENV{LLVM_PROFILE_FILE} "${WORK}/run.profraw")
program_expect("${name} instrumented" COMMAND "${generated}"
               EXPECTED "${expected}")
unset(ENV{LLVM_PROFILE_FILE})
if(NOT EXISTS "${WORK}/run.profraw")
    message(FATAL_ERROR "the run of ${generated} wrote no ${WORK}/run.profraw")
endif()
file(MAKE_DIRECTORY "${WORK}/default")
execute_process(COMMAND "${generated}" WORKING_DIRECTORY "${WORK}/default"
                RESULT_VARIABLE status OUTPUT_QUIET)
file(GLOB default_profiles "${WORK}/default/default_*.profraw")
if(NOT status EQUAL 0 OR NOT default_profiles)
    message(FATAL_ERROR "a run without LLVM_PROFILE_FILE gave ${status} and "
                        "wrote no default_<signature>.profraw")
endif()

# A raw profile is refused, and the message names the merge.
refused("run.profraw is a raw profile. llvm-profdata merge -o <file> "
        --profile-use "${WORK}/run.profraw")

set(profile "${WORK}/run.profdata")
execute_process(COMMAND "${LLVM_BIN}/llvm-profdata${exe}" merge
                        -o "${profile}" "${WORK}/run.profraw"
                RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "llvm-profdata merge gave ${status}\n${err}")
endif()

# The program built with the profile, its counts in the bitcode, and its
# output. --keep-llvm writes the bitcode beside the output under the name
# of the output, so Windows names it used.exe.bc.
set(used "${WORK}/used${exe}")
built("${used}" --profile-use "${profile}" --keep-llvm)
execute_process(COMMAND "${LLVM_BIN}/opt${exe}" -S -passes=verify
                        -o "${WORK}/used.opt.ll" "${used}.bc"
                RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "opt cannot read ${used}.bc\n${err}")
endif()
file(READ "${WORK}/used.opt.ll" optimized)
if(NOT optimized MATCHES
   "define [^\n]*@${name}\\.main\\(\\)[^\n]*!prof !([0-9]+)")
    message(FATAL_ERROR "main carries no count of the profile in "
                        "${WORK}/used.opt.ll")
endif()
if(NOT optimized MATCHES
   "\n!${CMAKE_MATCH_1} = !{!\"function_entry_count\", i64 1}")
    message(FATAL_ERROR "the profile does not count one call of main in "
                        "${WORK}/used.opt.ll")
endif()
program_expect("${name} with its profile" COMMAND "${used}"
               EXPECTED "${expected}")

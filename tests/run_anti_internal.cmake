# Drive every command of anti that compiles over the project of
# tests/anti-build/internal, whose modules share an internal item. The
# package name of the manifest decides which modules share one, so every
# compile of every command carries it. Run with cmake -P and these values:
#   ANTI     the anti executable
#   RUNTIME  the runtime archive
#   LLVM_MC  the assembler
#   FIXTURE  the directory that holds internal/
#   HOST     the target name of this host
#   WORK     a directory this run writes into

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

set(project "${WORK}/internal")
set(exe "")
if(HOST MATCHES "^windows-")
    set(exe ".exe")
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(COPY "${FIXTURE}/internal" DESTINATION "${WORK}")

# Every command runs, and the run lists each one that failed. The cache of
# the repositories is the user's, so the run names its own.
set(failures "")
function(run_anti what)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
                "XDG_CACHE_HOME=${WORK}/cache"
                "LOCALAPPDATA=${WORK}/cache"
                "${ANTI}" ${ARGN}
        WORKING_DIRECTORY "${project}"
        RESULT_VARIABLE status
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT status EQUAL 0)
        set(failures "${failures}${what} failed with ${status}\n${out}${err}\n"
            PARENT_SCOPE)
    endif()
endfunction()

run_anti("anti check" check --runtime "${RUNTIME}")
run_anti("the dev build" build --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}")
run_anti("the release build" build --release --runtime "${RUNTIME}"
         --llvm-mc "${LLVM_MC}")
run_anti("anti test" test --work "${WORK}/tests" -I src
         --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}"
         src/com/example/core.anti src/com/example/near.anti)
run_anti("anti doc" doc -o "${WORK}/doc" --work "${WORK}/doc-work"
         --runtime "${RUNTIME}")
if(NOT EXISTS "${WORK}/doc/com.example.core.html")
    set(failures "${failures}anti doc wrote no page of the core module\n")
endif()
if(NOT failures STREQUAL "")
    message(FATAL_ERROR "${failures}")
endif()
# The program of both modes, named for the last segment of the package
# name, returns the internal item.
program_expect("dev" COMMAND "${project}/dist/${HOST}/dev/example${exe}"
               STATUS 41)
program_expect("release"
               COMMAND "${project}/dist/${HOST}/release/example${exe}"
               STATUS 41)

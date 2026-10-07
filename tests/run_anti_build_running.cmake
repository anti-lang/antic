# Build a program with `anti build`, keep it running from dist/ and build
# a changed program while it runs. The second build replaces the program
# in dist/, the running program ends as it began, and the new program runs
# from dist/ afterwards. Linux refuses a write into a program that runs
# with ETXTBSY, and a copy that fails partway leaves no program at all, so
# the build writes the program beside its place and renames it there. Run
# with cmake -P and these values:
#   ANTI     the anti executable
#   RUNTIME  the runtime archive
#   LLVM_MC  the assembler
#   FIXTURE  the directory that holds running/
#   HOST     the target name of this host
#   WORK     a directory this run writes into
# The script runs itself once more with REBUILD set, as the second command
# of a pipeline, so that it builds while the program runs.

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

set(project "${WORK}/running")
set(source "${project}/src/com/example/running.anti")
set(exe "")
if(HOST MATCHES "^windows-")
    set(exe ".exe")
endif()
set(program "${project}/dist/${HOST}/dev/running${exe}")
set(state "${WORK}/state")

# The cache of the repositories is the user's, so the run names its own.
function(build what)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
                "XDG_CACHE_HOME=${WORK}/cache"
                "LOCALAPPDATA=${WORK}/cache"
                "${ANTI}" build --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}"
        WORKING_DIRECTORY "${project}"
        RESULT_VARIABLE status
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
        ENCODING NONE)
    if(NOT status EQUAL 0)
        set(failure "${what} failed with ${status}\n${out}${err}" PARENT_SCOPE)
    endif()
endfunction()

if(REBUILD)
    # Wait for the program to start, change its result and build again.
    # The file stop ends the program whether or not the build passed.
    foreach(turn RANGE 600)
        if(EXISTS "${state}/started")
            break()
        endif()
        execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 0.1)
    endforeach()
    set(failure "")
    if(NOT EXISTS "${state}/started")
        set(failure "the program did not start")
    else()
        file(READ "${source}" text)
        string(REPLACE "return 7;" "return 8;" text "${text}")
        file(WRITE "${source}" "${text}")
        build("the build while the program ran")
    endif()
    file(WRITE "${state}/failure" "${failure}")
    file(WRITE "${state}/stop" "")
    return()
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${state}")
file(COPY "${FIXTURE}/running" DESTINATION "${WORK}")
set(failure "")
build("the first build")
if(failure)
    message(FATAL_ERROR "${failure}")
endif()

execute_process(
    COMMAND "${program}"
    COMMAND "${CMAKE_COMMAND}" -DREBUILD=ON
            "-DANTI=${ANTI}" "-DRUNTIME=${RUNTIME}" "-DLLVM_MC=${LLVM_MC}"
            "-DHOST=${HOST}" "-DWORK=${WORK}"
            -P "${CMAKE_CURRENT_LIST_FILE}"
    WORKING_DIRECTORY "${state}"
    RESULTS_VARIABLE statuses
    OUTPUT_FILE "${WORK}/out.txt"
    ERROR_FILE "${WORK}/err.txt")
list(GET statuses 0 first)
list(GET statuses 1 rebuild)
file(READ "${state}/failure" failure)
if(failure)
    message(FATAL_ERROR "${failure}")
endif()
if(NOT rebuild EQUAL 0)
    message(FATAL_ERROR "the second build ended with ${rebuild}")
endif()
exit_status(first "${first}")
if(NOT first EQUAL 7)
    message(FATAL_ERROR "the program that ran during the build ended with "
                        "${first}")
endif()
if(EXISTS "${program}.new")
    message(FATAL_ERROR "the build left ${program}.new")
endif()

# The file stop stands, so the new program returns at once.
execute_process(COMMAND "${program}" WORKING_DIRECTORY "${state}"
                RESULT_VARIABLE code)
exit_status(code "${code}")
if(NOT code EQUAL 8)
    message(FATAL_ERROR "the program the build replaced ended with ${code}")
endif()

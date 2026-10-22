# The runner of the fuzzer. For each seed it writes the program of
# tools/fuzz/generate.cmake, builds it in dev mode, in release mode and
# with --memory-checks, runs the three and compares their outputs. A seed
# whose check fails is a find, saved with its seed, its kind, the program
# and the outputs under FINDS/<seed>/. docs/notes/fuzz.md lists the kinds.
#
#   cmake [-DFIRST=<seed>] [-DCOUNT=<n>] [-DSIZE=<functions>] [-DJOBS=<n>]
#         [-DWORK=<dir>] [-DFINDS=<dir>] [-DKEEP=ON]
#         [-DANTIC=<antic>] [-DRUNTIME=<runtime directory>]
#         -P tools/fuzz/run.cmake
#
# FIRST is the first seed, 1 by default, and COUNT the count of seeds, 20
# by default. JOBS seeds run at once, 8 by default. WORK holds a
# directory per seed, removed after a seed that shows nothing unless KEEP
# is set. ANTIC and RUNTIME default to the Debug build of the tree. The
# script fails when a seed shows a find, and names each, so a test can
# run it.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/generate.cmake")

fuzz_default_tools()
if(NOT WORK)
    set(WORK "${FUZZ_HOME}/work")
endif()
if(NOT FINDS)
    set(FINDS "${FUZZ_DEFAULT_FINDS}")
endif()
if(NOT SIZE)
    set(SIZE ${FUZZ_DEFAULT_SIZE})
endif()

# Check the seed <seed> in <work>/<seed> and write its kind, empty for no
# find, into the file FUZZ_KIND_NAME there. A find is copied to
# FINDS/<seed>/ with the program and every output of the check.
function(fuzz_run_one seed)
    set(dir "${WORK}/${seed}")
    file(REMOVE_RECURSE "${dir}")
    file(MAKE_DIRECTORY "${dir}")
    fuzz_generate(program ${seed} ${SIZE})
    set(source "${dir}/${FUZZ_PROGRAM_NAME}")
    file(WRITE "${source}" "${program}")
    fuzz_check(kind detail "${source}" "${dir}" LANG_OBJECT "${LANG_OBJECT}")
    file(WRITE "${dir}/${FUZZ_KIND_NAME}" "${kind}")
    if(kind STREQUAL "")
        if(NOT KEEP)
            file(REMOVE_RECURSE "${dir}")
            file(MAKE_DIRECTORY "${dir}")
            file(WRITE "${dir}/${FUZZ_KIND_NAME}" "")
        endif()
        return()
    endif()
    set(find "${FINDS}/${seed}")
    file(REMOVE_RECURSE "${find}")
    file(MAKE_DIRECTORY "${find}")
    file(GLOB outputs "${dir}/*.antic" "${dir}/*.out" "${dir}/*.err")
    file(COPY "${source}" ${outputs} DESTINATION "${find}")
    file(WRITE "${find}/${FUZZ_SEED_NAME}" "${seed}\n")
    file(WRITE "${find}/${FUZZ_KIND_NAME}" "${kind}\n")
    file(WRITE "${find}/${FUZZ_DETAIL_NAME}" "${detail}\n")
endfunction()

# A child of the runner checks one seed. The parent runs JOBS of them at
# once as the commands of one execute_process.
if(DEFINED ONE)
    fuzz_run_one(${ONE})
    return()
endif()

if(NOT FIRST)
    set(FIRST 1)
endif()
if(NOT COUNT)
    set(COUNT 20)
endif()
if(NOT JOBS)
    set(JOBS 8)
endif()
file(MAKE_DIRECTORY "${WORK}")
fuzz_lang_object(LANG_OBJECT "${WORK}")

# DESIGN: execute_process runs its commands at the same time, joined by
# pipes, so one call runs a batch of children. A child writes nothing to
# its standard output, which is the input of the next, and reports its
# result in a file.
math(EXPR last "${FIRST} + ${COUNT} - 1")
set(seeds "")
foreach(seed RANGE ${FIRST} ${last})
    list(APPEND seeds ${seed})
endforeach()
set(found "")
while(seeds)
    set(batch "")
    set(commands "")
    foreach(turn RANGE 1 ${JOBS})
        if(NOT seeds)
            break()
        endif()
        list(POP_FRONT seeds seed)
        list(APPEND batch ${seed})
        file(REMOVE "${WORK}/${seed}/${FUZZ_KIND_NAME}")
        list(APPEND commands COMMAND "${CMAKE_COMMAND}" "-DONE=${seed}"
             "-DSIZE=${SIZE}" "-DWORK=${WORK}" "-DFINDS=${FINDS}"
             "-DKEEP=${KEEP}" "-DANTIC=${ANTIC}" "-DRUNTIME=${RUNTIME}"
             "-DLANG_OBJECT=${LANG_OBJECT}" -P "${CMAKE_CURRENT_LIST_FILE}")
    endforeach()
    execute_process(${commands} OUTPUT_QUIET ERROR_VARIABLE errors
                    RESULTS_VARIABLE statuses)
    foreach(seed IN LISTS batch)
        set(result "${WORK}/${seed}/${FUZZ_KIND_NAME}")
        if(NOT EXISTS "${result}")
            message(FATAL_ERROR "the check of seed ${seed} wrote no result:\n${errors}")
        endif()
        file(READ "${result}" kind)
        if(NOT kind STREQUAL "")
            file(READ "${FINDS}/${seed}/${FUZZ_DETAIL_NAME}" detail)
            list(APPEND found "seed ${seed}: ${kind}: ${detail}")
        endif()
    endforeach()
endwhile()

list(LENGTH found finds)
if(finds GREATER 0)
    list(JOIN found "\n" found)
    message(FATAL_ERROR "${finds} of ${COUNT} seeds from ${FIRST} show a find, "
                        "saved under ${FINDS}:\n${found}")
endif()
message("${COUNT} seeds from ${FIRST}: no find")

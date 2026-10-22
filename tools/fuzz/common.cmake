# The names and the check that the generator, the runner and the reducer of
# tools/fuzz share: where a find is saved, the tools a check uses, and the
# check of one program in dev mode, in release mode and with
# --memory-checks. docs/notes/fuzz.md describes the whole.

cmake_minimum_required(VERSION 3.21)

get_filename_component(FUZZ_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

# DESIGN: every path of the fuzzer is built from these names, so a script
# and the test of the suite cannot spell one two ways.
set(FUZZ_BUILD_NAME "build")
set(FUZZ_DIR_NAME "fuzz")
set(FUZZ_FINDS_NAME "finds")
set(FUZZ_PROGRAM_NAME "program.anti")
set(FUZZ_SEED_NAME "seed")
set(FUZZ_KIND_NAME "kind")
set(FUZZ_DETAIL_NAME "detail.txt")
set(FUZZ_REDUCED_NAME "reduced.anti")
set(FUZZ_LANG_OBJECT_NAME "anti_lang_dev")

# The directory of the fuzzer under the root, and the finds in it.
set(FUZZ_HOME "${FUZZ_ROOT}/${FUZZ_BUILD_NAME}/${FUZZ_DIR_NAME}")
set(FUZZ_DEFAULT_FINDS "${FUZZ_HOME}/${FUZZ_FINDS_NAME}")

# The three modes of a check, each with the options of antic it passes.
set(FUZZ_MODES dev release memory_checks)
set(FUZZ_OPTIONS_dev --dev)
set(FUZZ_OPTIONS_release "")
set(FUZZ_OPTIONS_memory_checks --memory-checks)

# The seconds antic may take for one build, and a program for one run. A
# generated program runs in milliseconds, so a run that takes longer is a
# find of its own.
set(FUZZ_ANTIC_SECONDS 60)
set(FUZZ_RUN_SECONDS 20)

# Set ANTIC and RUNTIME to the Debug build of the tree when the caller
# names neither, so a script runs from the root with no option.
macro(fuzz_default_tools)
    if(NOT ANTIC)
        set(ANTIC "${FUZZ_ROOT}/${FUZZ_BUILD_NAME}/host/antic")
    endif()
    if(NOT RUNTIME)
        set(RUNTIME "${FUZZ_ROOT}/${FUZZ_BUILD_NAME}/host/runtime")
    endif()
    foreach(fuzz_tool ANTIC RUNTIME)
        if(NOT EXISTS "${${fuzz_tool}}")
            message(FATAL_ERROR "${fuzz_tool} ${${fuzz_tool}} does not exist")
        endif()
    endforeach()
endmacro()

# Write the dev object of anti.lang into <dir> unless it stands there, and
# set <out> to its path. Every generated program imports anti.lang alone,
# and a dev build links the dev object of each module it imports.
function(fuzz_lang_object out dir)
    set(base "${dir}/${FUZZ_LANG_OBJECT_NAME}")
    if(NOT EXISTS "${base}.o")
        file(MAKE_DIRECTORY "${dir}")
        execute_process(COMMAND "${ANTIC}" --runtime "${RUNTIME}" --dev
                                -I "${RUNTIME}/std" -o "${base}"
                                "${RUNTIME}/std/anti/lang.antl"
                        RESULT_VARIABLE status
                        OUTPUT_FILE "${dir}/lang-object.out"
                        ERROR_FILE "${dir}/lang-object.out")
        if(NOT status EQUAL 0)
            message(FATAL_ERROR "the dev object of anti.lang failed: see "
                                "${dir}/lang-object.out")
        endif()
    endif()
    set(${out} "${base}.o" PARENT_SCOPE)
endfunction()

# Set <out> to the kind of what antic printed into <log> for <source>
# when it exited with <status>: `refused` for a message of the checker at
# a position of the source, `crash` for a signal, an abort or a report of
# a sanitizer, and `backend` for any other failure, a tool of LLVM or the
# linker among them.
function(fuzz_antic_kind out status log source)
    file(READ "${log}" text)
    get_filename_component(name "${source}" NAME)
    if(NOT status MATCHES "^[0-9]+$" OR status GREATER 1
       OR text MATCHES "Sanitizer|Assertion|internal error")
        set(${out} crash PARENT_SCOPE)
    elseif(text MATCHES "${name}:[0-9]+:[0-9]+: error:")
        set(${out} refused PARENT_SCOPE)
    else()
        set(${out} backend PARENT_SCOPE)
    endif()
endfunction()

# Check <source> in each mode in <work>: build it with antic, run it, and
# compare the three outputs and exit statuses. Set <kind> to the empty
# text when the three builds succeed, the three programs exit with 0 and
# print the same bytes, and the run with --memory-checks writes nothing
# to its standard error. Otherwise set it to the kind of the find:
#   refused   the checker refused the program
#   crash     antic crashed
#   backend   antic failed after the checker, in LLVM or the link
#   timeout   antic or a program ran past its time
#   memory    the run with --memory-checks reported on its standard error
#   differs   two modes printed other bytes or exited otherwise
#   status    the three modes agree and exit with another status than 0
# and <detail> to a text of a few lines that names the mode and the
# output files. LANG_OBJECT names the dev object of anti.lang for the dev
# link. ONLY optionally names the modes to build, for a reducer whose find
# shows in them alone.
function(fuzz_check kind detail source work)
    cmake_parse_arguments(PARSE_ARGV 4 arg "" "LANG_OBJECT" "ONLY")
    file(MAKE_DIRECTORY "${work}")
    set(${kind} "" PARENT_SCOPE)
    set(${detail} "" PARENT_SCOPE)
    set(modes ${FUZZ_MODES})
    if(arg_ONLY)
        set(modes ${arg_ONLY})
    endif()
    foreach(mode IN LISTS modes)
        set(exe "${work}/${mode}")
        set(objects "")
        if(mode STREQUAL "dev")
            set(objects "${arg_LANG_OBJECT}")
        endif()
        file(REMOVE "${exe}")
        execute_process(COMMAND "${ANTIC}" --runtime "${RUNTIME}"
                                ${FUZZ_OPTIONS_${mode}} -o "${exe}"
                                "${source}" ${objects}
                        RESULT_VARIABLE status
                        TIMEOUT ${FUZZ_ANTIC_SECONDS}
                        OUTPUT_FILE "${work}/${mode}.antic"
                        ERROR_FILE "${work}/${mode}.antic")
        if(status MATCHES "timeout|Process terminated due to timeout")
            set(${kind} timeout PARENT_SCOPE)
            set(${detail} "antic ran past ${FUZZ_ANTIC_SECONDS} s in mode ${mode}"
                PARENT_SCOPE)
            return()
        endif()
        if(NOT status EQUAL 0)
            fuzz_antic_kind(found "${status}" "${work}/${mode}.antic" "${source}")
            file(STRINGS "${work}/${mode}.antic" lines LIMIT_COUNT 3)
            list(JOIN lines "\n" lines)
            set(${kind} ${found} PARENT_SCOPE)
            set(${detail} "antic exited with ${status} in mode ${mode}:\n${lines}"
                PARENT_SCOPE)
            return()
        endif()
        execute_process(COMMAND "${exe}"
                        RESULT_VARIABLE run_status
                        TIMEOUT ${FUZZ_RUN_SECONDS}
                        OUTPUT_FILE "${work}/${mode}.out"
                        ERROR_FILE "${work}/${mode}.err")
        if(run_status MATCHES "timeout|Process terminated due to timeout")
            set(${kind} timeout PARENT_SCOPE)
            set(${detail} "the program ran past ${FUZZ_RUN_SECONDS} s in mode ${mode}"
                PARENT_SCOPE)
            return()
        endif()
        set(status_${mode} "${run_status}")
    endforeach()
    list(GET modes 0 first)
    foreach(mode IN LISTS modes)
        if(NOT status_${mode} STREQUAL status_${first})
            set(${kind} differs PARENT_SCOPE)
            set(${detail} "mode ${first} exited with ${status_${first}} and mode ${mode} with ${status_${mode}}"
                PARENT_SCOPE)
            return()
        endif()
        execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files
                                "${work}/${first}.out" "${work}/${mode}.out"
                        RESULT_VARIABLE same OUTPUT_QUIET ERROR_QUIET)
        if(NOT same EQUAL 0)
            set(${kind} differs PARENT_SCOPE)
            set(${detail} "${first}.out and ${mode}.out differ" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    if("memory_checks" IN_LIST modes)
        file(SIZE "${work}/memory_checks.err" size)
        if(size GREATER 0)
            file(STRINGS "${work}/memory_checks.err" lines LIMIT_COUNT 3)
            list(JOIN lines "\n" lines)
            set(${kind} memory PARENT_SCOPE)
            set(${detail} "memory_checks.err holds a report:\n${lines}" PARENT_SCOPE)
            return()
        endif()
    endif()
    if(NOT status_${first} EQUAL 0)
        set(${kind} status PARENT_SCOPE)
        set(${detail} "every mode exited with ${status_${first}}" PARENT_SCOPE)
    endif()
endfunction()

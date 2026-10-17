# ./r --dry-run on the copy of tests/release_copy.cmake warns on a version
# that is a tag of the checkout or of origin, where ./r refuses it in
# step 1, and a rerun goes on over the tag of the release it resumes.
#
#   cmake -DROOT=<repository> -DWORK=<dir> -DCC=<compiler>
#         -DHOST_LINK=<options> -DSDK=<Apple SDK> -DLLVM_BIN=<dir>
#         -P tests/runners/run_release_tags.cmake
#
# tests/release_copy.cmake describes the values.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../release_copy.cmake")
if(release_skipped)
    return()
endif()

# DESIGN: a version that is a tag is published, and a real run refuses
# it in step 1. A dry run warns, runs every other check and prints the
# plan, so it checks the script at any time and not only before a
# release. Both hold for a tag of the checkout and for a tag of origin
# alone.
function(tagged_runs where)
    set(ENV{PATH} "${WORK}/bin:${saved_path}")
    execute_process(COMMAND "${copy}/r" --dry-run WORKING_DIRECTORY "${copy}"
                    RESULT_VARIABLE failed OUTPUT_VARIABLE out
                    ERROR_VARIABLE err ENCODING NONE)
    set(ENV{PATH} "${saved_path}")
    if(NOT failed EQUAL 0)
        message(FATAL_ERROR "./r --dry-run refused a version that is a tag "
                            "${where}\n${out}${err}")
    endif()
    if(NOT "${out}${err}" MATCHES "warning: [^\n]*v${version}")
        message(FATAL_ERROR "./r --dry-run gave no warning on the tag "
                            "${where}\n${out}${err}")
    endif()
    foreach(line "would sign" "would tag v${version}" "would install"
            "the dry run of ${version} is done")
        if(NOT out MATCHES "${line}")
            message(FATAL_ERROR "./r --dry-run on the tag ${where} printed no "
                                "`${line}`\n${out}${err}")
        endif()
    endforeach()
    set(ENV{PATH} "${WORK}/bin:${saved_path}")
    execute_process(COMMAND "${copy}/r" WORKING_DIRECTORY "${copy}"
                    RESULT_VARIABLE refused OUTPUT_VARIABLE out
                    ERROR_VARIABLE err ENCODING NONE)
    set(ENV{PATH} "${saved_path}")
    if(refused EQUAL 0 OR NOT "${out}${err}" MATCHES "v${version}"
       OR "${out}${err}" MATCHES "step 2")
        message(FATAL_ERROR "./r did not refuse a version that is a tag "
                            "${where} in step 1\n${out}${err}")
    endif()
endfunction()
tool_run("the tag failed" "${GIT}" -C "${copy}" tag "v${version}")
tagged_runs("of the checkout")
tool_run("the push of the tag failed" "${GIT}" -C "${copy}" push --quiet origin
         "v${version}")
tool_run("the tag did not go" "${GIT}" -C "${copy}" tag -d "v${version}")
tagged_runs("of origin")
tool_run("the tag did not leave origin" "${GIT}" -C "${copy}" push --quiet origin
         ":refs/tags/v${version}")

# A release that stopped after step 7 holds a tag of its own, and its
# rerun goes on rather than refusing it. The stamp of step 7 in the state
# of this commit is what tells that tag from a published version.
tool_run("the tag failed" "${GIT}" -C "${copy}" tag "v${version}")
execute_process(COMMAND "${GIT}" -C "${copy}" rev-parse HEAD
                OUTPUT_VARIABLE resumed_head
                OUTPUT_STRIP_TRAILING_WHITESPACE ENCODING NONE)
file(WRITE "${dist}/state/head" "${resumed_head}\n")
file(WRITE "${dist}/state/07-release" "2026-09-21T00:00:00Z\n")
set(ENV{PATH} "${WORK}/bin:${saved_path}")
execute_process(COMMAND "${copy}/r" --dry-run WORKING_DIRECTORY "${copy}"
                RESULT_VARIABLE resumed OUTPUT_VARIABLE out
                ERROR_VARIABLE err ENCODING NONE)
set(ENV{PATH} "${saved_path}")
if(NOT resumed EQUAL 0 OR NOT "${out}" MATCHES "step 7 of this run made")
    message(FATAL_ERROR "./r refuses the tag of the release it is resuming\n"
                        "${out}${err}")
endif()
file(REMOVE "${dist}/state/07-release")
tool_run("the tag did not go" "${GIT}" -C "${copy}" tag -d "v${version}")

file(REMOVE_RECURSE "${WORK}")

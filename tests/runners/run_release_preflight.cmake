# The preflight of ./r --dry-run on the copy of tests/release_copy.cmake
# refuses an uncommitted change and a version without an entry in the
# changelog.
#
#   cmake -DROOT=<repository> -DWORK=<dir> -DCC=<compiler>
#         -DHOST_LINK=<options> -DSDK=<Apple SDK> -DLLVM_BIN=<dir>
#         -P tests/runners/run_release_preflight.cmake
#
# tests/release_copy.cmake describes the values.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../release_copy.cmake")
if(release_skipped)
    return()
endif()

# An uncommitted change is refused, since a release names a commit that
# origin holds.
file(APPEND "${copy}/CHANGELOG.md" "\n")
set(ENV{PATH} "${WORK}/bin:${saved_path}")
execute_process(COMMAND "${copy}/r" --dry-run WORKING_DIRECTORY "${copy}"
                RESULT_VARIABLE refused OUTPUT_VARIABLE out
                ERROR_VARIABLE err ENCODING NONE)
set(ENV{PATH} "${saved_path}")
if(refused EQUAL 0 OR NOT "${out}${err}" MATCHES "uncommitted")
    message(FATAL_ERROR "./r packed a tree with an uncommitted change\n"
                        "${out}${err}")
endif()

# A version without an entry in the changelog is refused as well.
file(READ "${copy}/CHANGELOG.md" changelog)
string(REPLACE "## ${version} - 2026-09-20" "## 98.0.0 - 2026-09-20"
       changelog "${changelog}")
file(WRITE "${copy}/CHANGELOG.md" "${changelog}")
tool_run("the commit failed" "${GIT}" -C "${copy}" commit --quiet -a -m "No entry")
tool_run("the push failed" "${GIT}" -C "${copy}" push --quiet origin main)
set(ENV{PATH} "${WORK}/bin:${saved_path}")
execute_process(COMMAND "${copy}/r" --dry-run WORKING_DIRECTORY "${copy}"
                RESULT_VARIABLE refused OUTPUT_VARIABLE out
                ERROR_VARIABLE err ENCODING NONE)
set(ENV{PATH} "${saved_path}")
if(refused EQUAL 0 OR NOT "${out}${err}" MATCHES "CHANGELOG.md")
    message(FATAL_ERROR "./r packed a version the changelog does not name\n"
                        "${out}${err}")
endif()

file(REMOVE_RECURSE "${WORK}")

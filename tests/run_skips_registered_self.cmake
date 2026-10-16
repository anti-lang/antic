# tests/run_skips_registered.cmake refuses a test whose runner prints
# SKIP: without SKIP_REGULAR_EXPRESSION, and accepts the others. Run with
# cmake -P and these values:
#   RUNNER  tests/run_skips_registered.cmake
#   WORK    a directory for the files
#
# Each case writes a list in the form of `ctest --show-only=json-v1` and
# the runners it names, runs the check on it and states whether it must
# accept the list. A case that must be refused names a text of the
# message, so a refusal for another reason fails the case.

cmake_minimum_required(VERSION 3.21)

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(WRITE "${WORK}/run_skips.cmake"
     "if(NOT EXISTS \"\${RUNTIME}\")\n"
     "    message(\"SKIP: no runtime\")\n"
     "    return()\n"
     "endif()\n")
file(WRITE "${WORK}/run_plain.cmake" "message(\"a pass\")\n")
file(WRITE "${WORK}/run_told.cmake"
     "if(DEFINED SKIP_RUN)\n"
     "    message(\"SKIP: \${SKIP_RUN}\")\n"
     "    return()\n"
     "endif()\n")

# The text of one test of the list. SKIP is TRUE for a test that sets
# SKIP_REGULAR_EXPRESSION. A fifth argument is one more word of its
# command, -D<name>=<value>.
function(test_entry out name runner skip)
    set(define "")
    if(ARGC GREATER 4)
        set(define "        \"${ARGV4}\",\n")
    endif()
    set(properties "")
    if(skip)
        set(properties "        {\n          \"name\" : \"SKIP_REGULAR_EXPRESSION\",\n          \"value\" : \n          [\n            \"SKIP: \"\n          ]\n        },\n")
    endif()
    set(${out} "    {\n      \"backtrace\" : 1,\n      \"command\" : \n      [\n        \"/bin/cmake\",\n        \"-DWORK=/tmp\",\n${define}        \"-P\",\n        \"${runner}\"\n      ],\n      \"name\" : \"${name}\",\n      \"properties\" : \n      [\n${properties}        {\n          \"name\" : \"WORKING_DIRECTORY\",\n          \"value\" : \"/tmp\"\n        }\n      ]\n    }" PARENT_SCOPE)
endfunction()

set(failures "")
# skips_case(<name> accept|<text of the refusal> <entry>...)
function(skips_case name verdict)
    list(JOIN ARGN ",\n" entries)
    set(list "${WORK}/${name}.json")
    file(WRITE "${list}" "{\n  \"kind\" : \"ctestInfo\",\n  \"tests\" : \n  [\n${entries}\n  ],\n  \"version\" : \n  {\n    \"major\" : 1,\n    \"minor\" : 0\n  }\n}\n")
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DJSON=${list}"
                            -P "${RUNNER}"
                    RESULT_VARIABLE status OUTPUT_VARIABLE out
                    ERROR_VARIABLE err ENCODING NONE)
    if(verdict STREQUAL "accept")
        if(NOT status EQUAL 0)
            set(failures "${failures}${name}: refused\n${out}${err}\n"
                PARENT_SCOPE)
        endif()
        return()
    endif()
    if(status EQUAL 0)
        set(failures "${failures}${name}: accepted\n" PARENT_SCOPE)
        return()
    endif()
    string(FIND "${out}${err}" "${verdict}" at)
    if(at EQUAL -1)
        set(failures "${failures}${name}: refused without `${verdict}`\n"
                     "${out}${err}\n" PARENT_SCOPE)
    endif()
endfunction()

test_entry(skips_bare a_skip "${WORK}/run_skips.cmake" FALSE)
test_entry(skips_named a_skip "${WORK}/run_skips.cmake" TRUE)
test_entry(plain a_plain "${WORK}/run_plain.cmake" FALSE)
test_entry(told_bare a_told "${WORK}/run_told.cmake" FALSE
           "-DSKIP_RUN=no machine")
test_entry(told_named a_told "${WORK}/run_told.cmake" TRUE
           "-DSKIP_RUN=no machine")
test_entry(untold a_run "${WORK}/run_told.cmake" FALSE)

skips_case(bare "a_skip, run_skips.cmake" "${plain}" "${skips_bare}")
skips_case(named accept "${plain}" "${skips_named}")
skips_case(plain accept "${plain}")
skips_case(told "a_told, run_told.cmake" "${untold}" "${told_bare}")
skips_case(told_named accept "${untold}" "${told_named}")
skips_case(none "lists no test that runs a runner")

if(NOT failures STREQUAL "")
    message(FATAL_ERROR "run_skips_registered.cmake:\n${failures}")
endif()

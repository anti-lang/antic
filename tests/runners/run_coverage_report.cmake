# The report of the coverage measurement, on a program of three files
# whose functions two runs reach, one function of src/antic/half.c each.
#
#   cmake -DCC=<clang> -DSDK=<Apple SDK> -DHOST_LINK=<options>
#         -DCOMPILE_FLAGS=<flags> -DLINK_FLAGS=<flags> -DLLVM_BIN=<dir>
#         -DSCRIPT=<tools/coverage-report.cmake> -DWORK=<dir>
#         -P tests/runners/run_coverage_report.cmake
#
# CC is the pinned clang, SDK the pinned Apple SDK and HOST_LINK the
# options of a link of a program of this host. COMPILE_FLAGS and
# LINK_FLAGS are the flags of tools/coverage.cmake, which a coverage tree
# gives antic, anti and the unit tests. COMPILE_FLAGS are those of sources
# under <WORK>/root, where the program stands.
#
# The first run writes its profile through the pattern of a coverage
# tree, and the second names a file of its own, as a second part of a
# suite leaves one. The report must count both. src/antic/half.c holds a
# third function that no run reaches, src/rt/whole.c one that both reach,
# and tests/unit/main.c stands outside the three directories of the
# summary.

cmake_minimum_required(VERSION 3.21)

set(root "${WORK}/root")
set(profiles "${WORK}/profiles")
set(out "${WORK}/out")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${root}/src/antic" "${root}/src/anti" "${root}/src/rt"
     "${root}/tests/unit" "${profiles}" "${out}")
file(WRITE "${root}/src/antic/half.c"
     "int first(int x) { return x + 1; }\n"
     "int second(int x) { return x + 2; }\n"
     "int never(int x) { return x + 3; }\n")
file(WRITE "${root}/src/rt/whole.c"
     "int whole(int x) { return x; }\n")
file(WRITE "${root}/tests/unit/main.c"
     "int first(int x);\n"
     "int second(int x);\n"
     "int whole(int x);\n"
     "int main(int argc, char **argv) {\n"
     "    (void)argv;\n"
     "    return whole(argc > 1 ? second(-2) : first(-1));\n"
     "}\n")

function(run what)
    execute_process(COMMAND ${ARGN} WORKING_DIRECTORY "${WORK}"
                    RESULT_VARIABLE status OUTPUT_VARIABLE text
                    ERROR_VARIABLE text ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "${what} gave ${status}\n${text}")
    endif()
endfunction()

# The report of the profiles on disk, with its status and its output.
function(report status_out text_out)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DLLVM_BIN=${LLVM_BIN}" "-DROOT=${root}"
                "-DPROFILES=${profiles}" "-DOUT=${out}"
                "-DOBJECTS=${WORK}/probe"
                -P "${SCRIPT}"
        RESULT_VARIABLE status OUTPUT_VARIABLE text ERROR_VARIABLE text
        ENCODING NONE)
    set(${status_out} "${status}" PARENT_SCOPE)
    set(${text_out} "${text}" PARENT_SCOPE)
endfunction()

run("the compile of the program"
    "${CC}" -isysroot "${SDK}" ${HOST_LINK} ${COMPILE_FLAGS} ${LINK_FLAGS}
    -o "${WORK}/probe"
    "${root}/src/antic/half.c" "${root}/src/rt/whole.c"
    "${root}/tests/unit/main.c")

# No profile yet: the script names the directory and writes no report.
report(status text)
if(status EQUAL 0 OR NOT text MATCHES "holds no raw profile")
    message(FATAL_ERROR "the report without a profile gave ${status}\n${text}")
endif()
if(EXISTS "${out}/summary.txt")
    message(FATAL_ERROR "the report without a profile wrote ${out}/summary.txt")
endif()

run("the first run"
    "${CMAKE_COMMAND}" -E env "LLVM_PROFILE_FILE=${profiles}/%9m.profraw"
    "${WORK}/probe")
run("the second run"
    "${CMAKE_COMMAND}" -E env "LLVM_PROFILE_FILE=${profiles}/second.profraw"
    "${WORK}/probe" again)
file(GLOB raw "${profiles}/*.profraw")
list(LENGTH raw count)
if(NOT count EQUAL 2)
    message(FATAL_ERROR "the two runs left ${count} profiles: ${raw}")
endif()

report(status text)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "the report gave ${status}\n${text}")
endif()

# The columns of a row are the regions, the functions, the lines and the
# branches, each with its count, what was missed and a percentage. Two of
# the three functions and of the three lines of half.c were reached, which
# takes the profiles of both runs. llvm-cov names a file from the
# directory that all files of the report share, which is src/ here as in
# the tree.
file(READ "${out}/summary.txt" summary)
if(NOT summary MATCHES "Regions.*Functions.*Lines.*Branches")
    message(FATAL_ERROR "the summary lacks a column\n${summary}")
endif()
if(NOT summary MATCHES
   "\nantic/half\\.c +3 +1 +66\\.67% +3 +1 +66\\.67% +3 +1 +66\\.67% +0 +0")
    message(FATAL_ERROR "the summary counts half.c otherwise\n${summary}")
endif()
if(NOT summary MATCHES "\nrt/whole\\.c +1 +0 +100\\.00% +1 +0 +100\\.00%")
    message(FATAL_ERROR "the summary counts whole.c otherwise\n${summary}")
endif()
if(summary MATCHES "main\\.c")
    message(FATAL_ERROR "the summary holds a file of tests/\n${summary}")
endif()

# The annotated source of half.c, and none of main.c.
if(NOT EXISTS "${out}/html/index.html")
    message(FATAL_ERROR "the report wrote no ${out}/html/index.html")
endif()
file(GLOB_RECURSE pages "${out}/html/*.c.html")
if(NOT pages MATCHES "src/antic/half\\.c\\.html" OR
   NOT pages MATCHES "src/rt/whole\\.c\\.html" OR pages MATCHES "main\\.c")
    message(FATAL_ERROR "the annotated sources are ${pages}")
endif()

# A second report replaces the first: a page of a file that left the
# measurement does not stay.
file(WRITE "${out}/html/stale.c.html" "stale\n")
report(status text)
if(NOT status EQUAL 0 OR EXISTS "${out}/html/stale.c.html")
    message(FATAL_ERROR "the second report gave ${status} and kept a page "
                        "of the first\n${text}")
endif()

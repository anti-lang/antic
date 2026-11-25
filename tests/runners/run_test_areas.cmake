# Every test of the tree carries an area of tests/test-areas.txt as a
# label, by answer 2 of "Answers of 2026-11-25" in docs/decisions.md, and
# every line of the map names a path of the repository. The test refuses
# a test without a label, a label outside the areas of the map, a test
# whose command names a program or a directory of an area it lacks, a line
# of the map whose words are wrong and a line whose path names nothing.
# Run with cmake -P and these values:
#   CTEST      the ctest executable
#   TREE       the build tree
#   MAP        tests/test-areas.txt
#   ROOT       the root of the repository
#   COMPILERS  the builds of antic, with a `,` between two
#   TOOLS      the builds of anti, with a `,` between two
#   RUNTIME    the runtime archives, with a `,` between two
#   SCRIPTS    the scripts of ROOT that read the map, with a `,` between
#              two, which each spell its path from ROOT
# or with JSON, a file that holds the list of `ctest --show-only=json-v1`,
# in place of CTEST and TREE.
#
# DESIGN: a test that names antic runs it, and antic reads the standard
# library of the runtime archive and links its runtime, so the test
# carries compiler, runtime and std. One that names anti carries tool
# beside those three, since anti runs antic. One that names the runtime
# archive carries runtime and std, and one that names src/antic, src/anti,
# src/rt, src/native or src/std carries the area of that directory. So a
# change selects at least every test that can see it through those names.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../test_areas.cmake")

if(DEFINED JSON)
    set(list "${JSON}")
else()
    set(list "${TREE}/Testing/test-areas.json")
    execute_process(COMMAND "${CTEST}" --test-dir "${TREE}"
                            --show-only=json-v1
                    OUTPUT_FILE "${list}" RESULT_VARIABLE status
                    ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "ctest --show-only failed with ${status}\n${err}")
    endif()
endif()

set(wrong "")

# The map: the areas, and each line of a path.
test_areas_set(areas "${MAP}")
test_areas_lines("${MAP}" map_lines map_wrong)
foreach(number IN LISTS map_wrong)
    string(APPEND wrong "  line ${number} of the map is no path and words\n")
endforeach()
foreach(line IN LISTS map_lines)
    string(REGEX MATCH "^([^ ]*) (.*)$" parts "${line}")
    set(path "${CMAKE_MATCH_1}")
    string(REPLACE " " ";" words "${CMAKE_MATCH_2}")
    list(LENGTH words count)
    foreach(word IN LISTS words)
        if(word IN_LIST ANTIC_TEST_AREAS_WORDS)
            if(NOT count EQUAL 1)
                string(APPEND wrong
                    "  ${path}: ${word} stands alone on its line\n")
            endif()
        elseif(NOT word IN_LIST areas)
            string(APPEND wrong "  ${path}: ${word} is no area of the map\n")
        endif()
    endforeach()
    if(path MATCHES "/$")
        if(NOT IS_DIRECTORY "${ROOT}/${path}")
            string(APPEND wrong "  ${path}: no such directory\n")
        endif()
    elseif(path MATCHES "\\*")
        file(GLOB matches "${ROOT}/${path}")
        if(matches STREQUAL "")
            string(APPEND wrong "  ${path}: names no file\n")
        endif()
    elseif(NOT EXISTS "${ROOT}/${path}" OR IS_DIRECTORY "${ROOT}/${path}")
        string(APPEND wrong "  ${path}: no such file\n")
    endif()
endforeach()

# tools/compile.sh reads the map in another language, so it spells the
# path that tests/test_areas.cmake names.
file(RELATIVE_PATH map_path "${ROOT}" "${MAP}")
string(REPLACE "," ";" scripts "${SCRIPTS}")
foreach(script IN LISTS scripts)
    file(READ "${ROOT}/${script}" body)
    string(FIND "${body}" "${map_path}" at)
    if(at EQUAL -1)
        string(APPEND wrong "  ${script} does not spell ${map_path}\n")
    endif()
endforeach()

# The text of <path> as a regular expression.
function(regex_quote out path)
    foreach(special "\\" "." "+" "*" "?" "^" "$" "(" ")" "|" "{" "}")
        string(REPLACE "${special}" "\\${special}" path "${path}")
    endforeach()
    set(${out} "${path}" PARENT_SCOPE)
endfunction()

# Each probe is a regular expression of a word of a command and the areas
# a test that names it carries. A program ends its word or stands before
# a `,`, and a directory may stand before a `/` as well.
set(probes "")
set(probe_areas "")
set(probe_names "")
function(add_probe expression name)
    list(JOIN ARGN "," joined)
    set(probes ${probes} "${expression}" PARENT_SCOPE)
    set(probe_areas ${probe_areas} "${joined}" PARENT_SCOPE)
    set(probe_names ${probe_names} "${name}" PARENT_SCOPE)
endfunction()
string(REPLACE "," ";" compilers "${COMPILERS}")
foreach(program IN LISTS compilers)
    regex_quote(quoted "${program}")
    get_filename_component(file "${program}" NAME)
    add_probe("${quoted}(,|$)" "${file}" compiler runtime std)
endforeach()
string(REPLACE "," ";" tools "${TOOLS}")
foreach(program IN LISTS tools)
    regex_quote(quoted "${program}")
    get_filename_component(file "${program}" NAME)
    add_probe("${quoted}(,|$)" "${file}" tool compiler runtime std)
endforeach()
string(REPLACE "," ";" archives "${RUNTIME}")
foreach(archive IN LISTS archives)
    regex_quote(quoted "${archive}")
    get_filename_component(file "${archive}" NAME)
    add_probe("${quoted}([,/]|$)" "${file}/" runtime std)
endforeach()
regex_quote(root "${ROOT}")
add_probe("${root}/src/antic([,/]|$)" "src/antic" compiler)
add_probe("${root}/src/anti([,/]|$)" "src/anti" tool)
add_probe("${root}/src/rt([,/]|$)" "src/rt" runtime)
add_probe("${root}/src/native([,/]|$)" "src/native" runtime)
add_probe("${root}/src/std([,/]|$)" "src/std" std)
list(LENGTH probes probe_count)
math(EXPR probe_last "${probe_count} - 1")

# DESIGN: the list is read line by line, by the indentation ctest gives
# it, as run_skips_registered.cmake reads it, because string(JSON) parses
# the whole text of several megabytes again at every call. A test stands
# at four spaces, the words of its command at eight and the words of the
# value of a property at twelve.
file(READ "${list}" text)
string(REPLACE "\r" "" text "${text}")
string(REPLACE ";" "," text "${text}")
string(REPLACE "[" "<" text "${text}")
string(REPLACE "]" ">" text "${text}")
string(REPLACE "\n" ";" lines "${text}")

set(count 0)
set(name "")
set(labels "")
set(needed "")
set(seen "")
set(in_labels FALSE)
foreach(line IN LISTS lines)
    if(in_labels)
        if(line MATCHES "^            \"(.*)\",?$")
            list(APPEND labels "${CMAKE_MATCH_1}")
            continue()
        elseif(line MATCHES "^          >")
            set(in_labels FALSE)
        endif()
    endif()
    if(line MATCHES "^        \"(.*)\",?$")
        set(word "${CMAKE_MATCH_1}")
        foreach(index RANGE ${probe_last})
            list(GET probes ${index} probe)
            if(word MATCHES "${probe}")
                list(GET probe_areas ${index} wanted)
                list(GET probe_names ${index} what)
                string(REPLACE "," ";" wanted "${wanted}")
                list(APPEND needed ${wanted})
                list(APPEND seen "${what}")
            endif()
        endforeach()
    elseif(line MATCHES "^      \"name\" : \"(.*)\",?$")
        set(name "${CMAKE_MATCH_1}")
    elseif(line MATCHES "^          \"name\" : \"LABELS\"")
        set(in_labels TRUE)
    elseif(line MATCHES "^    },?$")
        if(NOT name STREQUAL "")
            math(EXPR count "${count} + 1")
            if(labels STREQUAL "")
                string(APPEND wrong "  ${name} carries no area\n")
            endif()
            foreach(label IN LISTS labels)
                if(NOT label IN_LIST areas)
                    string(APPEND wrong
                        "  ${name} carries ${label}, which is no area\n")
                endif()
            endforeach()
            set(missing "")
            list(REMOVE_DUPLICATES needed)
            foreach(area IN LISTS needed)
                if(NOT area IN_LIST labels)
                    list(APPEND missing "${area}")
                endif()
            endforeach()
            if(NOT missing STREQUAL "")
                list(REMOVE_DUPLICATES seen)
                list(JOIN seen ", " seen)
                list(JOIN missing " " missing)
                string(APPEND wrong
                    "  ${name} names ${seen} and lacks the areas ${missing}\n")
            endif()
        endif()
        set(name "")
        set(labels "")
        set(needed "")
        set(seen "")
        set(in_labels FALSE)
    endif()
endforeach()

if(count EQUAL 0)
    message(FATAL_ERROR "${list} lists no test")
endif()
if(NOT wrong STREQUAL "")
    message(FATAL_ERROR "the areas of the tests, by tests/test-areas.txt:\n"
                        "${wrong}")
endif()

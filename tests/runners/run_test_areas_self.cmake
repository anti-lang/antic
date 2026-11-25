# tests/runners/run_test_areas.cmake refuses a test without an area, an
# area outside the map, a test that lacks the area of what its command
# names and a wrong line of the map, and accepts the rest. Run with cmake
# -P and these values:
#   RUNNER  tests/runners/run_test_areas.cmake
#   WORK    a directory for the files
#
# Each case writes a list in the form of `ctest --show-only=json-v1` and
# a map, runs the check on both and states whether it must accept them. A
# case that must be refused names a text of the message, so a refusal for
# another reason fails the case.

cmake_minimum_required(VERSION 3.21)

file(REMOVE_RECURSE "${WORK}")
set(root "${WORK}/root")
file(MAKE_DIRECTORY "${root}/src/antic" "${root}/docs" "${root}/tools"
     "${root}/tests")
file(WRITE "${root}/tools/a-pin" "1\n")
file(WRITE "${root}/README.md" "text\n")
set(build "${WORK}/build")
set(antic "${build}/antic")
set(anti "${build}/anti")
set(runtime "${build}/runtime")

# The text of one test of the list: its name, its areas with a `,`
# between two, and the words of its command.
function(test_entry out name areas)
    set(words "")
    foreach(word IN LISTS ARGN)
        string(APPEND words "        \"${word}\",\n")
    endforeach()
    set(labels "")
    if(NOT areas STREQUAL "")
        string(REPLACE "," ";" areas "${areas}")
        set(values "")
        foreach(area IN LISTS areas)
            list(APPEND values "            \"${area}\"")
        endforeach()
        list(JOIN values ",\n" values)
        set(labels "        {\n          \"name\" : \"LABELS\",\n          \"value\" : \n          [\n${values}\n          ]\n        },\n")
    endif()
    set(${out} "    {\n      \"backtrace\" : 1,\n      \"command\" : \n      [\n${words}        \"-P\",\n        \"/run.cmake\"\n      ],\n      \"name\" : \"${name}\",\n      \"properties\" : \n      [\n${labels}        {\n          \"name\" : \"WORKING_DIRECTORY\",\n          \"value\" : \"/tmp\"\n        }\n      ]\n    }" PARENT_SCOPE)
endfunction()

set(good_map "areas: compiler runtime std tool unit\n# a comment\n\ndocs/ none\nREADME.md none\ntools/*-pin all\nsrc/antic/ compiler\n")

set(failures "")
# areas_case(<name> accept|<text of the refusal> <map> <entry>...)
function(areas_case name verdict map)
    list(JOIN ARGN ",\n" entries)
    set(list "${WORK}/${name}.json")
    file(WRITE "${list}" "{\n  \"kind\" : \"ctestInfo\",\n  \"tests\" : \n  [\n${entries}\n  ],\n  \"version\" : \n  {\n    \"major\" : 1,\n    \"minor\" : 0\n  }\n}\n")
    file(WRITE "${WORK}/${name}.txt" "${map}")
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DJSON=${list}"
                            "-DMAP=${WORK}/${name}.txt" "-DROOT=${root}"
                            "-DCOMPILERS=${antic}" "-DTOOLS=${anti}"
                            "-DRUNTIME=${runtime}"
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
    # CMake wraps a long message, so the text is read with its spaces and
    # its line ends made one space.
    string(REGEX REPLACE "[ \t\r\n]+" " " flat "${out}${err}")
    string(FIND "${flat}" "${verdict}" at)
    if(at EQUAL -1)
        set(failures
            "${failures}${name}: refused without `${verdict}`\n${out}${err}\n"
            PARENT_SCOPE)
    endif()
endfunction()

test_entry(unit a_unit unit "/bin/cmake" "-DWORK=${build}/w")
test_entry(bare a_bare "" "/bin/cmake")
test_entry(outside an_outside "unit,docs" "/bin/cmake")
test_entry(compiled a_program "compiler,runtime,std"
           "/bin/cmake" "-DANTIC=${antic}" "-DRUNTIME=${runtime}")
test_entry(uncompiled a_program "compiler" "/bin/cmake" "-DANTIC=${antic}")
test_entry(listed a_refusal "compiler,runtime,std"
           "/bin/cmake" "-DCOMMAND=${antic},-o,x")
test_entry(unlisted a_refusal "unit" "/bin/cmake" "-DCOMMAND=${antic},-o,x")
test_entry(unit_program a_unit_program unit
           "/bin/cmake" "-DPROGRAM=${antic}_unit_tests")
test_entry(built a_build "tool,compiler,runtime,std"
           "/bin/cmake" "-DANTI=${anti}")
test_entry(unbuilt a_build "compiler,runtime,std" "/bin/cmake" "-DANTI=${anti}")
test_entry(archived a_library "runtime,std" "/bin/cmake" "${runtime}/lib")
test_entry(unarchived a_library "compiler" "/bin/cmake" "${runtime}/lib")
test_entry(sourced a_guard "compiler" "/bin/cmake" "-DFILES=${root}/src/antic")
test_entry(unsourced a_guard "unit" "/bin/cmake" "-DFILES=${root}/src/antic/a.c")
test_entry(std_source a_guard "unit" "/bin/cmake" "-DFILES=${root}/src/std")

areas_case(good accept "${good_map}" "${unit}" "${compiled}" "${listed}"
           "${unit_program}" "${built}" "${archived}" "${sourced}")
areas_case(bare "a_bare carries no area" "${good_map}" "${unit}" "${bare}")
areas_case(outside "an_outside carries docs, which is no area" "${good_map}"
           "${outside}")
areas_case(uncompiled "a_program names antic and lacks the areas runtime std"
           "${good_map}" "${uncompiled}")
areas_case(unlisted
           "a_refusal names antic and lacks the areas compiler runtime std"
           "${good_map}" "${unlisted}")
areas_case(unbuilt "a_build names anti and lacks the areas tool"
           "${good_map}" "${unbuilt}")
areas_case(unarchived "a_library names runtime/ and lacks the areas runtime std"
           "${good_map}" "${unarchived}")
areas_case(unsourced "a_guard names src/antic and lacks the areas compiler"
           "${good_map}" "${unsourced}")
areas_case(std_source "a_guard names src/std and lacks the areas std"
           "${good_map}" "${std_source}")
areas_case(no_test "lists no test" "${good_map}")
areas_case(no_areas "holds 0 lines that start with areas:"
           "docs/ none\n" "${unit}")
areas_case(no_directory "src/anti/: no such directory"
           "${good_map}src/anti/ tool\n" "${unit}")
areas_case(no_file "INSTALL.md: no such file"
           "${good_map}INSTALL.md none\n" "${unit}")
areas_case(directory_as_file "docs: no such file"
           "${good_map}docs none\n" "${unit}")
areas_case(no_match "tools/*.sh: names no file"
           "${good_map}tools/*.sh all\n" "${unit}")
areas_case(unknown_word "README.md: packaging is no area of the map"
           "${good_map}README.md packaging\n" "${unit}")
areas_case(word_beside "docs/: all stands alone on its line"
           "${good_map}docs/ all unit\n" "${unit}")
areas_case(no_words "line 8 of the map is no path and words"
           "${good_map}README.md\n" "${unit}")

# A script that reads the map spells its path from the root.
file(WRITE "${root}/tests/areas.txt" "${good_map}")
file(WRITE "${root}/tools/reader.sh" "map=tests/areas.txt\n")
file(WRITE "${root}/tools/other.sh" "map=areas.txt\n")
foreach(script reader other)
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DJSON=${WORK}/good.json"
                            "-DMAP=${root}/tests/areas.txt" "-DROOT=${root}"
                            "-DCOMPILERS=${antic}" "-DTOOLS=${anti}"
                            "-DRUNTIME=${runtime}"
                            "-DSCRIPTS=tools/${script}.sh"
                            -P "${RUNNER}"
                    RESULT_VARIABLE status OUTPUT_VARIABLE out
                    ERROR_VARIABLE err ENCODING NONE)
    string(REGEX REPLACE "[ \t\r\n]+" " " flat "${out}${err}")
    string(FIND "${flat}" "tools/other.sh does not spell tests/areas.txt" at)
    if(script STREQUAL "reader" AND NOT status EQUAL 0)
        string(APPEND failures "script_spells: refused\n${out}${err}\n")
    elseif(script STREQUAL "other" AND (status EQUAL 0 OR at EQUAL -1))
        string(APPEND failures "script_misspells: not refused for it\n${out}${err}\n")
    endif()
endforeach()

if(NOT failures STREQUAL "")
    message(FATAL_ERROR "run_test_areas.cmake:\n${failures}")
endif()

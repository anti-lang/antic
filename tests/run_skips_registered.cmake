# Every test whose runner can print `SKIP:` sets SKIP_REGULAR_EXPRESSION,
# so ctest shows the skip and its reason. A runner that prints the line
# and returns ends with status 0, which ctest counts as a pass without
# the property. The test reads the tests of the tree as
# `ctest --show-only=json-v1` lists them, finds each one that runs a
# runner with -P and reads the runner for a message that starts with
# `SKIP:`. Run with cmake -P and these values:
#   CTEST   the ctest executable
#   TREE    the build tree
# or with JSON, a file that holds such a list, in place of both.

cmake_minimum_required(VERSION 3.21)

if(DEFINED JSON)
    set(list "${JSON}")
else()
    set(list "${TREE}/Testing/skips-registered.json")
    execute_process(COMMAND "${CTEST}" --test-dir "${TREE}"
                            --show-only=json-v1
                    OUTPUT_FILE "${list}" RESULT_VARIABLE status
                    ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "ctest --show-only failed with ${status}\n${err}")
    endif()
endif()

# DESIGN: the list is read line by line, by the indentation ctest gives
# it, because string(JSON) parses the whole text of several megabytes
# again at every call. A test stands at an indentation of four spaces,
# its keys at six, the words of its command at eight and the keys of a
# property at ten. A bracket and a semicolon of the text would bend the
# list of lines, so each becomes another character first. The ctest of
# Windows ends a line with a carriage return as well.
file(READ "${list}" text)
string(REPLACE "\r" "" text "${text}")
string(REPLACE ";" "," text "${text}")
string(REPLACE "[" "<" text "${text}")
string(REPLACE "]" ">" text "${text}")
string(REPLACE "\n" ";" lines "${text}")

# Sets ${out} to ALWAYS when the runner can print a line that starts with
# SKIP: whatever it is given, to the variables that give the reason when
# each such line prints the value of one variable alone, and to the empty
# text when it prints none. A reason the caller passes, as SKIP_RUN of
# run_program.cmake, skips only a test whose command defines it. The
# answer is kept per runner.
function(runner_skips runner out)
    string(MAKE_C_IDENTIFIER "${runner}" key)
    get_property(known GLOBAL PROPERTY "skips_${key}" SET)
    if(NOT known)
        set(skips "")
        if(EXISTS "${runner}")
            file(READ "${runner}" text)
            string(REGEX MATCHALL "message\\([ \n]*\"SKIP:[^\"]*\"" lines
                   "${text}")
            foreach(line IN LISTS lines)
                if(line MATCHES "\"SKIP: \\$\\{([A-Za-z_]+)\\}\"$")
                    list(APPEND skips "${CMAKE_MATCH_1}")
                else()
                    set(skips ALWAYS)
                    break()
                endif()
            endforeach()
        endif()
        set_property(GLOBAL PROPERTY "skips_${key}" "${skips}")
    endif()
    get_property(skips GLOBAL PROPERTY "skips_${key}")
    set(${out} "${skips}" PARENT_SCOPE)
endfunction()

set(missing "")
set(count 0)
set(name "")
set(runner "")
set(has_skip FALSE)
set(defined "")
set(after_p FALSE)
foreach(line IN LISTS lines)
    if(line MATCHES "^        \"-D([A-Za-z_]+)=")
        list(APPEND defined "${CMAKE_MATCH_1}")
    endif()
    if(after_p)
        if(line MATCHES "^        \"(.*)\",?$")
            set(runner "${CMAKE_MATCH_1}")
        endif()
        set(after_p FALSE)
    elseif(line MATCHES "^        \"-P\",?$")
        set(after_p TRUE)
    elseif(line MATCHES "^      \"name\" : \"(.*)\",?$")
        set(name "${CMAKE_MATCH_1}")
    elseif(line MATCHES "^          \"name\" : \"SKIP_REGULAR_EXPRESSION\"")
        set(has_skip TRUE)
    elseif(line MATCHES "^    },?$")
        if(NOT name STREQUAL "" AND NOT runner STREQUAL "")
            math(EXPR count "${count} + 1")
            if(NOT has_skip)
                runner_skips("${runner}" skips)
                set(skipping FALSE)
                if(skips STREQUAL "ALWAYS")
                    set(skipping TRUE)
                else()
                    foreach(variable IN LISTS skips)
                        if(variable IN_LIST defined)
                            set(skipping TRUE)
                        endif()
                    endforeach()
                endif()
                if(skipping)
                    get_filename_component(file "${runner}" NAME)
                    string(APPEND missing "  ${name}, ${file}\n")
                endif()
            endif()
        endif()
        set(name "")
        set(runner "")
        set(has_skip FALSE)
        set(defined "")
    endif()
endforeach()

if(count EQUAL 0)
    message(FATAL_ERROR "${list} lists no test that runs a runner")
endif()
if(NOT missing STREQUAL "")
    message(FATAL_ERROR "these tests run a runner that prints SKIP: and set "
                        "no SKIP_REGULAR_EXPRESSION, so a skip passes:\n"
                        "${missing}")
endif()

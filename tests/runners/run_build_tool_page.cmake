# The site page of the build tool describes `anti` as it is, by decision 8
# of "Decisions of 2026-10-11" in docs/decisions.md (M363). Run with
# cmake -P and these values:
#   ANTI   the anti executable
#   ANTIC  the antic executable
#   PAGE   the page, docs/site/build-tool/index.md
#
# The commands are the ones the usage of `anti --help` names: the first
# word after `anti`, and the second as well where it is a word of small
# letters, as `symbols resolve` is. The table under "## Commands" names
# each of them once and nothing else.
#
# A code span or a line of an `sh` block that starts with `anti ` names a
# command of the usage, or the first word of a command of two words, and
# every `--option` in it is an option the usage of anti names, or the
# --help or --version that anti takes before a command. One that
# starts with `antic ` or a path that ends in `/antic` holds options of
# the usage of antic alone. A code span that starts with `--` names an
# option of either tool. Before it reads the page, the runner checks
# itself on spans that must be refused and spans that must pass, so a
# pattern that finds nothing fails the test.

cmake_minimum_required(VERSION 3.21)

foreach(value ANTI ANTIC PAGE)
    if(NOT DEFINED ${value})
        message(FATAL_ERROR "${value} is not set")
    endif()
endforeach()

# A `;`, `[` or `]` of a text would split or join the elements of a list,
# so each stands as a control character while the lines are a list.
string(ASCII 1 semicolon)
string(ASCII 2 open_bracket)
string(ASCII 3 close_bracket)

function(escape out text)
    string(REPLACE ";" "${semicolon}" text "${text}")
    string(REPLACE "[" "${open_bracket}" text "${text}")
    string(REPLACE "]" "${close_bracket}" text "${text}")
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

# usage_of(<out> <tool>): what `<tool> --help` prints, which ends with 0.
function(usage_of out tool)
    execute_process(COMMAND "${tool}" --help
        RESULT_VARIABLE status OUTPUT_VARIABLE text ERROR_VARIABLE text)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "${tool} --help ends with ${status}")
    endif()
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

# options_of(<out> <text>): every `--option` the text names, sorted.
function(options_of out text)
    string(REGEX MATCHALL "--[a-z][a-z-]*" found "${text}")
    list(REMOVE_DUPLICATES found)
    list(SORT found)
    set(${out} "${found}" PARENT_SCOPE)
endfunction()

usage_of(anti_usage "${ANTI}")
usage_of(antic_usage "${ANTIC}")
options_of(anti_options "${anti_usage}")
options_of(antic_options "${antic_usage}")
# The usage of anti lists the options of its commands. The tool itself
# takes --help and --version before any command, which the usage does
# not list, so the runner asks for both.
foreach(option --help --version)
    execute_process(COMMAND "${ANTI}" ${option}
        RESULT_VARIABLE status OUTPUT_QUIET ERROR_QUIET)
    if(status EQUAL 0)
        list(APPEND anti_options "${option}")
    endif()
endforeach()

# The commands of the usage, and the first words of the commands of two.
escape(usage_escaped "${anti_usage}")
string(REPLACE "\n" ";" usage_lines "${usage_escaped}")
set(commands "")
set(heads "")
foreach(line IN LISTS usage_lines)
    if(line MATCHES "^(usage:)? *anti ([a-z]+)( ([a-z]+)( |$))?")
        if("${CMAKE_MATCH_4}" STREQUAL "")
            list(APPEND commands "${CMAKE_MATCH_2}")
        else()
            list(APPEND commands "${CMAKE_MATCH_2} ${CMAKE_MATCH_4}")
            list(APPEND heads "${CMAKE_MATCH_2}")
        endif()
    endif()
endforeach()
list(REMOVE_DUPLICATES commands)
list(REMOVE_DUPLICATES heads)
list(LENGTH commands command_count)
if(command_count LESS 10)
    message(FATAL_ERROR "the usage of anti gives ${command_count} commands: ${commands}")
endif()

# command_of(<out> <text>): the command a text that starts with `anti `
# names, as the usage spells it. A word after the first word of a command
# of two words is its second word, and any other text names the first
# word alone.
function(command_of out text)
    string(REGEX MATCH "^anti ([a-z]+)( ([a-z]+))?" ignored "${text}")
    set(first "${CMAKE_MATCH_1}")
    set(second "${CMAKE_MATCH_3}")
    if(NOT "${second}" STREQUAL "" AND
       ("${first} ${second}" IN_LIST commands OR first IN_LIST heads))
        set(${out} "${first} ${second}" PARENT_SCOPE)
    else()
        set(${out} "${first}" PARENT_SCOPE)
    endif()
endfunction()

# check_text(<out> <text>): what is wrong with one code span or one
# command line of the page, or nothing.
function(check_text out text)
    set(wrong "")
    options_of(named "${text}")
    if(text MATCHES "^anti ")
        command_of(command "${text}")
        # `anti --help` names the tool and no command.
        if(text MATCHES "^anti -")
            set(command "")
        endif()
        if(NOT "${command}" STREQUAL "" AND NOT command IN_LIST commands AND
           NOT command IN_LIST heads)
            list(APPEND wrong "`anti ${command}` is no command of anti")
        endif()
        foreach(option IN LISTS named)
            if(NOT option IN_LIST anti_options)
                list(APPEND wrong "${option} is no option of anti")
            endif()
        endforeach()
    elseif(text MATCHES "^(antic|[^ ]*/antic) ")
        foreach(option IN LISTS named)
            if(NOT option IN_LIST antic_options)
                list(APPEND wrong "${option} is no option of antic")
            endif()
        endforeach()
    elseif(text MATCHES "^(--[a-z][a-z-]*)")
        set(option "${CMAKE_MATCH_1}")
        if(NOT option IN_LIST anti_options AND NOT option IN_LIST antic_options)
            list(APPEND wrong "${option} is no option of anti or antic")
        endif()
    endif()
    set(${out} "${wrong}" PARENT_SCOPE)
endfunction()

set(failures "")

# The check of the runner itself: each text and whether it is refused.
function(self_case text refused)
    check_text(wrong "${text}")
    if(refused AND "${wrong}" STREQUAL "")
        set(failures "${failures}\nthe runner takes `${text}`, which it must refuse"
            PARENT_SCOPE)
    elseif(NOT refused AND NOT "${wrong}" STREQUAL "")
        set(failures "${failures}\nthe runner refuses `${text}`: ${wrong}"
            PARENT_SCOPE)
    endif()
endfunction()
self_case("anti build --release --target all" FALSE)
self_case("anti symbols resolve <trace.txt> --symbols <symbols.zip>" FALSE)
self_case("anti symbols" FALSE)
self_case("anti license --project --notice" FALSE)
self_case("--lto full" FALSE)
self_case("antic --dev --package-name <name>" FALSE)
self_case("anti publish --to ff" TRUE)
self_case("anti test --all-modes" TRUE)
self_case("anti build --llvm-mc x" TRUE)
self_case("../build/antic --dev --llvm-mc x" TRUE)
self_case("--all-modes" TRUE)
self_case("anti symbols upload" TRUE)
self_case("anti --help" FALSE)
self_case("anti --verbose" TRUE)
if(NOT "${failures}" STREQUAL "")
    message(FATAL_ERROR "the runner does not check what it says:${failures}")
endif()

file(READ "${PAGE}" content)
escape(content "${content}")
string(REPLACE "\n" ";" lines "${content}")

set(table "")
set(section "")
set(fence "")
set(pending "")
set(number 0)
foreach(line IN LISTS lines)
    math(EXPR number "${number} + 1")
    # The line keeps its control characters, so that a span with a `;`
    # or a bracket stays one element of a list.
    set(text "${line}")
    if(text MATCHES "^```(.*)$")
        if("${fence}" STREQUAL "")
            set(fence "${CMAKE_MATCH_1}")
            if("${fence}" STREQUAL "")
                set(fence "none")
            endif()
        else()
            set(fence "")
        endif()
        continue()
    endif()
    if(NOT "${fence}" STREQUAL "")
        # A line of a command that ends in `\` goes on in the next line.
        if("${fence}" STREQUAL "sh")
            string(APPEND pending "${text}")
            if(pending MATCHES "\\\\$")
                string(REGEX REPLACE "\\\\$" "" pending "${pending}")
                continue()
            endif()
            string(STRIP "${pending}" command)
            set(pending "")
            check_text(wrong "${command}")
            foreach(item IN LISTS wrong)
                string(APPEND failures "\n${PAGE}:${number}: ${item}")
            endforeach()
        endif()
        continue()
    endif()
    if(text MATCHES "^## (.*)$")
        set(section "${CMAKE_MATCH_1}")
    endif()
    string(REPLACE "\\|" "|" unescaped "${text}")
    string(REGEX MATCHALL "`[^`]+`" spans "${unescaped}")
    set(first TRUE)
    foreach(span IN LISTS spans)
        string(REGEX REPLACE "^`(.*)`$" "\\1" span "${span}")
        check_text(wrong "${span}")
        foreach(item IN LISTS wrong)
            string(APPEND failures "\n${PAGE}:${number}: ${item}")
        endforeach()
        # The first cell of a row of the table of commands.
        if(first AND "${section}" STREQUAL "Commands" AND text MATCHES "^\\| `anti ")
            command_of(command "${span}")
            list(APPEND table "${command}")
        endif()
        set(first FALSE)
    endforeach()
endforeach()

foreach(command IN LISTS commands)
    if(NOT command IN_LIST table)
        string(APPEND failures "\nthe usage names `anti ${command}`, and the table under \"## Commands\" does not")
    endif()
endforeach()
foreach(command IN LISTS table)
    if(NOT command IN_LIST commands)
        string(APPEND failures "\nthe table under \"## Commands\" names `anti ${command}`, which the usage does not")
    endif()
endforeach()

if(NOT "${failures}" STREQUAL "")
    message(FATAL_ERROR "the page describes another tool than anti:${failures}")
endif()
message(STATUS "${command_count} commands of anti, each in the table of the page")

# Every option of antic and every command and option of anti is passed by
# a test, or stands in the list of the open ones. Run with cmake -P and
# these values:
#   ROOT      the source tree, which holds src/antic/main.c,
#             src/anti/main.c and tests
#   OPEN      tests/options-open.txt
#   ANTIC     the path of antic in the build tree
#   ANTI      the path of anti in the build tree
#   CTEST     ctest, with TREE the build tree whose tests it lists and
#             WORK a directory for what it prints
#   COMMANDS  instead of CTEST, TREE and WORK, a file that holds what
#             `ctest -N -V` prints
#
# A unit is one line of OPEN: `antic <option>`, `anti <command>`,
# `anti <command> <option>`, `anti <command> <sub-command>` or
# `anti <option>`. The units are read from the sources:
#   - of src/antic/main.c, every `strcmp(arg, "-...")`
#   - of src/anti/main.c, every `strcmp(argv[1], "...")` as a command or an
#     option of anti itself, every `strcmp(argv[2], "...")` as a
#     sub-command, and every other `strcmp(<array>[...], "-...")` as an
#     option. The function they stand in names the command, as
#     `doc_command` and `build_options` do.
#
# A test passes a unit of antic when
#   - the command line of a registered test holds the path of antic and
#     the option as an argument, or
#   - a runner under tests/ holds `${ANTIC}`, or a call of antic_build,
#     antic_run or antic_program, and the option as a word.
# A test passes a unit of anti when, with a call of anti_run read as the
# call of anti it makes,
#   - the command line of a registered test holds the path of anti, then
#     the command and then the option, each as an argument, or
#   - one command of a runner that holds `${ANTI}` holds the command and
#     then the option, or
#   - the option stands in a runner that runs that one command of anti and
#     no other, or
#   - a function of such a runner adds the option to its `${ARGN}`, and a
#     call of that function holds the command.
#
# The test fails when
#   - a unit has no line in OPEN and no test passes it
#   - a line of OPEN names a unit that a test passes
#   - a line of OPEN names no unit of the sources, or stands twice
#   - an option of anti stands in a function that names no command
#
# DESIGN: the command lines come from `ctest -N -V`, which prints the
# command of every test the build tree registered, with every variable of
# tests/CMakeLists.txt expanded. The dump tests pass `--dump-${stage}`,
# which no reading of the CMake file finds. A runner is read as text, with
# its comment lines removed. The runner follows no variable of a runner,
# so an option that only a variable holds reads as open until a test
# passes it in a command of its own.

cmake_minimum_required(VERSION 3.21)

string(ASCII 1 SEMI)
string(ASCII 2 LEFT)
string(ASCII 3 RIGHT)
string(ASCII 4 BS)
# The paths of antic and of anti in a command line.
string(ASCII 5 IS_ANTIC)
string(ASCII 6 IS_ANTI)

function(encode out text)
    string(REPLACE ";" "${SEMI}" text "${text}")
    string(REPLACE "[" "${LEFT}" text "${text}")
    string(REPLACE "]" "${RIGHT}" text "${text}")
    string(REPLACE "\\" "${BS}" text "${text}")
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

function(read_text out file)
    file(READ "${file}" text)
    encode(text "${text}")
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

set(faults "")
set(units "")

# The options of antic.
read_text(source "${ROOT}/src/antic/main.c")
string(REGEX MATCHALL "strcmp\\(arg, \"-[^\"]+\"\\)" calls "${source}")
foreach(call IN LISTS calls)
    string(REGEX REPLACE "^strcmp\\(arg, \"(.*)\"\\)$" "\\1" option "${call}")
    list(APPEND units "antic ${option}")
endforeach()

# The commands and the options of anti, function by function. A function
# starts at a line that begins with a letter and names it before its `(`.
read_text(source "${ROOT}/src/anti/main.c")
set(source "\n${source}")
set(header "\n[A-Za-z_][^\n(]*[ *]([A-Za-z_0-9]+)\\(")
set(compare "strcmp\\(([a-z]+)${LEFT}([^${RIGHT}]*)${RIGHT}, \"([^\"]+)\"\\)")
set(bodies "")
set(rest "${source}")
set(function "")
set(commands "")
set(pending "")
while(TRUE)
    if(rest MATCHES "${header}")
        set(next "${CMAKE_MATCH_1}")
        string(FIND "${rest}" "${CMAKE_MATCH_0}" at)
        string(LENGTH "${CMAKE_MATCH_0}" length)
        string(SUBSTRING "${rest}" 0 ${at} body)
        math(EXPR after "${at} + ${length}")
        string(SUBSTRING "${rest}" ${after} -1 rest)
        set(last OFF)
    else()
        set(body "${rest}")
        set(last ON)
    endif()
    string(REGEX MATCHALL "${compare}" calls "${body}")
    foreach(call IN LISTS calls)
        string(REGEX MATCH "${compare}" call "${call}")
        set(index "${CMAKE_MATCH_2}")
        set(word "${CMAKE_MATCH_3}")
        if(index STREQUAL "1")
            if(word MATCHES "^-")
                list(APPEND units "anti ${word}")
            else()
                list(APPEND commands "${word}")
                list(APPEND units "anti ${word}")
            endif()
        elseif(index STREQUAL "2")
            list(APPEND pending "${function} ${word}")
        elseif(word MATCHES "^-")
            list(APPEND pending "${function} ${word}")
        endif()
    endforeach()
    if(last)
        break()
    endif()
    set(function "${next}")
endwhile()
# The command of a function is the first word of its name.
foreach(entry IN LISTS pending)
    # A group is read inside quotes, since `if` reads the name of an
    # unset CMAKE_MATCH_<n> as its own text.
    string(REGEX MATCH "^([a-z0-9]+)_[^ ]* (.*)$" whole "${entry}")
    set(command "${CMAKE_MATCH_1}")
    set(word "${CMAKE_MATCH_2}")
    if("${whole}" STREQUAL "" OR NOT "${command}" IN_LIST commands)
        string(APPEND faults "  src/anti/main.c: `${entry}` is a function and "
               "its option, and the function names no command of anti\n")
        continue()
    endif()
    list(APPEND units "anti ${command} ${word}")
endforeach()
list(REMOVE_DUPLICATES units)

# The command lines of the registered tests.
if(NOT DEFINED COMMANDS)
    # ctest writes into a file, as every program a test reads does.
    file(MAKE_DIRECTORY "${WORK}")
    set(COMMANDS "${WORK}/commands.txt")
    execute_process(COMMAND "${CTEST}" --test-dir "${TREE}" -N -V
                    RESULT_VARIABLE status OUTPUT_FILE "${COMMANDS}"
                    ERROR_FILE "${WORK}/ctest.err")
    if(NOT status EQUAL 0)
        file(READ "${WORK}/ctest.err" err)
        message(FATAL_ERROR "ctest -N -V failed with ${status}\n${err}")
    endif()
endif()
file(READ "${COMMANDS}" listing)
# antic first, since the path of anti starts the path of antic.
string(REPLACE "${ANTIC}" "${IS_ANTIC}" listing "${listing}")
string(REPLACE "${ANTI}" "${IS_ANTI}" listing "${listing}")
encode(listing "${listing}")
string(REGEX MATCHALL "Test command:[^\n]*${IS_ANTIC}[\", ][^\n]*" lines
       "${listing}")
list(JOIN lines "\n" antic_lines)
string(APPEND antic_lines "\n")
string(REGEX MATCHALL "Test command:[^\n]*${IS_ANTI}[\", ][^\n]*" lines
       "${listing}")
list(JOIN lines "\n" anti_lines)
string(APPEND anti_lines "\n")

# The runners, without their comment lines.
file(GLOB_RECURSE scripts "${ROOT}/tests/*.cmake")
set(antic_runners "")
set(anti_runners "")
set(count 0)
foreach(file IN LISTS scripts)
    read_text(text "${file}")
    string(REGEX REPLACE "(^|\n)[ \t]*#[^\n]*" "\\1" text "${text}")
    # A call of anti_run of tests/program_output.cmake reads as the call
    # of anti it makes.
    string(REGEX REPLACE
           "anti_run\\([ \t\n]*[A-Za-z_0-9]+[ \t\n]+[A-Za-z_0-9]+[ \t\n]+[A-Za-z_0-9]+[ \t\n]+(\"[^\"\n]*\"|[^ \t\n\"]+)[ \t\n]+(ENV[^)]*[ \t\n]COMMAND[ \t\n]+)?"
           "\"\${ANTI}\" " text "${text}")
    if(text MATCHES "[$]{ANTIC}|(^|[^a-z_])antic_(build|run|program)[(]")
        string(APPEND antic_runners "${text}\n")
    endif()
    if(text MATCHES "[$]{ANTI}")
        set(runner_${count} "${text}")
        # The commands the runner gives anti itself.
        string(REGEX MATCHALL "[$]{ANTI}\"[ \t\n]+\"?[a-z]+" calls "${text}")
        set(runs "")
        foreach(call IN LISTS calls)
            string(REGEX REPLACE "^.*[ \t\n\"]([a-z]+)$" "\\1" call "${call}")
            list(APPEND runs "${call}")
        endforeach()
        list(REMOVE_DUPLICATES runs)
        set(runs_${count} "${runs}")
        # The functions of the runner that hand their arguments on.
        string(REPLACE "endfunction()" "${IS_ANTIC}" blocks "${text}")
        string(REGEX MATCHALL "(^|\n)function\\([^${IS_ANTIC}]*" blocks
               "${blocks}")
        set(helpers 0)
        foreach(block IN LISTS blocks)
            if(block MATCHES "^\n?function\\(([A-Za-z_0-9]+)" AND
               block MATCHES "[$]{ARGN}")
                set(helper_${count}_${helpers} "${block}")
                math(EXPR helpers "${helpers} + 1")
            endif()
        endforeach()
        set(helpers_${count} ${helpers})
        math(EXPR count "${count} + 1")
    endif()
endforeach()
# What stands around a word of a runner, and around an argument of a
# command line. A runner hands a list of arguments on with `,` or `|`
# between them.
set(before "[^A-Za-z0-9_/.-]")
set(after "[^A-Za-z0-9_=-]")
set(opens "[\",=|]")
set(closes "[\",|]")

# Whether a test passes the unit.
function(passed out tool first second)
    set(${out} ON PARENT_SCOPE)
    if(tool STREQUAL "antic")
        if(antic_lines MATCHES "${opens}${first}${closes}" OR
           antic_runners MATCHES "${before}${first}${after}")
            return()
        endif()
        set(${out} OFF PARENT_SCOPE)
        return()
    endif()
    set(tool_then "${IS_ANTI}(\"? \"|[,|])")
    if(second STREQUAL "")
        if(anti_lines MATCHES "${tool_then}${first}${closes}")
            return()
        endif()
        set(index 0)
        while(index LESS count)
            if(runner_${index} MATCHES
               "[$]{ANTI}\"[ \t\n]+\"?${first}${after}")
                return()
            endif()
            math(EXPR index "${index} + 1")
        endwhile()
        set(${out} OFF PARENT_SCOPE)
        return()
    endif()
    if(anti_lines MATCHES
       "${tool_then}${first}${closes}[^\n]*${opens}${second}${closes}")
        return()
    endif()
    set(index 0)
    while(index LESS count)
        if(runner_${index} MATCHES
           "${before}${first}[ \t\n\"]*\\(?[^()]*${before}${second}${after}")
            return()
        endif()
        if(runs_${index} STREQUAL first AND
           runner_${index} MATCHES "${before}${second}${after}")
            return()
        endif()
        set(helper 0)
        while(helper LESS helpers_${index})
            set(block "${helper_${index}_${helper}}")
            math(EXPR helper "${helper} + 1")
            if(NOT block MATCHES "${before}${second}${after}")
                continue()
            endif()
            string(REGEX MATCH "^\n?function\\(([A-Za-z_0-9]+)" name "${block}")
            if(runner_${index} MATCHES
               "${before}${CMAKE_MATCH_1}\\([^()]*${before}${first}${after}")
                return()
            endif()
        endwhile()
        math(EXPR index "${index} + 1")
    endwhile()
    set(${out} OFF PARENT_SCOPE)
endfunction()

# The lines of OPEN.
get_filename_component(open_name "${OPEN}" NAME)
read_text(open_text "${OPEN}")
string(REPLACE "\n" ";" open_lines "${open_text}")
set(open "")
set(number 0)
foreach(line IN LISTS open_lines)
    math(EXPR number "${number} + 1")
    if("${line}" STREQUAL "" OR line MATCHES "^#")
        continue()
    endif()
    if(line IN_LIST open)
        string(APPEND faults "  ${open_name}:${number}: `${line}` stands twice\n")
        continue()
    endif()
    list(FIND units "${line}" at)
    if(at EQUAL -1)
        string(APPEND faults "  ${open_name}:${number}: `${line}` is no option "
               "and no command of the sources\n")
        continue()
    endif()
    list(APPEND open "${line}")
    set(open_line_${at} ${number})
endforeach()

set(total 0)
set(covered 0)
set(listed 0)
set(index -1)
foreach(unit IN LISTS units)
    math(EXPR index "${index} + 1")
    math(EXPR total "${total} + 1")
    if(NOT unit MATCHES "^(antic|anti) ([^ ]+) ?([^ ]*)$")
        message(FATAL_ERROR "the unit `${unit}` has another form")
    endif()
    passed(held "${CMAKE_MATCH_1}" "${CMAKE_MATCH_2}" "${CMAKE_MATCH_3}")
    if(held)
        math(EXPR covered "${covered} + 1")
        if(DEFINED open_line_${index})
            string(APPEND faults "  ${open_name}:${open_line_${index}}: a test "
                   "passes `${unit}`, so the line goes\n")
        endif()
    elseif(DEFINED open_line_${index})
        math(EXPR listed "${listed} + 1")
    else()
        string(APPEND faults "  no test passes `${unit}`, and ${open_name} "
               "does not list it\n")
    endif()
endforeach()

if(NOT faults STREQUAL "")
    message(FATAL_ERROR "the options and the tests that pass them "
                        "disagree:\n${faults}")
endif()
message(STATUS "${total} options and commands: ${covered} passed by a test, "
               "${listed} open")

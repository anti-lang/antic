# tests/run_options_covered.cmake accepts a tree whose options and
# commands are each passed by a test or listed as open, and refuses every
# other one with a text that names the fault. Run with cmake -P and these
# values:
#   RUNNER  tests/run_options_covered.cmake
#   WORK    a directory for the trees
#
# Each case writes the two sources, two runners, the command lines of four
# tests and a list, runs the runner on them and states whether it must
# accept them. A case that must be refused names a text of the message, so
# a refusal for another reason fails the case.

cmake_minimum_required(VERSION 3.21)

set(antic [=[
int main(int argc, char **argv)
{
    if (strcmp(arg, "--alpha") == 0) {
    } else if (strcmp(arg, "--beta") == 0) {
    } else if (strcmp(arg, "-g") == 0) {
    } else if (strcmp(arg, "--dump-ir") == 0) {
    }
}
]=])
# The options of two commands, a sub-command, a value that is no option
# and an option of anti itself.
set(anti [=[
static int build_options(int argc, char **argv)
{
    if (strcmp(argv[i], "--fast") == 0) {
    } else if (strcmp(argv[i], "--slow") == 0) {
    } else if (strcmp(argv[i], "--tools") == 0) {
    } else if (strcmp(argv[i], "--lib") == 0 && i + 1 < argc) {
        if (strcmp(argv[i + 1], "static") == 0) {
        }
    }
}

static int pack_command(int argc, char **argv)
{
    if (strcmp(argv[2], "list") == 0) {
    } else if (strcmp(argv[i], "--out") == 0) {
    } else if (strcmp(argv[i], "--all") == 0) {
    }
}

static int command(int argc, char **argv)
{
    if (strcmp(argv[1], "--version") == 0) {
    } else if (strcmp(argv[1], "build") == 0) {
    } else if (strcmp(argv[1], "pack") == 0) {
    }
}
]=])
# A runner that gives anti one command, and one whose function adds an
# option to the arguments of its callers.
set(script [=[
# "${ANTI}" pack --all is a comment and passes nothing
execute_process(COMMAND "${ANTI}" pack list --out "${WORK}/x")
execute_process(COMMAND "${ANTIC}" ${options} "${SOURCE}")
set(options -g)
]=])
set(helper [=[
function(build what)
    execute_process(COMMAND "${ANTI}" ${ARGN} --tools "${TOOLS}")
endfunction()
build("a library" build --lib static)
]=])
set(commands [=[
  1: Test command: /tree/antic "--alpha" "x.anti"
  2: Test command: /usr/bin/cmake "-DANTIC=/tree/antic" "-DOPTIONS=--beta|--dump-ir" "-P" "/src/tests/run_a.cmake"
  3: Test command: /tree/anti "build" "--fast"
  4: Test command: /usr/bin/cmake "-DCOMMAND=/tree/anti,--version" "-P" "/src/tests/run_expect.cmake"
  5: Test command: /tree/antic_unit_tests "--slow"
]=])
set(open "# open\n\nanti build --slow\nanti pack --all\n")

set(failures "")
# covered_case(<name> accept|<text of the refusal> [ANTI <text>]
#              [COMMANDS <text>] [OPEN <text>])
# Each keyword replaces one file of the tree.
function(covered_case name verdict)
    cmake_parse_arguments(PARSE_ARGV 2 arg "" "ANTI;COMMANDS;OPEN" "")
    foreach(value ANTI COMMANDS OPEN)
        if(NOT DEFINED arg_${value})
            string(TOLOWER "${value}" variable)
            set(arg_${value} "${${variable}}")
        endif()
    endforeach()
    set(dir "${WORK}/${name}")
    file(REMOVE_RECURSE "${dir}")
    file(WRITE "${dir}/src/antic/main.c" "${antic}")
    file(WRITE "${dir}/src/anti/main.c" "${arg_ANTI}")
    file(WRITE "${dir}/tests/run_a.cmake" "${script}")
    file(WRITE "${dir}/tests/run_b.cmake" "${helper}")
    file(WRITE "${dir}/commands.txt" "${arg_COMMANDS}")
    file(WRITE "${dir}/open.txt" "${arg_OPEN}")
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DROOT=${dir}"
                            "-DOPEN=${dir}/open.txt" "-DANTIC=/tree/antic"
                            "-DANTI=/tree/anti"
                            "-DCOMMANDS=${dir}/commands.txt" -P "${RUNNER}"
                    RESULT_VARIABLE status OUTPUT_VARIABLE out
                    ERROR_VARIABLE err ENCODING NONE)
    if(verdict STREQUAL "accept")
        if(NOT status EQUAL 0)
            set(failures "${failures}${name}: refused\n${out}${err}\n"
                PARENT_SCOPE)
        endif()
        return()
    endif()
    string(FIND "${out}${err}" "${verdict}" at)
    if(status EQUAL 0)
        set(failures "${failures}${name}: accepted\n" PARENT_SCOPE)
    elseif(at EQUAL -1)
        set(failures "${failures}${name}: refused without `${verdict}`\n${out}${err}\n"
            PARENT_SCOPE)
    endif()
endfunction()

# Fourteen units, twelve of them passed and two open.
covered_case(agree accept)
# A line for a unit that a test passes.
covered_case(open_and_passed
    "open.txt:5: a test passes `anti build --fast`, so the line goes"
    OPEN "${open}anti build --fast\n")
# A unit that no test passes and no line names. `--slow` goes to another
# program, and `--all` stands in a comment.
covered_case(neither "no test passes `anti build --slow`"
    OPEN "# open\nanti pack --all\n")
covered_case(a_comment "no test passes `anti pack --all`"
    OPEN "# open\nanti build --slow\n")
# The option of a command counts for that command alone: a test passes
# `--fast` to build, and pack takes it as well.
covered_case(another_command "no test passes `anti pack --fast`"
    ANTI "${anti}\nstatic int pack_more(int argc, char **argv)\n{\n    if (strcmp(argv[i], \"--fast\") == 0) {\n    }\n}\n")
# An option that reaches antic through no command line and no runner.
covered_case(no_command_line "no test passes `antic --alpha`"
    COMMANDS "  3: Test command: /tree/anti \"build\" \"--fast\" \"--alpha\"\n  4: Test command: /tree/anti \"--version\"\n")
# A line that names no unit, and a line that stands twice.
covered_case(no_unit "open.txt:5: `antic --gamma` is no option"
    OPEN "${open}antic --gamma\n")
covered_case(twice "open.txt:5: `anti pack --all` stands twice"
    OPEN "${open}anti pack --all\n")
# An option in a function that names no command.
covered_case(no_command "`other_options --odd` is a function and its option"
    ANTI "${anti}\nstatic int other_options(int argc, char **argv)\n{\n    if (strcmp(argv[i], \"--odd\") == 0) {\n    }\n}\n")

if(NOT failures STREQUAL "")
    message(FATAL_ERROR "run_options_covered.cmake:\n${failures}")
endif()

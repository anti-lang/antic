# tests/run_diagnostics_covered.cmake accepts a tree whose messages are
# each expected by a test or listed as open, and refuses every other one
# with a text that names the fault. Run with cmake -P and these values:
#   RUNNER  tests/run_diagnostics_covered.cmake
#   WORK    a directory for the trees
#
# Each case writes a tree of two sources, three expected outputs and a
# list, runs the runner on it and states whether it must accept the tree.
# A case that must be refused names a text of the message, so a refusal
# for another reason fails the case. No message of a case is a message of
# antic or of anti, since the runner of the suite reads this file as an
# expected output.

cmake_minimum_required(VERSION 3.21)

set(header [=[
#define WORD "two"
]=])
# A function that hands its format on, a format with a macro between its
# pieces, a conditional with two messages and a text that reaches its call
# through `%s`.
set(antic [=[
void diagnostics_add(struct diagnostics *d, int line, int column,
                     const char *format, ...);

void sema_error_at(struct checker *c, struct pos pos, const char *format,
                   ...)
{
    /* diagnostics_add(d, 1, 1, "a call inside a comment"); */
    diagnostics_add(c->diags, pos.line, pos.column, "%s", message);
}

static void check(struct checker *c, bool second)
{
    sema_error_at(c, c->pos, "the sample `%s` has " WORD
                  " parts; and %d more", c->name, 3);
    sema_error_at(c, c->pos,
                  second ? "a second sample" : "no sample at all");
    diagnostics_add(c->diags, 1, 1, "%s", "a sample [of] text");
}
]=])
# Two messages, a format that hands a text on and a text that goes to
# another stream.
set(anti [=[
static int run(const char *path, const char *message)
{
    fprintf(stderr, "anti: %s is no sample\n", path);
    fputs("anti: the sample is closed\n", stderr);
    fprintf(stderr, "anti: %s\n", message);
    fputs("usage: anti sample\n", stdout);
    return 1;
}
]=])
set(err "a.anti:1:1: error: the sample `x` has two parts; and 3 more\na.anti:2:1: warning: a sample [of] text [sample-name]\n")
set(unit "    expect(\"a second \"\n           \"sample\");\n")
set(script "if(NOT text MATCHES \"anti: .* is no sample\")\nendif()\n")
set(open "# open\nno sample at all\nanti: the sample is closed\\n\n")

set(failures "")
# covered_case(<name> accept|<text of the refusal> [ANTIC <text>]
#              [SCRIPT <text>] [OPEN <text>])
# Each keyword replaces one file of the tree.
function(covered_case name verdict)
    cmake_parse_arguments(PARSE_ARGV 2 arg "" "ANTIC;SCRIPT;OPEN" "")
    foreach(value ANTIC SCRIPT OPEN)
        if(NOT DEFINED arg_${value})
            string(TOLOWER "${value}" variable)
            set(arg_${value} "${${variable}}")
        endif()
    endforeach()
    set(dir "${WORK}/${name}")
    file(REMOVE_RECURSE "${dir}")
    file(WRITE "${dir}/src/antic/a.h" "${header}")
    file(WRITE "${dir}/src/antic/a.c" "${arg_ANTIC}")
    file(WRITE "${dir}/src/anti/b.c" "${anti}")
    file(WRITE "${dir}/tests/errors/a.err" "${err}")
    file(WRITE "${dir}/tests/unit/test_a.c" "${unit}")
    file(WRITE "${dir}/tests/run_b.cmake" "${arg_SCRIPT}")
    file(WRITE "${dir}/open.txt" "${arg_OPEN}")
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DROOT=${dir}"
                            "-DOPEN=${dir}/open.txt" -P "${RUNNER}"
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

# Five messages, three of them expected and two open.
covered_case(agree accept)
# A line for a message that a test expects.
covered_case(open_and_expected
    "open.txt:4: a test expects `a second sample` of src/antic/a.c"
    OPEN "${open}a second sample\n")
# A message that no test expects and no line names.
covered_case(neither
    "src/antic/a.c: no test expects `no sample at all`"
    OPEN "# open\nanti: the sample is closed\\n\n")
# An expected output that holds a part of a message does not hold it.
covered_case(a_part
    "src/anti/b.c: no test expects `anti: %s is no sample\\n`"
    SCRIPT "if(NOT text MATCHES \"is no sample\")\nendif()\n")
# A message of antic stands whole in an expected output: the end of
# another message is not it.
covered_case(an_end "src/antic/a.c: no test expects `and 3 more`"
    ANTIC "${antic}\nstatic void more(struct checker *c)\n{\n    sema_error_at(c, c->pos, \"and 3 more\");\n}\n")
# A message of anti that ends its line ends there in the expected output.
covered_case(a_longer_line
    "src/anti/b.c: no test expects `anti: %s is no sample\\n`"
    SCRIPT "if(NOT text MATCHES \"anti: .* is no sample of ours\")\nendif()\n")
# A line that names no message, and a line that stands twice.
covered_case(no_message "open.txt:4: `a sample of nobody` is no message"
    OPEN "${open}a sample of nobody\n")
covered_case(twice "open.txt:4: `no sample at all` stands twice"
    OPEN "${open}no sample at all\n")
# A function that hands its text on and that the runner does not know.
covered_case(unread
    "the text of sema_error_at in helper is `text`, which the runner cannot read"
    ANTIC "${antic}\nstatic void helper(struct checker *c, const char *text)\n{\n    sema_error_at(c, c->pos, text);\n}\n")

if(NOT failures STREQUAL "")
    message(FATAL_ERROR "run_diagnostics_covered.cmake:\n${failures}")
endif()

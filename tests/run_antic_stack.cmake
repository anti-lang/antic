# Compile the most deeply nested expression the parser accepts, with the
# stack of the main thread cut to 256 KB. antic runs its work on a thread
# with a stack of its own size, so the limit of the main thread does not
# reach the passes. A debug antic needs more than 256 KB for this source,
# and before the thread it ended by SIGSEGV here. Run with cmake -P and
# these values:
#   ANTIC  the antic executable
#   WORK   a directory this run writes into
#
# The limit is `ulimit -s` of a POSIX shell, so Windows has no run of
# this test. Its main thread has 1 MB, which the unit test deep_stack of
# test_tool_platform.c passes on the same helper.

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

# 126 parentheses around `x`, each with a `+ 1`: the parser refuses one
# more as nesting deeper than PARSE_DEPTH_MAX.
set(depth 126)
set(expr "x")
foreach(n RANGE 1 ${depth})
    set(expr "(${expr} + 1)")
endforeach()
file(WRITE "${WORK}/deep.anti"
    "fn main() -> int\n{\n\tlet x = 1;\n\treturn ${expr} - x - ${depth};\n}\n")

execute_process(
    COMMAND sh -c "ulimit -s 256 && exec \"$0\" -S \"$1\" -o \"$2\""
            "${ANTIC}" "${WORK}/deep.anti" "${WORK}/deep.s"
    RESULT_VARIABLE status
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
    ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR
        "antic failed on the deepest source with a main stack of 256 KB, "
        "status ${status}:\n${out}${err}")
endif()
if(NOT EXISTS "${WORK}/deep.s")
    message(FATAL_ERROR "antic wrote no assembly:\n${out}${err}")
endif()

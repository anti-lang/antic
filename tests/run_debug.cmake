# The debug information of antic -g: a program and a module it imports, a
# breakpoint set by file and line in each, and a backtrace that names the
# Anti functions. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime archive
#   WORK      a directory for the sources and the binaries
#   DEBUGGER  lldb or gdb, the debugger of the host
#   KIND      lldb or gdb, which one it is
#
# The programs are written here rather than kept as fixtures, because the
# test names the line of every breakpoint.

set(root "${WORK}/root")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${root}/com/example" "${WORK}/lib/com/example")

# Line 3 holds the addition and line 4 the multiplication.
file(WRITE "${root}/com/example/step.anti"
     "pub fn step(n: int) -> int\n"
     "{\n"
     "\tlet m = n + 1;\n"
     "\treturn m * 2;\n"
     "}\n")
# Line 5 holds the call and line 6 the subtraction.
file(WRITE "${root}/app.anti"
     "import com.example.step;\n"
     "\n"
     "fn main() -> int\n"
     "{\n"
     "\tlet v = step.step(3);\n"
     "\treturn v - 8;\n"
     "}\n")

execute_process(COMMAND "${ANTIC}" -c -I "${root}"
                        -o "${WORK}/lib/com/example/step.antl"
                        "${root}/com/example/step.anti"
                RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic -c failed\n${err}")
endif()

# Build the program twice, with the source positions and without them.
function(build name)
    execute_process(COMMAND "${ANTIC}" ${ARGN} --llvm-mc "${LLVM_MC}"
                            --runtime "${RUNTIME}" -I "${root}"
                            -I "${WORK}/lib" -o "${WORK}/${name}"
                            "${root}/app.anti"
                    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "antic failed for ${name}\n${err}")
    endif()
endfunction()

build(app -g)
build(plain)

# A .file directive per source and a .loc before the first instruction of
# every statement.
file(READ "${WORK}/app.s" assembly)
foreach(wanted "\.file 1 \"com/example/step\.anti\"" "\.file 2 \"app\.anti\""
        "\.loc 1 3 0" "\.loc 1 4 0" "\.loc 2 5 0" "\.loc 2 6 0")
    if(NOT assembly MATCHES "${wanted}")
        message(FATAL_ERROR "the assembly of -g holds no `${wanted}`")
    endif()
endforeach()

# Without -g the assembly carries no position at all.
file(READ "${WORK}/plain.s" bare)
if(bare MATCHES "\.loc |\.file |\.cv_")
    message(FATAL_ERROR "a build without -g wrote a source position")
endif()

# The program runs the same either way, and returns 0 from 3 + 1 times 2
# less 8.
foreach(name app plain)
    execute_process(COMMAND "${WORK}/${name}" RESULT_VARIABLE code
                    OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
    if(NOT code EQUAL 0)
        message(FATAL_ERROR "${name} exited with ${code}\n${out}${err}")
    endif()
endforeach()

# One breakpoint by file and line, then a backtrace. The frame of the
# breakpoint names the function of the imported module and its line, and
# the frame below it names main and the line of the call.
if(KIND STREQUAL "lldb")
    set(command "${DEBUGGER}" -b -o "breakpoint set -f step.anti -l 4"
        -o "run" -o "bt" -o "quit" "${WORK}/app")
else()
    set(command "${DEBUGGER}" -batch -ex "break step.anti:4" -ex "run"
        -ex "bt" "${WORK}/app")
endif()
execute_process(COMMAND ${command} WORKING_DIRECTORY "${root}"
                OUTPUT_VARIABLE session ERROR_VARIABLE session_err
                RESULT_VARIABLE status ENCODING NONE)
set(session "${session}${session_err}")
foreach(frame "com\.example\.step\.step[^\n]*step\.anti:4"
        "app\.main[^\n]*app\.anti:5")
    if(NOT session MATCHES "${frame}")
        message(FATAL_ERROR
                "${KIND} printed no frame matching `${frame}`\n${session}")
    endif()
endforeach()

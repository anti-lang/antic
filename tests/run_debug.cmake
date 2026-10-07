# The debug information of antic -g: a program and a module it imports, a
# breakpoint set by file and line in each, and a backtrace that names the
# Anti functions. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime archive
#   WORK      a directory for the sources and the binaries
#   DEBUGGER  lldb or gdb, the debugger of the host
#   KIND      lldb or gdb, which one it is
#   OBJECT    the suffix of an object file, .o or .obj
#
# The programs are written here rather than kept as fixtures, because the
# test names the line of every breakpoint. Both build in dev mode, which
# inlines nothing, so every function is a frame of its own. Release mode
# of the LLVM back end inlines step and folds the program to its result,
# as the entry on inlined frames in docs/decisions.md says.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

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
# Line 5 holds the call of the imported module, line 10 the call of the
# function that holds it and line 11 the call of a copy of a generic and
# the subtraction. Line 16 holds the addition in the copy. main calls
# through helper so that the frame below the breakpoint is a function of
# the program and not the runtime entry, which shares its address with
# main.
file(WRITE "${root}/app.anti"
     "import com.example.step;\n"
     "\n"
     "fn helper() -> int\n"
     "{\n"
     "\treturn step.step(3);\n"
     "}\n"
     "\n"
     "fn main() -> int\n"
     "{\n"
     "\tlet v = helper();\n"
     "\treturn twice(v) - 16;\n"
     "}\n"
     "\n"
     "fn twice<T: add>(v: T) -> T\n"
     "{\n"
     "\treturn v + v;\n"
     "}\n")

execute_process(COMMAND "${ANTIC}" -c -I "${root}"
                        -o "${WORK}/lib/com/example/step.antl"
                        "${root}/com/example/step.anti"
                RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic -c failed\n${err}")
endif()

# Run antic in dev mode in the root, with the library file of the module
# on the search path.
function(dev_build name source)
    execute_process(COMMAND "${ANTIC}" --dev ${ARGN} --llvm-mc "${LLVM_MC}"
                            --runtime "${RUNTIME}" -I "${root}"
                            -I "${WORK}/lib" -o "${WORK}/${name}"
                            "${source}"
                    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "antic failed for ${name}\n${err}")
    endif()
endfunction()

# The object and the assembly of each module, with the source positions
# and without them, and the program linked from the objects of each kind.
foreach(module "com/example/step" "app")
    get_filename_component(base "${module}" NAME)
    dev_build(${base}.s "${root}/${module}.anti" -g -S)
    dev_build(${base}_plain.s "${root}/${module}.anti" -S)
endforeach()
dev_build(step "${root}/com/example/step.anti" -g)
dev_build(step_plain "${root}/com/example/step.anti")
dev_build(app "${root}/app.anti" -g "${WORK}/step${OBJECT}")
dev_build(plain "${root}/app.anti" "${WORK}/step_plain${OBJECT}")

# A .file directive per source and a .loc before the first instruction of
# every statement. The number of each file comes from its directive. llc
# writes `.file 1 "" "com/example/step.anti"`, the same with a tab, or
# the directory apart, `.file 1 "com/example" "step.anti"`.
foreach(source "com/example/step" "app")
    get_filename_component(base "${source}" NAME)
    get_filename_component(directory "${source}" DIRECTORY)
    file(READ "${WORK}/${base}.s" assembly)
    if(NOT assembly MATCHES
       "\\.file[ \t]+([0-9]+)[ \t]+((\"\"[ \t]+)?\"${source}|\"${directory}\"[ \t]+\"${base})\\.anti\"")
        message(FATAL_ERROR "the assembly of -g names no ${source}.anti")
    endif()
    set(file_${base} "${CMAKE_MATCH_1}")
    set(assembly_${base} "${assembly}")
endforeach()
foreach(wanted "step ${file_step} 3" "step ${file_step} 4"
        "app ${file_app} 5" "app ${file_app} 10" "app ${file_app} 11")
    string(REPLACE " " ";" parts "${wanted}")
    list(POP_FRONT parts base)
    list(JOIN parts " " position)
    string(REPLACE " " "[ \t]+" pattern "${position}")
    if(NOT assembly_${base} MATCHES "\\.loc[ \t]+${pattern}[ \t]+0")
        message(FATAL_ERROR "the assembly of -g holds no `.loc ${position} 0`")
    endif()
endforeach()

# Without -g the assembly carries no position at all. llc names the
# source of an ELF object with a `.file` of no number, which is no
# position.
foreach(base step app)
    file(READ "${WORK}/${base}_plain.s" bare)
    if(bare MATCHES "\\.loc[ \t]|\\.file[ \t]+[0-9]|\\.cv_")
        message(FATAL_ERROR "a build of ${base} without -g wrote a source "
                            "position")
    endif()
endforeach()

# The program runs the same either way, and returns 0 from 3 + 1 times 2,
# doubled, less 16.
foreach(name app plain)
    program_expect("${name}" COMMAND "${WORK}/${name}")
endforeach()

# A breakpoint by file and line, then a backtrace. The frame that stops
# names its Anti function and the position, and the frames below it name
# the Anti functions that called it.
#
# The two debuggers spell the name of a function differently. lldb prints
# the symbol, `com.example.step.step`, and gdb writes the last segment in
# brackets, `com.example.step[step]`. The character before the segment is
# left open for both.
#
# The compile unit holds an entry per function, with the name a person
# reads, so both debuggers read a position for every frame. A copy of a
# generic is named as the program writes it, `app.twice<int>`, where its
# symbol escapes the angle brackets.
function(session file line)
    if(KIND STREQUAL "lldb")
        set(command "${DEBUGGER}" -b -o "breakpoint set -f ${file} -l ${line}"
            -o "run" -o "bt" -o "quit" "${WORK}/app")
    else()
        set(command "${DEBUGGER}" -batch -ex "break ${file}:${line}"
            -ex "run" -ex "bt" "${WORK}/app")
    endif()
    execute_process(COMMAND ${command} WORKING_DIRECTORY "${root}"
                    OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
    set(session "${out}${err}" PARENT_SCOPE)
endfunction()

function(holds session wanted)
    if(NOT session MATCHES "${wanted}")
        message(FATAL_ERROR
                "${KIND} printed nothing matching `${wanted}`\n${session}")
    endif()
endfunction()

# A breakpoint in the module that came from a library file. The backtrace
# holds all three Anti functions. main shares its address with the symbol
# of the runtime entry, and a debugger prints one of the two names.
session(step.anti 4)
holds("${session}" "com\\.example\\.step.step[^\n]*step\\.anti:4")
holds("${session}" "app.helper")
holds("${session}" "app.main|anti\\.rt.main")
holds("${session}" "app.helper[^\n]*app\\.anti:5")
holds("${session}" "(app.main|anti\\.rt.main)[^\n]*app\\.anti:10")

# A breakpoint in the program's own file resolves as well.
session(app.anti 5)
holds("${session}" "app.helper[^\n]*app\\.anti:5")

# A breakpoint in a copy of a generic names the copy as the program writes
# it, and its caller below it.
session(app.anti 16)
holds("${session}" "app\\.twice<int>[^\n]*app\\.anti:16")
holds("${session}" "(app.main|anti\\.rt.main)[^\n]*app\\.anti:11")

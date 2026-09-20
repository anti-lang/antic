# Every dev-mode check, in a build that has the checks and a build that
# does not. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime archive
#   SOURCES   the directory of the check programs
#   WORK      a directory for the files
#   STRINGS   a tool that lists the strings of a file
#
# Each program runs to its end and exits with 7 when the checks are
# absent. With them it prints the file, the line, the operation and the
# values, then aborts. Release mode drops them and dev mode keeps them,
# and --checks and --no-checks override either mode.

function(build name source)
    execute_process(COMMAND "${ANTIC}" ${ARGN} --llvm-mc "${LLVM_MC}"
                            --runtime "${RUNTIME}" -o "${WORK}/${name}"
                            "${source}"
                    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "antic failed for ${name}\n${err}")
    endif()
endfunction()

# One check: the release build carries no text of it and runs to its end,
# and the dev build prints the failure and aborts. A program whose
# operation traps by itself without the check is built in release mode
# and not run.
function(check name phrase pattern runs)
    set(source "${SOURCES}/${name}.anti")
    build("${name}.release" "${source}")
    execute_process(COMMAND "${STRINGS}" "${WORK}/${name}.release"
                    OUTPUT_VARIABLE text ENCODING NONE)
    if(text MATCHES "${phrase}")
        message(FATAL_ERROR "the release build of ${name} carries `${phrase}`")
    endif()
    if(runs)
        execute_process(COMMAND "${WORK}/${name}.release" RESULT_VARIABLE code
                        OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
        if(NOT code EQUAL 7)
            message(FATAL_ERROR "the release build of ${name} exited with "
                                "${code}, expected 7\n${out}${err}")
        endif()
    endif()
    build("${name}.dev" "${source}" --dev)
    execute_process(COMMAND "${WORK}/${name}.dev" RESULT_VARIABLE code
                    OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
    if(code EQUAL 7)
        message(FATAL_ERROR "the check of ${name} did not stop the program")
    endif()
    if(NOT err MATCHES "${pattern}")
        message(FATAL_ERROR "${name} printed `${err}`, expected `${pattern}`")
    endif()
endfunction()

file(MAKE_DIRECTORY "${WORK}")

check(bounds_array "index out of bounds"
      "bounds_array\\.anti:[0-9]+: index out of bounds: index 5, length 4" ON)
check(bounds_slice "index out of bounds"
      "bounds_slice\\.anti:[0-9]+: index out of bounds: index 4, length 2" ON)
check(bounds_str "index out of bounds"
      "bounds_str\\.anti:[0-9]+: index out of bounds: index 7, length 3" ON)
check(overflow_add "overflow in"
      "overflow_add\\.anti:[0-9]+: overflow in \\+: left 9223372036854775807, right 1" ON)
check(overflow_sub "overflow in"
      "overflow_sub\\.anti:[0-9]+: overflow in -: left -9223372036854775807, right 2" ON)
check(overflow_mul "overflow in"
      "overflow_mul\\.anti:[0-9]+: overflow in \\*: left 4000000000, right 4000000000" ON)
check(overflow_mul32 "overflow in"
      "overflow_mul32\\.anti:[0-9]+: overflow in \\*: left 100000, right 100000" ON)
check(narrow "out of range"
      "narrow\\.anti:[0-9]+: value out of range for i8: value 300" ON)
check(narrow_sign "out of range"
      "narrow_sign\\.anti:[0-9]+: value out of range for u64: value -1" ON)
check(divide "division by zero"
      "divide\\.anti:[0-9]+: division by zero in /: left 10" OFF)
check(remainder "division by zero"
      "remainder\\.anti:[0-9]+: division by zero in %: left 10" OFF)
check(shift_wide "shift count out of range"
      "shift_wide\\.anti:[0-9]+: shift count out of range for <<: count 64, width 64" ON)
check(shift_negative "shift count out of range"
      "shift_negative\\.anti:[0-9]+: shift count out of range for >>: count -1, width 64" ON)

# --checks puts them into a release build, and --no-checks takes them out
# of a dev build. Both override the mode.
set(bounds "${SOURCES}/bounds_array.anti")
build(forced "${bounds}" --checks)
execute_process(COMMAND "${WORK}/forced" RESULT_VARIABLE code
                ERROR_VARIABLE err ENCODING NONE)
if(code EQUAL 7 OR NOT err MATCHES "index out of bounds: index 5, length 4")
    message(FATAL_ERROR "--checks did not reach a release build\n${err}")
endif()
execute_process(COMMAND "${ANTIC}" --dev --no-checks -S -o "${WORK}/off.s"
                        "${bounds}" RESULT_VARIABLE status)
file(READ "${WORK}/off.s" off)
if(NOT status EQUAL 0 OR off MATCHES "check_failed")
    message(FATAL_ERROR "--no-checks kept the checks of a dev build")
endif()
execute_process(COMMAND "${ANTIC}" --dev -S -o "${WORK}/on.s" "${bounds}"
                RESULT_VARIABLE status)
file(READ "${WORK}/on.s" on)
if(NOT status EQUAL 0 OR NOT on MATCHES "check_failed")
    message(FATAL_ERROR "dev mode dropped the checks")
endif()

# DESIGN: the decision belongs to the build that compiles the program. A
# library file carries every check, and the same library gives a release
# program without them and a dev program that stops on one.
file(MAKE_DIRECTORY "${WORK}/root/com/example" "${WORK}/lib/com/example")
file(WRITE "${WORK}/root/com/example/nth.anti"
     "pub fn nth(a: []int, i: int) -> int\n{\n\treturn a[i];\n}\n")
execute_process(COMMAND "${ANTIC}" -c -I "${WORK}/root"
                        -o "${WORK}/lib/com/example/nth.antl"
                        "${WORK}/root/com/example/nth.anti"
                RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic -c failed\n${err}")
endif()
file(WRITE "${WORK}/consumer.anti"
     "import com.example.nth;\n\nfn main() -> int\n{\n\tlet a = [1, 2];\n\treturn nth.nth(a[0..2], 5);\n}\n")
foreach(name quiet trap)
    set(flag "")
    if(name STREQUAL "trap")
        set(flag --checks)
    endif()
    execute_process(COMMAND "${ANTIC}" ${flag} --llvm-mc "${LLVM_MC}"
                            --runtime "${RUNTIME}" -I "${WORK}/lib"
                            -o "${WORK}/${name}" "${WORK}/consumer.anti"
                    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "antic failed for ${name}\n${err}")
    endif()
    execute_process(COMMAND "${WORK}/${name}" RESULT_VARIABLE code
                    ERROR_VARIABLE ran ENCODING NONE)
    execute_process(COMMAND "${STRINGS}" "${WORK}/${name}"
                    OUTPUT_VARIABLE text ENCODING NONE)
    if(name STREQUAL "quiet")
        if(text MATCHES "index out of bounds")
            message(FATAL_ERROR "a library check reached a release program")
        endif()
    elseif(NOT ran MATCHES "nth\\.anti:[0-9]+: index out of bounds: index 5, length 2")
        message(FATAL_ERROR "--checks did not keep the library check\n${ran}")
    endif()
endforeach()

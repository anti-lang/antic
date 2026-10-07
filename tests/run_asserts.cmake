# The five behaviours of `assert`. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime archive
#   SOURCE    a program whose assertion fails
#   WORK      a directory for the files
#
# A release build runs the program to its end and carries no text of the
# assertion. A dev build, and any build with --asserts, prints the text
# and aborts. --no-asserts drops them in dev mode too.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

file(MAKE_DIRECTORY "${WORK}")

# Release is the default, so the assertion is gone.
antic_program("${WORK}/release" "${SOURCE}")
program_expect("release" COMMAND "${WORK}/release" STATUS 7)
file(STRINGS "${WORK}/release" text)
if(text MATCHES "assertion failed")
    message(FATAL_ERROR "the release build carries the text of an assertion")
endif()

# --asserts overrides the mode, and the failure names file, line and the
# source of the condition.
antic_program("${WORK}/forced" "${SOURCE}" --asserts)
program_expect("forced" COMMAND "${WORK}/forced" ABORTS
               ERR_MATCH "^asserts\\.anti:[0-9]+: assertion failed: n > 0\n$")

# Dev mode keeps them without a flag, and --no-asserts drops them.
execute_process(COMMAND "${ANTIC}" --dev --runtime "${RUNTIME}" -S
                        -o "${WORK}/dev.s" "${SOURCE}"
                RESULT_VARIABLE status)
file(READ "${WORK}/dev.s" dev)
if(NOT status EQUAL 0 OR NOT dev MATCHES "assert_failed")
    message(FATAL_ERROR "dev mode dropped the assertion")
endif()
execute_process(COMMAND "${ANTIC}" --dev --no-asserts --runtime "${RUNTIME}" -S
                        -o "${WORK}/off.s" "${SOURCE}" RESULT_VARIABLE status)
file(READ "${WORK}/off.s" off)
if(NOT status EQUAL 0 OR off MATCHES "assert_failed")
    message(FATAL_ERROR "--no-asserts kept the assertion")
endif()

# DESIGN: the decision belongs to the build that compiles the program. A
# library file carries the assertion, and the same library gives a release
# program without it and a program built with --asserts one that traps.
get_filename_component(here "${SOURCE}" DIRECTORY)
file(MAKE_DIRECTORY "${WORK}/root/com/example" "${WORK}/lib/com/example")
file(WRITE "${WORK}/root/com/example/checked.anti"
     "pub fn halve(n: int) -> int\n{\n\tassert(n > 0);\n\treturn n / 2;\n}\n")
execute_process(COMMAND "${ANTIC}" -c -I "${WORK}/root"
                        -o "${WORK}/lib/com/example/checked.antl"
                        "${WORK}/root/com/example/checked.anti"
                RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic -c failed\n${err}")
endif()
file(WRITE "${WORK}/consumer.anti"
     "import com.example.checked;\n\nfn main() -> int\n{\n\treturn checked.halve(0);\n}\n")
foreach(case "quiet;" "trap;--asserts")
    list(GET case 0 name)
    list(LENGTH case parts)
    set(flag "")
    if(parts GREATER 1)
        list(GET case 1 flag)
    endif()
    antic_program("${WORK}/${name}" "${WORK}/consumer.anti" ${flag}
                  -I "${WORK}/lib")
    if(name STREQUAL "quiet")
        program_expect("${name}" COMMAND "${WORK}/${name}")
        file(STRINGS "${WORK}/${name}" text)
        if(text MATCHES "assertion failed")
            message(FATAL_ERROR "the library assertion reached a release "
                                "program")
        endif()
    else()
        program_expect("${name}" COMMAND "${WORK}/${name}" ABORTS
                       ERR_MATCH "^com/example/checked\\.anti:3: assertion failed: n > 0\n$")
    endif()
endforeach()

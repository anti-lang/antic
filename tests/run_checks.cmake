# The options that decide the dev-mode checks instead of the mode, and a
# library whose checks follow the build that links it. tests/run_check.cmake
# runs each check. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime archive
#   SOURCES   the directory of the check programs
#   WORK      a directory for the files
#
# Release mode drops the checks and dev mode keeps them, and --checks and
# --no-checks override either mode.

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

file(MAKE_DIRECTORY "${WORK}")

# --checks puts them into a release build, and --no-checks takes them out
# of a dev build. Both override the mode.
set(bounds "${SOURCES}/bounds_array.anti")
antic_program("${WORK}/forced" "${bounds}" --checks)
program_expect("forced" COMMAND "${WORK}/forced" ABORTS
               ERR_MATCH "index out of bounds: index 5, length 4")
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
# The release program reads past the slice, so it is not run: what it
# returns is whatever lies there.
antic_program("${WORK}/quiet" "${WORK}/consumer.anti" -I "${WORK}/lib")
file(STRINGS "${WORK}/quiet" text)
if(text MATCHES "index out of bounds")
    message(FATAL_ERROR "a library check reached a release program")
endif()
antic_program("${WORK}/trap" "${WORK}/consumer.anti" --checks
              -I "${WORK}/lib")
program_expect("trap" COMMAND "${WORK}/trap" ABORTS
               ERR_MATCH "nth\\.anti:[0-9]+: index out of bounds: index 5, length 2")

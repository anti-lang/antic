# Every dev-mode check, in a build that has the checks and a build that
# does not. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime archive
#   SOURCES   the directory of the check programs
#   WORK      a directory for the files
#   OBJECT    the suffix of an object of the host, .o or .obj
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
# and not run. Further arguments are objects that the dev build links.
function(check name phrase pattern runs)
    set(source "${SOURCES}/${name}.anti")
    build("${name}.release" "${source}")
    file(STRINGS "${WORK}/${name}.release" text)
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
    build("${name}.dev" "${source}" --dev ${ARGN})
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
check(bounds_simd "index out of bounds"
      "bounds_simd\\.anti:[0-9]+: index out of bounds: index 7, length 6" ON)
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
check(overflow_carry "overflow in"
      "overflow_carry\\.anti:[0-9]+: overflow in \\+: left 9223372036854775806, right 1" ON)
check(narrow "out of range"
      "narrow\\.anti:[0-9]+: value out of range for i8: value 300" ON)
check(narrow_sign "out of range"
      "narrow_sign\\.anti:[0-9]+: value out of range for u64: value -1" ON)
check(narrow_char "out of range"
      "narrow_char\\.anti:[0-9]+: value out of range for char: value 1114112" ON)
check(narrow_surrogate "out of range"
      "narrow_surrogate\\.anti:[0-9]+: value out of range for char: value 55296" ON)
check(narrow_enum "not declared by"
      "narrow_enum\\.anti:[0-9]+: value not declared by Kind: value 7" ON)
check(divide "division by zero"
      "divide\\.anti:[0-9]+: division by zero in /: left 10" OFF)
check(remainder "division by zero"
      "remainder\\.anti:[0-9]+: division by zero in %: left 10" OFF)
check(shift_wide "shift count out of range"
      "shift_wide\\.anti:[0-9]+: shift count out of range for <<: count 64, width 64" ON)
check(shift_negative "shift count out of range"
      "shift_negative\\.anti:[0-9]+: shift count out of range for >>: count -1, width 64" ON)
# A walk of a collection that changes. The program imports anti.lang, and
# no tool builds the dev objects of the standard library yet, so the test
# builds that one.
execute_process(COMMAND "${ANTIC}" --dev --llvm-mc "${LLVM_MC}"
                        --runtime "${RUNTIME}" -o "${WORK}/anti_lang_dev"
                        "${RUNTIME}/std/anti/lang.antl"
                RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "antic --dev of anti.lang failed\n${err}")
endif()
check(walk_changed "was changed while"
      "walk_changed\\.anti:68: `bag` was changed while `for` walked it: changed at walk_changed\\.anti:70"
      ON "${WORK}/anti_lang_dev${OBJECT}")
# The same over the base of the generic collections, which counts the
# change in `remove_all`. anti.collection imports anti.mem and anti.text,
# so the dev build links an object of each.
foreach(module mem text collection)
    execute_process(COMMAND "${ANTIC}" --dev --llvm-mc "${LLVM_MC}"
                            --runtime "${RUNTIME}" -o "${WORK}/anti_${module}_dev"
                            "${RUNTIME}/std/anti/${module}.antl"
                    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "antic --dev of anti.${module} failed\n${err}")
    endif()
endforeach()
check(collection_changed "was changed while"
      "collection_changed\\.anti:82: `pair` was changed while `for` walked it: changed at collection_changed\\.anti:84"
      ON "${WORK}/anti_lang_dev${OBJECT}" "${WORK}/anti_mem_dev${OBJECT}"
      "${WORK}/anti_text_dev${OBJECT}" "${WORK}/anti_collection_dev${OBJECT}")
# The same over List<T>, which counts the change in `push`, and the index
# checks of List<T> and Grid<T>, which trap as an array does. Grid<T> checks
# each index against its own dimension, so a column past the width traps in
# a row that holds cells after it. The patterns leave the file of a trap in a
# generic open: a copy from a library file names the file of the program
# with the line of the library. The dev build links an object of each module
# the program reaches.
foreach(module list grid)
    execute_process(COMMAND "${ANTIC}" --dev --llvm-mc "${LLVM_MC}"
                            --runtime "${RUNTIME}"
                            -o "${WORK}/anti_collection_${module}_dev"
                            "${RUNTIME}/std/anti/collection/${module}.antl"
                    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR
            "antic --dev of anti.collection.${module} failed\n${err}")
    endif()
endforeach()
set(collection_objects "${WORK}/anti_lang_dev${OBJECT}"
    "${WORK}/anti_mem_dev${OBJECT}" "${WORK}/anti_text_dev${OBJECT}"
    "${WORK}/anti_collection_dev${OBJECT}")
check(list_changed "was changed while"
      "list_changed\\.anti:12: `people` was changed while `for` walked it: changed at list_changed\\.anti:14"
      ON ${collection_objects} "${WORK}/anti_collection_list_dev${OBJECT}")
check(list_bounds "index out of bounds"
      "\\.anti:[0-9]+: index out of bounds: index 5, length 4"
      ON ${collection_objects} "${WORK}/anti_collection_list_dev${OBJECT}")
check(grid_bounds "index out of bounds"
      "\\.anti:[0-9]+: index out of bounds: index 3, length 3"
      ON ${collection_objects} "${WORK}/anti_collection_grid_dev${OBJECT}")
# The same over a sorted map, whose module links an object of its own and
# one of anti.reflect, which it imports.
foreach(module reflect collection/sorted)
    string(REPLACE "/" "_" object "${module}")
    execute_process(COMMAND "${ANTIC}" --dev --llvm-mc "${LLVM_MC}"
                            --runtime "${RUNTIME}" -o "${WORK}/anti_${object}_dev"
                            "${RUNTIME}/std/anti/${module}.antl"
                    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "antic --dev of anti.${module} failed\n${err}")
    endif()
endforeach()
check(sorted_changed "was changed while"
      "sorted_changed\\.anti:13: `ages` was changed while `for` walked it: changed at sorted_changed\\.anti:15"
      ON "${WORK}/anti_lang_dev${OBJECT}" "${WORK}/anti_mem_dev${OBJECT}"
      "${WORK}/anti_text_dev${OBJECT}" "${WORK}/anti_collection_dev${OBJECT}"
      "${WORK}/anti_reflect_dev${OBJECT}" "${WORK}/anti_collection_sorted_dev${OBJECT}")

# The same over the maps and the sets of round five, whose modules stand
# under anti/collection/ and import anti.collection.
foreach(module map set)
    execute_process(COMMAND "${ANTIC}" --dev --llvm-mc "${LLVM_MC}"
                            --runtime "${RUNTIME}"
                            -o "${WORK}/anti_collection_${module}_dev"
                            "${RUNTIME}/std/anti/collection/${module}.antl"
                    RESULT_VARIABLE status ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "antic --dev of anti.collection.${module} failed\n${err}")
    endif()
endforeach()
check(map_changed "was changed while"
      "map_changed\\.anti:12: `ages` was changed while `for` walked it: changed at map_changed\\.anti:14"
      ON "${WORK}/anti_lang_dev${OBJECT}" "${WORK}/anti_mem_dev${OBJECT}"
      "${WORK}/anti_text_dev${OBJECT}" "${WORK}/anti_collection_dev${OBJECT}"
      "${WORK}/anti_collection_map_dev${OBJECT}")
check(set_changed "was changed while"
      "set_changed\\.anti:12: `seen` was changed while `for` walked it: changed at set_changed\\.anti:13"
      ON "${WORK}/anti_lang_dev${OBJECT}" "${WORK}/anti_mem_dev${OBJECT}"
      "${WORK}/anti_text_dev${OBJECT}" "${WORK}/anti_collection_dev${OBJECT}"
      "${WORK}/anti_collection_map_dev${OBJECT}" "${WORK}/anti_collection_set_dev${OBJECT}")

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
    file(STRINGS "${WORK}/${name}" text)
    if(name STREQUAL "quiet")
        if(text MATCHES "index out of bounds")
            message(FATAL_ERROR "a library check reached a release program")
        endif()
    elseif(NOT ran MATCHES "nth\\.anti:[0-9]+: index out of bounds: index 5, length 2")
        message(FATAL_ERROR "--checks did not keep the library check\n${ran}")
    endif()
endforeach()

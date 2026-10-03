# The data layout string of every target and the two CPU attributes of every
# level are the ones the pinned clang writes. src/antic/llvm_target.c holds
# them, `antic --print-llvm-targets` prints them, and this test runs the
# pinned clang once per target and per level of the target's architecture
# and compares. A mismatch after a toolchain upgrade fails here instead of
# miscompiling. Run with cmake -P and these values:
#   ANTIC   the antic executable
#   CLANG   the pinned clang
#   WORK    a scratch directory
#
# antic prints one line per target and one per level:
#   target <name> <architecture> <triple> <relocation model> <data layout>
#   level <name> <architecture> <clang -march= value> <target-cpu> <target-features>
#
# DESIGN: on ARM64 the clang command names the target-cpu with -mcpu and
# passes -mno-fmv. clang picks apple-m1 on macOS without -mcpu, which at
# armv8.0 would turn on features the level lacks. -mno-fmv makes clang
# write the same feature string for all three ARM64 triples, so one row per
# level holds for every target.

execute_process(COMMAND "${ANTIC}" --print-llvm-targets
    RESULT_VARIABLE status OUTPUT_VARIABLE printed ERROR_VARIABLE err
    ENCODING NONE)
if(NOT status EQUAL 0 OR NOT err STREQUAL "")
    message(FATAL_ERROR "antic --print-llvm-targets failed\n${err}")
endif()
file(MAKE_DIRECTORY "${WORK}")
set(empty "${WORK}/empty.c")
file(WRITE "${empty}" "void anti_probe(void) {}\n")

string(REPLACE "\n" ";" lines "${printed}")
set(targets "")
set(levels "")
foreach(line IN LISTS lines)
    if(line STREQUAL "")
        continue()
    endif()
    string(REPLACE " " ";" fields "${line}")
    list(LENGTH fields count)
    list(GET fields 0 kind)
    if(kind STREQUAL "target" AND count EQUAL 6)
        list(APPEND targets "${line}")
    elseif(kind STREQUAL "level" AND count EQUAL 6)
        list(APPEND levels "${line}")
    else()
        message(FATAL_ERROR "antic --print-llvm-targets wrote a line of "
                            "the wrong form: ${line}")
    endif()
endforeach()
list(LENGTH targets target_count)
list(LENGTH levels level_count)
if(NOT target_count EQUAL 6 OR NOT level_count EQUAL 6)
    message(FATAL_ERROR "antic --print-llvm-targets printed ${target_count} "
                        "targets and ${level_count} levels, not 6 and 6")
endif()

# The value of `name = "value"` on a line of clang's output.
function(quoted_value out text name)
    string(REGEX MATCH "\"${name}\"=\"[^\"]*\"" pair "${text}")
    string(REGEX REPLACE "^\"${name}\"=\"([^\"]*)\"$" "\\1" value "${pair}")
    set(${out} "${value}" PARENT_SCOPE)
endfunction()

set(checked 0)
foreach(target_line IN LISTS targets)
    string(REPLACE " " ";" fields "${target_line}")
    list(GET fields 1 target)
    list(GET fields 2 target_arch)
    list(GET fields 3 triple)
    list(GET fields 5 layout)
    foreach(level_line IN LISTS levels)
        string(REPLACE " " ";" fields "${level_line}")
        list(GET fields 1 level)
        list(GET fields 2 level_arch)
        list(GET fields 3 march)
        list(GET fields 4 cpu)
        list(GET fields 5 features)
        if(NOT level_arch STREQUAL target_arch)
            continue()
        endif()
        if(target_arch STREQUAL "arm64")
            set(flags "-mcpu=${cpu}" -mno-fmv "-march=${march}")
        else()
            set(flags "-march=${march}")
        endif()
        execute_process(
            COMMAND "${CLANG}" -target "${triple}" ${flags} -S -emit-llvm
                    -x c "${empty}" -o -
            RESULT_VARIABLE status OUTPUT_VARIABLE ir ERROR_VARIABLE err
            ENCODING NONE)
        if(NOT status EQUAL 0 OR NOT err STREQUAL "")
            message(FATAL_ERROR "clang failed for ${target} at ${level}\n${err}")
        endif()
        string(REGEX MATCH "target datalayout = \"[^\"]*\"" line "${ir}")
        string(REGEX REPLACE "^target datalayout = \"([^\"]*)\"$" "\\1"
               clang_layout "${line}")
        if(NOT clang_layout STREQUAL layout)
            message(FATAL_ERROR "the data layout of ${target} differs from "
                                "clang\nantic: ${layout}\nclang: ${clang_layout}")
        endif()
        quoted_value(clang_cpu "${ir}" "target-cpu")
        if(NOT clang_cpu STREQUAL cpu)
            message(FATAL_ERROR "the target-cpu of ${level} on ${target} "
                                "differs from clang\nantic: ${cpu}\n"
                                "clang: ${clang_cpu}")
        endif()
        quoted_value(clang_features "${ir}" "target-features")
        if(NOT clang_features STREQUAL features)
            message(FATAL_ERROR "the target-features of ${level} on ${target} "
                                "differ from clang\nantic: ${features}\n"
                                "clang: ${clang_features}")
        endif()
        math(EXPR checked "${checked} + 1")
    endforeach()
endforeach()
if(NOT checked EQUAL 18)
    message(FATAL_ERROR "compared ${checked} pairs of target and level, not 18")
endif()

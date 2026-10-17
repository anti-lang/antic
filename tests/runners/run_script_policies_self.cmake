# tests/runners/run_script_policies.cmake refuses a script of cmake -P without
# cmake_minimum_required, and one that names another version than the
# root, wherever the call stands. Run with cmake -P and these values:
#   RUNNER  tests/runners/run_script_policies.cmake
#   WORK    a directory for the tree of the case

cmake_minimum_required(VERSION 3.21)

file(REMOVE_RECURSE "${WORK}")
set(right "cmake_minimum_required(VERSION 3.21)\n")
file(WRITE "${WORK}/CMakeLists.txt" "${right}"
     "add_test(NAME a COMMAND cmake -P \"\${DIR}/tests/runners/run_right.cmake\")\n"
     "add_test(NAME b COMMAND cmake -P \"\${DIR}/tests/runners/run_bare.cmake\")\n")
file(WRITE "${WORK}/src/native/recipe.cmake"
     "add_test(NAME c COMMAND cmake -P \"\${DIR}/tests/runners/run_older.cmake\")\n")
file(WRITE "${WORK}/README.md" "cmake -P tools/fetch.cmake\n")
file(WRITE "${WORK}/tools/fetch.cmake" "${right}")
file(WRITE "${WORK}/tools/release.sh"
     "cmake -P \"$root/tools/pack.cmake\"\n")
file(WRITE "${WORK}/tests/runners/run_right.cmake" "${right}")
file(WRITE "${WORK}/tests/runners/run_bare.cmake" "message(\"no line\")\n")
file(WRITE "${WORK}/tests/runners/run_older.cmake"
     "cmake_minimum_required(VERSION 3.20)\n")
file(WRITE "${WORK}/tools/pack.cmake" "message(\"no line\")\n")
file(WRITE "${WORK}/tests/helper.cmake" "message(\"no line\")\n")

execute_process(COMMAND "${CMAKE_COMMAND}" "-DROOT=${WORK}" -DMIN=1
                        -P "${RUNNER}"
                RESULT_VARIABLE status OUTPUT_VARIABLE out
                ERROR_VARIABLE err ENCODING NONE)
# CMake wraps a long message, so the text is read with its spaces and its
# line ends made one space.
set(said "${out}${err}")
string(REGEX REPLACE "[ \t\r\n]+" " " flat "${out}${err}")
if(status EQUAL 0)
    message(FATAL_ERROR "run_script_policies.cmake took scripts without the "
                        "line of the root\n${said}")
endif()
set(failures "")
foreach(shown "tests/runners/run_bare.cmake: no cmake_minimum_required"
        "tests/runners/run_older.cmake: cmake_minimum_required(VERSION 3.20)"
        "tools/pack.cmake: no cmake_minimum_required")
    string(FIND "${flat}" "${shown}" at)
    if(at EQUAL -1)
        string(APPEND failures "names no `${shown}`\n")
    endif()
endforeach()
foreach(name run_right fetch helper)
    string(FIND "${flat}" "${name}.cmake" at)
    if(NOT at EQUAL -1)
        string(APPEND failures "names ${name}.cmake\n")
    endif()
endforeach()
if(NOT failures STREQUAL "")
    message(FATAL_ERROR "run_script_policies.cmake:\n${failures}${said}")
endif()
file(REMOVE_RECURSE "${WORK}")

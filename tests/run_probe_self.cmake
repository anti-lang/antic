# tests/run_probe.cmake fails for two probes that print the same and end
# with a status other than 0, as two that stop on the same fault do. Run
# with cmake -P and the values of run_probe.cmake apart from SOURCES and
# NAME, and RUNNER, tests/run_probe.cmake.

cmake_minimum_required(VERSION 3.21)

file(REMOVE_RECURSE "${WORK}")
file(WRITE "${WORK}/sources/stops.c"
     "#include <stdio.h>\n"
     "int main(void) { printf(\"layout\\n\"); return 3; }\n")
file(WRITE "${WORK}/sources/stops.anti"
     "extern fn printf(format: ?*byte, ...) -> c_int;\n"
     "\n"
     "fn main() -> int\n"
     "{\n"
     "\tprintf(\"layout\\n\".ptr);\n"
     "\treturn 3;\n"
     "}\n")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DNAME=stops"
                        "-DANTIC=${ANTIC}" "-DLLVM_MC=${LLVM_MC}"
                        "-DRUNTIME=${RUNTIME}" "-DSOURCES=${WORK}/sources"
                        "-DWORK=${WORK}/run" "-DCC=${CC}"
                        "-DHOST_LINK=${HOST_LINK}" -P "${RUNNER}"
                RESULT_VARIABLE status OUTPUT_VARIABLE out
                ERROR_VARIABLE err ENCODING NONE)
if(status EQUAL 0)
    message(FATAL_ERROR "run_probe.cmake took two probes that end with 3")
endif()
if(NOT "${out}${err}" MATCHES "ended with 3 in C and 3 in Anti")
    message(FATAL_ERROR "run_probe.cmake failed for another reason\n"
                        "${out}${err}")
endif()
file(REMOVE_RECURSE "${WORK}")

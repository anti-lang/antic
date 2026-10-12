# Build a host and a plugin whose interface takes and gives every kind of
# value, and run the host. The library is compiled with --lib shared
# --no-runtime and is bound against the host that loads it. A Windows
# library links against the import library of its host. Run with cmake -P
# and these values:
#   ANTIC     the antic executable
#   LLVM_MC   the llvm-mc executable
#   RUNTIME   the runtime directory
#   SOURCES   tests/plugin
#   SUFFIX    the suffix of a shared library on this host
#   EXE       the suffix of an executable on this host
#   WORK      a directory for the objects and the executables
#   HOST      the name of the host's source in SOURCES, without .anti
#   LIBRARY   the module of the plugin under net/example
#   INTERFACE the module of the interface under net/example
#   EXPECTED  the output of the host

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

# Run antic, which must succeed and print nothing.
function(run)
    execute_process(COMMAND "${ANTIC}" ${ARGV} RESULT_VARIABLE status
                    OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0 OR NOT out STREQUAL "" OR NOT err STREQUAL "")
        message(FATAL_ERROR "antic failed with ${status}: ${ARGV}\n${out}${err}")
    endif()
endfunction()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/net/example" "${WORK}/lib")

# The interface is a library file that both sides import.
run(-c -I "${SOURCES}" --runtime "${RUNTIME}" -o "${WORK}/net/example/${INTERFACE}.antl"
    "${SOURCES}/net/example/${INTERFACE}.anti")
run(--runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}" -I "${SOURCES}"
    -I "${WORK}" -o "${WORK}/${HOST}${EXE}" "${SOURCES}/${HOST}.anti")
set(import_library "")
if(SUFFIX STREQUAL ".dll")
    set(import_library "${WORK}/${HOST}.lib")
endif()
set(library "${WORK}/lib/lib${LIBRARY}${SUFFIX}")
run(--lib shared --no-runtime --runtime "${RUNTIME}" --llvm-mc "${LLVM_MC}"
    -I "${SOURCES}" -I "${WORK}" -o "${library}"
    "${SOURCES}/net/example/${LIBRARY}.anti" ${import_library})
program_expect("the host" COMMAND "${WORK}/${HOST}${EXE}" "${library}"
               OUT_FILE "${EXPECTED}")

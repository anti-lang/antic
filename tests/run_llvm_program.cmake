# Build one test program through the LLVM back end by hand and compare its
# result with the expected file beside the source, as run_program.cmake
# does for the native back end. The steps follow "Integration route" of
# docs/work-order-llvm-back-end.md: antic prints the text with
# --dump-llvm, opt runs default<O2> on it in release mode, llc writes the
# object, at -O2 in release mode and at -O1 on the text in dev mode, and
# ld64.lld of the runtime archive links it. The step emit-run moves these
# steps into antic. Run with cmake -P and these values:
#   ANTIC     the antic executable
#   OPT       opt of the pinned release
#   LLC       llc of the pinned release
#   RUNTIME   the runtime directory, holding bin/, lib/<target>/<level>/
#             and sysroot/<target>/
#   SOURCE    the .anti file
#   TARGET_NAME  macos-arm64 or macos-x86_64, the targets a Mac runs
#   LEVEL     the processor level, which names the runtime's directory
#   MODE      release or dev
#   WORK      a directory for the text, the objects and the executable
#   OPTIONS   optional options of antic, separated by commas
#   EXPECTED  optional expected file, instead of NAME.expected beside the
#             source
#   OBJECTS   optional objects of C that the program links, separated by
#             commas
#
# DESIGN: the runtime reads anti_licenses, the notice that antic appends
# after the build id. The build id digests the runtime library, so the
# notice stays out of --dump-llvm, whose goldens would follow every change
# of the runtime. Until the step emit-run writes the notice, the link takes
# an empty one from a text of one line. No scalar program reads it.

include("${CMAKE_CURRENT_LIST_DIR}/program_output.cmake")

if(TARGET_NAME STREQUAL "macos-arm64")
    set(arch arm64)
elseif(TARGET_NAME STREQUAL "macos-x86_64")
    set(arch x86_64)
else()
    message(FATAL_ERROR "the link by hand covers the macOS targets, not ${TARGET_NAME}")
endif()

get_filename_component(name "${SOURCE}" NAME_WE)
get_filename_component(dir "${SOURCE}" DIRECTORY)
set(expected_file "${dir}/${name}.expected")
if(DEFINED EXPECTED)
    set(expected_file "${EXPECTED}")
endif()
set(base "${WORK}/${name}.${MODE}.${TARGET_NAME}")
file(MAKE_DIRECTORY "${WORK}")

string(REPLACE "," ";" options "${OPTIONS}")
set(mode_options "")
if(MODE STREQUAL "dev")
    set(mode_options --dev)
endif()
execute_process(
    COMMAND "${ANTIC}" --runtime "${RUNTIME}" --target "${TARGET_NAME}"
            ${mode_options} ${options} --dump-llvm "${SOURCE}"
    RESULT_VARIABLE status
    OUTPUT_FILE "${base}.ll"
    ERROR_VARIABLE err
    ENCODING NONE)
if(NOT status EQUAL 0 OR NOT err STREQUAL "")
    message(FATAL_ERROR "antic --dump-llvm gave ${status}\n${err}")
endif()

# Run a tool of the pinned release, which must succeed and print nothing.
function(run_tool label)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE status
                    OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0 OR NOT out STREQUAL "" OR NOT err STREQUAL "")
        message(FATAL_ERROR "${label} gave ${status} for ${base}.ll\n${out}${err}")
    endif()
endfunction()

if(MODE STREQUAL "dev")
    run_tool(llc "${LLC}" -O1 -filetype=obj -relocation-model=pic
             -o "${base}.o" "${base}.ll")
else()
    run_tool(opt "${OPT}" "-passes=default<O2>" -o "${base}.bc" "${base}.ll")
    run_tool(llc "${LLC}" -O2 -filetype=obj -relocation-model=pic
             -o "${base}.o" "${base}.bc")
endif()

file(STRINGS "${base}.ll" triple REGEX "^target triple = " LIMIT_COUNT 1)
if(NOT triple MATCHES "macos([0-9]+\\.[0-9]+)")
    message(FATAL_ERROR "no macOS version in ${triple}")
endif()
set(macos_version "${CMAKE_MATCH_1}")
file(WRITE "${base}.licenses.ll"
     "${triple}\n@anti_licenses = constant [1 x i8] zeroinitializer, align 1\n")
run_tool(llc "${LLC}" -filetype=obj -relocation-model=pic
         -o "${base}.licenses.o" "${base}.licenses.ll")

set(sysroot "${RUNTIME}/sysroot/${TARGET_NAME}")
file(STRINGS "${sysroot}/sdk-version" sdk_version LIMIT_COUNT 1)
string(REPLACE "," ";" objects "${OBJECTS}")
run_tool(ld64.lld "${RUNTIME}/bin/ld64.lld" -S -arch ${arch}
         -platform_version macos "${macos_version}" "${sdk_version}" -syslibroot "${sysroot}"
         -o "${base}" "${base}.o" "${base}.licenses.o" ${objects}
         "${RUNTIME}/lib/${TARGET_NAME}/${LEVEL}/libanti_rt.a" -lSystem)

set(program_args "")
if(EXISTS "${dir}/${name}.args")
    file(STRINGS "${dir}/${name}.args" program_args ENCODING UTF-8)
endif()
program_expect("${name}" COMMAND "${base}" ${program_args}
               EXPECTED "${expected_file}" ANY_ERR)

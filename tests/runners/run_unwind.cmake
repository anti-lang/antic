# Decode the Windows unwind data of a program and compare it byte for byte
# with an expected file. Run with cmake -P and these values:
#   ANTIC         the antic executable
#   RUNTIME       the runtime archive, whose bin/ holds opt and llc
#   LLVM_MC       the llvm-mc executable
#   LLVM_READOBJ  the llvm-readobj executable
#   TARGET        windows-x86_64 or windows-arm64
#   TRIPLE        the llvm-mc triple of the target
#   SOURCE        the .anti file
#   WORK          a directory for the assembly and the object
#   EXPECTED      the decoded unwind data without the line that names the file

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/../program_output.cmake")

file(MAKE_DIRECTORY "${WORK}")
get_filename_component(base "${SOURCE}" NAME_WE)
set(assembly "${WORK}/${base}.${TARGET}.s")
set(object "${WORK}/${base}.${TARGET}.obj")
antic_build(--runtime "${RUNTIME}" -S --target "${TARGET}" -o "${assembly}"
            "${SOURCE}")
tool_run("" QUIET "${LLVM_MC}" "-triple=${TRIPLE}" -filetype=obj -o "${object}" "${assembly}")
tool_run("" QUIET "${LLVM_READOBJ}" --unwind "${object}")
string(REGEX REPLACE "^\nFile: [^\n]*\n" "" decoded "${tool_stdout}")
file(READ "${EXPECTED}" expected)
expect_same_text("the unwind data for ${EXPECTED}" "${expected}" "${decoded}")

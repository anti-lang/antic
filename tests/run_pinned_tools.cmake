# Every tool a build of this tree runs is one of the pinned release, and
# the configure step refuses a tree that would run a tool of the host. Run
# with cmake -P and these values:
#   CHECK     tools/pinned-tools.cmake
#   CACHE     CMakeCache.txt of this tree
#   CLANG_DIR the pinned clang
#   LLVM_DIR  the pinned LLVM tools
#   OS        the operating system of the host: macos, linux or windows
#   PROGRAMS  antic and anti of this tree, separated by commas
#
# The cache is checked as the configure step checks it, and so is a copy
# of it with a tool of the host in each place. Then the linker's own mark
# in each program: `Linker: LLD` in the .comment of an ELF file, the tool
# lld in the build version of a Mach-O file, and no Rich header, which
# link.exe writes and lld-link does not, in a PE file.

include("${CHECK}")

set(names CMAKE_C_COMPILER CMAKE_AR CMAKE_RANLIB CMAKE_LINKER ANTIC_LLVM_AR
    ANTIC_LLVM_MC CMAKE_EXE_LINKER_FLAGS CMAKE_SHARED_LINKER_FLAGS
    CMAKE_MODULE_LINKER_FLAGS)
foreach(name IN LISTS names)
    file(STRINGS "${CACHE}" line REGEX "^${name}:[A-Z]+=")
    # REGEX REPLACE would strip each `=` in turn, so the value is matched.
    string(REGEX MATCH "^[^=]*=(.*)$" line "${line}")
    set(${name} "${CMAKE_MATCH_1}")
endforeach()

antic_pinned_tool_problems(problems "${CLANG_DIR}" "${LLVM_DIR}" "${OS}")
if(problems)
    list(JOIN problems "\n" text)
    message(FATAL_ERROR "the tree runs a tool of the host:\n${text}")
endif()

# A copy of the values with one tool of the host in place of a pinned one
# is refused, and the problem names that variable.
function(expect_refused name value)
    set(${name} "${value}")
    antic_pinned_tool_problems(problems "${CLANG_DIR}" "${LLVM_DIR}" "${OS}")
    string(FIND "${problems}" "${name}" at)
    if(at EQUAL -1)
        message(FATAL_ERROR "${name} = ${value} was not refused: ${problems}")
    endif()
endfunction()
expect_refused(CMAKE_C_COMPILER "/usr/bin/cc")
expect_refused(CMAKE_AR "/usr/bin/ar")
expect_refused(CMAKE_RANLIB "/usr/bin/ranlib")
expect_refused(CMAKE_LINKER "/usr/bin/ld")
expect_refused(ANTIC_LLVM_AR "/usr/bin/llvm-ar")
expect_refused(CMAKE_EXE_LINKER_FLAGS "")
expect_refused(CMAKE_SHARED_LINKER_FLAGS "-fuse-ld=bfd")

string(REPLACE "," ";" programs "${PROGRAMS}")
foreach(program IN LISTS programs)
    file(READ "${program}" head LIMIT 4 HEX)
    if(head STREQUAL "7f454c46")
        execute_process(COMMAND "${LLVM_DIR}/bin/llvm-readobj" -p .comment
                                "${program}"
                        OUTPUT_VARIABLE out RESULT_VARIABLE status
                        ENCODING NONE)
        if(NOT status EQUAL 0 OR NOT out MATCHES "Linker: LLD")
            message(FATAL_ERROR "${program} was not linked by lld:\n${out}")
        endif()
    elseif(head MATCHES "^(cffaedfe|cefaedfe)$")
        execute_process(COMMAND "${LLVM_DIR}/bin/llvm-objdump" --macho
                                --private-headers "${program}"
                        OUTPUT_VARIABLE out RESULT_VARIABLE status
                        ENCODING NONE)
        if(NOT status EQUAL 0 OR NOT out MATCHES "tool lld")
            message(FATAL_ERROR "${program} was not linked by lld")
        endif()
    elseif(head MATCHES "^4d5a")
        # The Rich header stands between the DOS stub and the PE header,
        # inside the first kilobyte, and ends in the text `Rich` and its
        # key.
        file(READ "${program}" stub LIMIT 1024 HEX)
        string(FIND "${stub}" "52696368" at)
        if(NOT at EQUAL -1)
            message(FATAL_ERROR "${program} carries the Rich header of "
                                "link.exe")
        endif()
    else()
        message(FATAL_ERROR "${program} is no program this test reads")
    endif()
endforeach()

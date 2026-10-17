# tests/runners/run_emit_identity.cmake keeps the texts of the manifest it
# last wrote or passed, and names the first line that moved in each text
# that differs from them. Run with cmake -P and these values:
#   ANTIC    the antic executable
#   RUNTIME  the runtime directory
#   VERSION  the version of antic
#   RUNNER   tests/runners/run_emit_identity.cmake
#   ROOT     the repository
#   WORK     a directory for the files
#
# The runner writes a manifest on the Mac alone, so the test runs there.

cmake_minimum_required(VERSION 3.21)

include("${ROOT}/tests/program_output.cmake")
file(REMOVE_RECURSE "${WORK}")
set(programs "${WORK}/programs")
set(manifest "${WORK}/programs.sha256")
set(pinned "${WORK}/pinned")

# Run the runner, with WRITE=yes when <write> is yes, and set <status> and
# <text> to what it gave.
function(identity status text write)
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DANTIC=${ANTIC}"
                            "-DRUNTIME=${RUNTIME}" "-DPROGRAMS=${programs}"
                            "-DMANIFEST=${manifest}" "-DVERSION=${VERSION}"
                            "-DWORK=${WORK}/texts" "-DPINNED=${pinned}"
                            "-DWRITE=${write}" -P "${RUNNER}"
                    RESULT_VARIABLE code OUTPUT_VARIABLE out
                    ERROR_VARIABLE err ENCODING NONE)
    set(${status} "${code}" PARENT_SCOPE)
    set(${text} "${out}${err}" PARENT_SCOPE)
endfunction()

function(program value)
    write_bytes("${programs}/answer.anti"
                "fn main() -> int\n{\n\treturn ${value};\n}\n")
endfunction()

# The texts of a written manifest stay beside it.
program(41)
identity(status text yes)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "the runner wrote no manifest\n${text}")
endif()
if(NOT EXISTS "${pinned}/answer.macos-arm64.ll")
    message(FATAL_ERROR "the runner kept no text of the manifest it wrote")
endif()

# A text that moved names its first line that differs.
program(42)
identity(status text no)
if(status EQUAL 0)
    message(FATAL_ERROR "the runner took a text that moved")
endif()
foreach(pattern "answer\\.macos-arm64\\.ll differs at line [0-9]+"
                "expected: [^\n]*41" "got: [^\n]*42")
    if(NOT text MATCHES "${pattern}")
        message(FATAL_ERROR "the failure holds no `${pattern}`\n${text}")
    endif()
endforeach()

# Writing the manifest again shows the same, and keeps the new texts.
identity(status text yes)
if(NOT status EQUAL 0 OR NOT text MATCHES "expected: [^\n]*41" OR
   NOT text MATCHES "got: [^\n]*42")
    message(FATAL_ERROR "the manifest was written again without the "
                        "difference\n${text}")
endif()
file(READ "${pinned}/answer.linux-x86_64.ll" kept)
if(NOT kept MATCHES "42")
    message(FATAL_ERROR "the runner kept the texts of the old manifest")
endif()

# A run that passes keeps the texts of the manifest where none stand, as
# in a new build tree.
file(REMOVE_RECURSE "${pinned}")
identity(status text no)
if(NOT status EQUAL 0 OR NOT EXISTS "${pinned}/answer.windows-arm64.ll")
    message(FATAL_ERROR "a run that passed kept no texts\n${text}")
endif()
file(REMOVE_RECURSE "${WORK}")

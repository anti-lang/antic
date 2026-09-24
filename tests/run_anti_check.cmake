# Drive `anti check` over the fixture projects of tests/check and read what
# it reports. Run with cmake -P and these values:
#   ANTI      the anti executable
#   RUNTIME   the runtime archive
#   PROJECTS  the directory that holds the projects
#   STD       the standard library of this checkout
#   WORK      a directory for the interface files and the doc-block modules
#
# The run covers each class of the check command: the front end on every
# source of a project, the `anti` blocks of the doc comments in their two
# contexts, the doc warnings, the formatting rules and the line that says
# the pattern check waits for PCRE2. It then shows that the first failing
# class ends the run, and it reads the whole standard library.

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

function(run project arguments out_status out_text)
    execute_process(
        COMMAND "${ANTI}" check ${arguments} --work "${WORK}/${project}"
                --runtime "${RUNTIME}"
        WORKING_DIRECTORY "${PROJECTS}/${project}"
        RESULT_VARIABLE status
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
        ENCODING NONE)
    set(${out_status} "${status}" PARENT_SCOPE)
    set(${out_text} "${out}${err}" PARENT_SCOPE)
endfunction()

function(expect text needle what)
    string(FIND "${text}" "${needle}" at)
    if(at LESS 0)
        message(FATAL_ERROR "${what}: no `${needle}` in\n${text}")
    endif()
endfunction()

function(refuse text needle what)
    string(FIND "${text}" "${needle}" at)
    if(NOT at LESS 0)
        message(FATAL_ERROR "${what}: `${needle}` is in\n${text}")
    endif()
endfunction()

# A project every class accepts. The check reads the source and the test
# directory of the manifest, compiles the two doc blocks and says that the
# pattern check waits for PCRE2.
run(clean "" status text)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "the clean project failed with ${status}\n${text}")
endif()
expect("${text}" "front end: 3 files, 1 target, 0 warnings" "the front end")
expect("${text}" "doc blocks: 2 blocks" "the doc blocks")
expect("${text}" "doc warnings: 0" "the doc warnings")
expect("${text}" "formatting: 3 files" "the formatting")
expect("${text}" "patterns: skipped, the check of a `regex.compile` pattern waits for PCRE2" "the pattern line")

# --targets all runs the front end once per target.
run(clean "--targets;all" status text)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "--targets all failed with ${status}\n${text}")
endif()
expect("${text}" "front end: 3 files, 6 targets, 0 warnings" "the six targets")

# A project whose checker warns. The check passes --warnings-as-errors, as a
# release build refuses every warning. The front end fails with each warning
# named, and no class below it runs.
run(warnings "" status text)
if(status EQUAL 0)
    message(FATAL_ERROR "the warnings project passed\n${text}")
endif()
expect("${text}" "error: `close_it` may fail and never does [never-fails]" "may fail")
expect("${text}" "error: `e` shadows the outer `e` [shadowed-catch]" "the shadowing")
expect("${text}" "front end: 1 file, 1 target, 2 warnings, 1 failed" "the two warnings")
refuse("${text}" "doc warnings:" "the class below")

# The same project with both warnings allowed. The doc warnings are counted
# apart and the run ends with success, since the doc class fails nothing.
run(allowed "" status text)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "the allowed project failed with ${status}\n${text}")
endif()
refuse("${text}" "may fail and never does" "the allowed warning")
refuse("${text}" "shadows the outer" "the allowed warning")
expect("${text}" "warning: `nowhere` in the doc comment of `handler` resolves to nothing [doc-unresolved]" "the name")
expect("${text}" "holds a heading, which the doc markup has not [doc-markup]" "the markup")
expect("${text}" "front end: 1 file, 1 target, 0 warnings\n" "no warning")
expect("${text}" "doc warnings: 2" "the two doc warnings")
refuse("${text}" "has no `///` comment" "the undocumented item")

# --warn-undocumented adds the `pub` item without a `///` comment.
run(allowed "--warn-undocumented" status text)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "--warn-undocumented failed with ${status}\n${text}")
endif()
expect("${text}" "the pub item `undocumented` has no `///` comment [undocumented]" "the item")
expect("${text}" "doc warnings: 3" "the three doc warnings")

# The front end refuses one module, so no class below it runs.
run(front-end "" status text)
if(status EQUAL 0)
    message(FATAL_ERROR "the front-end project passed\n${text}")
endif()
expect("${text}" "expected `int`, found `str`" "the error of the checker")
expect("${text}" "front end: 1 file, 1 target, 0 warnings, 1 failed" "the class")
refuse("${text}" "doc blocks:" "the class below")
refuse("${text}" "formatting:" "the class below")
refuse("${text}" "patterns:" "the class below")

# A block of a user doc comment sees the public items alone, so one that
# names a private function fails. The report names the doc comment it
# stands in.
run(block "" status text)
if(status EQUAL 0)
    message(FATAL_ERROR "the block project passed\n${text}")
endif()
expect("${text}" "`hidden` has no public item `helper`" "the private item")
expect("${text}" "the `anti` block of `twice` at src/com/example/hidden.anti:3 failed" "the origin")
expect("${text}" "doc blocks: 1 block, 1 failed" "the class")
refuse("${text}" "formatting:" "the class below")

# The formatting class writes the canonical text and compares the bytes,
# which is the test `anti fmt --check` runs. It names the first line that
# differs.
run(format "" status text)
if(status EQUAL 0)
    message(FATAL_ERROR "the format project passed\n${text}")
endif()
expect("${text}" "loose.anti:5:1: error: the line is not the one `anti fmt` writes" "the line")
expect("${text}" "formatting: 1 file, 1 finding" "the class")
refuse("${text}" "patterns:" "the class below")

# The standard library of this checkout, read as one package. Every class
# passes, and the doc warnings are reported without failing.
file(MAKE_DIRECTORY "${WORK}/std")
file(WRITE "${WORK}/std/anti.toml"
     "[package]\nname = \"anti\"\nversion = \"0.1.0\"\n\n[layout]\nsrc = \"${STD}\"\ntest = \"no-test-directory\"\n")
execute_process(
    COMMAND "${ANTI}" check --targets all --work "${WORK}/std/work"
            --runtime "${RUNTIME}"
    WORKING_DIRECTORY "${WORK}/std"
    RESULT_VARIABLE status
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
    ENCODING NONE)
set(text "${out}${err}")
if(NOT status EQUAL 0)
    message(FATAL_ERROR "the standard library failed with ${status}\n${text}")
endif()
expect("${text}" "front end: 22 files, 6 targets" "the front end of std")
expect("${text}" "formatting: 22 files\n" "the formatting of std")

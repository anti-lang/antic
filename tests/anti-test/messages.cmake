# The messages of `anti test`, one case each.
# tests/runners/run_anti_messages.cmake states the form of a case.

anti_message(no_source STATUS 2
    ARGS test --work "${WORK}/test" --runtime "${RUNTIME}"
    ERR "anti: test takes one or more .anti files")

# Fault injection, through the variable ANTI_FAULT of src/rt/fault.h. T153:
# the work directory, which anti test leaves without a message. The work
# directory is relative, so the count of its directories is the same on
# every machine.
set(twice_text "//! Doubling.\n\n/// Twice n.\npub fn twice(n: int) -> int\n{\n\treturn n * 2;\n}\n\ntests\n{\n\tfn doubles()\n\t{\n\t\tlet n = twice(2);\n\t\tassert(n == 4);\n\t}\n}\n")
anti_message(fault_work_not_made
    FILES com/example/twice.anti "${twice_text}"
    FAULT mkdir:2:EACCES
    ARGS test --work ../test --runtime "${RUNTIME}" com/example/twice.anti
    ERR "anti: cannot make ../test")
# Without --work the files of the run stand in the build directory of
# `[layout]`, by Q7 of "Answers of 2026-10-24" in docs/decisions.md. A
# file stands where that directory belongs.
anti_message(work_in_the_layout
    FILES anti.toml "[package]\nname = \"com.example\"\nversion = \"0.1.0\"\n\n[layout]\nbuild = \"out\"\n"
          com/example/twice.anti "${twice_text}"
          out "A file where the build directory belongs.\n"
    ARGS test --runtime "${RUNTIME}" com/example/twice.anti
    ERR "anti: out is a file where a directory belongs")
# The module has no root, so its path is one segment and the compile
# warns of it before every message that follows.
set(one_segment "com/example/twice.anti:1:1: warning: the module path `twice` has one segment, which is for a program's own files [single-segment-path]")
# T153: the directory of the library file of the module, unit_file of
# units.c, which read_unit of test.c leaves without a message. The
# fault makes the mkdir of the work directory fail although it stands.
anti_message(fault_library_directory_not_made
    FILES com/example/twice.anti "${twice_text}"
    FAULT mkdir:4:EACCES
    ARGS test --work ../test --runtime "${RUNTIME}" com/example/twice.anti
    ERR "anti: cannot make ../test")
# T153: the directory of the runner, run_unit of test.c.
anti_message(fault_runner_directory_not_made
    FILES com/example/twice.anti "${twice_text}"
    FAULT mkdir:7:EACCES
    OUT "anti test: 1 tests in 1 modules, 1 module failed"
    ARGS test --work ../test --runtime "${RUNTIME}" com/example/twice.anti
    ERR "${one_segment}"
        "anti: cannot make ../test/anti/test")
# T153: the directory of the library files the runner imports,
# compile_imports of test.c.
anti_message(fault_imports_directory_not_made
    FILES com/example/twice.anti "${twice_text}"
    FAULT mkdir:13:EACCES
    OUT "anti test: 1 tests in 1 modules, 1 module failed"
    ARGS test --work ../test --runtime "${RUNTIME}" com/example/twice.anti
    ERR "${one_segment}"
        "anti: cannot make ../test/anti/test/imports/twice")
# The source of the runner, run_unit of test.c.
anti_message(fault_runner_not_written
    FILES com/example/twice.anti "${twice_text}"
    FAULT write:3:ENOSPC
    OUT "anti test: 1 tests in 1 modules, 1 module failed"
    ARGS test --work ../test --runtime "${RUNTIME}" com/example/twice.anti
    ERR "${one_segment}"
        "anti: cannot write ../test/anti/test/twice.anti")
# The library file of the module, read for the libraries it imports,
# compile_imports of test.c, and for the frameworks it links,
# runner_frameworks of test.c.
anti_message(fault_imports_unread
    FILES com/example/twice.anti "${twice_text}"
    FAULT read:9:EIO
    OUT "anti test: 1 tests in 1 modules, 1 module failed"
    ARGS test --work ../test --runtime "${RUNTIME}" com/example/twice.anti
    ERR "${one_segment}"
        "antic: cannot read ../test/twice.antl")
anti_message(fault_frameworks_unread
    FILES com/example/twice.anti "${twice_text}"
    FAULT read:11:EIO
    OUT "anti test: 1 tests in 1 modules, 1 module failed"
    ARGS test --work ../test --runtime "${RUNTIME}" com/example/twice.anti
    ERR "${one_segment}"
        "antic: cannot read ../test/twice.antl")
# The compile of a library the runner imports, compile_imports of test.c,
# whose llc is the second program the run starts, after that of the
# object of the module.
set(text_text "//! Doubling.\n\nimport anti.text;\n\n/// Twice n.\npub fn twice(n: int) -> int\n{\n\treturn n * 2;\n}\n\ntests\n{\n\tfn doubles()\n\t{\n\t\tlet same = text.equal(\"a\", \"a\");\n\t\tassert(same);\n\t}\n}\n")
anti_message(fault_import_not_compiled
    FILES com/example/twice.anti "${text_text}"
    FAULT run:3:EACCES
    OUT "anti test: 1 tests in 1 modules, 1 module failed"
    ARGS test --work ../test --runtime "${RUNTIME}" com/example/twice.anti
    ERR "${one_segment}"
        "anti: cannot run ${RUNTIME}/bin/llc${EXE}: Permission denied"
        "antic: llc failed")
# The compile of the object of the module in a dev run, compile_object of
# test.c, whose llc is the first program the run starts. The run stops
# before it counts the tests.
anti_message(fault_object_not_compiled
    FILES com/example/twice.anti "${twice_text}"
    FAULT run:1:EACCES
    ARGS test --work ../test --runtime "${RUNTIME}" com/example/twice.anti
    ERR "${one_segment}"
        "anti: cannot run ${RUNTIME}/bin/llc${EXE}: Permission denied"
        "antic: llc failed")

# The manifest, read for its layout and its injections.
anti_message(manifest_no_toml
    FILES anti.toml "name = \n" com/example/twice.anti "${twice_text}"
    ARGS test --work ../test --runtime "${RUNTIME}" com/example/twice.anti
    ERR "anti: anti.toml is no TOML that anti reads")
# Without --runtime the run takes the runtime archive of the user's data
# directory, whose lib/ tells it from any other directory. The module
# does not compile, so the run needs nothing else of the archive.
anti_message(runtime_of_the_data_directory
    FILES src/com/example/one.anti "//! Wrong.\n\n/// One.\npub fn one() -> int\n{\n\treturn true;\n}\n"
    DIRECTORIES data/anti/lib
    ENV "XDG_DATA_HOME=${PROJECT}/data" "LOCALAPPDATA=${PROJECT}/data"
    ARGS test --work ../test -I src src/com/example/one.anti
    ERR "src/com/example/one.anti:6:9: error: expected `int`, found `bool`")

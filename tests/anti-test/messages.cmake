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
# A module the parser refuses, read_unit of test.c, fails the run before
# it compiles anything.
anti_message(source_not_parsed
    FILES com/example/bad.anti "//! Broken.\n\nfn (\n"
    ARGS test --work ../test --runtime "${RUNTIME}" com/example/bad.anti
    ERR "com/example/bad.anti:3:4: error: expected identifier")

# A project of two modules, whose tests block imports the other, for the
# cases of a run over a project below. The run reuses the files of the
# build of the project in build/<host>/dev/ and caches its own there.
function(write_project)
    write_bytes("${PROJECT}/anti.toml"
                "[package]\nname = \"com.example\"\nversion = \"0.1.0\"\n")
    write_bytes("${PROJECT}/src/com/example.anti"
                "//! The package.\n\n/// One.\npub fn one() -> int\n{\n\treturn 1;\n}\n")
    write_bytes("${PROJECT}/src/com/example/half.anti"
                "//! Halving.\n\n/// Half n.\npub fn half(n: int) -> int\n{\n\treturn n / 2;\n}\n")
    write_bytes("${PROJECT}/src/com/example/twice.anti"
                "//! Doubling.\n\nimport com.example.half;\n\n/// Twice n.\npub fn twice(n: int) -> int\n{\n\treturn n * 2;\n}\n\ntests\n{\n\tfn doubles()\n\t{\n\t\tlet n = half.half(twice(2));\n\t\tassert(n == 2);\n\t}\n}\n")
endfunction()
set(built "./build/${HOST}/dev")
set(failed_module "anti test: 1 tests in 1 modules, 1 module failed")
# The library directory of the run, test_run of test.c, which it empties
# first: the stat of the walk that removes it, and then its mkdir.
anti_message(project_library_directory_not_replaced
    SETUP write_project
    FAULT stat:38:EACCES
    ARGS test --runtime "${RUNTIME}"
    ERR "anti: cannot replace build/tests/lib")
anti_message(project_library_directory_not_made
    SETUP write_project
    FAULT mkdir:31:EACCES
    ARGS test --runtime "${RUNTIME}"
    ERR "anti: cannot make build/tests/lib")
# The library file of the module with its tests, compile_library of
# test.c: the directory of its cache in obj/, its compile, the library
# files it imports, read for the key, the key beside it and the copy
# into the library directory of the run.
anti_message(project_library_cache_not_made
    SETUP write_project
    FAULT mkdir:50:EACCES
    ARGS test --runtime "${RUNTIME}"
    ERR "anti: cannot make ${built}/obj/com/example")
anti_message(project_library_not_compiled
    SETUP write_project
    FAULT write:8:EACCES
    ARGS test --runtime "${RUNTIME}"
    ERR "antic: cannot write ${built}/obj/com/example/twice.tests.antl")
anti_message(project_library_imports_unread
    SETUP write_project
    FAULT open:58:EACCES
    ARGS test --runtime "${RUNTIME}"
    ERR "anti: cannot read ${built}/lib/com/example/half.antl")
anti_message(project_library_key_not_written
    SETUP write_project
    FAULT write:9:EACCES
    ARGS test --runtime "${RUNTIME}"
    ERR "anti: cannot write ${built}/obj/com/example/twice.tests.antl.key")
anti_message(project_library_not_copied
    SETUP write_project
    FAULT write:10:EACCES
    ARGS test --runtime "${RUNTIME}"
    ERR "anti: cannot write build/tests/lib/com/example/twice.antl")
# A second run finds the library file of the module that the first one
# cached, and reads it and the library files it imports for the key. When
# a read fails, the run compiles the library file again and passes, and
# says nothing, by Q5 of "Answers of 2026-11-11": the cached file, the
# thirty-sixth open, the import that the walk opens, the fortieth, and
# the digest of the import, the forty-first.
function(write_project_and_run)
    write_project()
    execute_process(COMMAND "${ANTI}" test --runtime "${RUNTIME}"
                    WORKING_DIRECTORY "${PROJECT}"
                    RESULT_VARIABLE status OUTPUT_VARIABLE out
                    ERROR_VARIABLE err ENCODING NONE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "the first run ended with ${status}\n${out}${err}")
    endif()
endfunction()
anti_message(project_cached_library_unread STATUS 0
    SETUP write_project_and_run
    FAULT open:36:EACCES
    OUT "ok com.example.twice.doubles"
        "anti test: 1 tests in 1 modules, 0 modules failed"
    ARGS test --runtime "${RUNTIME}")
anti_message(project_cached_library_import_unopened STATUS 0
    SETUP write_project_and_run
    FAULT open:40:EACCES
    OUT "ok com.example.twice.doubles"
        "anti test: 1 tests in 1 modules, 0 modules failed"
    ARGS test --runtime "${RUNTIME}")
anti_message(project_cached_library_imports_unread STATUS 0
    SETUP write_project_and_run
    FAULT open:41:EACCES
    OUT "ok com.example.twice.doubles"
        "anti test: 1 tests in 1 modules, 0 modules failed"
    ARGS test --runtime "${RUNTIME}")
# The object of the module, compile_object of test.c: the directory of its
# cache, the library files it imports and the key beside it.
anti_message(project_object_cache_not_made
    SETUP write_project
    FAULT mkdir:59:EACCES
    ARGS test --runtime "${RUNTIME}"
    ERR "anti: cannot make ${built}/obj/com/example")
anti_message(project_object_imports_unread
    SETUP write_project
    FAULT open:68:EACCES
    ARGS test --runtime "${RUNTIME}"
    ERR "anti: cannot read ${built}/lib/com/example/half.antl")
anti_message(project_object_key_not_written
    SETUP write_project
    FAULT write:12:EACCES
    ARGS test --runtime "${RUNTIME}"
    ERR "anti: cannot write ${built}/obj/com/example/twice.tests.o.key")
# The object of the import that the dev link of the runner takes from the
# build, compile_imports of test.c.
anti_message(project_import_not_compiled
    SETUP write_project
    FAULT write:14:EACCES
    OUT "${failed_module}"
    ARGS test --runtime "${RUNTIME}"
    ERR "antic: cannot write ${built}/obj/com/example/half.ll")
# The runner, run_unit of test.c: the directory of its cache, the files
# its key reads, which are the library file of the run, the library files
# it imports and the key of an object it links, its link and its key.
anti_message(project_runner_cache_not_made
    SETUP write_project
    FAULT mkdir:70:EACCES
    OUT "${failed_module}"
    ARGS test --runtime "${RUNTIME}"
    ERR "anti: cannot make ${built}/obj/com/example")
anti_message(project_runner_library_unread
    SETUP write_project
    FAULT open:114:EACCES
    OUT "${failed_module}"
    ARGS test --runtime "${RUNTIME}"
    ERR "anti: cannot read build/tests/lib/com/example/twice.antl")
anti_message(project_runner_imports_unread
    SETUP write_project
    FAULT open:120:EACCES
    OUT "${failed_module}"
    ARGS test --runtime "${RUNTIME}"
    ERR "anti: cannot read ${built}/lib/com/example/half.antl")
anti_message(project_runner_object_key_unread
    SETUP write_project
    FAULT open:121:EACCES
    OUT "${failed_module}"
    ARGS test --runtime "${RUNTIME}"
    ERR "anti: cannot read ${built}/obj/com/example/twice.tests.o.key")
anti_message(project_runner_not_linked
    SETUP write_project
    FAULT write:16:EACCES
    OUT "${failed_module}"
    ARGS test --runtime "${RUNTIME}"
    ERR "antic: cannot write ${built}/obj/com/example/twice.runner.ll")
anti_message(project_runner_key_not_written
    SETUP write_project
    FAULT write:17:EACCES
    OUT "${failed_module}"
    ARGS test --runtime "${RUNTIME}"
    ERR "anti: cannot write ${built}/obj/com/example/twice.runner.key")

# The messages of `anti check`, one case each.
# tests/runners/run_anti_messages.cmake states the form of a case.

set(check_manifest "[package]\nname = \"com.example\"\nversion = \"0.1.0\"\n")

# A wrong use of the command ends with 2, as `anti doc` ends, by R160 of
# "Rules of 2026-10-13" in docs/decisions.md.
anti_message(no_source STATUS 2
    FILES anti.toml "${check_manifest}" src/notes.txt "No module.\n"
    ARGS check --work "${WORK}/check" --runtime "${RUNTIME}"
    ERR "anti: no .anti file under src or test")
# Two modules that import each other have no order, by R125 of "Rules of
# 2026-10-13" in docs/decisions.md.
anti_message(modules_import_each_other
    FILES anti.toml "${check_manifest}"
          src/com/example/a.anti "//! The first.\n\nimport com.example.b;\n\n/// One.\npub fn one() -> int\n{\n\treturn 1;\n}\n"
          src/com/example/b.anti "//! The second.\n\nimport com.example.a;\n\n/// Two.\npub fn two() -> int\n{\n\treturn 2;\n}\n"
    ARGS check --work "${WORK}/check" --runtime "${RUNTIME}"
    ERR "anti: the imports form a cycle: com.example.a -> com.example.b -> com.example.a")
# The cycle is named from the first module the order left out, which
# here comes after one that the order took.
anti_message(modules_import_each_other_after_one_in_order
    FILES anti.toml "${check_manifest}"
          src/com/example/a.anti "//! The first.\n\n/// One.\npub fn one() -> int\n{\n\treturn 1;\n}\n"
          src/com/example/b.anti "//! The second.\n\nimport com.example.c;\n\n/// Two.\npub fn two() -> int\n{\n\treturn 2;\n}\n"
          src/com/example/c.anti "//! The third.\n\nimport com.example.b;\n\n/// Three.\npub fn three() -> int\n{\n\treturn 3;\n}\n"
    ARGS check --work "${WORK}/check" --runtime "${RUNTIME}"
    ERR "anti: the imports form a cycle: com.example.b -> com.example.c -> com.example.b")
# The block names a function its module keeps private, which a user of
# the module does not reach.
anti_message(block_fails
    FILES anti.toml "${check_manifest}"
          src/com/example/hidden.anti "//! A user block reaches the public items alone.\n\n/// The doubled value of `n`.\n///\n/// ```anti\n/// let m = hidden.helper(3);\n/// ```\npub fn twice(n: int) -> int\n{\n\treturn helper(n) + helper(n);\n}\n\nfn helper(n: int) -> int\n{\n\treturn n;\n}\n"
    OUT "anti check: front end: 1 file, 1 target, 0 warnings"
        "anti check: doc blocks: 1 block, 1 failed"
    ARGS check --work "${WORK}/check" --runtime "${RUNTIME}"
    ERR "${WORK}/check/check/docs/com_example_hidden_0.anti:5:9: error: `hidden` has no public item `helper`"
        "anti: the `anti` block of `twice` at src/com/example/hidden.anti:3 failed, and its module is ${WORK}/check/check/docs/com_example_hidden_0.anti")

# --targets all runs the front end of every target on the files that
# parsed, so a file that does not parse fails once and is left out of
# the rest.
anti_message(front_end_all_targets_with_a_file_that_does_not_parse
    FILES anti.toml "${check_manifest}"
          src/com/example/broken.anti "//! A module.\n\n/// One.\npub fn one() -> int\n{\n\treturn );\n}\n"
          src/com/example/fine.anti "//! A module.\n\n/// Two.\npub fn two() -> int\n{\n\treturn 2;\n}\n"
    OUT "anti check: front end: 2 files, 6 targets, 0 warnings, 1 failed"
    ARGS check --targets all --work "${WORK}/check" --runtime "${RUNTIME}"
    ERR "src/com/example/broken.anti:6:9: error: expected an expression")

# The formatting class. Two files that each hold a line `anti fmt` writes
# otherwise are two findings.
anti_message(format_two_files
    FILES anti.toml "${check_manifest}"
          src/com/example/first.anti "//! A module.\n\n/// One.\npub fn one() -> int\n{\n  return 1;\n}\n"
          src/com/example/second.anti "//! A module.\n\n/// Two.\npub fn two() -> int\n{\n  return 2;\n}\n"
    OUT "anti check: front end: 2 files, 1 target, 0 warnings"
        "anti check: doc blocks: 0 blocks"
        "anti check: doc warnings: 0"
        "anti check: formatting: 2 files, 2 findings"
    ARGS check --work "${WORK}/check" --runtime "${RUNTIME}"
    ERR "src/com/example/first.anti:6:1: error: the line is not the one `anti fmt` writes"
        "src/com/example/second.anti:6:1: error: the line is not the one `anti fmt` writes")
# A file that goes on after the text `anti fmt` writes: the finding names
# the first line past it.
anti_message(format_empty_lines_at_the_end
    FILES anti.toml "${check_manifest}"
          src/com/example/long.anti "//! A module whose text ends with two empty lines.\n\n/// One.\npub fn one() -> int\n{\n\treturn 1;\n}\n\n\n"
    OUT "anti check: front end: 1 file, 1 target, 0 warnings"
        "anti check: doc blocks: 0 blocks"
        "anti check: doc warnings: 0"
        "anti check: formatting: 1 file, 1 finding"
    ARGS check --work "${WORK}/check" --runtime "${RUNTIME}"
    ERR "src/com/example/long.anti:8:1: error: the line is not the one `anti fmt` writes")
# A file that stops before the text `anti fmt` writes, here without its
# last newline: the finding names its last line.
anti_message(format_last_newline_missing
    FILES anti.toml "${check_manifest}"
          src/com/example/short.anti "//! A module whose text ends without its last newline.\n\n/// One.\npub fn one() -> int\n{\n\treturn 1;\n}"
    OUT "anti check: front end: 1 file, 1 target, 0 warnings"
        "anti check: doc blocks: 0 blocks"
        "anti check: doc warnings: 0"
        "anti check: formatting: 1 file, 1 finding"
    ARGS check --work "${WORK}/check" --runtime "${RUNTIME}"
    ERR "src/com/example/short.anti:7:1: error: the line is not the one `anti fmt` writes")

# The fence of a block. A space after the tag `anti` belongs to no tag,
# so the block compiles. A fence that no second one closes holds no block,
# and by R179 of "Rules of 2026-10-13" in docs/decisions.md it gives a doc
# warning.
anti_message(block_fence_with_a_space
    STATUS 0
    FILES anti.toml "${check_manifest}"
          src/com/example/spaced.anti "//! A module.\n\n/// One.\n///\n/// ```anti \n/// let n = spaced.one();\n/// ```\npub fn one() -> int\n{\n\treturn 1;\n}\n"
    OUT "anti check: front end: 1 file, 1 target, 0 warnings"
        "anti check: doc blocks: 1 block"
        "anti check: doc warnings: 0"
        "anti check: formatting: 1 file"
    ARGS check --work "${WORK}/check" --runtime "${RUNTIME}")
anti_message(block_fence_never_closed
    STATUS 0
    FILES anti.toml "${check_manifest}"
          src/com/example/open.anti "//! A module.\n\n/// One.\n///\n/// ```anti\n/// let n = nothing_here();\npub fn one() -> int\n{\n\treturn 1;\n}\n"
    OUT "anti check: front end: 1 file, 1 target, 0 warnings"
        "anti check: doc blocks: 0 blocks"
        "anti check: doc warnings: 1"
        "anti check: formatting: 1 file"
    ARGS check --work "${WORK}/check" --runtime "${RUNTIME}"
    ERR "src/com/example/open.anti:3:1: warning: the doc comment of `one` holds a fence that nothing closes [doc-markup]")
anti_message(module_fence_never_closed
    STATUS 0
    FILES anti.toml "${check_manifest}"
          src/com/example/open.anti "//! A module.\n//!\n//! ```anti\n\n/// One.\npub fn one() -> int\n{\n\treturn 1;\n}\n"
    OUT "anti check: front end: 1 file, 1 target, 0 warnings"
        "anti check: doc blocks: 0 blocks"
        "anti check: doc warnings: 1"
        "anti check: formatting: 1 file"
    ARGS check --work "${WORK}/check" --runtime "${RUNTIME}"
    ERR "src/com/example/open.anti:1:1: warning: the doc comment of the module holds a fence that nothing closes [doc-markup]")

# Fault injection: each case makes one call of the system fail in
# anti_faults, through the variable ANTI_FAULT of src/rt/fault.h. The
# work directory is relative, so the count of its directories is the same
# on every machine.
set(fault_library "//! The smallest library.\n\n/// One.\npub fn one() -> int\n{\n\treturn 1;\n}\n")
anti_message(fault_source_unread
    FILES anti.toml "${check_manifest}" src/com/example/one.anti "${fault_library}"
    FAULT open:5:EACCES
    OUT "anti check: front end: 1 file, 1 target, 0 warnings, 1 failed"
    ARGS check --work ../check --runtime "${RUNTIME}"
    ERR "anti: cannot read src/com/example/one.anti")
# The listing of a directory fails part of the way, platform_list_directory
# of the platform layer of antic.
anti_message(fault_listing_unread
    FILES anti.toml "${check_manifest}" src/com/example/one.anti "${fault_library}"
    FAULT read:6:EACCES
    ARGS check --work ../check --runtime "${RUNTIME}"
    ERR "anti: cannot read src")
# The kind of an entry and of the test directory, walk_entry and walk of
# files.c.
anti_message(fault_entry_unstated STATUS 2
    FILES anti.toml "${check_manifest}" src/com/example/one.anti "${fault_library}"
    FAULT stat:6:EACCES
    ARGS check --work ../check --runtime "${RUNTIME}"
    ERR "anti: cannot read src/com/example/one.anti")
anti_message(fault_test_directory_unstated
    FILES anti.toml "${check_manifest}" src/com/example/one.anti "${fault_library}"
    FAULT stat:7:EACCES
    ARGS check --work ../check --runtime "${RUNTIME}"
    ERR "anti: cannot read test")
# T153: the work directory, which anti check leaves without a message.
anti_message(fault_work_not_made
    FILES anti.toml "${check_manifest}" src/com/example/one.anti "${fault_library}"
    FAULT mkdir:2:EACCES
    ARGS check --work ../check --runtime "${RUNTIME}"
    ERR "anti: cannot make ../check")
# The source read again for the doc blocks, unit_blocks of check.c, and
# for the formatting, format_class of check.c.
anti_message(fault_block_source_unread
    FILES anti.toml "${check_manifest}" src/com/example/one.anti "${fault_library}"
    FAULT open:8:EACCES
    OUT "anti check: front end: 1 file, 1 target, 0 warnings"
        "anti check: doc blocks: 0 blocks, 1 failed"
    ARGS check --work ../check --runtime "${RUNTIME}"
    ERR "anti: cannot read src/com/example/one.anti")
anti_message(fault_format_source_unread
    FILES anti.toml "${check_manifest}" src/com/example/one.anti "${fault_library}"
    FAULT open:9:EACCES
    OUT "anti check: front end: 1 file, 1 target, 0 warnings"
        "anti check: doc blocks: 0 blocks"
        "anti check: doc warnings: 0"
        "anti check: formatting: 1 file, 1 finding"
    ARGS check --work ../check --runtime "${RUNTIME}"
    ERR "anti: cannot read src/com/example/one.anti")
# T153: the directory of the library file of the module under the work
# directory, unit_file of units.c, which front_end_class of check.c
# leaves without a message.
anti_message(fault_library_directory_not_made
    FILES anti.toml "${check_manifest}" src/com/example/one.anti "${fault_library}"
    FAULT mkdir:5:EACCES
    OUT "anti check: front end: 1 file, 1 target, 0 warnings, 1 failed"
    ARGS check --work ../check --runtime "${RUNTIME}"
    ERR "anti: cannot make ../check/com/example")
# The module of each doc block, compile_block of check.c. A user block
# stands in a file of its own under the directory docs of the work
# directory. A dev block stands in a copy of its module under the
# directory dev, which reads the source again.
set(fault_user_block "//! A library with a block.\n\n/// One.\n///\n/// ```anti\n/// let n = one.one();\n/// ```\npub fn one() -> int\n{\n\treturn 1;\n}\n")
set(fault_dev_block "//! A library with a dev block.\n\n//# ```anti\n//# let n = helper();\n//# ```\nfn helper() -> int\n{\n\treturn 2;\n}\n\n/// Two.\npub fn two() -> int\n{\n\treturn helper();\n}\n")
anti_message(fault_user_block_not_written
    FILES anti.toml "${check_manifest}" src/com/example/one.anti "${fault_user_block}"
    FAULT open:9:EACCES
    OUT "anti check: front end: 1 file, 1 target, 0 warnings"
        "anti check: doc blocks: 1 block, 1 failed"
    ARGS check --work ../check --runtime "${RUNTIME}"
    ERR "anti: cannot write ../check/check/docs/com_example_one_0.anti")
# T153: the directory of the user blocks.
anti_message(fault_user_block_directory_not_made
    FILES anti.toml "${check_manifest}" src/com/example/one.anti "${fault_user_block}"
    FAULT mkdir:10:EACCES
    OUT "anti check: front end: 1 file, 1 target, 0 warnings"
        "anti check: doc blocks: 1 block, 1 failed"
    ARGS check --work ../check --runtime "${RUNTIME}"
    ERR "anti: cannot make ../check/check/docs")
anti_message(fault_dev_block_source_unread
    FILES anti.toml "${check_manifest}" src/com/example/two.anti "${fault_dev_block}"
    FAULT open:9:EACCES
    OUT "anti check: front end: 1 file, 1 target, 0 warnings"
        "anti check: doc blocks: 1 block, 1 failed"
    ARGS check --work ../check --runtime "${RUNTIME}"
    ERR "anti: cannot read src/com/example/two.anti")
anti_message(fault_dev_block_not_written
    FILES anti.toml "${check_manifest}" src/com/example/two.anti "${fault_dev_block}"
    FAULT open:10:EACCES
    OUT "anti check: front end: 1 file, 1 target, 0 warnings"
        "anti check: doc blocks: 1 block, 1 failed"
    ARGS check --work ../check --runtime "${RUNTIME}"
    ERR "anti: cannot write ../check/check/dev/com/example/two.anti")
# T153: the directory of the copy of the module of a dev block.
anti_message(fault_dev_block_directory_not_made
    FILES anti.toml "${check_manifest}" src/com/example/two.anti "${fault_dev_block}"
    FAULT mkdir:12:EACCES
    OUT "anti check: front end: 1 file, 1 target, 0 warnings"
        "anti check: doc blocks: 1 block, 1 failed"
    ARGS check --work ../check --runtime "${RUNTIME}"
    ERR "anti: cannot make ../check/check/dev/com/example")

# The manifest, which names the source directory.
anti_message(manifest_no_toml
    FILES anti.toml "name = \n" src/com/example/one.anti "${fault_library}"
    ARGS check --work ../check --runtime "${RUNTIME}"
    ERR "anti: anti.toml is no TOML that anti reads")
# Without --runtime the check takes the runtime archive of the user's data
# directory, whose lib/ tells it from any other directory.
anti_message(runtime_of_the_data_directory STATUS 0
    FILES anti.toml "${check_manifest}" src/com/example/one.anti "${fault_library}"
    DIRECTORIES data/anti/lib
    ENV "XDG_DATA_HOME=${PROJECT}/data" "LOCALAPPDATA=${PROJECT}/data"
    OUT "anti check: front end: 1 file, 1 target, 0 warnings"
        "anti check: doc blocks: 0 blocks"
        "anti check: doc warnings: 0"
        "anti check: formatting: 1 file"
    ARGS check --work ../check)
